/*   -*- c -*-
 *  
 *  $Id: crc32.h,v 1.1 2002/04/27 20:46:48 cras Exp $
 *  ----------------------------------------------------------------------
 *  Crypto for IRC.
 *  ----------------------------------------------------------------------
 *  Created      : Fri Feb 28 18:28:18 1997 tri
 *  Last modified: Sat Mar  1 18:30:54 1997 tri
 *  ----------------------------------------------------------------------
 */
#ifndef CRC32_H
#define CRC32_H

/* This computes a 32 bit CRC of the data in the buffer, and returns the
   CRC.  The polynomial used is 0xedb88320. */
unsigned int idea_crc32(const unsigned char *s, unsigned int len);

#endif /* CRC32_H */
