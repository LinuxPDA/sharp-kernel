/*
 * Iris APM Driver Header File
 *
 * based on collie_apm.h
 *
 */

#ifndef IRIS_APM_H_INCLUDED
#define IRIS_APM_H_INCLUDED

/* Wakeup Source Definitions , including currently ignored wakeup-srcs */

#define WAKEUP_POWERON  ( 1 << 0 )  /* PowerOn Reset ( ignored ... ) */
#define WAKEUP_COMM     ( 1 << 1 )  /* GSM Communication Module Ring */
#define WAKEUP_EXTPOW   ( 1 << 2 )  /* External Power ( ignored ... ) */
#define WAKEUP_UART2    ( 1 << 3 )  /* External Port , UART2 DSR */
#define WAKEUP_USB      ( 1 << 4 )  /* External Port , USB */
#define WAKEUP_MMCSLOT  ( 1 << 5 )  /* MMC Card Slot ( ignored ... ) */
#define WAKEUP_TOUCH    ( 1 << 6 )  /* Touch Panel  ( ignored ... )  */
#define WAKEUP_ANYKEY   ( 1 << 7 )  /* Key ( see bit16 or upper )  */
#define WAKEUP_BATCOV   ( 1 << 8 )  /* Battery Cover Open */
#define WAKEUP_CHARGE   ( 1 << 9 )  /* Charger Full ( ignored ...) */
#define WAKEUP_RTC      ( 1 << 10 ) /* RTC event */

#define WAKEUP_BATCRIT  ( 1 << 15 ) /* Battery Critical State. Need Resume and OFF */

#define WAKEUP_PHONE    ( 1 << 16 ) /* Phone Key Only Resume ... GSM Phone Mode */
#define WAKEUP_PDA      ( 1 << 17 ) /* Phone and Main Key Resume ... PDA Mode */
#define WAKEUP_ST_DISP  ( 1 << 18 ) /* Disp Key Resume from Standby */
#define WAKEUP_ST_MAIN  ( 1 << 19 ) /* Main Key Resume from Standby */
#define WAKEUP_ST_PHONE ( 1 << 20 ) /* Phone Key Resume from Standby */


#define WAKEUP_SRC_GA_MASK   0x3ff
#define WAKEUP_SRC_GA_SHIFT  0


#endif /* IRIS_APM_H_INCLUDED */

