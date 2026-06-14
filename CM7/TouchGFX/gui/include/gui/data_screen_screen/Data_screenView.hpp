#ifndef DATA_SCREENVIEW_HPP
#define DATA_SCREENVIEW_HPP

#include <gui_generated/data_screen_screen/Data_screenViewBase.hpp>
#include <gui/data_screen_screen/Data_screenPresenter.hpp>

class Data_screenView : public Data_screenViewBase
{
public:
    Data_screenView();
    virtual ~Data_screenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // DATA_SCREENVIEW_HPP
