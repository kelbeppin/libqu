
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libqu.h>

//------------------------------------------------------------------------------

#define IMAGE_WIDTH         (128)
#define IMAGE_HEIGHT        (128)
#define IMAGE_CHANNELS      (2)
#define EMPTY_PIXEL         (0xff00)
#define XY(x, y) \
    ((y) * IMAGE_WIDTH * IMAGE_CHANNELS + (x) * IMAGE_CHANNELS)

//------------------------------------------------------------------------------
// State

static struct
{
    qu_image image;
    qu_texture texture;
    bool drawing;
    int old_x;
    int old_y;
    bool cleaning;
    int cy;
} state;

//------------------------------------------------------------------------------
// Bresenham's line algorithm

static void plot_line_low(int x0, int y0, int x1, int y1)
{
    unsigned char *pixels = qu_get_image_pixels(state.image);

    int dx = x1 - x0;
    int dy = y1 - y0;
    int yi = 1;

    if (dy < 0) {
        yi = -1;
        dy = -dy;
    }

    int D = (2 * dy) - dx;
    int y = y0;

    for (int x = x0; x <= x1; x++) {
        pixels[XY(x, y) + 1] = 0;

        if (D > 0) {
            y = y + yi;
            D = D + (2 * (dy - dx));
        } else {
            D = D + (2 * dy);
        }
    }
}

static void plot_line_high(int x0, int y0, int x1, int y1)
{
    unsigned char *pixels = qu_get_image_pixels(state.image);

    int dx = x1 - x0;
    int dy = y1 - y0;
    int xi = 1;

    if (dx < 0) {
        xi = -1;
        dx = -dx;
    }

    int D = (2 * dx) - dy;
    int x = x0;

    for (int y = y0; y <= y1; y++) {
        pixels[XY(x, y) + 1] = 0;

        if (D > 0) {
            x = x + xi;
            D = D + (2 * (dx - dy));
        } else {
            D = D + (2 * dx);
        }
    }
}

static void plot_line(int x0, int y0, int x1, int y1)
{
    if (abs(y1 - y0) < abs(x1 - x0)) {
        if (x0 > x1) {
            plot_line_low(x1, y1, x0, y0);
        } else {
            plot_line_low(x0, y0, x1, y1);
        }
    } else {
        if (y0 > y1) {
            plot_line_high(x1, y1, x0, y0);
        } else {
            plot_line_high(x0, y0, x1, y1);
        }
    }
}

//------------------------------------------------------------------------------

static void on_mouse_button_pressed(qu_mouse_button button)
{
    if (button == QU_MOUSE_BUTTON_LEFT) {
        if (!state.drawing) {
            state.drawing = true;
            state.old_x = -1;
            state.old_y = -1;
        }
    }
}

static void on_mouse_button_released(qu_mouse_button button)
{
    if (button == QU_MOUSE_BUTTON_LEFT) {
        if (state.drawing) {
            state.drawing = false;
        }
    }
}

static int update(void)
{
    if (state.cleaning) {
        unsigned char *pixels = qu_get_image_pixels(state.image);

        for (int y = state.cy; y < (state.cy + 4); y++) {
            for (int x = 0; x < IMAGE_WIDTH; x++) {
                pixels[XY(x, y) + 0] = 0;
                pixels[XY(x, y) + 1] = 255;
            }
        }

        qu_update_texture(state.texture, state.image);

        state.cy += 4;

        if (state.cy >= IMAGE_HEIGHT) {
            state.cleaning = false;
        }
    } else if (state.drawing) {
        qu_vec2i pos = qu_get_mouse_cursor_position();

        int x = (int) (pos.x / 512.f * IMAGE_WIDTH);
        int y = (int) (pos.y / 512.f * IMAGE_HEIGHT);

        if (state.old_x == -1 && state.old_y == -1) {
            state.old_x = x;
            state.old_y = y;
        }

        plot_line(state.old_x, state.old_y, x, y);
        qu_update_texture(state.texture, state.image);

        state.old_x = x;
        state.old_y = y;
    } else {
        if (qu_is_key_pressed(QU_KEY_BACKSPACE)) {
            if (!state.cleaning) {
                state.cleaning = true;
                state.cy = 0;
            }
        }
    }

    return 0;
}

static void draw(double edt)
{
    double x = qu_get_time_mediump() * 6.0;

    int r = 128.0 * sin(x) + 127.0;
    int g = 128.0 * sin(x + (2.0 * M_PI) / 3.0) + 127.0;
    int b = 128.0 * sin(x + (4.0 * M_PI) / 3.0) + 127.0;

    qu_clear(QU_COLOR(r, g, b));
    qu_draw_texture(state.texture, 0.f, 0.f, 512.f, 512.f);
    qu_present();
}

int main(int argc, char *argv[])
{
    qu_set_window_size(512, 512);
    qu_set_window_flags(QU_WINDOW_USE_CANVAS);

    qu_initialize();
    atexit(qu_terminate);

    state.image = qu_create_image(IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);

    if (!state.image.id) {
        fprintf(stderr, "Failed to create an image.\n");
        return -1;
    }

    state.texture = qu_create_texture(IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);

    if (!state.texture.id) {
        fprintf(stderr, "Failed to create a texture.\n");
        return -1;
    }

    qu_fill_image(state.image, EMPTY_PIXEL);
    qu_update_texture(state.texture, state.image);

    qu_on_mouse_button_pressed(on_mouse_button_pressed);
    qu_on_mouse_button_released(on_mouse_button_released);

    return qu_execute_game_loop(30, update, draw);
}

