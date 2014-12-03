/* $XConsortium: MQ_utils.c $ */

/*
 MediaQ driver - utilities
*/

#define ErrorF printk
/* All drivers should typically include these */
//#include "xf86.h"
//#include "xf86_OSproc.h"
//#include "xf86Resources.h"

/* All drivers need this */
//#include "xf86_ansic.h"

/*#include "compiler.h"*/

/* Drivers for PCI hardware need this */
//#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
//#include "xf86Pci.h"

/* All drivers initialising the SW cursor need this */
//#include "mipointer.h"

/* All drivers implementing backing store need this */
//#include "mibstore.h"

//#include "micmap.h"

//#include "xf86RAC.h"


/*
 * If using cfb, cfb.h is required.  Select the others for the bpp values
 * the driver supports.
 */

#define DUMP_MQ200_REG		/* register dump for debugging */

//#include "mq.h"
//#include "mqplat.h"
#include "mq2hw.h"
//#include "mqproto.h"
//#include "mqdata.h"

//#include "xaa.h"
//#include "xf86cmap.h"
//#include "shadowfb.h"
//#include "fbdevhw.h"


//#include "mq_utils.h"

/* PLL1 data */
#define PLL1_83MHZ			0x0EF2082A
#define DEF_MIU2_83MHZ			0x4143E086
#define PLL1_50MHZ			0x0B200A2A
#define DEF_MIU2_50MHZ			0x40C30086

/* Miscellaneous default data */
#define DEF_D1				0x05000271
#define DEF_D2				0x00000271
#define DEF_MIU3			0x6D6AABFF
#define DEF_MIU4			0x00000001
#define DEF_MIU5			0x0000010D
#define DEF_GPO_CONTROL			0x00000000
#define DEF_GPIO_CONTROL		0x00000000
#define DEF_PWM_CONTROL			0x00005CA1

#define MQ_DELAY(val)   {int delay; for (delay=0; delay<10000*val; delay++);}

void my_setmq_gc1(void);
void my_setmq_gc2(void);
void my_setmq_fpc(void);
void my_setmq_cp(void);

/* Setmode for MediaQ chip
 *
 */
