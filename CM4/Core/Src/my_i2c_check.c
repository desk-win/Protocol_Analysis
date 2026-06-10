#include "my_i2c_check.h"
#include "i2c.h"
#include "main.h"
#include "stm32_hal_legacy.h"
#include "stm32h747xx.h"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_gpio.h"
#include "stm32h7xx_hal_i2c.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "my_dwt_count.h"


/************************************目前实现的功能************************************

*主机模式：
*    ├── 7bit 地址全扫描（0x00~0x7F）
*    ├── 10bit 地址扫描
*    ├── 显示所有响应 ACK 的从机地址
*    └── 扫描进度显示（这个可有可无）
*    ├── 向指定地址写入任意字节序列
*    ├── 从指定地址读取指定长度数据
*    ├── 写后读（Write-then-Read，先写寄存器地址再读数据）
*    ├── 重复 START 操作（不释放总线直接切换方向）
*    └── 支持 7bit 和 10bit 从机地址
*从机模式：
*    ├── 响应指定地址的主机寻址
*    ├── 接收主机发来的不定长数据
*    └── 地址匹配中断通知
*参数配置：
*    ├── 速率模式：标准模式 100kHz / 快速模式 400kHz / 快速 1MHz
*    ├── 地址设置，作为从机的时候设置自己的地址
*    ├── 时钟拉伸：使能 / 禁用
*    └── 模拟噪声滤波器：使能 / 禁用
*错误检测：
*    ├── 总线错误（BERR）：START/STOP 位置不合法
*    ├── 仲裁丢失（ARLO）：多主机仲裁失败
*    ├── ACK 失败（NACKF）：从机未响应
*    ├── 过载错误（OVR）：数据来不及处理
*    ├── 超时错误（TIMEOUT）：SCL 拉低超时
*    ├── 总线死锁检测（SCL/SDA 长时间低电平）
*    └── 总线死锁恢复（发送 9 个时钟脉冲解锁）
*协议层分析：
*    ├── 每帧字节数统计
*    ├── 帧数统计
*    ├── 时钟拉伸时间测量
*    ├── START 到第一个数据字节的延迟测量
*    └── 传输成功率统计

*************************************************************************************/


I2C_Data my_i2c_data;								//全局数据内容结构体
I2C_Deploy my_i2c_deploy;							//配置结构体
I2C_Analyse my_i2c_analyse;							//协议层分析结构体
I2C_This_Frame now_frame;
I2C_Range_Buffer i2c_rangebuffer;				//用来存放接收的数据

#define I2C_FREQ_HZ		100000			//i2c频率
#define I2C_KERNEL_CLK_HZ	120000000UL		//i2c4外设时钟源频率
#define I2C_PORT		GPIOD
#define I2C_SDA_PIN		GPIO_PIN_13
#define I2C_SCL_PIN		GPIO_PIN_12

void Slave_Analyse(void);
void Frame_Reset(void);
void Frame_Analyse(void);
void Record_Stretch(void);
/*****************************************工具函数**************************************/
//向缓冲区写入数据
uint8_t I2C_RangeBuffer_Write(uint8_t data){
	uint8_t if_cover = 0;
    if ((i2c_rangebuffer.buffer_head + 1)%I2C_RANGE_BUFFER_LEN == i2c_rangebuffer.buffer_tail) {
        //如果缓冲区满了，丢掉最老的一个数据
        i2c_rangebuffer.buffer_tail = (i2c_rangebuffer.buffer_tail + 1)%I2C_RANGE_BUFFER_LEN;
        if_cover = 1;
    }
    i2c_rangebuffer.range_buffer[i2c_rangebuffer.buffer_head] = data;
    i2c_rangebuffer.buffer_head = (i2c_rangebuffer.buffer_head + 1)%I2C_RANGE_BUFFER_LEN;
    return if_cover;
}

