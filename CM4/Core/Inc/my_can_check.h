#ifndef __MY_CAN_CHECK_H
#define __MY_CAN_CHECK_H


#include "stm32h7xx_hal.h"
#include "fdcan.h"
#include <stdint.h>


#define MAX_CAN_DATA_LEN   8        //一个数据包里面最大数据长度
#define CAN_RANGER_BUFFER_LEN   100 //环形缓冲区长度

#define MAX_TRACKED_IDS     10      //最多接收的id数量

//仲裁段里面的帧类型，数据帧/遥控帧
typedef enum{
    CAN_FRAME_DATA,             // 数据帧
    CAN_FRAME_REMOTE            // 遥控帧
} Can_Frame_Type;

//仲裁段里面的ID类型
typedef enum{
    CAN_ID_STANDARD,            // 标准帧
    CAN_ID_EXTENDED             // 拓展帧
} Can_ID_Type;

//can的报文结构
typedef struct{
    uint32_t time_tick;         // 时间戳
    uint32_t can_id;            // 11位或者29位id
    Can_ID_Type id_type;        // ID类型
    Can_Frame_Type frametype;   // 帧类型
    uint8_t DLC;                // 数据长度
    uint8_t my_can_data[MAX_CAN_DATA_LEN];       //数据内容
    uint8_t rxfifo;             // 标记来自fifo0还是fifo1
} Can_Message_Struct;

//环形缓冲区,用来存放数据包
typedef struct{
    Can_Message_Struct buffer[CAN_RANGER_BUFFER_LEN];
    uint16_t head;              // 
    uint16_t tail;
    uint16_t count;
} Can_Range_Buffer;

//循环发送配置结构体
typedef struct{
    uint8_t enable;             // 是否使用循环发送
    uint32_t interval;          // 发送时间间隔
    uint32_t last_tick;         // 上一次发送时间
} Can_Cycle;

//总线状态
typedef enum{
    CAN_ACTIVE_ERROR_STATE,           // 主动错误状态
    CAN_PASSIVE_ERROR_STATE,          // 被动错误状态
    CAN_BUS_OFF_STATE                 // 总线关闭
} Can_Bus_State;

//ID频率统计结构体
typedef struct{
    uint32_t ArbID;
    uint32_t FrameCount;
    uint32_t FrequencyHz; 
} ID_Frequency;

//综合结构体
typedef struct {
    //错误计数与状态
    uint8_t TEC_Count;
    uint8_t REC_Count;
    Can_Bus_State BusState;
    uint8_t Alarm_Sign;     // 告警标志
    
    //错误帧类型计数器
    uint32_t BitErrors;         // 位错误
    uint32_t StuffErrors;       // 填充错误
    uint32_t CRCErrors;         // CRC校验错误 
    uint32_t FormErrors;        // 格式错误
    uint32_t AckErrors;         // 应答错误
    uint32_t TotalErrorFrames;  // 总共发生错误次数
    uint32_t ErrorFrameFreqHz;   // 错误帧发生频率
    
    // 3. 协议层统计
    uint32_t TotalBitsCount;     // 用于计算利用率的累计总比特数
    float    BusUtilization;     // 总线利用率 (%)
    uint32_t DroppedFrames;      // FIFO 溢出丢帧计数
    
    // 4. 各 ID 频率统计
    ID_Frequency IdStats[MAX_TRACKED_IDS];
    uint8_t  UniqueIdCount;       // 独特ID计数
} Can_Analyse;


#endif
