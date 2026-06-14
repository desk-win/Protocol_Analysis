#ifndef SETTINGS_SCREENVIEW_HPP
#define SETTINGS_SCREENVIEW_HPP

#include <gui_generated/settings_screen_screen/Settings_ScreenViewBase.hpp>
#include <gui/settings_screen_screen/Settings_ScreenPresenter.hpp>

class Settings_ScreenView : public Settings_ScreenViewBase
{
public:
    Settings_ScreenView();
    virtual ~Settings_ScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // SETTINGS_SCREENVIEW_HPP
