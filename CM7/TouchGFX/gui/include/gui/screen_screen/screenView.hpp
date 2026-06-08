#ifndef SCREENVIEW_HPP
#define SCREENVIEW_HPP

#include <gui_generated/screen_screen/screenViewBase.hpp>
#include <gui/screen_screen/screenPresenter.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/canvas/PainterRGB565.hpp>

class screenView : public screenViewBase
{
public:
    screenView();
    virtual ~screenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    /* Direct-poll touch every tick (bypasses framework for debug) */
    virtual void handleTickEvent();

protected:
    touchgfx::Circle   touchDot;
    touchgfx::PainterRGB565 touchDotPainter;
    bool touchDotAdded;
};

#endif // SCREENVIEW_HPP
