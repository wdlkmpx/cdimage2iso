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

// modified by Luigi Auriemma for emulating the shameful customizations of MagicISO

/*
 *	loki91.c - library routines for a 64 bit LOKI91 implementation
 *
 * Designed by  Matthew Kwan <mkwan@crypto.cs.adfa.oz.au> and
 *		Lawrence Brown <lpb@cs.adfa.oz.au>

 *  Modifications:
 *		v2.0 - original set of code by mkwan 9/91
 *		v3.0 - support both LOKI89 & LOKI91 versions   10/92 lpb
 *
 *  Copyright 1991 by Lawrence Brown and UNSW.
 **    Lawrence Brown <Lawrie.Brown@adfa.edu.au> has agreed to grant permission for this  
 **    code to be used in the uif2iso tool, and for the provisions of the GPL2 to now apply to it.
 **    original code http://www.unsw.adfa.edu.au/~lpb/research/loki91/loki_book.shar
 *
 *	nb: if this program is compiled on a little-endian machine (eg Vax)
 *		#define LITTLE_ENDIAN
 *		in order to enable the byte swapping  routines
 *
 *	    if a detailed trace of LOKI91 function f is required for debugging
 *		#define TRACE=n		n=1 print blocks, n=2 print fn f also
 *                                      n=3 print individual S-box calcs also
 *
 *	    these routines assume that the 8-byte char arrays used to pass
 *		the 64-bit blocks fall on a word boundary, so that the blocks
 *		may be read as longwords for efficiency reasons. If this is
 *		not true, the load & save of the parameters will need changing.
 */


#include <stdio.h>
#include "loki.h"	/* include Interface Specification header file */
#include "loki_tab.h"

/*
 *	perm32(out, in, perm) is the general permutation of a 32-bit input
 *		block to a 32-bit output block, under the control of a
 *		permutation array perm. Each element of perm specifies which
 *		input bit is to be permuted to the output bit with the same
 *		index as the array element.
 *
 *	nb: to set bits in the output word, as mask with a single 1 in it is
 *		used. On each step, the 1 is shifted into the next location
 */

#define	MSB	0x80000000L		/* MSB of 32-bit word */

void perm32(uint32_t *out, // Output 32-bit block to be permuted
            uint32_t *in,  // Input  32-bit block after permutation
            char perm[32]) // Permutation array
{
	uint32_t	mask = MSB;		/* mask used to set bit in output    */
	register int	i, o, b;	/* input bit no, output bit no, value */
	register char	*p = perm;	/* ptr to permutation array  */

	*out = 0;			/* clear output block */
	for (o=0; o<32; o++) {		/* For each output bit position o */
		i =(int)*p++;		/* get input bit permuted to output o */
		b = (*in >> i) & 01;	/* value of input bit i */
		if (b)			/* If the input bit i is set */
			*out |= mask;		/*  OR in mask to output i */
		mask >>= 1;			/* Shift mask to next bit    */
	}
}

/*
 *	mult8(a, b, gen) - returns the product of two binary
 *		strings a and b using the generator gen as the modulus
 *			mult = a * b mod gen
 *		gen generates a suitable Galois field in GF(2^8)
 */

#define SIZE 256		/* 256 elements in GF(2^8) */

short mult8(short a, short b, /* operands for multiply */
            short gen)        /* irreducible polynomial generating Galois Field */
{
	short	product = 0;		/* result of multiplication */

	while(b != 0) {			/* while multiplier is non-zero */
		if (b & 01)
			product ^= a;	/*   add multiplicand if LSB of b set */
		a <<= 1;		/*   shift multiplicand one place */
		if (a >= SIZE)
			a ^= gen;	/*   and modulo reduce if needed */
		b >>= 1;		/*   shift multiplier one place  */
	}
	return(product);
}

/*
 *	exp8(base, exponent, gen) - returns the result of
 *		exponentiation given the base, exponent, generator of GF,
 *			exp = base ^ exp mod gen
 */

