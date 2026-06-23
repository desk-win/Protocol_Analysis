#include <gui/data_screen_screen/WaveformWidget.hpp>
#include <string.h>

WaveformWidget::WaveformWidget()
    : Widget(), count_(0), bitWidth_(3), highY_(20), lowY_(70), color_(0x07E0), bgColor_(0x0000), byteGap_(0)  /* 绿波形，黑背景 */
{
    memset(bytes_, 0, sizeof(bytes_));
}

void WaveformWidget::setBytes(const uint8_t *bytes, uint16_t count)
{
    if (count > MAX_BYTES) count = MAX_BYTES;
    memcpy(bytes_, bytes, count);
    count_ = count;
}

void WaveformWidget::setParams(uint16_t bitWidth, int16_t highY, int16_t lowY, touchgfx::colortype color, touchgfx::colortype bgColor, uint16_t byteGap)
{
    bitWidth_ = bitWidth;
    highY_ = highY;
    lowY_ = lowY;
    color_ = color;
    bgColor_ = bgColor;
    byteGap_ = byteGap;
}

void WaveformWidget::draw(const touchgfx::Rect &invalidatedArea) const
{
    touchgfx::Rect absRect = getAbsoluteRect();
    const int16_t lineH = 2;
    /* 只清方波实际占用的窄带（highY 到 lowY），不清整个 widget——整个区域每帧
     * fillRect 黑色会让 LTDC 刷新时看到大面积黑闪。只擦方波带即可去掉残留叠加。*/
    int16_t bandY = absRect.y + highY_ - lineH;
    int16_t bandH = lowY_ - highY_ + 3 * lineH;
    touchgfx::HAL::lcd().fillRect(touchgfx::Rect(absRect.x, bandY, absRect.width, bandH), bgColor_);

    /* 参考线：高/低电平位置画淡灰水平线。
     * 注意：必须用 Color::getColorFromRGB，不能硬编码 0xXXXX——TouchGFX framebuffer
     * 有 byte swap，硬编码的 RGB565 值会错位（0xF800 红会显示成绿）。*/
    const touchgfx::colortype refColor = touchgfx::Color::getColorFromRGB(80, 80, 80);
    touchgfx::HAL::lcd().fillRect(touchgfx::Rect(absRect.x, absRect.y + highY_, absRect.width, 1), refColor);
    touchgfx::HAL::lcd().fillRect(touchgfx::Rect(absRect.x, absRect.y + lowY_, absRect.width, 1), refColor);

    if (count_ == 0) return;

    int16_t x = absRect.x;
    int16_t prevY = absRect.y + lowY_;   /* 空闲态 = 低电平 */

    for (uint16_t b = 0; b < count_; b++)
    {
        uint8_t byte = bytes_[b];
        /* 10 bit：起始(0) + D0..D7(LSB first) + 停止(1) */
        int bits[10];
        bits[0] = 0;
        for (int i = 0; i < 8; i++) bits[1 + i] = (byte >> i) & 1;
        bits[9] = 1;

        for (int i = 0; i < 10; i++)
        {
            /* 起始位红、停止位蓝、数据位绿——用 Color API 避免 byte swap 错位 */
            touchgfx::colortype bitColor;
            if (i == 0) bitColor = touchgfx::Color::getColorFromRGB(255, 0, 0);        /* 起始位 红 */
            else if (i == 9) bitColor = touchgfx::Color::getColorFromRGB(0, 0, 255);   /* 停止位 蓝 */
            else bitColor = color_;                                                     /* 数据位 绿 */

            int16_t y = bits[i] ? (absRect.y + highY_) : (absRect.y + lowY_);
            /* 电平跳变：画垂直线连接 */
            if (y != prevY)
            {
                int16_t top = (y < prevY) ? y : prevY;
                int16_t h = (y > prevY) ? (y - prevY) : (prevY - y);
                touchgfx::HAL::lcd().fillRect(touchgfx::Rect(x, top, lineH, h + lineH), bitColor);
            }
            /* 当前 bit 的水平电平线 */
            touchgfx::HAL::lcd().fillRect(touchgfx::Rect(x, y, bitWidth_, lineH), bitColor);
            prevY = y;
            x += bitWidth_;
        }
        x += byteGap_;                      /* 字节间隙，分隔相邻字节让用户看清边界 */
        prevY = absRect.y + lowY_;          /* 间隙后重置，避免跨字节画连线 */
    }
}

touchgfx::Rect WaveformWidget::getSolidRect() const
{
    /* 只方波窄带是 solid，其余透明（底层 box1 金色透出）。
     * 全 solid 时 modal 残留字符落在波形上半区，draw 只清窄带，solid 阻止 box1 重绘 → 残留。*/
    const int16_t lineH = 2;
    return touchgfx::Rect(0, highY_ - lineH, getWidth(), lowY_ - highY_ + 3 * lineH);
}
