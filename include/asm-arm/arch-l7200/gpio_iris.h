/*
 * GPIO definitions
 *
 *
 *
 */
#ifndef __ASM_ARCH_GPIO_IRIS_H
#define __ASM_ARCH_GPIO_IRIS_H

#include <asm/arch/hardware.h>

#define bwPA0  1
#define bwPA1  1
#define bwPA2  1
#define bwPA3  1
#define bwPA4  1
#define bwPA5  1
#define bwPA6  1
#define bwPA7  1

#define bwPB0  1
#define bwPB1  1
#define bwPB2  1
#define bwPB3  1
#define bwPB4  1
#define bwPB5  1
#define bwPB6  1
#define bwPB7  1

#define bwPC0  1
#define bwPC1  1
#define bwPC2  1
#define bwPC3  1
#define bwPC4  1
#define bwPC5  1
#define bwPC6  1
#define bwPC7  1

#define bwPD0  1
#define bwPD1  1
#define bwPD2  1
#define bwPD3  1
#define bwPD4  1
#define bwPD5  1
#define bwPD6  1
#define bwPD7  1

#define bwPE0  1
#define bwPE1  1
#define bwPE2  1
#define bwPE3  1
#define bwPE4  1
#define bwPE5  1
#define bwPE6  1
#define bwPE7  1

#define bwPF0  1
#define bwPF1  1
#define bwPF2  1
#define bwPF3  1
#define bwPF4  1
#define bwPF5  1
#define bwPF6  1
#define bwPF7  1

#define bsPA0  0
#define bsPA1  1
#define bsPA2  2
#define bsPA3  3
#define bsPA4  4
#define bsPA5  5
#define bsPA6  6
#define bsPA7  7

#define bsPB0  0
#define bsPB1  1
#define bsPB2  2
#define bsPB3  3
#define bsPB4  4
#define bsPB5  5
#define bsPB6  6
#define bsPB7  7

#define bsPC0  0
#define bsPC1  1
#define bsPC2  2
#define bsPC3  3
#define bsPC4  4
#define bsPC5  5
#define bsPC6  6
#define bsPC7  7

#define bsPD0  0
#define bsPD1  1
#define bsPD2  2
#define bsPD3  3
#define bsPD4  4
#define bsPD5  5
#define bsPD6  6
#define bsPD7  7

#define bsPE0  0
#define bsPE1  1
#define bsPE2  2
#define bsPE3  3
#define bsPE4  4
#define bsPE5  5
#define bsPE6  6
#define bsPE7  7

#define bsPF0  0
#define bsPF1  1
#define bsPF2  2
#define bsPF3  3
#define bsPF4  4
#define bsPF5  5
#define bsPF6  6
#define bsPF7  7

#define bitPA0 0x01
#define bitPA1 0x02
#define bitPA2 0x04
#define bitPA3 0x08
#define bitPA4 0x10
#define bitPA5 0x20
#define bitPA6 0x40
#define bitPA7 0x80

#define bitPB0 0x01
#define bitPB1 0x02
#define bitPB2 0x04
#define bitPB3 0x08
#define bitPB4 0x10
#define bitPB5 0x20
#define bitPB6 0x40
#define bitPB7 0x80

#define bitPC0 0x01
#define bitPC1 0x02
#define bitPC2 0x04
#define bitPC3 0x08
#define bitPC4 0x10
#define bitPC5 0x20
#define bitPC6 0x40
#define bitPC7 0x80

#define bitPD0 0x01
#define bitPD1 0x02
#define bitPD2 0x04
#define bitPD3 0x08
#define bitPD4 0x10
#define bitPD5 0x20
#define bitPD6 0x40
#define bitPD7 0x80

#define bitPE0 0x01
#define bitPE1 0x02
#define bitPE2 0x04
#define bitPE3 0x08
#define bitPE4 0x10
#define bitPE5 0x20
#define bitPE6 0x40
#define bitPE7 0x80

#define bitPF0 0x01
#define bitPF1 0x02
#define bitPF2 0x04
#define bitPF3 0x08
#define bitPF4 0x10
#define bitPF5 0x20
#define bitPF6 0x40
#define bitPF7 0x80

