#ifndef WAVEFORM_WIDGET_HPP
#define WAVEFORM_WIDGET_HPP

#include <touchgfx/widgets/Widget.hpp>
#include <touchgfx/hal/HAL.hpp>
#include <touchgfx/Color.hpp>

/* 时序波形 widget：逻辑分析仪风格。
 * 把 UART 字节展开成 bit 画方波：起始位(0) + 8 数据位(LSB first) + 停止位(1) = 10 bit。
 * 用法：setParams 设参数，setBytes 设要显示的字节，invalidate 触发重绘。*/
class WaveformWidget : public touchgfx::Widget
{
public:
    WaveformWidget();
    virtual ~WaveformWidget() {}

    /* 设置要显示的最新字节（拷贝到内部缓冲）*/
    void setBytes(const uint8_t *bytes, uint16_t count);

    /* 配置：每 bit 像素宽、高电平 Y、低电平 Y（widget 内坐标）、波形颜色、背景色、字节间隙 */
    void setParams(uint16_t bitWidth, int16_t highY, int16_t lowY, touchgfx::colortype color, touchgfx::colortype bgColor, uint16_t byteGap);

    virtual void draw(const touchgfx::Rect &invalidatedArea) const;
    virtual touchgfx::Rect getSolidRect() const;

private:
    static const uint16_t MAX_BYTES = 40;
    uint8_t bytes_[MAX_BYTES];
    uint16_t count_;
    uint16_t bitWidth_;
    int16_t highY_;     /* 高电平 Y（widget 局部坐标）*/
    int16_t lowY_;      /* 低电平 Y */
    touchgfx::colortype color_;
    touchgfx::colortype bgColor_;   /* 背景色（每帧清屏，避免旧波形叠加）*/
    uint16_t byteGap_;              /* 字节间间隙（像素），分隔相邻字节让用户看清边界 */
};

#endif /* WAVEFORM_WIDGET_HPP */
