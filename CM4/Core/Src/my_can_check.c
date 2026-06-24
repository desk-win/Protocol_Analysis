#include "my_can_check.h"
#include "fdcan.h"
#include "main.h"
#include "stm32h7xx.h"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_fdcan.h"
#include <stdint.h>

/************************************************************************************
*收发节点：
*    发送功能：
*    ├── 发送标准帧（11bit ID）
*    ├── 发送扩展帧（29bit ID）
*    ├── 发送远程帧（RTR 请求帧）
*    ├── 配置 ID / DLC（数据长度）/ 数据内容
*    ├── 单次发送 / 循环发送（可配置间隔）
*    └── 发送优先级配置（3个发送邮箱）
*    接收功能：
*    ├── 接收总线上所有帧（过滤器全通）
*    ├── 配置 ID 过滤器（只接收指定 ID）
*    ├── 支持 FIFO0 和 FIFO1 双缓冲接收
*    ├── 显示：时间戳 / ID / 帧类型 / DLC / 数据
*    └── 循环显示最近 N 帧（可配置缓存深度）
*参数配置：
*    ├── 波特率预设：125Kbps / 250Kbps / 500Kbps / 1Mbps
*    ├── 自定义波特率（配置 BRP / BS1 / BS2 / SJW）
*    ├── 采样点位置（推荐 75%~87.5%）
*    ├── 工作模式：正常模式 / 静默模式 / 回环模式 / 回环静默模式
*    └── 自动重传：使能 / 禁用
*错误检测与总线状态：
*    错误计数器：
*    ├── 发送错误计数 TEC（0~255）
*    ├── 接收错误计数 REC（0~127）
*    └── 实时显示，超阈值告警
*    总线状态机：
*    ├── 主动错误状态（TEC/REC < 128）
*    ├── 被动错误状态（TEC/REC >= 128）
*    ├── 总线关闭状态（TEC >= 256）
*    └── 总线关闭后自动恢复（等待 128 × 11 位）
*    错误帧类型检测：
*    ├── 位错误（Bit Error）
*    ├── 填充错误（Stuff Error）
*    ├── CRC 错误（CRC Error）
*    ├── 格式错误（Form Error）
*    └── 应答错误（ACK Error）
*协议层分析：
*    ├── 总线利用率统计（有效帧时间 / 总时间）
*    ├── 各 ID 的帧频率统计
*    ├── 错误帧发生频率统计
*    ├── 丢帧检测（接收 FIFO 溢出）
*    └── 总线波特率自动检测（测量位时间）
*********************************************************************************************/


Can_Range_Buffer my_can_rangebuffer;        // 建立一个环形缓冲区
FDCAN_TxHeaderTypeDef TxHeader;             //发送数据时的参数句柄
Can_Cycle my_can_cycle;
Can_Message_Struct rx_message;              //接收报文的信息
Can_Analyse my_can_judge;                   //用来分析错误的句柄

/*****************************************工具函数*******************************************/
/**
*   @brief 将数据放入环形缓冲区
*   @param rangebuffer:环形缓冲区句柄
*   @param rx_mes:报文信息句柄
*   @retval uint8,0为缓冲区已经出现覆盖现象，1为正常写入
*/
uint8_t CAN_RangeBuffer_Write(Can_Range_Buffer *rangebuffer, Can_Message_Struct *rx_mes){
    uint32_t next_head = (rangebuffer->head + 1)%CAN_RANGER_BUFFER_LEN;
    uint8_t return_data = 1;
    if (next_head == rangebuffer->tail) {
        //如果满了，环形缓冲区的尾指针向前进一位，将最老的数据覆盖
        rangebuffer->tail = (rangebuffer->tail + 1)%CAN_RANGER_BUFFER_LEN;
        rangebuffer->count--;
        return_data = 0;
    }
    rangebuffer->buffer[rangebuffer->head] = *rx_mes;
    rangebuffer->head = next_head;
    rangebuffer->count++;
    
    return return_data;
}

