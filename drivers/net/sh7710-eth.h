typedef volatile u64 	vu64;
typedef volatile u32 	vu32;
typedef volatile u16	vu16;
typedef volatile u8 	vu8;

/* description */
#define MAC_NUM				2		// MAC num on board
//#define DESC_NUM			16		// desc buff num  (org )
#define DESC_NUM			21		// desc buff num 
//#define DESC_BUF_SIZE		2048	// buffer size for desc(byte)
//#define DESC_BUF_SIZE		1600	// buffer size for desc(byte)
#define DESC_BUF_SIZE		1520	// buffer size for desc(byte)
#define DESC_BUF_SIZE_SHIFT DESC_BUF_SIZE << 16 // for set desc member
#define DESC_MEM_BOUNDARY	16		// desc area boundary (byte)
#define DESC_MAX_BUF_SIZE	DESC_BUF_SIZE * DESC_NUM // max buf size of desc-list

/* descpriter control */
#define TX_DESC_VALID		0x80000000 // desc is valid
#define TX_DESC_LAST		0x40000000 // last of desc-list
#define TX_DESC_FRAME_ALL	0x30000000 // this frame is all
#define TX_DESC_FRAME_TOP	0x20000000 // top of same frame this desc-list
#define TX_DESC_FRAME_LAST	0x10000000 // last of same frame this desc-list
#define TX_DESC_FRAME_MID	~TX_DESC_FRAME_ALL // mid of same frame this desc-list(off bit)
#define TX_DESC_FRAME_INIT	~TX_DESC_FRAME_ALL // clear frame index

#define RX_DESC_VALID		0x80000000 // rx desc is valid
#define RX_DESC_LAST		0x40000000 // last of desc-list
#define RX_DESC_FRAME_ALL	0x30000000 // this frame is all
#define RX_DESC_FRAME_TOP	0x20000000 // top of same frame this desc-list
#define RX_DESC_FRAME_LAST	0x10000000 // last of same frame this desc-list
#define RX_DESC_FRAME_MID	~RX_DESC_FRAME_ALL // mid of same frame this desc-list(off bit)
#define RX_DESC_FRAME_INIT	~RX_DESC_FRAME_ALL // clear frame index
#define RX_DESC_SIZE_MASK	0x0000ffff // mask value for rx total size
#define RX_DESC_SIZE_INIT	0xffff0000 // clear data size

/* private data struct */
struct sh7710_priv {
    struct net_device_stats stats;
    int status;
    int rx_packetlen;
    u8 *rx_packetdata;
    int tx_packetlen;
    u8 *tx_packetdata;
    struct sk_buff *skb;
    spinlock_t lock;
};

/* for copy & byte swap */
union sh7710_cp_sw {
	vu32	i_data;
	vu8		c_data[4];
};

/* descripter struct */
struct sh7710_desc {
	vu32	td0; // control bit
	vu32	td1; // tr/rec buf size
	vu32	td2; // tr/tex buf addr (adress is 16byte boundary)
	vu32	pad; // padding struct size = 16byte
};

/* descripter */
struct sh7710_txrxdesc {
	struct sh7710_desc mactxd[DESC_NUM];	// for transmit desc
	struct sh7710_desc macrxd[DESC_NUM];	// for receiv  desc
};

/* E-DMAC descripter structer
 *
 *                            set sh7710 E-DMAC reg
 *  alloc 16byte boundary      |                 alloc 16byte boundary
 *  |                          |                  |
 *  +->+--------------------+  +->+-----------+   +->+--------------+
 *     | MAC 0 tx desc * 16 | --> | td0 32bit |  +-> |MAC0 tx buff 0|
 *     +--------------------+     +-----------+  |   +--------------+
 *     | MAC 0 rx desc * 16 |     | td1 32bit |  |   |MAC0 tx buff 1|
 *     +--------------------+     +-----------+  |   +--------------+
 *     | MAC 1 tx desc * 16 |     | td2 32bit | -+   |       :      |
 *     +--------------------+     +-----------+      :       :      :
 *     | MAC 1 rx desc * 16 |     | pad 32bit |      |       :      |
 *     +--------------------+     +-----------+      +--------------+
 *                                                   |MAC1 rx buff n|
 *                                                   +--------------+
 *           * td2 addr = physical addr
 */

