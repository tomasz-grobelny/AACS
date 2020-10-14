// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "descriptors.h"
#include <linux/usb/functionfs.h>
#include "utils.h"
#include <linux/types.h>
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
} __attribute__((packed)) descriptors = {
    .header =
        {
            .magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
            .length = cpu_to_le32(sizeof descriptors),
            .flags =
                cpu_to_le32(FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC),
        },
    .fs_count = cpu_to_le32(3),
    .hs_count = cpu_to_le32(3),
    .fs_descs =
        {
            .intf =
                {
                    .bLength = sizeof descriptors.fs_descs.intf,
                    .bDescriptorType = USB_DT_INTERFACE,
                    .bNumEndpoints = 2,
                    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
                    .bInterfaceSubClass = USB_SUBCLASS_VENDOR_SPEC,
                    .bInterfaceProtocol = 0x00,
                    .iInterface = 1,
                },
            .sink =
                {
                    .bLength = sizeof descriptors.fs_descs.sink,
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 1 | USB_DIR_IN,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = cpu_to_le16(512),
                    .bInterval = 0,
                },
            .source =
                {
                    .bLength = sizeof descriptors.fs_descs.source,
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
                    .bLength = sizeof descriptors.hs_descs.intf,
                    .bDescriptorType = USB_DT_INTERFACE,
                    .bNumEndpoints = 2,
                    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
                    .bInterfaceSubClass = USB_SUBCLASS_VENDOR_SPEC,
                    .bInterfaceProtocol = 0x00,
                    .iInterface = 1,
                },
            .sink =
                {
                    .bLength = sizeof descriptors.hs_descs.sink,
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 1 | USB_DIR_IN,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = cpu_to_le16(512),
                    .bInterval = 0,
                },
            .source =
                {
                    .bLength = sizeof descriptors.hs_descs.source,
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = 2 | USB_DIR_OUT,
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
                    .wMaxPacketSize = cpu_to_le16(512),
                    .bInterval = 0,
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

void write_descriptors(int fd, int additionalFlags) {
  auto local_descriptors = descriptors;
  local_descriptors.header.flags |= cpu_to_le32(additionalFlags);
  checkError(write(fd, &local_descriptors, sizeof local_descriptors), {});
  checkError(write(fd, &strings, sizeof strings), {});
}
