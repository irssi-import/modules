/*
   IDEA encryption plugin for irssi
   
   Copyright (C) 1999 Heikki Hannikainen <hessu@hes.iki.fi>

   April 2001, rewrite by Pekka Aleksi Knuutila <zur@edu.lahti.fi>
   
   IRC encryption using libirccrypt by Timo J. Rinne <tri@iki.fi>
   see README.irc_crypt and COPYRIGHT.irc_crypt
   
   Based on the sample plugin and some other original irssi code, which is
   Copyright (C) 1999 Timo Sirainen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "module.h"
#include "modules.h"
#include "module-formats.h"

#include "hilight-text.h"
#include "fe-messages.h"
#include "servers.h"
#include "queries.h"
#include "window-items.h"
#include "nicklist.h"
#include "settings.h"
#include "chat-protocols.h"
#include "printtext.h"
#include "levels.h"
#include "signals.h"
#include "channels.h"
#include "commands.h"
#include "irc_crypt.h"

static GString *tmpstr;
static int next_crypto;

static void idea_event_decrypt(SERVER_REC *server, const char *msg,
			       const char *nick, const char *addr,
			       const char *target)
{
	char *d_data = NULL, *d_nick = NULL;
	unsigned int d_tdiff;
	int r;
	
	g_return_if_fail(msg != NULL);

	if (nick == NULL) nick = "!server!";
	
	/* Check if the message is *E*ncrypted using IDEA */
	
	if (!next_crypto && strncmp(msg, "|*E*|IDEA|", 10) == 0) {

	/* It is, decrypt the message */

	    r = irc_decrypt_message(msg, &d_data, &d_nick, &d_tdiff);
	    if (!r) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			"Decryption error: %s", d_data);
		g_free(d_data);
		return;
	    }

	    /* Emit a decrypted signal, that we'll catch later */

	    if (settings_get_bool("idea_formats")) next_crypto = TRUE;
	    signal_stop();

	    signal_emit(signal_get_emitted(), 5, server,
			d_data, nick, addr, target);

	    g_free(d_data);
	    g_free(d_nick);
	}

}

static void idea_event_public(SERVER_REC *server, const char *msg,
			      const char *nick, const char *address,
			      const char *target, NICK_REC *nickrec)
{
	CHANNEL_REC *chanrec;
        const char *nickmode;
	int for_me, print_channel, level;
	char *color, *freemsg = NULL;

	g_return_if_fail(msg != NULL);

	if (!next_crypto)
		return;

	chanrec = channel_find(server, target);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis((WI_ITEM_REC *) chanrec, msg);

	for_me = nick_match_msg(chanrec, msg, server->nick);
	color = for_me ? NULL :
		hilight_match_nick(server, target, nick, address, MSGLEVEL_PUBLIC, msg);
	nickmode = channel_get_nickmode(chanrec, server->nick);

	level = MSGLEVEL_PUBLIC | (for_me || color != NULL ?
				   MSGLEVEL_HILIGHT : MSGLEVEL_NOHILIGHT);

	print_channel = chanrec == NULL ||
		!window_item_is_active((WI_ITEM_REC *) chanrec);
	if (!print_channel && settings_get_bool("print_active_channel") &&
	    window_item_window((WI_ITEM_REC *) chanrec)->items->next != NULL)
		print_channel = TRUE;

	if (!print_channel) {
		/* on this channel */
		if (color != NULL) {
			printformat(server, target, level,
				    TXT_IDEA_PUBMSG_HILIGHT,
				    color, nick, msg, nickmode);
		} else {
			printformat(server, target, level,
				    for_me ? TXT_IDEA_PUBMSG_ME : TXT_IDEA_PUBMSG,
				    nick, msg, nickmode);
		}
	} else {
		/* on some other channel */
		if (color != NULL) {
			printformat(server, target, level,
				    TXT_IDEA_PUBMSG_HILIGHT_CHANNEL,
				    color, nick, target, msg, nickmode);
		} else {
			printformat(server, target, level,
				    for_me ?  TXT_IDEA_PUBMSG_ME_CHANNEL :
				    TXT_IDEA_PUBMSG_CHANNEL,
				    nick, target, msg, nickmode);

		}
	}

        g_free_not_null(freemsg);
	g_free_not_null(color);

	next_crypto = FALSE;
	signal_stop();
}

static void idea_event_private(SERVER_REC *server, const char *msg,
			       const char *nick, const char *address)
{
	QUERY_REC *query;
        char *freemsg = NULL;

	if (!next_crypto)
		return;

	query = query_find(server, nick);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis((WI_ITEM_REC *) query, msg);

	printformat(server, nick, MSGLEVEL_MSGS,
		    query == NULL ? TXT_IDEA_MSG_PRIVATE :
		    TXT_IDEA_MSG_PRIVATE_QUERY, nick, address, msg);

	g_free_not_null(freemsg);

	next_crypto = FALSE;
	signal_stop();
}

/*
 * SYNTAX: KEY ADD [-known] [<target>] <key>
 */
 
