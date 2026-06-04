// 4.26.1 0x3ef60ae3
// Patched: removed image_Capture001 (157MB, does not fit in internal flash)

#include <images/BitmapDatabase.hpp>
#include <touchgfx/Bitmap.hpp>

const touchgfx::Bitmap::BitmapData bitmap_database[] = {
    // No bitmaps — all color blocks, no images
};

namespace BitmapDatabase
{
const touchgfx::Bitmap::BitmapData* getInstance()
{
    return bitmap_database;
}

uint16_t getInstanceSize()
{
    return (uint16_t)(sizeof(bitmap_database) / sizeof(touchgfx::Bitmap::BitmapData));
}
} // namespace BitmapDatabase
