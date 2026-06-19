#include "my_store.h"
#include "sdmmc.h"
#include <stdio.h>

__attribute__((aligned(32))) uint8_t tx_buf[512] = {0x03,0x04,0x01,0x04};
__attribute__((aligned(32))) uint8_t rx_buf[512];


void Verify_SDNAND(void)
{
    HAL_SD_CardInfoTypeDef CardInfo;
    uint32_t block_addr = 100; // 选择一个安全的测试块地址（比如第100块）

    // 检查底层初始化状态
    if (hsd2.State == HAL_SD_STATE_READY) {
        printf("SDNAND Base Init OK!\r\n");
    } else {
        printf("SDNAND Base Init Failed!\r\n");
        return;
    }

    // 获取芯片信息（验证主机与芯片的握手是否真正成功）
    if (HAL_SD_GetCardInfo(&hsd2, &CardInfo) == HAL_OK) {
        // 4Gbit = 512MB。在这里可以通过串口打印容量看看对不对
        printf("Card Capacity: %d MB\r\n", (int)(CardInfo.BlockNbr * CardInfo.BlockSize / 1024 / 1024));
        printf("Block Size: %d Bytes\r\n", (int)CardInfo.BlockSize);
    } else {
        printf("Get Card Info Failed!\r\n");
        return;
    }

    // 准备测试数据（填充自增数据）
    for (int i = 0; i < 512; i++) {
        tx_buf[i] = i & 0xFF;
        rx_buf[i] = 0; // 清空接收区
    }
    
    SCB_CleanDCache_by_Addr((uint32_t*)tx_buf, 512);

    // 写入一个块的数据（使用轮询阻塞模式即可，方便测试）
    if (HAL_SD_WriteBlocks(&hsd2, tx_buf, block_addr, 1, 1000) == HAL_OK) {
        printf("Write Block Command Sent.\r\n");
        // 等待芯片内部擦写搬移完成（从编程状态恢复到传输状态）
        while (HAL_SD_GetCardState(&hsd2) != HAL_SD_CARD_TRANSFER);
        printf("Write Block Success!\r\n");
    } else {
        printf("Write Block Failed!\r\n");
        return;
    }

    // 读取刚刚写入的块
    if (HAL_SD_ReadBlocks(&hsd2, rx_buf, block_addr, 1, 1000) == HAL_OK) {
        while (HAL_SD_GetCardState(&hsd2) != HAL_SD_CARD_TRANSFER);
        
        SCB_InvalidateDCache_by_Addr((uint32_t*)rx_buf, 512);

        printf("Read Block Success!\r\n");
    } else {
        printf("Read Block Failed!\r\n");
        return;
    }

    // 校验数据是否一致
    if (memcmp(tx_buf, rx_buf, 512) == 0) {
        printf(">>>> SDNAND Read/Write Verification PASSED! <<<<\r\n");
    } else {
        printf(">>>> Data Mismatch! Verification FAILED! <<<<\r\n");
    }

    printf("%x,%x,%x,%x,%x\r\n",rx_buf[0],rx_buf[1],rx_buf[2],rx_buf[3],rx_buf[4]);
}
