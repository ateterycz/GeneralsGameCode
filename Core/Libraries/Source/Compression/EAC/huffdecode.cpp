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

#ifndef __HUFREAD
#define __HUFREAD 1

#include <string.h>
#include "codex.h"
#include "huffcodex.h"

#if defined(_MSC_VER)
#pragma warning(push,1)
#endif

/****************************************************************/
/*  Internal Functions                                          */
/****************************************************************/

struct HuffDecodeContext
{
    unsigned char  *s;
    int             bitsleft;
    unsigned int   bits;
};

/****************************************************************/
/*  Huffman Unpacker                                            */
/****************************************************************/

static int ZERO=0;

// TheSuperHackers @bugfix ateterycz 29/10/2026 Reads zeros past the end of the compressed data, as far as the 32 bit buffer
// reads ahead of the last bit of a valid stream, and fails beyond.
#define HUFF_READAHEAD 4
#define HUFF_SOURCEBYTE(i) ((i) < packsize ? (unsigned int) packbuf[i] : 0u)

#define GET16BITS() \
    if (qpos > packsize+HUFF_READAHEAD-2)\
        goto huff_fail;\
    bitsunshifted =  HUFF_SOURCEBYTE(qpos) | (bitsunshifted << 8);\
    bitsunshifted =  HUFF_SOURCEBYTE(qpos+1) | (bitsunshifted << 8);\
    qpos += 2;

// TheSuperHackers @bugfix ateterycz 29/10/2026 Fails instead of writing past the decoded size of the header.
#define HUFF_PUT(c) \
    if (qd >= qdend)\
        goto huff_fail;\
    *qd++ = (c);

// TheSuperHackers @bugfix ateterycz 29/10/2026 Fails on numbers of more than 30 bits, which the encoder never writes and
// whose shifts are undefined, so that a run of zero bits cannot loop forever.
#define HUFF_MAXNUMBITS 30


#define SQgetbits(v,n)\
    if (n) \
    { \
        v = bits >> (32-(n));\
        bits <<= (n);\
        bitsleft -= (n);\
    } \
\
    if (bitsleft<0)\
    {\
        GET16BITS() \
\
        bits = bitsunshifted<<(-bitsleft);\
        bitsleft += 16;\
    }


#define SQgetnum(v) \
    if ((int)bits<0)\
    {\
        SQgetbits(v,3);\
        v -= 4;\
    }\
    else\
    {\
        int             n;\
        unsigned int   v1;\
\
        if (bits>>16)\
        {\
            n=2;\
            do\
            {\
                bits <<= 1; \
                ++n;\
            }\
            while ((int)bits>=0);\
            bits <<= 1;\
            bitsleft -= (n-1);\
            SQgetbits(v,ZERO);\
        }\
        else\
        {\
            n=2;\
            do\
            {\
                if (n>=HUFF_MAXNUMBITS)\
                    goto huff_fail;\
                ++n;\
                SQgetbits(v,1);\
            }\
            while (!v);\
        }\
        if (n>16)\
        {\
            SQgetbits(v,n-16);\
            SQgetbits(v1,16);\
            v = (v1|(v<<16))+(1<<n)-4;\
        }\
        else\
        {\
            SQgetbits(v,n);\
            v = (v+(1<<n)-4);\
        }\
    }

/* note:  requires 'amt' be a multiple of 16 */
#define SQmemset(ptr, val, amt) \
{\
    int i = amt/16;\
    int intval = (val<<24)|(val<<16)|(val<<8)|val;\
    int* lptr = (int*)ptr;\
\
    while (i)\
    {\
        lptr[0] = intval;\
        lptr[1] = intval;\
        lptr[2] = intval;\
        lptr[3] = intval;\
        lptr += 4;\
        --i;\
    }\
}\

