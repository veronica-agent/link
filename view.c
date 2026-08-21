#include "link.h"

#include <stdio.h>
#include <gui/elements.h>

const char* link_mode_name(LinkMode mode) {
    switch(mode) {
    case LinkModeSlave:
        return "SLAVE";
    case LinkModeMaster:
        return "MASTER";
    case LinkModeTap:
        return "TAP";
    default:
        return "?";
    }
}

const char* link_transport_name(LinkTransport t) {
    switch(t) {
    case LinkTransportWait:
        return "WAIT";
    case LinkTransportRun:
        return "RUN";
    default:
        return "STOP";
    }
}

const char* link_usb_name(LinkUsbState s) {
    switch(s) {
    case LinkUsbMidi:
        return "USB";
    case LinkUsbBusy:
        return "BUSY";
    default:
        return "---";
    }
}

const char* link_divide_name(uint8_t divide) {
    switch(divide) {
    case 2:
        return "1/2";
    case 4:
        return "1/4";
    case 8:
        return "1/8";
    default:
        return "1/1";
    }
}

void link_view_draw(Canvas* canvas, void* ctx) {
    LinkApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    LinkMode mode = app->mode;
    LinkTransport transport = app->transport;
    LinkUsbState usb = app->usb;
    LinkPinout pinout = app->pinout;
    uint16_t bpm = app->bpm;
    uint8_t divide = app->divide;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "LINK");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 2, AlignRight, AlignTop, link_usb_name(usb));
    canvas_draw_str(canvas, 2, 26, link_mode_name(mode));
    canvas_draw_str_aligned(canvas, 126, 16, AlignRight, AlignTop, link_transport_name(transport));

    char line[24];
    snprintf(line, sizeof(line), "%u BPM", bpm);
    canvas_draw_str(canvas, 2, 40, line);
    canvas_draw_str_aligned(canvas, 126, 30, AlignRight, AlignTop, link_divide_name(divide));
    canvas_draw_str(canvas, 2, 54, link_pinout_name(pinout));
    canvas_draw_str_aligned(canvas, 126, 44, AlignRight, AlignTop, "L/R mode");
}

static void link_cycle_divide(LinkApp* app, int dir) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    static const uint8_t steps[] = {1, 2, 4, 8};
    int idx = 0;
    for(int i = 0; i < 4; i++) {
        if(steps[i] == app->divide) idx = i;
    }
    idx += dir;
    if(idx < 0) idx = 3;
    if(idx > 3) idx = 0;
    app->divide = steps[idx];
    app->divide_count = 0;
    furi_mutex_release(app->mutex);
    LinkEvent ev = {.type = LinkEventTypeRedraw};
    furi_message_queue_put(app->queue, &ev, 0);
}

void link_view_input(InputEvent* event, void* ctx) {
    LinkApp* app = ctx;
    if(event->type != InputTypeShort && event->type != InputTypeLong &&
       event->type != InputTypeRepeat) {
        return;
    }

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        LinkEvent ev = {.type = LinkEventTypeExit};
        furi_message_queue_put(app->queue, &ev, FuriWaitForever);
        return;
    }
    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        link_pinout_cycle(app);
        return;
    }

    if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        LinkMode m = (app->mode == LinkModeSlave) ? LinkModeTap : (LinkMode)(app->mode - 1);
        link_engine_set_mode(app, m);
        return;
    }
    if(event->key == InputKeyRight && event->type == InputTypeShort) {
        LinkMode m = (app->mode == LinkModeTap) ? LinkModeSlave : (LinkMode)(app->mode + 1);
        link_engine_set_mode(app, m);
        return;
    }

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        LinkTransport next = (app->transport == LinkTransportRun) ? LinkTransportStop :
                                                                   LinkTransportRun;
        furi_mutex_release(app->mutex);
        link_engine_set_transport(app, next);
        return;
    }

    int step = (event->type == InputTypeRepeat) ? 5 : 1;
    if(event->key == InputKeyUp) {
        if(app->mode == LinkModeTap) {
            link_engine_set_bpm(app, step);
        } else {
            link_cycle_divide(app, 1);
        }
    } else if(event->key == InputKeyDown) {
        if(app->mode == LinkModeTap) {
            link_engine_set_bpm(app, -step);
        } else {
            link_cycle_divide(app, -1);
        }
    }
}
