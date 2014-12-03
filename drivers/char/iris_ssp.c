/*
 *  drivers/char/iris_ssp.c
 *  for SSP access to ADS7846 touch screen controller
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/arch/irqs.h>
#include <asm/irq.h>
#include <linux/delay.h>
#include <asm/delay.h>
#include <asm/hardware.h>

#include <asm/arch/sib.h>
#include <asm/arch/hardware.h>
#include <asm/arch/gpio.h>
#include <asm/arch/fpga.h>
#include <asm/arch/touchscr_iris.h>
#include <asm/arch/sspads_iris.h>


/* ==============================================
 *    Linkup Register Bits
 * ============================================== */

// SSCR0 (16bits)
// Data Size Select
#define DSS_MASK	0x000F
#define DSS_16		0x000F
#define DSS_15		0x000E
#define DSS_14		0x000D
#define DSS_13		0x000C
#define DSS_12		0x000B
#define DSS_11		0x000A
#define DSS_10		0x0009
#define DSS_9		0x0008
#define DSS_8		0x0007
#define DSS_7		0x0006
#define DSS_6		0x0005
#define DSS_5		0x0004
#define DSS_4		0x0003

// Frame Format
#define FRF_MASK	0x0030
#define FRF_MOT		0x0000
#define FRF_TISS	0x0001
#define FRF_NSM		0x0010

// SYCLK Polarity
#define SPO_MASK	0x0040
#define SPO_NL_PH	0x0000
#define SPO_NH_PL	0x0040

// SYCLK Phase
#define SPH_MASK	0x0080
#define SPH_TRAIL	0x0000
#define SPH_LEAD	0x0080

// Serial Clock Rate
#define SCR_MASK	0xFF00
//#define SCR			0x1200
#define SCR		0x0000

// SSCR1 (16bits)
#define RIM	0x0001	// Receive FIFO Interupt Mask
#define TIM	0x0002	// Transimit FIFO Interupt Mask
#define LBM	0x0004	// Loop Back Mode
#define SSE 0x0008	// SSP Enable


// SSSR (16bits)
#define TFE	0x0001	// Transmit FIFO is empty		(R)
#define TNF	0x0002	// Transmit FIFO is not full		(R)
#define RNE	0x0004	// Recieve FIFO is not empty		(R)
#define BSY     0x0008	// Busy					(R)
#define TFS	0x0010	// Transmit FIFO Service Request	(R)
#define RFS	0x0020	// Recieve FIFO Service Request		(R)
#define ROR	0x0040	// Receive FIFO Overrun			(R/W)
#define RFF	0x0080	// Recieve FIFO is full			(R)


// SSDR (16bits)
#define SSDR_PUP_X16		0x00D3
#define SSDR_PUP_Y16		0x0093
#define SSDR_PUP_Z1_16		0x00B3
#define SSDR_PUP_Z2_16		0x00C3
#define SSDR_READ_X16		0x00D0
#define SSDR_READ_Y16		0x0090
#define SSDR_READ_Z1_16		0x00B0
#define SSDR_READ_Z2_16		0x00C0

/* ==============================================
 *    Iris Register Bits
 * ============================================== */

#define	TABLET_CS_BIT	(bitPE3)
#define THERM_STB_BIT	(bitPE4)
#define WSRC_PENIRQ_BIT ((ushort)bitWSRC6)

/*****************************************************************************************
	SSP operations
*****************************************************************************************/

#define DEBUGPRINT(s)   /* printk s */ /* ignore */

#define SSCR0_ENA	(DSS_8|FRF_MOT|SPO_NL_PH|SPH_TRAIL|SCR)
#define SSCR1_ENA	(SSE)

#define TMS_EN_BIT	0x00000004	// 3M6 enable bit


#ifndef NEW_SSP_CODE


static int ssp_access_crit = 0;
static int reference_count = 0;
static int ssp_penint_enabled = 0;

static int no_more_access = 0;
static int suspended_and_singletask = 0;
#ifdef DECLARE_WAITQUEUE
static wait_queue_head_t ssp_suspend_wait;
static wait_queue_head_t ssp_access_release_wait;
#else	
static struct wait_queue *ssp_suspend_wait;
static struct wait_queue *ssp_access_release_wait;
#endif	

int iris_is_ssp_penint_enabled(void)
{
  return(ssp_penint_enabled);
}

int iris_is_ssp_adc_enabled(void)
{
  return( ( GET_PEDR(TABLET_CS_BIT) ? 0 : 1 ) ); /* LO means enabled */
}