/* Ether control reg */
#define ETHER_C_0_BASE	0xa7000160	// MAC0 BASE
#define ETHER_C_1_BASE	0xa7000560	// MAC1 BASE
#define ETHER_C_ARSTR	0xa7000800  // Software Reset

#define ARSTR_SW_RST	0x00000001	// Ether-C software reset

/* Ether-C control reg offset (MAC0/1) */
#define ECMR			0x00		// Mode reg
#define ECSR			0x04		// ststus reg
#define ECSIPR			0x08		// leave interrupt
#define PIR				0x0c		// for PHY interface 
#define MAHR			0x10		// set MAC address upper 32 bit
#define MALR			0x14		// set MAC address lower 16 bit
#define RFLR			0x18		// receive frame length setting 1518 - 2048 byte
#define PSR				0x1c
#define TROCR			0x20
#define CDCR			0x24
#define LCCR			0x28
#define CNDCR			0x2c
#define CEFCR			0x34
#define FRECR			0x38
#define TSFRCR			0x3c
#define TLFRCR			0x40
#define RFCR			0x44
#define MAFCR			0x48
#define IPGR			0x54

/* Ether-C reg setting value */
/* ECMR */
#define ECMR_MULTICAST	0x00002000	// multicast frame receive leave
#define ECMR_RX			0x00000040	// rx leave
#define ECMR_TX			0x00000020	// tx leave
#define ECMR_LB_INSIDE	0x00000008	// loopback inside leave
#define ECMR_LB_OUT_HI	0x00000004	// loopback outside Hi-level leave
#define ECMR_DUP_FULL	0x00000002	// Duplex mode full (half = 0)
#define ECMR_PROMISCAS	0x00000001	// promiscuous mode (normal = 0)
#define ECMR_INIT		ECMR_RX | ECMR_TX  | ECMR_DUP_FULL

/* ECMR */
#define ECSIPR_LINK		0x00000004	// link change irq leave
#define ECSIPR_MP		0x00000002	// Magic packet irq leave
#define ECSIPR_IC		0x00000001	// incorrect carryer irq leave
#define ECSIPR_INIT		0x0			// none

/* RFLR */
#define RFLR_LEN2048	0x00000fff	// receive frame data length 2048 byte

#define ETH_TSU_BASE	ETHER_C_ARSTR	// TSU base ( = ETHER_C_ARSTR)
#define FWSLC			0x38		// forward select control
#define FWSLC_INIT		0x00000041	// refer to CAMSENn forward only

/* E-DMAC reg */
#define EDMAC0_BASE		0xa7000000  // E-DMAC0 BASE
#define EDMAC1_BASE		0xa7000400  // E-DMAC1 BASE

/* E-DMAC reg offset */
#define EDMR			0x00		// E-DMAC mode 
#define EDTRR			0x04
#define EDRRR			0x08
#define TDLAR			0x0c
#define RDLAR			0x10
#define EESR			0x14
#define EESIPR			0x18
#define TRSCER			0x1c
#define RMFCR			0x20
#define TFTR			0x24		// transmit FIFO threshhold size
#define FDR				0x28		// (tx / rx)FIFO volume
#define RMCR			0x2c
#define EDOCR			0x30
#define FCFTR			0x34
#define RPADIR			0x38
#define TRIMD			0x3c
#define RBWAR			0x40
#define RDFAR			0x44
#define TBRAR			0x4c
#define TDFAR			0x50

