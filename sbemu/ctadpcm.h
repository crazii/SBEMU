/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

//Creative Labs ADPCM decoding
//This is not an original DOSBox header, but is copied & modifed from DOSBox sourcce code: sblaster.cpp

static void MUTE_(const char* s) {}
#define INLINE
#define LOG(x,y) MUTE_
#define LOG_SB
#define LOG_ERROR
#include <stdint.h>
typedef int Bits;


static INLINE uint8_t decode_ADPCM_4_sample(uint8_t sample,uint8_t * reference,Bits* scale) {
    static const int8_t scaleMap[64] = {
        0,  1,  2,  3,  4,  5,  6,  7,  0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,
        1,  3,  5,  7,  9, 11, 13, 15, -1,  -3,  -5,  -7,  -9, -11, -13, -15,
        2,  6, 10, 14, 18, 22, 26, 30, -2,  -6, -10, -14, -18, -22, -26, -30,
        4, 12, 20, 28, 36, 44, 52, 60, -4, -12, -20, -28, -36, -44, -52, -60
    };
    static const uint8_t adjustMap[64] = {
          0, 0, 0, 0, 0, 16, 16, 16,
          0, 0, 0, 0, 0, 16, 16, 16,
        240, 0, 0, 0, 0, 16, 16, 16,
        240, 0, 0, 0, 0, 16, 16, 16,
        240, 0, 0, 0, 0, 16, 16, 16,
        240, 0, 0, 0, 0, 16, 16, 16,
        240, 0, 0, 0, 0,  0,  0,  0,
        240, 0, 0, 0, 0,  0,  0,  0
    };

    Bits samp = sample + *scale;

    if ((samp < 0) || (samp > 63)) {
        LOG(LOG_SB,LOG_ERROR)("Bad ADPCM-4 sample");
        if(samp < 0 ) samp =  0;
        if(samp > 63) samp = 63;
    }

    Bits ref = *reference + scaleMap[samp];
    if (ref > 0xff) *reference = 0xff;
    else if (ref < 0x00) *reference = 0x00;
    else *reference = (uint8_t)(ref&0xff);
    *scale = (*scale + adjustMap[samp]) & 0xff;

    return *reference;
}

static INLINE uint8_t decode_ADPCM_2_sample(uint8_t sample,uint8_t * reference,Bits* scale) {
    static const int8_t scaleMap[24] = {
        0,  1,  0,  -1,  1,  3,  -1,  -3,
        2,  6, -2,  -6,  4, 12,  -4, -12,
        8, 24, -8, -24, 16, 48, -16, -48
    };
    static const uint8_t adjustMap[24] = {
          0, 4,   0, 4,
        252, 4, 252, 4, 252, 4, 252, 4,
        252, 4, 252, 4, 252, 4, 252, 4,
        252, 0, 252, 0
    };

    Bits samp = sample + *scale;
    if ((samp < 0) || (samp > 23)) {
        LOG(LOG_SB,LOG_ERROR)("Bad ADPCM-2 sample");
        if(samp < 0 ) samp =  0;
        if(samp > 23) samp = 23;
    }

    Bits ref = *reference + scaleMap[samp];
    if (ref > 0xff) *reference = 0xff;
    else if (ref < 0x00) *reference = 0x00;
    else *reference = (uint8_t)(ref&0xff);
    *scale = (*scale + adjustMap[samp]) & 0xff;

    return *reference;
}

INLINE uint8_t decode_ADPCM_3_sample(uint8_t sample,uint8_t * reference,Bits* scale) {
    static const int8_t scaleMap[40] = {
        0,  1,  2,  3,  0,  -1,  -2,  -3,
        1,  3,  5,  7, -1,  -3,  -5,  -7,
        2,  6, 10, 14, -2,  -6, -10, -14,
        4, 12, 20, 28, -4, -12, -20, -28,
        5, 15, 25, 35, -5, -15, -25, -35
    };
    static const uint8_t adjustMap[40] = {
          0, 0, 0, 8,   0, 0, 0, 8,
        248, 0, 0, 8, 248, 0, 0, 8,
        248, 0, 0, 8, 248, 0, 0, 8,
        248, 0, 0, 8, 248, 0, 0, 8,
        248, 0, 0, 0, 248, 0, 0, 0
    };

    Bits samp = sample + *scale;
    if ((samp < 0) || (samp > 39)) {
        LOG(LOG_SB,LOG_ERROR)("Bad ADPCM-3 sample");
        if(samp < 0 ) samp =  0;
        if(samp > 39) samp = 39;
    }

    Bits ref = *reference + scaleMap[samp];
    if (ref > 0xff) *reference = 0xff;
    else if (ref < 0x00) *reference = 0x00;
    else *reference = (uint8_t)(ref&0xff);
    *scale = (*scale + adjustMap[samp]) & 0xff;

    return *reference;
}