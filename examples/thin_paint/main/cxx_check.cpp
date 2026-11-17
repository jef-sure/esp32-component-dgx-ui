/* every public header of dgx-ui read by C++, as a program of Arduino reads them: those of FreeRTOS and esp_timer too */
#include "dgx_ui.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_post.h"
#include "dgx_ui_text.h"
#include "dgx_ui_ticker.h"

struct cxx_record {
    int16_t a, b;
};
DGX_UI_MESSAGE(cxx_record, 99, cxx_record)
