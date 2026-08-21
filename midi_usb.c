#include "link.h"

#include <usb.h>
#include <usb_std.h>

/* USB MIDI 1.0 device class — descriptors follow USB-IF Appendix B
 * (simple MIDI adapter). New implementation, not a copy of any FAP. */

#define LINK_USB_VID 0x0483
#define LINK_USB_PID 0x4C4B
#define LINK_EP0_SIZE 8
#define LINK_MIDI_EP_SIZE 64
#define LINK_MIDI_EP_IN 0x81
#define LINK_MIDI_EP_OUT 0x01

#define USB_AUDIO_SUBCLASS_CONTROL 0x01
#define USB_AUDIO_SUBCLASS_MIDI 0x03
#define USB_CS_INTERFACE 0x24
#define USB_CS_ENDPOINT 0x25
#define USB_AUDIO_HEADER 0x01
#define USB_MIDI_HEADER 0x01
#define USB_MIDI_IN_JACK 0x02
#define USB_MIDI_OUT_JACK 0x03
#define USB_MIDI_JACK_EMBEDDED 0x01
#define USB_MIDI_JACK_EXTERNAL 0x02
#define USB_MIDI_GENERAL 0x01

static const struct usb_device_descriptor link_dev_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = LINK_EP0_SIZE,
    .idVendor = LINK_USB_VID,
    .idProduct = LINK_USB_PID,
    .bcdDevice = VERSION_BCD(0, 1, 0),
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

struct LinkAcHeader {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdADC;
    uint16_t wTotalLength;
    uint8_t bInCollection;
    uint8_t baInterfaceNr;
} FURI_PACKED;

struct LinkMsHeader {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdMSC;
    uint16_t wTotalLength;
} FURI_PACKED;

struct LinkMidiInJack {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bJackType;
    uint8_t bJackID;
    uint8_t iJack;
} FURI_PACKED;

struct LinkMidiOutJack {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bJackType;
    uint8_t bJackID;
    uint8_t bNrInputPins;
    uint8_t baSourceID;
    uint8_t baSourcePin;
    uint8_t iJack;
} FURI_PACKED;

struct LinkMidiCsEp {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bNumEmbMIDIJack;
    uint8_t baAssocJackID;
} FURI_PACKED;

struct LinkMidiCfg {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor ac_if;
    struct LinkAcHeader ac_cs;
    struct usb_interface_descriptor ms_if;
    struct LinkMsHeader ms_hdr;
    struct LinkMidiInJack in_emb;
    struct LinkMidiInJack in_ext;
    struct LinkMidiOutJack out_emb;
    struct LinkMidiOutJack out_ext;
    struct usb_endpoint_descriptor bulk_out;
    struct LinkMidiCsEp cs_out;
    struct usb_endpoint_descriptor bulk_in;
    struct LinkMidiCsEp cs_in;
} FURI_PACKED;

#define LINK_MS_CS_LEN                                       \
    (sizeof(struct LinkMsHeader) + 2 * sizeof(struct LinkMidiInJack) + \
     2 * sizeof(struct LinkMidiOutJack))

static const struct LinkMidiCfg link_cfg_desc = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct LinkMidiCfg),
            .bNumInterfaces = 2,
            .bConfigurationValue = 1,
            .iConfiguration = 0,
            .bmAttributes = USB_CFG_ATTR_RESERVED,
            .bMaxPower = USB_CFG_POWER_MA(100),
        },
    .ac_if =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 0,
            .bInterfaceClass = USB_CLASS_AUDIO,
            .bInterfaceSubClass = USB_AUDIO_SUBCLASS_CONTROL,
            .bInterfaceProtocol = USB_PROTO_NONE,
            .iInterface = 0,
        },
    .ac_cs =
        {
            .bLength = sizeof(struct LinkAcHeader),
            .bDescriptorType = USB_CS_INTERFACE,
            .bDescriptorSubtype = USB_AUDIO_HEADER,
            .bcdADC = VERSION_BCD(1, 0, 0),
            .wTotalLength = sizeof(struct LinkAcHeader),
            .bInCollection = 1,
            .baInterfaceNr = 1,
        },
    .ms_if =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 1,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = USB_CLASS_AUDIO,
            .bInterfaceSubClass = USB_AUDIO_SUBCLASS_MIDI,
            .bInterfaceProtocol = USB_PROTO_NONE,
            .iInterface = 0,
        },
    .ms_hdr =
        {
            .bLength = sizeof(struct LinkMsHeader),
            .bDescriptorType = USB_CS_INTERFACE,
            .bDescriptorSubtype = USB_MIDI_HEADER,
            .bcdMSC = VERSION_BCD(1, 0, 0),
            .wTotalLength = LINK_MS_CS_LEN,
        },
    .in_emb = {6, USB_CS_INTERFACE, USB_MIDI_IN_JACK, USB_MIDI_JACK_EMBEDDED, 0x01, 0},
    .in_ext = {6, USB_CS_INTERFACE, USB_MIDI_IN_JACK, USB_MIDI_JACK_EXTERNAL, 0x02, 0},
    .out_emb = {9, USB_CS_INTERFACE, USB_MIDI_OUT_JACK, USB_MIDI_JACK_EMBEDDED, 0x03, 1, 0x02, 1, 0},
    .out_ext = {9, USB_CS_INTERFACE, USB_MIDI_OUT_JACK, USB_MIDI_JACK_EXTERNAL, 0x04, 1, 0x01, 1, 0},
    .bulk_out =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = LINK_MIDI_EP_OUT,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = LINK_MIDI_EP_SIZE,
            .bInterval = 0,
        },
    .cs_out = {5, USB_CS_ENDPOINT, USB_MIDI_GENERAL, 1, 0x01},
    .bulk_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = LINK_MIDI_EP_IN,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = LINK_MIDI_EP_SIZE,
            .bInterval = 0,
        },
    .cs_in = {5, USB_CS_ENDPOINT, USB_MIDI_GENERAL, 1, 0x03},
};

