/***************************************************
 *
 *	ehci.c
 *
 *	EHCI USB host driver
 *
 *	We will use design ideas sound in Linux EHCI driver: there is one QH per endpoint (control/bulk/interrupt).
 *	Requests to an endpoint are submitted via QTDs to the same QH. 'A' (active) bit in QTD serves as indication
 *	of whether the transfer is to be performed. QTDs are linked in a cyclic list. QHs are removed as endpoints
 *	are removed (device/function is disables/detached). It is possible that no QTDs are currently 'active'
 *	for a QH - this endpoint doesn't have a progressing/pending transfer.
 *
 *	Meanwhile we don't know what to do with reclamation lists
 *
 *	TBD: (1) whether the completed QTD is disbanded, freed and unlinked. Probably this should be due to a
 *	specific device driver.
 *
 *	Buffers allocation for a TD may use a single malloc()/calloc(), just splitting the contiguous buffer
 *	to separate page-aligned 4K pages (except for the original address which may be not aligned).
 *
 *	Submitting QTD may include async. enable bit set in USBCMD. TBD: Async queue will not be enabled when 
 *	there are no devices/endpoints attached? Obviously, initially this is the case, but isn't it better to
 *	have a sinlge QH with no active transfers (a single "dummy" QTD) in this case?
 *
 *	(?) A strange thing - once we enable doorbell interrupts, under VmWare we get an interrupt 
 *	"async pointer advanced" even when async queue is disabled. Is it right or just VmWare misbehavior?
 *
 ***************************************************/

#include	"sosdef.h"
#include	"drvint.h"
#include	"config.h"
#include	"pcihost.h"
#include	"errno.h"
#include	"ehci.h"

#define	DEBUG_IRQ	1
#define	DEBUG_INIT	1
#define	DEBUG_PCI_CONFIG	1

// Table to indetify EHCI controllers from different vendors
struct ehci_creds
{
	int	vendor_id;
	int	device_id;
} ehci_dev_tbl[] =
{
	{0x15AD, 0x0770}		// Vmware EHCI controller
};

static int	pci_dev_index = -1;

// static vars below will probably move to device-instance structure once we will need to support systems with multiple EHCI controllers
static struct	ehci_cap_regs	*ehci_cap_regs;
static struct	ehci_op_regs	*ehci_op_regs;
static int	num_ports;
static dword 	*periodic_list_real_base, *periodic_list_aligned_base;
static struct ehci_qh 	*async_list_head;
//static struct ehci_qh 	*async_list_tail;
///////////////////////////////

// USB devices array
#define	MAX_USB_DEVICES	127
struct usb_dev	usb_devices[MAX_USB_DEVICES];
int	num_usb_devices = 0;
///////////////////////////////

static	void	ehci_timer_handler(void *unused);
static	int	ehci_isr(void);

timer_t	ehci_timer = {20, 0, TICKS_PER_SEC, TF_PERIODIC, 0, ehci_timer_handler, NULL};

static	void	ehci_timer_handler(void *unused)
{
	dword	int_sts;
	static unsigned	timer_count;
	
//#ifdef	DEBUG_IRQ
//	printfxy(0, 4, "%s(): Received EHCI timer no. %u", __func__, ++timer_count);
//#endif

	int_sts = ehci_op_regs->usb_sts;
	if ((int_sts & EHCI_INTR_ENABLE) != 0)
	{
		serial_printf("%s(): EHCI HC interrupt - not received via PIC\r\n", __func__);
		ehci_isr();
	}
}


static int	ehci_reset_port(int port_num)
{
	// Reset the port, for 50 ms (due to spec). Now just delay, later we'll change that to a timer
	ehci_op_regs->port_sc[port_num] = ehci_op_regs->port_sc[port_num] | EHCI_PORTSC_PORT_EN /* clear */ | EHCI_PORTSC_PORT_RESET;
	udelay(50000);
	ehci_op_regs->port_sc[port_num] = ehci_op_regs->port_sc[port_num] & ~(EHCI_PORTSC_PORT_EN /* don't clear */ | EHCI_PORTSC_PORT_RESET);
	// Wait until port reset becomes 0
	while (EHCI_PORTSC_PORT_RESET & ehci_op_regs->port_sc[port_num])
		;
	udelay(2000);
	serial_printf("%s(): port %d is reset\n", __func__, port_num);
	return	0;
}


struct ehci_qh	*new_qh(void)
{
	struct ehci_qh	*qh;

	qh = calloc(1, sizeof(struct ehci_qh));
	if (qh == NULL)
		return	NULL;
	qh->ep_char = (64U << 16) | 0x2000;		// Maximum Packet Length = 64 bytes
							// H = 0 -- this is an active schedule
							// Access as a high-speed device
							// Endpoint Number = 0 (default pipe)
							// Device Address = 0 (default address)
	qh->ep_caps = 0x40000000;			// High-Bandwidth Pipe Multiplier = 1

	qh->qh_hlp = async_list_head->qh_hlp;
	async_list_head->qh_hlp = (dword)qh | 2;	// Link right after the head (dummy) QH -- due to Linux driver

	return	qh;
}

static int	ehci_free_qtd_list(struct ehci_qel_qtd *qtd)
{
	dword	qtd_val;
	struct ehci_qel_qtd	*next_qtd;
	int	i;

	while (!((qtd_val = (dword)qtd) & 1))
	{
		next_qtd = (struct ehci_qel_qtd*)qtd->next_qtd_ptr;
		for (i = 0; i < 5; ++i)
			if (qtd->qtd_buf_ptr[i] != 0)
				free((void*)qtd->qtd_buf_ptr[i]);
		free(qtd);
		qtd = next_qtd;
	}

	return	0;
}


