/*   -*- c -*-
 *  
 *  $Id: irc_b64.c,v 1.1 2002/04/27 20:46:47 cras Exp $
 *  ----------------------------------------------------------------------
 *  Crypto for IRC.
 *  ----------------------------------------------------------------------
 *  Created      : Fri Feb 28 18:28:18 1997 tri
 *  Last modified: Wed Jan  6 15:22:30 1999 tri
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

static char *b64_alpha =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char b64_dec[256] = { 255 };
static int b64_dec_valid = 0;

static void b64_build_dec(void)
{
    int i;

    for(i = 0; i < 64; i++)
	b64_dec[(int) b64_alpha[i]] = i;
    b64_dec['='] = 254;
    return;
}


char *b64_encode_buffer(const char *buf, int *buflen)
{
    char *r;
    int i, j, len;
    unsigned char *hlp;

    len = *buflen;
    hlp = (unsigned char *)buf;
    r = g_malloc(((len * 4) / 3) + 16);
    j = 0;
#define hlp_lu(x) (((x) < len) ? hlp[x] : 0)
    for (i = 0; i < len; i += 3) {
	r[j++] = b64_alpha[hlp_lu(i) >> 2];
	r[j++] = b64_alpha[(63 & (hlp_lu(i) << 4)) | (hlp_lu(i + 1) >> 4)];
	r[j++] = b64_alpha[(63 & (hlp_lu(i + 1) << 2)) | (hlp_lu(i + 2) >> 6)];
	r[j++] = b64_alpha[hlp_lu(i + 2) & 63];
	r[j] = 0;
	if ((i + 1) == len)
	    r[j - 1] = r[j - 2] = '=';
	if ((i + 2) == len)
	    r[j - 1] = '=';
    }
    *buflen = j;
    return r;
}
    
char *b64_decode_buffer(const char *buf, int *len)
{
    int l, i, j, e0, e1, e2, e3;
    unsigned char *r, *hlp;

    if (!b64_dec_valid) {
	b64_build_dec();
	b64_dec_valid = 1;
    }
    l = *len;
    
    if (l % 4) {
	/* Ignore garbage */
	l -= (l % 4);
    }
    *len = l * 3 / 4;
    r = g_malloc(*len + 4);
    hlp = (unsigned char *)buf;
    j = 0;
    for (i = 0; i < (l / 4); i++) {
	e0 = b64_dec[hlp[(i * 4) + 0]];
	e1 = b64_dec[hlp[(i * 4) + 1]];
	e2 = b64_dec[hlp[(i * 4) + 2]];
	e3 = b64_dec[hlp[(i * 4) + 3]];
	if ((e0 == 255) || (e1 == 255) || (e2 == 255) || (e3 == 255)) {
	    g_free(r);
	    return NULL;
	}
	r[j++] = (255 & (e0 << 2)) | (e1 >> 4);
	if (e2 != 254)
	    r[j++] = (255 & (e1 << 4)) | (e2 >> 2);
	else
	    (*len)--;
	if (e3 != 254)
	    r[j++] = (255 & (e2 << 6)) | e3;
	else
	    (*len)--;
	r[j] = 0;
    }
    return (char *)r;
}

