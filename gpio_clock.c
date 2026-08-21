#include "link.h"

#define TAG "LinkGpio"

static void link_master_isr(void* ctx) {
    LinkApp* app = ctx;
    bool bit = furi_hal_gpio_read(app->si);
    app->master_byte = (uint8_t)((app->master_byte << 1) | (bit ? 1 : 0));
    app->master_bits++;
    if(app->master_bits >= 8) {
        app->master_bit = 1;
        app->master_bits = 0;
        furi_thread_flags_set(furi_thread_get_id(app->worker), 1u << 1);
    }
    furi_thread_flags_set(furi_thread_get_id(app->worker), 1u << 2);
}

void link_gpio_clock_tick(LinkApp* app) {
    const GpioPin* clk = app->clk;
    if(!clk) return;
    for(int i = 0; i < 8; i++) {
        furi_hal_gpio_write(clk, false);
        furi_hal_gpio_write(clk, true);
    }
}

void link_pinout_release(LinkApp* app) {
    link_gpio_master_isr_enable(app, false);
    if(!app->gpio_live) return;
    if(app->clk) {
        furi_hal_gpio_init(app->clk, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
    if(app->si) {
        furi_hal_gpio_init(app->si, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
    if(app->so) {
        furi_hal_gpio_init(app->so, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
    app->gpio_live = false;
}

void link_gpio_set_slave_pins(LinkApp* app) {
    link_gpio_master_isr_enable(app, false);
    furi_hal_gpio_init(app->clk, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(app->clk, true);
    furi_hal_gpio_init(app->si, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(app->si, false);
    furi_hal_gpio_init(app->so, GpioModeInput, GpioPullNo, GpioSpeedLow);
    app->gpio_live = true;
}

void link_gpio_set_master_pins(LinkApp* app) {
    link_gpio_master_isr_enable(app, false);
    furi_hal_gpio_init(app->clk, GpioModeInput, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_init(app->si, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(app->so, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(app->so, false);
    app->gpio_live = true;
}

void link_gpio_master_isr_enable(LinkApp* app, bool enable) {
    if(enable) {
        if(app->clk_isr_on) return;
        app->master_bits = 0;
        app->master_byte = 0;
        furi_hal_gpio_init(app->clk, GpioModeInterruptFall, GpioPullUp, GpioSpeedVeryHigh);
        furi_hal_gpio_add_int_callback(app->clk, link_master_isr, app);
        furi_hal_gpio_enable_int_callback(app->clk);
        app->clk_isr_on = true;
    } else if(app->clk_isr_on) {
        furi_hal_gpio_disable_int_callback(app->clk);
        furi_hal_gpio_remove_int_callback(app->clk);
        app->clk_isr_on = false;
    }
}
