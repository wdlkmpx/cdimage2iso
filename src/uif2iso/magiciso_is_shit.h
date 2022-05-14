/*
    Copyright (C) 2008,2009 Luigi Auriemma

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    http://www.gnu.org/licenses/gpl-2.0.txt
*/

#ifndef MAGICISO_IS_SHIT
#define MAGICISO_IS_SHIT 1

void x3way(uint8_t *data, int datalen, uint32_t *key);

#define WORDS_PER_SEAL_CALL 1024
typedef struct {
	uint32_t t[520]; /* 512 rounded up to a multiple of 5 + 5*/
	uint32_t s[265]; /* 256 rounded up to a multiple of 5 + 5*/
	uint32_t r[20]; /* 16 rounded up to multiple of 5 */
	uint32_t counter; /* 32-bit synch value. */
	uint32_t ks_buf[WORDS_PER_SEAL_CALL];
	int ks_pos;
} seal_ctx;
void seal_key(seal_ctx *c, unsigned char *key);
void seal_encrypt(seal_ctx *c, uint32_t *data_ptr, int w);
void seal_decrypt(seal_ctx *c, uint32_t *data_ptr, int w);

void RC5decrypt(uint32_t *in, int len, int b, uint32_t *key);
void RC5key(unsigned char *key, int b, uint32_t **ctx);

#include "loki.h"

#define IDEA_ROUNDS 8
#define IDEA_KEYLEN (6*IDEA_ROUNDS+4)
typedef struct {
    u16 ek[IDEA_KEYLEN];
    u16 dk[IDEA_KEYLEN];
    int have_dk;
} IDEA_context;
int do_setkey( IDEA_context *c, u8 *key, unsigned keylen );
void encrypt_block( IDEA_context *bc, u8 *outbuf, u8 *inbuf );
void decrypt_block( IDEA_context *bc, u8 *outbuf, u8 *inbuf );

void kboxinit(void);
void gostdecrypt(uint32_t const in[2], uint32_t out[2], uint32_t const key[8]);

#include "blowfish.h"

void dunno_key(uint32_t *dunnoctx, char *key);
void dunno_dec(uint32_t *dunnoctx, uint8_t *data, int datalen);



