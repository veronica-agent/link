#include "link.h"

#define TAG "LinkEngine"

#define FLAG_MIDI (1u << 0)
#define FLAG_MASTER (1u << 1)
#define FLAG_EDGE (1u << 2)
#define FLAG_TAP (1u << 3)
#define FLAG_WD (1u << 4)
#define FLAG_EXIT (1u << 5)
#define FLAG_ALL (FLAG_MIDI | FLAG_MASTER | FLAG_EDGE | FLAG_TAP | FLAG_WD | FLAG_EXIT)

static void link_midi_rx_isr(void* ctx) {
    LinkApp* app = ctx;
    if(app->worker) furi_thread_flags_set(furi_thread_get_id(app->worker), FLAG_MIDI);
}

static void link_tap_timer(void* ctx) {
    LinkApp* app = ctx;
    if(app->worker) furi_thread_flags_set(furi_thread_get_id(app->worker), FLAG_TAP);
}

static void link_wd_timer(void* ctx) {
    LinkApp* app = ctx;
    if(app->worker) furi_thread_flags_set(furi_thread_get_id(app->worker), FLAG_WD);
}

static void link_redraw(LinkApp* app) {
    LinkEvent ev = {.type = LinkEventTypeRedraw};
    furi_message_queue_put(app->queue, &ev, 0);
}

static bool link_divide_hit(LinkApp* app) {
    app->divide_count++;
    if(app->divide_count < app->divide) return false;
    app->divide_count = 0;
    return true;
}

static void link_send_realtime(uint8_t status) {
    link_midi_tx_packet(0x0F, status, 0, 0);
}

static uint32_t link_tap_period_ms(uint16_t bpm) {
    uint32_t ms = (60000u + (bpm * 12u)) / (bpm * 24u);
    if(ms < 1) ms = 1;
    return ms;
}

void link_engine_set_transport(LinkApp* app, LinkTransport t) {
    app->transport = t;
    if(t == LinkTransportStop) {
        app->divide_count = 0;
        if(app->mode == LinkModeMaster) {
            link_gpio_master_isr_enable(app, false);
            link_send_realtime(0xFC);
        }
    } else if(t == LinkTransportRun && app->mode == LinkModeMaster) {
        link_gpio_master_isr_enable(app, true);
    }
    link_redraw(app);
}

void link_engine_set_bpm(LinkApp* app, int delta) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    int bpm = (int)app->bpm + delta;
    if(bpm < LINK_BPM_MIN) bpm = LINK_BPM_MIN;
    if(bpm > LINK_BPM_MAX) bpm = LINK_BPM_MAX;
    app->bpm = (uint16_t)bpm;
    if(app->mode == LinkModeTap && app->tap_timer) {
        furi_timer_start(app->tap_timer, link_tap_period_ms(app->bpm));
    }
    furi_mutex_release(app->mutex);
    link_redraw(app);
}

void link_engine_set_mode(LinkApp* app, LinkMode mode) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    link_gpio_master_isr_enable(app, false);
    if(app->watchdog) furi_timer_stop(app->watchdog);
    if(app->tap_timer) furi_timer_stop(app->tap_timer);
    app->mode = mode;
    app->transport = LinkTransportStop;
    app->divide_count = 0;
    if(mode == LinkModeMaster) {
        link_gpio_set_master_pins(app);
        app->transport = LinkTransportWait;
        link_gpio_master_isr_enable(app, true);
    } else {
        link_gpio_set_slave_pins(app);
        if(mode == LinkModeTap && app->tap_timer) {
            furi_timer_start(app->tap_timer, link_tap_period_ms(app->bpm));
        }
    }
    furi_mutex_release(app->mutex);
    link_redraw(app);
}

