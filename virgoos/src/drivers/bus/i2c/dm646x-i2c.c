/*
 *	dm646x-i2c.c
 *
 *	DM646x I2C driver.
 */

#include "config.h"
#include "sosdef.h"
#include "dm646x-i2c.h"
#include "errno.h"

//#define	DEBUG_I2C	1

// 1 (s) timeout - taken from Linux driver
#define	DAVINCI_I2C_TIMEOUT	TICKS_PER_SEC
// 12 MHz fixed - taken from Linux driver
#define	I2C_PRESCALED_CLOCK	12000000

extern uint32_t	timer_counter;

static unsigned	i2c_davinci_busFreq = 100;	// kHz
static unsigned	i2c_davinci_inputClock = 148500000;
static int i2c_davinci_own_addr = 0x1;  /* Randomly assigned own address */

static	int	i2c_isr(void)
{
	return	1;
}

// Returns 0 if bus is ready, -1 if timed out
// (!) busy-polls with interrupts enabled, should be moved to interrupt? or disable preemption?
static int	wait_for_bus_ready(void)
{
	uint32_t	timeout;

	timeout = timer_counter + TICKS_PER_SEC;

	while (timeout > timer_counter)
		if (!(ind(DM646x_I2C_BASE + ICSTR) & ICSTR_BB))
			return	0;

	return	-1;
}


// Returns 0 for success, -1 with errno set upon error.
// Always sends/receives complete message of given length
//
// TODO: this never gets out of "bus busy" indication, even if we manually clear it! (probably bus is somehow busy indeed)
// We have problems with some other buses: IDE, USB - is it the same issue?
static int	i2c_transfer(struct i2c_msg *msg, int wr)
{
	int	retries = 5;			// Linux driver's suggestion
	uint8_t	zero_byte = 0;			// Linux driver's suggestion
	size_t	count;
	uint32_t	status, mode;
	size_t	len;
	void	*buf;
	uint32_t	addr;
	uint32_t	timeout;

	if (wait_for_bus_ready() < 0)
	{
#ifdef DEBUG_I2C
		serial_printf("wait_for_bus_ready() returned BUSY\n", __func__);
#endif
		errno = EBUSY;
		return	-1;
	}

	addr = msg->addr;
#ifdef DEBUG_I2C
	serial_printf("%s(): addr=%02X, len=%d flags=%08X wr=%d\n", __func__, addr, msg->len, msg->flags, wr);
#endif

	while (retries--)
	{
		// Transfer message
		len = msg->len;
		buf = msg->buf;

		// Set slave address
		outd(DM646x_I2C_BASE + ICSAR, addr);

		// They say, we can't do zero-length transfers. So they send one zero-byte instead
		if (len == 0)
		{
			buf = &zero_byte;
			len = 1;
		}
		// Transfer length
		outd(DM646x_I2C_BASE + ICCNT, len);

		// Clear any pending interrupts
		status = ind(DM646x_I2C_BASE + ICIVR);

		// Set mode for transfer
		mode = IRS | MST | STT | STP /* always STOP */;
		if (msg->flags & I2C_MSG_FLAG_XA)
			mode |= XA;
//		if (msg->flags & I2C_MSG_FLAG_STOP)
//			mode |= STP;
		if (wr)
			mode |= TRX;

#if 0
		// Enable receive and transmit interrupts
		if (wr)
			outd(DM646x_I2C_BASE + ICIMR, ind(DM646x_I2C_BASE + ICIMR) | ICIMR_ICXRDY);
		else
			outd(DM646x_I2C_BASE + ICIMR, ind(DM646x_I2C_BASE + ICIMR) | ICIMR_ICRRDY);
#endif

		outd(DM646x_I2C_BASE + ICMDR, mode);

		// Do transfer by CPU
		if (wr)
		{
			// Write transfer
			for (count = 0; count < len; ++count)
			{
				outd(DM646x_I2C_BASE + ICDXR, *((unsigned char*)buf + count));
				timeout = timer_counter + TICKS_PER_SEC;	// Timeout 1 (s)
				while (!(ind(DM646x_I2C_BASE + ICSTR) & ICSTR_ICXRDY))
					if (timeout <= timer_counter)
					{
#ifdef DEBUG_I2C
						serial_printf("%s(): write timeout on count=%d\n", __func__, count);
#endif
						return	ETIMEDOUT;
					}
			}
		}
		else
		{
			// Read transfer
			for (count = 0; count < len; ++count)
			{
				timeout = timer_counter + TICKS_PER_SEC;	// Timeout 1 (s)
				while (!(ind(DM646x_I2C_BASE + ICSTR) & ICSTR_ICRRDY))
					if (timeout <= timer_counter)
					{
#ifdef DEBUG_I2C
						serial_printf("%s(): read timeout on count=%d\n", __func__, count);
#endif
						return	ETIMEDOUT;
					}
				*((unsigned char*)buf + count) = ind(DM646x_I2C_BASE + ICDRR);
			}
		}

		return	0;

		//////////////////

		udelay(1000);
	}
}