static const u8 magiciso_is_shit_sbox[1920] = {
    0x68,0x5a,0x3d,0xe9,0xf7,0x40,0x81,0x94,0x1c,0x26,0x4c,0xf6,0x34,0x29,0x69,0x94,
    0xf7,0x20,0x15,0x41,0xf7,0xd4,0x02,0x76,0x2e,0x6b,0xf4,0xbc,0x68,0x00,0xa2,0xd4,
    0x71,0x24,0x08,0xd4,0x6a,0xf4,0x20,0x33,0xb7,0xd4,0xb7,0x43,0xaf,0x61,0x00,0x50,
    0x2e,0xf6,0x39,0x1e,0x46,0x45,0x24,0x97,0x74,0x4f,0x21,0x14,0x40,0x88,0x8b,0xbf,
    0x1d,0xfc,0x95,0x4d,0xaf,0x91,0xb5,0x96,0xd3,0xdd,0xf4,0x70,0x45,0x2f,0xa0,0x66,
    0xec,0x09,0xbc,0xbf,0x85,0x97,0xbd,0x03,0xd0,0x6d,0xac,0x7f,0x04,0x85,0xcb,0x31,
    0xb3,0x27,0xeb,0x96,0x41,0x39,0xfd,0x55,0xe6,0x47,0x25,0xda,0x9a,0x0a,0xca,0xab,
    0x25,0x78,0x50,0x28,0xf4,0x29,0x04,0x53,0xda,0x86,0x2c,0x0a,0xfb,0x6d,0xb6,0xe9,
    0x62,0x14,0xdc,0x68,0x00,0x69,0x48,0xd7,0xa4,0xc0,0x0e,0x68,0xee,0x8d,0xa1,0x27,
    0xa2,0xfe,0x3f,0x4f,0x8c,0xad,0x87,0xe8,0x06,0xe0,0x8c,0xb5,0xb6,0xd6,0xf4,0x7a,
    0x7c,0x1e,0xce,0xaa,0xec,0x5f,0x37,0xd3,0x99,0xa3,0x78,0xce,0x42,0x2a,0x6b,0x40,
    0x35,0x9e,0xfe,0x20,0xb9,0x85,0xf3,0xd9,0xab,0xd7,0x39,0xee,0x8b,0x4e,0x12,0x3b,
    0xf7,0xfa,0xc9,0x1d,0x56,0x18,0x6d,0x4b,0x31,0x66,0xa3,0x26,0xb2,0x97,0xe3,0xea,
    0x74,0xfa,0x6e,0x3a,0x32,0x43,0x5b,0xdd,0xf7,0xe7,0x41,0x68,0xfb,0x20,0x78,0xca,
    0x4e,0xf5,0x0a,0xfb,0x97,0xb3,0xfe,0xd8,0xac,0x56,0x40,0x45,0x27,0x95,0x48,0xba,
    0x3a,0x3a,0x53,0x55,0x87,0x8d,0x83,0x20,0xb7,0xa9,0x6b,0xfe,0x4b,0x95,0x96,0xd0,
    0xbc,0x67,0xa8,0x55,0x58,0x9a,0x15,0xa1,0x63,0x29,0xa9,0xcc,0x33,0xdb,0xe1,0x99,
    0x56,0x4a,0x2a,0xa6,0xf9,0x25,0x31,0x3f,0x1c,0x7e,0xf4,0x5e,0x7c,0x31,0x29,0x90,
    0x02,0xe8,0xf8,0xfd,0x70,0x2f,0x27,0x04,0x5c,0x15,0xbb,0x80,0xe3,0x2c,0x28,0x05,
    0x48,0x15,0xc1,0x95,0x22,0x6d,0xc6,0xe4,0x3f,0x13,0xc1,0x48,0xdc,0x86,0x0f,0xc7,
    0xee,0xc9,0xf9,0x07,0x0f,0x1f,0x04,0x41,0xa4,0x79,0x47,0x40,0x17,0x6e,0x88,0x5d,
    0xeb,0x51,0x5f,0x32,0xd1,0xc0,0x9b,0xd5,0x8f,0xc1,0xbc,0xf2,0x64,0x35,0x11,0x41,
    0x34,0x78,0x7b,0x25,0x60,0x9c,0x2a,0x60,0xa3,0xe8,0xf8,0xdf,0x1b,0x6c,0x63,0x1f,
    0xc2,0xb4,0x12,0x0e,0x9e,0x32,0xe1,0x02,0xd1,0x4f,0x66,0xaf,0x15,0x81,0xd1,0xca,
    0xe0,0x95,0x23,0x6b,0xe1,0x92,0x3e,0x33,0x62,0x0b,0x24,0x3b,0x22,0xb9,0xbe,0xee,
    0x0e,0xa2,0xb2,0x85,0x99,0x0d,0xba,0xe6,0x8c,0x0c,0x72,0xde,0x28,0xf7,0xa2,0x2d,
    0x45,0x78,0x12,0xd0,0xfd,0x94,0xb7,0x95,0x62,0x08,0x7d,0x64,0xf0,0xf5,0xcc,0xe7,
    0x6f,0xa3,0x49,0x54,0xfa,0x48,0x7d,0x87,0x27,0xfd,0x9d,0xc3,0x1e,0x8d,0x3e,0xf3,
    0x41,0x63,0x47,0x0a,0x74,0xff,0x2e,0x99,0xab,0x6e,0x6f,0x3a,0x37,0xfd,0xf8,0xf4,
    0x60,0xdc,0x12,0xa8,0xf8,0xdd,0xeb,0xa1,0x4c,0xe1,0x1b,0x99,0x0d,0x6b,0x6e,0xdb,
    0x10,0x55,0x7b,0xc6,0x37,0x2c,0x67,0x6d,0x3b,0xd4,0x65,0x27,0x04,0xe8,0xd0,0xdc,
    0xc7,0x0d,0x29,0xf1,0xa3,0xff,0x00,0xcc,0x92,0x0f,0x39,0xb5,0x0b,0xed,0x0f,0x69,
    0xfb,0x9f,0x7b,0x66,0x9c,0x7d,0xdb,0xce,0x0b,0xcf,0x91,0xa0,0xa3,0x5e,0x15,0xd9,
    0x88,0x2f,0x13,0xbb,0x24,0xad,0x5b,0x51,0xbf,0x79,0x94,0x7b,0xeb,0xd6,0x3b,0x76,
    0xb3,0x2e,0x39,0x37,0x79,0x59,0x11,0xcc,0x97,0xe2,0x26,0x80,0x2d,0x31,0x2e,0xf4,
    0xa7,0xad,0x42,0x68,0x3b,0x2b,0x6a,0xc6,0xcc,0x4c,0x75,0x12,0x1c,0xf1,0x2e,0x78,
    0x37,0x42,0x12,0x6a,0xe7,0x51,0x92,0xb7,0xe6,0xbb,0xa1,0x06,0x50,0x63,0xfb,0x4b,
    0x18,0x10,0x6b,0x1a,0xfa,0xed,0xca,0x11,0xd8,0xbd,0x25,0x3d,0xc9,0xc3,0xe1,0xe2,
    0x59,0x16,0x42,0x44,0x86,0x13,0x12,0x0a,0x6e,0xec,0x0c,0xd9,0x2a,0xea,0xab,0xd5,
    0x4e,0x67,0xaf,0x64,0x5f,0xa8,0x86,0xda,0x88,0xe9,0xbf,0xbe,0xfe,0xc3,0xe4,0x64,
    0x57,0x80,0xbc,0x9d,0x86,0xc0,0xf7,0xf0,0xf8,0x7b,0x78,0x60,0x4d,0x60,0x03,0x60,
    0x46,0x83,0xfd,0xd1,0xb0,0x1f,0x38,0xf6,0x04,0xae,0x45,0x77,0xcc,0xfc,0x36,0xd7,
    0x33,0x6b,0x42,0x83,0x71,0xab,0x1e,0xf0,0x87,0x41,0x80,0xb0,0x5f,0x5e,0x00,0x3c,
    0xbe,0x57,0xa0,0x77,0x24,0xae,0xe8,0xbd,0x99,0x42,0x46,0x55,0x61,0x2e,0x58,0xbf,
    0x8f,0xf4,0x58,0x4e,0xa2,0xfd,0xdd,0xf2,0x38,0xef,0x74,0xf4,0xc2,0xbd,0x89,0x87,
    0xc3,0xf9,0x66,0x53,0x74,0x8e,0xb3,0xc8,0x55,0xf2,0x75,0xb4,0xb9,0xd9,0xfc,0x46,
    0x61,0x26,0xeb,0x7a,0x84,0xdf,0x1d,0x8b,0x79,0x0e,0x6a,0x84,0xe2,0x95,0x5f,0x91,
    0x8e,0x59,0x6e,0x46,0x70,0x57,0xb4,0x20,0x91,0x55,0xd5,0x8c,0x4c,0xde,0x02,0xc9,
    0xe1,0xac,0x0b,0xb9,0xd0,0x05,0x82,0xbb,0x48,0x62,0xa8,0x11,0x9e,0xa9,0x74,0x75,
    0xb6,0x19,0x7f,0xb7,0x09,0xdc,0xa9,0xe0,0xa1,0x09,0x2d,0x66,0x33,0x46,0x32,0xc4,
    0x02,0x1f,0x5a,0xe8,0x8c,0xbe,0xf0,0x09,0x25,0xa0,0x99,0x4a,0x10,0xfe,0x6e,0x1d,
    0x1d,0x3d,0xb9,0x1a,0xdf,0xa4,0xa5,0x0b,0x0f,0xf2,0x86,0xa1,0x69,0xf1,0x68,0x28,
    0x83,0xda,0xb7,0xdc,0xfe,0x06,0x39,0x57,0x9b,0xce,0xe2,0xa1,0x52,0x7f,0xcd,0x4f,
    0x01,0x5e,0x11,0x50,0xfa,0x83,0x06,0xa7,0xc4,0xb5,0x02,0xa0,0x27,0xd0,0xe6,0x0d,
    0x27,0x8c,0xf8,0x9a,0x41,0x86,0x3f,0x77,0x06,0x4c,0x60,0xc3,0xb5,0x06,0xa8,0x61,
    0x28,0x7a,0x17,0xf0,0xe0,0x86,0xf5,0xc0,0xaa,0x58,0x60,0x00,0x62,0x7d,0xdc,0x30,
    0xd7,0x9e,0xe6,0x11,0x63,0xea,0x38,0x23,0x94,0xdd,0xc2,0x53,0x34,0x16,0xc2,0xc2,
    0x56,0xee,0xcb,0xbb,0xde,0xb6,0xbc,0x90,0xa1,0x7d,0xfc,0xeb,0x76,0x1d,0x59,0xce,
    0x09,0xe4,0x05,0x6f,0x88,0x01,0x7c,0x4b,0x3d,0x0a,0x72,0x39,0x24,0x7c,0x92,0x7c,
    0x5f,0x72,0xe3,0x86,0xb9,0x9d,0x4d,0x72,0xb4,0x5b,0xc1,0x1a,0xfc,0xb8,0x9e,0xd3,
    0x78,0x55,0x54,0xed,0xb5,0xa5,0xfc,0x08,0xd3,0x7c,0x3d,0xd8,0xc4,0x0f,0xad,0x4d,
    0x5e,0xef,0x50,0x1e,0xf8,0xe6,0x61,0xb1,0xd9,0x14,0x85,0xa2,0x3c,0x13,0x51,0x6c,
    0xe7,0xc7,0xd5,0x6f,0xc4,0x4e,0xe1,0x56,0xce,0xbf,0x2a,0x36,0x37,0xc8,0xc6,0xdd,
    0x34,0x32,0x9a,0xd7,0x12,0x82,0x63,0x92,0x8e,0xfa,0x0e,0x67,0xe0,0x00,0x60,0x40,
    0x37,0xce,0x39,0x3a,0xcf,0xf5,0xfa,0xd3,0x37,0x77,0xc2,0xab,0x1b,0x2d,0xc5,0x5a,
    0x9e,0x67,0xb0,0x5c,0x42,0x37,0xa3,0x4f,0x40,0x27,0x82,0xd3,0xbe,0x9b,0xbc,0x99,
    0x9d,0x8e,0x11,0xd5,0x15,0x73,0x0f,0xbf,0x7e,0x1c,0x2d,0xd6,0x7b,0xc4,0x00,0xc7,
    0x6b,0x1b,0x8c,0xb7,0x45,0x90,0xa1,0x21,0xbe,0xb1,0x6e,0xb2,0xb4,0x6e,0x36,0x6a,
    0x2f,0xab,0x48,0x57,0x79,0x6e,0x94,0xbc,0xd2,0x76,0xa3,0xc6,0xc8,0xc2,0x49,0x65,
    0xee,0xf8,0x0f,0x53,0x7d,0xde,0x8d,0x46,0x1d,0x0a,0x73,0xd5,0xc6,0x4d,0xd0,0x4c,
    0xdb,0xbb,0x39,0x29,0x50,0x46,0xba,0xa9,0xe8,0x26,0x95,0xac,0x04,0xe3,0x5e,0xbe,
    0xf0,0xd5,0xfa,0xa1,0x9a,0x51,0x2d,0x6a,0xe2,0x8c,0xef,0x63,0x22,0xee,0x86,0x9a,
    0xb8,0xc2,0x89,0xc0,0xf6,0x2e,0x24,0x43,0xaa,0x03,0x1e,0xa5,0xa4,0xd0,0xf2,0x9c,
    0xba,0x61,0xc0,0x83,0x4d,0x6a,0xe9,0x9b,0x50,0x15,0xe5,0x8f,0xd6,0x5b,0x64,0xba,
    0xf9,0xa2,0x26,0x28,0xe1,0x3a,0x3a,0xa7,0x86,0x95,0xa9,0x4b,0xe9,0x62,0x55,0xef,
    0xd3,0xef,0x2f,0xc7,0xda,0xf7,0x52,0xf7,0x69,0x6f,0x04,0x3f,0x59,0x0a,0xfa,0x77,
    0x15,0xa9,0xe4,0x80,0x01,0x86,0xb0,0x87,0xad,0xe6,0x09,0x9b,0x93,0xe5,0x3e,0x3b,
    0x5a,0xfd,0x90,0xe9,0x97,0xd7,0x34,0x9e,0xd9,0xb7,0xf0,0x2c,0x51,0x8b,0x2b,0x02,
    0x3a,0xac,0xd5,0x96,0x7d,0xa6,0x7d,0x01,0xd6,0x3e,0xcf,0xd1,0x28,0x2d,0x7d,0x7c,
    0xcf,0x25,0x9f,0x1f,0x9b,0xb8,0xf2,0xad,0x72,0xb4,0xd6,0x5a,0x4c,0xf5,0x88,0x5a,
    0x71,0xac,0x29,0xe0,0xe6,0xa5,0x19,0xe0,0xfd,0xac,0xb0,0x47,0x9b,0xfa,0x93,0xed,
    0x8d,0xc4,0xd3,0xe8,0xcc,0x57,0x3b,0x28,0x29,0x66,0xd5,0xf8,0x28,0x2e,0x13,0x79,
    0x91,0x01,0x5f,0x78,0x55,0x60,0x75,0xed,0x44,0x0e,0x96,0xf7,0x8c,0x5e,0xd3,0xe3,
    0xd4,0x6d,0x05,0x15,0xba,0x6d,0xf4,0x88,0x25,0x61,0xa1,0x03,0xbd,0xf0,0x64,0x05,
    0x15,0x9e,0xeb,0xc3,0xa2,0x57,0x90,0x3c,0xec,0x1a,0x27,0x97,0x2a,0x07,0x3a,0xa9,
    0x9b,0x6d,0x3f,0x1b,0xf5,0x21,0x63,0x1e,0xfb,0x66,0x9c,0xf5,0x19,0xf3,0xdc,0x26,
    0x28,0xd9,0x33,0x75,0xf5,0xfd,0x55,0xb1,0x82,0x34,0x56,0x03,0xbb,0x3c,0xba,0x8a,
    0x11,0x77,0x51,0x28,0xf8,0xd9,0x0a,0xc2,0x67,0x51,0xcc,0xab,0x5f,0x92,0xad,0xcc,
    0x51,0x17,0xe8,0x4d,0x8e,0xdc,0x30,0x38,0x62,0x58,0x9d,0x37,0x91,0xf9,0x20,0x93,
    0xc2,0x90,0x7a,0xea,0xce,0x7b,0x3e,0xfb,0x64,0xce,0x21,0x51,0x32,0xbe,0x4f,0x77,
    0x7e,0xe3,0xb6,0xa8,0x46,0x3d,0x29,0xc3,0x69,0x53,0xde,0x48,0x80,0xe6,0x13,0x64,
    0x10,0x08,0xae,0xa2,0x24,0xb2,0x6d,0xdd,0xfd,0x2d,0x85,0x69,0x66,0x21,0x07,0x09,
    0x0a,0x46,0x9a,0xb3,0xdd,0xc0,0x45,0x64,0xcf,0xde,0x6c,0x58,0xae,0xc8,0x20,0x1c,
    0xdd,0xf7,0xbe,0x5b,0x40,0x8d,0x58,0x1b,0x7f,0x01,0xd2,0xcc,0xbb,0xe3,0xb4,0x6b,
    0x7e,0x6a,0xa2,0xdd,0x45,0xff,0x59,0x3a,0x44,0x0a,0x35,0x3e,0xd5,0xcd,0xb4,0xbc,
    0xa8,0xce,0xea,0x72,0xbb,0x84,0x64,0xfa,0xae,0x12,0x66,0x8d,0x47,0x6f,0x3c,0xbf,
    0x63,0xe4,0x9b,0xd2,0x9e,0x5d,0x2f,0x54,0x1b,0x77,0xc2,0xae,0x70,0x63,0x4e,0xf6,
    0x8d,0x0d,0x0e,0x74,0x57,0x13,0x5b,0xe7,0x71,0x16,0x72,0xf8,0x5d,0x7d,0x53,0xaf,
    0x08,0xcb,0x40,0x40,0xcc,0xe2,0xb4,0x4e,0x6a,0x46,0xd2,0x34,0x84,0xaf,0x15,0x01,
    0x28,0x04,0xb0,0xe1,0x1d,0x3a,0x98,0x95,0xb4,0x9f,0xb8,0x06,0x48,0xa0,0x6e,0xce,
    0x82,0x3b,0x3f,0x6f,0x82,0xab,0x20,0x35,0x4b,0x1d,0x1a,0x01,0xf8,0x27,0x72,0x27,
    0xb1,0x60,0x15,0x61,0xdc,0x3f,0x93,0xe7,0x2b,0x79,0x3a,0xbb,0xbd,0x25,0x45,0x34,
    0xe1,0x39,0x88,0xa0,0x4b,0x79,0xce,0x51,0xb7,0xc9,0x32,0x2f,0xc9,0xba,0x1f,0xa0,
    0x7e,0xc8,0x1c,0xe0,0xf6,0xd1,0xc7,0xbc,0xc3,0x11,0x01,0xcf,0xc7,0xaa,0xe8,0xa1,
    0x49,0x87,0x90,0x1a,0x9a,0xbd,0x4f,0xd4,0xcb,0xde,0xda,0xd0,0x38,0xda,0x0a,0xd5,
    0x2a,0xc3,0x39,0x03,0x67,0x36,0x91,0xc6,0x7c,0x31,0xf9,0x8d,0x4f,0x2b,0xb1,0xe0,
    0xb7,0x59,0x9e,0xf7,0x3a,0xbb,0xf5,0x43,0xff,0x19,0xd5,0xf2,0x9c,0x45,0xd9,0x27,
    0x2c,0x22,0x97,0xbf,0x2a,0xfc,0xe6,0x15,0x71,0xfc,0x91,0x0f,0x25,0x15,0x94,0x9b,
    0x61,0x93,0xe5,0xfa,0xeb,0x9c,0xb6,0xce,0x59,0x64,0xa8,0xc2,0xd1,0xa8,0xba,0x12,
    0x5e,0x07,0xc1,0xb6,0x0c,0x6a,0x05,0xe3,0x65,0x50,0xd2,0x10,0x42,0xa4,0x03,0xcb,
    0x0e,0x6e,0xec,0xe0,0x3b,0xdb,0x98,0x16,0xbe,0xa0,0x98,0x4c,0x64,0xe9,0x78,0x32,
    0x32,0x95,0x1f,0x9f,0xdf,0x92,0xd3,0xe0,0x2b,0x34,0xa0,0xd3,0x1e,0xf2,0x71,0x89,
    0x41,0x74,0x0a,0x1b,0x8c,0x34,0xa3,0x4b,0x20,0x71,0xbe,0xc5,0xd8,0x32,0x76,0xc3,
    0x8d,0x9f,0x35,0xdf,0x2e,0x2f,0x99,0x9b,0x47,0x6f,0x0b,0xe6,0x1d,0xf1,0xe3,0x0f,
    0x54,0xda,0x4c,0xe5,0x91,0xd8,0xda,0x1e,0xcf,0x79,0x62,0xce,0x6f,0x7e,0x3e,0xcd,
    0x66,0xb1,0x18,0x16,0x05,0x1d,0x2c,0xfd,0xc5,0xd2,0x8f,0x84,0x99,0x22,0xfb,0xf6,
    0x57,0xf3,0x23,0xf5,0x23,0x76,0x32,0xa6,0x31,0x35,0xa8,0x93,0x02,0xcd,0xcc,0x56,
    0x62,0x81,0xf0,0xac,0xb5,0xeb,0x75,0x5a,0x97,0x36,0x16,0x6e,0xcc,0x73,0xd2,0x88,
    0x92,0x62,0x96,0xde,0xd0,0x49,0xb9,0x81,0x1b,0x90,0x50,0x4c,0x14,0x56,0xc6,0x71,
    0xbd,0xc7,0xc6,0xe6,0x0a,0x14,0x7a,0x32,0x06,0xd0,0xe1,0x45,0x9a,0x7b,0xf2,0xc3 };