/* E-DMAC control */
/* FCFTR rx FIFO overflow thresh hold size */
#define FCFTR_FR_NUM_1		0x00000000	// rx frame num 1
#define FCFTR_FR_NUM_2		0x00010000	// rx frame num 2
#define FCFTR_FR_NUM_3		0x00020000	// rx frame num 3
#define FCFTR_FR_NUM_4		0x00030000	// rx frame num 4
#define FCFTR_FR_NUM_5		0x00040000	// rx frame num 5
#define FCFTR_FR_NUM_6		0x00050000	// rx frame num 6
#define FCFTR_FR_NUM_7		0x00060000	// rx frame num 7
#define FCFTR_FR_NUM_8		0x00070000	// rx frame num 8

#define FCFTR_SIZE_256		0x00000006	// 256  - 32 byte
#define FCFTR_SIZE_512		0x00000006	// 512  - 32 byte
#define FCFTR_SIZE_768		0x00000006	// 768  - 32 byte
#define FCFTR_SIZE_1024		0x00000006	// 1024 - 32 byte
#define FCFTR_SIZE_1280		0x00000006	// 1280 - 32 byte
#define FCFTR_SIZE_1536		0x00000006	// 1536 - 32 byte
#define FCFTR_SIZE_1792		0x00000006	// 1792 - 32 byte
#define FCFTR_SIZE_2048		0x00000006	// 2048 - 64 byte

/* EDTRR */
#define EDTRR_TX_REQ		0x1	// transmit request to E-DMAC

/* EDRRR */
#define EDRRR_RX_REQ		0x1 // receive request to E-DMAC

/*  EDMR desc size */
#define EDMR_DESC16			0x00000000 // 16 byte
#define EDMR_DESC32			0x00000010 // 32 byte
#define EDMR_DESC64			0x00000020 // 64 byte
#define EDMR_SW_RST			0x00000001 // EDMAC software reset

/* TFTR */
//#define TFTR_SIZE			1024>>2				// transmit FIFO threshhold size
#define TFTR_SIZE			DESC_BUF_SIZE>>2	// transmit FIFO threshhold size

/* FDR */
#define FDR_TX_VOL			0x00000700	// transmit FIFO volume 2k
#define FDR_RX_VOL			0x00000007	// receive FIFO volume 2k

/* RMCR */
#define RMCR_STOP			0x00000000	// stop receive frame
#define RMCR_CONT			0x00000001	// continue receive frame

/* EESIPR state irq leave reg */
#define	EESIPR_WB			0x40000000	// write-back leave 
#define	EESIPR_TX_INT		0x04000000	// tx interrupt irq leave 
#define	EESIPR_RX_INT		0x02000000	// rx interrupt irq leave 
#define	EESIPR_RX_OFLOW		0x01000000	// rx counter over flow irq leave 
#define	EESIPR_ADDR_ERR		0x00800000	// addr err irq leave 
#define	EESIPR_STATE_INT	0x00400000	// Eth-C state reg  irq leave 
#define	EESIPR_TX_DONE		0x00200000	// frame tx done irq leave 
#define	EESIPR_TX_DESC_NONE	0x00100000	// tx desc drain irq leave 
#define	EESIPR_TX_FIFO_UFLW	0x00080000	// tx FIFO under flow irq leave 
#define	EESIPR_RX			0x00040000	// rx irq leave 
#define	EESIPR_RX_DESC_NONE	0x00020000	// rx desc drain irq leave 
#define	EESIPR_RX_FIFO_OFLW	0x00010000	// rx FIFO over flow irq leave 
#define	EESIPR_CARY_NONE	0x00000800	// carryer none  irq leave 
#define	EESIPR_CARY_LOST	0x00000400	// carryer lost  irq leave 
#define	EESIPR_COLLISION	0x00000200	// collision  irq leave 
#define	EESIPR_TX_RET_OVR	0x00000100	// tx retry over  irq leave 
#define	EESIPR_RX_MULTI_CAS	0x00000080	// multicast frame receive  irq leave 
#define	EESIPR_RX_FRACT		0x00000010	// fraction bit frame receive  irq leave 
#define	EESIPR_RX_LONG		0x00000008	// long frame receive  irq leave 
#define	EESIPR_RX_SHORT		0x00000004	// short frame receive  irq leave 
#define	EESIPR_PHY_RX_ERR	0x00000002	// PHY rx err  irq leave 
#define	EESIPR_PHY_CRC_ERR	0x00000001	// PHY CRC err  irq leave 
#define EESIPR_INIT			\
							EESIPR_WB			| \
							EESIPR_TX_INT		| \
							EESIPR_RX_INT		| EESIPR_RX_OFLOW		| \
							EESIPR_ADDR_ERR		| EESIPR_STATE_INT		| \
							\
							EESIPR_TX_DESC_NONE	| \
							EESIPR_TX_DONE		| \
							\
							EESIPR_TX_FIFO_UFLW	| EESIPR_RX				| \
							EESIPR_RX_DESC_NONE	| EESIPR_RX_FIFO_OFLW	| \
							EESIPR_CARY_NONE	| EESIPR_CARY_LOST		| \
							EESIPR_COLLISION	| EESIPR_TX_RET_OVR		| \
							EESIPR_RX_MULTI_CAS	| EESIPR_RX_FRACT		| \
							EESIPR_RX_LONG		| EESIPR_RX_SHORT		| \
							EESIPR_PHY_RX_ERR	| EESIPR_PHY_CRC_ERR