//void setmqmode(PDISPLAY_CONFIG pDC, void *pMQMMIO)
void
setmqmode(/*PDISPLAY_CONFIG pDC, MQPtr pMQ*/void)
{
    unsigned long regdata, pmmisc;
    int x, y, freq, paneltype;
    unsigned long screensize, gc_startaddr;
//    void *pMQMMIO = pMQ->IOBase;

    /* ErrorF("setmqmode:x=%d,y=%d,bpp=%d,flag=%x,ref=%d,IOBase=%x\n",
    	pDC->x, pDC->y, pDC->bpp, pDC->flag, pDC->refresh, pMQMMIO); */
    regdata = READ32(PCI_VENDOR_DEVICE);
    ErrorF("PCI_VENDOR_DEVICE=0x%08x\n",regdata);
    if( regdata == 0x02004d51 )
        ErrorF("....ok\n");
    else
        ErrorF("....error, 0x02004d51\n");

    /* use 50MHz for LCD only and 83MHz if CRT is on */
//    if (pDC->flag & CRT_ON)
	regdata = PLL1_83MHZ;
//    else
//        regdata = PLL1_50MHZ;
    REG32(DC_0, regdata);
    MQ_DELAY(5);

    /* Enter D0 state from reset D3 state */
    REG32(PCI_PM_CNTL_STATUS, ENTER_D0);
    MQ_DELAY(30);
    /*
    while (1)
    {
        regdata = READ32(PCI_PM_CNTL_STATUS);
	if ((regdata & 3) == 3)
	    break;
    }*/

    ErrorF("setmqmode: in D0 state...\n");
    ErrorF("DC_1 : 0x%08x\n",READ32(DC_1));
    /* In stable D0 state here ... */

    /* Set up PMU misc registers 
    * - also PMCLK_2048CYCLE and FP_PMCLK_128K if SA1110
    */
/*    if ((READ32(DC_1) & BUS_MODE_MASK) == BUS_MODE_SA1110) */
#ifdef CONFIG_SA1100_PANGOLIN
	regdata = GE_ENABLE|GE_BY_PLL1|PMCLK_16CYCLE|FP_PMCLK_1024;
#else
	regdata = GE_ENABLE|GE_BY_PLL1|PMCLK_2048CYCLE|FP_PMCLK_128K;
#endif
/*    else */
	/* the rest of CPUs */
/*	regdata = GE_ENABLE|GE_BY_PLL1;*/
    REG32(PM_MISC, regdata);

    REG32(D1_STATE, DEF_D1);
    REG32(D2_STATE, DEF_D2);

    /* To initialize MIU block ... */
    REG32(MIU_CONTROL1, DRAM_RESET_DISABLE);
    MQ_DELAY(5);

    REG32(MIU_CONTROL1, 0x00);
    MQ_DELAY(5);

//    if (pDC->flag & CRT_ON)
	regdata = DEF_MIU2_83MHZ;
//    else
//	regdata = DEF_MIU2_50MHZ;
    REG32(MIU_CONTROL2, regdata);
    REG32(MIU_CONTROL3, DEF_MIU3);
    /* MIU REG 5 MUST BE PROGRAMMED BEFORE MIU REG 4 */
    REG32(MIU_CONTROL5, DEF_MIU5);
    REG32(MIU_CONTROL4, DEF_MIU4);
    MQ_DELAY(5);

    REG32(MIU_CONTROL1, MIU_ENABLE | MIU_RESET_DISABLE);
    MQ_DELAY(5);

    /* Here, MIU is supposed to ready to serve ... */
#ifdef CONFIG_SA1100_PANGOLIN
    my_setmq_gc1();
    my_setmq_fpc();
#else
    my_setmq_gc1();
    my_setmq_gc2();
    my_setmq_fpc();
    my_setmq_cp();
#endif

    /* enable PPL2 and set PPL2 parameters */
#ifdef CONFIG_SA1100_PANGOLIN
    REG32(PLL2_CONTROL, 0x00e90830);
#else
    REG32(PLL2_CONTROL, 0x00a30930);
#endif
    REG32(PM_MISC, 0x0904);
#if 0
    /* Initialize usablesize with max buffer minus 1KB for cursor */
    pMQ->FbUsableSize = pMQ->FbMapSize - 1024;
    gc_startaddr = 0;	//GC start address always 0 for now!
    setup_cursor((MQ200_FB_SIZE - 1024), pMQMMIO);

    /* Set up flat panel parameters
    *
    */
    paneltype = pDC->flag & PANEL_TYPE_MASK;
    if (paneltype || fpControlData[0].x)
    {
	/* Panel is used as a display in the system */
	setupfp(paneltype, pMQMMIO);

	/* Set up DSTN half frame buffer */
	screensize = pDC->x * pDC->y * pDC->bpp / 8;
	setuphfbuffer(paneltype, screensize, &(pMQ->FbUsableSize), pMQMMIO);

	/* Get flat panel frequency - paneltype is offset by 1 */
	//freq = fpControlData[paneltype - 1].freq;
	/* paneltype 0 is for OEM specific */
	freq = fpControlData[paneltype].freq;
    }
    else
        freq = (int)pDC->refresh;

    /* Based on display configuration, proper GC is set up .. */
    x = pDC->x;
    y = pDC->y;

    /* Set up GC memory configuration */
    setupgcmem(pDC, gc_startaddr, pMQMMIO);
	
    pmmisc = READ32(PM_MISC);

    /* Set up 2 GCs */
    if (pDC->flag & USE_2GCs)
    {
        ErrorF("setmqmode:2 gcs...\n");
	/* Set up GC1 for CRT */
	setupgc(IS_GC1, x, y, pDC->bpp, pDC->refresh, pMQMMIO);

	/* Set up GC2 for flat panel */
	setupgc(IS_GC2, x, y, pDC->bpp, freq, pMQMMIO);

	/* PLL2 and PLL3 are both used ... */
	/* to save a little bit power, can shut down PLL3 if both LCD
	   and CRT are the same frequency...
	*/
	pmmisc |= (PLL2_ENABLE | PLL3_ENABLE);
	REG32(PM_MISC, pmmisc);
		
	/* Enable panel and CRT accordingly */
	if (pDC->flag & LCD_ON)
	    onoffdisplay(ENABLE_LCD_GC2, pMQMMIO);
			
	if (pDC->flag & CRT_ON)
	    onoffdisplay(ENABLE_CRT_GC1, pMQMMIO);
    }
    else
    {
	/* Simultaneous mode - set up GC1 only */
    	ErrorF("setmqmode:simul... %d, %d, %d\n",
			paneltype, freq, pDC->refresh);
	if (paneltype)
	    setupgc(IS_GC1, x, y, pDC->bpp, freq, pMQMMIO);
	else
	    setupgc(IS_GC1, x, y, pDC->bpp, pDC->refresh, pMQMMIO);
    	ErrorF("setmqmode:simul setupgc done...\n");

	/* Use PLL2 */
	pmmisc |= PLL2_ENABLE;
	REG32(PM_MISC, pmmisc);

    	ErrorF("setmqmode:simul before onoffdisplay done...\n");
	/* Enable panel and CRT accordingly */
	if (pDC->flag & LCD_ON)
	    onoffdisplay(ENABLE_LCD_GC1, pMQMMIO);
			
	if (pDC->flag & CRT_ON)
	    onoffdisplay(ENABLE_CRT_GC1, pMQMMIO);
    	ErrorF("setmqmode:end simul...\n");
    }

#ifdef DUMP_MQ200_REG
    dump_mq200(pMQMMIO);
#endif
#endif
}

