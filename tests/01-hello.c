
#include <stdio.h>
#include <libqu.h>

#define TICK_RATE               10
#define FRAME_DURATION          1.0 / TICK_RATE

struct rectangle
{
    float w;
    float h;
    
    float x;
    float y;
    float r;

    float dx;
    float dy;
    float dr;

    qu_color outline;
    qu_color fill;
};

static struct
{
    bool running;
    double frame_start_time;
    double frame_lag;

    struct rectangle rectangles[2];
} app;

static void key_press_callback(qu_key key)
{
    switch (key) {
    case QU_KEY_ESCAPE:
        app.running = false;
        break;
    default:
        break;
    }
}

static void rectangle_update(struct rectangle *rectangle)
{
    rectangle->x += rectangle->dx;
    rectangle->y += rectangle->dy;
    rectangle->r += rectangle->dr;
}

static void rectangle_draw(struct rectangle *rectangle, float lag_offset)
{
    float x = rectangle->x + rectangle->dx * lag_offset;
    float y = rectangle->y + rectangle->dy * lag_offset;
    float r = rectangle->r + rectangle->dr * lag_offset;

    float a0 = -rectangle->w / 2.f;
    float b0 = -rectangle->h / 2.f;
    float a1 = rectangle->w;
    float b1 = rectangle->h;

    qu_push_matrix();
    qu_translate(x, y);
    qu_rotate(r);
    qu_draw_rectangle(a0, b0, a1, b1, rectangle->outline, rectangle->fill);
    qu_pop_matrix();
}

static void update(void)
{
    for (int i = 0; i < 2; i++) {
        rectangle_update(&app.rectangles[i]);
    }
}

static void draw(float lag_offset)
{
    qu_clear(QU_COLOR(24, 24, 24));
    
    for (int i = 0; i < 2; i++) {
        rectangle_draw(&app.rectangles[i], lag_offset);
    }

    qu_present();
}

static bool loop(void)
{
    if (!app.running) {
        return false;
    }

    double current_time = qu_get_time_highp();
    double elapsed_time = current_time - app.frame_start_time;

    app.frame_start_time = current_time;
    app.frame_lag += elapsed_time;

    while (app.frame_lag >= FRAME_DURATION) {
        update();
        app.frame_lag -= FRAME_DURATION;
    }

    draw((float) app.frame_lag * TICK_RATE);

    return true;
}

int main(int argc, char *argv[])
{
    app.running = true;
    app.frame_start_time = 0.0;
    app.frame_lag = 0.0;

    app.rectangles[0].w = 128.f;
    app.rectangles[0].h = 128.f;
    app.rectangles[0].x = 256.f;
    app.rectangles[0].y = 256.f;
    app.rectangles[0].r = 45.f;
    app.rectangles[0].dx = 0.f;
    app.rectangles[0].dy = 0.f;
    app.rectangles[0].dr = 4.f;
    app.rectangles[0].outline = QU_COLOR(224, 224, 224);
    app.rectangles[0].fill = QU_COLOR(32, 32, 32);

    app.rectangles[1].w = 80.f;
    app.rectangles[1].h = 80.f;
    app.rectangles[1].x = 256.f;
    app.rectangles[1].y = 256.f;
    app.rectangles[1].r = 45.f;
    app.rectangles[1].dx = 0.f;
    app.rectangles[1].dy = 0.f;
    app.rectangles[1].dr = -4.f;
    app.rectangles[1].outline = QU_COLOR(224, 0, 0);
    app.rectangles[1].fill = QU_COLOR(24, 24, 24);

    qu_initialize(&(qu_params) {
        .display_width = 512,
        .display_height = 512,
        .enable_canvas = true,
    });

    qu_on_key_pressed(key_press_callback);

    qu_execute(loop);

    return 0;
}