/* EESR  : Ether-C/EDMAC status */
#define EESR_TX_WB			EESIPR_WB			// tx done
#define EESR_TX_DONE		EESIPR_TX_DONE		// tx done
#define EESR_RX_DONE		EESIPR_RX			// finish receive a frame
#define EESR_RX_MULTI		EESIPR_RX_MULTI_CAS	// receive multicast frame
#define EESR_RX_SUSPEND		EESIPR_RX_INT		// receive frame is suspended
#define EESR_RX_FIFO_OFLW	EESIPR_RX_FIFO_OFLW	// rx FIFO over flow
#define EESR_TX_FIFO_UFLW	EESIPR_TX_FIFO_UFLW	// tx FIFO under flow
#define EESR_RX_DESC_NONE	EESIPR_RX_DESC_NONE	// rx desc none(drain)
#define EESR_RX_CNT_OFLW	EESIPR_RX_OFLOW		// rx counter over flow


/* TRIMD */
#define TRIMD_TX_NOWB		0x00000000	// write-back/frame INT off
#define TRIMD_TX_WB			0x00000001	// write-back/frame INT on

/* for statusword (priv->status) */
#define SH7710_RX_INTR 0x1
#define SH7710_TX_INTR 0x2

/* MII control data */
#define PRE_OPERATION_CODE	0xffffffff	// MII pre-operation code 
#define AFTER_IDLE_CODE		0x00000000	// MII after-idle code 
#define MII_MANAGE_W		0x00000002	// MII data write mode
#define MII_MANAGE_R		0x00000000	// MII data read mode
#define MII_MANAGE_CL		0x00000001	// MII data manage clock
#define MII_WRITE_1			0x00000004	// MII write 1 
#define MII_WRITE_0			0x00000000	// MII write 0 
#define MII_CTRL_FRAME_TOP	0x4000		// MII control frame top
#define MII_CTRL_OP_W		0x1000		// MII control write
#define MII_CTRL_OP_R		0x2000		// MII control read
#define MII_CTRL_PHY_ADDR	0x0000		// MII control PHY addr
#define MII_CTRL_TA_W		0x0002		// MII control I/F change (write)
#define MII_DELAY			10			// MII reg after r/w delay time (us)

