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

void WaveformWidget::setParams(uint16_t bitWidth, int16_t highY, int16_t lowY, touchgfx::colortype color, touchgfx::colortype bgColor, uint16_t byteGap,
                               uint8_t databits, uint8_t parity, uint8_t stopbits, bool framed)
{
    bitWidth_ = bitWidth;
    highY_ = highY;
    lowY_ = lowY;
    color_ = color;
    bgColor_ = bgColor;
    byteGap_ = byteGap;
    databits_ = (databits >= 4 && databits <= 9) ? databits : 8;   /* clamp，超范围用默认 8（SPI datasize 可 4）*/
    parity_   = (parity <= 2) ? parity : 0;
    stopbits_ = (stopbits >= 1 && stopbits <= 3) ? stopbits : 1;
    framed_   = framed;
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

    /* 动态 bit 数：framed_ 时 start(1) + databits + parity(0/1) + stopbits（UART 风格）；
     * !framed_ 时只画 databits 个数据位（SPI/I2C/CAN 无 framing）。
     * bit 颜色：start 红 / data 绿(配置) / parity 黄 / stop 蓝 */
    const int stop_real  = (stopbits_ >= 2) ? 2 : 1;   /* 1→1, 2→2, 1.5(=3)→2 近似 */
    const int dataBits   = databits_;
    const int parityBits = (framed_ && (parity_ != 0)) ? 1 : 0;
    const int nbits      = framed_ ? (1 + dataBits + parityBits + stop_real) : dataBits;
    const touchgfx::colortype startColor  = touchgfx::Color::getColorFromRGB(255, 0, 0);
    const touchgfx::colortype parityColor = touchgfx::Color::getColorFromRGB(255, 255, 0);
    const touchgfx::colortype stopColor   = touchgfx::Color::getColorFromRGB(0, 0, 255);

    for (uint16_t b = 0; b < count_; b++)
    {
        uint8_t byte = bytes_[b];
        int bits[16];
        int btype[16];   /* 0=start 1=data 2=parity 3=stop */
        int idx = 0, par = 0;
        if (framed_) { bits[0] = 0; btype[0] = 0; idx = 1; }
        for (int i = 0; i < dataBits; i++) { int d = (byte >> i) & 1; bits[idx] = d; btype[idx] = 1; par ^= d; idx++; }
        if (parityBits) { if (parity_ == 2) par ^= 1; bits[idx] = par; btype[idx] = 2; idx++; }
        if (framed_) { for (int i = 0; i < stop_real; i++) { bits[idx] = 1; btype[idx] = 3; idx++; } }

        for (int i = 0; i < nbits; i++)
        {
            touchgfx::colortype bitColor;
            switch (btype[i]) {
                case 0:  bitColor = startColor;  break;
                case 1:  bitColor = color_;      break;   /* data 配置色 */
                case 2:  bitColor = parityColor; break;
                default: bitColor = stopColor;   break;
            }
            int16_t y = bits[i] ? (absRect.y + highY_) : (absRect.y + lowY_);
            if (y != prevY)
            {
                int16_t top = (y < prevY) ? y : prevY;
                int16_t h = (y > prevY) ? (y - prevY) : (prevY - y);
                touchgfx::HAL::lcd().fillRect(touchgfx::Rect(x, top, lineH, h + lineH), bitColor);
            }
            touchgfx::HAL::lcd().fillRect(touchgfx::Rect(x, y, bitWidth_, lineH), bitColor);
            prevY = y;
            x += bitWidth_;
        }
        x += byteGap_;
        prevY = absRect.y + lowY_;
    }
}

touchgfx::Rect WaveformWidget::getSolidRect() const
{
    /* 只方波窄带是 solid，其余透明（底层 box1 金色透出）。
     * 全 solid 时 modal 残留字符落在波形上半区，draw 只清窄带，solid 阻止 box1 重绘 → 残留。*/
    const int16_t lineH = 2;
    return touchgfx::Rect(0, highY_ - lineH, getWidth(), lowY_ - highY_ + 3 * lineH);
}
