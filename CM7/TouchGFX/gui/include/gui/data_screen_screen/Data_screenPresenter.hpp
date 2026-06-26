#ifndef DATA_SCREENPRESENTER_HPP
#define DATA_SCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Data_screenView;

class Data_screenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    Data_screenPresenter(Data_screenView& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();

    virtual ~Data_screenPresenter() {}

private:
    Data_screenPresenter();

    Data_screenView& view;
};

#endif // DATA_SCREENPRESENTER_HPP
