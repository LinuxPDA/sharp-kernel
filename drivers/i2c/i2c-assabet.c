/*
  i2c-assabet.c

  simple i2c access for the Intel Assabet StrongArm platform.

  currently the only i2c device on the Assabet is the ADV7171 video
  encoder.
*/

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stddef.h>
#include <linux/parport.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include <asm/hardware.h>

#ifdef CONFIG_SA1100_ASSABET
#define L3_DataPin	GPIO_GPIO(15)
#define L3_ClockPin	GPIO_GPIO(18)
#define L3_ModePin	GPIO_GPIO(17)
#endif

static int iic_acquired;
static int iic_data_out;
static int iic_clock_out;

/*
 * Grab control of the IIC/L3 shared pins
 */
static inline void iic_acquirepins(void)
{
	GPSR = L3_ClockPin;
	GPCR = L3_DataPin;
	GPDR |= L3_ClockPin;
	GPDR &= ~L3_DataPin);
	iic_acquired = 1;
	iic_clock_out = 1;
}

/*
 * Release control of the IIC/L3 shared pins
 */
static inline void iic_releasepins(void)
{
	GPDR &= ~(L3_ClockPin | L3_DataPin);
	GPCR = (L3_ClockPin | L3_DataPin);
	iic_acquired = 0;
}

/*
 * Initialize the interface
 */
static void iic_init(void)
{
	GAFR &= ~(L3_DataPin | L3_ClockPin);
	GPSR = L3_ModePin;
	GPDR |= L3_ModePin;
	iic_releasepins();
}


/* --------- */

static void bit_assabet_setscl(void *data, int state)
{
	if (!iic_acquired)
		iic_acquirepins();

	if (!iic_clock_out) {
		GPDR |= L3_ClockPin;
		iic_clock_out = 1;
	}

	if (state) {
		GPSR = L3_ClockPin;
	} else {
		GPCR = L3_ClockPin;
	}
	
}

static void bit_assabet_setsda(void *data, int state)
{
	if (!iic_acquired)
		iic_acquirepins();

#if	0
	if (!iic_data_out) {
		GPDR |= L3_DataPin;
		iic_data_out = 1;
	}
#endif

	if (state) {
		GPDR &= ~(L3_DataPin); /* XXX Hack/test/debug */
	} else {
		GPCR = L3_DataPin;
		GPDR |= L3_DataPin; /* XXX Hack/test/debug */
	}
	
} 

static int bit_assabet_getscl(void *data)
{
	if (!iic_acquired)
		iic_acquirepins();

	if (iic_clock_out) {
		GPDR &= ~(L3_ClockPin);
		iic_clock_out = 0;
	}

	return (GPLR & L3_ClockPin) ? 1 : 0;
}

static int bit_assabet_getsda(void *data)
{
	if (!iic_acquired)
		iic_acquirepins();

	if (iic_data_out) {
		GPDR &= ~(L3_DataPin);
		iic_data_out = 0;
	}

	return (GPLR & L3_DataPin) ? 1 : 0;
}

/* ---------- */

static int bit_assabet_reg(struct i2c_client *client)
{
	return 0;
}

static int bit_assabet_unreg(struct i2c_client *client)
{
	return 0;
}

static void bit_assabet_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
	iic_acquirepins();
}

static void bit_assabet_dec_use(struct i2c_adapter *adap)
{
	iic_releasepins();
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static struct i2c_algo_bit_data bit_assabet_data = {
	NULL,
	bit_assabet_setsda,
	bit_assabet_setscl,
	bit_assabet_getsda,
	NULL,
	5, 5, 100,		/*	waits, timeout */
};


static struct i2c_adapter bit_assabet_ops = {
	"Assabet",
	I2C_HW_B_IOC,
	NULL,
	&bit_assabet_data,
	bit_assabet_inc_use,
	bit_assabet_dec_use,
	bit_assabet_reg,
	bit_assabet_unreg,
};

int __init i2c_assabet_init(void)
{
	printk("i2c-assabet.o: i2c Assabet module\n");

	/* enable the one and only assabet i2c device */
	BCR_set(BCR_CODEC_RST);

	/* reset hardware to sane state */
	iic_init();

	if (i2c_bit_add_bus(&bit_assabet_ops) < 0) {
		printk("i2c-assabet: Unable to register with I2C\n");
		return -ENODEV;
	}

	return 0;
}

void __exit i2c_assabet_exit(void)
{
	i2c_bit_del_bus(&bit_assabet_ops);
}


unsigned char adv7171_settings[] = {
  0x00, /* subaddr */
  0x00, /* Mode Register 0 */
  0x80, /* Mode Register 1 */
  0x08, /* Mode Register 2 */
  0x00, /* Mode Register 3 */
  0x18, /* Mode Register 4 */
  0x00, /* Reserved */
  0x00, /* Reserved */
  0x4c, /* Timing Register 0 */
  0x00, /* Timing Register 1 */
  0x16, /* Subcarrier Freq 0 */
  0x7c, /* Subcarrier Freq 1 */
  0xf0, /* Subcarrier Freq 2 */
  0x21, /* Subcarrier Freq 3 */
  0x00, /* Subcarrier phase */
  0x00, /* Extended Captioning 0 */
  0x00, /* Extended Captioning 1 */
  0x00, /* Closed Captioning 0 */
  0x00, /* Closed Captioning 1 */
  0x00, /* Pedestal Register 0 */
  0x00, /* Pedestal Register 1 */
  0x00, /* Pedestal Register 2 */
  0x00, /* Pedestal Register 3 */
  0x00, /* CGMS_WSS 0 */
  0x00, /* CGMS_WSS 1 */
  0x00, /* CGMS_WSS 2 */
  0x00  /* Teletext request pos */
};

int assabet_i2c_adv7171_init(void)
{
  struct i2c_adapter *adap = &bit_assabet_ops;
  struct i2c_msg msg[1];
  int ret;

  printk("i2c_assabet: adv7171 init\n");

  msg[0].addr = 0x54 / 2;
  msg[0].flags = 0;
  msg[0].len = 26+1;
  msg[0].buf = adv7171_settings;

  printk("starting hack\n");
  ret = adap->algo->master_xfer(adap, msg, 1);
  printk("hack ret %d\n", ret);

  printk("i2c_assabet: adv7171 init done\n");
  return 0;
}

#ifdef MODULE
MODULE_AUTHOR("Brad Parker <brad@heeltoe.com>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for Intel Assabet");

module_init(i2c_assabet_init);
module_exit(i2c_assabet_exit);
#endif