// Common function that handles all setup requests
static int	ehci_issue_setup(struct ehci_qh *qh, struct usb_setup_data *setup_data, void *data_buf)
{
	struct ehci_qel_qtd	*q0 = NULL, *q1 = NULL, *q2 = NULL;

	// Prepare QTDs

	//-------------------------- Prepare SETUP transfer (0) ------------------------------------
	q0 = calloc(1, sizeof(struct ehci_qel_qtd));
	q0->qtd_tok = (sizeof(struct usb_setup_data) << 16) | 0x0200 | EHCI_QTD_STATUS_ACTIVE;	// Size, !IOC, PAGE = 0, PID = SETUP, ACTIVE = 1
	q0->qtd_buf_ptr[0] = (dword)setup_data;
	
	if (setup_data->wLength != 0)
	{
		//-------------------------- Allocate and fill IN packet for ep0 (transfer 1) --------------
		q1 = calloc(1, sizeof(struct ehci_qel_qtd));		// QTD needs to be 32-byte aligned!
									// (currently defined by allocation alignment in `memman.h')
		q1->qtd_tok = (MAX_CONTROL_PACKET << 16) | (setup_data->bmRequestType & USB_REQTYPE_DIR_IN) << 1 | EHCI_QTD_STATUS_ACTIVE;	// Size, !IOC, PAGE = 0, PID = IN/OUT depending on request type, ACTIVE = 1
		q1->qtd_buf_ptr[0] = (dword)data_buf;
		// Link second (IN) transfer element
		q0->next_qtd_ptr = (dword)q1;				// 'T' bit is NOT set
		q0->alt_next_qtd_ptr = (dword)q1;			// 'T' bit is NOT set
	
		//-------------------------- Allocate and fill OUT packet for ACK (0 length - transfer 2) -----
		q2 = calloc(1, sizeof(struct ehci_qel_qtd));		// QTD needs to be 32-byte aligned!
		q2->next_qtd_ptr = 0x1;					// 'T' bit is set - last transfer in a QH
		q2->alt_next_qtd_ptr = 0x1;				// 'T' bit is set - last transfer in a QH
		q2->qtd_tok = 0x8000 | ((setup_data->bmRequestType & USB_REQTYPE_DIR_IN ^ USB_REQTYPE_DIR_IN) << 1) | EHCI_QTD_STATUS_ACTIVE;		// Size = 0, IOC, PAGE = 0, PID = IN/OUT depending (opposite) on request type, ACTIVE = 1
	
		// Buffer pointer is 0, but we don't care - it's a 0-length transfer
		// Link third (OUT) transfer element
		q1->next_qtd_ptr = (dword)q2;				// 'T' bit is NOT set
		q1->alt_next_qtd_ptr = (dword)q2;			// 'T' bit is NOT set
	}
	else
	{
		// 0-length data stage we set always IN
		q1 = calloc(1, sizeof(struct ehci_qel_qtd));            // QTD needs to be 32-byte aligned!
		q1->next_qtd_ptr = 0x1;                                 // 'T' bit is set - last transfer in a QH
		q1->alt_next_qtd_ptr = 0x1;                             // 'T' bit is set - last transfer in a QH
		q1->qtd_tok = 0x8100 | EHCI_QTD_STATUS_ACTIVE;          // Size = 0, IOC, PAGE = 0, PID = IN, ACTIVE = 1
		// Link second (IN) transfer element
		q0->next_qtd_ptr = (dword)q1;				// 'T' bit is NOT set
		q0->alt_next_qtd_ptr = (dword)q1;			// 'T' bit is NOT set
	}

	//-------------------------- Prepare QH --------------------------------------------------------
	qh->qtd.next_qtd_ptr = (dword)q0;
	qh->first_qtd = q0;
	//-------------------------- Start async request -----------------------------------------------
	serial_printf("%s(): qh = 0x%08X q0=%08X q1=%08X q2=%08X\n", __func__, qh, q0, q1, q2);
	serial_printf("%s(): Prepared QH to run: qh_hlp = 0x%08X, ep_char = 0x%08X, ep_caps = 0x%08X, curr_qtd_ptr = 0x%08X, next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n",
		__func__, qh->qh_hlp, qh->ep_char, qh->ep_caps, qh->curr_qtd_ptr, qh->qtd.next_qtd_ptr,
		qh->qtd.alt_next_qtd_ptr, qh->qtd.qtd_tok, qh->qtd.qtd_buf_ptr[0]);
	
	return	0;
}


// We add 'size' parameter because during enumeration we may need to read this request as 8 bytes
static int	ehci_get_dev_descr(struct ehci_qh *qh, int sz)
{
	struct usb_setup_data	*setup_data;
	void	*buf;

	//memset(&qh->qtd, 0, sizeof(qh->qtd));

	if (sz > sizeof(struct usb_device_descr))
		sz = sizeof(struct usb_device_descr);

	// Allocate data buffer for SETUP packet
	buf = calloc(1, sizeof(struct usb_setup_data));		// First pointer needs not be page-aligned!
	setup_data = (struct usb_setup_data*)buf;
				
	// Fill SETUP packet for ep0 - get device descriptor
	setup_data->bmRequestType = USB_REQTYPE_DIR_IN;		// Direction: device-to-host
								// Type: standard, recipient: device
	setup_data->bRequest = USB_REQ_GET_DESCRIPTOR;
	setup_data->wValue = (USB_DESC_DEVICE << 8);	// Descriptor type = device, descriptor index = 0
	setup_data->wIndex = 0;
	setup_data->wLength = sz;
	
	// Allocate buffer for received device descriptor
	buf = calloc(1, MAX_CONTROL_PACKET);
	return	ehci_issue_setup(qh, setup_data, buf);
}


