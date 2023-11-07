
//------------------------------------------------------------------------------

#include <libqu.h>
#include <math.h>
#include <stdio.h>

//------------------------------------------------------------------------------
// Forward declarations

static void exit_loop(void);

//------------------------------------------------------------------------------
// Application

enum
{
    TEXTURE_SKY,
    TEXTURE_MOUNTAINS_BG,
    TEXTURE_MOUNTAINS_FG,
    TEXTURE_TREES_BG,
    TEXTURE_TREES_FG,
    TOTAL_TEXTURES,
};

enum
{
    FONT_ALAGARD16,
    FONT_ALAGARD32,
    FONT_ROMULUS16,
    TOTAL_FONTS,
};

struct app
{
    qu_texture textures[TOTAL_TEXTURES];
    qu_font fonts[TOTAL_FONTS];

    int clock_hours;
    int clock_minutes;
    int clock_seconds;

    char message[512];

    float x_camera;
    float dx_camera;
};

static struct app app;

static void format_date(char *out, size_t size, qu_date_time const *date_time)
{
    if (date_time->weekday < 1 || date_time->weekday > 7) {
        return;
    }

    if (date_time->month < 1 || date_time->month > 12) {
        return;
    }

    char const *weekdays[] = {
        "monday", "tuesday", "wednesday",
        "thursday", "friday", "saturday",
        "sunday",
    };

    char const *months[] = {
        "Jan", "Feb", "Mar",
        "Apr", "May", "Jun",
        "Jul", "Aug", "Sep",
        "Oct", "Nov", "Dec",
    };

    snprintf(out, size, "%s, %d %s %d",
        weekdays[date_time->weekday - 1],
        date_time->day,
        months[date_time->month - 1],
        date_time->year);
}

static void app_initialize(void)
{
    char const *texturePaths[TOTAL_TEXTURES] = {
        "assets/sky.png",
        "assets/mountains-bg.png",
        "assets/mountains-fg.png",
        "assets/trees-bg.png",
        "assets/trees-fg.png",
    };

    for (int i = 0; i < TOTAL_TEXTURES; i++) {
        app.textures[i] = qu_load_texture(texturePaths[i]);
        qu_set_texture_smooth(app.textures[i], false);
    }

    char const *fontPaths[TOTAL_FONTS] = {
        "assets/alagard.ttf",
        "assets/alagard.ttf",
        "assets/romulus.ttf",
    };

    float fontSizes[TOTAL_FONTS] = {
        16.f,
        32.f,
        16.f,
    };

    for (int i = 0; i < TOTAL_FONTS; i++) {
        app.fonts[i] = qu_load_font(fontPaths[i], fontSizes[i]);
    }

    app.dx_camera = 8.f;
}

static void app_update(void)
{
    qu_date_time date_time = qu_get_date_time();

    app.clock_hours = date_time.hours;
    app.clock_minutes = date_time.minutes;
    app.clock_seconds = date_time.seconds;

    format_date(app.message, sizeof(app.message) - 1, &date_time);

    app.x_camera += app.dx_camera;
}

static void draw_clock(int hours, int minutes, int seconds)
{
    qu_color color = 0xfff0b905;
    qu_color shadow = 0xff000000;

    qu_font hm_font = app.fonts[FONT_ALAGARD32];
    qu_font s_font = app.fonts[FONT_ALAGARD16];

    char hm_buf[32];
    char s_buf[32];

    snprintf(hm_buf, 31, "%02d:%02d", hours, minutes);
    snprintf(s_buf, 31, ":%02d", seconds);

    qu_vec2f hm_box = qu_calculate_text_box(hm_font, hm_buf);
    qu_vec2f s_box = qu_calculate_text_box(s_font, s_buf);

    float x = 240.f / 2.f - (hm_box.x + s_box.x) / 2.f;
    float hm_y = 100.f;
    float s_y = 110.f;

    qu_draw_text_fmt(hm_font, x + 2.f, hm_y + 2.f, shadow, hm_buf);
    qu_draw_text_fmt(hm_font, x, hm_y, color, hm_buf);

    qu_draw_text_fmt(s_font, x + hm_box.x + 2.f, s_y + 2.f, shadow, s_buf);
    qu_draw_text_fmt(s_font, x + hm_box.x, s_y, color, s_buf);

    qu_draw_text(app.fonts[FONT_ROMULUS16], 9.f, 141.f, QU_COLOR(0, 0, 0), app.message);
    qu_draw_text(app.fonts[FONT_ROMULUS16], 8.f, 140.f, QU_COLOR(210, 199, 234), app.message);
}

