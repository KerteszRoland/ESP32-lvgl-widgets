#include <lvgl.h>
#include "linkedin_widget.h"

void render_linkedin_widget(lv_obj_t *parent)
{
    LV_IMAGE_DECLARE(qrcode);
    lv_obj_t *img1 = lv_image_create(parent);
    lv_image_set_src(img1, &qrcode);
    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);
}