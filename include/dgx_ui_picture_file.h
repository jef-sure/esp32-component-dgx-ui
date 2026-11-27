#pragma once
/*
 * dgx_ui_picture_file.h
 *
 *  The pictures of a file of DGX (.dgxi, dgx_image_file.h) as a table of the widgets' pictures, read where they lie:
 *  the C dgx_ui_c makes of a names file's line `picture <name> @<asset>` fills its table by it at its start. A program
 *  that names no picture of a file carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdbool.h>
#include <stdint.h>

#include "dgx_image.h"
#include "dgx_ui.h"

#ifdef __cplusplus
// @formatter:off
extern "C" {
// @formatter:on
#endif

/** A file of pictures and the table it fills, as the made C gives them; constants of flash, the tables of RAM. */
typedef struct dgx_ui_picture_file_t {
    const uint8_t    *bytes;    ///< the file, dgx_asset_<name> of DGX's dgx_assets.h
    const uint32_t   *length;   ///< its length, &dgx_asset_<name>_size
    const char       *name;     ///< the asset's name, for the log
    dgx_ui_picture_t *pictures; ///< the table filled, count of them
    dgx_image_t      *images;   ///< 1, 4 and 8 bits, and 16 with transparency: a picture of DGX each, count of them; else NULL
    const uint8_t    *map;      ///< 4 and 8 bits of an indexed file: map[p - 1] the table's index of the value p; else NULL
    uint16_t          count;    ///< the pictures the file holds, as the made C read it
    uint8_t           bits;     ///< 1, 4 or 8 of an indexed file, 4 or 8 of a mask, 16 of pixels
    uint8_t           scale;    ///< dgx_ui_picture_t::scale of each
    uint8_t           solid;    ///< 4 and 8 bits of a full palette: the value 0 a colour, `zero`
    uint8_t           zero;     ///< its index of the table
    uint8_t           kind;     ///< DGX_IMAGE_FILE_INDEXED, DGX_IMAGE_FILE_PIXELS or DGX_IMAGE_FILE_MASK of dgx_image_file.h
} dgx_ui_picture_file_t;

/**
 * @brief The pictures of a file into its table: a dgx_image_t each for 1, 4 and 8 bits and for a mask (`mask` set), the
 * rows of RGB565 for 16, or a dgx_image_t of 16 bits with its plane of opacity for one that has transparent places; the
 * map, the scale and `solid` of the made C. False, and the table left zeros (its widgets draw paper), when the file
 * does not read or is not what the C was made of: its count, its depth, its kind, transparency where the made C has no
 * room for it; told in the log.
 */
bool dgx_ui_picture_file_read(const dgx_ui_picture_file_t *file);

#ifdef __cplusplus
// @formatter:off
}
// @formatter:on
#endif
