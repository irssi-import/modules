/*   -*- c -*-
 *  
 *  $Id: irc_idea_v3.c,v 1.1 2002/04/27 20:46:45 cras Exp $
 *  ----------------------------------------------------------------------
 *  Crypto for IRC.
 *  ----------------------------------------------------------------------
 *  Created      : Thu Jan  7 12:25:15 1999 tri
 *  Last modified: Thu Jan  7 17:41:43 1999 tri
 *  ----------------------------------------------------------------------
 *  Copyright © 1997, 1999
 *  Timo J. Rinne <tri@iki.fi>
 *
 *  See file COPYRIGHT for license details.
 * 
 *  Address: Cirion oy, PO-BOX 250, 00121 Helsinki, Finland
 *  ----------------------------------------------------------------------
 *  Any express or implied warranties are disclaimed.  In no event
 *  shall the author be liable for any damages caused (directly or
 *  otherwise) by the use of this software.
 */

#include "module.h"
#include "idea.h"

static unsigned short *idea_v3_cleartext_string_to_block_list(const char *str, 
							      int len, 
							      int *block_len);
static void idea_v3_interlace_block_list(unsigned short *buf, int buf_len);
static void idea_v3_xor_idea_blocks(unsigned short *dst, unsigned short *src);

char *irc_idea_key_fingerprint_v3(const char *key_str)
{
    unsigned short *b;
    unsigned char buf[17];

    b = irc_idea_key_expand_v3(key_str, -1);
    if (b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == 0 && 
	b[4] == 0 && b[5] == 0 && b[6] == 0 && b[7] == 0) {
        g_free(b);
	return g_strdup("0000000000000000");
    }
    buf[15] = b[0] & 0xff;
    buf[14] = (b[0] >> 8) & 0xff;
    buf[13] = b[1] & 0xff;
    buf[12] = (b[1] >> 8) & 0xff;
    buf[11] = b[2] & 0xff;
    buf[10] = (b[2] >> 8) & 0xff;
    buf[9] = b[3] & 0xff;
    buf[8] = (b[3] >> 8) & 0xff;
    buf[7] = b[4] & 0xff;
    buf[6] = (b[4] >> 8) & 0xff;
    buf[5] = b[5] & 0xff;
    buf[4] = (b[5] >> 8) & 0xff;
    buf[3] = b[6] & 0xff;
    buf[2] = (b[6] >> 8) & 0xff;
    buf[1] = b[7] & 0xff;
    buf[0] = (b[7] >> 8) & 0xff;
    g_free(b);
    b = irc_idea_key_expand_v3((char *)buf, 16);
    buf[0] = 'a' + (b[0] % 26);
    buf[1] = 'a' + (b[1] % 26);
    buf[2] = 'a' + (b[2] % 26);
    buf[3] = 'a' + (b[3] % 26);
    buf[4] = 'a' + (b[4] % 26);
    buf[5] = 'a' + (b[5] % 26);
    buf[6] = 'a' + (b[6] % 26);
    buf[7] = 'a' + (b[7] % 26);
    buf[8] = 'a' + ((b[0] >> 8) % 26);
    buf[9] = 'a' + ((b[1] >> 8) % 26);
    buf[10] = 'a' + ((b[2] >> 8) % 26);
    buf[11] = 'a' + ((b[3] >> 8) % 26);
    buf[12] = 'a' + ((b[4] >> 8) % 26);
    buf[13] = 'a' + ((b[5] >> 8) % 26);
    buf[14] = 'a' + ((b[6] >> 8) % 26);
    buf[15] = 'a' + ((b[7] >> 8) % 26);
    buf[16] = 0;
    g_free(b);
    return g_strdup((char *)buf);
}