/* MII control reg addr (DAVICOM/INTEL) */
#define MII_CTRL_REG_BMCR		0x0000<<2	// MII control BMCR reg addr(0)
#define MII_CTRL_REG_BMSR		0x0001<<2	// MII control BMSR reg addr(1)
#define MII_CTRL_REG_PHYID1		0x0002<<2	// MII control PHYID1 reg addr(0x02)
#define MII_CTRL_REG_PHYID2		0x0003<<2	// MII control PHYID2 reg addr(0x03)
#define MII_CTRL_REG_ANAR		0x0004<<2	// MII control ANAR reg addr(0x04)
#define MII_CTRL_REG_ANLAPR		0x0005<<2	// MII control ANLAPR reg addr(0x05)
#define MII_CTRL_REG_ANER		0x0006<<2	// MII control ANER reg addr(0x06)

#define MII_BIT_CHECK			0x8000		// for write data check

											// MII reg write operation define
#define MII_CTRL_REG_WRITE		MII_CTRL_FRAME_TOP	| \
								MII_CTRL_OP_W		| \
								MII_CTRL_PHY_ADDR

											// MII reg read operation define
#define MII_CTRL_REG_READ		MII_CTRL_FRAME_TOP	| \
								MII_CTRL_OP_R		| \
								MII_CTRL_PHY_ADDR

/* MII reg data */
#define MII_BMCR_RESET				0x8000		// PHY reset
#define MII_BMCR_AUTO_NEG_E			0x1000		// auto negotiation enable
#define MII_BMCR_AUTO_NEG_RESTART	0x0200		// auto negotiation restart
#define MII_BMCR_SPEED_100			0x2000		// set 100BASE
#define MII_BMCR_ISOLATE			0x0400		// isolate MII
												// BMCR reg write operation code
#define MII_BMCR_WRITE		MII_CTRL_REG_WRITE | \
							MII_CTRL_REG_BMCR | \
							MII_CTRL_TA_W

												// BMCR reg read operation code
#define MII_BMCR_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_BMCR

												// BMSR reg read operation code
#define MII_BMSR_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_BMSR

												// PHYID1 reg read operation code
#define MII_PHYID1_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_PHYID1
												// PHYID2 reg read operation code
#define MII_PHYID2_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_PHYID2

												// ANAR reg read operation code
#define MII_ANER_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_ANER
												// ANLAPR reg read operation code
#define MII_ANLAPR_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_ANLAPR

#define MII_ANAR_100_FULL	0x0100		// 100BASE Full Duplex support
#define MII_ANAR_100_HALF	0x0080		// 100BASE Half Duplex support
#define MII_ANAR_10_FULL	0x0040		//  10BASE Full Duplex support
#define MII_ANAR_10_HALF	0x0020		//  10BASE Half Duplex support

#define MII_ANAR_FCS		0x0400		//  Flow Control Support

					// auto negotiation (100/10BASE,full/half flow ctl support)
#define MII_ANAR_AUTO		MII_ANAR_100_FULL | \
							MII_ANAR_100_HALF | \
							MII_ANAR_10_FULL | \
							MII_ANAR_10_HALF

#define MII_ANAR_IEEE		0x0001		// select IEEE 802.3 
												// ANAR reg write operation code
#define MII_ANAR_WRITE		MII_CTRL_REG_WRITE | \
							MII_CTRL_REG_ANAR | \
							MII_CTRL_TA_W

												// ANAR reg read operation code
#define MII_ANAR_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_ANAR

#if defined (CONFIG_SH_MS7710SE)
/* **** DAVICOM DM9161E origine reg **** */
#define MII_CTRL_REG_DSCR		0x0010<<2	// MII control DSCR reg addr(0x10)
#define MII_CTRL_REG_DSCSR		0x0011<<2	// MII control DSCSR reg addr(0x11)
#define MII_CTRL_REG_10BTCSR	0x0012<<2	// MII control 10BTCSR reg addr(0x12)
#define MII_CTRL_REG_INTR		0x0015<<2	// MII control BMCR reg addr(0x15)
#define MII_CTRL_REG_RECR		0x0016<<2	// MII control RECR reg addr(0x16)
#define MII_CTRL_REG_DISCR		0x0017<<2	// MII control DISCR reg addr(0x17)
#define MII_CTRL_REG_RLSR		0x0018<<2	// MII control RLSR reg addr(0x18)

