#include <gui/settings_screen_screen/Settings_ScreenView.hpp>
#include <touchgfx/Color.hpp>
#include <images/BitmapDatabase.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include <stdio.h>
#include <shared_config.h>
#include "stm32h7xx.h"   /* SCB_Clean/InvalidateDCache_by_Addr（SHM_CONFIG 在 SRAM1，CM7 DCache 维护）*/

/* CM7 main.c 提供的 HSEM 通知 shim（C++ 不直接碰 HAL，通过 extern "C" 调）*/
extern "C" void shm_config_notify(void);

/* ===== 档位表（UI idx → 实际值/显示，applyConfig 映射写 SHM_CONFIG）===== */
/* UART */
static const uint32_t u_baud[8]  = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
static const uint8_t  u_data[3]  = {7, 8, 9};                 /* STM32 USART 只支持 7/8/9 bit */
static const uint8_t  u_stop[2]  = {1, 2};                    /* 常规 USART 只 1/2（1.5 仅智能卡模式）*/
static const char*    u_stopD[2] = {"1", "2"};
static const char*    u_par[3]   = {"None", "Even", "Odd"};
static const char*    u_flow[4]  = {"None", "RTS", "CTS", "RTS/CTS"};
/* SPI */
static const char*    s_modeD[4] = {"0", "1", "2", "3"};
static const uint8_t  s_data[5]  = {4, 5, 6, 7, 8};        /* v1 ≤8：BDMA byte 对齐限制（spec §8 #1）*/
static const char*    s_roleD[2] = {"Slave", "Master"};
static const uint32_t s_baud[6]  = {468750, 937500, 1875000, 3750000, 7500000, 15000000};  /* SPI6=120MHz /{256,128,64,32,16,8} */
static const char*    s_first[2] = {"MSB", "LSB"};
/* I2C */
static const uint32_t i_clk[3]   = {100000, 400000, 1000000};
static const char*    i_addrD[2] = {"7-bit", "10-bit"};
/* CAN */
static const uint32_t c_baud[5]  = {50000, 125000, 250000, 500000, 1000000};
static const char*    c_modeD[4] = {"Normal", "Loopback", "Silent", "LB+Silent"};
static const char*    protoNm[4] = {"UART", "SPI", "I2C", "CAN"};

Settings_ScreenView::Settings_ScreenView()
    : upCb(this, &Settings_ScreenView::onUpClick),
      downCb(this, &Settings_ScreenView::onDownClick),
      plusCb(this, &Settings_ScreenView::onPlusClick),
      minusCb(this, &Settings_ScreenView::onMinusClick),
      backCb(this, &Settings_ScreenView::onBackClick),
      applyCb(this, &Settings_ScreenView::onApplyClick),
      discardCb(this, &Settings_ScreenView::onDiscardClick),
      cancelCb(this, &Settings_ScreenView::onCancelClick),
      selRow(0), protoIdx(0),
      uBaud(4), uData(1), uStop(0), uPar(0), uFlow(0),
      sMode(0), sData(4), sBaud(3), sFirst(0), sRole(0),
      iClock(0), iAddr(0),
      cBaud(3), cMode(0),
      modalState(MODAL_HIDDEN), pendingAction(PENDING_BACK), switchDir(0), resultTicks(0)
{
}

