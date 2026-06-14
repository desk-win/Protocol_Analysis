#ifndef SD_TEST_SCREENVIEW_HPP
#define SD_TEST_SCREENVIEW_HPP

#include <gui_generated/sd_test_screen_screen/SD_Test_ScreenViewBase.hpp>
#include <gui/sd_test_screen_screen/SD_Test_ScreenPresenter.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/canvas/PainterRGB565.hpp>
#include <touchgfx/widgets/Box.hpp>
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

    touchgfx::TextAreaWithOneWildcard capacityText;
    bool capacityTextAdded;
    touchgfx::Unicode::UnicodeChar capacityBuf[30];
};

#endif // SD_TEST_SCREENVIEW_HPP