static int	ehci_get_conf_descr(struct ehci_qh *qh)
{
	struct usb_setup_data	*setup_data;
	struct ehci_qel_qtd	*q0, *q, *q2;
	struct usb_conf_descr	*usb_conf_descr;
	void	*buf;

	//memset(&qh->qtd, 0, sizeof(qh->qtd));

	// Allocate data buffer for SETUP packet
	buf = calloc(1, sizeof(struct usb_setup_data));		// First pointer needs not be page-aligned!
	setup_data = (struct usb_setup_data*)buf;
				
	// Fill SETUP packet for ep0 - get device descriptor
	setup_data->bmRequestType = USB_REQTYPE_DIR_IN;		// Direction: device-to-host
								// Type: standard, recipient: device
	setup_data->bRequest = USB_REQ_GET_DESCRIPTOR;
	setup_data->wValue = (USB_DESC_CONFIGURATION << 8);	// Descriptor type = device, descriptor index = 0
	setup_data->wIndex = 0;
	setup_data->wLength = MAX_CONTROL_PACKET /*sizeof(struct usb_conf_descr)*/;
	
	// Allocated buffer for received device descriptor
	buf = calloc(1, MAX_CONTROL_PACKET);
	return	ehci_issue_setup(qh, setup_data, buf);
}


static int	ehci_set_address(struct ehci_qh *qh, int address)
{
	struct usb_setup_data   *setup_data;
	struct ehci_qel_qtd	*q0, *q2;
	unsigned char	*buf;

	// Allocate data buffer for SETUP packet
	buf = calloc(1, sizeof(struct usb_setup_data));		// First pointer needs not be page-aligned!
	setup_data = (struct usb_setup_data*)buf;
				
	// Fill SETUP packet for ep0 - get device descriptor
	setup_data->bmRequestType = 0x0;			// Direction: host-to-device
								// Type: standard, recipient: device
	setup_data->bRequest = USB_REQ_SET_ADDRESS;
	setup_data->wValue = address;
	setup_data->wIndex = 0;
	setup_data->wLength = 0;
	
	return	ehci_issue_setup(qh, setup_data, NULL);

#if 0
	// Prepare QTDs

	//-------------------------- Prepare SETUP transfer (0) ------------------------------------
	q0 = calloc(1, sizeof(struct ehci_qel_qtd));
	q0->qtd_tok = (sizeof(struct usb_setup_data) << 16) | 0x0200 | EHCI_QTD_STATUS_ACTIVE;	// Size, !IOC, PAGE = 0, PID = SETUP, ACTIVE = 1
	q0->qtd_buf_ptr[0] = (dword)buf;
	q0->next_qtd_ptr = 0x1;
	q0->alt_next_qtd_ptr = 0x1;

	//-------------------------- Allocate and fill IN packet for ACK (0 length - transfer 2) -----
	q2 = calloc(1, sizeof(struct ehci_qel_qtd));		// QTD needs to be 32-byte aligned!
	q2->next_qtd_ptr = 0x1;					// 'T' bit is set - last transfer in a QH
	q2->alt_next_qtd_ptr = 0x1;				// 'T' bit is set - last transfer in a QH
	q2->qtd_tok = 0x8100 | EHCI_QTD_STATUS_ACTIVE;		// Size = 0, IOC, PAGE = 0, PID = IN, ACTIVE = 1
	
	// Buffer pointer is 0, but we don't care - it's a 0-length transfer
	// Link second (IN) transfer element
	q0->next_qtd_ptr = (dword)q2;						// 'T' bit is NOT set
	q0->alt_next_qtd_ptr = (dword)q2;					// 'T' bit is NOT set

	//-------------------------- Prepare QH --------------------------------------------------------
	qh->qtd.next_qtd_ptr = (dword)q0;
	qh->first_qtd = q0;
	//----------------------------------------------------------------------------------------------

	return	0;
#endif
}


static int	ehci_complete_dev_descr(struct ehci_qh *qh)
{
	struct ehci_qel_qtd	*q0, *q, *q2;
	struct usb_device_descr	*usb_dev_descr;

	serial_printf("%s(): Next QH to run: qh_hlp = 0x%08X, ep_char = 0x%08X, ep_caps = 0x%08X, curr_qtd_ptr = 0x%08X, next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n",
		__func__, qh->qh_hlp, qh->ep_char, qh->ep_caps, qh->curr_qtd_ptr, qh->qtd.next_qtd_ptr,
		qh->qtd.alt_next_qtd_ptr, qh->qtd.qtd_tok, qh->qtd.qtd_buf_ptr[0]);
	q0 = qh->first_qtd;
	serial_printf("%s(): q0=%08X -- next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n", __func__, q0, q0->next_qtd_ptr, q0->alt_next_qtd_ptr, q0->qtd_tok, q0->qtd_buf_ptr[0]);
	q = (void*)q0->next_qtd_ptr;
	usb_dev_descr = (struct usb_device_descr*)q->qtd_buf_ptr[0];

	serial_printf("%s(): --------------- dumping received device descriptor ------------------\n", __func__);
	serial_printf("bLength=%02X bDescriptorType=%02X bcdUSB=%04X bDeviceClass=%02X bDeviceSubClass=%02X bDeviceProtocol=%02X\n"
		"bMaxPacketSize0=%02X idVendor=%04X idProduct=%04X bcdDevice=%04X iManufacturer=%02X iProduct=%02X\n"
		"iSerialNumber=%02X bNumConfigurations=%02X\n",
		usb_dev_descr->bLength, usb_dev_descr->bDescriptorType, usb_dev_descr->bcdUSB, usb_dev_descr->bDeviceClass,
		usb_dev_descr->bDeviceSubClass, usb_dev_descr->bDeviceProtocol, usb_dev_descr->bMaxPacketSize0, usb_dev_descr->idVendor,
		usb_dev_descr->idProduct, usb_dev_descr->bcdDevice, usb_dev_descr->iManufacturer, usb_dev_descr->iProduct,
		usb_dev_descr->iSerialNumber, usb_dev_descr->bNumConfigurations);
	serial_printf("\n"
			"%s(): ---------------------------------------------------------------------\n", __func__);

	serial_printf("%s(): q=%08X -- next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n", __func__, q, q->next_qtd_ptr, q->alt_next_qtd_ptr, q->qtd_tok, q->qtd_buf_ptr[0]);
	q2 = (void*)q->next_qtd_ptr;
	serial_printf("%s(): q2=%08X -- next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n", __func__, q2, q2->next_qtd_ptr, q2->alt_next_qtd_ptr, q2->qtd_tok, q2->qtd_buf_ptr[0]);
	q = (void*)&qh->qtd;
	serial_printf("%s(): Transfer Overlay: curr_qtd_ptr=%08X, q=%08X -- next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n", __func__, qh->curr_qtd_ptr, q, q->next_qtd_ptr, q->alt_next_qtd_ptr, q->qtd_tok, q->qtd_buf_ptr[0]);
	//ehci_free_qtd_list(q0);

	if (qh->usb_dev->enum_state == USB_ENUM_STATE_DETACHED)
		qh->usb_dev->enum_state = USB_ENUM_STATE_POWERED;
	else
		qh->usb_dev->enum_state = USB_ENUM_STATE_CONF;
	return	0;
}