static void command_key_add(const char *data, SERVER_REC *server,
			    WI_ITEM_REC *item)
{
	GHashTable *optlist;
	char *target, *key;
	void *free_arg;

	g_return_if_fail(data != NULL);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTIONS,
			    "key add", &optlist, &target, &key))
		return;

	if (*target == '\0')
		cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	if (*key == '\0') {
                /* one paramter given - it's the key */
                key = target;
		target = "";
	}

	if (g_hash_table_lookup(optlist, "known") != NULL) {
		irc_add_known_key(key);
		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			  "Added known key \"%s\"", key);
	} else {
		if (*target == '\0') {
			if (item != NULL)
				target = item->name;
			else
				printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
					  "Please define channel/nick");
		}
		if (target && *target) {
			irc_add_default_key(target, key);
			printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
				  "Added default key \"%s\" for \"%s\"",
				  key, target);
		}
	}
	cmd_params_free(free_arg);
}

/* SYNTAX: KEY DROP -all [known | default]
 *         KEY DROP -known key
 *         KEY DROP <target>
 */

static void command_key_drop(const char *data, SERVER_REC *server,
	                WI_ITEM_REC *item)
{
	GHashTable *optlist;
	char *target;
	void *free_arg;

	g_return_if_fail(data != NULL);

	if (!cmd_get_params(data, &free_arg, 1 | PARAM_FLAG_OPTIONS,
			   "key drop", &optlist, &target))
		return;

	if (g_hash_table_lookup(optlist, "all") == NULL && *target == '\0')
		cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	if (g_hash_table_lookup(optlist, "all") != NULL) {
		char *msg = NULL;

		if (g_strcasecmp(target, "known") == 0) {
			irc_delete_all_known_keys();
			msg = " known";
		} else if (g_strcasecmp(target, "default") == 0) {
			irc_delete_all_default_keys();
			msg = " default";
		} else if (*target == '\0') {
			irc_delete_all_keys();
			msg = "";
		} else {
			printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
				  "Invalid argument: %s", target);
		}
		if (msg != NULL)
			printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
				  "Dropped all%s keys.", msg);

	} else if (g_hash_table_lookup(optlist, "known") != NULL) {
		irc_delete_known_key(target);
		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			  "Dropped known key \"%s\".", target);

	} else {
		irc_delete_default_key(target);
		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			  "Dropped default key for \"%s\".", target);
	}

	cmd_params_free(free_arg);
}

static void command_key(const char *data, SERVER_REC *server,
			WI_ITEM_REC *item)
{
	command_runsub("key", data, server, item);
}
/*
 *	Send an IDEA encrypted message to target
 */

static void send_idea(SERVER_REC *server, const char *target,
		      const char *msg, int target_type, int send_signal)
{
        MODULE_SERVER_REC *mserver;
	char *ct;

	ct = irc_encrypt_message_to_address(target, server->nick, msg);
	if (!ct) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			"IDEA encryption failed! Perhaps no key set for \"%s\"?", target);
		return;
	}

	mserver = MODULE_DATA(server);
	mserver->orig_send_message(server, target, ct, target_type);

	if (settings_get_bool("idea_formats")) next_crypto = TRUE;

	if (send_signal) {
		/* send out a signal that we'll later catch again */
		if (server->ischannel(server, target)) {

			signal_emit("message own_public", 3, server, msg, target);

		} else {

			signal_emit("message own_private", 4, server, msg, target, target);

		}
	}

	g_free(ct);
}

static void idea_event_ownmsg(SERVER_REC *server, 
	                      char *msg, char *target, char *orig_target)
{
    CHANNEL_REC *chanrec;
    QUERY_REC *queryrec;
    char *nickmode, *freemsg = NULL;

    if (next_crypto)
    {

	chanrec = channel_find(server, target);
	nickmode = channel_get_nickmode(chanrec, server->nick);

	if (settings_get_bool("emphasis"))
	    msg = freemsg = expand_emphasis((WI_ITEM_REC *) chanrec, msg);

	if (IS_CHANNEL(chanrec)) {
	    /* msg to channel */ 
	    if (window_item_is_active((WI_ITEM_REC *) chanrec)) {
		printformat(server, target,
			MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT,
			TXT_IDEA_OWN_MSG, server->nick, msg, nickmode);
	    } else { 
		printformat(server, target,
			MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT,
			TXT_IDEA_OWN_MSG_CHANNEL, server->nick, target, msg, nickmode);
	    }

	} else {
	    /* private message */ 

	    queryrec = query_find(server, target);

	    printformat(server, target, MSGLEVEL_MSGS | MSGLEVEL_NOHILIGHT,
		    queryrec == NULL ? TXT_IDEA_OWN_MSG_PRIVATE : TXT_IDEA_OWN_MSG_PRIVATE_QUERY, 
		    target, msg, server->nick);

	}

        g_free_not_null(freemsg);

	next_crypto = FALSE;
	signal_stop();

    }
}

/*
 *	Command: /ideam
 */