//从缓冲区读取数据
uint8_t I2C_RangeBuffer_Read(uint8_t *data, uint32_t data_len){
	uint8_t if_empty = 1;
    uint32_t data_index = 0;                //索引
    if (i2c_rangebuffer.buffer_tail != i2c_rangebuffer.buffer_head) {
        //当缓冲区里面还有数据的时候
        while (i2c_rangebuffer.buffer_tail != i2c_rangebuffer.buffer_head) {
            data[data_index] = i2c_rangebuffer.range_buffer[i2c_rangebuffer.buffer_tail];
            data_index++;
            if(data_index >= data_len) break;
            i2c_rangebuffer.buffer_tail = (i2c_rangebuffer.buffer_tail + 1)%I2C_RANGE_BUFFER_LEN;
        }
    }else {
        if_empty = 0;
    }
    return if_empty;
}
//每次传输结束后将my_i2c_data.i2c_master_rxdata的数据放入缓冲区
void I2C_PutData_To_Buffer(){
	uint16_t data_index;
	if (my_i2c_deploy.my_i2c_mode == MY_I2C_MASTER) {
		for (data_index = 0; data_index < my_i2c_data.i2c_master_rxlen; data_index++) {
			I2C_RangeBuffer_Write(my_i2c_data.i2c_master_rxdata[data_index]);
		}
	}else if (my_i2c_deploy.my_i2c_mode == MY_I2C_SLAVE) {
		for (data_index = 0; data_index < my_i2c_data.i2c_slave_rxlen; data_index++) {
			I2C_RangeBuffer_Write(my_i2c_data.i2c_slave_rxdata[data_index]);
		}
	}
}
/***************************************初始化配置**************************************/

/**
* @brief 根据目标频率计算 Timing 寄存器值
* @param target_hz  目标 SCL 频率，例如 100000 / 400000 / 1000000
* @return Timing 寄存器值，出错返回 0
**/
uint32_t I2C_CalcTiming(uint32_t target_hz)
{
    // PRESC 从小到大尝试，找到 SCLL/SCLH 都在 0~255 范围内的第一个解
    for (uint32_t presc = 0; presc <= 15; presc++) {
        uint32_t t_presc_ns = (presc + 1) * 1000000000UL / I2C_KERNEL_CLK_HZ;
        uint32_t period_ns  = 1000000000UL / target_hz;

        // 总分频数（减去 SCLL/SCLH 各自的 +1 修正）
        int32_t total = (int32_t)(period_ns / t_presc_ns) - 2;
        if (total < 2 || total > 510) continue;   // 超出范围，换 PRESC

        // 低电平略长，符合 I2C 规范
        uint32_t scll = total / 2;
        uint32_t sclh = total - scll - 1;
        if (sclh < 1) continue;

        // SCLDEL / SDADEL 根据速率段选经验值
        uint32_t scldel, sdadel;
        if (target_hz <= 100000) {
            scldel = 4; sdadel = 2;
        } else if (target_hz <= 400000) {
            scldel = 3; sdadel = 1;
        } else {
            scldel = 2; sdadel = 0;
        }

        return (presc  << 28) |
               (scldel << 20) |
               (sdadel << 16) |
               (sclh   <<  8) |
               (scll   <<  0);
    }
    return 0;   // 找不到合适的分频组合
}

/**
*	@brief 初始化i2c的配置，双地址模式、广播模式、时钟拉伸、时钟频率部分是一起配置的，
*	地址部分分为主从两部分配置。作为主机的时候address_7bit/address_10bit是从设备的
*	地址。作为从机的时候address_7bit/address_10bit是自己的地址。因为i2c没有主从模式
*	这个说法，所以我自己加入了my_i2c_deploy.my_i2c_mode作为区分主从模式的依据
*	@param now_mode:初始化主从模式
*	@param address_mode:寻址模式
*	@param address_7bit:7位地址
*	@param address_10bit:10位地址
*	@param stretch:时钟拉神，1开启，0关闭
*	@param scl_fq:scl频率
**/