#if 0
/* Set up flat panel register depending on panel type
 *
 */
void
setupfp(int panel, void *pMQMMIO)
{
    PFPDATA_CONTROL pFP;
    int frcaddr, frcidx, i;

    /* Locate panel data pointer */
    pFP = &fpControlData[panel - 1];
    REG32(FP_CONTROL, pFP->fpControl);
    REG32(FP_PIN_CONTROL, pFP->fpPinControl);
    REG32(STN_CONTROL, pFP->stnControl);
    REG32(FP_GPO_CONTROL, DEF_GPO_CONTROL);
    REG32(FP_GPIO_CONTROL, DEF_GPIO_CONTROL);
    REG32(PWM_CONTROL, DEF_PWM_CONTROL);

    /* Program FRC registers for STN panel (DSTN and SSTN) */
    frcidx = 0; /* DSTN */
    if ( (pFP->fpControl & FP_TYPE_MASK) != FP_TYPE_TFT )
    {
	if ((pFP->fpControl & FP_TYPE_MASK) == FP_TYPE_SSTN)
	    frcidx++; /* SSTN index */

	for ( i = frcaddr = 0; i < FRC_PATTERN_CNT; i++,frcaddr+=4 )
	    REG32((FRC_PATTERN + frcaddr),
                                FRCControlData[frcidx].frcPattern[i]);

	for ( i = frcaddr = 0; i < FRC_WEIGHT_CNT; i++,frcaddr+=4 )
  	    REG32((FRC_WEIGHT + frcaddr), FRCControlData[frcidx].frcWeight[i]);
    }
	
    /* Set up flat panel GPO and GPIO register from default */
    REG32(FP_GPO_CONTROL, DEF_GPO_CONTROL);
    REG32(FP_GPIO_CONTROL, DEF_GPIO_CONTROL);

    return;
}

unsigned long
computehfbuffer(PFPDATA_CONTROL pFP)
{
    unsigned long dstnfbsize;

    /* Color D-STN memory requirement - no need to *3 for mono dstn panel */
    if (pFP->fpControl & FP_MONO) 
	dstnfbsize = pFP->x;
    else
	dstnfbsize = pFP->x * 3;
    dstnfbsize = (((dstnfbsize + 127) >> 7) * pFP->y) << 3;
 
    return dstnfbsize;
}

/* Set up DSTN half frame buffer register depending on panel type
 *
 * panel : panel type
 * sizeused : used (occupied) area of frame buffer
 *
 */