static int	ehci_complete_conf_descr(struct ehci_qh *qh)
{
	struct ehci_qel_qtd	*q0, *q, *q2;
	struct usb_conf_descr	*usb_conf_descr;
	struct usb_iface_descr	*usb_iface_descr; 
	struct usb_ep_descr	*usb_ep_descr;
	unsigned char	*buf;

	int	i, j;
	unsigned	n;

	serial_printf("%s(): Next QH to run: qh_hlp = 0x%08X, ep_char = 0x%08X, ep_caps = 0x%08X, curr_qtd_ptr = 0x%08X, next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n",
		__func__, qh->qh_hlp, qh->ep_char, qh->ep_caps, qh->curr_qtd_ptr, qh->qtd.next_qtd_ptr,
		qh->qtd.alt_next_qtd_ptr, qh->qtd.qtd_tok, qh->qtd.qtd_buf_ptr[0]);
	q0 = qh->first_qtd;
	serial_printf("%s(): q0=%08X -- next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n", __func__, q0, q0->next_qtd_ptr, q0->alt_next_qtd_ptr, q0->qtd_tok, q0->qtd_buf_ptr[0]);
	q = (void*)q0->next_qtd_ptr;
	usb_conf_descr = (struct usb_conf_descr*)q->qtd_buf_ptr[0];
	serial_printf("%s(): --------------- dumping received configuration descriptor ------------------\n", __func__);

	buf = (unsigned char*)q->qtd_buf_ptr[0];
	for (i = 0; i < MAX_CONTROL_PACKET; ++i)
	{
		serial_printf("%02X ", (buf)[i]);
		if (i % 16 == 15)
			serial_printf("\n");
	}

	serial_printf("Configuration descriptor: bLength=%02X bDescriptorType=%02X wTotalLength=%04X bNumInterfaces=%02X bConfigurationValue=%02X\n"
			"iConfiguration=%02X bmAttributes=%02X bMaxPower=%02X\n", usb_conf_descr->bLength,
			usb_conf_descr->bDescriptorType, usb_conf_descr->wTotalLength, usb_conf_descr->bNumInterfaces,
			usb_conf_descr->bConfigurationValue, usb_conf_descr->iConfiguration, usb_conf_descr->bmAttributes,
			usb_conf_descr->bMaxPower);
	n = usb_conf_descr->bLength;
	for (i = 0; i < usb_conf_descr->bNumInterfaces; ++i)
	{
		usb_iface_descr = (struct usb_iface_descr*)(q->qtd_buf_ptr[0] + n);
		serial_printf("|\n"
				"--->Interface descriptor: bLength=%02X bDescriptorType=%02X bInterfaceNumber=%02X bAlternateSetting=%02X bNumEndpoints=%02X bInterfaceClass=%02X bInterfaceSubClass=%02X bInterfaceProtocol=%02X iInterface=%02X\n",
				usb_iface_descr->bLength, usb_iface_descr->bDescriptorType, usb_iface_descr->bInterfaceNumber, usb_iface_descr->bAlternateSetting, usb_iface_descr->bNumEndpoints, usb_iface_descr->bInterfaceClass, usb_iface_descr->bInterfaceSubClass, usb_iface_descr->bInterfaceProtocol, usb_iface_descr->iInterface);
		n += usb_iface_descr->bLength;
		for (j = 0; j < usb_iface_descr->bNumEndpoints; ++j)
		{
			usb_ep_descr = (struct usb_ep_descr*)(q->qtd_buf_ptr[0] + n);
			serial_printf("   |\n"
					"   --->Endpoint descriptor: bLength=%02X bDescriptorType=%02X bEndpointAddress=%02X bmAttributes=%02X wMaxPacketSize=%04X bInterval=%02X\n", usb_ep_descr->bLength, usb_ep_descr->bDescriptorType, usb_ep_descr->bEndpointAddress, usb_ep_descr->bmAttributes, usb_ep_descr->wMaxPacketSize, usb_ep_descr->bInterval); 
			n += usb_ep_descr->bLength;
		}
	}
	serial_printf("%s(): n=%u, wTotalLength=%u\n", __func__, n, (unsigned)usb_conf_descr->wTotalLength);

	serial_printf("\n"
			"%s(): ---------------------------------------------------------------------\n", __func__);
	serial_printf("%s(): q=%08X -- next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n", __func__, q, q->next_qtd_ptr, q->alt_next_qtd_ptr, q->qtd_tok, q->qtd_buf_ptr[0]);
	q2 = (void*)q->next_qtd_ptr;
	serial_printf("%s(): q2=%08X -- next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n", __func__, q2, q2->next_qtd_ptr, q2->alt_next_qtd_ptr, q2->qtd_tok, q2->qtd_buf_ptr[0]);
	q = (void*)&qh->qtd;
	serial_printf("%s(): Transfer Overlay: curr_qtd_ptr=%08X, q=%08X -- next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n", __func__, qh->curr_qtd_ptr, q, q->next_qtd_ptr, q->alt_next_qtd_ptr, q->qtd_tok, q->qtd_buf_ptr[0]);
	//ehci_free_qtd_list(q0);

	qh->usb_dev->enum_state = USB_ENUM_STATE_OPER;
	return	0;
}