/* Port PA */
#define SET_PADR_LO(bits)     IO_PADR = (IO_PADR & ~( bits ))          /* 指定portを H に */
#define SET_PADR_HI(bits)     IO_PADR = (IO_PADR | ( bits ))           /* 指定portを L に */
#define GET_PADR(bits)        (IO_PADR & ( bits ))                     /* 指定portの値を得る */
#define SET_PA_IN(bits)       IO_PADDR = (IO_PADDR & ~( bits ))        /* 指定portを出力に */
#define SET_PA_OUT(bits)      IO_PADDR = (IO_PADDR | ( bits ))         /* 指定portを入力に */
#define ENABLE_PA_IRQ(bits)   IO_PAIMR = (IO_PAIMR | ( bits ))         /* 指定portを入力に */
#define DISABLE_PA_IRQ(bits)  IO_PAIMR = (IO_PAIMR & ~( bits ))        /* 指定portを出力に */
#define SET_PASBSR_LO(bits)   IO_PASBSR = (IO_PASBSR & ~( bits ))      /* 指定portを H に */
#define SET_PASBSR_HI(bits)   IO_PASBSR = (IO_PASBSR | ( bits ))       /* 指定portを L に */

#define SET_PAESNR_LO(bits)   IO_PAESNR = (IO_PAESNR & ~( bits ))      /* 指定portを H に */
#define SET_PAESNR_HI(bits)   IO_PAESNR = (IO_PAESNR | ( bits ))       /* 指定portを L に */
#define SET_PAEENR_LO(bits)   IO_PAEENR = (IO_PAEENR & ~( bits ))      /* 指定portを H に */
#define SET_PAEENR_HI(bits)   IO_PAEENR = (IO_PAEENR | ( bits ))       /* 指定portを L に */
#define SET_PAECLR(bits)      IO_PAECLR |= (bits)                      /* 指定portの割込みクリア */

#define ADD_PA_IRQMASK(bit)   IO_PAIMR = (IO_PAIMR | bit)
#define DEL_PA_IRQMASK(bit)   IO_PAIMR = (IO_PAIMR & ~(bit))

#define GET_PAINT(bits)       (IO_PAINT & (bits))                       /* interrupt status */

#define SET_PADE_LO(bits)     IO_PADE = (IO_PADE & ~( bits ))          /* 指定portを H に */
#define SET_PADE_HI(bits)     IO_PADE = (IO_PADE | ( bits ))           /* 指定portを L に */

static __inline__ void enable_pa_irq(unsigned int pin)
{
  ADD_PA_IRQMASK(pin);
  if( ! ( IO_IRQENABLE & ( 1 << IRQ_GPIO ) ) )
    enable_irq(IRQ_GPIO);
  return;
}

static __inline__ void disable_pa_irq(unsigned int pin)
{
  DEL_PA_IRQMASK(pin);
  /* do not disable_irq ... ?? */
  return;
}

/* Port PB */
#define SET_PBDR_LO(bits)     IO_PBDR = (IO_PBDR & ~( bits ))          /* 指定portを H に */
#define SET_PBDR_HI(bits)     IO_PBDR = (IO_PBDR | ( bits ))           /* 指定portを L に */
#define GET_PBDR(bits)        (IO_PBDR & ( bits ))                     /* 指定portの値を得る */
#define SET_PB_IN(bits)       IO_PBDDR = (IO_PBDDR | ( bits ))         /* 指定portを入力に */
#define SET_PB_OUT(bits)      IO_PBDDR = (IO_PBDDR & ~( bits ))        /* 指定portを出力に */
#define ENABLE_PB_IRQ(bits)   IO_PBIMR = (IO_PBIMR | ( bits ))         /* 指定portを入力に */
#define DISABLE_PB_IRQ(bits)  IO_PBIMR = (IO_PBIMR & ~( bits ))        /* 指定portを出力に */
#define SET_PBSBSR_LO(bits)   IO_PBSBSR = (IO_PBSBSR & ~( bits ))      /* 指定portを H に */
#define SET_PBSBSR_HI(bits)   IO_PBSBSR = (IO_PBSBSR | ( bits ))       /* 指定portを L に */

#define ADD_PB_IRQMASK(bit)   IO_PBIMR = (IO_PBIMR | bit)
#define DEL_PB_IRQMASK(bit)   IO_PBIMR = (IO_PBIMR & ~(bit))

#define GET_PBINT(bits)       (IO_PBINT & (bits))                       /* interrupt status */

