#include "link.h"

#include <stdlib.h>
#include <string.h>

#define TAG "Link"

static LinkApp* link_app_alloc(void) {
    LinkApp* app = malloc(sizeof(LinkApp));
    memset(app, 0, sizeof(LinkApp));
    app->queue = furi_message_queue_alloc(8, sizeof(LinkEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->view_port = view_port_alloc();
    app->mode = LinkModeSlave;
    app->transport = LinkTransportStop;
    app->usb = LinkUsbNone;
    app->bpm = LINK_BPM_DEFAULT;
    app->divide = 1;
    app->midi_channel = LINK_MIDI_CHANNEL;
    link_pinout_load(app);
    view_port_draw_callback_set(app->view_port, link_view_draw, app);
    view_port_input_callback_set(app->view_port, link_view_input, app);
    return app;
}

static void link_app_free(LinkApp* app) {
    view_port_free(app->view_port);
    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->queue);
    free(app);
}

int32_t link_app(void* p) {
    UNUSED(p);
    LinkApp* app = link_app_alloc();

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->usb_prev = furi_hal_usb_get_config();
    if(!furi_hal_usb_set_config(&link_midi_usb, NULL)) {
        app->usb = LinkUsbBusy;
    }

    link_gpio_set_slave_pins(app);
    link_engine_start(app);

    bool running = true;
    LinkEvent event;
    while(running) {
        if(furi_message_queue_get(app->queue, &event, 200) == FuriStatusOk) {
            if(event.type == LinkEventTypeExit) running = false;
        }
        view_port_update(app->view_port);
    }

    link_engine_stop(app);
    if(app->usb != LinkUsbBusy) {
        furi_hal_usb_set_config(app->usb_prev, NULL);
    }

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    link_app_free(app);
    return 0;
}