void
setuphfbuffer(int panel, unsigned long sizeused,
		   unsigned long *usable, void *pMQMMIO)
{
    PFPDATA_CONTROL pFP;
    unsigned long dstnfbsize, dstnstart, dstnend;

    /* Locate panel data pointer */
    //pFP = &fpControlData[panel - 1];
    pFP = &fpControlData[panel];

    /* Figure out half frame buffer for DSTN panel */
    if ( (pFP->fpControl & FP_TYPE_MASK) == FP_TYPE_DSTN )
    {
        dstnfbsize = computehfbuffer( pFP );

	/* make it suitable for register bits definition */
#if 1	//HSU: Point dstn buffer to bottom of frameBuffer
	dstnstart = (MQ200_FB_SIZE - dstnfbsize - 1024) & ~127;
		//aligned to closest 128bit on the left, on top of cursor buf
	dstnend = (dstnstart + dstnfbsize + 15) >> 4;
	*usable -= (MQ200_FB_SIZE - dstnstart);
	ErrorF("DSTN:%x,%x,%x\n", *usable, dstnstart, dstnend);
	dstnstart = dstnstart >> 7;
#else
  	dstnstart = (sizeused + 127) >> 7;
  	dstnend = (sizeused + dstnfbsize + 15) >> 4;
#endif
  	REG32(DSTN_FB_CONTROL, (dstnstart | ((dstnend - 1) << 16)));
    }
    return;
}

/* Set up graphics controller (GC1 or GC2) timing registers and PLLx
 *
 * gc: GC1 or GC2
 * x : horizontal viewport size
 * y : vertical viewport size
 * refresh : refresh rate (mainly VESA-supported mode)
 *
 */
void
setupgc(int gc, int x, int y, int bpp, int refresh, void *pMQMMIO)
{
    PDISPLAY_TIMING pDT;
    unsigned long gccontrol;

    /* Locate GC timing parameters first */
    pDT = getgcparam(x, y, refresh);
    if ( pDT == NULL )
		ErrorF( "setupgc: fail getgcparam: %d, %d, %d\r\n",
				x, y, refresh );

    /* error checking for pDT here */

    gccontrol = getbppbits(bpp) |
			FDx_1 | (1L << 24) |
			IM_ENABLE;

    if (gc == IS_GC1)
    {
		/* Set up GC window as display */
		REG32(HW1_CONTROL, ((x - 1) << 16));
		REG32(VW1_CONTROL, ((y - 1) << 16));
		
		REG32(HD1_CONTROL, pDT->hd);
		REG32(VD1_CONTROL, pDT->vd);
		REG32(HS1_CONTROL, pDT->hs);
		REG32(VS1_CONTROL, pDT->vs);
		REG32(VS1_CONTROL, pDT->vs);
		REG32(GC1_CRT_CONTROL, pDT->crtc);

		/* Program PLL2 for GC1 */
		REG32(PLL2_CONTROL, pDT->pll);

		/* GC1 control register */
		gccontrol |= GxRCLK_PLL2;
		REG32(GC1_CONTROL, gccontrol);
    }
    else if (gc == IS_GC2)
    {
	/* Set up GC window as display */
	REG32(HW2_CONTROL, ((x - 1) << 16));
	REG32(VW2_CONTROL, ((y - 1) << 16));

	REG32(HD2_CONTROL, pDT->hd);
	REG32(VD2_CONTROL, pDT->vd);
	REG32(HS2_CONTROL, pDT->hs);
	REG32(VS2_CONTROL, pDT->vs);
	REG32(VS2_CONTROL, pDT->vs);
	REG32(GC1_CRT_CONTROL, pDT->crtc);

	/* Program PLL3 for GC2 */
	REG32(PLL3_CONTROL, pDT->pll);

	/* GC2 control register */
	gccontrol |= GxRCLK_PLL3;
	REG32(GC2_CONTROL, gccontrol);
    }
    return;
}

/* Set up graphics controller (GC1 or GC2) memory configuration (stride and
 * starting address etc.)
 *
 * pDC : pointer to active DIPSLAY_CONFIG structure
 * startaddr : starting address - 0 if starting from very beginning
 *
 * - use GC1 for Simultaneous mode (1 GC)
 * - use GC1 for CRT and GC2 for LCD at QView mode
 *
 */
