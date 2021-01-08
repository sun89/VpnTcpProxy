/* ========================================================================== **
 *
 *                                    MD4.c
 *
 * Copyright:
 *  Copyright (C) 2003-2005 by Christopher R. Hertel
 *
 * Email: crh@ubiqx.mn.org
 *
 * $Id: MD4.c,v 0.11 2005/06/08 18:35:05 crh Exp $
 *
 * -------------------------------------------------------------------------- **
 *
 * Description:
 *  Implements the MD4 hash algorithm, as described in RFC 1320.
 *
 * -------------------------------------------------------------------------- **
 *
 * License:
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * -------------------------------------------------------------------------- **
 *
 * Notes:
 *
 *  None of this will make any sense unless you're studying RFC 1320 as you
 *  read the code.
 *
 *  MD4 is described in RFC 1320 (which superceeds RFC 1186).
 *  The MD*5* algorithm is described in RFC 1321 (that's 1320 + 1).
 *  MD4 is very similar to MD5, but not quite  similar enough to justify
 *  putting the two into a single module.
 *
 *  There are three primary motivations for this particular implementation.
 *  1) Programmer's pride.  I wanted to be able to say I'd done it, and I
 *     wanted to learn from the experience.
 *  2) Portability.  I wanted an implementation that I knew to be portable
 *     to a reasonable number of platforms.  In particular, the algorithm
 *     is designed with little-endian platforms in mind, and I wanted an
 *     endian-agnostic implementation.
 *  3) Compactness.  While not an overriding goal, I thought it worth-while
 *     to see if I could reduce the overall size of the result.  This is in
 *     keeping with my hopes that this library will be suitable for use in
 *     some embedded environments.
 *  Beyond that, cleanliness and clarity are always worth pursuing.
 *
 *  As mentioned above, the code really only makes sense if you are familiar
 *  with the MD4 algorithm or are using RFC 1320 as a guide.  This code is
 *  quirky, however, so you'll want to be reading carefully.
 *
 * -------------------------------------------------------------------------- **
 *
 * References:
 *  IETF RFC 1320: The MD4 Message-Digest Algorithm
 *       Ron Rivest. IETF, April, 1992
 *
 * ========================================================================== **
 */

#include "MD4.h"
#include <stdint.h>

/* -------------------------------------------------------------------------- **
 * Static Constants:
 *
 *  Round2_k  - In each round, there is a value (given the label 'k') used
 *              to indicate the longword stored in X[] that should be used
 *              per iteration.  In round one, k is simply 0, 1, 2, ... but
 *              it gets a little more complicated in round 2.  I actually
 *              figured out a simple algorithm to calculate the correct k
 *              in round 2, but storing the sequence seems much easier.
 *
 *  Round3_k  - There's probably a way to calculate this sequence on the
 *              fly as well.  Storing it seems easier and quicker (from
 *              a running program point of view).
 *
 *  S[][]     - In each round there is a left rotate operation performed as
 *              part of the 16 permutations.  The number of bits varies in
 *              a repeating patter.  This array keeps track of the patterns
 *              used in each round.
 */

static const uint8_t Round2_k[] =
  {
  0, 4,  8, 12,
  1, 5,  9, 13,
  2, 6, 10, 14,
  3, 7, 11, 15
  };

static const uint8_t Round3_k[] =
  {
  0,  8, 4, 12,
  2, 10, 6, 14,
  1,  9, 5, 13,
  3, 11, 7, 15
  };

static const uint8_t S[3][4] =
  {
    { 3, 7, 11, 19 },   /* round 0 */
    { 3, 5,  9, 13 },   /* round 1 */
    { 3, 9, 11, 15 }    /* round 2 */
  };


/* -------------------------------------------------------------------------- **
 * Macros:
 *  md4F(), md4G(), and md4H() are described in RFC 1320.
 *  All of these operations are bitwise, and so not impacted by endian-ness.
 *
 *  GetLongByte()
 *    Extract one byte from a (32-bit) longword.  A value of 0 for <idx>
 *    indicates the lowest order byte, while 3 indicates the highest order
 *    byte.
 *    
 */

#define md4F( X, Y, Z ) ( ((X) & (Y)) | ((~(X)) & (Z)) )
#define md4G( X, Y, Z ) ( ((X) & (Y)) | ((X) & (Z)) | ((Y) & (Z)) )
#define md4H( X, Y, Z ) ( (X) ^ (Y) ^ (Z) )

#define GetLongByte( L, idx ) ((uchar)(( L >> (((idx) & 0x03) << 3) ) & 0xFF))


/* -------------------------------------------------------------------------- **
 * Static Functions:
 */