void Settings_ScreenView::setupScreen()
{
    Settings_ScreenViewBase::setupScreen();

    /* 背景改深灰（box1 默认金色，白字对比不够）*/
    box1.setColor(touchgfx::Color::getColorFromRGB(40, 40, 40));
    box1.invalidate();

    selRow = 0;
    /* invalidate DCache：SHM_CONFIG 在 SRAM1，CM7 DCache 可能缓存旧值（MPU non-cacheable 历史未完全生效，
     * 参见 data_screen 的 SCB_InvalidateDCache_by_Addr）。不 invalidate → Apply 后再进 Settings 显示旧配置。*/
    SCB_InvalidateDCache_by_Addr((uint32_t*)SHM_CONFIG_ADDR, sizeof(proto_config_t) + 32);
    protoIdx = (SHM_CONFIG->active_proto >= 1 && SHM_CONFIG->active_proto <= 4) ? (SHM_CONFIG->active_proto - 1) : 0;
    /* UART idx 从 SHM_CONFIG 映射（其他协议用默认 idx，首次乱值不映射）*/
    uint32_t b = SHM_CONFIG->uart.baudrate;
    uBaud = 4; for (uint8_t i = 0; i < 8; i++) if (u_baud[i] == b) { uBaud = i; break; }
    uint8_t d = SHM_CONFIG->uart.databits;
    uData = 1; for (uint8_t i = 0; i < 3; i++) if (u_data[i] == d) { uData = i; break; }
    uint8_t s = SHM_CONFIG->uart.stopbits;
    uStop = 0; for (uint8_t i = 0; i < 2; i++) if (u_stop[i] == s) { uStop = i; break; }
    uPar  = (SHM_CONFIG->uart.parity < 3)      ? SHM_CONFIG->uart.parity      : 0;
    uFlow = (SHM_CONFIG->uart.flowcontrol < 4) ? SHM_CONFIG->uart.flowcontrol : 0;
    /* SPI idx 从 SHM_CONFIG 映射（再进 Settings 显示保存值，不只 UART）*/
    { uint32_t sb = SHM_CONFIG->spi.baudrate;
      sBaud = 3; for (uint8_t i = 0; i < 6; i++) if (s_baud[i] == sb) { sBaud = i; break; } }
    { uint8_t sd = SHM_CONFIG->spi.datasize;
      sData = 4; for (uint8_t i = 0; i < 5; i++) if (s_data[i] == sd) { sData = i; break; } }
    sRole = (SHM_CONFIG->spi.role < 2) ? SHM_CONFIG->spi.role : 0;
    sMode  = (SHM_CONFIG->spi.mode < 4)     ? SHM_CONFIG->spi.mode     : 0;
    sFirst = (SHM_CONFIG->spi.firstbit < 2) ? SHM_CONFIG->spi.firstbit : 0;
    /* I2C idx 从 SHM_CONFIG 映射 */
    { uint32_t ic = SHM_CONFIG->i2c.clock_speed;
      iClock = 0; for (uint8_t i = 0; i < 3; i++) if (i_clk[i] == ic) { iClock = i; break; } }
    iAddr = (SHM_CONFIG->i2c.addressing < 2) ? SHM_CONFIG->i2c.addressing : 0;
    /* CAN idx 从 SHM_CONFIG 映射 */
    { uint32_t cb = SHM_CONFIG->can.baudrate;
      cBaud = 3; for (uint8_t i = 0; i < 5; i++) if (c_baud[i] == cb) { cBaud = i; break; } }
    cMode = (SHM_CONFIG->can.mode < 4) ? SHM_CONFIG->can.mode : 0;

    /* snap：保存进屏幕时的所有 idx，onBackClick 对比用（refreshAll 不写 SHM_CONFIG，cancel 自然回退）*/
    snap.protoIdx = protoIdx;
    snap.uBaud = uBaud; snap.uData = uData; snap.uStop = uStop; snap.uPar = uPar; snap.uFlow = uFlow;
    snap.sMode = sMode; snap.sData = sData; snap.sBaud = sBaud; snap.sFirst = sFirst; snap.sRole = sRole;
    snap.iClock = iClock; snap.iAddr = iAddr;
    snap.cBaud = cBaud; snap.cMode = cMode;

    const touchgfx::colortype white = touchgfx::Color::getColorFromRGB(255, 255, 255);
    /* 行 0 协议 + 行 1-5 参数 */
    protoRow.setPosition(50, 60, 600, 40);  protoRow.setTypedText(TypedText(T___SINGLEUSE_T387));  protoRow.setWildcard(protoBuf);  protoRow.setColor(white); add(protoRow);
    row1.setPosition(50, 110, 600, 40);     row1.setTypedText(TypedText(T___SINGLEUSE_T387));     row1.setWildcard(row1Buf);       row1.setColor(white);  add(row1);
    row2.setPosition(50, 150, 600, 40);     row2.setTypedText(TypedText(T___SINGLEUSE_T387));     row2.setWildcard(row2Buf);       row2.setColor(white);  add(row2);
    row3.setPosition(50, 190, 600, 40);     row3.setTypedText(TypedText(T___SINGLEUSE_T387));     row3.setWildcard(row3Buf);       row3.setColor(white);  add(row3);
    row4.setPosition(50, 230, 600, 40);     row4.setTypedText(TypedText(T___SINGLEUSE_T387));     row4.setWildcard(row4Buf);       row4.setColor(white);  add(row4);
    row5.setPosition(50, 270, 600, 40);     row5.setTypedText(TypedText(T___SINGLEUSE_T387));     row5.setWildcard(row5Buf);       row5.setColor(white);  add(row5);

    /* 4 控制按钮 Up/Down/+/- */
    const touchgfx::Bitmap bN(BITMAP_ALTERNATE_THEME_IMAGES_WIDGETS_BUTTON_REGULAR_HEIGHT_36_TINY_ROUNDED_NORMAL_ID);
    const touchgfx::Bitmap bP(BITMAP_ALTERNATE_THEME_IMAGES_WIDGETS_BUTTON_REGULAR_HEIGHT_36_TINY_ROUNDED_PRESSED_ID);
    upBtn.setXY(150, 350);    upBtn.setBitmaps(bN, bP);    upBtn.setAction(upCb);    add(upBtn);
    downBtn.setXY(270, 350);  downBtn.setBitmaps(bN, bP);  downBtn.setAction(downCb); add(downBtn);
    plusBtn.setXY(430, 350);  plusBtn.setBitmaps(bN, bP);  plusBtn.setAction(plusCb); add(plusBtn);
    minusBtn.setXY(550, 350); minusBtn.setBitmaps(bN, bP); minusBtn.setAction(minusCb); add(minusBtn);
    touchgfx::Unicode::strncpy(upBuf, "Up", 8);
    touchgfx::Unicode::strncpy(downBuf, "Dn", 8);
    touchgfx::Unicode::strncpy(plusBuf, "+", 8);
    touchgfx::Unicode::strncpy(minusBuf, "-", 8);
    upLbl.setPosition(165, 352, 60, 32);    upLbl.setTypedText(TypedText(T___SINGLEUSE_T387));    upLbl.setWildcard(upBuf);    upLbl.setColor(white); add(upLbl);
    downLbl.setPosition(285, 352, 60, 32);  downLbl.setTypedText(TypedText(T___SINGLEUSE_T387));  downLbl.setWildcard(downBuf); downLbl.setColor(white); add(downLbl);
    plusLbl.setPosition(445, 352, 60, 32);  plusLbl.setTypedText(TypedText(T___SINGLEUSE_T387));  plusLbl.setWildcard(plusBuf); plusLbl.setColor(white); add(plusLbl);
    minusLbl.setPosition(565, 352, 60, 32); minusLbl.setTypedText(TypedText(T___SINGLEUSE_T387)); minusLbl.setWildcard(minusBuf); minusLbl.setColor(white); add(minusLbl);

    /* 覆盖基类 back_setting 跳转：改过参数先弹 modal 确认 */
    back_setting.setAction(backCb);

    /* —— 确认 modal（改过参数点 back → Apply/Discard/Cancel）——
     * 等效 data_screen ModalWindow：遮罩 touchable 拦截底层点击，最后 add 保证最上层。*/
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
    modalText.setColor(white);
    touchgfx::Unicode::strncpy(modalBuf, "Apply changes?", 32);
    modalOverlay.add(modalText);

    btnApply.setXY(215, 235);    btnApply.setBitmaps(bN, bP);    btnApply.setAction(applyCb);    modalOverlay.add(btnApply);
    btnDiscard.setXY(340, 235);  btnDiscard.setBitmaps(bN, bP);  btnDiscard.setAction(discardCb); modalOverlay.add(btnDiscard);
    btnCancel.setXY(465, 235);   btnCancel.setBitmaps(bN, bP);   btnCancel.setAction(cancelCb);  modalOverlay.add(btnCancel);

    /* 按钮标签（叠加在按钮上，TextArea 默认 touchable=false 点击穿透）*/
    touchgfx::Unicode::strncpy(applyBuf,   "Apply",   12);
    touchgfx::Unicode::strncpy(discardBuf, "Discard", 12);
    touchgfx::Unicode::strncpy(cancelBuf,  "Cancel",  12);
    applyLbl.setPosition(220, 237, 90, 32);   applyLbl.setTypedText(TypedText(T___SINGLEUSE_T387));   applyLbl.setWildcard(applyBuf);   applyLbl.setColor(white); modalOverlay.add(applyLbl);
    discardLbl.setPosition(343, 237, 90, 32); discardLbl.setTypedText(TypedText(T___SINGLEUSE_T387)); discardLbl.setWildcard(discardBuf); discardLbl.setColor(white); modalOverlay.add(discardLbl);
    cancelLbl.setPosition(470, 237, 90, 32);  cancelLbl.setTypedText(TypedText(T___SINGLEUSE_T387));  cancelLbl.setWildcard(cancelBuf);  cancelLbl.setColor(white); modalOverlay.add(cancelLbl);

    /* Container 必须设全屏尺寸：setVisible+invalidate 的重绘区域 = Container rect */
    modalOverlay.setPosition(0, 0, 800, 480);
    modalOverlay.setVisible(false);
    add(modalOverlay);   /* 最后 add = 最上层 */

    refreshAll();
}

