#include <gui/settings_screen_screen/Settings_ScreenView.hpp>
#include <touchgfx/Color.hpp>
#include <images/BitmapDatabase.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include <stdio.h>
#include <shared_config.h>

/* ===== 档位表（UI idx → 实际值/显示，refreshAll 映射写 SHM_CONFIG）===== */
/* UART */
static const uint32_t u_baud[8]  = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
static const uint8_t  u_data[5]  = {5, 6, 7, 8, 9};
static const uint8_t  u_stop[3]  = {1, 2, 3};                 /* 3 表示 1.5 */
static const char*    u_stopD[3] = {"1", "2", "1.5"};
static const char*    u_par[3]   = {"None", "Even", "Odd"};
static const char*    u_flow[4]  = {"None", "RTS", "CTS", "RTS/CTS"};
/* SPI */
static const char*    s_modeD[4] = {"0", "1", "2", "3"};
static const uint8_t  s_data[7]  = {4, 5, 6, 7, 8, 12, 16};
static const uint32_t s_baud[6]  = {187500, 375000, 750000, 1500000, 3000000, 6000000};
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
      selRow(0), protoIdx(0),
      uBaud(4), uData(3), uStop(0), uPar(0), uFlow(0),
      sMode(0), sData(4), sBaud(3), sFirst(0),
      iClock(0), iAddr(0),
      cBaud(3), cMode(0)
{
}

void Settings_ScreenView::setupScreen()
{
    Settings_ScreenViewBase::setupScreen();

    /* 背景改深灰（box1 默认金色，白字对比不够）*/
    box1.setColor(touchgfx::Color::getColorFromRGB(40, 40, 40));
    box1.invalidate();

    selRow = 0;
    protoIdx = (SHM_CONFIG->active_proto >= 1 && SHM_CONFIG->active_proto <= 4) ? (SHM_CONFIG->active_proto - 1) : 0;
    /* UART idx 从 SHM_CONFIG 映射（其他协议用默认 idx，首次乱值不映射）*/
    uint32_t b = SHM_CONFIG->uart.baudrate;
    uBaud = 4; for (uint8_t i = 0; i < 8; i++) if (u_baud[i] == b) { uBaud = i; break; }
    uint8_t d = SHM_CONFIG->uart.databits;
    uData = 3; for (uint8_t i = 0; i < 5; i++) if (u_data[i] == d) { uData = i; break; }
    uint8_t s = SHM_CONFIG->uart.stopbits;
    uStop = 0; for (uint8_t i = 0; i < 3; i++) if (u_stop[i] == s) { uStop = i; break; }
    uPar  = (SHM_CONFIG->uart.parity < 3)      ? SHM_CONFIG->uart.parity      : 0;
    uFlow = (SHM_CONFIG->uart.flowcontrol < 4) ? SHM_CONFIG->uart.flowcontrol : 0;

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

    refreshAll();
}

void Settings_ScreenView::tearDownScreen()
{
    Settings_ScreenViewBase::tearDownScreen();
}

/* 更新行 0 协议 + 行 1-5（按 protoIdx 动态）+ 写 SHM_CONFIG 全协议字段 */
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
        case 1: /* SPI 4 参数 + row5 空 */
            ROW(1, "%s Mode: %s",    s_modeD[sMode]);
            ROW(2, "%s Data: %u",    (unsigned)s_data[sData]);
            ROW(3, "%s Baud: %lu",   (unsigned long)s_baud[sBaud]);
            ROW(4, "%s First: %s",   s_first[sFirst]);
            ROWEMPTY(5);
            break;
        case 2: /* I2C 2 参数 + row3-5 空（own_address 固定不调）*/
            ROW(1, "%s Clock: %lu",  (unsigned long)i_clk[iClock]);
            ROW(2, "%s Addr: %s",    i_addrD[iAddr]);
            ROWEMPTY(3); ROWEMPTY(4); ROWEMPTY(5);
            break;
        case 3: /* CAN 2 参数 + row3-5 空 */
            ROW(1, "%s Baud: %lu",   (unsigned long)c_baud[cBaud]);
            ROW(2, "%s Mode: %s",    c_modeD[cMode]);
            ROWEMPTY(3); ROWEMPTY(4); ROWEMPTY(5);
            break;
    }
    #undef ROW
    #undef ROWEMPTY

    /* 写 SHM_CONFIG（全协议字段，CM4 按 active_proto 用对应协议）*/
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
    SHM_CONFIG->i2c.clock_speed     = i_clk[iClock];
    SHM_CONFIG->i2c.addressing      = iAddr;
    SHM_CONFIG->can.baudrate        = c_baud[cBaud];
    SHM_CONFIG->can.mode            = cMode;
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
    if (selRow == 0) { protoIdx = (protoIdx + 1) % 4; refreshAll(); return; }
    switch (protoIdx * 16 + selRow) {
        case 0*16+1: uBaud  = (uBaud  + 1) % 8; break;
        case 0*16+2: uData  = (uData  + 1) % 5; break;
        case 0*16+3: uStop  = (uStop  + 1) % 3; break;
        case 0*16+4: uPar   = (uPar   + 1) % 3; break;
        case 0*16+5: uFlow  = (uFlow  + 1) % 4; break;
        case 1*16+1: sMode  = (sMode  + 1) % 4; break;
        case 1*16+2: sData  = (sData  + 1) % 7; break;
        case 1*16+3: sBaud  = (sBaud  + 1) % 6; break;
        case 1*16+4: sFirst = (sFirst + 1) % 2; break;
        case 2*16+1: iClock = (iClock + 1) % 3; break;
        case 2*16+2: iAddr  = (iAddr  + 1) % 2; break;
        case 3*16+1: cBaud  = (cBaud  + 1) % 5; break;
        case 3*16+2: cMode  = (cMode  + 1) % 4; break;
    }
    refreshAll();
}

void Settings_ScreenView::onMinusClick(const touchgfx::AbstractButton&)
{
    if (selRow == 0) { protoIdx = (protoIdx == 0) ? 3 : (protoIdx - 1); refreshAll(); return; }
    switch (protoIdx * 16 + selRow) {
        case 0*16+1: uBaud  = (uBaud  == 0) ? 7 : (uBaud  - 1); break;
        case 0*16+2: uData  = (uData  == 0) ? 4 : (uData  - 1); break;
        case 0*16+3: uStop  = (uStop  == 0) ? 2 : (uStop  - 1); break;
        case 0*16+4: uPar   = (uPar   == 0) ? 2 : (uPar   - 1); break;
        case 0*16+5: uFlow  = (uFlow  == 0) ? 3 : (uFlow  - 1); break;
        case 1*16+1: sMode  = (sMode  == 0) ? 3 : (sMode  - 1); break;
        case 1*16+2: sData  = (sData  == 0) ? 6 : (sData  - 1); break;
        case 1*16+3: sBaud  = (sBaud  == 0) ? 5 : (sBaud  - 1); break;
        case 1*16+4: sFirst = (sFirst == 0) ? 1 : (sFirst - 1); break;
        case 2*16+1: iClock = (iClock == 0) ? 2 : (iClock - 1); break;
        case 2*16+2: iAddr  = (iAddr  == 0) ? 1 : (iAddr  - 1); break;
        case 3*16+1: cBaud  = (cBaud  == 0) ? 4 : (cBaud  - 1); break;
        case 3*16+2: cMode  = (cMode  == 0) ? 3 : (cMode  - 1); break;
    }
    refreshAll();
}
