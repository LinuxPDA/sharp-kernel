/*

for a 32MB SDRAM:

  romvec : ORIGIN = 0x01e00000, LENGTH = 1k
  flash  : ORIGIN = 0x01e00400, LENGTH = 2M - 1k
  eflash : ORIGIN = 0x02000000, LENGTH = 0
  ramvec : ORIGIN = 0x00000000, LENGTH = 1k
  ram    : ORIGIN = 0x00000400, LENGTH = 30M - 1k
  eram   : ORIGIN = 0x01e00000, LENGTH = 0

for a 16MB SDRAM:

  romvec : ORIGIN = 0x00e00000, LENGTH = 1k
  flash  : ORIGIN = 0x00e00400, LENGTH = 2M - 1k
  eflash : ORIGIN = 0x01000000, LENGTH = 0
  ramvec : ORIGIN = 0x00000000, LENGTH = 1k
  ram    : ORIGIN = 0x00000400, LENGTH = 14M - 1k
  eram   : ORIGIN = 0x00e00000, LENGTH = 0

for a 8MB SDRAM:

  romvec : ORIGIN = 0x00600000, LENGTH = 1k
  flash  : ORIGIN = 0x00600400, LENGTH = 2M - 1k
  eflash : ORIGIN = 0x00800000, LENGTH = 0
  ramvec : ORIGIN = 0x00000000, LENGTH = 1k
  ram    : ORIGIN = 0x00000400, LENGTH = 6M - 1k
  eram   : ORIGIN = 0x00600000, LENGTH = 0

*/

MEMORY 
	{
        romvec : ORIGIN = 0x00600000, LENGTH = 1k
        flash  : ORIGIN = 0x00600400, LENGTH = 2M - 1k
        eflash : ORIGIN = 0x00800000, LENGTH = 0
        ramvec : ORIGIN = 0x00000000, LENGTH = 1k
        ram    : ORIGIN = 0x00000400, LENGTH = 6M - 1k
        eram   : ORIGIN = 0x00600000, LENGTH = 0
	}

INCLUDE arch/m68knommu/platform/68VZ328/de2/fixed.ld