static int	ehci_complete_transactions(void)
{
	struct ehci_qh	*qh, *next_qh;
	struct ehci_qel_qtd	*q0, *q;
	struct usb_device_descr	*usb_dev_descr;
	int	get_conf = 0;
	int	set_addr = 0;
	int	get_dev2 = 0;
	struct usb_setup_data   *setup_data;

	for (qh = async_list_head; ; qh = next_qh)
	{
serial_printf("%s(): qh=%08X first_qtd=%08X, next_qtd_ptr=%08X comparison result=%d\n", __func__, qh, qh->first_qtd, qh->qtd.next_qtd_ptr,
		((struct ehci_qel_qtd*)(qh->qtd.next_qtd_ptr & ~0x1F) != qh->first_qtd));
		if (qh->first_qtd != NULL && (struct ehci_qel_qtd*)(qh->qtd.next_qtd_ptr & ~0x1F) != qh->first_qtd)
		{
			//ehci_complete_transaction(qh);
			//Below
			q0 = qh->first_qtd;
			serial_printf("%s(): q0=%08X -- next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\n", __func__, q0, q0->next_qtd_ptr, q0->alt_next_qtd_ptr, q0->qtd_tok, q0->qtd_buf_ptr[0]);

			if ((q0->qtd_tok & 0x300) != 0x200)
			{
				// Completing non-setup transaction
				serial_printf("%s(): completing non-setup transaction\n", __func__); 
				continue;
			}

			// Completing setup transaction
			setup_data = (struct usb_setup_data*)q0->qtd_buf_ptr[0];

			serial_printf("%s(): completing setup transaction bmRequestType=%02X bRequest=%02X wValue=%04X wIndex=%04X wLength=%04X\n", __func__,
				setup_data->bmRequestType, setup_data->bRequest, setup_data->wValue, setup_data->wIndex, setup_data->wLength); 

			if (setup_data->bRequest == USB_REQ_SET_ADDRESS)
			{
				// Completing set address request
				udelay(800000);
				qh->ep_char &= ~0x7F;
				qh->ep_char |= setup_data->wValue & 0x7F;
				serial_printf("%s(): set address completed. QH=%08X is assigned to device address %d\n", __func__, qh, setup_data->wValue & 0x7F);
				qh->usb_dev->enum_state = USB_ENUM_STATE_ADDRESS;
				//get_dev2 = 1;
				get_conf = 1;
			}
			else if (setup_data->bRequest == USB_REQ_GET_DESCRIPTOR)
			{
				serial_printf("%s(): completing get descriptor request\n", __func__);

				// Completing get descriptor request
				q = (void*)q0->next_qtd_ptr;
				usb_dev_descr = (struct usb_device_descr*)q->qtd_buf_ptr[0];

				serial_printf("%s(): q=%08X, usb_dev_descr=%08X bDescriptorType=%02X\n", __func__, q, usb_dev_descr, usb_dev_descr->bDescriptorType);
				serial_printf("%s(): q=%08X -- next_qtd_ptr = 0x%08X, alt_next_qtd_ptr = 0x%08X, qtd_tok = 0x%08X, qtd_buf_ptr[0] = 0x%08X\r\n", __func__, q, q->next_qtd_ptr, q->alt_next_qtd_ptr, q->qtd_tok, q->qtd_buf_ptr[0]);

				switch (usb_dev_descr->bDescriptorType)
				{
				default:
					break;
	
				case USB_DESC_DEVICE:
					ehci_complete_dev_descr(qh);
					serial_printf("%s(): device descriptor completed\n", __func__);
					if (qh->usb_dev->enum_state == USB_ENUM_STATE_POWERED)
						set_addr = 1;
					else
						get_conf = 1;
					break;
	
				case USB_DESC_CONFIGURATION:
					ehci_complete_conf_descr(qh);
					serial_printf("%s(): configuration descriptor completed\n", __func__);
					break;
				}
			}
			else
			{
				// Completing other requests
			}
	
			// Remove all QTDs between first_qtd and first qtd that has IOC set
			for (q = qh->first_qtd; q != NULL; q = (struct ehci_qel_qtd*)(q->next_qtd_ptr & ~0x1F))
			{
				if (q->qtd_tok & 0x8000)
				{
					q0 = q;
					q = (struct ehci_qel_qtd*)(q->next_qtd_ptr & ~0x1F);
					q0->next_qtd_ptr = 0x1;
					break;
				}
			}
	
			ehci_free_qtd_list(qh->first_qtd);
			qh->first_qtd = q;
			serial_printf("%s(): QTD list freed\n", __func__);
		}

		next_qh = (struct ehci_qh*)(qh->qh_hlp & ~0x1F);
		if (next_qh == async_list_head)
			break;
	}

	if (get_conf)
	{
		serial_printf("%s(): Calling get configuration descriptor\n", __func__);
		ehci_get_conf_descr(qh);
	}
	if (set_addr)
	{
		ehci_reset_port(qh->usb_dev->port_num);
		serial_printf("%s(): Calling set address\n", __func__);
		ehci_set_address(qh, qh->usb_dev->addr);
	}
	if (get_dev2)
	{
		serial_printf("%s(): Calling get device descriptor (8 bytes)\n", __func__);
		ehci_get_dev_descr(qh, 8);
	}

	return	0;
}