short exp8(short base,     /* base of exponentiation 	*/
           short exponent, /* exponent			*/
           short gen)      /* irreducible polynomial generating Galois Field */
{
	short	accum = base;	/* superincreasing sequence of base */
	short	result = 1;	/* result of exponentiation	    */

	if (base == 0)		/* if zero base specified then      */
		return(0);	/* the result is "0" if base = 0    */

	while (exponent != 0) {	/* repeat while exponent non-zero */
		if (( exponent & 0x0001) == 0x0001)	/* multiply if exp 1 */
			result = mult8(result, accum, gen);
		exponent >>= 1;		/* shift exponent to next digit */
		accum = mult8(accum, accum, gen);	/* & square  */
	}
	return(result);
}


/*
 *	string specifying version and copyright message
 */
char	*loki_lib_ver = "LOKI91 library v3.0, Copyright (C) 1991 Lawrence Brown & UNSW";

		/* subkeys at the 16 rounds */
static uint32_t	f();			/* declare LOKI function f */
static short	s();			/* declare LOKI S-box fn s */


/*
 *	ROL12(b) - macro to rotate 32-bit block b left by 12 bits
 *	ROL13(b) - macro to rotate 32-bit block b left by 13 bits
 */

#define ROL12(b) b = ((b << 12) | (b >> 20));
#define ROL13(b) b = ((b << 13) | (b >> 19));

/*
 *	setlokikey(key) - save 64-bit key for use in encryptions & decryptions
 *			  and compute sub-keys using the key schedule
 */

void setlokikey(uint32_t loki_subkeys[ROUNDS], char key[LOKIBLK])
{
	register int	i;
	register uint32_t	KL, KR;

#if	TRACE >= 1
	fprintf(stderr,"  keyinit(%08lx, %08lx)\n", ((uint32_t *)key)[0], ((uint32_t *)key)[1]);
#endif

	KL = ((uint32_t *)key)[0];
	KR = ((uint32_t *)key)[1];

	for (i=0; i<ROUNDS; i+=4) {	/* Generate the 16 subkeys */
	    loki_subkeys[i] = KL;
	    ROL12 (KL);
	    loki_subkeys[i+1] = KL;
	    ROL13 (KL);
	    loki_subkeys[i+2] = KR;
	    ROL12 (KR);
	    loki_subkeys[i+3] = KR;
	    ROL13 (KR);
	}
}


/*
 *	enloki(b) - main LOKI91 encryption routine, this routine encrypts one
 *		64-bit block b using the LOKI91 algorithm with loki_subkeys
 *
 *		nb: The 64-bit block is passed as two longwords. For the
 *			purposes of the LOKI89 algorithm, the bits are numbered:
 *			    [63 62 .. 33 32] [31 30 ... 1 0]
 *			The L (left) half is b[0], the R (right) half is b[1]
 *
 */

void enloki (uint32_t loki_subkeys[ROUNDS], char b[LOKIBLK])
{
	register int	i;
	register uint32_t	L, R;		/* left & right data halves  */

#if	TRACE >= 1
	fprintf(stderr,"  enloki(%08lx, %08lx)\n", ((uint32_t *)b)[0], ((uint32_t *)b)[1]);
#endif

	L = ((uint32_t *)b)[0];
	R = ((uint32_t *)b)[1];

	for (i=0; i<ROUNDS; i+=2) {	/* Encrypt with the 16 subkeys */
	    L ^= f (R, loki_subkeys[i]);
	    R ^= f (L, loki_subkeys[i+1]);
	}

	((uint32_t *)b)[0] = R;		/* Y = swap(LR) */
	((uint32_t *)b)[1] = L;

#if	TRACE >= 1
	fprintf(stderr,"  enloki returns %08lx, %08lx\n", ((uint32_t *)b)[0], ((uint32_t *)b)[1]);
#endif
}


