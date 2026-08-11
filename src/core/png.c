#include "core/png.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define AF_PNG_MAX_DIMENSION 16384U
#define AF_DEFLATE_STORED_BLOCK_SIZE 65535U

static uint32_t af_png_crc_update(uint32_t crc, const uint8_t *data, size_t size) {
    for (size_t index = 0U; index < size; ++index) {
        crc ^= data[index];
        for (uint32_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ (UINT32_C(0xedb88320) & (uint32_t)-(int32_t)(crc & 1U));
        }
    }
    return crc;
}

static bool af_png_write(FILE *file, const void *data, size_t size) {
    return size == 0U || fwrite(data, 1U, size, file) == size;
}

static bool af_png_write_u32(FILE *file, uint32_t value) {
    const uint8_t bytes[4] = {(uint8_t)(value >> 24U), (uint8_t)(value >> 16U),
                              (uint8_t)(value >> 8U), (uint8_t)value};
    return af_png_write(file, bytes, sizeof(bytes));
}

static bool af_png_write_chunk(FILE *file, const char type[4], const uint8_t *data, uint32_t size) {
    uint32_t crc = UINT32_MAX;
    crc = af_png_crc_update(crc, (const uint8_t *)type, 4U);
    crc = af_png_crc_update(crc, data, size) ^ UINT32_MAX;
    return af_png_write_u32(file, size) && af_png_write(file, type, 4U) &&
           af_png_write(file, data, size) && af_png_write_u32(file, crc);
}

static uint8_t af_png_raw_byte(const uint8_t *pixels, uint32_t width, uint32_t row_stride,
                               AfPixelOrder order, uint64_t offset) {
    const uint64_t row_size = (uint64_t)width * 4U + 1U;
    const uint64_t row = offset / row_size;
    const uint64_t column = offset % row_size;
    const uint8_t *pixel = NULL;
    uint32_t component = 0U;
    if (column == 0U) {
        return 0U;
    }
    component = (uint32_t)((column - 1U) & 3U);
    pixel = pixels + row * row_stride + (column - 1U) / 4U * 4U;
    if (order == AF_PIXEL_ORDER_BGRA && component < 3U) {
        component = 2U - component;
    }
    return pixel[component];
}

AfResult af_png_write_rgba8(const char *path, const void *pixels, uint32_t width, uint32_t height,
                            uint32_t row_stride, AfPixelOrder order) {
    static const uint8_t signature[8] = {137U, 80U, 78U, 71U, 13U, 10U, 26U, 10U};
    uint8_t ihdr[13] = {0};
    uint8_t block[AF_DEFLATE_STORED_BLOCK_SIZE];
    const uint64_t row_size = (uint64_t)width * 4U + 1U;
    const uint64_t raw_size = row_size * height;
    const uint64_t block_count =
        (raw_size + AF_DEFLATE_STORED_BLOCK_SIZE - 1U) / AF_DEFLATE_STORED_BLOCK_SIZE;
    const uint64_t idat_size64 = 2U + raw_size + block_count * 5U + 4U;
    uint64_t raw_offset = 0U;
    uint32_t idat_crc = UINT32_MAX;
    uint32_t adler_a = 1U;
    uint32_t adler_b = 0U;
    FILE *file = NULL;
    bool success = true;
    if (path == NULL || pixels == NULL || width == 0U || height == 0U ||
        width > AF_PNG_MAX_DIMENSION || height > AF_PNG_MAX_DIMENSION || row_stride < width * 4U ||
        order > AF_PIXEL_ORDER_BGRA || raw_size > UINT32_MAX || idat_size64 > UINT32_MAX) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        return AF_RESULT_IO_ERROR;
    }
    ihdr[0] = (uint8_t)(width >> 24U);
    ihdr[1] = (uint8_t)(width >> 16U);
    ihdr[2] = (uint8_t)(width >> 8U);
    ihdr[3] = (uint8_t)width;
    ihdr[4] = (uint8_t)(height >> 24U);
    ihdr[5] = (uint8_t)(height >> 16U);
    ihdr[6] = (uint8_t)(height >> 8U);
    ihdr[7] = (uint8_t)height;
    ihdr[8] = 8U;
    ihdr[9] = 6U;
    success = af_png_write(file, signature, sizeof(signature)) &&
              af_png_write_chunk(file, "IHDR", ihdr, sizeof(ihdr)) &&
              af_png_write_u32(file, (uint32_t)idat_size64) && af_png_write(file, "IDAT", 4U);
    if (!success) {
        goto close_file;
    }
    idat_crc = af_png_crc_update(idat_crc, (const uint8_t *)"IDAT", 4U);
    {
        const uint8_t zlib_header[2] = {0x78U, 0x01U};
        if (!af_png_write(file, zlib_header, sizeof(zlib_header))) {
            success = false;
            goto close_file;
        }
        idat_crc = af_png_crc_update(idat_crc, zlib_header, sizeof(zlib_header));
    }
    while (raw_offset < raw_size) {
        const uint32_t size = (uint32_t)((raw_size - raw_offset) > AF_DEFLATE_STORED_BLOCK_SIZE
                                             ? AF_DEFLATE_STORED_BLOCK_SIZE
                                             : (raw_size - raw_offset));
        const uint16_t short_size = (uint16_t)size;
        const uint16_t inverse_size = (uint16_t)~short_size;
        const uint8_t header[5] = {(raw_offset + size == raw_size) ? 1U : 0U, (uint8_t)short_size,
                                   (uint8_t)(short_size >> 8U), (uint8_t)inverse_size,
                                   (uint8_t)(inverse_size >> 8U)};
        for (uint32_t index = 0U; index < size; ++index) {
            const uint8_t value =
                af_png_raw_byte(pixels, width, row_stride, order, raw_offset + index);
            block[index] = value;
            adler_a = (adler_a + value) % 65521U;
            adler_b = (adler_b + adler_a) % 65521U;
        }
        if (!af_png_write(file, header, sizeof(header)) || !af_png_write(file, block, size)) {
            success = false;
            goto close_file;
        }
        idat_crc = af_png_crc_update(idat_crc, header, sizeof(header));
        idat_crc = af_png_crc_update(idat_crc, block, size);
        raw_offset += size;
    }
    {
        const uint32_t adler = (adler_b << 16U) | adler_a;
        const uint8_t bytes[4] = {(uint8_t)(adler >> 24U), (uint8_t)(adler >> 16U),
                                  (uint8_t)(adler >> 8U), (uint8_t)adler};
        idat_crc = af_png_crc_update(idat_crc, bytes, sizeof(bytes)) ^ UINT32_MAX;
        success = af_png_write(file, bytes, sizeof(bytes)) && af_png_write_u32(file, idat_crc) &&
                  af_png_write_chunk(file, "IEND", NULL, 0U);
    }

close_file:
    if (fclose(file) != 0) {
        success = false;
    }
    return success ? AF_RESULT_SUCCESS : AF_RESULT_IO_ERROR;
}
