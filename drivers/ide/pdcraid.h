/*-
 * Copyright (c) 2000,2001 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

struct promise_raid_conf {
    char                promise_id[24];

    u32             dummy_0;
    u32             magic_0;
    u32             dummy_1;
    u32             magic_1;
    u16             dummy_2;
    u8              filler1[470];
    struct {
        u32 flags;                          /* 0x200 */
        u8          dummy_0;
        u8          disk_number;
        u8          channel;
        u8          device;
        u32         magic_0;
        u32         dummy_1;
        u32         dummy_2;                /* 0x210 */
        u32         disk_secs;
        u32         dummy_3;
        u16         dummy_4;
        u8          status;
        u8          type;
        u8        total_disks;            /* 0x220 */
        u8        raid0_shift;
        u8        raid0_disks;
        u8        array_number;
        u32       total_secs;
        u16       cylinders;
        u8        heads;
        u8        sectors;
        u32         magic_1;
        u32         dummy_5;                /* 0x230 */
        struct {
            u16     dummy_0;
            u8      channel;
            u8      device;
            u32     magic_0;
            u32     disk_number;
        } disk[8];
    } raid;
    u32             filler2[346];
    u32            checksum;
};

#define PR_MAGIC        "Promise Technology, Inc."