int	i2c_init(unsigned drv_id)
{
	uint32_t	psc, d, div, clk;

	//
	// Initialize I2C controller according to spruer0b, 2.11 (14 steps)
	//
	set_int_callback(I2CINT_IRQ, i2c_isr);

	// Step 1 (enable clock from PSC) is done in platform_init(). No MUX setup is needed for I2C

	// Step 2 - put I2C controller in reset
	outd(DM646x_I2C_BASE + ICMDR, ind(DM646x_I2C_BASE + ICMDR) & ~IRS);

	psc = (i2c_davinci_inputClock + (I2C_PRESCALED_CLOCK - 1)) / I2C_PRESCALED_CLOCK - 1;
	if (psc == 0)
		d = 7;
	else if (psc == 1)
		d = 6;
	else
		d = 5;

	div = 2*(psc + 1)*i2c_davinci_busFreq*1000;
	clk = (i2c_davinci_inputClock + div - 1)/div;
	if (clk >= d)
		clk -= d;
	else
		clk = 0;

	outd(DM646x_I2C_BASE + ICPSC, psc);
	outd(DM646x_I2C_BASE + ICCLKH, clk);
	outd(DM646x_I2C_BASE + ICCLKL, clk);

	serial_printf("%s(): CLK = %ld KHz\n", __func__, i2c_davinci_inputClock / (2 * (psc + 1) * (clk + d) * 1000));

	/* Set Own Address: */
	outd(DM646x_I2C_BASE + ICOAR, i2c_davinci_own_addr);

	/* Enable interrupts */
	outd(DM646x_I2C_BASE + ICIMR, DM646x_I2C_INTR_MASK);

	// Take I2C module out of reset
	outd(DM646x_I2C_BASE + ICMDR, ind(DM646x_I2C_BASE + ICMDR) | IRS);

	serial_printf("%s(): done\n", __func__);

	return	0;
}

int	i2c_deinit(void)
{
	return	0;
}

int	i2c_open(unsigned subdev_id)
{
	return	0;
}

// I2C read() is not very useful; ioctl() is used instead
int	i2c_read(unsigned subdev_id, void *buffer, unsigned long length)
{
	errno = EINVAL;
	return	-1;
}

// I2C write() is not very useful; ioctl() is used instead
int	i2c_write(unsigned subdev_id, const void *buffer, unsigned long length)
{
	errno = EINVAL;
	return	-1;
}

/*
 *	IOCTL - send and receive messages
 */
int i2c_ioctl(unsigned subdev_id, int cmd, va_list argp)
{
	struct i2c_msg	*msg;

	if (cmd == IOCTL_DM646x_I2C_READ)
	{
		msg = va_arg(argp, void*);
		return	i2c_transfer(msg, 0);
	}
	else if (cmd == IOCTL_DM646x_I2C_WRITE)
	{
		msg = va_arg(argp, void*);
		return	i2c_transfer(msg, 1);
	}

	errno = EINVAL;
	return	-1;
}


/*
 *	Close a i2c driver.
 */
int i2c_close(unsigned sub_id)
{
	return	0;
}

drv_entry	dm646x_i2c = {i2c_init, i2c_deinit, i2c_open, i2c_read, i2c_write, i2c_ioctl, i2c_close};

//
// Linux reference code - will be removed
//
#ifdef LINUX

/*
 * This functions configures I2C and brings I2C out of reset.
 * This function is called during I2C init function. This function
 * also gets called if I2C encounetrs any errors. Clock calculation portion
 * of this function has been taken from some other driver.
 */
