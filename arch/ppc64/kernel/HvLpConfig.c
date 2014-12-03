/*
 * HvLpConfig.c
 * Copyright (C) 2001  Kyle A. Lucke, IBM Corporation
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef _HVLPCONFIG_H
#include <asm/iSeries/HvLpConfig.h>
#endif

HvLpIndex HvLpConfig_getLpIndex_outline(void)
{
    return HvLpConfig_getLpIndex();
}

const HvLpIndexMap HvLpIndexMapDefault = (1 << (sizeof(HvLpIndexMap) * 8 - 1));
const HvLpIndex	HvHardcodedPrimaryLpIndex = 0;
const HvLpIndex HvMaxArchitectedLps = HVMAXARCHITECTEDLPS;
const HvLpVirtualLanIndex HvMaxArchitectedVirtualLans = 16;
const HvLpSharedPoolIndex HvMaxArchitectedSharedPools = 16;
const HvLpSharedPoolIndex HvMaxSupportedSharedPools = 1;
const HvLpIndex HvMaxRuntimeLpsPreCondor = 12;
const HvLpIndex HvMaxRuntimeLps = HVMAXARCHITECTEDLPS;
const HvLpIndex HvLpIndexInvalid = 0xff;
const u16	HvInvalidProcIndex = 0xffff;
const u32       HvVirtualFlashSize = 0x200;
const u32	HvMaxBusesPreCondor = 32; 
const u32	HvMaxBusesCondor = 256;	 
const u32	HvMaxArchitectedBuses = 512;
const HvLpBus	HvCspBusNumber = 1;
const u32	HvMaxSanHwSets = 16;
const HvLpCard	HvMaxSystemIops = 200;
const HvLpCard	HvMaxBusIops = 20;
const u16	HvMaxUnitsPerIop = 100;
const u64	HvPageSize = 4 * 1024;
const u64	HvChunkSize = HVCHUNKSIZE;
const u64	HvChunksPerMeg = HVCHUNKSPERMEG;
const u64	HvPagesPerChunk = HVPAGESPERCHUNK;
const u64	HvPagesPerMeg = HVPAGESPERMEG;
const u64       HvLpMinMegsPrimary = HVLPMINMEGSPRIMARY;
const u64       HvLpMinMegsSecondary = HVLPMINMEGSSECONDARY;
const u64       HvLpMinChunksPrimary = HVLPMINMEGSPRIMARY * HVCHUNKSPERMEG;
const u64       HvLpMinChunksSecondary = HVLPMINMEGSSECONDARY * HVCHUNKSPERMEG;
const u64       HvLpMinPagesPrimary = HVLPMINMEGSPRIMARY * HVPAGESPERMEG;
const u64       HvLpMinPagesSecondary = HVLPMINMEGSSECONDARY * HVPAGESPERMEG;
const u8        HvLpMinProcs = 1;
const u8	HvLpConfigMinInteract = 1;
const u16	HvLpMinSharedProcUnitsX100 = 10;
const u16	HvLpMaxSharedProcUnitsX100 = 100;
const HvLpSharedPoolIndex HvLpSharedPoolIndexInvalid = 0xff;
