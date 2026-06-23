#include <gui/data_screen_screen/Data_screenView.hpp>
#include <touchgfx/Color.hpp>
#include <stdio.h>
#include <texts/TextKeysAndLanguages.hpp>
#include <images/BitmapDatabase.hpp>   /* 按钮 bitmap ID（modal 按钮）*/
#include <main.h>   /* g_playback_*/g_record_*/g_file_* 全局 */
#include <shared_buf.h>
#include "stm32h7xx.h"   /* SCB_InvalidateDCache_by_Addr（MPU non-cacheable 未生效，手动 invalidate）*/

Data_screenView::Data_screenView()
    : menuClickCb(this, &Data_screenView::onMenuClick),
      uartClickCb(this, &Data_screenView::onUartClick),
      spiClickCb(this, &Data_screenView::onSpiClick),
      i2cClickCb(this, &Data_screenView::onI2cClick),
      canClickCb(this, &Data_screenView::onCanClick),
      backCb(this, &Data_screenView::onBackClick),
      keepCb(this, &Data_screenView::onKeepClick),
      discardCb(this, &Data_screenView::onDiscardClick),
      cancelCb(this, &Data_screenView::onCancelClick),
      pbCb(this, &Data_screenView::onPauseClick),
      prevCb(this, &Data_screenView::onPrevClick),
      nextCb(this, &Data_screenView::onNextClick),
      stopCb(this, &Data_screenView::onStopClick)
{
    shmCountBuf[0] = 0;
    modalBuf[0] = 0;
    modalPending = PENDING_NONE;
    modalSwitchProto = 0;
    modalState = MODAL_HIDDEN;
    resultTicks = 0;
    playbackUiShown = false;
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
    /* 覆盖基类 back_data 跳转：记录中先弹 modal 确认 */
    back_data.setAction(backCb);
    /* Container 初始隐藏 */
    choose_contain.setVisible(false);
    choose_contain.invalidate();

    /* —— 确认 modal（记录中点 back/再点当前协议 → 保存/不保存/取消）——
     * 等效官方 ModalWindow：遮罩 touchable 拦截底层 touch，最后 add 保证最上层。
     * setVisible(false) 时子 widget 不参与 hit-test，隐藏时不拦截。*/
    modalShade.setPosition(0, 0, 800, 480);
    modalShade.setColor(touchgfx::Color::getColorFromRGB(0, 0, 0));
    modalShade.setAlpha(120);
    modalShade.setTouchable(true);
    modalOverlay.add(modalShade);

    modalPanel.setPosition(200, 160, 400, 160);
    modalPanel.setColor(touchgfx::Color::getColorFromRGB(60, 60, 60));
    modalOverlay.add(modalPanel);

    modalText.setPosition(220, 170, 360, 40);
    modalText.setTypedText(TypedText(T___SINGLEUSE_T387));
    modalText.setWildcard(modalBuf);
    modalText.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    touchgfx::Unicode::strncpy(modalBuf, "Save recording?", 32);
    modalOverlay.add(modalText);

    /* 3 按钮（ButtonWithLabel 当 Button 用——这个版本 setLabelText 只接受 TypedText，
     * 自定义文字用 TextArea 叠加在按钮上，TextArea 默认 touchable=false 点击穿透）*/
    const touchgfx::Bitmap btnN(BITMAP_ALTERNATE_THEME_IMAGES_WIDGETS_BUTTON_REGULAR_HEIGHT_36_TINY_ROUNDED_NORMAL_ID);
    const touchgfx::Bitmap btnP(BITMAP_ALTERNATE_THEME_IMAGES_WIDGETS_BUTTON_REGULAR_HEIGHT_36_TINY_ROUNDED_PRESSED_ID);

    btnKeep.setXY(230, 235);
    btnKeep.setBitmaps(btnN, btnP);
    btnKeep.setAction(keepCb);
    modalOverlay.add(btnKeep);

    btnDiscard.setXY(340, 235);
    btnDiscard.setBitmaps(btnN, btnP);
    btnDiscard.setAction(discardCb);
    modalOverlay.add(btnDiscard);

    btnCancel.setXY(450, 235);
    btnCancel.setBitmaps(btnN, btnP);
    btnCancel.setAction(cancelCb);
    modalOverlay.add(btnCancel);

    /* 按钮标签（叠加在按钮上，wildcard 填自定义文字）*/
    touchgfx::Unicode::strncpy(keepLblBuf,    "Save",    16);
    touchgfx::Unicode::strncpy(discardLblBuf, "Delete", 16);
    touchgfx::Unicode::strncpy(cancelLblBuf,  "Cancel",  16);
    keepText.setPosition(238, 237, 84, 32);
    keepText.setTypedText(TypedText(T___SINGLEUSE_T387));
    keepText.setWildcard(keepLblBuf);
    keepText.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    modalOverlay.add(keepText);
    discardText.setPosition(348, 237, 84, 32);
    discardText.setTypedText(TypedText(T___SINGLEUSE_T387));
    discardText.setWildcard(discardLblBuf);
    discardText.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    modalOverlay.add(discardText);
    cancelText.setPosition(458, 237, 84, 32);
    cancelText.setTypedText(TypedText(T___SINGLEUSE_T387));
    cancelText.setWildcard(cancelLblBuf);
    cancelText.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    modalOverlay.add(cancelText);

    /* Container 必须设全屏尺寸：setVisible+invalidate 的重绘区域 = Container rect，
     * 默认 0×0 时子 widget 不会被重绘（但 touchable 仍拦截 → 看不到 modal 却点不动 = 卡死假象）*/
    modalOverlay.setPosition(0, 0, 800, 480);
    modalOverlay.setVisible(false);
    add(modalOverlay);   /* 最后 add = 最上层 */

    /* 回放控制按钮（g_playback_mode=1 时显示，初始隐藏；handleTickEvent 检测 mode 切换）*/
    const touchgfx::Bitmap pbN(BITMAP_ALTERNATE_THEME_IMAGES_WIDGETS_BUTTON_REGULAR_HEIGHT_36_TINY_ROUNDED_NORMAL_ID);
    const touchgfx::Bitmap pbP(BITMAP_ALTERNATE_THEME_IMAGES_WIDGETS_BUTTON_REGULAR_HEIGHT_36_TINY_ROUNDED_PRESSED_ID);
    pbBtn.setXY(50, 410);   pbBtn.setBitmaps(pbN, pbP);   pbBtn.setAction(pbCb);   add(pbBtn);
    prevBtn.setXY(170, 410); prevBtn.setBitmaps(pbN, pbP); prevBtn.setAction(prevCb); add(prevBtn);
    nextBtn.setXY(290, 410); nextBtn.setBitmaps(pbN, pbP); nextBtn.setAction(nextCb); add(nextBtn);
    stopBtn.setXY(410, 410); stopBtn.setBitmaps(pbN, pbP); stopBtn.setAction(stopCb); add(stopBtn);
    touchgfx::Unicode::strncpy(pbLblBuf, "Run", 12);      /* 进回放默认暂停，按钮显示 Run（点后切换）*/
    touchgfx::Unicode::strncpy(prevLblBuf, "Prev", 12);
    touchgfx::Unicode::strncpy(nextLblBuf, "Next", 12);
    touchgfx::Unicode::strncpy(stopLblBuf, "Stop", 12);
    pbText.setPosition(60, 412, 80, 32);   pbText.setTypedText(TypedText(T___SINGLEUSE_T387));   pbText.setWildcard(pbLblBuf);   pbText.setColor(touchgfx::Color::getColorFromRGB(255,255,255)); add(pbText);
    prevText.setPosition(180, 412, 80, 32); prevText.setTypedText(TypedText(T___SINGLEUSE_T387)); prevText.setWildcard(prevLblBuf); prevText.setColor(touchgfx::Color::getColorFromRGB(255,255,255)); add(prevText);
    nextText.setPosition(300, 412, 80, 32); nextText.setTypedText(TypedText(T___SINGLEUSE_T387)); nextText.setWildcard(nextLblBuf); nextText.setColor(touchgfx::Color::getColorFromRGB(255,255,255)); add(nextText);
    stopText.setPosition(420, 412, 80, 32); stopText.setTypedText(TypedText(T___SINGLEUSE_T387)); stopText.setWildcard(stopLblBuf); stopText.setColor(touchgfx::Color::getColorFromRGB(255,255,255)); add(stopText);
    pbBtn.setVisible(false); prevBtn.setVisible(false); nextBtn.setVisible(false); stopBtn.setVisible(false);
    pbText.setVisible(false); prevText.setVisible(false); nextText.setVisible(false); stopText.setVisible(false);
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

    /* modal 结果倒计时：Saved!/Discarded! 显示一会儿后 hide + 执行 pending */
    if (modalState == MODAL_RESULT)
    {
        if (--resultTicks <= 0)
        {
            int pend = modalPending;
            hideConfirmModal();
            if (pend == PENDING_BACK) application().gotostart_screenScreenNoTransition();
        }
        return;   /* 结果期间不刷新波形 */
    }

    /* 回放模式（g_playback_mode=1）：画文件字节，忽略共享内存实时数据 */
    if (g_playback_mode)
    {
        if (!playbackUiShown)
        {
            playbackUiShown = true;
            choose.setVisible(false);        choose.invalidate();
            choose_contain.setVisible(false); choose_contain.invalidate();
            back_data.setVisible(false);     back_data.invalidate();
            pbBtn.setVisible(true);  pbBtn.invalidate();
            prevBtn.setVisible(true); prevBtn.invalidate();
            nextBtn.setVisible(true); nextBtn.invalidate();
            stopBtn.setVisible(true); stopBtn.invalidate();
            pbText.setVisible(true);  pbText.invalidate();
            prevText.setVisible(true); prevText.invalidate();
            nextText.setVisible(true); nextText.invalidate();
            stopText.setVisible(true); stopText.invalidate();
        }
        char pbtmp[48];
        snprintf(pbtmp, sizeof(pbtmp), "PLAY %lu/%lu",
                 (unsigned long)g_playback_pos, (unsigned long)g_playback_len);
        touchgfx::Unicode::strncpy(shmCountBuf, pbtmp, sizeof(shmCountBuf)/sizeof(shmCountBuf[0]));
        shmCountText.setWildcard(shmCountBuf);
        shmCountText.invalidate();
        /* 波形每 5 tick 画一次 + 自动推进（!pause）/ 手动 step */
        static int pb_div = 0;
        if (++pb_div < 5) return;
        pb_div = 0;
        /* !pause 时自动推进（Run 模式）；Prev/Next 按钮直接改 pos，不需这里处理 */
        if (!g_playback_pause && g_playback_pos < g_playback_len) g_playback_pos++;
        if (g_playback_pos >= g_playback_len) g_playback_stop = 1;   /* 回放完，defaultTask 关文件 */
        const int N = 6;
        uint8_t bytes[N];
        for (int i = 0; i < N; i++)
        {
            uint32_t p = g_playback_pos + i;
            bytes[i] = (p < g_playback_len) ? g_playback_buf[p] : 0;
        }
        waveWidget.setBytes(bytes, N);
        waveWidget.invalidate();
        return;
    }
    /* 回放刚结束（mode 1→0）且 UI 还是回放态 → 恢复实时 UI */
    if (playbackUiShown)
    {
        playbackUiShown = false;
        choose.setVisible(true);    choose.invalidate();
        back_data.setVisible(true); back_data.invalidate();
        pbBtn.setVisible(false);  pbBtn.invalidate();
        prevBtn.setVisible(false); prevBtn.invalidate();
        nextBtn.setVisible(false); nextBtn.invalidate();
        stopBtn.setVisible(false); stopBtn.invalidate();
        pbText.setVisible(false);  pbText.invalidate();
        prevText.setVisible(false); prevText.invalidate();
        nextText.setVisible(false); nextText.invalidate();
        stopText.setVisible(false); stopText.invalidate();
    }

    /* modal 确认中暂停波形（否则波形刷新盖住 modal 面板）*/
    if (modalState == MODAL_CONFIRM) return;

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

/* 4 协议按钮 → 记录中(同协议)弹 modal 确认停止，否则设 g_record_req 开始/切换 */
void Data_screenView::onUartClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_req;
    extern volatile uint8_t g_record_active;
    if (g_record_active == 1) { showConfirmModal(PENDING_NONE); return; }
    if (g_record_active != 0) { modalSwitchProto = 1; showConfirmModal(PENDING_SWITCH); return; }
    g_record_req = 1;
}
void Data_screenView::onSpiClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_req;
    extern volatile uint8_t g_record_active;
    if (g_record_active == 2) { showConfirmModal(PENDING_NONE); return; }
    if (g_record_active != 0) { modalSwitchProto = 2; showConfirmModal(PENDING_SWITCH); return; }
    g_record_req = 2;
}
void Data_screenView::onI2cClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_req;
    extern volatile uint8_t g_record_active;
    if (g_record_active == 3) { showConfirmModal(PENDING_NONE); return; }
    if (g_record_active != 0) { modalSwitchProto = 3; showConfirmModal(PENDING_SWITCH); return; }
    g_record_req = 3;
}
void Data_screenView::onCanClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_req;
    extern volatile uint8_t g_record_active;
    if (g_record_active == 4) { showConfirmModal(PENDING_NONE); return; }
    if (g_record_active != 0) { modalSwitchProto = 4; showConfirmModal(PENDING_SWITCH); return; }
    g_record_req = 4;
}