/**
*   @brief 这个函数用来向环形缓冲区读取数据
*   @param rangebuffer:环形缓冲区句柄
*   @param rx_mes:读取报文句柄
*   @retval uint8_t,当缓冲区为空的时候返回0
*/
uint8_t CAN_RangeBuffer_Read(Can_Message_Struct *rx_mes, uint8_t len){
    uint16_t data_index = 0;                    //用来存放的缓冲区索引
    if (my_can_rangebuffer.tail == my_can_rangebuffer.head) return 0; 
    while ((my_can_rangebuffer.tail != my_can_rangebuffer.head)&&(data_index<len)) {
        rx_mes[data_index] = my_can_rangebuffer.buffer[my_can_rangebuffer.tail];
        data_index++;
        my_can_rangebuffer.tail = (my_can_rangebuffer.tail+1)%CAN_RANGER_BUFFER_LEN;
    }
    return 1;
}

/**
*   @brief 这个函数将输入的字节长度转化为DLC宏
*   @param len:字节长度
*/
uint32_t Switch_Len_TO_DLC(uint8_t len){
    return (len << 16);
}

/**
*   @brief 将DLC宏转化为字节数
**/
uint8_t Switch_DLC_TO_Len(uint32_t DLC){
    return (uint8_t)((DLC & 0x000F0000U) >> 16);
}

/***********************************初始化配置和参数更改****************************************/
/**
*   @brief 这个函数用来开启需要用到的所有中断
*/
void CAN_Handler_Start(void){
    HAL_FDCAN_ActivateNotification(&hfdcan1,
        FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE | FDCAN_IT_BUS_OFF
         | FDCAN_IT_RX_FIFO0_MESSAGE_LOST | FDCAN_IT_RX_FIFO1_MESSAGE_LOST
         | FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO1_NEW_MESSAGE
         | FDCAN_IT_TX_COMPLETE, 0);
    HAL_FDCAN_ConfigInterruptLines(&hfdcan1, FDCAN_IT_RX_FIFO0_MESSAGE_LOST | FDCAN_IT_RX_FIFO1_MESSAGE_LOST
         | FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO1_NEW_MESSAGE
         | FDCAN_IT_TX_COMPLETE, FDCAN_INTERRUPT_LINE1);
    HAL_FDCAN_ConfigInterruptLines(&hfdcan1, FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE 
         | FDCAN_IT_BUS_OFF, FDCAN_INTERRUPT_LINE1);
    my_can_judge.BusState = CAN_ACTIVE_ERROR_STATE;
}

/**
*   @brief 这个函数用来给用户更改can的相关配置
*   @param baud:波特率预设值
*   @param userBRP:自定义BRP,时钟分频
*   @param userBS1:自定义BS1
*   @param userBS2:自定义BS2
*   @param userSJW:自定义SJW
*   @param mode:工作模式
*   @param autosend:是否开启自动重传
*   @retval HAL_StatusTypeDef
*/
HAL_StatusTypeDef CAN_Param_Change(uint32_t baud, uint32_t userBRP, uint32_t userBS1, uint32_t userBS2,
                                    uint32_t userSJW, uint32_t mode, FunctionalState autosend){
    //停止正在进行的can通信
    if (HAL_FDCAN_Stop(&hfdcan1) != HAL_OK) return HAL_ERROR;
    //配置模式
    hfdcan1.Init.Mode = mode;
    //自动重传
    if (autosend == ENABLE) {
        hfdcan1.Init.AutoRetransmission = ENABLE;
    }else {
        hfdcan1.Init.AutoRetransmission = DISABLE;
    }
    //设置波特率
    //波特率Baud = can_clk/(BRP*(1+BS1+BS2))       
    //采样点位置 = (1+BS1)/(1+BS1+BS2)
    hfdcan1.Init.NominalPrescaler = userBRP;
    hfdcan1.Init.NominalTimeSeg1 = userBS1;
    hfdcan1.Init.NominalTimeSeg2 = userBS2;
    hfdcan1.Init.NominalSyncJumpWidth = userSJW;
    

    //重新初始化
    if(HAL_FDCAN_Init(&hfdcan1) != HAL_OK) return HAL_ERROR;
    if(HAL_FDCAN_Start(&hfdcan1) != HAL_OK) return HAL_ERROR;

    CAN_Handler_Start();
    return HAL_OK;
}

/****************************************接收发送部分****************************************/

