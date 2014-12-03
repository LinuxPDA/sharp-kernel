/*******************************************************************************
 * File:		FILE
 * Module:		MODULE
 * Author:		AUTHOR
 * Date:		DATE
 * 
 * Notes:
 *
 * $Id$
 *
 * History:
 * $Log$
 *
 ******************************************************************************/


#include <stdio.h>

typedef unsigned char u8;

#include "fcs8.c"
#include "fcs8.h"

test(char *cp)
{
    unsigned char buf[64];
    int i;
    int l;

    u8 fcs;

    l = strlen(buf);

    fcs = fcs_memcpy8(buf, cp,  l, CRC8_INITFCS);
    buf[l] = ~fcs;

    for (i = 0; i <= l; i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");

    if ((fcs = fcs_compute8(buf, l+1, CRC8_INITFCS)) != CRC8_GOODFCS) {
        printf("fcs: %02x\n", fcs);
    }

}

int main(int argc, char **argv)
{

    int i;
    int l;

    test("aaaaabbbbbccccc");
    test("bbbbccccc");
    test("xxxxxbbbbbccccc");
    test("yaaabbbbbccccc");
    test("bybccccc");
    test("xxyxxxxxbbbbbccccc");
    test("aaaaazzzzzzzzzzzzbbbbbccccc");
    test("bbbbsccccc");
    test("xxxxxaaaabbbbbccccc");

}






/* End of FILE */