/* back 按钮：记录中弹 modal（pending=BACK，确认后回主屏），否则直接回主屏 */
void Data_screenView::onBackClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_active;
    if (g_record_active != 0) showConfirmModal(PENDING_BACK);
    else application().gotostart_screenScreenNoTransition();
}

/* modal"保存"：停止+保留 → 显示"Saved!"结果，倒计时后 hide+执行 pending（在 handleTickEvent）*/
void Data_screenView::onKeepClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_req;
    extern volatile uint8_t g_record_active;
    extern volatile uint8_t g_record_discard;
    g_record_discard = 0;
    /* PENDING_SWITCH → 切到目标协议；NONE/BACK → 停止当前 */
    g_record_req = (modalPending == PENDING_SWITCH) ? modalSwitchProto : g_record_active;
    touchgfx::Unicode::strncpy(modalBuf, "Saved!", 32);
    modalText.setWildcard(modalBuf);
    modalText.invalidate();
    btnKeep.setVisible(false);    btnKeep.invalidate();
    btnDiscard.setVisible(false); btnDiscard.invalidate();
    btnCancel.setVisible(false);  btnCancel.invalidate();
    modalState = MODAL_RESULT;
    resultTicks = 90;   /* ~1.5 秒（60Hz tick），让用户看清结果 */
}

