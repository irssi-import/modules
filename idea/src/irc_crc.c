/*   -*- c -*-
 *  
 *  $Id: irc_crc.c,v 1.1 2002/04/27 20:46:44 cras Exp $
 *  ----------------------------------------------------------------------
 *  Crypto for IRC.
 *  ----------------------------------------------------------------------
 *  Created      : Fri Feb 28 18:28:18 1997 tri
 *  Last modified: Wed Jan  6 15:22:22 1999 tri
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
#include "crc32.h"

char *irc_crc_string(const char *str)
{
	return g_strdup_printf("%08x", irc_crc_string_numeric(str));
}

unsigned int irc_crc_string_numeric(const char *str)
{
	return idea_crc32((unsigned char *)str, strlen(str));
}

char *irc_crc_buffer(const char *buf, int len)
{
	return g_strdup_printf("%08x", irc_crc_buffer_numeric(buf, len));
}

unsigned int irc_crc_buffer_numeric(const char *buf, int len)
{
	return idea_crc32((unsigned char *)buf, len);
}

int irc_check_crc_string(const char *str, const char *crc)
{
	int r;
	char *crc2;

	crc2 = irc_crc_string(str);
	r = strcmp(crc, crc2) == 0;
	g_free(crc2);
	return r;
}

int irc_check_crc_string_numeric(const char *str, unsigned int crc)
{
	return irc_crc_string_numeric(str) == crc;
}

int irc_check_crc_buffer(const char *str, int len, const char *crc)
{
	int r;
	char *crc2;

	crc2 = irc_crc_buffer(str, len);
	r = strcmp(crc, crc2) == 0;
	g_free(crc2);
	return r;
}

int irc_check_crc_buffer_numeric(const char *str, int len, unsigned int crc)
{
	return irc_crc_buffer_numeric(str, len) == crc;
}
