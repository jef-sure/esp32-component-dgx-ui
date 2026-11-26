/*
 * dgx_ui_picture_file.c
 *
 *  The pictures of a file of DGX into a table of the widgets' pictures, where they lie (dgx_ui_picture_file.h).
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <string.h>

#include "dgx_image_file.h"
#include "dgx_ui_picture_file.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

bool dgx_ui_picture_file_read(const dgx_ui_picture_file_t *f)
{
    memset(f->pictures, 0, (size_t)f->count * sizeof *f->pictures);
    dgx_image_file_t file;
    if (dgx_image_file_open(f->bytes, *f->length, &file) != ESP_OK) {
        ESP_LOGE(TAG, "picture file %s: it does not read; its widgets draw paper", f->name);
        return false;
    }
    const uint8_t kind = f->bits == 16 ? DGX_IMAGE_FILE_PIXELS : DGX_IMAGE_FILE_INDEXED;
    if (file.count != f->count || file.bits != f->bits || file.kind != kind) {
        ESP_LOGE(TAG, "picture file %s: %u pictures of %u bits, made C of %u of %u bits; build it again", f->name, (unsigned)file.count, (unsigned)file.bits,
                 (unsigned)f->count, (unsigned)f->bits);
        return false;
    }
    for (unsigned k = 0; k < f->count; ++k) {
        dgx_ui_picture_t *p = &f->pictures[k];
        if (kind == DGX_IMAGE_FILE_INDEXED) {
            if (dgx_image_file_image(&file, k, &f->images[k]) != ESP_OK) return memset(f->pictures, 0, (size_t)f->count * sizeof *p), false;
            *p = (dgx_ui_picture_t){.image = &f->images[k], .map = f->bits > 1 ? f->map : NULL, .scale = f->scale, .solid = f->solid, .zero = f->zero};
        } else {
            dgx_image_file_picture_t pixels;
            if (dgx_image_file_picture(&file, k, &pixels) != ESP_OK || pixels.alpha) return memset(f->pictures, 0, (size_t)f->count * sizeof *p), false;
            *p = (dgx_ui_picture_t){.rgb565 = (const uint16_t *)(const void *)pixels.pixels, .width = (int16_t)pixels.width, .height = (int16_t)pixels.height,
                                    .scale = f->scale};
        }
    }
    return true;
}
