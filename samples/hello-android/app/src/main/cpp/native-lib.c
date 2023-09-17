
#include <math.h>
#include <libqu.h>

void android_main(void)
{
    qu_initialize(&(qu_params) {
        .display_width = 512,
        .display_height = 512,
        .enable_canvas = true,
        .canvas_smooth = true,
    });

    qu_font font = qu_load_font("sansation.ttf", 24.f);

    if (font.id == 0) {
        return;
    }

    qu_texture texture = qu_load_texture("duck.png");

    if (texture.id == 0) {
        return;
    }

    int counter = 0;

    float local_time = 0.f;
    float prev_time = qu_get_time_mediump();

    while (qu_process()) {
        float current_time = qu_get_time_mediump();
        float elapsed_time = current_time - prev_time;

        local_time += elapsed_time;

        qu_clear(QU_COLOR(16, 16, 16));

        int x = 127 + (int) (128.f * (0.5f + (sinf(local_time * 4.f) * 0.5f)));
        int y = 127 + (int) (128.f * (0.5f + (cosf(local_time * 4.f) * 0.5f)));
        int z = 255 - (int) (128.f * (0.5f + (sinf(local_time * 4.f) * 0.5f)));

        qu_set_blend_mode(QU_BLEND_MODE_ADD);
        qu_draw_circle(256.f, 192.f, 96.f, 0, QU_RGBA(x, 0, 0, 128));
        qu_draw_circle(192.f, 320.f, 96.f, 0, QU_RGBA(0, y, 0, 128));
        qu_draw_circle(320.f, 320.f, 96.f, 0, QU_RGBA(0, 0, z, 128));

        qu_set_blend_mode(QU_BLEND_MODE_ALPHA);
        qu_draw_text_fmt(font, 32.f, 32.f, QU_COLOR(255, 255, 255),
                         "%.2f", local_time);
        qu_draw_text_fmt(font, 192.f, 32.f, QU_COLOR(255, 255, 255),
                         "%.2f", current_time);
        qu_draw_text_fmt(font, 384.f, 32.f, QU_COLOR(255, 255, 255),
                         "%.2f", current_time - local_time);
        qu_draw_text_fmt(font, 32.f, 64.f, QU_COLOR(255, 255, 255),
                         "frame: %d", counter++);

        qu_present();

        prev_time = current_time;
    }

    qu_terminate();
}
