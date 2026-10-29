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

#ifndef __BTRREAD
#define __BTRREAD 1

#include <string.h>
#include "codex.h"
#include "btreecodex.h"

/****************************************************************/
/*  Internal Functions                                          */
/****************************************************************/

struct BTreeDecodeContext
{
    signed char    cluetbl[256];
    unsigned char  left[256];
    unsigned char  right[256];
    unsigned char  *d;
    unsigned char  *dend;
};

// TheSuperHackers @bugfix ateterycz 29/10/2026 Fails when the tree is deeper than its 256 codes allow, which only a node
// that contains itself can be, or when the codes do not fit the destination.
static bool BTREE_chase(struct BTreeDecodeContext *DC, unsigned char node, int depth)
{
    if (DC->cluetbl[node])
    {
        if (depth >= 256)
            return(false);
        return(BTREE_chase(DC,DC->left[node],depth+1) &&
            BTREE_chase(DC,DC->right[node],depth+1));
    }
    if (DC->d >= DC->dend)
        return(false);
    *DC->d++ = node;
    return(true);
}

// TheSuperHackers @bugfix ateterycz 29/10/2026 Checks every read against the end of the compressed data and every write
// against the decoded size of the header.
static int BTREE_decompress(unsigned char *packbuf,int packsize,unsigned char *unpackbuf,int unpacksize)
{
    int  node;
    int  i;
    int  nodes;
    int  clue;
    int ulen;
    unsigned char *s;
    unsigned char *send;
    signed char c;
    unsigned int type;
    struct BTreeDecodeContext DC;

    s = packbuf;
    DC.d = unpackbuf;
    ulen = 0L;

    if (!s || !DC.d || packsize < 2 || unpacksize < 0)
        return(0);
    send = s+packsize;

    if (s)
    {
        type = ggetm(s,2);
        s += 2;

        /* header, clue and node count */
        if (send-s < ((type==0x47fb) ? 3 : 0)+3+2)
            return(0);

        /* (skip nothing for 0x46fb) */
        if (type==0x47fb)                       /* skip ulen */
            s += 3;

        ulen = ggetm(s,3);
        s += 3;

        if (ulen > unpacksize)
            return(0);
        DC.dend = DC.d+ulen;

        for (i=0;i<256;++i)                     /* 0 means a code is a leaf */
            DC.cluetbl[i] = 0;

        clue = *s++;
        DC.cluetbl[clue] = 1;                   /* mark clue as special */

        nodes = *s++;
        if (send-s < nodes*3)
            return(0);
        for (i=0;i<nodes;++i)
        {   node = *s++;
            DC.left[node]  = *s++;
            DC.right[node] = *s++;
            DC.cluetbl[node] = (signed char)-1;
        }

        for (;;)
        {
            if (s >= send)
                return(0);
            node = (int) *s++;
            c=DC.cluetbl[node];
            if (!c)
            {
                if (DC.d >= DC.dend)
                    return(0);
                *DC.d++ = (unsigned char) node;
                continue;
            }
            if (c<0)
            {
                if (!BTREE_chase(&DC,DC.left[node],1) ||
                    !BTREE_chase(&DC,DC.right[node],1))
                    return(0);
                continue;
            }
            if (s >= send)
                return(0);
            node = (int) *s++;
            if (node)
            {
                if (DC.d >= DC.dend)
                    return(0);
                *DC.d++ = (char) node;
                continue;
            }
            break;
        }

        if (DC.d != DC.dend)
            return(0);
    }
    return(ulen);
}

/****************************************************************/
/*  Information Functions                                       */
/****************************************************************/

/* check for reasonable header: */
/* 46fb header */

bool GCALL BTREE_is(const void *compresseddata)
{
    bool ok=false;

    if (ggetm(compresseddata,2)==0x46fb ||
     ggetm(compresseddata,2)==0x47fb)
        ok = true;

    return(ok);
}


/****************************************************************/
/*  Decode Functions                                            */
/****************************************************************/

int GCALL BTREE_size(const void *compresseddata)
{
    int len=0;

    if (ggetm(compresseddata,2)==0x46fb)
    {
        len = ggetm((char *)compresseddata+2,3);
    }
    else
    {
        len = ggetm((char *)compresseddata+2+3,3);
    }

    return(len);
}

int GCALL BTREE_decode(void *dest, int destsize, const void *compresseddata, int compressedsize)
{
    return(BTREE_decompress((unsigned char *)compresseddata,compressedsize,(unsigned char *)dest,destsize));
}

#endif

