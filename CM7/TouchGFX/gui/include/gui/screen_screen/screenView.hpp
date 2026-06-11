#ifndef SCREENVIEW_HPP
#define SCREENVIEW_HPP

#include <gui_generated/screen_screen/screenViewBase.hpp>
#include <gui/screen_screen/screenPresenter.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/canvas/PainterRGB565.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <texts/TextKeysAndLanguages.hpp>

class screenView : public screenViewBase
{
public:
    screenView();
    virtual ~screenView() {}
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

#endif // SCREENVIEW_HPP
