
//------------------------------------------------------------------------------

#include <libqu.h>

//------------------------------------------------------------------------------

#define TICK_RATE               (10)
#define FRAME_DURATION          (1.0 / TICK_RATE)
#define MAX_CIRCLES             (8)

//------------------------------------------------------------------------------

struct app
{
    double frame_start_time;
    double frame_lag;
    bool enable_crosshair;
};

struct circles
{
    int current;
    qu_vec2f position[MAX_CIRCLES];
    float radius[MAX_CIRCLES];
    float d_radius[MAX_CIRCLES];
};

//------------------------------------------------------------------------------

static struct app app;
static struct circles circles;

//------------------------------------------------------------------------------

static void mouse_button_press_callback(qu_mouse_button button)
{
    qu_vec2i position = qu_get_mouse_cursor_position();

    if (button == QU_MOUSE_BUTTON_LEFT) {
        circles.position[circles.current].x = (float) position.x;
        circles.position[circles.current].y = (float) position.y;
        circles.radius[circles.current] = 8.f;
        circles.d_radius[circles.current] = 2.f;
    } else if (button == QU_MOUSE_BUTTON_RIGHT) {
        app.enable_crosshair = !app.enable_crosshair;
    }
}

static void mouse_button_release_callback(qu_mouse_button button)
{
    if (button == QU_MOUSE_BUTTON_LEFT) {
        circles.d_radius[circles.current] = -1.f;
        circles.current = (circles.current + 1) % MAX_CIRCLES;
    }
}

//------------------------------------------------------------------------------

static void update(void)
{
    for (int i = 0; i < MAX_CIRCLES; i++) {
        if (circles.radius[i] < 0.f) {
            circles.d_radius[i] = 0.f;
        }

        circles.radius[i] += circles.d_radius[i];
    }
}

static void draw(float lag_offset)
{
    qu_clear(QU_COLOR(0, 160, 128));

    if (app.enable_crosshair) {
        qu_vec2i position = qu_get_mouse_cursor_position();

        float x = (float) position.x;
        float y = (float) position.y;

        qu_draw_line(x, 0.f, x, 512.f, QU_COLOR(0, 0, 0));
        qu_draw_line(0.f, y, 512.f, y, QU_COLOR(0, 0, 0));
    }

    for (int i = 0; i < MAX_CIRCLES; i++) {
        if (circles.radius[i] < 0.f) {
            continue;
        }

        float x = circles.position[i].x;
        float y = circles.position[i].y;
        float radius = circles.radius[i] + circles.d_radius[i] * lag_offset;

        qu_draw_circle(x, y, radius, 0, QU_COLOR(255, 255, 255));
    }

    qu_present();
}

static bool loop(void)
{
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

//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    qu_initialize(&(qu_params) {
        .title = "[libquack sample] mouse",
        .display_width = 512,
        .display_height = 512,
        .enable_canvas = true,
        .canvas_smooth = true,
    });

    qu_on_mouse_button_pressed(mouse_button_press_callback);
    qu_on_mouse_button_released(mouse_button_release_callback);

    qu_execute(loop);
}
