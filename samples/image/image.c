
#include <stdio.h>
#include <math.h>
#include <libqu.h>

static int frame;
static qu_texture textures[128];

static double magic(double f, int n)
{
    return (sin((f / n) * M_PI * 2.0) + 1.0) / 2.0;
}

static qu_image generate_image(int n)
{
    qu_image image = qu_create_image(128, 128, 4);

    if (!image.id) {
        return image;
    }

    for (int y = 0; y < 128; y++) {
        unsigned char *p = qu_get_image_pixels(image);

        for (int x = 0; x < 128; x++) {
            p[y * 4 * 128 + x * 4 + 0] = 255.0 * magic(n + x, 128);
            p[y * 4 * 128 + x * 4 + 1] = 255.0 * magic(n - x, 128);
            p[y * 4 * 128 + x * 4 + 2] = 255.0 * magic(x - n, 128);
            p[y * 4 * 128 + x * 4 + 3] = 255.0 * magic(y + n, 128);
        }
    }

    return image;
}

static int update(void)
{
    if (qu_is_key_pressed(QU_KEY_ESCAPE)) {
        return -1;
    }

    return 0;
}

static void draw_background(double f)
{
    double x = f / 128.0 * M_PI;

    int r = 128.0 * sin(x) + 127.0;
    int g = 128.0 * sin(x + (2.0 * M_PI) / 3.0) + 127.0;
    int b = 128.0 * sin(x + (4.0 * M_PI) / 3.0) + 127.0;

    qu_color opaque = QU_COLOR(r, g, b);
    qu_color trans = QU_RGBA(r, g, b, 64);

    double m = fmod(f * 0.25, 256.0);
    double s = 1.0 + 0.5 * sin(x);

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            qu_push_matrix();
            qu_translate(x * 128.f - m, y * 128.f - m);
            qu_rotate(f);
            qu_scale(s, s);
            qu_draw_rectangle(-16.f, -16.f, 32.f, 32.f, opaque, 0);
            qu_draw_rectangle(-12.f, -12.f, 24.f, 24.f, opaque, trans);
            qu_pop_matrix();
        }
    }
}

static void draw(double edt)
{
    qu_clear(QU_COLOR(35, 35, 35));

    draw_background(qu_get_time_mediump() * 128.0);
    qu_draw_texture(textures[frame], 0.f, 0.f, 512.f, 512.f);

    qu_present();

    frame = (frame + 1) % 128;
}

int main(int argc, char *argv[])
{
    qu_set_window_title("[libquack sample] image");
    qu_set_window_size(512, 512);
    qu_set_window_flags(QU_WINDOW_USE_CANVAS);
    qu_set_canvas_flags(QU_CANVAS_SMOOTH);

    qu_initialize();
    atexit(qu_terminate);

    for (int i = 0; i < 128; i++) {
        qu_image image = generate_image(i);

        if (!image.id) {
            return -1;
        }

        textures[i] = qu_create_texture_from_image(image);

        if (!textures[i].id) {
            return -1;
        }

        qu_set_texture_smooth(textures[i], true);
        qu_destroy_image(image);
    }

    return qu_execute_game_loop(10, update, draw);
}

