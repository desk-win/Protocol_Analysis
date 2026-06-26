/*----------------------------------------------*/
/* TJpgDec System Configurations R0.03          */
/*----------------------------------------------*/
#include "./BSP/LCD/ltdc.h"


#define	JD_SZBUF        1024
/* Specifies size of stream input buffer */

#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888 || LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
#define JD_FORMAT       0
#else
#define JD_FORMAT       1
#endif
/* Specifies output pixel format.
/  0: RGB888 (24-bit/pix)
/  1: RGB565 (16-bit/pix)
/  2: Grayscale (8-bit/pix)
*/

#define	JD_USE_SCALE    1
/* Switches output descaling feature.
/  0: Disable
/  1: Enable
*/

#define JD_TBLCLIP      1
/* Use table conversion for saturation arithmetic. A bit faster, but increases 1 KB of code size.
/  0: Disable
/  1: Enable
*/

#define JD_FASTDECODE   2
/* Optimization level
/  0: Basic optimization. Suitable for 8/16-bit MCUs.
/  1: + 32-bit barrel shifter. Suitable for 32-bit MCUs.
/  2: + Table conversion for huffman decoding (wants 6 << HUFF_BIT bytes of RAM)
*/

/******************************************************************************************/

#define JPEG_USE_MALLOC     1               /* 定义是否使用malloc,这里我们选择使用malloc */
#define JPEG_WBUF_SIZE      6144 + 4096     /* 定义工作区数组大小,最少应不小于3092字节, 如果JD_FASTDECODE==2, 则需要增加 6144字节内存 */

/******************************************************************************************/