typedef struct {
    IDEA_context    ideactx;
    seal_ctx        sealctx;
    blf_ctx         blfctx;
    u32             dunnoctx[6];
    u32             x3wayctx[3];
    u32             gostctx[8];
    u32             *rc5ctx;
    u32             lokictx[ROUNDS];
    u8              *key;
    u8              flag;
    u8              *easy_reset;    // lazy solution
} magiciso_is_shit_ctx_t;
#define magiciso_is_shit_ctx_t_size ((u8 *)&(ctx->easy_reset) - (u8 *)ctx)



const u8 *magiciso_is_shit_getkey(int num) {
    return(magiciso_is_shit_sbox + ((num & 0x0f) << (3 + 2 + 2)));
}



void magiciso_is_shit_endian(u8 *data, int len, int bytes) {
#ifdef WORDS_BIGENDIAN
    int     i,
            j;
    u8      tmp[16];
    if(bytes <= 1) return;
    for(i = 0; (i + bytes) <= len; i += bytes) {
        for(j = 0; j < bytes; j++) {
            tmp[j] = data[i + ((bytes - j) - 1)];
        }
        memcpy(data + i, tmp, bytes);
    }
#endif
}



void magiciso_is_shit_key(magiciso_is_shit_ctx_t *ctx, int num) {
    u8      tmpkey[32];

    if(!ctx) return;
    ctx->key  = (u8 *)magiciso_is_shit_getkey(num);
    memcpy(tmpkey, ctx->key, 32);   // avoids possible read-only problems

    if(ctx->flag & 0x01) {
        magiciso_is_shit_endian(tmpkey, 32, 4);
        memcpy((void *)ctx->x3wayctx, tmpkey, 12);
        magiciso_is_shit_endian(tmpkey, 32, 4);
    }
    if(ctx->flag & 0x02) {
        dunno_key(ctx->dunnoctx, tmpkey);
    }
    if(ctx->flag & 0x04) {
        InitializeBlowfish(&ctx->blfctx, tmpkey, 16);
    }
    if(ctx->flag & 0x08) {
        magiciso_is_shit_endian(tmpkey, 32, 4);
        kboxinit();
        memcpy(ctx->gostctx, tmpkey, 32);
        magiciso_is_shit_endian(tmpkey, 32, 4);
    }
    if(ctx->flag & 0x10) {
        do_setkey(&ctx->ideactx, tmpkey, 16);
    }
    if(ctx->flag & 0x20) {
        magiciso_is_shit_endian(tmpkey, 32, 4);
        setlokikey(ctx->lokictx, tmpkey);
        magiciso_is_shit_endian(tmpkey, 32, 4);
    }
    if(ctx->flag & 0x40) {
        magiciso_is_shit_endian(tmpkey, 5, 4);
        RC5key(tmpkey, 5, &ctx->rc5ctx);
        magiciso_is_shit_endian(tmpkey, 5, 4);
    }
    if(ctx->flag & 0x80) {
        seal_key(&ctx->sealctx, tmpkey);
    }

    ctx->easy_reset = malloc(magiciso_is_shit_ctx_t_size);
    memcpy(ctx->easy_reset, ctx, magiciso_is_shit_ctx_t_size);
}



