#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <math.h>

#include "ld2450_radar_uart.h"

#define RADAR_RANGE_FT 16.0f

typedef enum {
    LD2450ScreenText,
    LD2450ScreenRadar,
} LD2450Screen;

typedef struct {
    LD2450Data data;
    bool connected;
    bool uart_init_failed;
    uint32_t last_packet_time;
    uint32_t packet_count;
    uint32_t rx_bytes;
    LD2450Screen screen;
} LD2450RadarModel;

typedef struct {
    LD2450RadarUart* uart;
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    LD2450RadarModel model;
    FuriMutex* model_mutex;
} LD2450RadarApp;

typedef enum {
    LD2450RadarEventInput,
    LD2450RadarEventDataUpdate,
} LD2450RadarEventType;

typedef struct {
    LD2450RadarEventType type;
    InputEvent input;
} LD2450RadarEvent;

static float mm_to_ft(int16_t mm) {
    return (float)mm / 304.8f;
}

static void ld2450_radar_uart_callback(LD2450Data* data, void* context) {
    LD2450RadarApp* app = context;

    furi_mutex_acquire(app->model_mutex, FuriWaitForever);
    app->model.data = *data;
    app->model.connected = true;
    app->model.last_packet_time = furi_get_tick();
    app->model.packet_count++;
    furi_mutex_release(app->model_mutex);
}

static void ld2450_draw_text(Canvas* canvas, LD2450RadarModel* model) {
    char buffer[48];

    snprintf(buffer, sizeof(buffer), "Pkt:%lu RX:%lu", model->packet_count, model->rx_bytes);
    canvas_draw_str(canvas, 0, 20, buffer);

    for(uint8_t i = 0; i < LD2450_TARGET_COUNT; i++) {
        LD2450Target* t = &model->data.targets[i];

        if(t->valid) {
            snprintf(
                buffer,
                sizeof(buffer),
                "T%d X:%.1fft Y:%.1fft",
                i + 1,
                (double)mm_to_ft(t->x_mm),
                (double)mm_to_ft(t->y_mm));
        } else {
            snprintf(buffer, sizeof(buffer), "T%d none", i + 1);
        }

        canvas_draw_str(canvas, 0, 32 + (i * 10), buffer);
    }

    canvas_draw_str(canvas, 84, 63, "OK map");
}

static void ld2450_draw_radar(Canvas* canvas, LD2450RadarModel* model) {
    const int sensor_x = 64;
    const int sensor_y = 62;
    const int max_y_px = 48;
    const int max_x_px = 58;

    canvas_draw_str(canvas, 86, 8, "16ft");

    canvas_draw_line(canvas, sensor_x, sensor_y, 8, 14);
    canvas_draw_line(canvas, sensor_x, sensor_y, 120, 14);
    canvas_draw_line(canvas, sensor_x, sensor_y, sensor_x, 14);

    canvas_draw_circle(canvas, sensor_x, sensor_y, 12);
    canvas_draw_circle(canvas, sensor_x, sensor_y, 24);
    canvas_draw_circle(canvas, sensor_x, sensor_y, 36);
    canvas_draw_circle(canvas, sensor_x, sensor_y, 48);

    canvas_draw_str(canvas, sensor_x - 10, sensor_y, "LD");

    for(uint8_t i = 0; i < LD2450_TARGET_COUNT; i++) {
        LD2450Target* t = &model->data.targets[i];

        if(!t->valid) continue;

        float x_ft = -mm_to_ft(t->x_mm);
        float y_ft = mm_to_ft(t->y_mm);

        if(y_ft <= 0.0f || y_ft > RADAR_RANGE_FT) continue;

        /*
        LD2450 horizontal FOV is about 120 degrees total.
        That means about 60 degrees left/right from center.

        At each distance, the cone half-width is:
        half_width = distance * tan(60 deg)
        tan(60) ≈ 1.732
        */
        float cone_half_width_ft = y_ft * 1.732f;

        /* Ignore targets outside the cone */
        if(x_ft < -cone_half_width_ft || x_ft > cone_half_width_ft) continue;

        /*
        Normalize X based on cone width at that Y distance.
        So close targets have less side-to-side room,
        far targets have more side-to-side room.
        */
        float x_normalized = x_ft / cone_half_width_ft;

        /* Exaggerate left/right dot movement without changing cone lines */
        x_normalized *= 1.0f;

        if(x_normalized < -1.0f) x_normalized = -1.0f;
        if(x_normalized > 1.0f) x_normalized = 1.0f;

        int dot_x = sensor_x + (int)(x_normalized * max_x_px);
        int dot_y = sensor_y - (int)((y_ft / RADAR_RANGE_FT) * max_y_px);

        if(dot_x < 2) dot_x = 2;
        if(dot_x > 125) dot_x = 125;
        if(dot_y < 12) dot_y = 12;
        if(dot_y > 62) dot_y = 62;

        canvas_draw_disc(canvas, dot_x, dot_y, 2);

        char label[3];
        snprintf(label, sizeof(label), "%d", i + 1);
        canvas_draw_str(canvas, dot_x + 3, dot_y + 3, label);
    }

    canvas_draw_str(canvas, 98, 63, "Radar");
}