int iris_is_ssp_tempadc_enabled(void)
{
  return( ( GET_PEDR(THERM_STB_BIT) ? 0 : 1 ) ); /* LO means enabled */
}

void iris_enable_ssp_penint(void)
{
  //iris_get_access(); /* this access is probable to make dead-lock ! moved to iris_ts */

  iris_SspWrite(0x80);
  iris_SspRead();

  udelay(50);

  iris_ADCDisable();

  //iris_release_access(); /* this access is probable to make dead-lock ! moved to iris_ts */

  udelay(50);

  clear_fpga_int(WSRC_PENIRQ_BIT);
  udelay(10);
  enable_fpga_irq(WSRC_PENIRQ_BIT);
  udelay(10);
  clear_fpga_int(WSRC_PENIRQ_BIT);
  ssp_penint_enabled = 1;
}

void iris_enable_ssp_penint_fpga(void)
{
  clear_fpga_int(WSRC_PENIRQ_BIT);
  udelay(10);
  enable_fpga_irq(WSRC_PENIRQ_BIT);
  udelay(10);
  clear_fpga_int(WSRC_PENIRQ_BIT);
  ssp_penint_enabled = 1;
}

void iris_disable_ssp_penint(void)
{
  clear_fpga_int(WSRC_PENIRQ_BIT);
  udelay(10);
  disable_fpga_irq(WSRC_PENIRQ_BIT);
  udelay(10);
  clear_fpga_int(WSRC_PENIRQ_BIT);
  ssp_penint_enabled = 0;
}

void iris_turn_adc_to_lowpower(void)
{
  iris_SspWrite(0x80);
  iris_SspRead();
  udelay(50);
}

static int init_ssp_sys_initialized = 0;

void iris_init_ssp_sys(void)
{
  if( ! init_ssp_sys_initialized ){
    DEBUGPRINT(("initializing ssp sys....\n"));
    IO_SYS_CLOCK_ENABLE |= TMS_EN_BIT;
    SET_PE_OUT(TABLET_CS_BIT|THERM_STB_BIT);
    SET_PEDR_HI(TABLET_CS_BIT|THERM_STB_BIT);
    SET_PESBSR_HI(TABLET_CS_BIT|THERM_STB_BIT);
    iris_ADCDisable();
    ssp_access_crit = 0;
    reference_count = 0;
    ssp_penint_enabled = 0;
    init_waitqueue_head(&ssp_suspend_wait);
    init_waitqueue_head(&ssp_access_release_wait);
    init_ssp_sys_initialized = 1;
    DEBUGPRINT(("done initializing ssp sys....\n"));
  }
}

void iris_init_ssp(void)
{
  DEBUGPRINT(("iris_init_ssp:\n"));
  if( ! reference_count ){
    DEBUGPRINT(("initializing ssp....\n"));
    IO_SYS_CLOCK_ENABLE |= TMS_EN_BIT;
    IO_SSCR1 = (unsigned long)SSCR1_ENA;
    IO_SSCR0 = (unsigned long)SSCR0_ENA;
    IO_SSSR |= (unsigned long)ROR;
    iris_disable_ssp_penint();
    clear_fpga_int(WSRC_PENIRQ_BIT);
    set_fpga_int_trigger_lowlevel(WSRC_PENIRQ_BIT);
    DEBUGPRINT(("done initializing ssp....\n"));
  }
  reference_count++;
}

void iris_release_ssp(void)
{
  reference_count--;
  DEBUGPRINT(("iris_release_ssp:\n"));
  if( ! reference_count ){
    DEBUGPRINT(("releasing ssp....\n"));
    IO_SSCR1 &= ~((unsigned long)SSE);
    IO_SSSR &= ~((unsigned long)ROR);
    iris_disable_ssp_penint();
  }
  DEBUGPRINT(("done iris_release_ssp:\n"));
}

#define ACCESS_TIMEOUT 1000

int iris_get_access(void)
{
  unsigned long flags;
  int i;

  DEBUGPRINT(("iris_get_access start\n"));

  for(i=0;i<ACCESS_TIMEOUT;i++){
    if( ( ! suspended_and_singletask ) && no_more_access ){
      printk("enterring no_more_access\n");
      interruptible_sleep_on(&ssp_suspend_wait);
      printk("exiting no_more_access\n");
    }
    save_flags(flags); cli();
    if( ! ssp_access_crit ){
      ssp_access_crit = 1;
      DEBUGPRINT(("iris_get_access end\n"));
      restore_flags(flags);
      return 1;
    }
    restore_flags(flags);
    udelay(50);
  }
  return ( i == ACCESS_TIMEOUT ? 0 : 1 );
}