static void Permute( uint32_t ABCD[4], const uchar block[64] )
  /* ------------------------------------------------------------------------ **
   * Permute the ABCD "registers" using the 64-byte <block> as a driver.
   *
   *  Input:  ABCD  - Pointer to an array of four unsigned longwords.
   *          block - An array of bytes, 64 bytes in size.
   *
   *  Output: none.
   *
   *  Notes:  The MD4 algorithm operates on a set of four longwords stored
   *          (conceptually) in four "registers".  It is easy to imagine a
   *          simple MD4 chip that would operate this way.  In any case,
   *          the mangling of the contents of those registers is driven by
   *          the input message.  The message is chopped into 64-byte chunks
   *          and each chunk is used to manipulate the contents of the
   *          registers.
   *
   *          The MD4 Algorithm also calls for padding the input to ensure
   *          that it is a multiple of 64 bytes in length.  The last 16
   *          bytes of the padding space are used to store the message
   *          length (the length of the original message, before padding,
   *          expressed in terms of bits).  If there is not enough room
   *          for 16 bytes worth of bitcount (eg., if the original message
   *          was 122 bytes long) then the block is padded to the end with
   *          zeros and passed to this function.  Then *another* block is
   *          filled with zeros except for the last 16 bytes which contain
   *          the length.
   *
   *          Oh... and the algorithm requires that there be at least one
   *          padding byte.  The first padding byte has a value of 0x80,
   *          and any others are 0x00.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  int      round;
  int      i, j;
  uint8_t  s;
  uint32_t a, b, c, d;
  uint32_t KeepABCD[4];
  uint32_t X[16];

  /* Store the current ABCD values for later re-use.
   */
  for( i = 0; i < 4; i++ )
    KeepABCD[i] = ABCD[i];

  /* Convert the input block into an array of unsigned longs, taking care
   * to read the block in Little Endian order (the algorithm assumes this).
   * The uint32_t values are then handled in host order.
   */
  for( i = 0, j = 0; i < 16; i++ )
    {
    X[i]  =  (uint32_t)block[j++];
    X[i] |= ((uint32_t)block[j++] << 8);
    X[i] |= ((uint32_t)block[j++] << 16);
    X[i] |= ((uint32_t)block[j++] << 24);
    }

  /* This loop performs the three rounds of permutation.
   * The three rounds are each very similar.  The differences are in three
   * areas:
   *   - The function (F, G, or H) used to perform bitwise permutations on
   *     the registers,
   *   - The order in which values from X[] are chosen.
   *   - Changes to the number of bits by which the registers are rotated.
   * This implementation uses a switch statement to deal with some of the
   * differences between rounds.  Other differences are handled by storing
   * values in arrays and using the round number to select the correct set
   * of values.
   *
   * (My implementation appears to be a poor compromise between speed, size,
   * and clarity.  Ugh.  [crh])
   */
  for( round = 0; round < 3; round++ )
    {
    for( i = 0; i < 16; i++ )
      {
      j = (4 - (i % 4)) & 0x3;  /* <j> handles the rotation of ABCD.          */
      s = S[round][i%4];        /* <s> is the bit shift for this iteration.   */

      b = ABCD[(j+1) & 0x3];    /* Copy the b,c,d values per ABCD rotation.   */
      c = ABCD[(j+2) & 0x3];    /* This isn't really necessary, it just looks */
      d = ABCD[(j+3) & 0x3];    /* clean & will hopefully be optimized away.  */

      /* The actual perumation function.
       * This is broken out to minimize the code within the switch().
       */
      switch( round )
        {
        case 0:
          /* round 1 */
          a = md4F( b, c, d ) + X[i];
          break;
        case 1:
          /* round 2 */
          /* The value 0x5A827999 is trunc( (2^30) * sqrt(2) ). */
          a = md4G( b, c, d ) + X[ Round2_k[i] ] + 0x5A827999;
          break;
        default:
          /* round 3 */
          /* The value 0x6ED9EBA1 is trunc( (2^30) * sqrt(3) ). */
          a = md4H( b, c, d ) + X[ Round3_k[i] ] + 0x6ED9EBA1;
          break;
        }
      a = 0xFFFFFFFF & ( ABCD[j] + a );
      ABCD[j] = 0xFFFFFFFF & (( a << s ) | ( a >> (32 - s) ));
      }
    }

  /* Use the stored original A, B, C, D values to perform
   * one last convolution.
   */
  for( i = 0; i < 4; i++ )
    ABCD[i] = 0xFFFFFFFF & ( ABCD[i] + KeepABCD[i] );

  } /* Permute */


/* -------------------------------------------------------------------------- **
 * Functions:
 */

