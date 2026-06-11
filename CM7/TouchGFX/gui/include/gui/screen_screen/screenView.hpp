#ifndef SCREENVIEW_HPP
#define SCREENVIEW_HPP

#include <gui_generated/screen_screen/screenViewBase.hpp>
#include <gui/screen_screen/screenPresenter.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/canvas/PainterRGB565.hpp>
#include <touchgfx/widgets/Box.hpp>

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
    int  sdTestResult;    /* 0=ok, 2=CMD0, 3=CMD8, 10+=FatFs error */
    int  fatfsResult;     /* 0=ok, 1=mount fail, 2=opendir fail */
    int  tickCount;

    touchgfx::Box sdStatusBar;
    bool sdStatusBarAdded;
};

#endif // SCREENVIEW_HPP