/* **** DAVICOM DM9161E origine reg **** */
												// RECR reg read operation code
#define MII_RECR_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_RECR

												// 10BTCSR reg read operation code
#define MII_10BTCSR_READ	MII_CTRL_REG_READ | \
							MII_CTRL_REG_10BTCSR

												// RLSR reg read operation code
#define MII_RLSR_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_RLSR
												// DSCR reg read operation code
#define MII_DSCR_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_DSCR
												// BMCR reg write operation code
#define MII_DSCR_WRITE		MII_CTRL_REG_WRITE | \
							MII_CTRL_REG_DSCR | \
							MII_CTRL_TA_W
												// DISCR reg read operation code
#define MII_DSCR_FEF		0x0200		// Far End Fault enable

#define MII_DISCR_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_DISCR

/* MII interrupt Reg */
#define MII_INTR_NO_MASK	0x0000	// Interrupt no mask
#define MII_FDX_MASK		0x0800	// Full Duplex Interrupt Mask
#define MII_SPD_MASK		0x0400	// Speed Interrupt Mask
#define MII_LINK_MASK		0x0200	// link Interrupt Mask
#define MII_MASTER_INTR		0x0100	// Master Interrupt Mask
									// INTR reg write-operation code
#define MII_INTR_WRITE		MII_CTRL_REG_WRITE | \
							MII_CTRL_REG_INTR | \
							MII_CTRL_TA_W

									// INTR reg read-operation code
#define MII_INTR_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_INTR

									// FDX/SPD/LINK INTR Mask
#define MII_INTR_DEFAULT	MII_FDX_MASK | \
							MII_SPD_MASK | \
							MII_LINK_MASK

						
									// DSCSR reg read-operation code
#define MII_DSCSR_READ		MII_CTRL_REG_READ | \
							MII_CTRL_REG_DSCSR

#define MII_DSCSR_WRITE		MII_CTRL_REG_WRITE | \
							MII_CTRL_REG_DSCSR | \
							MII_CTRL_TA_W
#endif // CONFIG_SH_MS7710SE

#if defined (CONFIG_SH_FGRAB)
/* **** INTEL LXT971A  origine reg **** */
#define MII_CRTL_REG_ANNPTR		0x0007<<2	// A/N Next Page Txmit Reg addr (INTEL)
#define MII_CRTL_REG_ANLNPR		0x0008<<2	// A/N Link Next Page  Reg addr (INTEL)
#define MII_CRTL_REG_PCR		0x0010<<2	// Port Config Reg addr (INTEL)
#define MII_CRTL_REG_SR2		0x0011<<2	// Starus Reg #2 addr (INTEL)
#define MII_CRTL_REG_IER		0x0012<<2	// Interrupt Enable Reg addr (INTEL)
#define MII_CRTL_REG_ISR		0x0013<<2	// Interrupt Status Reg addr (INTEL)
#define MII_CRTL_REG_LEDCR		0x0014<<2	// LED Config Reg addr (INTEL)
#define MII_CRTL_REG_DCR		0x001a<<2	// Digital Config Reg addr (INTEL)
#define MII_CRTL_REG_TCR		0x001e<<2	// Trans Control Reg addr (INTEL)

// Trans Control Reg
#define MII_TCR_READ		MII_CTRL_REG_READ | \
							MII_CRTL_REG_TCR

#define MII_TCR_WRITE		MII_CTRL_REG_WRITE | \
							MII_CRTL_REG_TCR | \
							MII_CTRL_TA_W