static int	ehci_new_device(int port_num)
{
	struct ehci_qh	*qh;

	usb_devices[num_usb_devices].enum_state = USB_ENUM_STATE_DETACHED;
	usb_devices[num_usb_devices].port_num = port_num;
	usb_devices[num_usb_devices].addr = num_usb_devices + 1;

	serial_printf("%s(): async_list_addr=0x%08X \r\n", __func__, ehci_op_regs->async_list_addr);
	qh = new_qh();
	qh->usb_dev = &usb_devices[num_usb_devices];
	usb_devices[num_usb_devices].ep0_priv = qh;
	++num_usb_devices;
	ehci_get_dev_descr(qh, sizeof(struct usb_device_descr));

	return	0;
}


/*
 *	EHCI IRQ handler.
 */
static int	ehci_isr(void)
{
	int	i;
	static unsigned	irq_count;
	dword	int_sts;

#ifdef	DEBUG_IRQ
	serial_printf("%s(): Received EHCI HC IRQ no. %u\r\n", __func__, ++irq_count);
#endif

	int_sts = ehci_op_regs->usb_sts;

	// Acknowledge all interrupts
	// For some reason they want to do it before processing?
	ehci_op_regs->usb_sts = EHCI_INTR_ENABLE;
	
	if ((int_sts & EHCI_INTR_ENABLE) != 0)
	{
		serial_printf("%s(): Received EHCI HC IRQ: int_sts=0x%08X\r\n", __func__, int_sts);
		
		// Handle all interrupts
		// ...
		
		// Port change
		if ((int_sts & EHCI_INTR_PORT_CHANGE) != 0)
		{
			serial_printf("Port change: ");
			for (i = 0; i < num_ports; ++i)
			{
				if (EHCI_PORTSC_CONN_CHANGE & ehci_op_regs->port_sc[i])
				{
					unsigned	line_status;
					unsigned	port_enable;

					serial_printf("%s(): connect status change on port %d (%08X): device is %s\n", __func__, i, 
					(unsigned)ehci_op_regs->port_sc[i], (EHCI_PORTSC_CONNECTED & ehci_op_regs->port_sc[i]) ? "connected" : "disconnected");
					if (EHCI_PORTSC_CONNECTED & ehci_op_regs->port_sc[i])
					{
						if (!(EHCI_PORTSC_PORT_POWER & ehci_op_regs->port_sc[i]))
						{
							serial_printf("%s(): Weird: connection state is changed but port power is 0\n", __func__);
							continue;
						}
						if (EHCI_PORTSC_PORT_EN & ehci_op_regs->port_sc[i])
						{
							serial_printf("%s(): Port enabled bit is 1, line state is not valid\n", __func__);
							continue;
						}
						line_status = ehci_op_regs->port_sc[i] >> 10 & 0x3;
						serial_printf("%s(): Line status is valid, value=%02X, device is %s\n", __func__, line_status,
							0x1 == line_status ? "low-speed, release port" : "full/high speed, reset port to determine");

						ehci_reset_port(i);
						// Now read port enable status
						port_enable = ((EHCI_PORTSC_PORT_EN & ehci_op_regs->port_sc[i]) != 0);
						serial_printf("%s(): after reset port status is %08X (port_enable=%u): device is %s speed\n", __func__, ehci_op_regs->port_sc[i], port_enable, port_enable ? "high" : "full");
						ehci_new_device(i);
					}

					// Handle one port change per IRQ (for now)
					//if (!port_enable)
					//	return	0;
					break;
				}
			} // for()

#if 0
			else
			{
				if ((((struct ehci_qh*)(ehci_op_regs->async_list_addr & 0xFFFFFFE0))->qtd.qtd_tok & EHCI_QTD_STATUS_ACTIVE) == 0)
				{
					// Link QH in at `async_list_addr'
					serial_printf("%s(): Putting QH in front of everything\r\n", __func__);
					qh->qh_hlp = (ehci_op_regs->async_list_addr & 0xFFFFFFE0) | 0x2;
					ehci_op_regs->async_list_addr = (dword)qh;
					async_list_tail->qh_hlp = (dword)qh & ~6 | 0x2;
				}
				else
				{
					// Link QH right before the first QH with 'active' bit clear
					serial_printf("%s(): Putting QH in front of everything inactive\r\n", __func__);
					for (next_qh = (struct ehci_qh*)(ehci_op_regs->async_list_addr & 0xFFFFFFE0), prev_qh = async_list_tail; 
						next_qh != async_list_tail;
						next_qh = (struct ehci_qh*)(next_qh ->qh_hlp & 0xFFFFFFE0))
					{
						if ((next_qh->qtd.qtd_tok & EHCI_QTD_STATUS_ACTIVE) == 0)
						{
							qh->qh_hlp = (dword)next_qh & 0xFFFFFFE0 | 0x2;
							prev_qh->qh_hlp = (dword)qh & 0xFFFFFFE0 | 0x2;
							break;
						}
					}
					if (next_qh == async_list_tail)
					{
						// All QH's are active, link as last (tail)
						serial_printf("%s(): Putting QH at tail\r\n", __func__);
						qh->qh_hlp = (ehci_op_regs->async_list_addr & 0xFFFFFFE0) | 0x2;
						async_list_tail->qh_hlp = (dword)qh & 0xFFFFFFE0 | 0x2;
						async_list_tail = qh;
					}
				}
			}
#endif
		}
		if ((int_sts & EHCI_USB_INTR) != 0)
		{
			serial_printf("%s(): USB interrupt! async_list_addr=%08X\n", __func__, ehci_op_regs->async_list_addr);
			ehci_complete_transactions();
			serial_printf("%s(): completions completed\n", __func__);
		}
		if ((int_sts & EHCI_INTR_USB_ERR) != 0)
		{
			serial_printf("%s(): USB error!\r\n", __func__);
		}
		if ((int_sts & EHCI_INTR_FRLIST_ROLLOVER) != 0)
		{
			serial_printf("%s(): USB frame list rollover!\r\n", __func__);
		}
		if ((int_sts & EHCI_INTR_HOST_SYS_ERR) != 0)
		{
			serial_printf("%s(): USB host system error!\r\n", __func__);
		}
		if ((int_sts & EHCI_INTR_ASYNC_ADVANCE) != 0)
		{
			if ((ehci_op_regs->usb_cmd & EHCI_CMD_ASYNC_EN) == 0)
			{
				serial_printf("%s():  USB asynchronous QH advanced, but async queue is not enabled. Spurious interrupt?\n", __func__);
				return	0;
			}
			serial_printf("%s(): USB asynchronous QH advanced! async_list_addr=0x%08X\r\n", __func__, ehci_op_regs->async_list_addr);

			// If "async queue advanced" interrupt arrived, EHCI controller will zero doorbell enable interrupt bit, so we must
			// set it again in order to receive further such interrupts (EHCI spec table 2.9 and 4.8.2)
			//if (int_sts & EHCI_USB_INTR)
			//	ehci_op_regs->usb_cmd |= EHCI_CMD_ASYNC_ADV_DOORBELL;	
		}
	}

	return	0;		/* Allow shared interrupts */
}

