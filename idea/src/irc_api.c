/*   -*- c -*-
 *  
 *  $Id: irc_api.c,v 1.1 2002/04/27 20:46:44 cras Exp $
 *  ----------------------------------------------------------------------
 *  Crypto for IRC.
 *  ----------------------------------------------------------------------
 *  Created      : Fri Feb 28 18:28:18 1997 tri
 *  Last modified: Wed Jan  6 15:22:36 1999 tri
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
#include "misc.h"

#define KEY_ALLOC_STEP 4

typedef struct {
    char *fingerprint;
    char *key;
} irc_key, *irc_key_t;

typedef struct {
    char *addr;
    char *key;
} irc_default_key, *irc_default_key_t;

static irc_key_t known_keys = NULL;
static int num_known_keys = 0;
static int spc_known_keys = 0;

static irc_default_key_t default_keys = NULL;
static int num_default_keys = 0;
static int spc_default_keys = 0;

static int irc_default_key_expand_version = 3;

static const char *irc_get_known_key(const char *fingerprint);
static int irc_add_known_key_internal(const char *key, int version);

static int irc_parse_encrypted_message(const char *msg,
				       char **type,
				       int *ver_maj,
				       int *ver_min,
				       char **fingerprint,
				       char **data)
{
	char **list, *p;
        int ret;

	/* |*E*|type|ver_maj.ver_min|fingerprint|data|  */
	list = g_strsplit(msg, "|", -1);
        ret = 0;
	if (strarray_length(list) == 6 && strcmp(list[1], "*E*") == 0) {
		if (type != NULL)
			*type = g_strdup(list[2]);

		if (ver_maj != NULL) {
			p = strchr(list[3], '.');
			if (p != NULL) *p++ = '\0'; else p = "";
			*ver_maj = atoi(list[3]);

			if (ver_min != NULL)
				*ver_min = atoi(p);
		}

		if (fingerprint != NULL)
			*fingerprint = g_strdup(list[4]);

		if (data != NULL)
			*data = g_strdup(list[5]);

                ret = 1;
	}

        g_strfreev(list);
        return ret;
}

const char *irc_get_default_key(const char *addr)
{
    int i;

    if (!default_keys)
	return NULL;
    
    for (i = 0; i < num_default_keys; i++)
	if (!(g_strcasecmp(default_keys[i].addr, addr))) {
	    return default_keys[i].key;
	}
    return NULL;
}

static const char *irc_get_known_key(const char *fingerprint)
{
    int i;

    if (!known_keys)
	return NULL;
    
    for (i = 0; i < num_known_keys; i++)
	if (!(g_strcasecmp(known_keys[i].fingerprint, fingerprint)))
	    return known_keys[i].key;
    return NULL;
}

static int irc_add_known_key_internal(const char *key, int version)
{
    int i;
    char *fp;

    if (!known_keys) {
	known_keys = g_new0(irc_key, KEY_ALLOC_STEP);
	num_known_keys = 0;
	spc_known_keys = KEY_ALLOC_STEP;
    }
    if (num_known_keys == spc_known_keys) {
	irc_key_t n_keys;

	n_keys = g_new0(irc_key, KEY_ALLOC_STEP + spc_known_keys);
	memcpy(n_keys, known_keys, num_known_keys * sizeof (irc_key));
	g_free(known_keys);
	known_keys = n_keys;
	spc_known_keys += KEY_ALLOC_STEP;
    }
    fp = irc_key_fingerprint(key, version);
    for (i = 0; i < num_known_keys; i++)
	if (!(strcmp(known_keys[i].fingerprint, fp))) {
	    g_free(fp);
	    return 1; /* Already there */
	}
    known_keys[num_known_keys].key = g_strdup(key);
    known_keys[num_known_keys].fingerprint = fp;
    num_known_keys++;
    return 1;
}

int irc_set_key_expand_version(int n)
{
	int x;

	if ((n == 1) || (n == 2) || (n == 3)) {
		x = irc_default_key_expand_version;
		irc_default_key_expand_version = n;
		return x;
	}
	return 0;
}

int irc_key_expand_version(void)
{
	return irc_default_key_expand_version;
}

int irc_add_known_key(const char *key)
{
	int r;

	r = 1;
	r &= irc_add_known_key_internal(key, 1);
	r &= irc_add_known_key_internal(key, 2);
	r &= irc_add_known_key_internal(key, 3);

	return r;
}

int irc_delete_all_known_keys(void)
{
	int i;

	for (i = 0; i < num_known_keys; i++) {
		g_free(known_keys[i].key);
		g_free(known_keys[i].fingerprint);
	}
        g_free_and_null(known_keys);
	num_known_keys = 0;
	return 1;
}

int irc_delete_all_default_keys(void)
{
	int i;

	for (i = 0; i < num_default_keys; i++) {
		g_free(default_keys[i].key);
		g_free(default_keys[i].addr);
	}
        g_free_and_null(default_keys);
	num_default_keys = 0;
	return 1;
}

int irc_delete_all_keys(void)
{
	irc_delete_all_default_keys();
	irc_delete_all_known_keys();
	return 1;
}