void iris_release_access(void)
{
  unsigned long flags;
  DEBUGPRINT(("iris_release_access start\n"));
  save_flags(flags); cli();
  ssp_access_crit = 0;
  restore_flags(flags);
  if( no_more_access ){
    wake_up_interruptible((wait_queue_head_t*)&ssp_access_release_wait);
  }
  DEBUGPRINT(("iris_release_access end\n"));
}

void iris_ssp_before_suspend_hook_under_sche(void)
{
  printk("waiting for access exit\n");
  if( suspended_and_singletask ){
    printk("already suspended\n");
    return;
  }
  no_more_access = 1;
  while( ssp_access_crit ){
    printk("sleepon\n");
    interruptible_sleep_on(&ssp_suspend_wait);
    printk("sleepon out\n");
  }
  suspended_and_singletask = 1;
  printk("all ssp cleared\n");
}
void iris_ssp_after_suspend_hook_under_sche(void)
{
  printk("prohibit ssp access\n");
  if( ! suspended_and_singletask ){
    printk("already resumed\n");
    return;
  }
  suspended_and_singletask = 0;
  no_more_access = 0;
  wake_up_interruptible((wait_queue_head_t*)&ssp_suspend_wait);
  printk("wake_up_interruptible for ssp done\n");
}


void iris_get_access_in_irq(void)
{
  /* this cannot be done , currently */
  while(1){
    printk("iris_get_access_in_irq is not supported!\n");
  }
}

void iris_release_access_in_irq(void)
{
  /* this cannot be done , currently */
  while(1){
    printk("iris_get_release_in_irq is not supported!\n");
  }
}

int iris_SspWrite(unsigned char data)
{
  unsigned int	loop_cnt = 0;

  while (	IO_SSSR & (ulong)BSY || (IO_SSSR & (ulong)TNF) == 0) {
    if (UINT_MAX <= loop_cnt)
      return (0);
    loop_cnt++;
  }
  IO_SSDR = (unsigned long)data;
  return (1);
}

unsigned char iris_SspRead(void)
{
  unsigned int	loop_cnt = 0;

  while ((IO_SSSR & (unsigned long)BSY) || (IO_SSSR & (unsigned long)RNE) == 0) {
    if (UINT_MAX <= loop_cnt)
      return (0);
    loop_cnt++;
  }
  return (unsigned char)IO_SSDR;
}

void iris_ADCEnable(void)
{
  /* Tablet controler CS - Low	*/
  SET_PEDR_LO(TABLET_CS_BIT);
  udelay(1);
}

void iris_ADCDisable(void)
{
  SET_PEDR_HI(TABLET_CS_BIT);	/* Tablet controler CS - High	*/
}

void iris_TempADCEnable(void)
{
  SET_PEDR_LO(THERM_STB_BIT);
  udelay(50);
}

void iris_TempADCDisable(void)
{
  SET_PEDR_HI(THERM_STB_BIT);
  udelay(50);
}

void iris_reset_adc(void)
{
  iris_SspWrite(0x80);
  iris_SspRead();
}

#endif /* ! NEW_SSP_CODE */

/*
 * code ends here.
 */

#ifdef NEW_SSP_CODE

#define DPRINTK(x, args...)  /* printk(__FUNCTION__ ": " x,##args) */

/*
 *    Pen IRQ Raw Stuff
 */

static void setup_penint(void)
{
  DPRINTK("\n");
  set_fpga_int_trigger_lowlevel(WSRC_PENIRQ_BIT);
  barrier();
}

static void enable_penint(void)
{
  DPRINTK("\n");
  clear_fpga_int(WSRC_PENIRQ_BIT);
  enable_fpga_irq(WSRC_PENIRQ_BIT);
  barrier();
}

static void disable_penint(void)
{
  DPRINTK("\n");
  disable_fpga_irq(WSRC_PENIRQ_BIT);
  barrier();
}

static void clear_penint(void)
{
  DPRINTK("\n");
  clear_fpga_int(WSRC_PENIRQ_BIT);
  barrier();
}
    

/*
 *    SSP Stuff
 */

#define SSP_TIMEOUT_COUNT  UINT_MAX

static int ssp_write_byte(unsigned char data)
{
  unsigned int loop_cnt = 0;
  while( ( IO_SSSR & (ulong)BSY ) || ( IO_SSSR & (ulong)TNF ) == 0 ){
    if( SSP_TIMEOUT_COUNT <= loop_cnt )
      return (0);
    loop_cnt++;
  }
  IO_SSDR = (unsigned long)data;
  barrier();
  return (1);
}