// Digital Config Reg
#define MII_DCR_READ		MII_CTRL_REG_READ | \
							MII_CRTL_REG_DCR

#define MII_DCR_WRITE		MII_CTRL_REG_WRITE | \
							MII_CRTL_REG_DCR | \
							MII_CTRL_TA_W

// LED Config Reg
#define MII_LEDCR_READ		MII_CTRL_REG_READ | \
							MII_CRTL_REG_LEDCR

#define MII_LEDCR_WRITE		MII_CTRL_REG_WRITE | \
							MII_CRTL_REG_LEDCR | \
							MII_CTRL_TA_W

// Interrupt Status
#define MII_ISR_READ		MII_CTRL_REG_READ | \
							MII_CRTL_REG_ISR

#define MII_ISR_WRITE		MII_CTRL_REG_WRITE | \
							MII_CRTL_REG_ISR | \
							MII_CTRL_TA_W

// Interrupt Enable
#define MII_IER_READ		MII_CTRL_REG_READ | \
							MII_CRTL_REG_IER

#define MII_IER_WRITE		MII_CTRL_REG_WRITE | \
							MII_CRTL_REG_IER | \
							MII_CTRL_TA_W

// Starus Reg #2
#define MII_SR2_READ		MII_CTRL_REG_READ | \
							MII_CRTL_REG_SR2

#define MII_SR2_WRITE		MII_CTRL_REG_WRITE | \
							MII_CRTL_REG_SR2 | \
							MII_CTRL_TA_W

// Port Config Reg
#define MII_PCR_READ		MII_CTRL_REG_READ | \
							MII_CRTL_REG_PCR

#define MII_PCR_WRITE		MII_CTRL_REG_WRITE | \
							MII_CRTL_REG_PCR | \
							MII_CTRL_TA_W

// A/N Link Next Page  Reg
#define MII_ANLNPR_READ		MII_CTRL_REG_READ | \
							MII_CRTL_REG_ANLNPR

#define MII_ANLNPR_WRITE	MII_CTRL_REG_WRITE | \
							MII_CRTL_REG_ANLNPR | \
							MII_CTRL_TA_W

// A/N Next Page Txmit Reg
#define MII_ANNPTR_READ		MII_CTRL_REG_READ | \
							MII_CRTL_REG_ANNPTR

#define MII_ANNPTR_WRITE	MII_CTRL_REG_WRITE | \
							MII_CRTL_REG_ANNPTR | \
							MII_CTRL_TA_W

/* **** INTEL LXT971A  origine reg **** */
#endif // CONFIG_SH_FGRAB

int sh7710_init(struct net_device *dev);
void sh7710_interrupt(int irq, void *dev_id, struct pt_regs *regs);
void sh7710_rx(struct net_device *dev, int len, unsigned char *buf);
void sh7710_hw_tx(char *data, int len, struct net_device *dev);
int init_sh7710_EtherC(struct net_device *dev);
int init_sh7710_EDMAC(u32 mac);
static void etherC_soft_reset(void);
void sh7710_retry_rx(struct net_device *dev);
static int sh7710_init_controller(struct net_device *dev);
static void  sh7710_set_multicast_list(struct net_device *dev);

static int  get_sh7710_mac_address(int id, unsigned char *addr);
static void PHY_write(vu32 *reg, u16 phyreg, u16 data);
static void after_idle(vu32 *reg);
static void pre_operation(vu32 *reg);
static void write_bit0(vu32 *reg);
static void write_bit1(vu32 *reg);
static void sh7710_PHY_init(u32 mac);
static void EDMAC_soft_reset(int ch);
static void memcpy_sw(vu32 *to, vu32 *from, int size);


#ifdef DEBUG
/* **** for debug ***** */
static void release_mii_bus(vu32 *reg);
static u16 PHY_read(vu32 *reg, u16 phyreg);
static void ether_reg_dump(void);
static void ether_reg_dump2(void);
static void desc_dump_all(void);
#endif
