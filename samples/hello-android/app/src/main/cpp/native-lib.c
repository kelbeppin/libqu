
//------------------------------------------------------------------------------
// This should be rewritten.
//------------------------------------------------------------------------------

#include <math.h>
#include <libqu.h>

//------------------------------------------------------------------------------

struct resources
{
    qu_sound fanfare;
    qu_music ostrich;
    qu_texture duck;
    qu_font font;
};

//------------------------------------------------------------------------------

static struct resources resources;
static int touch_flag;

//------------------------------------------------------------------------------

static void draw_touch_point(int index, int x, int y)
{
    qu_draw_line(x, 0.f, x, 512.f, QU_COLOR(224, 224, 224));
    qu_draw_line(0.f, y, 512.f, y, QU_COLOR(224, 224, 224));

    qu_push_matrix();
    qu_translate(x, y);
    qu_draw_circle(0.f, 0.f, 32.f, QU_COLOR(255, 255, 255), 0);
    qu_draw_text_fmt(resources.font, -64.f, -64.f, QU_COLOR(128, 128, 128),
                     "%d (%d, %d)", index, x, y);
    qu_pop_matrix();
}

void android_main(void)
{
    qu_initialize(&(qu_params) {
        .display_width = 512,
        .display_height = 512,
        .enable_canvas = true,
        .canvas_smooth = true,
    });

    resources.font = qu_load_font("sansation.ttf", 24.f);

    if (resources.font.id == 0) {
        return;
    }

    resources.duck = qu_load_texture("duck.png");

    if (resources.duck.id == 0) {
        return;
    }

    resources.fanfare = qu_load_sound("fanfare.wav");
    resources.ostrich = qu_open_music("ostrich.ogg");

    int counter = 0;

    float local_time = 0.f;
    float prev_time = qu_get_time_mediump();

    while (qu_process()) {
        float current_time = qu_get_time_mediump();
        float elapsed_time = current_time - prev_time;

        if (!qu_is_window_active()) {
            continue;
        }

        local_time += elapsed_time;

        if (qu_is_touch_pressed(0)) {
            qu_vec2i p = qu_get_touch_position(0);

            if (!touch_flag) {
                if (p.y < 256) {
                    qu_play_sound(resources.fanfare);
                } else {
                    qu_play_music(resources.ostrich);
                }
                touch_flag = 1;
            }
        } else {
            if (touch_flag) {
                touch_flag = 0;
            }
        }

        qu_clear(QU_COLOR(16, 16, 16));

        int x = 127 + (int) (128.f * (0.5f + (sinf(local_time * 4.f) * 0.5f)));
        int y = 127 + (int) (128.f * (0.5f + (cosf(local_time * 4.f) * 0.5f)));
        int z = 255 - (int) (128.f * (0.5f + (sinf(local_time * 4.f) * 0.5f)));

        qu_set_blend_mode(QU_BLEND_MODE_ADD);
        qu_draw_circle(256.f, 192.f, 96.f, 0, QU_RGBA(x, 0, 0, 128));
        qu_draw_circle(192.f, 320.f, 96.f, 0, QU_RGBA(0, y, 0, 128));
        qu_draw_circle(320.f, 320.f, 96.f, 0, QU_RGBA(0, 0, z, 128));

        qu_set_blend_mode(QU_BLEND_MODE_ALPHA);
        qu_draw_text_fmt(resources.font, 32.f, 32.f, QU_COLOR(255, 255, 255),
                         "%.2f", local_time);
        qu_draw_text_fmt(resources.font, 192.f, 32.f, QU_COLOR(255, 255, 255),
                         "%.2f", current_time);
        qu_draw_text_fmt(resources.font, 384.f, 32.f, QU_COLOR(255, 255, 255),
                         "%.2f", current_time - local_time);
        qu_draw_text_fmt(resources.font, 32.f, 64.f, QU_COLOR(255, 255, 255),
                         "frame: %d", counter++);

        for (int i = 0; i < QU_MAX_TOUCH_INPUTS; i++) {
            if (qu_is_touch_pressed(i)) {
                qu_vec2i position = qu_get_touch_position(i);
                draw_touch_point(i, position.x, position.y);
            }
        }

        qu_present();

        prev_time = current_time;
    }

    qu_terminate();
}