static unsigned char ssp_read_byte(void)
{
  unsigned int	loop_cnt = 0;

  while( ( IO_SSSR & (unsigned long)BSY ) || ( IO_SSSR & (unsigned long)RNE ) == 0) {
    if( SSP_TIMEOUT_COUNT <= loop_cnt)
      return (0);
    loop_cnt++;
  }
  barrier();
  return (unsigned char)IO_SSDR;
}

unsigned char exchange_one_byte(unsigned char c)
{
  unsigned long flags;
  unsigned char retval = 0;
  DPRINTK("1\n");
  save_flags(flags); cli();
  if( ! ssp_write_byte(c) ){
    DPRINTK("2\n");
    restore_flags(flags);
    return 0;
  }
  DPRINTK("3\n");
  retval = ssp_read_byte();
  DPRINTK("4\n");
  restore_flags(flags);
  barrier();
  return retval;
}

int exchange_three_bytes(unsigned char* w,unsigned char* r)
{
  unsigned long flags;
  int i = 0;
  DPRINTK("1\n");
  save_flags(flags); cli();
  for(i=0;i<3;i++){
    if( ! ssp_write_byte(w[i]) ) goto FAIL_SAFE;
  }
  barrier();
  DPRINTK("2\n");
  for(i=0;i<3;i++){
    r[i] = ssp_read_byte();
  }
  DPRINTK("3\n");
  restore_flags(flags);
  barrier();
  return 0;
 FAIL_SAFE:
  DPRINTK("4\n");
  restore_flags(flags);
  for(i=0;i<3;i++) r[i] = 0;
  barrier();
  return -1;
}

int send_adcon_reset_cmd(void)
{
  exchange_one_byte(0x80);
  barrier();
  return 0;
}

/*
 *    ADC Stuff
 */

static int adccon_ref = 0;
static int tempbit_ref = 0;

void enable_adc_con(void)
{
  unsigned long flags;
  DPRINTK("\n");
  if( ! adccon_ref ){
    DPRINTK("enable\n");
    SET_PEDR_LO(TABLET_CS_BIT);
    udelay(1);
  }
  save_flags(flags); cli();
  adccon_ref++;
  restore_flags(flags);
  barrier();
}

void disable_adc_con(void)
{
  unsigned long flags;
  DPRINTK("\n");
  save_flags(flags); cli();
  adccon_ref--;
  restore_flags(flags);
  if( ! adccon_ref ){
    DPRINTK("disable\n");
    SET_PEDR_HI(TABLET_CS_BIT);
  }
  barrier();
}

void enable_temp_adc(void)
{
  unsigned long flags;
  DPRINTK("\n");
  if( ! tempbit_ref ){
    DPRINTK("enable\n");
    SET_PEDR_LO(THERM_STB_BIT);
    udelay(1);
  }
  save_flags(flags); cli();
  tempbit_ref++;
  restore_flags(flags);
  barrier();
}

void disable_temp_adc(void)
{
  unsigned long flags;
  DPRINTK("\n");
  save_flags(flags); cli();
  tempbit_ref--;
  restore_flags(flags);
  if( ! tempbit_ref ){
    DPRINTK("disable\n");
    SET_PEDR_HI(THERM_STB_BIT);
  }
  barrier();
}

/*
 *    PenIRQ Stuff
 */

static int penirq_enabled_for_tablet = 0;
static int penirq_disabled_for_adc = 0;

void enable_tablet_irq(void)
{
  unsigned long flags;
  DPRINTK("\n");
  penirq_enabled_for_tablet = 1;
  save_flags(flags); cli();
  if( ! penirq_disabled_for_adc ){
    DPRINTK("enable\n");
    enable_penint();
  }
  restore_flags(flags);
  barrier();
}

void disable_tablet_irq(void)
{
  DPRINTK("\n");
  penirq_enabled_for_tablet = 0;
  disable_penint();
  barrier();
}

void clear_tablet_irq(void)
{
  DPRINTK("\n");
  clear_penint();
  barrier();
}

void begin_adc_disable_irq(void)
{
  unsigned long flags;
  DPRINTK("\n");
  save_flags(flags); cli();
  penirq_disabled_for_adc++;
  if( penirq_enabled_for_tablet ){
    disable_penint();
  }
  restore_flags(flags);
  barrier();
}

