/*
 *	General USB definitions
 *
 *	Definitions and (mostly) field names are due to USB 2.0 standard
 */
 
#ifndef	USB__H
 #define	USB__H

#include "sosdef.h"

#define	MAX_CONTROL_PACKET	64

struct usb_setup_data
{
#define	USB_REQTYPE_DIR_IN	0x80
	byte	bmRequestType;
	byte	bRequest;
	word	wValue;
	word	wIndex;
	word	wLength;
} __attribute__ ((packed));

// Standard USB request types (bRequest)
#define	USB_REQ_GET_STATUS	0
#define	USB_REQ_CLEAR_FEATURE	1
#define	USB_REQ_SET_FEATURE	3
#define	USB_REQ_SET_ADDRESS	5
#define	USB_REQ_GET_DESCRIPTOR	6
#define	USB_REQ_SET_DESCRIPTOR	7
#define	USB_REQ_GET_CONFIGURATION	8
#define	USB_REQ_SET_CONFIGURATION	9
#define	USB_REQ_GET_INTERFACE	10
#define	USB_REQ_SET_INTERFACE	11
#define	USB_REQ_SYNCH_FRAME	12

// Standard descriptor types
enum {USB_DESC_DEVICE = 1, USB_DESC_CONFIGURATION, USB_DESC_STRING, USB_DESC_INTERFACE, USB_DESC_ENDPOINT,
		USB_DESC_DEV_QUAL, USB_DESC_OTHER_SPEED_CONF, USB_DESC_INTERFACE_POWER};

// Standard device descriptor
struct usb_device_descr
{
	byte	bLength;
	byte	bDescriptorType;
	word	bcdUSB;
	byte	bDeviceClass;
	byte	bDeviceSubClass;
	byte	bDeviceProtocol;
	byte	bMaxPacketSize0;
	word	idVendor;
	word	idProduct;
	word	bcdDevice;
	byte	iManufacturer;
	byte	iProduct;
	byte	iSerialNumber;
	byte	bNumConfigurations;
} __attribute__ ((packed));
		
// Configuration descriptor
struct usb_conf_descr
{
	byte	bLength;
	byte	bDescriptorType;
	word	wTotalLength;
	byte	bNumInterfaces;
	byte	bConfigurationValue;
	byte	iConfiguration;
	byte	bmAttributes;
	byte	bMaxPower;
} __attribute__ ((packed));

// Interface descriptor
struct usb_iface_descr
{
	byte    bLength;
	byte    bDescriptorType;
	byte	bInterfaceNumber;
	byte	bAlternateSetting;
	byte	bNumEndpoints;
	byte	bInterfaceClass;
	byte	bInterfaceSubClass;
	byte	bInterfaceProtocol;
	byte	iInterface;
} __attribute__ ((packed));

// Endpoint descriptor
struct usb_ep_descr
{
	byte    bLength;
	byte    bDescriptorType;
	byte	bEndpointAddress;
	byte	bmAttributes;
	word	wMaxPacketSize;
	byte	bInterval;
} __attribute__ ((packed));


enum	{USB_ENUM_STATE_DETACHED, USB_ENUM_STATE_POWERED, USB_ENUM_STATE_ADDRESS, USB_ENUM_STATE_CONF, USB_ENUM_STATE_OPER};

struct usb_dev
{
	int	enum_state;
	int	port_num;
	int	addr;
	void	*ep0_priv;
};

struct usb_conf
{
};

struct usb_iface
{
};

struct usb_ep
{
};

#endif // USB__H