void
setupgcmem(PDISPLAY_CONFIG pDC, unsigned long startaddr, void *pMQMMIO)
{
    unsigned long stride, start1, start2;

    stride = pDC->stride;
    if (pDC->flag & LARGE_DESKTOP)
    {
	/* 4 possible layouts */
	switch (pDC->flag & LCDCRT_POS_MASK)
	{
	    case HORI_CRT_LCD:
		//HSU: stride = (pDC->x / 2) * pDC->bpp / 8;
		start1 = startaddr;
		start2 = startaddr + stride;
		break;

	    case HORI_LCD_CRT:
		//HSU: stride = (pDC->x / 2) * pDC->bpp / 8;
		start1 = startaddr + stride;
		start2 = startaddr;
		break;
				
	    case VERT_CRT_LCD:
		start1 = startaddr;
		//HSU: start2 = startaddr + stride * pDC->y / 2;
		start2 = startaddr + stride * pDC->y;
		break;

	    case VERT_LCD_CRT:
		//HSU: start1 = startaddr + stride * pDC->y / 2;
		start1 = startaddr + stride * pDC->y;
		start2 = startaddr;
		break;
             default:   //make it as if there's no large desktop
	        //HSU: stride = pDC->x * pDC->bpp / 8;
                start1 = start2 = startaddr;
                break;
	}

	/* Program to the chip */
	REG32(IW1_STRIDE, stride);
	REG32(IW2_STRIDE, stride);

	REG32(IW1_START_ADDR, start1);
	REG32(IW2_START_ADDR, start2);
    }
    else
    {
	/* QView Same Image and Simultaneous LCD and/or CRT
	 *
	 * - set up 2 GCs in any cases ...
	 * - 2 addidtional registers write vs. code size
	 *
	 */

	/* Calculate stride */
	//stride = pDC->x * pDC->bpp / 8;

	REG32(IW1_STRIDE, stride);
	REG32(IW2_STRIDE, stride);

	REG32(IW1_START_ADDR, startaddr);
	REG32(IW2_START_ADDR, startaddr);
    }
    return;
}

/* Program palette entry
 *
 */
void
setpal(int index, unsigned long color, void *pMQMMIO)
{
    unsigned long gccontrol;
    unsigned long mqcolor;
#ifdef MQ_COLOR_RGB
    unsigned char r,g,b;
#endif

    gccontrol = READ32(GC2_CONTROL);

    /* Ensure vertical blank in progress before DAC access if both GCs on */
    /*
    if (gccontrol & GC_ENABLE)
    {
        REG32(INT_MASK_REG,UM_GC1_VDE_F);
	invblank(pMQMMIO);
    }*/

    /* Program palette */
    mqcolor = color;
#ifdef MQ_COLOR_RGB
    r = GETR(color);
    g = GETG(color);
    b = GETB(color);
    mqcolor = MAKERGB(r, g, b);
#endif /* MQ_COLOR_RGB */
    REG32_PAL(index, mqcolor);

    /*
    if (gccontrol & GC_ENABLE)
	REG32(INT_MASK_REG,0);*/
}

/* Vertical blank time is in progress ..
 *
 */
#pragma optimize ("",off) /* This is required to not let compiler mess up */
void
invblank(void *pMQMMIO)
{
    unsigned long *intstat = (unsigned long *)((ULONG)pMQMMIO+INT_STATUS_REG);

    /* Make sure int occurs first */
    while ( !(*intstat & ST_GC1_VDE_F) );

    /* Reset GC1 VDE F status bit - write 1 to clear the status */
    REG32(INT_STATUS_REG,ST_GC1_VDE_F);

    /* Wait for next VDE falling edge for DAC access */
    while ( !(*intstat & ST_GC1_VDE_F) );

    /* Here MQ200 should be in V blank period ... */
    return;
}
#pragma optimize ("",on)

/* Retrive graphics controller parameters from supported table
 *
 */
PDISPLAY_TIMING
getgcparam(int x, int y, int refresh)
{
    int i;

    for (i=0; i < MAX_MQMODE; i++)
    {
	if ( TimingParam[i].x == x
		&& TimingParam[i].y == y
		&& TimingParam[i].refresh == refresh )
	    return ( (PDISPLAY_TIMING)&TimingParam[i] );
    }
    return (NULL);		/* not existed */
}

