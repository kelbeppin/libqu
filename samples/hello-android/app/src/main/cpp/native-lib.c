
#include <libqu.h>

void android_main(void)
{
    qu_initialize(&(qu_params) {
        .display_width = 512,
        .display_height = 512,
    });

    while (qu_process()) {
        qu_clear(QU_COLOR(35, 35, 35));

        qu_present();
    }

    qu_terminate();
}
