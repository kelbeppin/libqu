
#include <libqu.h>

static qu_sound fanfare;
static qu_sound negative;
static qu_stream stream;
static bool paused;

static void on_key_pressed(qu_key key)
{
    switch (key) {
    case QU_KEY_SPACE:
        qu_play_sound(fanfare);
        break;
    case QU_KEY_ENTER:
        if (stream.id) {
            if (paused) {
                qu_unpause_stream(stream);
                paused = false;
            } else {
                qu_pause_stream(stream);
                paused = true;
            }
        } else {
            stream = qu_loop_sound(negative);
        }
        break;
    case QU_KEY_Z:
        qu_stop_stream(stream);
        break;
    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    qu_initialize(&(qu_params) {
        .display_width = 512,
        .display_height = 512,
        .enable_canvas = true,
    });

    fanfare = qu_load_sound("assets/fanfare.wav");
    negative = qu_load_sound("assets/negative.wav");
    
    qu_on_key_pressed(on_key_pressed);

    while (qu_process()) {
        if (qu_is_key_pressed(QU_KEY_ESCAPE)) {
            break;
        }

        float t = qu_get_time_mediump();

        qu_clear(QU_COLOR(0, 0, 0));
        qu_translate(256.f, 256.f);
        qu_rotate(t * 45.f);
        qu_draw_line(-64.f, 0.f, 64.f, 0.f, QU_COLOR(255, 255, 255));
        qu_present();
    }

    return 0;
}