/* Return color depth setting for GC
 *
 */
unsigned long
getbppbits(int bpp)
{
    unsigned long bppbits;

    switch(bpp)
    {
        default:
	case  8UL: bppbits = GC_8BPP;	        break;
	case 16UL: bppbits = GC_16BPP_BP;	break;
	case 24UL: bppbits = GC_24BPP_BP;	break;
	//case 32UL: bppbits = GC_32BPP_ARGB_BP;	break;
	case 32UL: bppbits = GC_32BPP_ABGR_BP;	break;	//HSU: for linux only
	case  4UL: bppbits = GC_4BPP;	        break;
	case  2UL: bppbits = GC_2BPP;	        break;
	case  1UL: bppbits = GC_1BPP;	        break;
    }
    return (bppbits);
}

/* Turn on/off LCD or CRT driven by either GC1 or GC2
 *
 */
void
onoffdisplay(int display_flag, void *pMQMMIO)
{
    unsigned long fpcontrol, gccontrol, crtcontrol;

    switch (display_flag)
    {
	case ENABLE_LCD_GC1:
	    /* Obtain current setting */
	    fpcontrol = READ32(FP_CONTROL) & FPI_BY_GCxMASK;
	    gccontrol = READ32(GC1_CONTROL);

	    /* Turn on GC1 first if remain disabled */
	    if (!(gccontrol & GC_ENABLE))
		REG32(GC1_CONTROL, gccontrol | GC_ENABLE);

	    /* Flat panel controlled by GC1 */
	    REG32(FP_CONTROL, fpcontrol | FPI_BY_GC1);
	    break;

	case ENABLE_LCD_GC2:
	    /* Obtain current setting */
	    fpcontrol = READ32(FP_CONTROL) & FPI_BY_GCxMASK;
	    gccontrol = READ32(GC2_CONTROL);

	    /* Turn on GC1 first if remain disabled */
	    if (!(gccontrol & GC_ENABLE))
		REG32(GC2_CONTROL, gccontrol | GC_ENABLE);

	    /* Flat panel controlled by GC1 */
	    REG32(FP_CONTROL, fpcontrol | FPI_BY_GC2);
	    break;
			
	case DISABLE_LCD_GC1:
	    /* Disable flat panel first */
	    fpcontrol = READ32(FP_CONTROL) & FPI_BY_GCxMASK;
	    REG32(FP_CONTROL, fpcontrol);

	    crtcontrol = READ32(GC1_CRT_CONTROL) & (~CRT_BY_GCxMASK);

	    /* Disable GC1 if not used for CRT */
	    if (!(crtcontrol == CRT_BY_GC1))
	    {
		gccontrol = READ32(GC1_CONTROL);
		REG32(GC1_CONTROL, gccontrol & GC_DISABLE);
	    }
	    break;

	case DISABLE_LCD_GC2:
	    /* Disable flat panel first */
	    fpcontrol = READ32(FP_CONTROL) & FPI_BY_GCxMASK;
	    REG32(FP_CONTROL, fpcontrol);

	    crtcontrol = READ32(GC1_CRT_CONTROL) & (~CRT_BY_GCxMASK);

	    /* Disable GC2 if not used for CRT */
	    if (!(crtcontrol == CRT_BY_GC2))
	    {
		gccontrol = READ32(GC2_CONTROL);
		REG32(GC2_CONTROL, gccontrol & GC_DISABLE);
	    }
	    break;

	case ENABLE_CRT_GC1:
	    /* Enable GC1 if not yet enabled */
	    gccontrol = READ32(GC1_CONTROL);
	    if (!(gccontrol & GC_ENABLE))
		REG32(GC1_CONTROL, gccontrol | GC_ENABLE);

	    /* Enable CRT by GC1 */
	    crtcontrol = READ32(GC1_CRT_CONTROL) & CRT_BY_GCxMASK;
	    REG32(GC1_CRT_CONTROL, crtcontrol | CRT_BY_GC1);
	    break;

	case ENABLE_CRT_GC2:
	    /* Enable GC2 if not yet enabled */
	    gccontrol = READ32(GC2_CONTROL);
	    if (!(gccontrol & GC_ENABLE))
		REG32(GC2_CONTROL, gccontrol | GC_ENABLE);

	    /* Enable CRT by GC2 */
	    crtcontrol = READ32(GC1_CRT_CONTROL) & CRT_BY_GCxMASK;
	    REG32(GC1_CRT_CONTROL, crtcontrol | CRT_BY_GC2);
	    break;

	case DISABLE_CRT_GC1:
	    /* Disable CRT first */
	    crtcontrol = READ32(GC1_CRT_CONTROL) & CRT_BY_GCxMASK;
	    REG32(GC1_CRT_CONTROL, crtcontrol);

	    fpcontrol = READ32(FP_CONTROL) & (~FPI_BY_GCxMASK);

	    /* Disable GC1 if not used for CRT */
	    if (!(crtcontrol == CRT_BY_GC1))
	    {
		gccontrol = READ32(GC1_CONTROL);
		REG32(GC1_CONTROL, gccontrol & GC_DISABLE);
	    }
	    break;

	case DISABLE_CRT_GC2:
	    /* Disable CRT first */
	    crtcontrol = READ32(GC1_CRT_CONTROL) & CRT_BY_GCxMASK;
	    REG32(GC1_CRT_CONTROL, crtcontrol);

	    fpcontrol = READ32(FP_CONTROL) & (~FPI_BY_GCxMASK);

	    /* Disable GC2 if not used for CRT */
	    if (!(crtcontrol == CRT_BY_GC2))
	    {
		gccontrol = READ32(GC2_CONTROL);
		REG32(GC2_CONTROL, gccontrol & GC_DISABLE);
	    }
	    break;
    }
    return;
}

