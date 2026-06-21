#ifndef __MY_I2C_CHECK_H
#define __MY_I2C_CHECK_H


#include "stm32h7xx_hal.h"
#include "i2c.h"
#include "stdlib.h"
#include "usart_printf.h"
#include <math.h>
#include <stdint.h>

#define     I2C_RANGE_BUFFER_LEN     1024       //环形缓冲区最大长度   
#define		RXDATA_LEN		100			//最大接收数据长度
#define     TXDATA_LEN		100         //最大发送数据长度

extern uint8_t check;
extern uint8_t count;
extern uint8_t if_inexti;


//I2C模式
typedef enum{
    MY_I2C_MASTER,
    MY_I2C_SLAVE
} I2C_Mode;

//错误结构体
typedef struct{
    uint8_t berr_error;     // 总线错误
    uint8_t af_error;       // 应答错误
    uint8_t arlo_error;     // 多主机仲裁失败
    uint8_t over_error;     // 过载错误
    uint8_t timeout_error;  // 时间超时
    uint32_t error_code;
} I2C_Error_Type;

//收发状态
typedef enum{
    //作为主机时候
    I2C_TX_SIMPLE,       //普通发送
    I2C_RX_SIMPLE,       //普通接收
    I2C_SEQ,             //发送后接收

    //作为从机时候
    I2C_SLAVE_RX,
    I2C_SLAVE_TX,
    I2C_SLAVE_DILE,
    I2C_SLAVE_FULL
} I2C_Data_Mode;

//I2C数据内容
typedef struct{
    I2C_Data_Mode mode;
    //从机的数据缓冲区
    uint8_t i2c_slave_rxdata[RXDATA_LEN];
    uint32_t i2c_slave_rxlen;
    uint8_t i2c_slave_txdata[TXDATA_LEN];
    uint32_t i2c_slave_txlen;

    //主机的数据缓冲区
    uint8_t i2c_master_rxdata[RXDATA_LEN];
    uint32_t i2c_master_rxlen;
    uint8_t i2c_master_txdata[TXDATA_LEN];
    uint32_t i2c_master_txlen;
    uint32_t address;                       //要发送的从机地址
    uint8_t task_done;

    I2C_Error_Type i2c_err;
    uint8_t bit7_check[128];
    uint8_t bit10_check[1024];
}I2C_Data;

//整体协议分析结构体
typedef struct{
    //基础计数器
    uint32_t total_frame;           //总帧数
    uint32_t total_byte;            //总字节数
    //关于从start信号到第一字节的时间tfstfd (time_from_start_to_firstdata)
    uint32_t tfstfd_max;            //用来记录start到第一个字节的最大延时
    uint32_t tfstfd_min;            //最小延时
    float_t tfstfd_avg;             //平均延时
    //关于时钟拉伸 单位us
    uint32_t clock_stretch_max;            //拉伸时钟最大持续时间
    uint32_t clock_stretch_total;          //总拉伸时间
    float_t clock_stretch_avg;             //平均时间 
    //关于传输成功率
    uint32_t success_frame;         //成功通信帧数
    uint32_t fail_frame;            //失败通信帧数
    float_t success_rate;             //传输成功率
} I2C_Analyse;

//用来记录一帧里面的状态
typedef struct{
    uint32_t byte_count;            //本帧字节数
    uint8_t if_success;             //本帧是否通信成功
    //时钟拉伸只在作为主机的时候使用
    uint32_t clock_stretch;         //时钟拉伸时间
    uint32_t theory_no_stretch;     //理论上没有时钟拉伸情况下发送数据的时间
    uint32_t stretch_begin;         //记录时钟拉伸的开始

    //以下在作为从机的时候使用
    uint32_t start_to_firstdata;    //start信号到第一个字节的时间
    uint32_t start_tick;            //这个用来记录下每一帧的start的时刻
    uint32_t firstdata;             //用来记录每一帧第一个数据的时刻
    uint8_t if_first_byte;          //防止第一个字节被重复记录
    

} I2C_This_Frame;

//I2C配置结构体，记录目前的配置状况
typedef struct{
    I2C_Mode my_i2c_mode;           //当前主从模式
    uint32_t scl_fq;            
    uint8_t address_mode;           //寻址模式，填7为7bit模式，填10为10bit模式
    //当作为主机的时候从机的地址
    uint8_t device_address_7bit;
    uint16_t device_address_10bit;
    //当作为从机的时候自己的地址
    uint8_t my_address_7bit;
    uint16_t my_address_10bit;

} I2C_Deploy;

typedef struct{
    uint16_t buffer_head;
    uint16_t buffer_tail;
    uint8_t range_buffer[I2C_RANGE_BUFFER_LEN];
} I2C_Range_Buffer;

extern I2C_Data my_i2c_data;

uint8_t I2C_RangeBuffer_Write(uint8_t data);
uint8_t I2C_RangeBuffer_Read(uint8_t *data, uint32_t data_len);
void I2C_PutData_To_Buffer(void);
void My_I2C_Init(I2C_Mode now_mode, uint8_t address_mode, uint8_t address_7bit, uint16_t address_10bit, uint8_t stretch, uint32_t scl_fq);
void Switch_I2C_Mode(I2C_Mode now_mode);

void My_I2C_Master_ScanBus(void);
void My_I2C_Master_Send_Simple(uint16_t address, uint8_t *data, uint16_t len);
void My_I2C_Master_Read_Simple(uint16_t address, uint8_t *data, uint16_t len);
HAL_StatusTypeDef My_I2C_Master_Send_ReStart(uint16_t address);

void My_I2C_Bus_Check(void);

#endif