static void app_draw(float lagOffset)
{
    qu_clear(0xff800080);

    qu_draw_texture(app.textures[TEXTURE_SKY], -16.f, 0.f, 272.f, 160.f);

    float x_4 = fmodf(app.x_camera / 4.f, 272.f) + (app.dx_camera / 4.f) * lagOffset;
    qu_draw_texture(app.textures[TEXTURE_MOUNTAINS_BG], -x_4, 0.f, 272.f, 160.f);
    qu_draw_texture(app.textures[TEXTURE_MOUNTAINS_BG], 272.f - x_4, 0.f, 272.f, 160.f);
    
    float x_3 = fmodf(app.x_camera / 3.f, 544.f) + (app.dx_camera / 3.f) * lagOffset;
    qu_draw_texture(app.textures[TEXTURE_MOUNTAINS_FG], -x_3, 0.f, 544.f, 160.f);
    qu_draw_texture(app.textures[TEXTURE_MOUNTAINS_FG], 544.f - x_3, 0.f, 544.f, 160.f);

    float x_2 = fmodf(app.x_camera / 2.f, 544.f) + (app.dx_camera / 2.f) * lagOffset;
    qu_draw_texture(app.textures[TEXTURE_TREES_BG], -x_2, 0.f, 544.f, 160.f);
    qu_draw_texture(app.textures[TEXTURE_TREES_BG], 544.f - x_2, 0.f, 544.f, 160.f);

    float x_1 = fmodf(app.x_camera, 544.f) + app.dx_camera * lagOffset;
    qu_draw_texture(app.textures[TEXTURE_TREES_FG], -x_1, 0.f, 544.f, 160.f);
    qu_draw_texture(app.textures[TEXTURE_TREES_FG], 544.f - x_1, 0.f, 544.f, 160.f);

    draw_clock(app.clock_hours, app.clock_minutes, app.clock_seconds);

    qu_present();
}

//------------------------------------------------------------------------------
// Loop [TODO: integrate into libquack]

struct loop
{
    bool running;

    int tick_rate;
    double frame_duration;

    double frame_start_time;
    double frame_lag_time;

    void (*update_callback)(void);
    void (*draw_callback)(float);
};

static struct loop loop;

static bool loop_callback(void)
{
    if (!loop.running) {
        return false;
    }

    double currentTime = qu_get_time_highp();
    double elapsedTime = currentTime - loop.frame_start_time;

    loop.frame_start_time = currentTime;
    loop.frame_lag_time += elapsedTime;

    while (loop.frame_lag_time >= loop.frame_duration) {
        loop.update_callback();
        loop.frame_lag_time -= loop.frame_duration;
    }

    loop.draw_callback((float) loop.frame_lag_time * loop.tick_rate);

    return true;
}

static int execute_loop(int tickRate, void (*updateCallback)(void), void (*drawCallback)(float))
{
    loop.running = true;

    loop.tick_rate = tickRate;
    loop.frame_duration = 1.0 / tickRate;

    loop.frame_start_time = 0.0;
    loop.frame_lag_time = 0.0;

    loop.update_callback = updateCallback;
    loop.draw_callback = drawCallback;

    qu_execute(loop_callback);

    return 0;
}

static void exit_loop(void)
{
    loop.running = false;
}

//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    qu_set_window_title("libqu sample: clock");
    qu_set_window_size(720, 480);
    qu_set_window_flags(QU_WINDOW_USE_CANVAS);

    qu_set_canvas_size(240, 160);

    qu_initialize();

    app_initialize();

    return execute_loop(10, app_update, app_draw);
}
