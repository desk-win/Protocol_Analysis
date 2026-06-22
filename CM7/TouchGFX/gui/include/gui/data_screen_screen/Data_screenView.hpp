#ifndef DATA_SCREENVIEW_HPP
#define DATA_SCREENVIEW_HPP

#include <gui_generated/data_screen_screen/Data_screenViewBase.hpp>
#include <gui/data_screen_screen/Data_screenPresenter.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <gui/data_screen_screen/WaveformWidget.hpp>
#include <touchgfx/widgets/AbstractButton.hpp>

class Data_screenView : public Data_screenViewBase
{
public:
    Data_screenView();
    virtual ~Data_screenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    /* 协议选择 toggle + 4 协议记录回调（Button setAction 用 AbstractButton 签名）*/
    void onMenuClick(const touchgfx::AbstractButton& src);
    void onUartClick(const touchgfx::AbstractButton& src);
    void onSpiClick(const touchgfx::AbstractButton& src);
    void onI2cClick(const touchgfx::AbstractButton& src);
    void onCanClick(const touchgfx::AbstractButton& src);
protected:
    touchgfx::TextAreaWithOneWildcard shmCountText;   /* 显示 CM4→CM7 共享内存收到字节数 */
    touchgfx::Unicode::UnicodeChar shmCountBuf[48];
    WaveformWidget waveWidget;   /* 时序波形（UART 字节解码 bit 画方波）*/
    /* 按钮 click Callback（Button::setAction 要 lvalue 引用，必须声明为成员，不能传临时对象）*/
    touchgfx::Callback<Data_screenView, const touchgfx::AbstractButton&> menuClickCb;
    touchgfx::Callback<Data_screenView, const touchgfx::AbstractButton&> uartClickCb;
    touchgfx::Callback<Data_screenView, const touchgfx::AbstractButton&> spiClickCb;
    touchgfx::Callback<Data_screenView, const touchgfx::AbstractButton&> i2cClickCb;
    touchgfx::Callback<Data_screenView, const touchgfx::AbstractButton&> canClickCb;
};

#endif // DATA_SCREENVIEW_HPP