unsigned short *irc_idea_key_expand_v3(const char *str, int len)
{
    unsigned short r1[4], r2[4], r3[4], r4[4], ek[52], bl[4];
    unsigned short *kk;
    unsigned short *blk;
    int blk_len, i;

    if (len < 0)
	len = strlen(str);
    kk = g_new0(unsigned short, 8);
    if (len == 0)
      return kk;

    kk[0] = 31415; kk[1] = 58979; kk[2] = 32384; kk[3] = 62643;
    kk[4] = 38327; kk[5] = 16939; kk[6] =  5820; kk[7] = 45923;
    r1[0] =  7816; r1[1] = 40628; r1[2] = 62089; r1[3] =  3482;
    r2[0] = 53421; r2[1] = 17067; r2[2] = 13282; r2[3] = 30664;
    r3[0] = 44609; r3[1] = 55058; r3[2] = 22317; r3[3] = 25359;
    r4[0] = 40812; r4[1] = 17450; r4[2] = 28410; r4[3] = 27019;

    blk = idea_v3_cleartext_string_to_block_list(str, len, &blk_len);
    idea_v3_interlace_block_list(blk, blk_len);
    ExpandUserKey(kk, ek);
    for (i = 0; i < blk_len; i += 4) {
	bl[0] = blk[i + 0];
	bl[1] = blk[i + 1];
	bl[2] = blk[i + 2];
	bl[3] = blk[i + 3];
	idea_v3_xor_idea_blocks(bl, r2);
	Idea(bl, bl, ek);
	idea_v3_xor_idea_blocks(r1, bl);
	if ((i + 4) < blk_len) {
	    bl[0] = blk[i + 4];
	    bl[1] = blk[i + 5];
	    bl[2] = blk[i + 6];
	    bl[3] = blk[i + 7];
	    idea_v3_xor_idea_blocks(bl, r1);
	    Idea(bl, bl, ek);
	    idea_v3_xor_idea_blocks(r2, bl);
	}
    }
    kk[0] = r1[0]; kk[1] = r2[0]; kk[2] = r1[1]; kk[3] = r2[1]; 
    kk[4] = r1[2]; kk[5] = r2[2]; kk[6] = r1[3]; kk[7] = r2[3]; 
    ExpandUserKey(kk, ek);
    idea_v3_interlace_block_list(blk, blk_len);
    for (i = 0; i < blk_len; i += 4) {
	bl[0] = blk[i + 0];
	bl[1] = blk[i + 1];
	bl[2] = blk[i + 2];
	bl[3] = blk[i + 3];
	idea_v3_xor_idea_blocks(bl, r4);
	Idea(bl, bl, ek);
	idea_v3_xor_idea_blocks(r3, bl);
	if ((i + 4) < blk_len) {
	    bl[0] = blk[i + 4];
	    bl[1] = blk[i + 5];
	    bl[2] = blk[i + 6];
	    bl[3] = blk[i + 7];
	    idea_v3_xor_idea_blocks(bl, r3);
	    Idea(bl, bl, ek);
	    idea_v3_xor_idea_blocks(r4, bl);
	}
    }
    kk[0] = r3[0]; kk[1] = r4[0]; kk[2] = r3[1]; kk[3] = r4[1]; 
    kk[4] = r3[2]; kk[5] = r4[2]; kk[6] = r3[3]; kk[7] = r4[3]; 
    g_free(blk);
    return kk;
}

/* Static stuff... */
static unsigned short *idea_v3_cleartext_string_to_block_list(const char *str, 
							      int len, 
							      int *block_len)
{
    int padlen, i;
    unsigned char *buf;
    unsigned short *ret_buf;
    char *hlp;

    if (len < 0)
	len = strlen(str);
    padlen = 8 - (len % 8);
    if (padlen == 0)
        padlen = 8;
    buf = g_malloc(len + 20);
    for (i = 0; i < padlen; i++)
        buf[i] = 0;
    memcpy(&(buf[i + 8]), str, len);
    hlp = irc_crc_buffer(str, len);
    memcpy(&(buf[i]), hlp, 8);
    g_free(hlp);
    buf[0] = ((unsigned char)(buf[0] & 31)) |
	     ((unsigned char)(((padlen - 1) & 7) << 5));
    len += 8 + padlen;
    ret_buf = g_new0(unsigned short, len / 2);
    for (i = 0; i < len / 2; i++) {
	ret_buf[i] = ((((unsigned short)(buf[i * 2])) << 8) |
		      (((unsigned short)(buf[(i * 2) + 1]))));
    }
    g_free(buf);
    *block_len = len / 2;
    return ret_buf;
}

static void idea_v3_interlace_block_list(unsigned short *buf, int buf_len)
{
    unsigned short *nbuf;
    int i, j, b;

    nbuf = g_new0(unsigned short, buf_len);
    b = buf_len / 4;
    for (i = 0; i < b; i++) {
	j = i * 4;
	nbuf[(b * 0) + i] = buf[(i * 4) + 0];
	nbuf[(b * 1) + i] = buf[(i * 4) + 1];
	nbuf[(b * 2) + i] = buf[(i * 4) + 2];
	nbuf[(b * 3) + i] = buf[(i * 4) + 3];
    }
    for (i = 0; i < buf_len; i++)
	buf[i] = nbuf[i];
    g_free(nbuf);
}

static void idea_v3_xor_idea_blocks(unsigned short *dst, unsigned short *src)
{
    dst[0] ^= src[0];
    dst[1] ^= src[1];
    dst[2] ^= src[2];
    dst[3] ^= src[3];
}

/* eof (irc_idea_v3.c) */