void My_I2C_Init(I2C_Mode now_mode, uint8_t address_mode, uint8_t address_7bit, uint16_t address_10bit, uint8_t stretch, uint32_t scl_fq){
	HAL_I2C_DeInit(&hi2c4);
	hi2c4.Init.Timing = I2C_CalcTiming(scl_fq);				//配置scl频率
	my_i2c_deploy.scl_fq = scl_fq;
	hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;				//双地址模式，一般没有用到的
	hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;				//广播模式，一般不用用到
	//当stretch为1的时候开启时钟拉伸，0的时候关闭时钟拉伸
	if(stretch)	hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	else hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_ENABLE;
	//主机模式
	if (now_mode == MY_I2C_MASTER) {
		my_i2c_deploy.my_i2c_mode = MY_I2C_MASTER;
		hi2c4.Init.OwnAddress1 = 0;					
		hi2c4.Init.OwnAddress2 = 0;
		if (address_mode == 7) {										//7位地址
			my_i2c_deploy.address_mode = 7;
			my_i2c_deploy.device_address_7bit = address_7bit;
			hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
		}else {
			my_i2c_deploy.address_mode = 10;
			my_i2c_deploy.device_address_10bit = address_10bit;
			hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_10BIT;
		}
	}else if (now_mode == MY_I2C_SLAVE) {
		my_i2c_deploy.my_i2c_mode = MY_I2C_SLAVE;
		//作为从机的时候地址是自己的
		if (address_mode == 7) {
			my_i2c_deploy.address_mode = 7;
			my_i2c_deploy.my_address_7bit = address_7bit;

			hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
			hi2c4.Init.OwnAddress1 = address_7bit<<1;					//7位地址需要位移
			hi2c4.Init.OwnAddress2 = 0;
		}else {
			my_i2c_deploy.address_mode = 10;
			my_i2c_deploy.my_address_10bit = address_10bit;

			hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_10BIT;
			hi2c4.Init.OwnAddress1 = address_10bit;	
			hi2c4.Init.OwnAddress2 = 0;
		}
	}
	HAL_I2C_Init(&hi2c4);
	if (my_i2c_deploy.my_i2c_mode == MY_I2C_SLAVE) {
		//从机模式开启监听中断
		HAL_I2C_EnableListen_IT(&hi2c4);
	}

}

/**
*	@brief 这个函数用来切换I2C的模式
*	@param now_mode:现在的i2c模式
*/
void Switch_I2C_Mode(I2C_Mode now_mode){
	if (now_mode == MY_I2C_MASTER) {
		my_i2c_deploy.my_i2c_mode = MY_I2C_MASTER;
		//关闭监听中断
		HAL_I2C_DisableListen_IT(&hi2c4);
	}else {
		my_i2c_deploy.my_i2c_mode = MY_I2C_SLAVE;
		//开启监听中断
		HAL_I2C_EnableListen_IT(&hi2c4);
	}
}

/**************************************作为主机模式********************************************/

/**
*	@brief 这个函数用来实现作为主机时候的总线扫描，向所有的地址发送信号,有回应的设备地址对应的在数组里面置1
*	调用数组my_i2c_data.bit10_check或者my_i2c_data.bit7_check可以得到有回应的地址有哪些，适合在作为一主
*	多从的时候使用
*
**/
void My_I2C_Master_ScanBus(){
	HAL_StatusTypeDef if_reply;
	if (my_i2c_deploy.address_mode == 7) {
		//对7位的地址进行全扫描
		for (uint16_t i = 0; i<128; i++) {
			if_reply = HAL_I2C_IsDeviceReady(&hi2c4, (i << 1), 3, 5);
			if (if_reply == HAL_OK) {
				my_i2c_data.bit7_check[i] = 1;
			}else {
				my_i2c_data.bit7_check[i] = 0;
			}
		}
	}else if (my_i2c_deploy.address_mode == 10) {
		for (uint16_t i = 0; i<1024; i++) {
			if_reply = HAL_I2C_IsDeviceReady(&hi2c4, i, 2, 5);
			if (if_reply == HAL_OK) {
				my_i2c_data.bit10_check[i] = 1;
			}else {
				my_i2c_data.bit10_check[i] = 0;
			}
		}
	}
}