static void command_ideam(const char *data, SERVER_REC *server)
{
	char *target, *msg;
	void *free_arg;
        int target_type;

	cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &target, &msg);

	if (*target == '\0' || *msg == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	if (server == NULL || !server->connected) cmd_param_error(CMDERR_NOT_CONNECTED);

	target_type = server_ischannel(server, target) ?
		SEND_TARGET_CHANNEL : SEND_TARGET_NICK;
	send_idea(server, target, msg, target_type, TRUE);
	cmd_params_free(free_arg);
}

/*
 *	Command: /idea
 */

static void command_idea(const char *data, SERVER_REC *server,
			 WI_ITEM_REC *item)
{
	int target_type;

	if (server == NULL || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (item == NULL)
		cmd_return_error(CMDERR_NOT_JOINED);

	if (*data == '\0')
		cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	target_type = IS_CHANNEL(item) ?
		SEND_TARGET_CHANNEL : SEND_TARGET_NICK;
	send_idea(server, item->name, data, target_type, TRUE);
}

static void send_idea_msg(SERVER_REC *server, const char *target,
			  const char *msg, int target_type)
{
	MODULE_SERVER_REC *mserver;

	if (settings_get_bool("idea_autocrypt") &&
	    irc_get_default_key(target)) {
		send_idea(server, target, msg, target_type, FALSE);
	} else {
		mserver = MODULE_DATA(server);
		mserver->orig_send_message(server, target, msg, target_type);
	}
}

static void server_register_idea(SERVER_REC *server)
{
	MODULE_SERVER_REC *rec;

	g_return_if_fail(server != NULL);

	rec = g_new0(MODULE_SERVER_REC, 1);
	MODULE_DATA_SET(server, rec);
	rec->orig_send_message = server->send_message;
        server->send_message = send_idea_msg;
}

static void server_unregister_idea(SERVER_REC *server)
{
	MODULE_SERVER_REC *rec;

	rec = MODULE_DATA(server);
        server->send_message = rec->orig_send_message;
}

static void sig_disconnected(SERVER_REC *server)
{
	MODULE_SERVER_REC *rec;

	g_return_if_fail(server != NULL);

	rec = MODULE_DATA(server);
        g_free(rec);
}

void idea_init(void)
{
	g_slist_foreach(servers, (GFunc) server_register_idea, NULL);

	theme_register(idea_formats);

        signal_add_first("message private", (SIGNAL_FUNC) idea_event_decrypt);
        signal_add_first("message public", (SIGNAL_FUNC) idea_event_decrypt);

        signal_add("message private", (SIGNAL_FUNC) idea_event_private);
        signal_add("message public", (SIGNAL_FUNC) idea_event_public);
	signal_add("message own_private", (SIGNAL_FUNC) idea_event_ownmsg);
	signal_add("message own_public", (SIGNAL_FUNC) idea_event_ownmsg);

	signal_add("server connected", (SIGNAL_FUNC) server_register_idea);
	signal_add("server disconnected", (SIGNAL_FUNC) sig_disconnected);

	settings_add_bool("idea", "idea_autocrypt", TRUE);
	settings_add_bool("idea", "idea_formats", TRUE);

        command_bind("key", NULL, (SIGNAL_FUNC) command_key);
	command_bind("key add", NULL, (SIGNAL_FUNC) command_key_add);
	command_bind("key drop", NULL, (SIGNAL_FUNC) command_key_drop);
        command_bind("idea", NULL, (SIGNAL_FUNC) command_idea);
        command_bind("ideam", NULL, (SIGNAL_FUNC) command_ideam);

	command_set_options("key add", "known");
	command_set_options("key drop", "known all");

        tmpstr = g_string_new(NULL);

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		"IDEA-encryption plugin loaded. Messages will be encrypted "
		"whenever possible if idea_autocrypt is set, alternatively you "
		"can use /idea and /ideam. Modify your keyring with /key in "
		"advance."
		);

        module_register("idea", "core");
}

void idea_deinit(void)
{
	irc_delete_all_keys();

	g_slist_foreach(servers, (GFunc) server_unregister_idea, NULL);

        signal_remove("message private", (SIGNAL_FUNC) idea_event_decrypt);
        signal_remove("message public", (SIGNAL_FUNC) idea_event_decrypt);

        signal_remove("message private", (SIGNAL_FUNC) idea_event_private);
        signal_remove("message public", (SIGNAL_FUNC) idea_event_public);
	signal_remove("message own_private", (SIGNAL_FUNC) idea_event_ownmsg);
	signal_remove("message own_public", (SIGNAL_FUNC) idea_event_ownmsg);

	signal_remove("server connected", (SIGNAL_FUNC) server_register_idea);
	signal_remove("server disconnected", (SIGNAL_FUNC) sig_disconnected);

        command_unbind("key", (SIGNAL_FUNC) command_key);
        command_unbind("idea", (SIGNAL_FUNC) command_idea);
	command_unbind("ideam", (SIGNAL_FUNC) command_ideam);

	theme_unregister();

        g_string_free(tmpstr, TRUE);
}
