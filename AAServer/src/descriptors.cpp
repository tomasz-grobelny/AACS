// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "descriptors.h"
#include "utils.h"
#include <linux/types.h>
#include <linux/usb/functionfs.h>
#include <unistd.h>

static const struct {
  struct usb_functionfs_descs_head_v2 header;
  __le32 fs_count;
  __le32 hs_count;
  struct {
    struct usb_interface_descriptor intf;
    struct usb_endpoint_descriptor_no_audio sink;
    struct usb_endpoint_descriptor_no_audio source;
  } __attribute__((packed)) fs_descs, hs_descs;
} __attribute__((packed)) descriptors_accessory = {
    .header =
        {
            .magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
            .length = cpu_to_le32(sizeof descriptors_accessory),
            .flags =
                cpu_to_le32(FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC),
        },
    .fs_count = cpu_to_le32(3),
    .hs_count = cpu_to_le32(3),
    .fs_descs =
        {
            .intf =
                {
                    .bLength = sizeof descriptors_accessory.fs_descs.intf,
                    .bDescriptorType = USB_DT_INTERFACE,
                    .bNumEndpoints = 2,
                    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
                    .bInterfaceSubClass = USB_SUBCLASS_VENDOR_SPEC,
                    .bInterfaceProtocol = 0x00,
                    .iInterface = 1,
                },
            .sink =
                {
                    .bLength = sizeof descriptors_accessory.fs_descs.sink,
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 1 | USB_DIR_IN,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = cpu_to_le16(512),
                    .bInterval = 0,
                },
            .source =
                {
                    .bLength = sizeof descriptors_accessory.fs_descs.source,
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 2 | USB_DIR_OUT,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = cpu_to_le16(512),
                    .bInterval = 0,
                },
        },
    .hs_descs =
        {
            .intf =
                {
                    .bLength = sizeof descriptors_accessory.hs_descs.intf,
                    .bDescriptorType = USB_DT_INTERFACE,
                    .bNumEndpoints = 2,
                    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
                    .bInterfaceSubClass = USB_SUBCLASS_VENDOR_SPEC,
                    .bInterfaceProtocol = 0x00,
                    .iInterface = 1,
                },
            .sink =
                {
                    .bLength = sizeof descriptors_accessory.hs_descs.sink,
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 1 | USB_DIR_IN,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = cpu_to_le16(512),
                    .bInterval = 0,
                },
            .source =
                {
                    .bLength = sizeof descriptors_accessory.hs_descs.source,
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 2 | USB_DIR_OUT,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = cpu_to_le16(512),
                    .bInterval = 0,
                },
        },
};

static const struct {
  struct usb_functionfs_descs_head_v2 header;
  __le32 fs_count;
  __le32 hs_count;
  struct {
    struct usb_interface_descriptor intf;
  } __attribute__((packed)) fs_descs, hs_descs;
} __attribute__((packed)) descriptors_default = {
    .header =
        {
            .magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
            .length = cpu_to_le32(sizeof descriptors_default),
            .flags =
                cpu_to_le32(FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC |
                            FUNCTIONFS_ALL_CTRL_RECIP),
        },
    .fs_count = cpu_to_le32(1),
    .hs_count = cpu_to_le32(1),
    .fs_descs =
        {
            .intf =
                {
                    .bLength = sizeof descriptors_default.fs_descs.intf,
                    .bDescriptorType = USB_DT_INTERFACE,
                    .bNumEndpoints = 0,
                    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
                    .bInterfaceSubClass = USB_SUBCLASS_VENDOR_SPEC,
                    .bInterfaceProtocol = 0x00,
                    .iInterface = 1,
                },
        },
    .hs_descs =
        {
            .intf =
                {
                    .bLength = sizeof descriptors_default.hs_descs.intf,
                    .bDescriptorType = USB_DT_INTERFACE,
                    .bNumEndpoints = 0,
                    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
                    .bInterfaceSubClass = USB_SUBCLASS_VENDOR_SPEC,
                    .bInterfaceProtocol = 0x00,
                    .iInterface = 1,
                },
        },
};

#define STR1 "Android Accessory Interface"

static const struct {
  struct usb_functionfs_strings_head header;
  struct {
    __le16 code;
    const char str1[sizeof STR1];
  } __attribute__((packed)) lang0;
} __attribute__((packed)) strings = {
    .header =
        {
            .magic = cpu_to_le32(FUNCTIONFS_STRINGS_MAGIC),
            .length = cpu_to_le32(sizeof strings),
            .str_count = cpu_to_le32(1),
            .lang_count = cpu_to_le32(1),
        },
    .lang0 =
        {
            .code = cpu_to_le16(0x0409), /* en-us */
            .str1 = STR1,
        },
};

void write_descriptors_accessory(int fd) {
  checkError(write(fd, &descriptors_accessory, sizeof descriptors_accessory),
             {});
  checkError(write(fd, &strings, sizeof strings), {});
}

void write_descriptors_default(int fd) {
  checkError(write(fd, &descriptors_default, sizeof descriptors_default), {});
  checkError(write(fd, &strings, sizeof strings), {});
}
