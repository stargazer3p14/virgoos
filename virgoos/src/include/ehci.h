/*
 *	EHCI HCD definitions
 */
#ifndef	EHCI__H
 #define EHCI__H

#include "sosdef.h"
#include "usb.h"

// EHCI capability registers
struct ehci_cap_regs
{
	byte	cap_length;
	byte	reserved;
	word	hci_version;
	dword	hcs_params;
	dword	hcc_params;
	dword	hcsp_portroute[2];
} __attribute__ ((packed));

// EHCI operational registers
struct ehci_op_regs
{
	dword	usb_cmd;
	dword	usb_sts;
	dword	usb_intr;
	dword	fr_index;
	dword	ctrl_ds_segment;
	dword	periodic_list_base;
	dword	async_list_addr;
	dword	reserved[(0x40 - 0x1C) / 4];
	dword	config_flag;
	dword	port_sc[1];
} __attribute__ ((packed));

/*
 * EHCI shared-memory data structures
 */

// EHCI queue head - control, bulk and interrupt transfers
struct ehci_qel_qtd
{
	dword	next_qtd_ptr;
	dword	alt_next_qtd_ptr;
	dword	qtd_tok;
	dword	qtd_buf_ptr[5];
} __attribute__ ((packed));

struct ehci_qh
{
	dword	qh_hlp;					// Horizontal link pointer
	dword	ep_char;				// Endpoint characteristics
	dword	ep_caps;				// Endpoint capabilities
	dword	curr_qtd_ptr;
	struct ehci_qel_qtd	qtd;			// Queue element TD (transfer overlay)
	struct ehci_qel_qtd	*first_qtd;		// Start of transaction list
	struct usb_dev	*usb_dev;			// USB device structure
} __attribute__ ((packed));

// EHCI USBCMD bits
#define	EHCI_CMD_RUN	0x1				// Run/~stop
#define	EHCI_CMD_HCRESET	0x2			// Host Controller Reset
#define	EHCI_CMD_PERIODIC_EN	0x8		// Periodic Schedule Enable
#define	EHCI_CMD_ASYNC_EN		0x20	// Asynchronous Schedule Enable
#define	EHCI_CMD_ASYNC_ADV_DOORBELL	0x40	// Interrupt on Async Advance Doorbell

#define	EHCI_CMD_START_VAL	(EHCI_CMD_RUN | /*EHCI_CMD_PERIODIC_EN | */ EHCI_CMD_ASYNC_EN | \
	/*EHCI_CMD_ASYNC_ADV_DOORBELL |*/ 0x00010000)		// Interrupt threshold = 1 u-frame, START!

// Interrupt enable/status bits
#define	EHCI_USB_INTR	0x1				// USB interrupt (transfer end)
#define	EHCI_INTR_USB_ERR	0x2			// USB Error
#define	EHCI_INTR_PORT_CHANGE	0x4		// Port change
#define	EHCI_INTR_FRLIST_ROLLOVER	0x8	// Frame list rollover
#define	EHCI_INTR_HOST_SYS_ERR	0x10	// Host system error
#define	EHCI_INTR_ASYNC_ADVANCE	0x20	// Async advance

// EHCI port status/control bits
#define	EHCI_PORTSC_CONNECTED	0x1		// Device is connected
#define	EHCI_PORTSC_CONN_CHANGE	0x2		// Connect status change
#define	EHCI_PORTSC_PORT_EN	0x4		// Port enable/disable
#define	EHCI_PORTSC_PORT_EN_CHANGE	0x8		// Port enable/disable status changed
#define	EHCI_PORTSC_PORT_RESET	0x100		// Port reset
#define	EHCI_PORTSC_PORT_POWER	0x1000		// Port power
#define	EHCI_PORTSC_PORT_OWNER	0x2000		// Port ownership bit
// ... more to go

#define	EHCI_INTR_ENABLE	(EHCI_USB_INTR | EHCI_INTR_USB_ERR | EHCI_INTR_PORT_CHANGE | \
	EHCI_INTR_FRLIST_ROLLOVER | EHCI_INTR_HOST_SYS_ERR | EHCI_INTR_ASYNC_ADVANCE)
	
	
// QTD status bits
#define	EHCI_QTD_STATUS_ACTIVE	0x80	// 'Active' bit
#define	EHCI_QTD_STATUS_HALTED	0x40	// Halted on error

#endif	//EHCI__H

