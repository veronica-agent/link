#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>

#define LINK_BPM_MIN 40
#define LINK_BPM_MAX 240
#define LINK_BPM_DEFAULT 120
#define LINK_MIDI_CHANNEL 15 /* 1-indexed channel 16 */
#define LINK_WATCHDOG_MS 20

typedef enum {
    LinkModeSlave,
    LinkModeMaster,
    LinkModeTap,
} LinkMode;

typedef enum {
    LinkTransportStop,
    LinkTransportWait,
    LinkTransportRun,
} LinkTransport;

typedef enum {
    LinkUsbNone,
    LinkUsbMidi,
    LinkUsbBusy,
} LinkUsbState;

typedef enum {
    LinkPinoutOriginal,
    LinkPinoutMlvk25,
} LinkPinout;

typedef enum {
    LinkEventTypeInput,
    LinkEventTypeRedraw,
    LinkEventTypeExit,
} LinkEventType;

typedef struct {
    LinkEventType type;
    InputEvent input;
} LinkEvent;

typedef struct LinkApp LinkApp;

struct LinkApp {
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    FuriMutex* mutex;
    FuriThread* worker;
    FuriTimer* watchdog;
    FuriTimer* tap_timer;
    FuriHalUsbInterface* usb_prev;

    LinkMode mode;
    LinkTransport transport;
    LinkUsbState usb;
    LinkPinout pinout;
    uint16_t bpm;
    uint8_t divide; /* 1, 2, 4, or 8 */
    uint8_t divide_count;
    uint8_t midi_channel;

    const GpioPin* clk;
    const GpioPin* si;
    const GpioPin* so;
    bool clk_isr_on;
    bool gpio_live;

    volatile uint8_t master_bits;
    volatile uint8_t master_byte;
    volatile uint8_t master_bit;
    volatile bool worker_run;
};

void link_pinout_apply(LinkApp* app);
void link_pinout_release(LinkApp* app);
void link_pinout_cycle(LinkApp* app);
const char* link_pinout_name(LinkPinout pinout);
void link_pinout_load(LinkApp* app);
void link_pinout_save(const LinkApp* app);

void link_gpio_clock_tick(LinkApp* app);
void link_gpio_set_slave_pins(LinkApp* app);
void link_gpio_set_master_pins(LinkApp* app);
void link_gpio_master_isr_enable(LinkApp* app, bool enable);

extern FuriHalUsbInterface link_midi_usb;
void link_midi_set_rx_callback(void (*cb)(void* ctx), void* ctx);
bool link_midi_connected(void);
size_t link_midi_rx(uint8_t* buf, size_t max);
bool link_midi_tx_packet(uint8_t cin, uint8_t b0, uint8_t b1, uint8_t b2);

void link_engine_start(LinkApp* app);
void link_engine_stop(LinkApp* app);
void link_engine_set_mode(LinkApp* app, LinkMode mode);
void link_engine_set_transport(LinkApp* app, LinkTransport t);
void link_engine_set_bpm(LinkApp* app, int delta);
void link_engine_on_midi_bytes(LinkApp* app, const uint8_t* data, size_t len);
void link_engine_on_master_byte(LinkApp* app);

void link_view_draw(Canvas* canvas, void* ctx);
void link_view_input(InputEvent* event, void* ctx);

const char* link_mode_name(LinkMode mode);
const char* link_transport_name(LinkTransport t);
const char* link_usb_name(LinkUsbState s);
const char* link_divide_name(uint8_t divide);
