/*
 *  linux/include/asm-arm/arch-l7200/sspads_iris.h
 *
 *  Header for Iris-1 ADS7846 Controller on SSP
 *
 */
#ifndef __SSPADS_IRIS_H_INCLUDED
#define __SSPADS_IRIS_H_INCLUDED

#undef NEW_SSP_CODE

#ifndef NEW_SSP_CODE
extern void iris_init_ssp(void);
extern void iris_release_ssp(void);
extern int  iris_SspWrite(unsigned char data);
extern unsigned char iris_SspRead(void);
extern void iris_ADCEnable(void);
extern void iris_ADCDisable(void);
extern void iris_reset_adc(void);
extern void iris_init_ssp_sys(void);
extern void iris_release_access_in_irq(void);
extern void iris_get_access_in_irq(void);
extern void iris_release_access(void);
extern int iris_get_access(void);
extern void iris_enable_ssp_penint(void);
extern void iris_disable_ssp_penint(void);
extern int  iris_is_ssp_penint_enabled(void);
extern int  iris_is_ssp_adc_enabled(void);
extern void iris_TempADCEnable(void);
extern void iris_TempADCDisable(void);
extern int  iris_is_ssp_tempadc_enabled(void);
#endif  /* ! NEW_SSP_CODE */

#ifdef NEW_SSP_CODE
void iris_init_ssp_sys(void);
void iris_init_ssp(void);
void iris_release_ssp(void);
void enable_tablet_irq(void);
void disable_tablet_irq(void);
void clear_tablet_irq(void);
void begin_adc_disable_irq(void);
void end_adc_enable_irq(void);
unsigned char exchange_one_byte(unsigned char c);
int exchange_three_bytes(unsigned char* w,unsigned char* r);
int send_adcon_reset_cmd(void);
void iris_ssp_exit_blank(void);
void iris_ssp_enter_blank(void);
void iris_ssp_exit_standby(void);
void iris_ssp_enter_standby(void);
void iris_ssp_exit_suspend(void);
void iris_ssp_enter_suspend(void);
void enable_adc_con(void);
void disable_adc_con(void);
void enable_temp_adc(void);
void disable_temp_adc(void);
#endif /* NEW_SSP_CODE */

#endif /* SSPADS_H_INCLUDED */
