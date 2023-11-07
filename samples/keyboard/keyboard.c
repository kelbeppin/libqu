
//------------------------------------------------------------------------------

#include <libqu.h>

//------------------------------------------------------------------------------

#define TICK_RATE               (10)
#define FRAME_DURATION          (1.0 / TICK_RATE)

#define PRESS_EVENT_FLAG        (1 << 0)
#define REPEAT_EVENT_FLAG       (1 << 1)
#define RELEASE_EVENT_FLAG      (1 << 2)

//------------------------------------------------------------------------------

static char const *key_names[] = {
    "KEY_0",
    "KEY_1",
    "KEY_2",
    "KEY_3",
    "KEY_4",
    "KEY_5",
    "KEY_6",
    "KEY_7",
    "KEY_8",
    "KEY_9",
    "KEY_A",
    "KEY_B",
    "KEY_C",
    "KEY_D",
    "KEY_E",
    "KEY_F",
    "KEY_G",
    "KEY_H",
    "KEY_I",
    "KEY_J",
    "KEY_K",
    "KEY_L",
    "KEY_M",
    "KEY_N",
    "KEY_O",
    "KEY_P",
    "KEY_Q",
    "KEY_R",
    "KEY_S",
    "KEY_T",
    "KEY_U",
    "KEY_V",
    "KEY_W",
    "KEY_X",
    "KEY_Y",
    "KEY_Z",
    "KEY_GRAVE",
    "KEY_APOSTROPHE",
    "KEY_MINUS",
    "KEY_EQUAL",
    "KEY_LBRACKET",
    "KEY_RBRACKET",
    "KEY_COMMA",
    "KEY_PERIOD",
    "KEY_SEMICOLON",
    "KEY_SLASH",
    "KEY_BACKSLASH",
    "KEY_SPACE",
    "KEY_ESCAPE",
    "KEY_BACKSPACE",
    "KEY_TAB",
    "KEY_ENTER",
    "KEY_F1",
    "KEY_F2",
    "KEY_F3",
    "KEY_F4",
    "KEY_F5",
    "KEY_F6",
    "KEY_F7",
    "KEY_F8",
    "KEY_F9",
    "KEY_F10",
    "KEY_F11",
    "KEY_F12",
    "KEY_UP",
    "KEY_DOWN",
    "KEY_LEFT",
    "KEY_RIGHT",
    "KEY_LSHIFT",
    "KEY_RSHIFT",
    "KEY_LCTRL",
    "KEY_RCTRL",
    "KEY_LALT",
    "KEY_RALT",
    "KEY_LSUPER",
    "KEY_RSUPER",
    "KEY_MENU",
    "KEY_PGUP",
    "KEY_PGDN",
    "KEY_HOME",
    "KEY_END",
    "KEY_INSERT",
    "KEY_DELETE",
    "KEY_PRINTSCREEN",
    "KEY_PAUSE",
    "KEY_CAPSLOCK",
    "KEY_SCROLLLOCK",
    "KEY_NUMLOCK",
    "KEY_KP_0",
    "KEY_KP_1",
    "KEY_KP_2",
    "KEY_KP_3",
    "KEY_KP_4",
    "KEY_KP_5",
    "KEY_KP_6",
    "KEY_KP_7",
    "KEY_KP_8",
    "KEY_KP_9",
    "KEY_KP_MUL",
    "KEY_KP_ADD",
    "KEY_KP_SUB",
    "KEY_KP_POINT",
    "KEY_KP_DIV",
    "KEY_KP_ENTER",
    "(none)",
};

//------------------------------------------------------------------------------

struct app
{
    double frame_start_time;
    double frame_lag;

    qu_font font12;
    qu_font font16;
    qu_font font18;

    int event_flags;

    qu_key last_pressed_key;
    qu_key last_repeated_key;
    qu_key last_released_key;

    qu_keyboard_state keyboard_state;
};

//------------------------------------------------------------------------------

static struct app app;

//------------------------------------------------------------------------------

static void key_press_callback(qu_key key)
{
    app.event_flags |= PRESS_EVENT_FLAG;
    app.last_pressed_key = key;
}

static void key_repeat_callback(qu_key key)
{
    app.event_flags |= REPEAT_EVENT_FLAG;
    app.last_repeated_key = key;
}

static void key_release_callback(qu_key key)
{
    app.event_flags |= RELEASE_EVENT_FLAG;
    app.last_released_key = key;
}

//------------------------------------------------------------------------------

static void update(void)
{
    app.event_flags = 0;
    app.keyboard_state = *qu_get_keyboard_state();
}

static void draw(float lag_offset)
{
    qu_clear(QU_COLOR(0, 0, 0));

    qu_draw_text_fmt(app.font16, 10.f, 100.f,
                     (app.event_flags & PRESS_EVENT_FLAG) ? 0xFFFF0000 : 0xFFABCDEF,
                     "Last pressed key: %s", key_names[app.last_pressed_key]);

    qu_draw_text_fmt(app.font16, 10.f, 120.f,
                     (app.event_flags & REPEAT_EVENT_FLAG) ? 0xFFFF0000 : 0xFFABCDEF,
                     "Last repeated key: %s", key_names[app.last_repeated_key]);

    qu_draw_text_fmt(app.font16, 10.f, 140.f,
                     (app.event_flags & RELEASE_EVENT_FLAG) ? 0xFFFF0000 : 0xFFABCDEF,
                     "Last released key: %s", key_names[app.last_released_key]);

    qu_draw_text(app.font16, 10.f, 200.f, 0xFFABCDEF, "Currently pressed keys:");

    float y = 220.f;

    for (int i = 0; i < QU_TOTAL_KEYS; i++) {
        if (app.keyboard_state.keys[i] == QU_KEY_PRESSED) {
            qu_draw_text_fmt(app.font16, 10.f, y, 0xFFFF0000, "%s", key_names[i]);
            y += 20.f;
        }
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
    qu_set_window_title("[libquack sample] keyboard");
    qu_set_window_size(512, 512);
    qu_set_window_flags(QU_WINDOW_USE_CANVAS);

    qu_initialize();

    app.font12 = qu_load_font("assets/unispace.ttf", 12.f);
    app.font16 = qu_load_font("assets/unispace.ttf", 16.f);
    app.font18 = qu_load_font("assets/unispace.ttf", 18.f);

    app.last_pressed_key = QU_TOTAL_KEYS;
    app.last_repeated_key = QU_TOTAL_KEYS;
    app.last_released_key = QU_TOTAL_KEYS;

    qu_on_key_pressed(key_press_callback);
    qu_on_key_repeated(key_repeat_callback);
    qu_on_key_released(key_release_callback); 

    qu_execute(loop);
}