int irc_delete_known_key(const char *key)
{
	int i;

	if (!known_keys)
		return 0;

	for (i = 0; i < num_known_keys; i++)
		if (!(strcmp(known_keys[i].key, key))) {
			g_free(known_keys[i].key);
			g_free(known_keys[i].fingerprint);
			num_known_keys--;
			if (i < num_known_keys)
				memcpy(&(known_keys[i]),
				       &(known_keys[i + 1]),
				       (num_known_keys - i) * sizeof (irc_key));
			return 1;
		}

	return 0;
}

int irc_add_default_key(const char *addr, const char *key)
{
	if (!default_keys) {
		default_keys = g_new0(irc_default_key, KEY_ALLOC_STEP);
		num_default_keys = 0;
		spc_default_keys = KEY_ALLOC_STEP;
	}
	irc_delete_default_key(addr);
	if (!key)
		return 1;

	if (num_default_keys == spc_default_keys) {
		irc_default_key_t n_keys;

		n_keys = g_new0(irc_default_key,
				KEY_ALLOC_STEP + spc_default_keys);
		memcpy(n_keys, default_keys,
		       num_default_keys * sizeof (irc_default_key));
		g_free(default_keys);
		default_keys = n_keys;
		spc_default_keys += KEY_ALLOC_STEP;
	}
	default_keys[num_default_keys].key = g_strdup(key);
	default_keys[num_default_keys].addr = g_strdup(addr);
	num_default_keys++;
	irc_add_known_key(key);
	return 1;
}

int irc_delete_default_key(const char *addr)
{
	int i;

	if (!default_keys)
		return 0;

	for (i = 0; i < num_default_keys; i++) {
		if (g_strcasecmp(default_keys[i].addr, addr) == 0) {
			g_free(default_keys[i].key);
			g_free(default_keys[i].addr);
			num_default_keys--;
			if (i < num_default_keys) {
				memcpy(&(default_keys[i]),
				       &(default_keys[i + 1]),
				       (num_default_keys - i) *
				       sizeof (irc_default_key));
			}
			return 1;
		}
	}

	return 0;
}

char *irc_encrypt_message_to_address(const char *addr, const char *nick,
				     const char *message)
{
	const char *key;
	char *r;

	key = irc_get_default_key(addr);
	if (!key)
		return NULL;
	r = irc_encrypt_message_with_key(key, nick, message);
	return r;
}

char *irc_encrypt_message_with_key(const char *key, const char *nick,
				   const char *message)
{
	char *tmp, *fingerprint, *data, *ret;
        int len;

        // nick + \001 + %08lx(time) + \001 + message
	tmp = g_strdup_printf("%s\001%08lx\001%s", nick,
			      (long) time(NULL), message);
	len = strlen(tmp);
	data = irc_encrypt_buffer(key, tmp, &len);
	g_free(tmp);

        fingerprint = irc_key_fingerprint(key, irc_default_key_expand_version);

	ret = g_strdup_printf("|*E*|IDEA|%d.0|%s|%s|",
			      irc_default_key_expand_version,
			      fingerprint, data);
        g_free(fingerprint);
	g_free(data);

	return ret;
}

int irc_decrypt_message(const char *msg,
			char **message, char **nick, unsigned int *tdiff)
{
        const char *key;
	char *data, *fingerprint, *type;
	int vmaj, vmin;
	char **list, *buf;
	int version, len, ret;

        ret = 0;
	if (!(irc_parse_encrypted_message(msg, &type, &vmaj, &vmin,
					  &fingerprint, &data))) {
		if (message)
			*message = g_strdup("Invalid message format");
		return 0;
	}

	if (strcmp(type, "IDEA")) {
		if (message)
			*message = g_strdup("Unknown algorithm");
		goto i_d_m_fail;
	}
	if ((vmaj == 1) && (vmin == 0)) {
		version = 1;
	} else if ((vmaj == 2) && (vmin == 0)) {
		version = 2;
	} else if ((vmaj == 3) && (vmin == 0)) {
		version = 3;
	} else {
		if (message)
			*message = g_strdup("Unknown version");
		goto i_d_m_fail;
	}

	key = irc_get_known_key(fingerprint);
	if (!key) {
		if (message)
			*message = g_strdup("Unknown key");
		goto i_d_m_fail;
	}

	len = strlen(data);
	buf = irc_decrypt_buffer(key, data, &len, version);
	if (!buf) {
		g_free(buf);
		if (message)
			*message = g_strdup("Decryption failed");
		goto i_d_m_fail;
	}

	// nick + \001 + %08lx(time) + \001 + message
	list = g_strsplit(buf, "\001", -1);
	if (strarray_length(list) != 3) {
		g_strfreev(list);

		if (message)
			*message = g_strdup("Invalid data contents");
		goto i_d_m_fail;
	}

	if (nick != NULL)
		*nick = g_strdup(list[0]);
	if (tdiff != NULL) {
		*tdiff = time(NULL) - strtol(list[1], NULL, 16);
		if (*tdiff < 0) *tdiff = -*tdiff;
	}
	if (message != NULL)
                *message = g_strdup(list[2]);

	g_free(buf);
	g_strfreev(list);
        ret = 1;

i_d_m_fail:
	g_free(data);
	g_free(fingerprint);
	g_free(type);
	return ret;
}

int irc_is_encrypted_message_p(const char *msg)
{
	return irc_parse_encrypted_message(msg, NULL, NULL, NULL, NULL, NULL);
}
