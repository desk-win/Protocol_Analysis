#include <gui/data_screen_screen/Data_screenView.hpp>
#include <touchgfx/Color.hpp>
#include <stdio.h>
#include <texts/TextKeysAndLanguages.hpp>
#include <shared_buf.h>
#include "stm32h7xx.h"   /* SCB_InvalidateDCache_by_Addr（MPU non-cacheable 未生效，手动 invalidate）*/

Data_screenView::Data_screenView()
    : menuClickCb(this, &Data_screenView::onMenuClick),
      uartClickCb(this, &Data_screenView::onUartClick),
      spiClickCb(this, &Data_screenView::onSpiClick),
      i2cClickCb(this, &Data_screenView::onI2cClick),
      canClickCb(this, &Data_screenView::onCanClick)
{
    shmCountBuf[0] = 0;
}

void Data_screenView::setupScreen()
{
    Data_screenViewBase::setupScreen();

    /* 显示 CM4→CM7 共享内存收到的字节数（验证双向通路） */
    shmCountText.setPosition(50, 150, 500, 32);
    shmCountText.setTypedText(TypedText(T___SINGLEUSE_T387));  /* wildcard 文本 "<>" */
    shmCountText.setWildcard(shmCountBuf);
    shmCountText.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    add(shmCountText);

    /* 时序波形：宽 450（裁剪长度）+ 高 100（恢复），右侧 X500+ 留给按钮 */
    waveWidget.setPosition(50, 185, 450, 100);
    waveWidget.setParams(6, 20, 70, touchgfx::Color::getColorFromRGB(0, 255, 0), touchgfx::Color::getColorFromRGB(0, 0, 0), 10);  /* bit 6px，highY 20 lowY 70（widget 100 内）*/
    add(waveWidget);

    /* "协议选择"按钮 toggle Container 显示/隐藏（Callback 成员在构造函数初始化）*/
    choose.setAction(menuClickCb);
    /* 4 协议按钮 → g_record_req（defaultTask 开始/停止记录）*/
    UART.setAction(uartClickCb);
    SPI.setAction(spiClickCb);
    I2C.setAction(i2cClickCb);
    CAN.setAction(canClickCb);
    /* Container 初始隐藏 */
    choose_contain.setVisible(false);
    choose_contain.invalidate();
}

void Data_screenView::tearDownScreen()
{
    Data_screenViewBase::tearDownScreen();
}

void Data_screenView::handleTickEvent()
{
    Data_screenViewBase::handleTickEvent();

    extern volatile uint32_t g_shm_rx_count;
    extern volatile uint32_t g_sd_written;
    extern volatile uint32_t g_sd_status;     /* SD 状态 4=就绪 */
    extern volatile uint8_t  g_record_active; /* 当前记录 0=没记 1-4=协议 */
    char tmp[48];
    snprintf(tmp, sizeof(tmp), "st=%lu sd=%lu rec=%u", (unsigned long)g_sd_status, (unsigned long)g_sd_written, (unsigned)g_record_active);
    touchgfx::Unicode::strncpy(shmCountBuf, tmp, sizeof(shmCountBuf) / sizeof(shmCountBuf[0]));
    shmCountText.setWildcard(shmCountBuf);
    shmCountText.invalidate();

    /* 波形每 5 tick 刷新一次（约 80ms），降低刷新频率减少清屏/画线之间的闪烁 */
    static int wave_div = 0;
    if (++wave_div < 5) return;
    wave_div = 0;

    /* 读共享内存最新 10 字节，画时序波形。
     * 必须 invalidate cache——MPU non-cacheable 未生效，CM7 在缓存 SRAM1，
     * 不 invalidate 会读到旧值（波形只更新一次就不动）。*/
    SCB_InvalidateDCache_by_Addr((uint32_t*)SHM_BUF_ADDR, SHM_BUF_SIZE + 64);
    uint16_t head = SHM_RING->head;
    const int N = 6;
    uint8_t bytes[N];
    for (int i = 0; i < N; i++)
    {
        uint16_t pos = (uint16_t)((head - N + i + SHM_BUF_SIZE) % SHM_BUF_SIZE);
        bytes[i] = SHM_RING->data[pos];
    }
    waveWidget.setBytes(bytes, N);
    waveWidget.invalidate();
}

/* "协议选择"按钮：toggle Container 显示/隐藏（4 协议按钮）*/
void Data_screenView::onMenuClick(const touchgfx::AbstractButton&)
{
    choose_contain.setVisible(!choose_contain.isVisible());
    choose_contain.invalidate();
}

/* 4 协议按钮 → 设置 g_record_req，defaultTask 开始/停止记录对应协议 */
void Data_screenView::onUartClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_req;
    g_record_req = 1;
}
void Data_screenView::onSpiClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_req;
    g_record_req = 2;
}
void Data_screenView::onI2cClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_req;
    g_record_req = 3;
}
void Data_screenView::onCanClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_req;
    g_record_req = 4;
}