void link_engine_on_midi_bytes(LinkApp* app, const uint8_t* data, size_t len) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    for(size_t i = 0; i + 3 < len; i += 4) {
        uint8_t cin = data[i] & 0x0F;
        uint8_t b0 = data[i + 1];
        uint8_t b1 = data[i + 2];
        uint8_t b2 = data[i + 3];
        if(cin == 0x0F || cin == 0x05) {
            if(b0 == 0xF8 && app->mode == LinkModeSlave && app->transport == LinkTransportRun) {
                if(link_divide_hit(app)) link_gpio_clock_tick(app);
            } else if(b0 == 0xFA || b0 == 0xFB) {
                if(app->mode == LinkModeSlave) app->transport = LinkTransportRun;
            } else if(b0 == 0xFC) {
                if(app->mode == LinkModeSlave) app->transport = LinkTransportStop;
            }
        } else if((cin == 0x09 || cin == 0x08) && (b0 & 0x0F) == app->midi_channel) {
            bool on = (cin == 0x09) && (b2 > 0);
            if(!on) continue;
            if(b1 == 48) {
                app->transport = LinkTransportRun;
            } else if(b1 == 49) {
                app->transport = LinkTransportStop;
            } else if(b1 == 50) {
                app->divide = 1;
            } else if(b1 == 51) {
                app->divide = 2;
            } else if(b1 == 52) {
                app->divide = 4;
            } else if(b1 == 53) {
                app->divide = 8;
            }
            app->divide_count = 0;
        }
    }
    furi_mutex_release(app->mutex);
    link_redraw(app);
}

void link_engine_on_master_byte(LinkApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint8_t row = app->master_byte;
    app->master_byte = 0;
    if(app->mode != LinkModeMaster) {
        furi_mutex_release(app->mutex);
        return;
    }
    if(app->transport != LinkTransportRun) {
        app->transport = LinkTransportRun;
        link_midi_tx_packet(0x09, (uint8_t)(0x90 | app->midi_channel), row, 0x7F);
        link_send_realtime(0xFA);
    }
    link_send_realtime(0xF8);
    furi_mutex_release(app->mutex);
    link_redraw(app);
}

static int32_t link_worker(void* ctx) {
    LinkApp* app = ctx;

    while(app->worker_run) {
        uint32_t flags = furi_thread_flags_wait(FLAG_ALL, FuriFlagWaitAny, 100);
        if(flags & FuriFlagError) {
            app->usb = link_midi_connected() ? LinkUsbMidi : app->usb;
            if(app->usb == LinkUsbMidi && !link_midi_connected()) app->usb = LinkUsbNone;
            if(app->usb != LinkUsbBusy && link_midi_connected()) app->usb = LinkUsbMidi;
            continue;
        }
        if(flags & FLAG_EXIT) break;
        if(flags & FLAG_MIDI) {
            uint8_t buf[64];
            size_t n = link_midi_rx(buf, sizeof(buf));
            if(n) link_engine_on_midi_bytes(app, buf, n);
        }
        if(flags & FLAG_MASTER) link_engine_on_master_byte(app);
        if(flags & FLAG_EDGE) {
            if(app->mode == LinkModeMaster) {
                furi_timer_start(app->watchdog, LINK_WATCHDOG_MS);
            }
        }
        if(flags & FLAG_TAP) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            if(app->mode == LinkModeTap && app->transport == LinkTransportRun) {
                if(link_divide_hit(app)) link_gpio_clock_tick(app);
            }
            furi_mutex_release(app->mutex);
        }
        if(flags & FLAG_WD) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            if(app->mode == LinkModeMaster && app->transport == LinkTransportRun) {
                app->transport = LinkTransportStop;
                link_send_realtime(0xFC);
            }
            furi_mutex_release(app->mutex);
            link_redraw(app);
        }
        if(app->usb != LinkUsbBusy) {
            app->usb = link_midi_connected() ? LinkUsbMidi : LinkUsbNone;
        }
    }
    return 0;
}

void link_engine_start(LinkApp* app) {
    link_midi_set_rx_callback(link_midi_rx_isr, app);
    app->watchdog = furi_timer_alloc(link_wd_timer, FuriTimerTypeOnce, app);
    app->tap_timer = furi_timer_alloc(link_tap_timer, FuriTimerTypePeriodic, app);
    app->worker_run = true;
    app->worker = furi_thread_alloc_ex("link_wk", 1024, link_worker, app);
    furi_thread_start(app->worker);
}

void link_engine_stop(LinkApp* app) {
    app->worker_run = false;
    if(app->worker) {
        furi_thread_flags_set(furi_thread_get_id(app->worker), FLAG_EXIT);
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    if(app->tap_timer) {
        furi_timer_stop(app->tap_timer);
        furi_timer_free(app->tap_timer);
        app->tap_timer = NULL;
    }
    if(app->watchdog) {
        furi_timer_stop(app->watchdog);
        furi_timer_free(app->watchdog);
        app->watchdog = NULL;
    }
    link_midi_set_rx_callback(NULL, NULL);
    link_gpio_master_isr_enable(app, false);
    link_pinout_release(app);
}
