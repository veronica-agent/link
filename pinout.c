#include "link.h"

#include <storage/storage.h>
#include <string.h>

#define TAG "LinkPinout"
#define LINK_PINOUT_PATH APP_DATA_PATH("pinout.txt")

static const GpioPin* original_clk = &gpio_ext_pb2;
static const GpioPin* original_si = &gpio_ext_pc3;
static const GpioPin* original_so = &gpio_ext_pb3;

static const GpioPin* mlvk_clk = &gpio_ext_pb3;
static const GpioPin* mlvk_si = &gpio_ext_pa6;
static const GpioPin* mlvk_so = &gpio_ext_pa7;

void link_pinout_apply(LinkApp* app) {
    if(app->pinout == LinkPinoutMlvk25) {
        app->clk = mlvk_clk;
        app->si = mlvk_si;
        app->so = mlvk_so;
    } else {
        app->clk = original_clk;
        app->si = original_si;
        app->so = original_so;
    }
}

const char* link_pinout_name(LinkPinout pinout) {
    if(pinout == LinkPinoutMlvk25) return "MLVK25";
    return "ORIGINAL";
}

void link_pinout_cycle(LinkApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    link_gpio_master_isr_enable(app, false);
    link_pinout_release(app);
    app->pinout = (app->pinout == LinkPinoutOriginal) ? LinkPinoutMlvk25 : LinkPinoutOriginal;
    link_pinout_apply(app);
    if(app->mode == LinkModeMaster) {
        link_gpio_set_master_pins(app);
        if(app->transport == LinkTransportRun || app->transport == LinkTransportWait) {
            link_gpio_master_isr_enable(app, true);
        }
    } else {
        link_gpio_set_slave_pins(app);
    }
    furi_mutex_release(app->mutex);
    link_pinout_save(app);
}

void link_pinout_load(LinkApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    app->pinout = LinkPinoutOriginal;
    if(storage_file_open(file, LINK_PINOUT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[16] = {0};
        storage_file_read(file, buf, sizeof(buf) - 1);
        if(strncmp(buf, "mlvk25", 6) == 0) app->pinout = LinkPinoutMlvk25;
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    link_pinout_apply(app);
}

void link_pinout_save(const LinkApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, LINK_PINOUT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* s = (app->pinout == LinkPinoutMlvk25) ? "mlvk25\n" : "original\n";
        storage_file_write(file, s, strlen(s));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