/* modal"不保存"：停止+删除 → 显示"Discarded!"结果，倒计时后 hide+执行 pending */
void Data_screenView::onDiscardClick(const touchgfx::AbstractButton&)
{
    extern volatile uint8_t g_record_req;
    extern volatile uint8_t g_record_active;
    extern volatile uint8_t g_record_discard;
    g_record_discard = 1;                /* 删除（defaultTask f_unlink）*/
    /* PENDING_SWITCH → 切到目标协议（删当前后）；NONE/BACK → 停止当前 */
    g_record_req = (modalPending == PENDING_SWITCH) ? modalSwitchProto : g_record_active;
    touchgfx::Unicode::strncpy(modalBuf, "Deleted!", 32);
    modalText.setWildcard(modalBuf);
    modalText.invalidate();
    btnKeep.setVisible(false);    btnKeep.invalidate();
    btnDiscard.setVisible(false); btnDiscard.invalidate();
    btnCancel.setVisible(false);  btnCancel.invalidate();
    modalState = MODAL_RESULT;
    resultTicks = 90;
}

/* modal"取消"：继续记录，仅 hide modal */
void Data_screenView::onCancelClick(const touchgfx::AbstractButton&)
{
    hideConfirmModal();
}

void Data_screenView::showConfirmModal(int pending)
{
    modalPending = (pending == PENDING_BACK) ? PENDING_BACK : PENDING_NONE;
    touchgfx::Unicode::strncpy(modalBuf, "Save recording?", 32);
    modalText.setWildcard(modalBuf);
    btnKeep.setVisible(true);
    btnDiscard.setVisible(true);
    btnCancel.setVisible(true);
    /* 彻底隐藏波形：否则半透明遮罩下波形透出、面板外残留 */
    waveWidget.setVisible(false);
    waveWidget.invalidate();
    modalOverlay.setVisible(true);
    modalOverlay.invalidate();
    modalState = MODAL_CONFIRM;
}

