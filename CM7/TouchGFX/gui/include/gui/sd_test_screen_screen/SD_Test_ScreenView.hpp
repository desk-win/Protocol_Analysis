#ifndef SD_TEST_SCREENVIEW_HPP
#define SD_TEST_SCREENVIEW_HPP

#include <gui_generated/sd_test_screen_screen/SD_Test_ScreenViewBase.hpp>
#include <gui/sd_test_screen_screen/SD_Test_ScreenPresenter.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/canvas/PainterRGB565.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <touchgfx/widgets/ButtonWithLabel.hpp>
#include <touchgfx/widgets/AbstractButton.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <texts/TextKeysAndLanguages.hpp>

class SD_Test_ScreenView : public SD_Test_ScreenViewBase
{
public:
    SD_Test_ScreenView();
    virtual ~SD_Test_ScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    virtual void handleTickEvent();

protected:
    touchgfx::Circle   touchDot;
    touchgfx::PainterRGB565 touchDotPainter;
    bool touchDotAdded;
    bool sdTestDone;
    int  sdTestResult;
    int  fatfsResult;
    int  tickCount;

    touchgfx::Box sdStatusBar;
    bool sdStatusBarAdded;

    /* Buffer for Designer-generated textArea1 (its own textArea1Buffer is only
     * 10 chars — too small for "Total:X MB  Free:X MB  Used:X MB"). */
    touchgfx::Unicode::UnicodeChar capacityBuf[50];

    /* SD 根目录文件列表显示 */
    touchgfx::TextAreaWithOneWildcard fileText;
    touchgfx::Unicode::UnicodeChar fileBuf[200];

    /* 文件 selector 按钮 + 标签 TextArea（ButtonWithLabel setLabelText 只接受 TypedText，
     * 自定义文字 ^/v/Play/Delete 用 TextArea 叠加在按钮上）*/
    touchgfx::ButtonWithLabel upBtn, downBtn, playBtn, deleteBtn;
    touchgfx::TextAreaWithOneWildcard upText, downText, playText, deleteText;
    touchgfx::Unicode::UnicodeChar upLblBuf[8], downLblBuf[8], playLblBuf[8], deleteLblBuf[8];
    touchgfx::Callback<SD_Test_ScreenView, const touchgfx::AbstractButton&> upCb, downCb, playCb, deleteCb;
    void onUpClick(const touchgfx::AbstractButton&);
    void onDownClick(const touchgfx::AbstractButton&);
    void onPlayClick(const touchgfx::AbstractButton&);
    void onDeleteClick(const touchgfx::AbstractButton&);
    void refreshFileList();   /* 按 g_file_list/g_file_sel 更新 fileText（当前选中行）*/
    uint8_t lastFileCount, lastFileSel;   /* 检测 defaultTask 扫描结果变化触发刷新 */
    bool delConfirmPending;   /* Delete 双击确认：第一次点设 pending 提示，第二次执行 */
    int delConfirmTicks;      /* pending 超时倒计时（避免误删）*/
};

#endif // SD_TEST_SCREENVIEW_HPP