static int i2c_davinci_reset(struct i2c_davinci_device *dev)
{
	u32 psc, d, div, clk;

        DEB1("i2c: reset called");

	/* put I2C into reset */
	dev->regs->icmdr &= ~DAVINCI_I2C_ICMDR_IRS_MASK;

        /* NOTE: I2C Clock divider programming info
 	 * As per I2C specs the following formulas provide prescalar
         * and low/high divider values
 	 *
 	 * input clk --> PSC Div -----------> ICCL/H Div --> output clock
 	 *                       module clk
 	 *
 	 * output clk = module clk / (PSC + 1) [ (ICCL + d) + (ICCH + d) ]
 	 *
 	 * Thus,
 	 * (ICCL + ICCH) = clk = (input clk / ((psc +1) * output clk)) - 2d;
 	 *
 	 * where if PSC == 0, d = 7,
 	 *       if PSC == 1, d = 6
 	 *       if PSC > 1 , d = 5
 	 */

	/*
	 * Choose PSC to get a 12MHz or lower clock frequency after the
	 * prescaler.
	 */
	psc = (i2c_davinci_inputClock + (I2C_PRESCALED_CLOCK - 1)) /
		I2C_PRESCALED_CLOCK - 1;

	if (psc == 0)
		d = 7;
	else if (psc == 1)
		d = 6;
	else
		d = 5;

	div = 2*(psc + 1)*i2c_davinci_busFreq*1000;
	clk = (i2c_davinci_inputClock + div - 1)/div;
	if (clk >= d)
		clk -= d;
	else
		clk = 0;

	dev->regs->icpsc = psc;
	dev->regs->icclkh = clk; /* duty cycle should be 50% */
	dev->regs->icclkl = clk;

	DEB1("CLK = %ld KHz",
		i2c_davinci_inputClock / (2 * (psc + 1) * (clk + d) * 1000));
	DEB1("PSC  = %d", dev->regs->icpsc);
	DEB1("CLKL = %d", dev->regs->icclkl);
	DEB1("CLKH = %d", dev->regs->icclkh);

	/* Set Own Address: */
	dev->regs->icoar = i2c_davinci_own_addr;

	/* Enable interrupts */
	dev->regs->icimr = I2C_DAVINCI_INTR_ALL;

	enable_i2c_pins();

	/* Take the I2C module out of reset: */
	dev->regs->icmdr |= DAVINCI_I2C_ICMDR_IRS_MASK;

	return 0;
}

static int davinci_i2c_attach_client(struct i2c_adapter *adap, int addr)
{
        struct davinci_i2c_param *davinci_i2c_if = &davinci_i2c_dev;
        struct i2c_client *client = &davinci_i2c_if->client;
        int err;
        u8 data_to_u35 = 0xf6;

        if (client->adapter)
                return -EBUSY;  /* our client is already attached */

        client->addr = addr;
        client->flags = I2C_CLIENT_ALLOW_USE;
        client->driver = &davinci_i2c_if->driver;
        client->adapter = adap;

        err = i2c_attach_client(client);
        if (err) {
                client->adapter = NULL;
                return err;
        }

        err = davinci_i2c_write(1, &data_to_u35, 0x3A);

        return 0;
}


static int davinci_i2c_client_init(void)
{
        int err;
        struct i2c_driver *driver = &davinci_i2c_dev.driver;

        if (likely(initialized))
                return 0;
        initialized = 1;

        init_MUTEX(&expander_sema);

        driver->owner = THIS_MODULE;
        strlcpy(driver->name, "Davinci I2C driver", sizeof(driver->name));
        driver->id = I2C_DRIVERID_EXP0;
        driver->flags = I2C_DF_NOTIFY;
        driver->attach_adapter = davinci_i2c_probe_adapter;
        driver->detach_client = davinci_i2c_detach_client;

        err = i2c_add_driver(driver);
        if (err) {
                printk(KERN_ERR "Failed to register Davinci I2C client.\n");
                return err;
        }

        return 0;
}


/* This function is used for internal initialization */
int davinci_i2c_read(u8 size, u8 * val, u16 client_addr)
{
        int err;
        struct i2c_client *client = &davinci_i2c_dev.client;

        struct i2c_msg msg[1];

        if (!client->adapter)
                return -ENODEV;

        if (unlikely(!initialized))
                davinci_i2c_client_init();

        msg->addr = client_addr;
        msg->flags = I2C_M_RD;
        msg->len = size;
        msg->buf = val;

        err = i2c_transfer(client->adapter, msg, 1);

        if (err >= 0) {
                return 0;
        }

        return err;
}

EXPORT_SYMBOL(davinci_i2c_read);

/* This function is used for internal initialization */
int davinci_i2c_write(u8 size, u8 * val, u16 client_addr)
{
        int err;
        struct i2c_client *client = &davinci_i2c_dev.client;

        struct i2c_msg msg[1];

        if (!client->adapter)
                return -ENODEV;

        if (unlikely(!initialized))
                davinci_i2c_client_init();

        msg->addr = client_addr;
        msg->flags = 0;
        msg->len = size;
        msg->buf = val;

        err = i2c_transfer(client->adapter, msg, 1);
        if (err >= 0)
                return 0;

        return err;
}