void Data_screenView::hideConfirmModal()
{
    modalOverlay.setVisible(false);
    /* 显式 invalidate 各子 widget 区域：modalText/按钮标签延伸到 x=580，但波形只到 x=500，
     * 波形区域外的部分不被 waveWidget 重绘覆盖，必须单独 invalidate 才能清残留字符 */
    modalText.invalidate();
    btnKeep.invalidate();
    btnDiscard.invalidate();
    btnCancel.invalidate();
    modalPanel.invalidate();
    modalShade.invalidate();
    modalOverlay.invalidate();
    /* 恢复波形 + 重绘其区域 */
    waveWidget.setVisible(true);
    waveWidget.invalidate();
    modalPending = PENDING_NONE;
    modalSwitchProto = 0;
    modalState = MODAL_HIDDEN;
}

/* Run/Pause 切换：进回放默认 Pause（按钮 Run），点后 pause=0 自动播放，标签变 Pause */
void Data_screenView::onPauseClick(const touchgfx::AbstractButton&)
{
    g_playback_pause = !g_playback_pause;
    if (g_playback_mode) {
        touchgfx::Unicode::strncpy(pbLblBuf, g_playback_pause ? "Run" : "Pause", 12);
        pbText.setWildcard(pbLblBuf);
        pbText.invalidate();
    }
}
/* 上一步：pos--（不低于 0）*/
void Data_screenView::onPrevClick(const touchgfx::AbstractButton&)
{
    if (g_playback_pos > 0) g_playback_pos--;
}
/* 下一步：pos++（到 len 停）*/
void Data_screenView::onNextClick(const touchgfx::AbstractButton&)
{
    if (g_playback_pos < g_playback_len) g_playback_pos++;
}
void Data_screenView::onStopClick(const touchgfx::AbstractButton&)
{
    g_playback_stop = 1;
    application().gotostart_screenScreenNoTransition();
}
