/*   -*- c -*-
 *  
 *  $Id: irc_idea_v2.c,v 1.1 2002/04/27 20:46:44 cras Exp $
 *  ----------------------------------------------------------------------
 *  Crypto for IRC.
 *  ----------------------------------------------------------------------
 *  Created      : Thu Jan  7 12:25:15 1999 tri
 *  Last modified: Thu Jan  7 16:55:33 1999 tri
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

static char *idea_expand_string_v2(const char *str,
				   int len,
				   int salt1,
				   int salt2,
				   int *rlen);

char *irc_idea_key_fingerprint_v2(const char *key_str)
{
    unsigned short *b;
    unsigned char r[22], s[22];
    unsigned int c1, c2;
    char *pr;

    b = irc_idea_key_expand_v2(key_str, -1);
    if (b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == 0 && 
	b[4] == 0 && b[5] == 0 && b[6] == 0 && b[7] == 0) {
        g_free(b);
	return g_strdup("000000000000");
    }
    r[12] = s[21] = 0;
    r[11] = s[20] = b[0] & 0xff;
    r[10] = s[19] = (b[0] >> 8) & 0xff;
    r[9]  = s[18] = b[1] & 0xff;
    r[8]  = s[17] = (b[1] >> 8) & 0xff;
    r[7]  = s[16] = b[2] & 0xff;
    r[6]  = s[15] = (b[2] >> 8) & 0xff;
    r[5]  = s[14] = b[3] & 0xff;
    r[4]  = s[13] = (b[3] >> 8) & 0xff;
    s[12] = r[21] = 255;
    s[11] = r[20] = b[4] & 0xff;
    s[10] = r[19] = (b[4] >> 8) & 0xff;
    s[9]  = r[18] = b[5] & 0xff;
    s[8]  = r[17] = (b[5] >> 8) & 0xff;
    s[7]  = r[16] = b[6] & 0xff;
    s[6]  = r[15] = (b[6] >> 8) & 0xff;
    s[5]  = r[14] = b[7] & 0xff;
    s[4]  = r[13] = (b[7] >> 8) & 0xff;
    g_free(b);
    c1 = irc_crc_buffer_numeric((char *)(&r[4]), 18);
    s[0] = (c1 >> 24) & 0xff;
    s[1] = (c1 >> 16) & 0xff;
    s[2] = (c1 >> 8) & 0xff;
    s[3] = c1 & 0xff;
    c2 = irc_crc_buffer_numeric((char *)(&s[0]), 22);
    r[0] = (c2 >> 24) & 0xff;
    r[1] = (c2 >> 16) & 0xff;
    r[2] = (c2 >> 8) & 0xff;
    r[3] = c2 & 0xff;
    c1 = irc_crc_buffer_numeric((char *)r, 13);
    c2 = irc_crc_buffer_numeric((char *)s, 13);
    pr = g_malloc(13);
    sprintf(pr, "%06x%06x", (c1 >> 8) & 0xffffff, (c2 >> 8) & 0xffffff);
    return pr;
}

unsigned short *irc_idea_key_expand_v2(const char *str, int len)
{
    unsigned short *key;
    unsigned char *hlp, *s1, *s2, *s3, *s4;
    int l1, l2, l3, l4;
    unsigned int crc, v1, v2, v3, v4;

    key = g_new0(unsigned short, 8);
    if (len < 0)
	len = strlen(str);
    if (len == 0)
	return key;
    if (len > 3) {
	hlp = g_malloc(len);
	memcpy(hlp, str, len);
    } else {
	hlp = g_malloc(len + 4);
	memcpy(hlp, str, len);
	crc = irc_crc_string_numeric(str);
	hlp[len] = (crc >> 24) & 0xff;
	len++;
	hlp[len] = (crc >> 16) & 0xff;
	len++;
	hlp[len] = (crc >> 8) & 0xff;
	len++;
	hlp[len] = crc & 0xff;
	len++;
    }
    s1 = (unsigned char *)idea_expand_string_v2((char *)hlp, 
						len / 4, 
						0, 
						len & 0xff, 
						&l1);
    s2 = (unsigned char *)idea_expand_string_v2((char *)&(hlp[len / 4]), 
						(len / 2) - (len / 4), 
						85, 
						len & 0xff, 
						&l2);
    s3 = (unsigned char *)idea_expand_string_v2((char *)&(hlp[len / 2]), 
						(((len / 2) + (len / 4)) - 
						 (len / 2)), 
						170, 
						len & 0xff, 
						&l3);
    s4 = (unsigned char *)idea_expand_string_v2((char *)&(hlp[((len / 2) + 
							       (len / 4))]), 
						len - ((len / 2) + (len / 4)), 
						255, 
						len & 0xff, 
						&l4);
    v1 = irc_crc_buffer_numeric((char *)s1, l1);
    v2 = irc_crc_buffer_numeric((char *)s2, l2);
    v3 = irc_crc_buffer_numeric((char *)s3, l3);
    v4 = irc_crc_buffer_numeric((char *)s4, l4);
    key[0] = (v1 >> 16) & 0xffff;
    key[1] = v1 & 0xffff;
    key[2] = (v2 >> 16) & 0xffff;
    key[3] = v2 & 0xffff;
    key[4] = (v3 >> 16) & 0xffff;
    key[5] = v3 & 0xffff;
    key[6] = (v4 >> 16) & 0xffff;
    key[7] = v4 & 0xffff;
    g_free(hlp);
    g_free(s1);
    g_free(s2);
    g_free(s3);
    g_free(s4);
    return key;
}

/* Static stuff... */
static char *idea_expand_string_v2(const char *str,
				   int len,
				   int salt1,
				   int salt2,
				   int *rlen)
{
    unsigned char *r, *hlp;
    unsigned int crc;
    int x;


    if (len < 0)
	len = strlen(str);
    r = g_malloc(len + 7);
    r[4] = (unsigned char)(salt1 & 0xff);
    r[5] = (unsigned char)(salt2 & 0xff);
    memcpy(&(r[6]), str, len);
    len += 2;
    crc = irc_crc_buffer_numeric((char *)&(r[4]), len);
    r[0] = (crc >> 24) & 0xff;
    r[1] = (crc >> 16) & 0xff;
    r[2] = (crc >> 8) & 0xff;
    r[3] = crc & 0xff;
    len += 4;
    x = 3 + (r[0] & 3);
    while (x > 0) {
	hlp = g_malloc(len + 5);
	memcpy(&(hlp[4]), r, len);
	g_free(r);
	crc = irc_crc_buffer_numeric((char *)&(hlp[4]), len);
	hlp[0] = (crc >> 24) & 0xff;
	hlp[1] = (crc >> 16) & 0xff;
	hlp[2] = (crc >> 8) & 0xff;
	hlp[3] = crc & 0xff;
	r = hlp;
	len += 4;
	r[len] = 0;
	x--;
    }
    if (rlen)
	*rlen = len;
    return (char *)r;
}

/* eof (irc_idea_v2.c) */
