
//------------------------------------------------------------------------------

#include <libqu.h>

//------------------------------------------------------------------------------

#define MAX_CIRCLES             (8)

//------------------------------------------------------------------------------

struct app
{
    bool enable_crosshair;
    float background_brightness;
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

static void mouse_wheel_scroll_callback(int x_delta, int y_delta)
{
    app.background_brightness -= y_delta * 0.025f;

    if (app.background_brightness < 0.f) {
        app.background_brightness = 0.f;
    }

    if (app.background_brightness > 1.f) {
        app.background_brightness = 1.f;
    }
}

//------------------------------------------------------------------------------

static int update(void)
{
    for (int i = 0; i < MAX_CIRCLES; i++) {
        if (circles.radius[i] < 0.f) {
            circles.d_radius[i] = 0.f;
        }

        circles.radius[i] += circles.d_radius[i];
    }

    return 0;
}

static void draw(double lag_offset)
{
    int red = 0;
    int green = 160 * app.background_brightness;
    int blue = 128 * app.background_brightness;

    qu_clear(QU_COLOR(red, green, blue));

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

//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    qu_set_window_title("[libquack sample] mouse");
    qu_set_window_size(512, 512);
    qu_set_window_flags(QU_WINDOW_USE_CANVAS);

    qu_initialize();

    qu_on_mouse_button_pressed(mouse_button_press_callback);
    qu_on_mouse_button_released(mouse_button_release_callback);
    qu_on_mouse_wheel_scrolled(mouse_wheel_scroll_callback);

    return qu_execute_game_loop(10, update, draw);
}
