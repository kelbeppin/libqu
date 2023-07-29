
#include <libqu.h>

int main(int argc, char *argv[])
{
    qu_initialize(&(qu_params) {
        .display_width = 512,
        .display_height = 512,
        .enable_canvas = true,
    });

    while (qu_process() && !qu_is_key_pressed(QU_KEY_ESCAPE)) {
        float t = qu_get_time_mediump();

        qu_clear(QU_COLOR(54, 54, 54));
        qu_push_matrix();
        qu_translate(256.f, 256.f);
        qu_rotate(t * 45.f);
        qu_draw_rectangle(-64.f, -64.f, 128.f, 128.f, QU_COLOR(255, 255, 255), 0);
        qu_pop_matrix();
        qu_present();
    }

    return 0;
}