/**
*   @brief 这个函数用来配置单次发送数据帧参数，如果后面格式不改变可以只初始化一次
*   @param id:要发送的节点id
*   @param idtype:id类型（标准帧/拓展帧）
*   @param frametype:帧类型（数据帧/遥控帧）
*   @param dlc:数据长度
*/
void My_CAN_Send_Single_Init(uint32_t id, Can_ID_Type idtype, Can_Frame_Type frametype, uint8_t dlc){

    //设置ID类型，标准帧还是拓展帧
    if (idtype == CAN_ID_EXTENDED) {
        TxHeader.IdType = FDCAN_EXTENDED_ID;
    }else if (idtype == CAN_ID_STANDARD) {
        TxHeader.IdType = FDCAN_STANDARD_ID;
    }
    //接收节点ID
    TxHeader.Identifier = id;

    //设置帧类型，数据帧还是遥控帧
    if (frametype == CAN_FRAME_REMOTE) {
        TxHeader.TxFrameType = FDCAN_REMOTE_FRAME;
    }else if (frametype == CAN_FRAME_DATA) {
        TxHeader.TxFrameType = FDCAN_DATA_FRAME;
    }

    //设置数据长度
    TxHeader.DataLength = Switch_Len_TO_DLC(dlc);

    //其他 FDCAN 必要参数（经典 CAN 模式下关闭 BRS 和 FDCAN 格式）
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
    TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker = 0;
}

//下面两个发送函数在调用之前都要先调用一次上面的初始化函数

/**
*   @brief 这个函数用来单次发送数据，将数据发送到fifo，然后再排队等硬件将数据发送出去，完成后触发回调
*   @param data:要发送的数据
*   @retval HAL_StatusTypeDef
*/
HAL_StatusTypeDef My_CAN_Send_Single(uint8_t *data){
    HAL_StatusTypeDef a;
    //将报文放入fifo0里面等待发送
    a = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, data);
    return a;
}


/**
*   @brief 当配置为循环发送数据时，在while循环里面调用这个函数
*/
void My_CAN_Send_Cycle(uint8_t *data){
    if(my_can_cycle.enable == 0)return;
    uint32_t current_tick = HAL_GetTick();      //获取当前时间
    
    if (current_tick - my_can_cycle.last_tick >= my_can_cycle.interval) {
        my_can_cycle.last_tick = current_tick;

        My_CAN_Send_Single(data);
    }
}

/**
*   @brief 这个函数用来配置接收过滤器
*   @param mode:0 = 全通模式（接收所有帧）； 1 = 精准 ID 过滤模式
*   @param targetID:当配置为过滤模式的时候目标要接收的id号 
*   @param idType:id类型
*   @param targetFIFO
*/
void CAN_Filter_Config(uint8_t mode, uint32_t targetID, Can_ID_Type idType, uint32_t targetFIFO){
    FDCAN_FilterTypeDef FilterConfig = {0};

    if (idType == CAN_ID_EXTENDED) {
        FilterConfig.IdType = FDCAN_EXTENDED_ID;
    }else if (idType == CAN_ID_STANDARD) {
        FilterConfig.IdType = FDCAN_STANDARD_ID;
    }
    FilterConfig.FilterIndex = 0;
    if (targetFIFO == 1) {
        FilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO1;
    }else if (targetFIFO == 0) {
        FilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    }

    if (mode == 0) {
        //全通模式
        FilterConfig.FilterType = FDCAN_FILTER_RANGE;
        FilterConfig.FilterID1 = 0x000;
        if (idType == CAN_ID_EXTENDED) {
            FilterConfig.FilterID2 = 0x1FFFFFFF;
        }else if (idType == CAN_ID_STANDARD) {
            FilterConfig.FilterID2 = 0x7FF;
        }
    }else {
        // 精准过滤模式
        FilterConfig.FilterType = FDCAN_FILTER_DUAL; // 匹配单个或两个特定 ID
        FilterConfig.FilterID1 = targetID;           // 只接收这个 ID
        FilterConfig.FilterID2 = (idType == CAN_ID_EXTENDED) ? 0x1FFFFFFF : 0x7FF;
    }
    
    HAL_FDCAN_ConfigFilter(&hfdcan1, &FilterConfig);

    if (mode == 0) {
        // 未匹配到过滤器的帧，默认放入 FIFO0
        HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);
    }
}