/**
*	@brief 这个函数用来主机发送数据(普通数据发送)
**/
void My_I2C_Master_Send_Simple(uint16_t address, uint8_t *data, uint16_t len){
	my_i2c_data.mode = I2C_TX_SIMPLE;
	//计算理论传输这个数据所需要的时间
	now_frame.theory_no_stretch = (uint32_t)((uint64_t)len * 9 * 1000000 / my_i2c_deploy.scl_fq);
	now_frame.stretch_begin = Get_Sys_us();			//记录当前时间
	HAL_I2C_Master_Transmit_IT(&hi2c4, address, data, len);
}

/**
*	@brief 主机接收数据
*
**/
void My_I2C_Master_Read_Simple(uint16_t address, uint8_t *data, uint16_t len){
	my_i2c_data.mode = I2C_RX_SIMPLE;
	now_frame.theory_no_stretch = (uint32_t)((uint64_t)len * 9 * 1000000 / I2C_FREQ_HZ);
	now_frame.stretch_begin = Get_Sys_us();
	HAL_I2C_Master_Receive_IT(&hi2c4, address, data, len);
}

/**
*	@brief 这个函数用来不释放总线发送数据,可以让主机在发送从机地址之后直接读取数据
*	@param address:从机地址
*	@param send_data:要发送的数据
*	@param tx_len:发送的数据的长度
*	@param read_data:接收数据缓冲区
*	@param rx_len:接收数据长度
*	@return HAL_StatusTypeDef
**/
HAL_StatusTypeDef My_I2C_Master_Send_ReStart(uint16_t address){
	HAL_StatusTypeDef status;
	my_i2c_data.mode = I2C_SEQ;
	my_i2c_data.address = address;
	my_i2c_data.task_done = 0;

	now_frame.theory_no_stretch = (uint32_t)((uint64_t)my_i2c_data.i2c_master_txlen * 9 * 1000000 / I2C_FREQ_HZ);
	now_frame.stretch_begin = Get_Sys_us();
	status = HAL_I2C_Master_Sequential_Transmit_IT(&hi2c4, my_i2c_data.address, my_i2c_data.i2c_master_txdata, my_i2c_data.i2c_master_txlen, I2C_FIRST_FRAME);
	if (status != HAL_OK) return status;
	return status;
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c){
	if (hi2c->Instance == hi2c4.Instance) {

		if (my_i2c_data.mode == I2C_SEQ) {
			HAL_I2C_Master_Sequential_Receive_IT(&hi2c4, my_i2c_data.address, my_i2c_data.i2c_master_rxdata, my_i2c_data.i2c_master_rxlen, I2C_LAST_FRAME);
		}else if(my_i2c_data.mode == I2C_TX_SIMPLE){
			Record_Stretch();
		}
	}
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c){
	if (hi2c->Instance == hi2c4.Instance) {
		if (my_i2c_data.mode == I2C_SEQ) {
			Record_Stretch();
			now_frame.byte_count = my_i2c_data.i2c_master_rxlen;					
			Frame_Analyse();
			I2C_PutData_To_Buffer();
		}else if(my_i2c_data.mode == I2C_RX_SIMPLE){
			//普通接收函数中断回调后的操作
			Record_Stretch();
			now_frame.byte_count = my_i2c_data.i2c_master_rxlen;	
			Frame_Analyse();
			I2C_PutData_To_Buffer();
		}
	}
}

/***************************************作为从机模式****************************************/


//硬件比对地址后会进入这个回调函数
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode){
	if (hi2c->Instance == hi2c4.Instance) {
		if(TransferDirection == I2C_DIRECTION_TRANSMIT){
			now_frame.start_tick = Get_Sys_us();			//记录start信号
			Frame_Reset();				
			//此时从机的状态为读取
			my_i2c_data.mode = I2C_SLAVE_RX;
			my_i2c_data.i2c_slave_rxlen = 0;
			//打开接收中断
			HAL_I2C_Slave_Seq_Receive_IT(hi2c, &my_i2c_data.i2c_slave_rxdata[my_i2c_data.i2c_slave_rxlen], 1, I2C_FIRST_FRAME);
		}
		//如果主机要接收数据
		if(TransferDirection == I2C_DIRECTION_RECEIVE){
			//此时从机状态为发送
			my_i2c_data.mode = I2C_SLAVE_TX;
		
			//开启发送数据中断
			HAL_I2C_Slave_Seq_Transmit_IT(hi2c, my_i2c_data.i2c_slave_txdata, my_i2c_data.i2c_slave_txlen, I2C_FIRST_FRAME);
		}
	}
}