/* Setup hardware cursor data area start address in the frame buffer
 *
 */
void
setup_cursor(unsigned long addr, void *pMQMMIO)
{
    //HSU: It seems like X does not specify hotspot for cursor
    REG32(HW_CURSOR1_FGCLR, CURSOR_FGCLR);
    REG32(HW_CURSOR2_FGCLR, CURSOR_FGCLR);
    REG32(HW_CURSOR1_BGCLR, CURSOR_BGCLR);
    REG32(HW_CURSOR2_BGCLR, CURSOR_BGCLR);
    REG32(HW_CURSOR1_ADDR, (addr >> 10));
    REG32(HW_CURSOR2_ADDR, (addr >> 10));
}

BOOL
MQGetOptValInteger(OptionInfoPtr table, int token, int *val)
{
    const char *s;
    if (val && (s = xf86GetOptValString(table, token)))
    {
        *val = strtoul(s, NULL, 16);
	ErrorF("Token %s: 0x%x\n", table[token].name, *val);
	return TRUE;
    }
    return FALSE;
}

BOOL
MQGetOptValULONG(OptionInfoPtr table, int token, unsigned long *val)
{
    return MQGetOptValInteger(table, token, (int *)val);
}

#ifdef DUMP_MQ200_REG
void
dump_mq200(void *pMQMMIO)
{
    unsigned long reg;

    reg = READ32(MIU_CONTROL2);
    ErrorF("MIU_CONTROL2=%x\n",reg);
    reg = READ32(MIU_CONTROL3);
    ErrorF("MIU_CONTROL3=%x\n",reg);
    reg = READ32(DC_0);
    ErrorF("DC_0=%x\n",reg);
    reg = READ32(PM_MISC);
    ErrorF("PM_MISC=%x\n",reg);
    reg = READ32(GC1_CONTROL);
    ErrorF("GC1_CONTROL=%x\n",reg);
    reg = READ32(GC1_CRT_CONTROL);
    ErrorF("GC1_CRT_CONTROL=%x\n",reg);
    reg = READ32(IW1_START_ADDR);
    ErrorF("IW1_START_ADDR=%x\n",reg);
    reg = READ32(IW1_STRIDE);
    ErrorF("IW1_STRIDE=%x\n",reg);
    reg = READ32(GC2_CONTROL);
    ErrorF("GC2_CONTROL=%x\n",reg);
    reg = READ32(FP_CONTROL);
    ErrorF("FP_CONTROL=%x\n",reg);

    return;
}
#endif
#endif