/**
*   @brief 接收处理函数，放在回调函数里面调用
*   @param hfdcan:can外设句柄
*   @param RxFifo:接收进入了哪个fifo
*/
void CAN_Rx_Deal(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo){
    FDCAN_RxHeaderTypeDef RxHeader;
    Can_Message_Struct rx_mes;

    if(HAL_FDCAN_GetRxMessage(hfdcan, RxFifo, &RxHeader, rx_mes.my_can_data) != HAL_OK)return;
    
    //解析数据
    rx_mes.can_id = RxHeader.Identifier;
    rx_mes.id_type = (RxHeader.IdType == FDCAN_EXTENDED_ID) ? CAN_ID_EXTENDED : CAN_ID_STANDARD;
    rx_mes.frametype = (RxHeader.RxFrameType == FDCAN_REMOTE_FRAME) ? CAN_FRAME_REMOTE : CAN_FRAME_DATA;
    rx_mes.DLC = Switch_Len_TO_DLC(RxHeader.DataLength);
    rx_mes.time_tick = RxHeader.RxTimestamp;                             // 获取 FDCAN 硬件自带的 16 位内部时间戳
    rx_mes.rxfifo = (RxFifo == FDCAN_RX_FIFO0) ? 0 : 1;

    CAN_RangeBuffer_Write(&my_can_rangebuffer, &rx_mes);
}

/******************************************错误检测和协议分析部分*******************************************/

/**
*   @brief 这个函数用来统计各个ID出现的频率
*   @param id:接收得到的id
*   @param dlc:数据长度
*   @param if_extend:是否是扩展帧
*/
void CAN_ID_Frequence(uint32_t id, uint8_t dlc, uint8_t if_extend){
    uint8_t find = 0;
    for (uint8_t i = 0; i<my_can_judge.UniqueIdCount; i++) {
        if (my_can_judge.IdStats[i].ArbID == id) {
            my_can_judge.IdStats[i].FrameCount++;
            find = 1;
            break;
        }
    }
    if(!find && (my_can_judge.UniqueIdCount < MAX_TRACKED_IDS)){
        my_can_judge.IdStats[my_can_judge.UniqueIdCount].ArbID = id;
        my_can_judge.IdStats[my_can_judge.UniqueIdCount].FrameCount = 1;
        my_can_judge.UniqueIdCount++;
    }
    //利用率统计的基础：估算这帧报文消耗的理论物理比特数（经典 CAN）
    uint32_t frameBits = 0;
    if (if_extend) {
        // 扩展帧基础架构大约 67 ~ 68 bits (不含填充位)
        frameBits = 68 + (dlc * 8); 
    } else {
        // 标准帧基础架构大约 44 ~ 45 bits (不含填充位)
        frameBits = 44 + (dlc * 8); 
    }
    // 简单加上约 10% 的位填充估算值
    frameBits = (uint32_t)(frameBits * 1.1f); 
    
    my_can_judge.TotalBitsCount += frameBits;
}

/**
*   @brief 这个函数用来计算错误帧发生频率和总线利用率,1秒调用一次
*   @param currentBaudRate:当前波特率，每秒比特数
*/
void CAN_Statis_Task(uint32_t currentBaudRate){
    // 计算总线利用率
    if (currentBaudRate > 0) {
        //总线利用率=过去1秒实际传输的比特数/当前波特率
        my_can_judge.BusUtilization = ((float)my_can_judge.TotalBitsCount / currentBaudRate) * 100.0f;
    }
    my_can_judge.TotalBitsCount = 0; // 清空以便下一秒重新累计

    // 计算各个 ID 的帧频率 (Hz)
    for (uint8_t i = 0; i < my_can_judge.UniqueIdCount; i++) {
        my_can_judge.IdStats[i].FrequencyHz = my_can_judge.IdStats[i].FrameCount; // 因为窗口是1秒，所以数量即Hz
        my_can_judge.IdStats[i].FrameCount = 0; // 清空
    }

    // 计算错误帧发生频率
    my_can_judge.ErrorFrameFreqHz = my_can_judge.TotalErrorFrames;
    my_can_judge.TotalErrorFrames = 0; // 清空
    
    // 在界面上输出实时状态
}