//前面接收一个数据后会进入这个回调函数
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c){
	if (hi2c->Instance == hi2c4.Instance) {
		now_frame.byte_count++;				//一帧字节数加1
		//判断是不是第一个字节
		if(now_frame.if_first_byte == 0){
			//如果是第一个字节记录当前时间
			now_frame.firstdata = Get_Sys_us();
			now_frame.if_first_byte = 1;		//表示已经接收过第一字节了
		}
		if(my_i2c_data.i2c_slave_rxlen < RXDATA_LEN-1){			//如果没有满
			//重新开启中断
			my_i2c_data.i2c_slave_rxlen++;
			HAL_I2C_Slave_Seq_Receive_IT(hi2c, &my_i2c_data.i2c_slave_rxdata[my_i2c_data.i2c_slave_rxlen], 1, I2C_NEXT_FRAME);
		}else {												//如果满了，将数据写入之后继续开启
			I2C_PutData_To_Buffer();
			my_i2c_data.i2c_slave_rxlen = 0;
			HAL_I2C_Slave_Seq_Receive_IT(hi2c, &my_i2c_data.i2c_slave_rxdata[my_i2c_data.i2c_slave_rxlen], 1, I2C_NEXT_FRAME);
		}
	}
	
}

//接收到停止位进入这个回调函数
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c){
	if (hi2c->Instance == hi2c4.Instance) {
		if (my_i2c_data.mode == I2C_SLAVE_RX) {
			if (now_frame.if_first_byte) {
				now_frame.start_to_firstdata = now_frame.firstdata - now_frame.start_tick;
				if (now_frame.start_to_firstdata > my_i2c_analyse.tfstfd_max) {
					my_i2c_analyse.tfstfd_max = now_frame.start_to_firstdata;
					
				}
				my_i2c_analyse.tfstfd_avg = my_i2c_analyse.tfstfd_avg + (float_t)(now_frame.start_to_firstdata - my_i2c_analyse.tfstfd_avg)/my_i2c_analyse.total_frame;			//增量平均
			}
			Frame_Analyse();		//处理数据
		}
		I2C_PutData_To_Buffer();
		//重新开启监听模式
		HAL_I2C_EnableListen_IT(&hi2c4);
	}

}

/*************************************错误检测和协议分析*********************************************/

/**
*	@brief 计算时钟拉伸时长
**/
void Record_Stretch(){
	uint64_t mid_clock = Get_Sys_us() - now_frame.stretch_begin;
	if (mid_clock > now_frame.theory_no_stretch) {				//当间隔时间大于理论时长的时候
		now_frame.clock_stretch = mid_clock - now_frame.theory_no_stretch;					//记录下时钟拉伸时长
	}else {
		now_frame.clock_stretch = 0;							//没有时钟拉伸
	}
	my_i2c_analyse.clock_stretch_total += now_frame.clock_stretch;
	if (now_frame.clock_stretch > my_i2c_analyse.clock_stretch_max) {
		my_i2c_analyse.clock_stretch_max = now_frame.clock_stretch;			//记录最长的时钟拉伸时长
	}
	my_i2c_analyse.clock_stretch_avg = ((float_t)my_i2c_analyse.clock_stretch_total/my_i2c_analyse.total_frame);
}


/**
*	@brief 用来对一些帧数据进行清零
**/
void Frame_Reset(){
	now_frame.firstdata = 0;
    now_frame.if_first_byte = 0;
    now_frame.byte_count = 0;          // 补上清零
    now_frame.if_success = 0;
    now_frame.start_to_firstdata = 0;
	my_i2c_data.i2c_err.error_code = 0;
}