/*
 * Low level master read/write transaction. This function is called
 * from i2c_davinci_xfer.
 */
static int
i2c_davinci_xfer_msg(struct i2c_adapter *adap, struct i2c_msg *msg, int stop)
{
        struct i2c_davinci_device *dev = i2c_get_adapdata(adap);
        u8 zero_byte = 0;
        u32 flag = 0, stat = 0;
        int i;

        DEB1("addr: 0x%04x, len: %d, flags: 0x%x, stop: %d",
             msg->addr, msg->len, msg->flags, stop);

        /* Introduce a 100musec delay.  Required for Davinci EVM board only */
        if (cpu_is_davinci_dm644x())
                udelay(100);

        /* set the slave address */
        dev->regs->icsar = msg->addr;

        /* Sigh, seems we can't do zero length transactions. Thus, we
         * can't probe for devices w/o actually sending/receiving at least
         * a single byte. So we'll set count to 1 for the zero length
         * transaction case and hope we don't cause grief for some
         * arbitrary device due to random byte write/read during
         * probes.
         */
        if (msg->len == 0) {
                dev->buf = &zero_byte;
                dev->buf_len = 1;
        } else {
                dev->buf = msg->buf;
                dev->buf_len = msg->len;
        }

        dev->regs->iccnt = dev->buf_len;
        dev->cmd_complete = 0;
        dev->cmd_err = 0; 

        /* Clear any pending interrupts by reading the IVR */
        stat = dev->regs->icivr;

        /* Take I2C out of reset, configure it as master and set the start bit */
        flag =
            DAVINCI_I2C_ICMDR_IRS_MASK | DAVINCI_I2C_ICMDR_MST_MASK |
            DAVINCI_I2C_ICMDR_STT_MASK;

        /* if the slave address is ten bit address, enable XA bit */
        if (msg->flags & I2C_M_TEN)
                flag |= DAVINCI_I2C_ICMDR_XA_MASK;
        if (!(msg->flags & I2C_M_RD))
                flag |= DAVINCI_I2C_ICMDR_TRX_MASK;
        if (stop)
                flag |= DAVINCI_I2C_ICMDR_STP_MASK;

        /* Enable receive and transmit interrupts */
        if (msg->flags & I2C_M_RD)
                dev->regs->icimr |= DAVINCI_I2C_ICIMR_ICRRDY_MASK;
        else
                dev->regs->icimr |= DAVINCI_I2C_ICIMR_ICXRDY_MASK;

        /* write the data into mode register */
        dev->regs->icmdr = flag;

        /* wait for the transaction to complete */
        wait_event_timeout (dev->cmd_wait, dev->cmd_complete, DAVINCI_I2C_TIMEOUT);

        dev->buf_len = 0;

        if (!dev->cmd_complete) {
                i2c_warn("i2c: cmd complete failed: complete = 0x%x, \
                          icstr = 0x%x\n", dev->cmd_complete,
                          dev->regs->icstr);

                if (cpu_is_davinci_dm644x() || cpu_is_davinci_dm355()) {
                        /* Send the NACK to the slave */
                        dev->regs->icmdr |= DAVINCI_I2C_ICMDR_NACKMOD_MASK;
                        /* Disable I2C */
                        disable_i2c_pins();

                        /* Send high and low on the SCL line */
                        for (i = 0; i < 10; i++)
                                pulse_i2c_clock();

                        /* Re-enable I2C */
                        enable_i2c_pins();
                }


                i2c_davinci_reset(dev);
                dev->cmd_complete = 0;
                return -ETIMEDOUT;
        }
        dev->cmd_complete = 0;

        /* no error */
        if (!dev->cmd_err)
                return msg->len;

        /* We have an error */
        if (dev->cmd_err & DAVINCI_I2C_ICSTR_NACK_MASK) {
                if (msg->flags & I2C_M_IGNORE_NAK)
                        return msg->len;
                if (stop)
                        dev->regs->icmdr |= DAVINCI_I2C_ICMDR_STP_MASK;
                return -EREMOTEIO;
        }
        if (dev->cmd_err & DAVINCI_I2C_ICSTR_AL_MASK) {
                i2c_davinci_reset(dev);
                return -EIO;
        }
        return msg->len;
}


/*
 * Prepare controller for a transaction and call i2c_davinci_xfer_msg

 */
