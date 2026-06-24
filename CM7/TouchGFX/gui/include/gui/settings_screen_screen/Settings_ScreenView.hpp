#ifndef SETTINGS_SCREENVIEW_HPP
#define SETTINGS_SCREENVIEW_HPP

#include <gui_generated/settings_screen_screen/Settings_ScreenViewBase.hpp>
#include <gui/settings_screen_screen/Settings_ScreenPresenter.hpp>
#include <touchgfx/widgets/ButtonWithLabel.hpp>
#include <touchgfx/widgets/AbstractButton.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>

class Settings_ScreenView : public Settings_ScreenViewBase
{
public:
    Settings_ScreenView();
    virtual ~Settings_ScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
    /* 行 0：协议选择（UART/SPI/I2C/CAN）；行 1-5：当前协议参数（按 protoIdx 动态显示）*/
    touchgfx::TextAreaWithOneWildcard protoRow, row1, row2, row3, row4, row5;
    touchgfx::Unicode::UnicodeChar protoBuf[40], row1Buf[40], row2Buf[40], row3Buf[40], row4Buf[40], row5Buf[40];
    /* 4 控制按钮：Up/Down 选行，+/- 改选中行档位 */
    touchgfx::ButtonWithLabel upBtn, downBtn, plusBtn, minusBtn;
    touchgfx::TextAreaWithOneWildcard upLbl, downLbl, plusLbl, minusLbl;
    touchgfx::Unicode::UnicodeChar upBuf[8], downBuf[8], plusBuf[8], minusBuf[8];
    touchgfx::Callback<Settings_ScreenView, const touchgfx::AbstractButton&> upCb, downCb, plusCb, minusCb;
    void onUpClick(const touchgfx::AbstractButton&);
    void onDownClick(const touchgfx::AbstractButton&);
    void onPlusClick(const touchgfx::AbstractButton&);
    void onMinusClick(const touchgfx::AbstractButton&);
    void refreshAll();
    uint8_t selRow;       /* 0=协议, 1-5=参数 */
    uint8_t protoIdx;     /* 0=UART, 1=SPI, 2=I2C, 3=CAN */
    /* UART 参数 idx */
    uint8_t uBaud, uData, uStop, uPar, uFlow;
    /* SPI 参数 idx */
    uint8_t sMode, sData, sBaud, sFirst;
    /* I2C 参数 idx */
    uint8_t iClock, iAddr;
    /* CAN 参数 idx */
    uint8_t cBaud, cMode;
};

#endif // SETTINGS_SCREENVIEW_HPP
