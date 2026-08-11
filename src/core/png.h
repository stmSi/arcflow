#ifndef AF_PNG_H
#define AF_PNG_H

#include "arcflow/arcflow_result.h"

#include <stdint.h>

typedef enum AfPixelOrder { AF_PIXEL_ORDER_RGBA = 0, AF_PIXEL_ORDER_BGRA = 1 } AfPixelOrder;

AfResult af_png_write_rgba8(const char *path, const void *pixels, uint32_t width, uint32_t height,
                            uint32_t row_stride, AfPixelOrder order);

#endif
