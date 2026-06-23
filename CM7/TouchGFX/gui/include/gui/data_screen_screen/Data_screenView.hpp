#ifndef DATA_SCREENVIEW_HPP
#define DATA_SCREENVIEW_HPP

#include <gui_generated/data_screen_screen/Data_screenViewBase.hpp>
#include <gui/data_screen_screen/Data_screenPresenter.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <gui/data_screen_screen/WaveformWidget.hpp>
#include <touchgfx/widgets/AbstractButton.hpp>
#include <touchgfx/mixins/ClickListener.hpp>
#include <touchgfx/events/ClickEvent.hpp>

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
    /* 返回按钮 + modal 确认（记录中点 back/再点当前协议 → 保存/不保存/取消）*/
    void onBackClick(const touchgfx::AbstractButton& src);
    void onKeepClick(const touchgfx::AbstractButton& src);
    void onDiscardClick(const touchgfx::AbstractButton& src);
    void onCancelClick(const touchgfx::AbstractButton& src);
    void showConfirmModal(int pending);   /* pending: PENDING_BACK=回主屏, PENDING_NONE=只停止 */
    void hideConfirmModal();
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
    touchgfx::Callback<Data_screenView, const touchgfx::AbstractButton&> backCb;
    touchgfx::Callback<Data_screenView, const touchgfx::AbstractButton&> keepCb;
    touchgfx::Callback<Data_screenView, const touchgfx::AbstractButton&> discardCb;
    touchgfx::Callback<Data_screenView, const touchgfx::AbstractButton&> cancelCb;
    /* 确认 modal（遮罩+面板+文本+3按钮，等效官方 ModalWindow：最后 add 最上层 + 遮罩 touchable 拦截）*/
    touchgfx::Container modalOverlay;
    touchgfx::Box modalShade;
    touchgfx::Box modalPanel;
    touchgfx::TextAreaWithOneWildcard modalText;
    touchgfx::Unicode::UnicodeChar modalBuf[32];
    touchgfx::ButtonWithLabel btnKeep;
    touchgfx::ButtonWithLabel btnDiscard;
    touchgfx::ButtonWithLabel btnCancel;
    /* 按钮标签（ButtonWithLabel::setLabelText 只接受 TypedText，自定义文字用 TextArea 叠加）*/
    touchgfx::TextAreaWithOneWildcard keepText;
    touchgfx::TextAreaWithOneWildcard discardText;
    touchgfx::TextAreaWithOneWildcard cancelText;
    touchgfx::Unicode::UnicodeChar keepLblBuf[16];
    touchgfx::Unicode::UnicodeChar discardLblBuf[16];
    touchgfx::Unicode::UnicodeChar cancelLblBuf[16];
    enum { PENDING_NONE = 0, PENDING_BACK = 1, PENDING_SWITCH = 2 } modalPending;
    uint8_t modalSwitchProto;   /* PENDING_SWITCH 时目标协议号 1-4（点别协议时存）*/
    enum { MODAL_HIDDEN, MODAL_CONFIRM, MODAL_RESULT } modalState;
    int resultTicks;   /* 结果显示倒计时（Saved!/Discarded! 显示多久后 hide+执行 pending）*/

    /* 回放控制（g_playback_mode=1 时显示）：Run/Pause + Prev + Next + Stop */
    touchgfx::ButtonWithLabel pbBtn, prevBtn, nextBtn, stopBtn;
    touchgfx::TextAreaWithOneWildcard pbText, prevText, nextText, stopText;
    touchgfx::Unicode::UnicodeChar pbLblBuf[12], prevLblBuf[12], nextLblBuf[12], stopLblBuf[12];
    touchgfx::Callback<Data_screenView, const touchgfx::AbstractButton&> pbCb, prevCb, nextCb, stopCb;
    void onPauseClick(const touchgfx::AbstractButton&);   /* Run/Pause 切换（进回放默认 Pause，按钮显示 Run）*/
    void onPrevClick(const touchgfx::AbstractButton&);    /* 上一步 pos-- */
    void onNextClick(const touchgfx::AbstractButton&);    /* 下一步 pos++ */
    void onStopClick(const touchgfx::AbstractButton&);
    /* 回放进度条（可点击/拖动定位，自绘 Box+ClickListener，等效 Slider 不依赖 bitmap）*/
    touchgfx::ClickListener<touchgfx::Box> progressClickArea;
    touchgfx::Box progressFill;
    touchgfx::TextAreaWithOneWildcard progressPctText;
    touchgfx::Unicode::UnicodeChar progressPctBuf[16];
    touchgfx::Callback<Data_screenView, const touchgfx::Box&, const touchgfx::ClickEvent&> progressClickCb;
    void onProgressClick(const touchgfx::Box& src, const touchgfx::ClickEvent& evt);
    bool playbackUiShown;   /* 回放 UI 是否已显示（检测 mode 0↔1 切换 UI）*/
};

#endif // DATA_SCREENVIEW_HPP
