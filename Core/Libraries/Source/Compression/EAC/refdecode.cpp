/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Copyright (C) Electronic Arts Canada Inc. 1995-2002.  All rights reserved.

#ifndef __REFREAD
#define __REFREAD 1

#include <string.h>
#include "codex.h"
#include "refcodex.h"

/****************************************************************/
/*  Information Functions                                       */
/****************************************************************/

/* check for reasonable header: */
/* 10fb header */

bool GCALL REF_is(const void *compresseddata)
{
    bool ok=false;
    int packtype=ggetm(compresseddata,2);

    if (packtype==0x10fb ||
     packtype==0x11fb ||
     packtype==0x90fb ||
     packtype==0x91fb)
        ok = true;

    return(ok);
}


/****************************************************************/
/*  Decode Functions                                            */
/****************************************************************/

int GCALL REF_size(const void *compresseddata)
{
    int len=0;
    int packtype=ggetm(compresseddata,2);
    int ssize=(packtype&0x8000)?4:3;

    if (packtype&0x100)     /* 11fb */
    {
        len = ggetm((char *)compresseddata+2+ssize,ssize);
    }
    else                    /* 10fb */
    {
        len = ggetm((char *)compresseddata+2,ssize);
    }

    return(len);
}


// TheSuperHackers @bugfix ateterycz 29/10/2026 Checks every read against the end of the compressed data, every write against
// the decoded size of the header and every back reference against the start of the destination.
#define REF_SOURCELEFT ((unsigned int)(send-s))
#define REF_DESTLEFT ((unsigned int)(dend-d))
#define REF_DESTDONE ((unsigned int)(d-(unsigned char *)dest))

int GCALL REF_decode(void *dest, int destsize, const void *compresseddata, int compressedsize)
{
    const unsigned char *s;
    const unsigned char *send;
    unsigned char *ref;
    unsigned char *d;
    unsigned char *dend;
    unsigned char first;
    unsigned char second;
    unsigned char third;
    unsigned char forth;
    unsigned int  run;
    unsigned int  offset;
    unsigned int  type;
    unsigned int  ulen;
    unsigned int  headersize;

    s = (const unsigned char *) compresseddata;
    d = (unsigned char *) dest;
    ulen = 0L;

    if (!s || !d || compressedsize < 2 || destsize < 0)
        return(0);
    send = s+compressedsize;

    if (s)
    {
        type = *s++;
        type = (type<<8) + *s++;

        headersize = (type&0x8000) ? 4 : 3;
        if (type&0x100)
            headersize *= 2;
        if (REF_SOURCELEFT < headersize)
            return(0);

        if (type&0x8000) /* 4 byte size field */
        {
            if (type&0x100)                       /* skip ulen */
                s += 4;

            ulen = *s++;
            ulen = (ulen<<8) + *s++;
            ulen = (ulen<<8) + *s++;
            ulen = (ulen<<8) + *s++;
        }
        else
        {
            if (type&0x100)                       /* skip ulen */
                s += 3;

            ulen = *s++;
            ulen = (ulen<<8) + *s++;
            ulen = (ulen<<8) + *s++;
        }

        if (ulen > (unsigned int)destsize)
            return(0);
        dend = d+ulen;

        for (;;)
        {
            if (REF_SOURCELEFT < 1)
                return(0);
            first = *s++;
            if (!(first&0x80))          /* short form */
            {
                run = first&3;
                if (REF_SOURCELEFT < 1+run || REF_DESTLEFT < run)
                    return(0);
                second = *s++;
                while (run--)
                    *d++ = *s++;
                offset = ((first&0x60)<<3) + second;
                run = ((first&0x1c)>>2)+3-1;
                if (offset >= REF_DESTDONE || REF_DESTLEFT < run+1)
                    return(0);
                ref = d-1 - offset;
                do
                {
                    *d++ = *ref++;
                } while (run--);
                continue;
            }
            if (!(first&0x40))          /* int form */
            {
                if (REF_SOURCELEFT < 2)
                    return(0);
                second = *s++;
                third = *s++;
                run = second>>6;
                if (REF_SOURCELEFT < run || REF_DESTLEFT < run)
                    return(0);
                while (run--)
                    *d++ = *s++;

                offset = ((second&0x3f)<<8) + third;

                run = (first&0x3f)+4-1;
                if (offset >= REF_DESTDONE || REF_DESTLEFT < run+1)
                    return(0);
                ref = d-1 - offset;
                do
                {
                    *d++ = *ref++;
                } while (run--);
                continue;
            }
            if (!(first&0x20))          /* very int form */
            {
                if (REF_SOURCELEFT < 3)
                    return(0);
                second = *s++;
                third = *s++;
                forth = *s++;
                run = first&3;
                if (REF_SOURCELEFT < run || REF_DESTLEFT < run)
                    return(0);
                while (run--)
                    *d++ = *s++;

                offset = ((first&0x10)>>4<<16) +  (second<<8) + third;

                run = ((first&0x0c)>>2<<8) + forth + 5-1;
                if (offset >= REF_DESTDONE || REF_DESTLEFT < run+1)
                    return(0);
                ref = d-1 - offset;
                do
                {
                    *d++ = *ref++;
                } while (run--);
                continue;
            }
            run = ((first&0x1f)<<2)+4;  /* literal */
            if (run<=112)
            {
                if (REF_SOURCELEFT < run || REF_DESTLEFT < run)
                    return(0);
                while (run--)
                    *d++ = *s++;
                continue;
            }
            run = first&3;              /* eof (+0..3 literal) */
            if (REF_SOURCELEFT < run || REF_DESTLEFT < run)
                return(0);
            while (run--)
                *d++ = *s++;
            break;
        }

        if (d != dend)
            return(0);
    }
    return((int)ulen);
}

#endif