static const struct usb_string_descriptor link_manuf = USB_STRING_DESC("Obedience Corp");
static const struct usb_string_descriptor link_prod = USB_STRING_DESC("Link");
static const struct usb_string_descriptor link_serial = USB_STRING_DESC("1");

static void link_midi_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
static void link_midi_deinit(usbd_device* dev);
static void link_midi_wakeup(usbd_device* dev);
static void link_midi_suspend(usbd_device* dev);

FuriHalUsbInterface link_midi_usb = {
    .init = link_midi_init,
    .deinit = link_midi_deinit,
    .wakeup = link_midi_wakeup,
    .suspend = link_midi_suspend,
    .dev_descr = (struct usb_device_descriptor*)&link_dev_desc,
    .cfg_descr = (void*)&link_cfg_desc,
};

typedef struct {
    usbd_device* dev;
    void (*rx_cb)(void* ctx);
    void* rx_ctx;
    FuriSemaphore* tx_sem;
    volatile bool connected;
} LinkMidi;

static LinkMidi midi;

void link_midi_set_rx_callback(void (*cb)(void* ctx), void* ctx) {
    midi.rx_cb = cb;
    midi.rx_ctx = ctx;
}

bool link_midi_connected(void) {
    return midi.connected;
}

size_t link_midi_rx(uint8_t* buf, size_t max) {
    if(!midi.dev) return 0;
    int32_t n = usbd_ep_read(midi.dev, LINK_MIDI_EP_OUT, buf, max);
    return n > 0 ? (size_t)n : 0;
}

bool link_midi_tx_packet(uint8_t cin, uint8_t b0, uint8_t b1, uint8_t b2) {
    if(!midi.dev || !midi.connected || !midi.tx_sem) return false;
    if(furi_semaphore_acquire(midi.tx_sem, 20) != FuriStatusOk) return false;
    uint8_t pkt[4] = {cin, b0, b1, b2};
    int32_t n = usbd_ep_write(midi.dev, LINK_MIDI_EP_IN, pkt, 4);
    if(n <= 0) {
        furi_semaphore_release(midi.tx_sem);
        return false;
    }
    return true;
}

static void link_midi_ep_cb(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(ep);
    if(event == usbd_evt_eptx) {
        if(midi.tx_sem) furi_semaphore_release(midi.tx_sem);
    } else if(event == usbd_evt_eprx) {
        if(midi.rx_cb) midi.rx_cb(midi.rx_ctx);
    }
}

static usbd_respond link_midi_ep_config(usbd_device* dev, uint8_t cfg) {
    if(cfg == 0) {
        usbd_ep_deconfig(dev, LINK_MIDI_EP_OUT);
        usbd_ep_deconfig(dev, LINK_MIDI_EP_IN);
        usbd_reg_endpoint(dev, LINK_MIDI_EP_OUT, NULL);
        usbd_reg_endpoint(dev, LINK_MIDI_EP_IN, NULL);
        return usbd_ack;
    }
    usbd_ep_config(dev, LINK_MIDI_EP_OUT, USB_EPTYPE_BULK, LINK_MIDI_EP_SIZE);
    usbd_ep_config(dev, LINK_MIDI_EP_IN, USB_EPTYPE_BULK, LINK_MIDI_EP_SIZE);
    usbd_reg_endpoint(dev, LINK_MIDI_EP_OUT, link_midi_ep_cb);
    usbd_reg_endpoint(dev, LINK_MIDI_EP_IN, link_midi_ep_cb);
    return usbd_ack;
}

static usbd_respond
    link_midi_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(dev);
    UNUSED(req);
    UNUSED(callback);
    return usbd_fail;
}

static void link_midi_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);
    link_midi_usb.str_manuf_descr = (void*)&link_manuf;
    link_midi_usb.str_prod_descr = (void*)&link_prod;
    link_midi_usb.str_serial_descr = (void*)&link_serial;
    midi.dev = dev;
    if(!midi.tx_sem) midi.tx_sem = furi_semaphore_alloc(1, 1);
    usbd_reg_config(dev, link_midi_ep_config);
    usbd_reg_control(dev, link_midi_control);
    usbd_connect(dev, true);
}

static void link_midi_deinit(usbd_device* dev) {
    midi.connected = false;
    midi.dev = NULL;
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
}

static void link_midi_wakeup(usbd_device* dev) {
    UNUSED(dev);
    midi.connected = true;
}

static void link_midi_suspend(usbd_device* dev) {
    UNUSED(dev);
    midi.connected = false;
}