/**
*	@brief 对接收到的数据进行分析,统计总帧数，成功帧数和失败帧数; 统计总字节数，
*	计算成功率
*	
**/
void Frame_Analyse(){

	//成功与否
	if (my_i2c_data.i2c_err.error_code == 0) {
		now_frame.if_success = 1;
		my_i2c_analyse.success_frame++;
	}else {
		now_frame.if_success = 0;
		my_i2c_analyse.fail_frame++;
	}

    //累计统计
    my_i2c_analyse.total_frame++;
    my_i2c_analyse.total_byte += now_frame.byte_count;
       
    uint32_t total = my_i2c_analyse.success_frame + my_i2c_analyse.fail_frame;
    if (total > 0) my_i2c_analyse.success_rate = (float_t)my_i2c_analyse.success_frame / total;
}

/**
*	@brief 这个函数用来检测总线死锁和恢复死锁，通过发送9个时钟来解锁
*
**/
void My_I2C_Bus_Check(){
	//如果在没有数据传输的时候，SDA或者SCL被长时间拉低，说明总线死锁
	uint8_t if_lock;
	for (uint8_t i = 0; i<10; i++) {
		if ((HAL_GPIO_ReadPin(I2C_PORT, I2C_SCL_PIN) == GPIO_PIN_RESET || 
			HAL_GPIO_ReadPin(I2C_PORT, I2C_SDA_PIN) == GPIO_PIN_RESET)&&
			(HAL_I2C_GetState(&hi2c4) == HAL_I2C_STATE_BUSY)) {
			if_lock = 1;
		}else {
			if_lock = 0;
			break;
		}
		HAL_Delay(10);
	}
	if (if_lock) {
		//将SCL配置为推挽输出
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		HAL_I2C_DeInit(&hi2c4);			
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pin = I2C_SCL_PIN;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
		HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
		//将SDA配置为输入模式
		GPIO_InitStruct.Pin = I2C_SDA_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
		//用SCL发送9个模拟时钟脉冲
		for (uint8_t i = 0; i < 9; i++) 
    	{
			HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);
			HAL_Delay(5); // 约 100kHz 的半周期
			HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
			HAL_Delay(5);
			// 每发一个脉冲，检查一下从机是否释放了 SDA
			if (HAL_GPIO_ReadPin(I2C_PORT, I2C_SDA_PIN) == GPIO_PIN_SET) {
				break; // 从机已释放 SDA，提前退出
			}
    	}
		//手动产生一个stop信号
		GPIO_InitStruct.Pin = I2C_SDA_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
		HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
		
		HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
		HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_RESET);
		HAL_Delay(5);
		HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_SET); // STOP 信号完成
		HAL_Delay(5);

		// 重新初始化 I2C 外设，恢复正常工作
    	HAL_I2C_Init(&hi2c4);
	}
}

//I2C错误处理函数
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c){
	uint32_t err = hi2c->ErrorCode;
	//应答失败
	if(err & HAL_I2C_ERROR_AF){
		my_i2c_data.i2c_err.error_code |= (1 << 1);
		my_i2c_data.i2c_err.af_error++;
	}
	//总线错误，出现非法停止位或者起始位
	if (err & HAL_I2C_ERROR_BERR) {
		my_i2c_data.i2c_err.error_code |= (1 << 2);
		my_i2c_data.i2c_err.berr_error++;
	}
	//仲裁丢失
	if (err & HAL_I2C_ERROR_ARLO) {
		my_i2c_data.i2c_err.error_code |= (1 << 3);
		my_i2c_data.i2c_err.arlo_error++;
	}
	//SCL被拉低的时间过长
	if (err & HAL_I2C_ERROR_TIMEOUT) {
		my_i2c_data.i2c_err.error_code |= (1 << 4);
		my_i2c_data.i2c_err.timeout_error++;
	}
	if (err & HAL_I2C_ERROR_OVR) {
		my_i2c_data.i2c_err.error_code |= (1 << 6);
		my_i2c_data.i2c_err.over_error++;
	}
	// 从机模式的时候要重新开启监听
	if (my_i2c_deploy.my_i2c_mode == MY_I2C_SLAVE) {
		HAL_I2C_EnableListen_IT(&hi2c4);
	}
}