static void ld2450_radar_draw_callback(Canvas* canvas, void* context) {
    LD2450RadarApp* app = context;

    furi_mutex_acquire(app->model_mutex, FuriWaitForever);
    LD2450RadarModel model = app->model;
    furi_mutex_release(app->model_mutex);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(model.screen == LD2450ScreenText) {
    canvas_draw_str(canvas, 0, 8, "LD2450 Radar");
    canvas_draw_line(canvas, 0, 10, 127, 10);
    }

    if(model.uart_init_failed) {
        canvas_draw_str(canvas, 10, 32, "UART INIT ERROR");
        return;
    }

    bool stale = (furi_get_tick() - model.last_packet_time > 2000);

    if(!model.connected || stale) {
        canvas_draw_str(canvas, 0, 34, "Waiting for LD2450...");
        canvas_draw_str(canvas, 0, 48, "Check TX/RX/GND/5V");
        return;
    }

    if(model.screen == LD2450ScreenRadar) {
        ld2450_draw_radar(canvas, &model);
    } else {
        ld2450_draw_text(canvas, &model);
    }
}

static void ld2450_radar_input_callback(InputEvent* input_event, void* context) {
    LD2450RadarApp* app = context;

    LD2450RadarEvent event = {
        .type = LD2450RadarEventInput,
        .input = *input_event,
    };

    furi_message_queue_put(app->event_queue, &event, 0);
}

static void ld2450_radar_tick_callback(void* context) {
    LD2450RadarApp* app = context;

    LD2450RadarEvent event = {
        .type = LD2450RadarEventDataUpdate,
    };

    furi_message_queue_put(app->event_queue, &event, 0);
}

int32_t ld2450_radar_app(void* p) {
    UNUSED(p);

    LD2450RadarApp* app = malloc(sizeof(LD2450RadarApp));
    memset(app, 0, sizeof(LD2450RadarApp));

    furi_delay_ms(500);
    app->model.last_packet_time = furi_get_tick();
    app->model.screen = LD2450ScreenText;

    app->event_queue = furi_message_queue_alloc(8, sizeof(LD2450RadarEvent));
    app->model_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, ld2450_radar_draw_callback, app);
    view_port_input_callback_set(app->view_port, ld2450_radar_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->uart = ld2450_radar_uart_alloc();
    ld2450_radar_uart_set_handle_rx_data_cb(app->uart, ld2450_radar_uart_callback, app);

    bool init_success = ld2450_radar_uart_start(app->uart);

    if(!init_success) {
        furi_mutex_acquire(app->model_mutex, FuriWaitForever);
        app->model.uart_init_failed = true;
        furi_mutex_release(app->model_mutex);
    }

    FuriTimer* timer = furi_timer_alloc(ld2450_radar_tick_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(timer, 50);

    LD2450RadarEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == LD2450RadarEventInput) {
                if(event.input.key == InputKeyBack && event.input.type == InputTypeShort) {
                    running = false;
                } else if(event.input.type == InputTypeShort) {
                    furi_mutex_acquire(app->model_mutex, FuriWaitForever);

                    if(event.input.key == InputKeyOk || event.input.key == InputKeyRight) {
                        app->model.screen = LD2450ScreenRadar;
                    } else if(event.input.key == InputKeyLeft) {
                        app->model.screen = LD2450ScreenText;
                    }

                    furi_mutex_release(app->model_mutex);
                    view_port_update(app->view_port);
                }
            } else if(event.type == LD2450RadarEventDataUpdate) {
                furi_mutex_acquire(app->model_mutex, FuriWaitForever);
                app->model.rx_bytes = ld2450_radar_uart_get_rx_bytes(app->uart);
                furi_mutex_release(app->model_mutex);

                view_port_update(app->view_port);
            }
        }
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);

    ld2450_radar_uart_stop(app->uart);
    ld2450_radar_uart_free(app->uart);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_mutex_free(app->model_mutex);
    furi_message_queue_free(app->event_queue);
    free(app);

    return 0;
}