static __inline__ void enable_pb_irq(unsigned int pin)
{
  ADD_PB_IRQMASK(pin);
  if( ! ( IO_IRQENABLE & ( 1 << IRQ_GPIO ) ) )
    enable_irq(IRQ_GPIO);
  return;
}

static __inline__ void disable_pb_irq(unsigned int pin)
{
  DEL_PB_IRQMASK(pin);
  /* do not disable_irq ... ?? */
  return;
}

/* Port PC */
#define SET_PCDR_LO(bits)     IO_PCDR = (IO_PCDR & ~( bits ))          /* 指定portを H に */
#define SET_PCDR_HI(bits)     IO_PCDR = (IO_PCDR | ( bits ))           /* 指定portを L に */
#define GET_PCDR(bits)        (IO_PCDR & ( bits ))                     /* 指定portの値を得る */
#define SET_PC_IN(bits)       IO_PCDDR = (IO_PCDDR | ( bits ))         /* 指定portを入力に */
#define SET_PC_OUT(bits)      IO_PCDDR = (IO_PCDDR & ~( bits ))        /* 指定portを出力に */
#define ENABLE_PC_IRQ(bits)   IO_PCIMR = (IO_PCIMR | ( bits ))         /* 指定portを入力に */
#define DISABLE_PC_IRQ(bits)  IO_PCIMR = (IO_PCIMR & ~( bits ))        /* 指定portを出力に */
#define SET_PCSBSR_LO(bits)   IO_PCSBSR = (IO_PCSBSR & ~( bits ))      /* 指定portを H に */
#define SET_PCSBSR_HI(bits)   IO_PCSBSR = (IO_PCSBSR | ( bits ))       /* 指定portを L に */

#define ADD_PC_IRQMASK(bit)   IO_PCIMR = (IO_PCIMR | bit)
#define DEL_PC_IRQMASK(bit)   IO_PCIMR = (IO_PCIMR & ~(bit))

#define GET_PCINT(bits)       (IO_PCINT & (bits))                       /* interrupt status */

static __inline__ void enable_pc_irq(unsigned int pin)
{
  ADD_PC_IRQMASK(pin);
  if( ! ( IO_IRQENABLE & ( 1 << IRQ_GPIO ) ) )
    enable_irq(IRQ_GPIO);
  return;
}

static __inline__ void disable_pc_irq(unsigned int pin)
{
  DEL_PC_IRQMASK(pin);
  /* do not disable_irq ... ?? */
  return;
}

/* Port PD */
#define SET_PDDR_LO(bits)     IO_PDDR = (IO_PDDR & ~( bits ))          /* 指定portを H に */
#define SET_PDDR_HI(bits)     IO_PDDR = (IO_PDDR | ( bits ))           /* 指定portを L に */
#define GET_PDDR(bits)        (IO_PDDR & ( bits ))                     /* 指定portの値を得る */
#define SET_PD_IN(bits)       IO_PDDDR = (IO_PDDDR & ~( bits ))        /* 指定portを出力に */
#define SET_PD_OUT(bits)      IO_PDDDR = (IO_PDDDR | ( bits ))         /* 指定portを入力に */
#define ENABLE_PD_IRQ(bits)   IO_PDIMR = (IO_PDIMR | ( bits ))         /* 指定portを入力に */
#define DISABLE_PD_IRQ(bits)  IO_PDIMR = (IO_PDIMR & ~( bits ))        /* 指定portを出力に */
#define SET_PDSBSR_LO(bits)   IO_PDSBSR = (IO_PDSBSR & ~( bits ))      /* 指定portを H に */
#define SET_PDSBSR_HI(bits)   IO_PDSBSR = (IO_PDSBSR | ( bits ))       /* 指定portを L に */

#define ADD_PD_IRQMASK(bit)   IO_PDIMR = (IO_PDIMR | bit)
#define DEL_PD_IRQMASK(bit)   IO_PDIMR = (IO_PDIMR & ~(bit))

#define GET_PDINT(bits)       (IO_PDINT & (bits))                       /* interrupt status */

static __inline__ void enable_pd_irq(unsigned int pin)
{
  ADD_PD_IRQMASK(pin);
  if( ! ( IO_IRQENABLE & ( 1 << IRQ_GPIO ) ) )
    enable_irq(IRQ_GPIO);
  return;
}

static __inline__ void disable_pd_irq(unsigned int pin)
{
  DEL_PD_IRQMASK(pin);
  /* do not disable_irq ... ?? */
  return;
}