uchar *auth_md4Sum( uchar *dst, const uchar *src, const int srclen )
  /* ------------------------------------------------------------------------ **
   * Compute an MD4 message digest.
   *
   *  Input:  dst     - Destination buffer into which the result will be
   *                    written.  Must be 16 bytes long.
   *          src     - Source data block to be MD4'd.
   *          srclen  - The length, in bytes, of the source block.
   *                    (Note that the length is given in bytes, not bits.)
   *
   *  Output: A pointer to a 16-byte array of unsigned characters which
   *          contains the calculated MD4 message digest.  (Same as <dst>).
   *
   *  Notes:  This implementation takes only a single block.  It does not
   *          create or keep context, so there is no way to perform the MD4
   *          over multiple blocks of data.  This is okay for SMB, because
   *          the MD4 algorithm is used only in creating the NTLM hash and
   *          the NTLM Session Key, both of which have single-chunk input.
   *
   *          The MD4 algorithm is designed to work on data with of
   *          arbitrary *bit* length.  Most implementations, this one
   *          included, handle the input data in byte-sized chunks.
   *
   *          The MD4 algorithm does much of its work using four-byte
   *          words, and so can be tuned for speed based on the endian-ness
   *          of the host.  This implementation is intended to be
   *          endian-neutral, which may make it a teeny bit slower than
   *          others.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  uchar    block[64];  /* Scratch space. */
  int      i;
  uint32_t l;
  uint32_t m;
  uint32_t len = (uint32_t)srclen;
  uint32_t ABCD[4] =  /* ABCD[] contains the four 4-byte "registers" that are */
    {                 /* manipulated to produce the MD4 digest.  The input    */
    0x67452301,       /* (<src>) acts upon the registers, not the other way   */
    0xefcdab89,       /* 'round.  The initial values are those given in RFC   */
    0x98badcfe,       /* 1320 (pg.3).  Note, however, that RFC 1320 provides  */
    0x10325476        /* these values as bytes, not as longwords, and the     */
    };                /* bytes are arranged in little-endian order as if they */
                      /* were the bytes of (little endian) 32-bit ints.       */
                      /* That's confusing as all getout.                      */
                      /* The values given here are provided as 32-bit values  */
                      /* in C language format, so they are endian-agnostic.   */

  /* MD4 takes the input in 64-byte chunks and uses each chunk to drive the
   * manglement of the data in the ABCD[] array.  So...
   * Figure out how many complete 64-byte chunks there are in <src> and send
   * each to the Permute() function.  Note that if the input is shorter than
   * 64-bytes (as is the case with things like passwords) then we'll wind up
   * skipping this loop.
   */
  m = len & ~(uint32_t)63;        /* Number of bytes in whole 64-byte chunks. */
  for( l = 0; l < m; l += 64 )
    {
    Permute( ABCD, &src[l] );
    }

  /* Copy the remainder of the <src> bytes into <block>.
   * The byte of <block> immediately following the end of the copied <src>
   * bytes will be set to 0x80.  Fill the rest of <block> with zeros.
   */
  for( i = 0; i < 64; i++, l++ )
    {
    if( l < len )
      block[i] = src[l];        /* Copy bytes from <src> to block. */
    else
      {
      if( l == len )
        block[i] = 0x80;        /* We've written len+1 bytes here. */
      else /* l > len */
        block[i] = 0;           /* Filling the tail-end of block with zeros. */
      }
    }

  /* If there is *no room* for the 8-byte length field, then we MD4 this
   * sub-block, clear it out again, and add the length field later.  If
   * there *is* room for the length, then we skip this step so that the
   * length is added to the current sub-block.
   */
  if( (l - len) <= 8 )          /* Must be '<=' because we wrote len+1 bytes */
    {
    Permute( ABCD, block );
    for( i = 0; i < 64; i++ )   /* Re-clear the buffer. */
      block[i] = 0;
    }

  /* Write the length (in bits--which is <len> * 8) to the length field.
   * The length field is an 8-byte, little-endian value.
   * First we multiply the length by 8 (shift 3) to get a bit count.
   * Then copy the calculated bitlength, in little-endian order, to the
   * length field in the block.  Just to be sure, grab the tree high-order
   * bits of <len> and copy those as well (only necessary for very large
   * <src> blocks).  Finally, perform the last Permutation on the sub-block.
   */
  l = len << 3;
  for( i = 0; i < 4; i++ )
    block[56+i] |= GetLongByte( l, i );
  block[60] = ((GetLongByte( len, 3 ) & 0xE0) >> 5);  /* 'len' is not a typo. */
  Permute( ABCD, block );

  /* Now copy the result into the output buffer and we're done.
   */
  for( i = 0; i < 4; i++ )
    {
    dst[ 0+i] = GetLongByte( ABCD[0], i );
    dst[ 4+i] = GetLongByte( ABCD[1], i );
    dst[ 8+i] = GetLongByte( ABCD[2], i );
    dst[12+i] = GetLongByte( ABCD[3], i );
    }

  return( dst );
  } /* auth_md4Sum */


/* ========================================================================== */
