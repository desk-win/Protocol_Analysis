#ifndef SETTINGS_SCREENVIEW_HPP
#define SETTINGS_SCREENVIEW_HPP

#include <gui_generated/settings_screen_screen/Settings_ScreenViewBase.hpp>
#include <gui/settings_screen_screen/Settings_ScreenPresenter.hpp>
#include <touchgfx/widgets/ButtonWithLabel.hpp>
#include <touchgfx/widgets/AbstractButton.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <touchgfx/containers/Container.hpp>

class Settings_ScreenView : public Settings_ScreenViewBase
{
public:
    Settings_ScreenView();
    virtual ~Settings_ScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();   /* modal 结果倒计时（Applied!/Discarded! 显示后回主屏）*/
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
    void refreshAll();   /* 只更新 UI 显示，不写 SHM_CONFIG */
    void applyConfig();  /* 用户确认后：写 SHM_CONFIG 全字段 + shm_config_notify() 通知 CM4 */
    bool hasChanges();   /* 对比 snap，本地 idx 有没有被用户改过 */
    void loadConfigFromShm();  /* 从 SHM_CONFIG 读全部 idx + 更新 snap（setupScreen 和 config-loaded 自动刷新共用）*/
    uint8_t selRow;       /* 0=协议, 1-5=参数 */
    uint8_t protoIdx;     /* 0=UART, 1=SPI, 2=I2C, 3=CAN */
    /* UART 参数 idx */
    uint8_t uBaud, uData, uStop, uPar, uFlow;
    /* SPI 参数 idx */
    uint8_t sMode, sData, sBaud, sFirst, sRole;   /* sRole: 0=Slave, 1=Master */
    /* I2C 参数 idx */
    uint8_t iClock, iAddr;
    /* CAN 参数 idx */
    uint8_t cBaud, cMode;

    /* —— back 拦截：用户改过参数 → 弹 modal 确认 Apply/Discard/Cancel —— */
    touchgfx::Callback<Settings_ScreenView, const touchgfx::AbstractButton&> backCb, applyCb, discardCb, cancelCb;
    void onBackClick(const touchgfx::AbstractButton&);
    void onApplyClick(const touchgfx::AbstractButton&);
    void onDiscardClick(const touchgfx::AbstractButton&);
    void onCancelClick(const touchgfx::AbstractButton&);
    void showConfirmModal(int action);   /* action: PENDING_BACK/PENDING_SWITCH */
    void hideConfirmModal();
    void revertCurrentProtoToSnap();     /* Discard 时回退当前协议 idx 到 snap（丢弃改动）*/
    /* snap：进屏幕时所有 idx 的副本，hasChanges 对比用（refreshAll 不写 SHM_CONFIG，cancel 自然回退）*/
    struct { uint8_t protoIdx, uBaud, uData, uStop, uPar, uFlow, sMode, sData, sBaud, sFirst, sRole, iClock, iAddr, cBaud, cMode; } snap;
    /* 确认 modal（等效 data_screen：遮罩 + 面板 + 文本 + 3 按钮 + TextArea 标签）*/
    touchgfx::Container modalOverlay;
    touchgfx::Box modalShade;
    touchgfx::Box modalPanel;
    touchgfx::TextAreaWithOneWildcard modalText;
    touchgfx::Unicode::UnicodeChar modalBuf[32];
    touchgfx::ButtonWithLabel btnApply, btnDiscard, btnCancel;
    touchgfx::TextAreaWithOneWildcard applyLbl, discardLbl, cancelLbl;
    touchgfx::Unicode::UnicodeChar applyBuf[12], discardBuf[12], cancelBuf[12];
    enum { MODAL_HIDDEN, MODAL_CONFIRM, MODAL_RESULT } modalState;
    enum { PENDING_BACK = 0, PENDING_SWITCH = 1 };   /* pendingAction 取值常量 */
    uint8_t pendingAction;   /* 弹窗触发：PENDING_BACK/PENDING_SWITCH */
    int8_t switchDir;   /* PENDING_SWITCH 时切协议方向 +1/−1 */
    uint16_t resultTicks;
};

#endif // SETTINGS_SCREENVIEW_HPP