void end_adc_enable_irq(void)
{
  unsigned long flags;
  DPRINTK("\n");
  save_flags(flags); cli();
  penirq_disabled_for_adc--;
  if( ! penirq_disabled_for_adc ){
    DPRINTK("adc\n");
    if( penirq_enabled_for_tablet ){
      DPRINTK("enable\n");
      enable_penint();
    }
  }
  restore_flags(flags);
  barrier();
}

/*
 *    Init/Release Stuff
 */

static int reference_count = 0;

void iris_init_ssp_sys(void)
{
  static int init_ssp_sys_initialized = 0;
  DPRINTK("\n");
  if( ! init_ssp_sys_initialized ){
    DPRINTK("init\n");
    IO_SYS_CLOCK_ENABLE |= TMS_EN_BIT;
    SET_PE_OUT(TABLET_CS_BIT|THERM_STB_BIT);
    SET_PESBSR_HI(TABLET_CS_BIT|THERM_STB_BIT);
    SET_PEDR_HI(TABLET_CS_BIT|THERM_STB_BIT);
    reference_count = 0;
    init_ssp_sys_initialized = 1;
    DPRINTK("done\n");
  }
  barrier();
}

void iris_init_ssp(void)
{
  unsigned long flags;
  DPRINTK("\n");
  iris_init_ssp_sys();
  save_flags(flags); cli();
  if( ! reference_count ){
    DPRINTK("init\n");
    IO_SYS_CLOCK_ENABLE |= TMS_EN_BIT;
    IO_SSCR1 = (unsigned long)SSCR1_ENA;
    IO_SSCR0 = (unsigned long)SSCR0_ENA;
    IO_SSSR |= (unsigned long)ROR;
    disable_penint();
    clear_penint();
    setup_penint();
    clear_penint();
    DPRINTK("done\n");
  }
  reference_count++;
  restore_flags(flags);
  barrier();
}

void iris_release_ssp(void)
{
  unsigned long flags;
  DPRINTK("\n");
  save_flags(flags); cli();
  reference_count--;
  if( ! reference_count ){
    DPRINTK("releasing\n");
    IO_SSCR1 &= ~((unsigned long)SSCR1_ENA);
    IO_SSSR &= ~((unsigned long)ROR);
    disable_penint();
    clear_penint();
    SET_PESBSR_HI(TABLET_CS_BIT|THERM_STB_BIT);
    SET_PEDR_HI(TABLET_CS_BIT|THERM_STB_BIT);
  }
  DPRINTK("done\n");
  restore_flags(flags);
  barrier();
}

/*
 *   Power Management Stuff
 */

static int state_suspend = 0;
static int state_standby = 0;
static int state_blank = 0;

void iris_ssp_enter_suspend(void)
{
  if( state_suspend ) return;
  if( reference_count ){
    send_adcon_reset_cmd();
    IO_SSSR |= (unsigned long)ROR;
  }
  state_suspend = 1;
  barrier();
}

void iris_ssp_exit_suspend(void)
{
  if( ! state_suspend ) return;
  if( reference_count ){
    IO_SSSR |= (unsigned long)ROR;
    send_adcon_reset_cmd();
  }
  if( adccon_ref ){
    SET_PEDR_LO(TABLET_CS_BIT);
    udelay(1);
  }else{
    SET_PEDR_HI(TABLET_CS_BIT);
  }
  if( tempbit_ref ){
    SET_PEDR_LO(THERM_STB_BIT);
    udelay(1);
  }else{
    SET_PEDR_HI(THERM_STB_BIT);
  }
  state_suspend = 0;
  barrier();
}

void iris_ssp_enter_standby(void)
{
  if( state_standby ) return;
  if( reference_count ){
    send_adcon_reset_cmd();
    IO_SSSR |= (unsigned long)ROR;
  }
  state_standby = 1;
  barrier();
}

void iris_ssp_exit_standby(void)
{
  if( ! state_standby ) return;
  if( reference_count ){
    IO_SSSR |= (unsigned long)ROR;
    send_adcon_reset_cmd();
  }
  state_standby = 0;
  barrier();
}

void iris_ssp_enter_blank(void)
{
  if( state_blank ) return;
  state_blank = 1;
  barrier();
}

void iris_ssp_exit_blank(void)
{
  if( ! state_blank ) return;
  state_blank = 0;
  barrier();
}

void iris_ssp_before_suspend_hook_under_sche(void)
{
  barrier();
}

void iris_ssp_after_suspend_hook_under_sche(void)
{
  barrier();
}

#endif /* NEW_SSP_CODE */

