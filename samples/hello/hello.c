
#include <stdio.h>
#include <libqu.h>

#define TICK_RATE               10
#define FRAME_DURATION          1.0 / TICK_RATE

#define MAX_CIRCLES             8

#define MAX_DUCKS               64

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

struct circle
{
    float x;
    float y;

    float r;
    float dr;

    qu_color outline;
    qu_color fill;
};

struct duck
{
    float x;
    float y;

    float dx;
    float dy;
};

struct camera
{
    float x;
    float y;
    float z;

    float dx;
    float dy;
    float dz;
};

static struct
{
    bool running;
    double frame_start_time;
    double frame_lag;

    struct rectangle rectangles[2];
    struct circle circles[MAX_CIRCLES];

    int current_circle;

    struct duck ducks[MAX_DUCKS];
    qu_texture duck_texture;

    qu_font font;

    bool pause;
    float time;
    float render_time;

    struct camera camera;
} app;

static void key_press_callback(qu_key key)
{
    switch (key) {
    case QU_KEY_ESCAPE:
        app.running = false;
        break;
    case QU_KEY_SPACE:
        app.pause = !app.pause;
        break;
    default:
        break;
    }
}

static void mouse_button_press_callback(qu_mouse_button button)
{
    int x = qu_get_mouse_cursor_position().x;
    int y = qu_get_mouse_cursor_position().y;

    if (button == QU_MOUSE_BUTTON_LEFT) {
        struct circle *circle = &app.circles[app.current_circle];

        circle->x = (float) x;
        circle->y = (float) y;

        circle->r = 8.f;
        circle->dr = 2.f;

        circle->outline = QU_COLOR(224, 0, 0);
        circle->fill = QU_COLOR(24, 24, 24);
    }
}