//总线与协议状态回调,当协议层发生错误进入这个回调函数
void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs){
    FDCAN_ProtocolStatusTypeDef protocolstaus;
    FDCAN_ErrorCountersTypeDef errorcounters;

    //获取协议状态
    HAL_FDCAN_GetProtocolStatus(hfdcan, &protocolstaus);
    //获取TEC和REC计数器
    HAL_FDCAN_GetErrorCounters(hfdcan, &errorcounters);
    my_can_judge.TEC_Count = errorcounters.TxErrorCnt;
    my_can_judge.REC_Count = errorcounters.RxErrorCnt;
    //获取总线状态
    if (protocolstaus.BusOff) {
        my_can_judge.BusState = CAN_BUS_OFF_STATE;
    }else if (protocolstaus.ErrorPassive) {
        my_can_judge.BusState = CAN_PASSIVE_ERROR_STATE;
    }else {
        my_can_judge.BusState = CAN_ACTIVE_ERROR_STATE;
    }

    //错误分类
    if ((ErrorStatusITs & (FDCAN_IT_ARB_PROTOCOL_ERROR)) != 0) {
        my_can_judge.TotalErrorFrames++;
        switch (protocolstaus.LastErrorCode) {
            case FDCAN_PROTOCOL_ERROR_STUFF:
                my_can_judge.StuffErrors++;
            break;
            case FDCAN_PROTOCOL_ERROR_FORM:
                my_can_judge.FormErrors++;
            break;
            case FDCAN_PROTOCOL_ERROR_ACK:
                my_can_judge.AckErrors++;
            break;
            case FDCAN_PROTOCOL_ERROR_CRC:
                my_can_judge.CRCErrors++;
            break;
            case FDCAN_PROTOCOL_ERROR_BIT0:
                my_can_judge.BitErrors++;
            break;
            case FDCAN_PROTOCOL_ERROR_BIT1:
                my_can_judge.BitErrors++;
            break;
            default: break;
        }
    }
    
}   

//系统故障与过载回调
void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *hfdcan){
    // 检查是否发生了接收 FIFO 丢帧（过载）
    if (__HAL_FDCAN_GET_IT_SOURCE(hfdcan, FDCAN_IT_RX_FIFO0_MESSAGE_LOST)) {
        my_can_judge.DroppedFrames++;
        __HAL_FDCAN_CLEAR_FLAG(hfdcan, FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST);
    }
    if (__HAL_FDCAN_GET_IT_SOURCE(hfdcan, FDCAN_IT_RX_FIFO1_MESSAGE_LOST)) {
        my_can_judge.DroppedFrames++;
        __HAL_FDCAN_CLEAR_FLAG(hfdcan, FDCAN_FLAG_RX_FIFO1_MESSAGE_LOST);
    }

    // 检查是否是 Message RAM 损坏或访问冲突
    if (__HAL_FDCAN_GET_IT_SOURCE(hfdcan, FDCAN_IT_RAM_ACCESS_FAILURE)) {
        // 属于致命硬件故障，可能需要重启外设或记录系统 Log
        __HAL_FDCAN_CLEAR_FLAG(hfdcan, FDCAN_FLAG_RAM_ACCESS_FAILURE);
    }

    // 捕捉 HAL 库自身的软件报错
    if (hfdcan->ErrorCode != HAL_FDCAN_ERROR_NONE) {
        // 可以在这里把 hfdcan->ErrorCode 打印出来用于 Debug
        // 比如常见的 HAL_FDCAN_ERROR_TIMEOUT 或 HAL_FDCAN_ERROR_PARAM
    }
}


//FIFO0接收数据回调函数
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs) {
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0) {
        CAN_Rx_Deal(hfdcan, FDCAN_RX_FIFO0);
    }
}

//FIFO1接收数据回调函数
void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo1ITs) {
    if ((RxFifo1ITs & FDCAN_IT_RX_FIFO1_NEW_MESSAGE) != 0) {
        CAN_Rx_Deal(hfdcan, FDCAN_RX_FIFO1);
    }
}

//发送结束回调函数
void HAL_FDCAN_TxEventFifoCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t TxEventFifoITs){
    if (TxEventFifoITs & FDCAN_IT_TX_COMPLETE) {
        
    }
}