void magiciso_is_shit_reset(magiciso_is_shit_ctx_t *ctx) {  // easy solution
    if(!ctx) return;
    memcpy(ctx, ctx->easy_reset, magiciso_is_shit_ctx_t_size);
}



void magiciso_is_shit_free(magiciso_is_shit_ctx_t *ctx) {
    if(!ctx) return;
    free(ctx->easy_reset);
    free(ctx);
}



void magiciso_is_shit_dec(magiciso_is_shit_ctx_t *ctx, u8 *data, int datalen) {
    int     i;

    if(!ctx) return;
    datalen &= ~7;
    magiciso_is_shit_reset(ctx);

    if(ctx->flag & 0x01) {
        magiciso_is_shit_endian(data, datalen, 4);
        x3way(data, datalen, ctx->x3wayctx);
        magiciso_is_shit_endian(data, datalen, 4);
    }
    if(ctx->flag & 0x80) {
        magiciso_is_shit_endian(data, datalen, 4);
        seal_decrypt(&ctx->sealctx, (u32 *)data, datalen >> 2);
        magiciso_is_shit_endian(data, datalen, 4);
    }
    if(ctx->flag & 0x40) {
        magiciso_is_shit_endian(data, datalen, 4);
        RC5decrypt((u32 *)data, datalen >> 3, 5, ctx->rc5ctx);
        magiciso_is_shit_endian(data, datalen, 4);
    }
    if(ctx->flag & 0x20) {
        magiciso_is_shit_endian(data, datalen, 4);
        for(i = 0; i < datalen; i += 8) {
            deloki(ctx->lokictx, data + i);
        }
        magiciso_is_shit_endian(data, datalen, 4);
    }
    if(ctx->flag & 0x10) {
        magiciso_is_shit_endian(data, datalen, 2);
        for(i = 0; i < datalen; i += 8) {
            decrypt_block(&ctx->ideactx, data + i, data + i);
        }
        magiciso_is_shit_endian(data, datalen, 2);
    }
    if(ctx->flag & 0x08) {
        magiciso_is_shit_endian(data, datalen, 4);
        for(i = 0; i < datalen; i += 8) {
            gostdecrypt((u32 *)(data + i), (u32 *)(data + i), (void *)ctx->gostctx);
        }
        magiciso_is_shit_endian(data, datalen, 4);
    }
    if(ctx->flag & 0x04) {
        magiciso_is_shit_endian(data, datalen, 4);
        blf_dec(&ctx->blfctx, (u32 *)data, datalen >> 3);
        magiciso_is_shit_endian(data, datalen, 4);
    }
    if(ctx->flag & 0x02) {
        dunno_dec(ctx->dunnoctx, data, datalen);
    }
}

#endif /* MAGICISO_IS_SHIT */