static void mouse_button_release_callback(qu_mouse_button button)
{
    int x = qu_get_mouse_cursor_position().x;
    int y = qu_get_mouse_cursor_position().y;

    if (button == QU_MOUSE_BUTTON_LEFT) {
        struct circle *circle = &app.circles[app.current_circle];

        circle->dr = -2.f;
        circle->outline = QU_COLOR(224, 224, 224);
        circle->fill = QU_COLOR(24, 24, 24);

        app.current_circle = (app.current_circle + 1) % MAX_CIRCLES;
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

static void circle_update(struct circle *circle)
{
    if (circle->r < 0.f) {
        circle->dr = 0.f;
    }

    circle->r += circle->dr;
}

static void circle_draw(struct circle *circle, float lag_offset)
{
    if (circle->r < 0.f) {
        return;
    }

    qu_push_matrix();
    qu_translate(circle->x, circle->y);
    qu_draw_circle(0.f, 0.f, circle->r + circle->dr * lag_offset, circle->outline, circle->fill);
    qu_pop_matrix();
}

static void duck_update(struct duck *duck)
{
    duck->x += duck->dx;
    duck->y += duck->dy;

    if (duck->x < -128.f) {
        duck->x = 7 * 128.f;
    }

    if (duck->y < -128.f) {
        duck->y = 7 * 128.f;
    }
}

static void duck_draw(struct duck *duck, float lag_offset)
{
    qu_push_matrix();
    qu_translate(duck->x + duck->dx * lag_offset, duck->y + duck->dy * lag_offset);
    qu_draw_texture(app.duck_texture, -32.f, -32.f, 64.f, 64.f);
    qu_pop_matrix();
}

static void update(void)
{
    if (app.pause) {
        return;
    }

    for (int i = 0; i < 2; i++) {
        rectangle_update(&app.rectangles[i]);
    }

    for (int i = 0; i < MAX_CIRCLES; i++) {
        circle_update(&app.circles[i]);
    }

    for (int i = 0; i < MAX_DUCKS; i++) {
        duck_update(&app.ducks[i]);
    }

    app.camera.x += app.camera.dx;
    app.camera.y += app.camera.dy;
    app.camera.z += app.camera.dz;

    app.camera.dx = 0.f;
    app.camera.dy = 0.f;
    app.camera.dz = 0.f;

    if (qu_is_key_pressed(QU_KEY_W)) {
        app.camera.dy -= 30.f;
    }

    if (qu_is_key_pressed(QU_KEY_S)) {
        app.camera.dy += 30.f;
    }

    if (qu_is_key_pressed(QU_KEY_A)) {
        app.camera.dx -= 30.f;
    }

    if (qu_is_key_pressed(QU_KEY_D)) {
        app.camera.dx += 30.f;
    }

    if (qu_is_key_pressed(QU_KEY_Q)) {
        app.camera.dz -= 10.f;
    }

    if (qu_is_key_pressed(QU_KEY_E)) {
        app.camera.dz += 10.f;
    }

    if (app.camera.z < 16.f) {
        app.camera.z = 16.f;
    }
    
    if (app.camera.z > 1024.f) {
        app.camera.z = 1024.f;
    }

    app.time += FRAME_DURATION;
}

static void draw(float lag_offset)
{
    if (app.pause) {
        lag_offset = 0.f;
    }

    qu_clear(QU_COLOR(24, 24, 24));
    
    qu_set_view(app.camera.x + app.camera.dx * lag_offset,
                app.camera.y + app.camera.dy * lag_offset,
                app.camera.z + app.camera.dz * lag_offset,
                app.camera.z + app.camera.dz * lag_offset,
                0.f);
    
    qu_draw_rectangle(0.f, 0.f, 512.f, 512.f, QU_COLOR(32, 32, 32), 0);

    qu_push_matrix();

    qu_rotate(-10.f);

    for (int i = 0; i < MAX_DUCKS; i++) {
        duck_draw(&app.ducks[i], lag_offset);
    }

    qu_pop_matrix();

    for (int i = 0; i < 2; i++) {
        rectangle_draw(&app.rectangles[i], lag_offset);
    }

    for (int i = 0; i < MAX_CIRCLES; i++) {
        circle_draw(&app.circles[i], lag_offset);
    }

    // qu_reset_view();

    qu_draw_text_fmt(app.font, 8.f, 8.f, QU_COLOR(255, 255, 255), "Time: %.2f", app.time);
    qu_draw_text_fmt(app.font, 8.f, 32.f, QU_COLOR(255, 255, 255), "Render time: %.2f", app.render_time);
    qu_draw_text_fmt(app.font, 8.f, 56.f, QU_COLOR(255, 255, 255), "Lag: %.8f", app.frame_lag);

    qu_present();
}

static bool loop(void)
{
    if (!app.running) {
        return false;
    }

    double current_time = qu_get_time_highp();
    double elapsed_time = current_time - app.frame_start_time;

    app.render_time += elapsed_time;
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

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            app.ducks[y * 8 + x].x = x * 128.f;
            app.ducks[y * 8 + x].y = y * 128.f;
            app.ducks[y * 8 + x].dx = -5.f;
            app.ducks[y * 8 + x].dy = -5.f;
        }
    }

    app.camera.x = 256.f;
    app.camera.y = 256.f;
    app.camera.z = 512.f;

    qu_set_window_size(512, 512);
    qu_set_window_flags(QU_WINDOW_USE_CANVAS);
    qu_set_canvas_flags(QU_CANVAS_SMOOTH);

    qu_initialize();
    atexit(qu_terminate);

    app.duck_texture = qu_load_texture("assets/duck.png");

    if (!app.duck_texture.id) {
        return -1;
    }

    app.font = qu_load_font("assets/unispace.ttf", 16.f);

    if (!app.font.id) {
        return -1;
    }

    qu_on_key_pressed(key_press_callback);
    qu_on_mouse_button_pressed(mouse_button_press_callback);
    qu_on_mouse_button_released(mouse_button_release_callback);

    while (qu_process()) {
        if (!loop()) {
            break;
        }
    }

    return 0;
}