/*
 *	Initialize the EHCI host controller
 */
int	ehci_init(unsigned drv_id)
{
	dword	value = -1;
	word	eeprom_val;
	int	i;
	int	int_line;
	int	vendor_id, dev_id;
	struct ehci_qel_qtd	*qtd;
	volatile dword	temp;

	/* Open PCI host driver - no need */
	
	/* Find EHCI device */
	for (i = 0; i < sizeof(ehci_dev_tbl) / sizeof(ehci_dev_tbl[0]); ++i)
	{
		ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_FIND_DEV, ehci_dev_tbl[i].vendor_id, ehci_dev_tbl[i].device_id, &pci_dev_index);
		if (pci_dev_index != -1)
			break;
	}
	if (pci_dev_index != -1)
		serial_printf("Found EHCI HC device at index %d\n", pci_dev_index);
	else
	{
		serial_printf("EHCI HC device was not found\n");
		return	ENODEV;
	}

	/* Device found */
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_COMMAND_OFFS, &value);
	serial_printf("Writing Command with MASTER = 1\n");
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_WRITEW, pci_dev_index, PCI_CFG_COMMAND_OFFS, PCMCMD_MASTER | value);

	/* Print its current configuration (by BIOS) */
#ifdef	DEBUG_PCI_CONFIG
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_VENDORID_OFFS, &value);
	serial_printf("VendorID = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_DEVICEID_OFFS, &value);
	serial_printf("DeviceID = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_COMMAND_OFFS, &value);
	serial_printf("Command = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_STATUS_OFFS, &value);
	serial_printf("Status = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_REVID_OFFS, &value);
	serial_printf("Revision ID = %02X, class code= %08X ", value & 0xFF, value & ~0xFF);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_CACHELINE_OFFS, &value);
	serial_printf("Cache line size = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFCG_LATTIMER_OFFS, &value);
	serial_printf("Latency timer = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_HDRTYPE_OFFS, &value);
	serial_printf("Header type = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_BIST_OFFS, &value);
	serial_printf("BIST = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR0_OFFS, &value);
	serial_printf("BAR0 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR1_OFFS, &value);
	serial_printf("BAR1 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR2_OFFS, &value);
	serial_printf("BAR2 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR3_OFFS, &value);
	serial_printf("BAR3 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR4_OFFS, &value);
	serial_printf("BAR4 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR5_OFFS, &value);
	serial_printf("BAR5 = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_SUBVENDOR_OFFS, &value);
	serial_printf("Subsystem VendorID = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READW, pci_dev_index, PCI_CFG_SUBSYS_OFFS, &value);
	serial_printf("Subsystem DeviceID = %04X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_EXPROMBASE_OFFS, &value);
	serial_printf("Expansion ROM base = %08X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_CAPSPTR_OFFS, &value);
	serial_printf("PCaps = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_INTLINE_OFFS, &value);
	serial_printf("Int line = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_INTPIN_OFFS, &value);
	serial_printf("Int pin = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_MINGRANT_OFFS, &value);
	serial_printf("Min. grant = %02X ", value);
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_MAXLAT_OFFS, &value);
	serial_printf("Max. latency = %02X ", value);
	serial_printf("\r\n");
#endif

	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READD, pci_dev_index, PCI_CFG_BAR0_OFFS, &value);
	ehci_cap_regs = (struct ehci_cap_regs*)(value & ~0xF);
	ehci_op_regs = (struct ehci_op_regs*)((value & ~0xF) + ehci_cap_regs->cap_length);

	/* Setup IRQ */
	ioctl_drv(DEV_ID(PCIHOST_DEV_ID, 0), PCI_IOCTL_CFG_READB, pci_dev_index, PCI_CFG_INTLINE_OFFS, &int_line);
	serial_printf("The EHCI HC uses IRQ line %d\r\n", int_line);
	set_int_callback(int_line, ehci_isr);
	
	// Install timer "interrupt" pollong callback - interrupts seem not to work
	//install_timer(&ehci_timer);

	// Enable interrupts
	ehci_op_regs->usb_intr = EHCI_INTR_ENABLE;
	
	// Set up periodic list and initialize periodic list base register
	periodic_list_real_base = calloc(1, 0x2000);		// Allocate 8K in order to align on 4K-boundary
	periodic_list_aligned_base = (dword*)(((unsigned)periodic_list_real_base & 0xFFFFF000) + 0x1000);
	for (i = 0; i < 0x1000 / sizeof(dword); ++i)
		periodic_list_aligned_base[i] = 0x1;			// Set 'T' bit
	ehci_op_regs->periodic_list_base = (dword)periodic_list_aligned_base;
	
#ifdef	DEBUG_INIT
	serial_printf("%s(): periodic_list_aligned_base = 0x%08X \r\n", __func__, periodic_list_aligned_base);
#endif
	

	// Allocate async list head
	async_list_head = calloc(1, sizeof(struct ehci_qh));
	//async_list_tail = async_list_head;

#if 0
	// Allocate "dummy" QTD for async_list_head (due to Linux driver)
	qtd = calloc(1, sizeof(struct ehci_qel_qtd));
	qtd->qtd_tok = EHCI_QTD_STATUS_HALTED;
	qtd->next_qtd_ptr = 0x1;                                                 // 'T' bit is set - last transfer in a QH
	qtd->alt_next_qtd_ptr = 0x1;                                             // 'T' bit is set - last transfer in a QH
#endif

	// Point to self. Fields below are filled due to Linux driver
	async_list_head->qh_hlp = (dword)async_list_head | 0x2;			// TYP = QH, point to self
	async_list_head->ep_char = 0x8000;					// head of reclamation list (due to Linux driver)
	async_list_head->qtd.qtd_tok = EHCI_QTD_STATUS_HALTED;
	async_list_head->qtd.next_qtd_ptr = 0x1;				// Last ('T' = 1)
	async_list_head->qtd.alt_next_qtd_ptr = 0x1;				// Last ('T' = 1)	
	async_list_head->first_qtd = NULL;
#if 0
	async_list_head->qtd.alt_next_qtd_ptr = qtd;
#endif

	ehci_op_regs->async_list_addr = (dword)async_list_head;			// Async list is still disabled
	serial_printf(" %s(): async_list_head=%08X\n", __func__, async_list_head);

	// Set up interrupt threshold and turn HC "on" via USBCMD
	// Since we initialized "dummy" QH we may enable async schedule
	ehci_op_regs->usb_cmd = EHCI_CMD_START_VAL;
	
	// Set "configured" bit - route all ports to this host controller)
	ehci_op_regs->config_flag = 1;
	
	// Linux driver says: "unblock posted writes"
	temp = ehci_op_regs->usb_cmd;

#ifdef	DEBUG_PCI_CONFIG	
	serial_printf("%s(): ehci_cap_regs=%08X ehci_op_regs=%08X\r\n", __func__, ehci_cap_regs, ehci_op_regs);
#endif

	num_ports = ehci_cap_regs->hcs_params & 0xF;
	
#ifdef	DEBUG_INIT
	serial_printf("%s(): EHCI CAPS: CAPLENGTH=0x%02X HCIVERSION=0x%04hX HCSPARAMS=0x%08X HCCPARAMS=0x%08X\r\n",
		__func__, (unsigned)ehci_cap_regs->cap_length, ehci_cap_regs->hci_version, ehci_cap_regs->hcs_params,
			ehci_cap_regs->hcc_params);
	serial_printf("%s(): EHCI OPS: USBCMD=0x%08X USBSTS=0x%08X USBINTR=0x%08X FRINDEX=0x%08X CTRLDSSEGMENT=0x%08X PERIODICLISTBASE=0x%08X ASYNCLISTADDR=0x%08X CONFIGFLAG=0x%08X\r\n",
		__func__, ehci_op_regs->usb_cmd, ehci_op_regs->usb_sts, ehci_op_regs->usb_intr, ehci_op_regs->fr_index,
			ehci_op_regs->ctrl_ds_segment, ehci_op_regs->periodic_list_base, ehci_op_regs->async_list_addr,
			ehci_op_regs->config_flag);
	serial_printf("%s(): EHCI has %d ports. Ports' status: ", __func__, num_ports);
	
	for (i = 0; i < num_ports; ++i)
		serial_printf("0x%08X ", ehci_op_regs->port_sc[i]);
	
	serial_printf("\r\n");
#endif

	return	0;
}

int	ehci_deinit(void)
{
	return	0;
}


int	ehci_open(unsigned  subdev_id)
{
	return	0;
}


int ehci_read(unsigned  subdev_id, void *buffer, unsigned long length)
{
	return	0;
}


int ehci_write(unsigned  subdev_id, const void *buffer, unsigned long length)
{
	return	0;
}


int ehci_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	return	0;
}


int ehci_close(unsigned  sub_id)
{
	return	0;
}

struct drv_entry	ehci = {ehci_init, ehci_deinit, ehci_open, ehci_read,
	ehci_write, ehci_ioctl, ehci_close};


