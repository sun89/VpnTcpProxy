#ifndef AUTH_MD4_H
#define AUTH_MD4_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== **
 *
 *                                    MD4.h
 *
 * Copyright:
 *  Copyright (C) 2003-2005 by Christopher R. Hertel
 *
 * Email: crh@ubiqx.mn.org
 *
 * $Id: MD4.h,v 0.6 2005/06/08 18:35:05 crh Exp $
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

//#include "auth_common.h"


/* -------------------------------------------------------------------------- **
 * Functions:
 */
 
typedef unsigned char uchar;

uchar *auth_md4Sum( uchar *dst, const uchar *src, const int srclen );
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

#ifdef __cplusplus
}
#endif

/* ========================================================================== */
#endif /* AUTH_MD4_H */