void Settings_ScreenView::tearDownScreen()
{
    Settings_ScreenViewBase::tearDownScreen();
}

/* modal 结果倒计时：Applied!/Discarded! 显示 ~1.5s 后 hide + 回主屏 */
void Settings_ScreenView::handleTickEvent()
{
    Settings_ScreenViewBase::handleTickEvent();

    if (modalState == MODAL_RESULT)
    {
        if (--resultTicks <= 0)
        {
            int act = pendingAction;
            hideConfirmModal();
            if (act == PENDING_SWITCH) {
                /* 切协议（wrap）+ 刷新显示。
                 * snap 已在 applyConfig 更新（Apply）或 revertCurrentProtoToSnap 回退（Discard）*/
                protoIdx = (uint8_t)((protoIdx + switchDir + 4) % 4);
                refreshAll();
            } else {
                application().gotostart_screenScreenNoTransition();
            }
        }
    }
}

/* 更新行 0 协议 + 行 1-5（按 protoIdx 动态）—— 只刷新显示，不写 SHM_CONFIG */
void Settings_ScreenView::refreshAll()
{
    char tmp[48];
    const char* m[6] = { selRow==0?"*":" ", selRow==1?"*":" ", selRow==2?"*":" ", selRow==3?"*":" ", selRow==4?"*":" ", selRow==5?"*":" " };

    snprintf(tmp, sizeof(tmp), "%s Protocol: %s", m[0], protoNm[protoIdx]);
    touchgfx::Unicode::strncpy(protoBuf, tmp, 40); protoRow.setWildcard(protoBuf); protoRow.invalidate();

    #define ROW(R, fmt, ...) do { snprintf(tmp, sizeof(tmp), fmt, m[R], __VA_ARGS__); \
        touchgfx::Unicode::strncpy(row##R##Buf, tmp, 40); row##R.setWildcard(row##R##Buf); row##R.invalidate(); } while(0)
    #define ROWEMPTY(R) do { snprintf(tmp, sizeof(tmp), "%s", m[R]); \
        touchgfx::Unicode::strncpy(row##R##Buf, tmp, 40); row##R.setWildcard(row##R##Buf); row##R.invalidate(); } while(0)

    switch (protoIdx) {
        case 0: /* UART 5 参数 */
            ROW(1, "%s Baud: %lu",   (unsigned long)u_baud[uBaud]);
            ROW(2, "%s Data: %u",    (unsigned)u_data[uData]);
            ROW(3, "%s Stop: %s",    u_stopD[uStop]);
            ROW(4, "%s Parity: %s",  u_par[uPar]);
            ROW(5, "%s Flow: %s",    u_flow[uFlow]);
            break;
        case 1: /* SPI 5 参数：mode/data/baud/first/role */
            ROW(1, "%s Mode: %s",    s_modeD[sMode]);
            ROW(2, "%s Data: %u",    (unsigned)s_data[sData]);
            ROW(3, "%s Baud: %lu",   (unsigned long)s_baud[sBaud]);
            ROW(4, "%s First: %s",   s_first[sFirst]);
            ROW(5, "%s Role: %s",    s_roleD[sRole]);
            break;
        case 2: /* I2C: Clock + Addr位宽 + 从机地址(固定) */
            ROW(1, "%s Clock: %lu",  (unsigned long)i_clk[iClock]);
            ROW(2, "%s Addr: %s",    i_addrD[iAddr]);
            ROW(3, "%s Slave: 0x%02X (fixed)", (unsigned)SHM_CONFIG->i2c.own_address);
            ROWEMPTY(4); ROWEMPTY(5);
            break;
        case 3: /* CAN 2 参数 + row3-5 空 */
            ROW(1, "%s Baud: %lu",   (unsigned long)c_baud[cBaud]);
            ROW(2, "%s Mode: %s",    c_modeD[cMode]);
            ROWEMPTY(3); ROWEMPTY(4); ROWEMPTY(5);
            break;
    }
    #undef ROW
    #undef ROWEMPTY
}

/* 用户确认后：写 SHM_CONFIG 全协议字段 + Release HSEM 通知 CM4 重配外设 */
void Settings_ScreenView::applyConfig()
{
    SHM_CONFIG->active_proto        = protoIdx + 1;
    SHM_CONFIG->uart.baudrate       = u_baud[uBaud];
    SHM_CONFIG->uart.databits       = u_data[uData];
    SHM_CONFIG->uart.stopbits       = u_stop[uStop];
    SHM_CONFIG->uart.parity         = uPar;
    SHM_CONFIG->uart.flowcontrol    = uFlow;
    SHM_CONFIG->spi.mode            = sMode;
    SHM_CONFIG->spi.datasize        = s_data[sData];
    SHM_CONFIG->spi.baudrate        = s_baud[sBaud];
    SHM_CONFIG->spi.firstbit        = sFirst;
    SHM_CONFIG->spi.role            = sRole;
    SHM_CONFIG->i2c.clock_speed     = i_clk[iClock];
    SHM_CONFIG->i2c.addressing      = iAddr;
    SHM_CONFIG->can.baudrate        = c_baud[cBaud];
    SHM_CONFIG->can.mode            = cMode;
    __DSB();
    /* clean DCache：CM7 写 SHM_CONFIG 后强制刷到物理，CM4（无 cache）读到的就是新值。
     * 配合 setupScreen 的 invalidate，实现 CM7↔CM4 config 一致。*/
    SCB_CleanDCache_by_Addr((uint32_t*)SHM_CONFIG_ADDR, sizeof(proto_config_t) + 32);
    shm_config_notify();   /* __DSB + HAL_HSEM_Release(HSEM_ID_CONFIG) → CM4 中断读 */
    /* snap 更新为当前（已应用），避免同一次会话内重复弹窗 */
    snap.protoIdx = protoIdx;
    snap.uBaud = uBaud; snap.uData = uData; snap.uStop = uStop; snap.uPar = uPar; snap.uFlow = uFlow;
    snap.sMode = sMode; snap.sData = sData; snap.sBaud = sBaud; snap.sFirst = sFirst; snap.sRole = sRole;
    snap.iClock = iClock; snap.iAddr = iAddr;
    snap.cBaud = cBaud; snap.cMode = cMode;
}

/* 对比 snap：用户是否改过任何 idx（改过点 back 才弹确认）*/
bool Settings_ScreenView::hasChanges()
{
    /* 不含 protoIdx：切协议本身不算改动（切协议是浏览，改了参数才算）*/
    return uBaud != snap.uBaud || uData != snap.uData || uStop != snap.uStop ||
           uPar != snap.uPar || uFlow != snap.uFlow ||
           sMode != snap.sMode || sData != snap.sData || sBaud != snap.sBaud || sFirst != snap.sFirst ||
           sRole != snap.sRole ||
           iClock != snap.iClock || iAddr != snap.iAddr ||
           cBaud != snap.cBaud || cMode != snap.cMode;
}

void Settings_ScreenView::onUpClick(const touchgfx::AbstractButton&)
{
    selRow = (selRow == 0) ? 5 : (selRow - 1);
    refreshAll();
}
void Settings_ScreenView::onDownClick(const touchgfx::AbstractButton&)
{
    selRow = (selRow + 1) % 6;
    refreshAll();
}

/* +/- 按 selRow+protoIdx 改对应 idx（编码 protoIdx*16+selRow 区分）*/
void Settings_ScreenView::onPlusClick(const touchgfx::AbstractButton&)
{
    if (selRow == 0) {
        if (hasChanges()) { switchDir = +1; showConfirmModal(PENDING_SWITCH); return; }
        protoIdx = (protoIdx + 1) % 4; refreshAll(); return;
    }
    switch (protoIdx * 16 + selRow) {
        case 0*16+1: uBaud  = (uBaud  + 1) % 8; break;
        case 0*16+2: uData  = (uData  + 1) % 3; break;
        case 0*16+3: uStop  = (uStop  + 1) % 2; break;
        case 0*16+4: uPar   = (uPar   + 1) % 3; break;
        case 0*16+5: uFlow  = (uFlow  + 1) % 4; break;
        case 1*16+1: sMode  = (sMode  + 1) % 4; break;
        case 1*16+2: sData  = (sData  + 1) % 5; break;
        case 1*16+3: sBaud  = (sBaud  + 1) % 6; break;
        case 1*16+4: sFirst = (sFirst + 1) % 2; break;
        case 1*16+5: sRole  = (sRole  + 1) % 2; break;
        case 2*16+1: iClock = (iClock + 1) % 3; break;
        case 2*16+2: iAddr  = (iAddr  + 1) % 2; break;
        case 3*16+1: cBaud  = (cBaud  + 1) % 5; break;
        case 3*16+2: cMode  = (cMode  + 1) % 4; break;
    }
    refreshAll();
}

void Settings_ScreenView::onMinusClick(const touchgfx::AbstractButton&)
{
    if (selRow == 0) {
        if (hasChanges()) { switchDir = -1; showConfirmModal(PENDING_SWITCH); return; }
        protoIdx = (protoIdx == 0) ? 3 : (protoIdx - 1); refreshAll(); return;
    }
    switch (protoIdx * 16 + selRow) {
        case 0*16+1: uBaud  = (uBaud  == 0) ? 7 : (uBaud  - 1); break;
        case 0*16+2: uData  = (uData  == 0) ? 2 : (uData  - 1); break;
        case 0*16+3: uStop  = (uStop  == 0) ? 1 : (uStop  - 1); break;
        case 0*16+4: uPar   = (uPar   == 0) ? 2 : (uPar   - 1); break;
        case 0*16+5: uFlow  = (uFlow  == 0) ? 3 : (uFlow  - 1); break;
        case 1*16+1: sMode  = (sMode  == 0) ? 3 : (sMode  - 1); break;
        case 1*16+2: sData  = (sData  == 0) ? 4 : (sData  - 1); break;
        case 1*16+3: sBaud  = (sBaud  == 0) ? 5 : (sBaud  - 1); break;
        case 1*16+4: sFirst = (sFirst == 0) ? 1 : (sFirst - 1); break;
        case 1*16+5: sRole  = (sRole  == 0) ? 1 : (sRole  - 1); break;
        case 2*16+1: iClock = (iClock == 0) ? 2 : (iClock - 1); break;
        case 2*16+2: iAddr  = (iAddr  == 0) ? 1 : (iAddr  - 1); break;
        case 3*16+1: cBaud  = (cBaud  == 0) ? 4 : (cBaud  - 1); break;
        case 3*16+2: cMode  = (cMode  == 0) ? 3 : (cMode  - 1); break;
    }
    refreshAll();
}

/* back：改过参数 → 弹确认；没改过直接回主屏 */
void Settings_ScreenView::onBackClick(const touchgfx::AbstractButton&)
{
    if (hasChanges()) showConfirmModal(PENDING_BACK);
    else application().gotostart_screenScreenNoTransition();
}

/* Apply：写 SHM_CONFIG + 通知 CM4 → 显示 Applied! → 倒计时后回主屏 */
void Settings_ScreenView::onApplyClick(const touchgfx::AbstractButton&)
{
    applyConfig();
    touchgfx::Unicode::strncpy(modalBuf, "Applied!", 32);
    modalText.setWildcard(modalBuf);
    modalText.invalidate();
    btnApply.setVisible(false);   btnApply.invalidate();
    btnDiscard.setVisible(false); btnDiscard.invalidate();
    btnCancel.setVisible(false);  btnCancel.invalidate();
    applyLbl.setVisible(false);   applyLbl.invalidate();
    discardLbl.setVisible(false); discardLbl.invalidate();
    cancelLbl.setVisible(false);  cancelLbl.invalidate();
    modalState = MODAL_RESULT;
    resultTicks = 90;   /* ~1.5s（60Hz tick）*/
}

/* Discard：不写 SHM_CONFIG → 显示 Discarded! → 倒计时后回主屏 */
void Settings_ScreenView::onDiscardClick(const touchgfx::AbstractButton&)
{
    revertCurrentProtoToSnap();   /* 丢弃当前协议改动：idx 回退 snap */
    touchgfx::Unicode::strncpy(modalBuf, "Discarded!", 32);
    modalText.setWildcard(modalBuf);
    modalText.invalidate();
    btnApply.setVisible(false);   btnApply.invalidate();
    btnDiscard.setVisible(false); btnDiscard.invalidate();
    btnCancel.setVisible(false);  btnCancel.invalidate();
    applyLbl.setVisible(false);   applyLbl.invalidate();
    discardLbl.setVisible(false); discardLbl.invalidate();
    cancelLbl.setVisible(false);  cancelLbl.invalidate();
    modalState = MODAL_RESULT;
    resultTicks = 90;
}

/* Cancel：留在 Settings，仅 hide modal */
void Settings_ScreenView::onCancelClick(const touchgfx::AbstractButton&)
{
    hideConfirmModal();
}

void Settings_ScreenView::showConfirmModal(int action)
{
    pendingAction = (uint8_t)action;
    touchgfx::Unicode::strncpy(modalBuf, "Apply changes?", 32);
    modalText.setWildcard(modalBuf);
    modalText.invalidate();
    btnApply.setVisible(true);   btnApply.invalidate();
    btnDiscard.setVisible(true); btnDiscard.invalidate();
    btnCancel.setVisible(true);  btnCancel.invalidate();
    applyLbl.setVisible(true);   applyLbl.invalidate();
    discardLbl.setVisible(true); discardLbl.invalidate();
    cancelLbl.setVisible(true);  cancelLbl.invalidate();
    modalOverlay.setVisible(true);
    modalOverlay.invalidate();
    modalState = MODAL_CONFIRM;
}

void Settings_ScreenView::hideConfirmModal()
{
    modalOverlay.setVisible(false);
    modalText.invalidate();
    btnApply.invalidate();
    btnDiscard.invalidate();
    btnCancel.invalidate();
    modalPanel.invalidate();
    modalShade.invalidate();
    modalOverlay.invalidate();
    modalState = MODAL_HIDDEN;
}

/* Discard 时回退当前协议 idx 到 snap（丢弃用户改动，切协议后旧协议显示原始值）*/
void Settings_ScreenView::revertCurrentProtoToSnap()
{
    switch (protoIdx) {
        case 0: uBaud=snap.uBaud; uData=snap.uData; uStop=snap.uStop; uPar=snap.uPar; uFlow=snap.uFlow; break;
        case 1: sMode=snap.sMode; sData=snap.sData; sBaud=snap.sBaud; sFirst=snap.sFirst; sRole=snap.sRole; break;
        case 2: iClock=snap.iClock; iAddr=snap.iAddr; break;
        case 3: cBaud=snap.cBaud; cMode=snap.cMode; break;
    }
}