/* Port PE */
#define SET_PEDR_LO(bits)     IO_PEDR = (IO_PEDR & ~( bits ))          /* 指定portを H に */
#define SET_PEDR_HI(bits)     IO_PEDR = (IO_PEDR | ( bits ))           /* 指定portを L に */
#define GET_PEDR(bits)        (IO_PEDR & ( bits ))                     /* 指定portの値を得る */
#define SET_PE_IN(bits)       IO_PEDDR = (IO_PEDDR & ~( bits ))        /* 指定portを出力に */
#define SET_PE_OUT(bits)      IO_PEDDR = (IO_PEDDR | ( bits ))         /* 指定portを入力に */
#define ENABLE_PE_IRQ(bits)   IO_PEIMR = (IO_PEIMR | ( bits ))         /* 指定portを入力に */
#define DISABLE_PE_IRQ(bits)  IO_PEIMR = (IO_PEIMR & ~( bits ))        /* 指定portを出力に */
#define SET_PESBSR_LO(bits)   IO_PESBSR = (IO_PESBSR & ~( bits ))      /* 指定portを H に */
#define SET_PESBSR_HI(bits)   IO_PESBSR = (IO_PESBSR | ( bits ))       /* 指定portを L に */

#define ADD_PE_IRQMASK(bit)   IO_PEIMR = (IO_PEIMR | bit)
#define DEL_PE_IRQMASK(bit)   IO_PEIMR = (IO_PEIMR & ~(bit))

#define GET_PEINT(bits)       (IO_PEINT & (bits))                       /* interrupt status */

static __inline__ void enable_pe_irq(unsigned int pin)
{
  ADD_PE_IRQMASK(pin);
  if( ! ( IO_IRQENABLE & ( 1 << IRQ_GPIO ) ) )
    enable_irq(IRQ_GPIO);
  return;
}

static __inline__ void disable_pe_irq(unsigned int pin)
{
  DEL_PE_IRQMASK(pin);
  /* do not disable_irq ... ?? */
  return;
}

/* Port PF */
#define SET_PFDR_LO(bits)     IO_PFDR = (IO_PFDR & ~( bits ))          /* 指定portを H に */
#define SET_PFDR_HI(bits)     IO_PFDR = (IO_PFDR | ( bits ))           /* 指定portを L に */
#define GET_PFDR(bits)        (IO_PFDR & ( bits ))                     /* 指定portの値を得る */
#define SET_PF_IN(bits)       IO_PFDDR = (IO_PFDDR & ~( bits ))        /* 指定portを出力に */
#define SET_PF_OUT(bits)      IO_PFDDR = (IO_PFDDR | ( bits ))         /* 指定portを入力に */
#define ENABLE_PF_IRQ(bits)   IO_PFIMR = (IO_PFIMR | ( bits ))         /* 指定portを入力に */
#define DISABLE_PF_IRQ(bits)  IO_PFIMR = (IO_PFIMR & ~( bits ))        /* 指定portを出力に */
#define SET_PFSBSR_LO(bits)   IO_PFSBSR = (IO_PFSBSR & ~( bits ))      /* 指定portを H に */
#define SET_PFSBSR_HI(bits)   IO_PFSBSR = (IO_PFSBSR | ( bits ))       /* 指定portを L に */

#define ADD_PF_IRQMASK(bit)   IO_PFIMR = (IO_PFIMR | bit)
#define DEL_PF_IRQMASK(bit)   IO_PFIMR = (IO_PFIMR & ~(bit))

#define GET_PFINT(bits)       (IO_PFINT & (bits))                       /* interrupt status */

static __inline__ void enable_pf_irq(unsigned int pin)
{
  ADD_PF_IRQMASK(pin);
  if( ! ( IO_IRQENABLE & ( 1 << IRQ_GPIO ) ) )
    enable_irq(IRQ_GPIO);
  return;
}

static __inline__ void disable_pf_irq(unsigned int pin)
{
  DEL_PF_IRQMASK(pin);
  /* do not disable_irq ... ?? */
  return;
}

/*
 *  for Debug Port Registers
 */

int get_debug_port_data(void);
void clear_debug_port_data(void);
void add_debug_port_data(unsigned char pin);
void del_debug_port_data(unsigned char pin);
void set_debug_port_data(unsigned char pin);

#endif
