/*   -*- c -*-
 *  
 *  $Id: irc_crypt.c,v 1.1 2002/04/27 20:46:44 cras Exp $
 *  ----------------------------------------------------------------------
 *  Crypto for IRC.
 *  ----------------------------------------------------------------------
 *  Created      : Fri Feb 28 18:28:18 1997 tri
 *  Last modified: Thu Jan  7 13:59:24 1999 tri
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

unsigned short *irc_build_key(const char *str, int version)
{
    switch (version) {
    case 1:
	return irc_idea_key_expand_v1(str, -1);
    case 2:
	return irc_idea_key_expand_v2(str, -1);
    case 3:
	return irc_idea_key_expand_v3(str, -1);
    default:
	return NULL;
    }
}

char *irc_key_fingerprint(const char *key, int version)
{
    switch (version) {
    case 1:
	return irc_idea_key_fingerprint_v1(key);
    case 2:
	return irc_idea_key_fingerprint_v2(key);
    case 3:
	return irc_idea_key_fingerprint_v3(key);
    default:
	return NULL;
    }
}

char *irc_encrypt_buffer(const char *key, const char *str, int *buflen)
{
    unsigned short wk[52];
    unsigned short ctx[4];
    unsigned short cb[4];

    unsigned short *tmpkey;
    int i, padlen, len;
    unsigned char *buf;
    char *hlp;
    static int srandom_called = 0;

    if (!srandom_called) {
	srandom(time(NULL) ^ getpid());
	srandom_called = 1;
    }
    len = *buflen;
    padlen = 8 - (len % 8);
    if (padlen == 0)
	padlen = 8;
    buf = g_malloc(len + 9 + 16);
    for (i = 0; i < padlen; i++)
	buf[i] = random() & 255;
    memcpy(&(buf[i + 8]), str, len);
    hlp = irc_crc_buffer(str, len);
    memcpy(&(buf[i]), hlp, 8);
    g_free(hlp);
    buf[0] = ((unsigned char)(buf[0] & 31)) |
	     ((unsigned char)(((padlen - 1) & 7) << 5));
    len += 8 + padlen;

/*fprintf(stderr, ">>>str=\"%s\", len=%d, pad=%d\n", str, len, padlen);*/

    tmpkey = irc_build_key(key, irc_key_expand_version());
    ExpandUserKey(tmpkey, wk);
    g_free(tmpkey);
    ctx[0] = ctx[1] = ctx[2] = ctx[3] = 0;
    for (i = 0; i < (len / 8); i++) {
	cb[0] = (((unsigned short)(buf[(i * 8) + 0]) << 8) | buf[(i * 8) + 1])
	    ^ ctx[0];
	cb[1] = (((unsigned short)(buf[(i * 8) + 2]) << 8) | buf[(i * 8) + 3])
	    ^ ctx[1];
	cb[2] = (((unsigned short)(buf[(i * 8) + 4]) << 8) | buf[(i * 8) + 5])
	    ^ ctx[2];
	cb[3] = (((unsigned short)(buf[(i * 8) + 6]) << 8) | buf[(i * 8) + 7])
	    ^ ctx[3];
	Idea(cb, ctx, wk);
	buf[(i * 8) + 0] = (ctx[0] >> 8) & 0xff;
	buf[(i * 8) + 1] = ctx[0] & 0xff;
	buf[(i * 8) + 2] = (ctx[1] >> 8) & 0xff;
	buf[(i * 8) + 3] = ctx[1] & 0xff;
	buf[(i * 8) + 4] = (ctx[2] >> 8) & 0xff;
	buf[(i * 8) + 5] = ctx[2] & 0xff;
	buf[(i * 8) + 6] = (ctx[3] >> 8) & 0xff;
	buf[(i * 8) + 7] = ctx[3] & 0xff;
    }
    hlp = b64_encode_buffer((char *)buf, &len);
    *buflen = len;
    g_free(buf);
    return hlp;
}

char *irc_decrypt_buffer(const char *key, const char *str,
			 int *buflen, int version)
{
    unsigned short wk[52];
    unsigned short ctx[4];
    unsigned short cb[4];
    unsigned short tb[4];
    unsigned short *tmpkey;
    int i, padlen;
    int len;
    unsigned char *buf, *hlp;

    buf = (unsigned char *)b64_decode_buffer(str, buflen);
    if (!buf)
	return NULL;
    len = *buflen;
    if ((len % 8) || (len < 16)) {
	g_free(buf);
	return NULL;
    }
    tmpkey = irc_build_key(key, version);
    ExpandUserKey(tmpkey, wk);
    g_free(tmpkey);
    InvertIdeaKey(wk, wk);
    ctx[0] = ctx[1] = ctx[2] = ctx[3] = 0;
    for (i = 0; i < (len / 8); i++) {
	tb[0] = cb[0] = 
	    (((unsigned short)(buf[(i * 8) + 0]) << 8) | buf[(i * 8) + 1]);
	tb[1] = cb[1] =
	    (((unsigned short)(buf[(i * 8) + 2]) << 8) | buf[(i * 8) + 3]);
	tb[2] = cb[2] = 
	    (((unsigned short)(buf[(i * 8) + 4]) << 8) | buf[(i * 8) + 5]);
	tb[3] = cb[3] = 
	    (((unsigned short)(buf[(i * 8) + 6]) << 8) | buf[(i * 8) + 7]);
	Idea(cb, cb, wk);
	cb[0] = cb[0] ^ ctx[0];
	cb[1] = cb[1] ^ ctx[1];
	cb[2] = cb[2] ^ ctx[2];
	cb[3] = cb[3] ^ ctx[3];
	ctx[0] = tb[0];
	ctx[1] = tb[1];
	ctx[2] = tb[2];
	ctx[3] = tb[3];
	buf[(i * 8) + 0] = (cb[0] >> 8) & 0xff;
	buf[(i * 8) + 1] = cb[0] & 0xff;
	buf[(i * 8) + 2] = (cb[1] >> 8) & 0xff;
	buf[(i * 8) + 3] = cb[1] & 0xff;
	buf[(i * 8) + 4] = (cb[2] >> 8) & 0xff;
	buf[(i * 8) + 5] = cb[2] & 0xff;
	buf[(i * 8) + 6] = (cb[3] >> 8) & 0xff;
	buf[(i * 8) + 7] = cb[3] & 0xff;
    }
    buf[i * 8] = 0;
    padlen = (buf[0] >> 5) + 1;
/*fprintf(stderr, ">>>str=\"...\", len=%d, pad=%d\n", len, padlen);*/
    hlp = (unsigned char *)g_strdup((char *)&(buf[padlen]));
    g_free(buf);
    buf = (unsigned char *)hlp;
    hlp = (unsigned char *)g_strdup((char *)&(buf[8]));
    buf[8] = 0;
    (*buflen) -= padlen + 8;
    if (!(irc_check_crc_buffer((char *)hlp, *buflen, (char *)buf))) {
	g_free(hlp);
	g_free(buf);
	return NULL;
    }
    g_free(buf);
    return (char *)hlp;
}

/*
 * Key expand version 3 stuff.
 */
/*
 * End of key expand version 2 stuff.
 */

/*
 * Key expand version 3 stuff.
 */

/*
 * End of key expand version 3 stuff.
 */