// TheSuperHackers @bugfix ateterycz 29/10/2026 Checks the code tables against their arrays and every code against the tables.
static int HUFF_decompress(unsigned char *packbuf, int packsize, unsigned char *unpackbuf, int unpacksize)
{
    unsigned int    type;
    unsigned char   clue;
    int            ulen;
    unsigned int    cmp;
    int             bitnum=0;
    int             cluelen=0;
    int             qpos;
    unsigned char   *qd;
    unsigned char   *qdend;
    unsigned int   bits;
    unsigned int   bitsunshifted=0;
    int             numbits;
    int             bitsleft;
    unsigned int   v;

    qpos = 0;
    qd = unpackbuf;
    ulen = 0L;

    if (!packbuf || !unpackbuf || packsize < 0 || unpacksize < 0)
        return(0);

    if (packbuf)
    {
        {
            int             mostbits;
            int             numchars;
            int             i;
            int             bitnumtbl[16];
            unsigned int    deltatbl[16];
            unsigned int    cmptbl[16];
            unsigned char   codetbl[256];
            unsigned char   quickcodetbl[256];
            unsigned char   quicklentbl[256];

            bitsleft = -16;                                 /* init bit stream */
            bits = 0;
            SQgetbits(v,ZERO);

            SQgetbits(type,16);

            if (type&0x8000) /* 4 byte size field */
            {
                /* (skip nothing for 0x30fb) */
                if (type&0x100)                                 /* skip ulen */
                {
                    SQgetbits(v,16);
                    SQgetbits(v,16);
                }
                type &= ~0x100;

                SQgetbits(v,16);                                 /* unpack len */
                SQgetbits(ulen,16);
                ulen |= (v<<16);
            }
            else
            {
                /* (skip nothing for 0x30fb) */
                if (type&0x100)                                 /* skip ulen */
                {
                    SQgetbits(v,8);
                    SQgetbits(v,16);
                }
                type &= ~0x100;

                SQgetbits(v,8);                                 /* unpack len */
                SQgetbits(ulen,16);
                ulen |= (v<<16);
            }

            if (ulen < 0 || ulen > unpacksize)
                goto huff_fail;
            qdend = unpackbuf+ulen;

            {
                {
                    {
                        unsigned int basecmp;

                        {
                            unsigned int t;
                            SQgetbits(t,8);                          /* clue byte */
                            clue = (unsigned char)t;
                        }

                        numchars = 0;
                        numbits = 1;
                        basecmp = (unsigned int) 0;

                        /* decode bitnums */

                        do
                        {
                            // TheSuperHackers @bugfix ateterycz 29/10/2026 Fails on codes of more than 15 bits and on
                            // more than 256 codes, which do not fit the tables.
                            if (numbits>=16)
                                goto huff_fail;
                            basecmp <<= 1;
                            deltatbl[numbits] = basecmp-numchars;

                            SQgetnum(bitnum);               /* # of codes of n bits */
                            if (bitnum>256-numchars)
                                goto huff_fail;
                            bitnumtbl[numbits] = bitnum;

                            numchars += bitnum;
                            basecmp += bitnum;

                            cmp = 0;
                            if (bitnum)                             /* left justify cmp */
                                cmp = (basecmp << (16-numbits) & 0xffff);

                            cmptbl[numbits++] = cmp;

                        }
                        while (!bitnum || cmp);                     /* n+1 bits in cmp? */
                    }
                    cmptbl[numbits-1] = 0xffffffff;               /* force match on most bits */

                    mostbits = numbits-1;

                    /* decode leapfrog code table */

                    {
                        signed char     leap[256];
                        unsigned char   nextchar;

                        SQmemset(leap,0,256);
                        nextchar = (unsigned char) -1;

                        for (i=0;i<numchars;++i)
                        {
                            int leapdelta=0;

                            SQgetnum(leapdelta);
                            ++leapdelta;
                            // TheSuperHackers @bugfix ateterycz 29/10/2026 Fails on a leap past all 256 codes, which
                            // circles the table for a long time.
                            if (leapdelta>256)
                                goto huff_fail;

                            do
                            {
                                ++nextchar;
                                if (!leap[nextchar])
                                    --leapdelta;
                            } while (leapdelta);

                            leap[nextchar] = 1;
                            codetbl[i] = nextchar;
                        }
                    }
                }

/****************************************************************/
/*  Make fast 8 tables                                          */
/****************************************************************/

                SQmemset(quicklentbl,64,256);

                {
                    int bits;
                    int bitnum;
                    int numbitentries;
                    int nextcode;
                    int nextlen;
                    int i;
                    int quickleft;
                    unsigned char *codeptr;
                    unsigned char *quickcodeptr;
                    unsigned char *quicklenptr;

                    codeptr = codetbl;
                    quickcodeptr = quickcodetbl;
                    quicklenptr = quicklentbl;
                    quickleft = 256;

                    for (bits=1; bits<=mostbits; ++bits)
                    {
                        bitnum = bitnumtbl[bits];
                        if (bits>=9)
                            break;
                        numbitentries = 1<<(8-bits);
                        // TheSuperHackers @bugfix ateterycz 29/10/2026 Fails on more short codes than a prefix code has.
                        if (bitnum*numbitentries>quickleft)
                            goto huff_fail;
                        quickleft -= bitnum*numbitentries;

                        while (bitnum--)
                        {
                            nextcode = *codeptr++;
                            nextlen = bits;
                            if (nextcode==clue)
                            {
                                cluelen = bits;
                                nextlen = 96;                   /* will force out of main loop */
                            }
                            for (i=0; i<numbitentries; ++i)
                            {
                                *quickcodeptr++ = (unsigned char) nextcode;
                                *quicklenptr++ = (unsigned char) nextlen;
                            }
                        }
                    }
                }
            }

/****************************************************************/
/*  Main decoder                                                */
/****************************************************************/

            for (;;)
            {
                unsigned char   *quickcodeptr = quickcodetbl;
                unsigned char   *quicklenptr  = quicklentbl;

                goto nextloop;

/* quick 8 fetch */

                do
                {

                    HUFF_PUT(quickcodeptr[bits>>24]);
                    GET16BITS();
                    bits = bitsunshifted<<(16-bitsleft);

/* quick 8 decode */

nextloop:
                    numbits = quicklenptr[bits>>24];
                    bitsleft -= numbits;

                    if (bitsleft>=0)
                    {
                        do
                        {
                            HUFF_PUT(quickcodeptr[bits>>24]);
                            bits <<= numbits;

                            numbits = quicklenptr[bits>>24];
                            bitsleft -= numbits;
                            if (bitsleft<0) break;
                            HUFF_PUT(quickcodeptr[bits>>24]);
                            bits <<= numbits;

                            numbits = quicklenptr[bits>>24];
                            bitsleft -= numbits;
                            if (bitsleft<0) break;
                            HUFF_PUT(quickcodeptr[bits>>24]);
                            bits <<= numbits;

                            numbits = quicklenptr[bits>>24];
                            bitsleft -= numbits;
                            if (bitsleft<0) break;
                            HUFF_PUT(quickcodeptr[bits>>24]);
                            bits <<= numbits;

                            numbits = quicklenptr[bits>>24];
                            bitsleft -= numbits;

                        } while (bitsleft>=0);
                    }
                    bitsleft += 16;

                } while (bitsleft>=0);  /* would fetching 16 bits do it? */

                bitsleft = bitsleft-16+numbits;   /* back to normal */

/****************************************************************/
/*  16 bit decoder                                              */
/****************************************************************/

                {
                    unsigned char   code;


                    if (numbits!=96)
                    {
                        // TheSuperHackers @bugfix ateterycz 29/10/2026 Fails on a code that is not in the tables.
                        if (mostbits<9)
                            goto huff_fail;
                        cmp = (unsigned int) (bits>>16);  /* 16 bit left justified compare */

                        numbits = 8;
                        do
                        {
                            ++numbits;
                        }
                        while (cmp>=cmptbl[numbits]);
                    }
                    else
                        numbits = cluelen;


                    cmp = bits >> (32-(numbits));
                    bits <<= (numbits);
                    bitsleft -= (numbits);

                    if (cmp-deltatbl[numbits]>=(unsigned int)numchars)
                        goto huff_fail;
                    code = codetbl[cmp-deltatbl[numbits]];  /* the code */

                    if (code!=clue && bitsleft>=0)
                    {
                        HUFF_PUT(code);
                        goto nextloop;
                    }

                    if (bitsleft<0)
                    {
                        GET16BITS();
                        bits = bitsunshifted<<-bitsleft;
                        bitsleft += 16;
                    }

                    if (code!=clue)
                    {
                        HUFF_PUT(code);
                        goto nextloop;
                    }

                    /* handle clue */

                    {
                        int    runlen=0;
                        unsigned char *d=qd;
                        unsigned char *dest;

                        SQgetnum(runlen);
                        if (runlen)                             /* runlength sequence */
                        {
                            // TheSuperHackers @bugfix ateterycz 29/10/2026 Fails on a run without a byte to repeat
                            // and on a run past the decoded size of the header.
                            if (d==unpackbuf || runlen>qdend-d)
                                goto huff_fail;
                            dest = d+runlen;
                            code = *(d-1);
                            do
                            {
                                *d++ = code;
                            } while (d<dest);

                            qd = d;
                            goto nextloop;
                        }
                    }

                    SQgetbits(v,1);                         /* End Of File */
                    if (v)
                        break;

                    {
                        unsigned int t;
                        SQgetbits(t,8);                    /* explicite byte */
                        code = (unsigned char)t;
                    }
                    HUFF_PUT(code);
                    goto nextloop;
                }

            }

            // TheSuperHackers @bugfix ateterycz 29/10/2026 Fails when the stream ends before the decoded size of the header.
            if (qd!=qdend)
                goto huff_fail;

/****************************************************************/
/*  Undelta                                                     */
/****************************************************************/

            {
                int i;
                int nextchar;

                if (type==0x32fb || type==0xb2fb)                           /* deltaed? */
                {
                    i = 0;
                    qd = unpackbuf;
                    while (qd<unpackbuf+ulen)
                    {
                        i += (int) *qd;
                        *qd++ = (unsigned char) i;
                    }
                }
                else if (type==0x34fb || type==0xb4fb)                      /* accelerated? */
                {

                    i = 0;
                    nextchar = 0;
                    qd = unpackbuf;
                    while (qd<unpackbuf+ulen)
                    {
                        i += (int) *qd;
                        nextchar += i;
                        *qd++ = (unsigned char) nextchar;
                    }
                }
            }
        }
    }
    return(ulen);

huff_fail:
    return(0);
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

/****************************************************************/
/*  Information Functions                                       */
/****************************************************************/

/* check for reasonable header: */
/* 30fb..35fb header */

bool GCALL HUFF_is(const void *compresseddata)
{
    bool ok=false;
    int packtype=ggetm(compresseddata,2);

    if (packtype==0x30fb ||
     packtype==0x31fb ||
     packtype==0x32fb ||
     packtype==0x33fb ||
     packtype==0x34fb ||
     packtype==0x35fb ||
     packtype==0xb0fb ||
     packtype==0xb1fb ||
     packtype==0xb2fb ||
     packtype==0xb3fb ||
     packtype==0xb4fb ||
     packtype==0xb5fb)
        ok = true;

    return(ok);
}


/****************************************************************/
/*  Decode Functions                                            */
/****************************************************************/

int GCALL HUFF_size(const void *compresseddata)
{
    int len=0;
    int packtype=ggetm(compresseddata,2);
    int ssize=(packtype&0x8000)?4:3;

    if (packtype&0x100)     /* 31fb 33fb 35fb */
    {
        len = ggetm((char *)compresseddata+2+ssize,ssize);
    }
    else                    /* 30fb 32fb 34fb */
    {
        len = ggetm((char *)compresseddata+2,ssize);
    }

    return(len);
}

int GCALL HUFF_decode(void *dest, int destsize, const void *compresseddata, int compressedsize)
{
    return(HUFF_decompress((unsigned char *)compresseddata, compressedsize, (unsigned char *)dest, destsize));
}

#endif

