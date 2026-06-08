#include <gui/screen_screen/screenView.hpp>
#include <touchgfx/Color.hpp>
#include <touch.h>

screenView::screenView()
    : touchDotAdded(false)
{
}

void screenView::setupScreen()
{
    screenViewBase::setupScreen();

    touchDot.setPosition(0, 0, 32, 32);
    touchDot.setCenter(16, 16);
    touchDot.setRadius(15);
    touchDot.setLineWidth(0);
    touchDot.setArc(0, 360);
    touchDotPainter.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    touchDot.setPainter(touchDotPainter);
    touchDot.setVisible(false);
    touchDot.setTouchable(false);
}

void screenView::tearDownScreen()
{
    screenViewBase::tearDownScreen();
}

void screenView::handleTickEvent()
{
    screenViewBase::handleTickEvent();

    /*
     * Touch indicator & status bar
     * ────────────────────────────
     * tp_dev is already updated by STM32TouchController::sampleTouch()
     * which runs every tick before handleTickEvent().  We only READ
     * tp_dev here — no additional I2C traffic.
     *
     * Key: invalidate() BEFORE setPosition() to erase the old circle,
     * then invalidate() AFTER to draw at the new position.
     */
    if (tp_dev.sta & TP_PRES_DOWN)
    {
        uint16_t tx = tp_dev.x[0];
        uint16_t ty = tp_dev.y[0];

        if (tx < 800 && ty < 480)
        {
            if (!touchDotAdded)
            {
                add(touchDot);
                touchDotAdded = true;
            }
            touchDot.invalidate();                               /* erase old position */
            touchDot.setPosition(tx - 16, ty - 16, 32, 32);
            touchDot.setVisible(true);
            touchDot.invalidate();                               /* draw new position */
        }
    }
    else
    {
        if (touchDotAdded && touchDot.isVisible())
        {
            touchDot.setVisible(false);
            touchDot.invalidate();
        }
    }
}