static int
i2c_davinci_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
        int count;
        int ret = 0;
        char retries = 5;

        DEB1("msgs: %d", num);

        if (num < 1 || num > MAX_MESSAGES)
                return -EINVAL;

        /* Check for valid parameters in messages */
        for (count = 0; count < num; count++)
                if (msgs[count].buf == NULL)
                        return -EINVAL;

        if ((ret = i2c_davinci_wait_for_bb(1, adap)) < 0)
                return ret;

        for (count = 0; count < num; count++) {
                DEB1("msg: %d, addr: 0x%04x, len: %d, flags: 0x%x",
                     count, msgs[count].addr, msgs[count].len,
                     msgs[count].flags);

                do {
                        ret = i2c_davinci_xfer_msg(adap, &msgs[count],
                                                   (count == (num - 1)));

                        if (ret < 0) {
                                struct i2c_davinci_device *dev = i2c_get_adapdata(adap);

                                DEB1("i2c: retry %d - icstr = 0x%x",
                                      retries, dev->regs->icstr);
                                mdelay (1);
                                retries--;
                        } else
                                break;
                } while (retries);

                DEB1("ret: %d", ret);

                if (ret != msgs[count].len)
                        break;
        }

        if (ret >= 0 && num >= 1)
                ret = num;

        DEB1("ret: %d", ret);

        return ret;
}



static void davinci_i2c_client_cleanup(void)
{
        i2c_detach_client(&davinci_i2c_dev.client);
        davinci_i2c_dev.client.adapter = NULL;

        return;
}


static int __init i2c_davinci_init(void)
{
        int status;
        struct device   *dev = NULL;

        DEB0("%s %s", __TIME__, __DATE__);

        DEB1("i2c_davinci_init()");

        if (cpu_is_davinci_dm6467())
                davinci_i2c_expander_op (0x3A, I2C_INT_DM646X, 0);

        /* 
         * NOTE: On DaVinci EVM, the i2c bus frequency is set to 20kHz
         *       so that the MSP430, which is doing software i2c, has
         *       some extra processing time
         */
        if (machine_is_davinci_evm())
                i2c_davinci_busFreq = 20;
        else if (machine_is_davinci_dm6467_evm())
                i2c_davinci_busFreq = 100;
        else if (i2c_davinci_busFreq > 200)
                i2c_davinci_busFreq = 400;      /*Fast mode */
        else
                i2c_davinci_busFreq = 100;      /*Standard mode */


        i2c_clock = clk_get (dev, "I2CCLK");

        if (IS_ERR(i2c_clock))
                return -1;

        clk_use (i2c_clock);
        clk_enable (i2c_clock);
        i2c_davinci_inputClock = clk_get_rate (i2c_clock);

        DEB1 ("IP CLOCK = %ld", i2c_davinci_inputClock);

        memset(&i2c_davinci_dev, 0, sizeof(i2c_davinci_dev));
        init_waitqueue_head(&i2c_davinci_dev.cmd_wait);

        i2c_davinci_dev.regs = (davinci_i2cregsovly)I2C_BASE;

        status = (int)request_region(I2C_BASE, I2C_IOSIZE, MODULE_NAME);
        if (!status) {
                i2c_err("I2C is already in use\n");
                return -ENODEV;
        }

        status = request_irq(IRQ_I2C, i2c_davinci_isr, 0, "i2c",
                             &i2c_davinci_dev);
        if (status) {
                i2c_err("failed to request I2C IRQ");
                goto do_release_region;
        }

        i2c_set_adapdata(&i2c_davinci_adap, &i2c_davinci_dev);
        status = i2c_add_adapter(&i2c_davinci_adap);
        if (status) {
                i2c_err("failed to add adapter");
                goto do_free_irq;
        }

        i2c_davinci_reset(&i2c_davinci_dev);

        if (driver_register(&davinci_i2c_driver) != 0)
                printk(KERN_ERR "Driver register failed for davinci_i2c\n");
        if (platform_device_register(&davinci_i2c_device) != 0) {
                printk(KERN_ERR "Device register failed for i2c\n");
                driver_unregister(&davinci_i2c_driver);
        }

        return 0;

      do_free_irq:
        free_irq(IRQ_I2C, &i2c_davinci_dev);
      do_release_region:
        release_region(I2C_BASE, I2C_IOSIZE);

        return status;
}


static void __exit i2c_davinci_exit(void)
{
        struct i2c_davinci_device dev;

        clk_unuse (i2c_clock);
        clk_disable (i2c_clock);
        i2c_del_adapter(&i2c_davinci_adap);
        dev.regs->icmdr = 0;
        free_irq(IRQ_I2C, &i2c_davinci_dev);
        release_region(I2C_BASE, I2C_IOSIZE);
        driver_unregister(&davinci_i2c_driver);
        platform_device_unregister(&davinci_i2c_device);
}
#endif