/*
 *	deloki(b) - main LOKI91 decryption routine, this routine decrypts one
 *		64-bit block b using the LOKI91 algorithm with loki_subkeys
 *
 *		Decryption uses the same algorithm as encryption, except that
 *		the subkeys are used in reverse order.
 */
void deloki(uint32_t loki_subkeys[ROUNDS], char b[LOKIBLK])
{
	register int	i;
	register uint32_t	L, R;			/* left & right data halves  */

#if	TRACE >= 1
	fprintf(stderr,"  deloki(%08lx, %08lx)\n", ((uint32_t *)b)[0], ((uint32_t *)b)[1]);
#endif

	L = ((uint32_t *)b)[0];			/* LR = X XOR K */
	R = ((uint32_t *)b)[1];

	for (i=ROUNDS; i>0; i-=2) {		/* subkeys in reverse order */
	    L ^= f(R, loki_subkeys[i-1]);
	    R ^= f(L, loki_subkeys[i-2]);
	}

	((uint32_t *)b)[0] = R;			/* Y = LR XOR K */
	((uint32_t *)b)[1] = L;

#if	TRACE >= 1
	fprintf(stderr,"  deloki returns %08lx, %08lx\n", ((uint32_t *)b)[0], ((uint32_t *)b)[1]);
#endif
}


/*
 *	f(r, k) - is the complex non-linear LOKI function, whose output
 *		is a complex function of both input data and sub-key.
 *
 *	The data is XORed with the subkey, then expanded into 4 x 12-bit
 *	values, which are fed into the S-boxes. The 4 x 8-bit outputs
 *	from the S-boxes are permuted together to form the 32-bit value
 *	which is returned.
 *
 *	In this implementation the outputs from the S-boxes have been
 *	pre-permuted and stored in lookup tables.
 */

#define MASK12	0x0fff			/* 12 bit mask for expansion E */

static uint32_t f(register uint32_t r, /* Data value R(i-1) */
                  uint32_t k)          /* Key     K(i)   */
{
	uint32_t	a, b, c;		/* 32 bit S-box output, & P output */

	a = r ^ k;			/* A = R(i-1) XOR K(i) */

	/* want to use slow speed/small size version */
	b = ((uint32_t)s((a         & MASK12))      ) | /* B = S(E(R(i-1))^K(i)) */
	    ((uint32_t)s(((a >>  8) & MASK12)) <<  8) |
	    ((uint32_t)s(((a >> 16) & MASK12)) << 16) |
	    ((uint32_t)s((((a >> 24) | (a << 8)) & MASK12)) << 24);

	perm32(&c, &b, P);		/* C = P(S( E(R(i-1)) XOR K(i))) */

#if	TRACE >= 2	/* If Tracing, dump A, K(i), and f(R(i-1),K(i)) */
	fprintf(stderr,"  f(%08lx, %08lx) = P.S(%08lx) = P(%08lx) = %08lx\n",
		r, k, a, b, c);
#endif

	return(c);			/* f returns the result C */
}


/*
 *	s(i) - return S-box value for input i
 */
static short s (register uint32_t i)
{
	register short	r, c, v, t;
	//short	exp8();		/* exponentiation routine for GF(2^8) */

	r = ((i>>8) & 0xc) | (i & 0x3);		/* row value-top 2 & bottom 2 */
	c = (i>>2) & 0xff;			/* column value-middle 8 bits */
	t = (c + ((r * 17) ^ 0xff)) & 0xff;	/* base value for Sfn */
	v = exp8(t, sfn[r].exp, sfn[r].gen);	/* Sfn[r] = t ^ exp mod gen */
#if TRACE >= 3
	fprintf(stderr, "   s(%lx=[%d,%d]) = %x sup %d mod %d = %x\n",
			i, r, c, t, sfn[r].exp, sfn[r].gen, v);
#endif
	return(v);
}

