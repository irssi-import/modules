#include "module.h"
#include "network.h"
#include "line-split.h"
#include "signals.h"
#include "levels.h"
#include "settings.h"

#include "servers.h"
#include "channels.h"
#include "nicklist.h"

#include "fe-common/core/printtext.h"

#define MAX_MSG_QUEUE 10
#define MAX_MSGLINK_LEN 20
#define REFRESH_SECS 30

typedef struct {
	int read:1;
	int personal:1;
	char *message;
} MOBILE_MSG_REC;

typedef struct {
	CHANNEL_REC *channel;
	GList *last_msgs;
} MOBILE_CHANNEL_REC;

typedef struct {
        GIOChannel *handle;
	int tag;
	int connected:1;
	int wml:1;
	LINEBUF_REC *buffer;

	SERVER_REC *server;
	MOBILE_CHANNEL_REC *channel;
} CLIENT_REC;

static GIOChannel *listen_handle;
static int listen_tag;
static CLIENT_REC *client;
static GSList *mobile_channels;
static int cache_counter; /* to prevent caching */

void client_print(CLIENT_REC *client, const char *data, ...)
{
	va_list args;
	char *str;

	g_return_if_fail(client != NULL);
	g_return_if_fail(data != NULL);

	va_start(args, data);

	str = g_strdup_vprintf(data, args);
	net_transmit(client->handle, str, strlen(str));
	g_free(str);

	va_end(args);
}

static void mobile_msg_destroy(MOBILE_CHANNEL_REC *channel,
			       MOBILE_MSG_REC *rec)
{
	channel->last_msgs = g_list_remove(channel->last_msgs, rec);

	g_free(rec->message);
	g_free(rec);
}

static int mobile_msg_remove_oldest(MOBILE_CHANNEL_REC *channel)
{
	GList *tmp;

	for (tmp = channel->last_msgs; tmp != NULL; tmp = tmp->next) {
		MOBILE_MSG_REC *rec = tmp->data;

		if (!rec->personal || rec->read) {
			mobile_msg_destroy(channel, rec);
			return TRUE;
		}
	}
	
	return FALSE;
}

static void mobile_msg_add(MOBILE_CHANNEL_REC *channel,
			   MOBILE_MSG_REC *rec)
{
	GString *str;
	char *p;
	
	while (g_list_length(channel->last_msgs) > MAX_MSG_QUEUE) {
		/* remove one of the old messages */
		if (!mobile_msg_remove_oldest(channel))
			break;
	}
	
	str = g_string_new(NULL);
	p = rec->message;
	for (p = rec->message; *p != '\0'; p++) {
		if (*p == '<')
			g_string_append(str, "&lt;");
		else if (*p == '>')
			g_string_append(str, "&gt;");
		else if (*p == '&')
			g_string_append(str, "&amp;");
		else if (*p == '"')
			g_string_append(str, "&quot;");
		else if (*p == '\'')
			g_string_append(str, "&#039;");
		else
			g_string_append_c(str, *p);
	}
	g_free(rec->message);
	rec->message = str->str;
	g_string_free(str, FALSE);

        channel->last_msgs = g_list_append(channel->last_msgs, rec);
}

static MOBILE_CHANNEL_REC *mobile_channel_find(CHANNEL_REC *channel)
{
	GSList *tmp;

	for (tmp = mobile_channels; tmp != NULL; tmp = tmp->next) {
		MOBILE_CHANNEL_REC *rec = tmp->data;

		if (rec->channel == channel)
			return rec;
	}

	return NULL;
}

static void mobile_channel_create(CHANNEL_REC *channel)
{
	MOBILE_CHANNEL_REC *rec;

	rec = g_new0(MOBILE_CHANNEL_REC, 1);
	rec->channel = channel;

	mobile_channels = g_slist_append(mobile_channels, rec);
}

static void mobile_channel_destroy(MOBILE_CHANNEL_REC *rec)
{
	mobile_channels = g_slist_remove(mobile_channels, rec);

	while (rec->last_msgs != NULL)
		mobile_msg_destroy(rec, rec->last_msgs->data);
	g_free(rec);
}

static void client_disconnect(CLIENT_REC *rec)
{
	if (rec->buffer != NULL) line_split_free(rec->buffer);
	if (rec->handle != NULL) net_disconnect(rec->handle);
	if (rec->tag != -1) g_source_remove(rec->tag);
	g_free(rec);

	client = NULL;
}

static char *client_get_link(CLIENT_REC *client,
			     const char *server, const char *channel)
{
	return g_strdup_printf("?server=%s&amp;channel=%s&amp;counter=%x",
			       server, channel, cache_counter);
}

static void client_link(CLIENT_REC *client, const char *text,
			const char *server, const char *channel)
{
	char *link;

        link = client_get_link(client, server, channel);
	if (client->wml) {
		client_print(client, "<anchor>%s", text);
		client_print(client, "<go href=\"%s\" />", link);
		client_print(client, "</anchor>\n");
	} else {
		client_print(client, "<a href=\"%s\">%s</a>\n", link, text);
	}
        g_free(link);
}

static void send_client_channel_page(CLIENT_REC *client)
{
	GSList *tmp;
	char *str;

	client_print(client, "Channels:<br /><br />\n\n");

	for (tmp = channels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_REC *channel = tmp->data;

		str = g_strdup_printf("%s (%s)", channel->name,
				      channel->server->tag);
		client_link(client, str,
			    channel->server->tag, channel->name+1);
		g_free(str);

		client_print(client, "<br />\n");
	}

	client_print(client, "\n");
}

static void send_client_msg_page(CLIENT_REC *client)
{
	CHANNEL_REC *channel;
	GList *tmp;
	int num;

	channel = client->channel->channel;

	if (client->wml) {
		char *link;

		/* refresh menu item */
		client_print(client, "<do type=\"options\" label=\"[Refresh]\" name=\"refresh\">\n");
		link = client_get_link(client, channel->server->tag, channel->name+1);
		client_print(client, "  <go href=\"%s\" />\n", link);
                g_free(link);
		client_print(client, "</do>");
		/* write message menu item */
		client_print(client, "<do type=\"options\" label=\"[Send message]\" name=\"writemsg\">\n");
		client_print(client, "  <go href=\"#writemsg\" />\n");
		client_print(client, "</do>");
	}

	/* write short links for messages */
	tmp = g_list_last(client->channel->last_msgs);
	for (num = 0; tmp != NULL; tmp = tmp->prev, num++) {
		MOBILE_MSG_REC *rec = tmp->data;

		rec->read = TRUE;
		if (strlen(rec->message) <= MAX_MSGLINK_LEN)
			client_print(client, "%s", rec->message);
		else {
			char *str;

			str = g_strndup(rec->message, MAX_MSGLINK_LEN);
			client_print(client, rec->personal ?
				     "<b><a href=\"#m%d\">%s</a></b>" :
				     "<a href=\"#m%d\">%s</a>",
				     num, str);
			g_free(str);
		}
		client_print(client, "<br />\n");
	}
	client_print(client, "<br />\n");

	if (client->wml) {
		client_print(client, "<a href=\"#writemsg\">Send message</a>\n");
	} else {
		client_print(client, "<form method=\"post\">\n");
		client_print(client, "<input type=\"text\" name=\"msg\" />\n");
		client_print(client, "<input type=\"submit\" value=\"Send message\" />\n");
		client_print(client, "</form>\n");
	}

	client_link(client, "Refresh", channel->server->tag,
		    channel->name+1);
	client_print(client, "<br />\n");

	client_link(client, "Change channel", "", "");
	client_print(client, "<br />\n");

	client_print(client, "<br />\n");

	if (client->wml) {
                client_print(client, "</p></card><card id=\"writemsg\" title=\"Write message\"><p>");
		client_print(client, "<input type=\"text\" name=\"msg\" />\n");
		client_print(client, "<anchor>Send<go method=\"get\" href=\"?\">\n");
		client_print(client, "<postfield name=\"server\" value=\"%s\" />\n", channel->server->tag);
		client_print(client, "<postfield name=\"channel\" value=\"%s\" />\n", channel->name+1);
		client_print(client, "<postfield name=\"counter\" value=\"%x\" />\n", cache_counter);
		client_print(client, "<postfield name=\"msg\" value=\"$(msg)\" />\n");
		client_print(client, "</go></anchor><br />\n");
	}

	/* write the while messages to other cards */
	tmp = g_list_last(client->channel->last_msgs);
	for (num = 0; tmp != NULL; tmp = tmp->prev, num++) {
		MOBILE_MSG_REC *rec = tmp->data;

		if (strlen(rec->message) <= MAX_MSGLINK_LEN)
			continue;

		client_print(client, client->wml ?
			     "</p></card><card id=\"m%d\"><p>\n" :
			     "<a name=\"#m%d\"></a>\n", num);

		client_print(client, rec->personal ?
			     "<b>%s</b>\n" : "%s\n", rec->message);

		client_print(client, client->wml ?
			     "<do type=\"prev\"><prev /></do>\n" :
			     "<br /><br />\n");
	}
}

static void send_client_page(CLIENT_REC *client)
{
	char *link;

	if (client->wml) {
		client_print(client, "<?xml version=\"1.0\"?>\n\n");
		client_print(client, "<!DOCTYPE wml PUBLIC '-//WAPFORUM//DTD WML 1.1//EN' 'http://www.wapforum.org/DTD/wml_1.1.xml'>\n\n");
	}

	client_print(client, client->wml ?
		     "<wml><card id=\"main\" title=\"irssi\">\n" :
		     "<html><body>\n");
	if (client->wml && client->channel != NULL) {
		/* reload the msgs page every now and then */
		client_print(client, "<onevent type=\"ontimer\">\n");
		link = client_get_link(client, client->channel->channel->server->tag,
				       client->channel->channel->name+1);
		client_print(client, "  <go href=\"%s\" />\n", link);
		g_free(link);
		client_print(client, "</onevent>\n");
		client_print(client, "<timer value=\"%d\" />\n", REFRESH_SECS*10);
	}
	if (client->wml) client_print(client, "<p>\n");
	
	if (client->channel == NULL) {
		/* channel not selected, ask it. */
                send_client_channel_page(client);
	} else {
		/* write lastest messages */
		send_client_msg_page(client);
	}

	client_print(client, client->wml ?
		     "</p></card></wml>\n" : "</body></html>\n");

	cache_counter++;
}

static int handle_client_cmd(CLIENT_REC *client,
			     const char *cmd, const char *args)
{
	char *str;

	if (strcmp(cmd, "lang") == 0) {
                client->wml = strcmp(args, "wml") == 0;
	} else if (strcmp(cmd, "server") == 0) {
                /* select active server */
		client->server = server_find_tag(args);
	} else if (strcmp(cmd, "channel") == 0) {
		/* select active channel */
		CHANNEL_REC *channel;

		if (client->server == NULL)
			return FALSE;

		if (client->server->ischannel(client->server, args))
			channel = channel_find(client->server, args);
		else {
			str = g_strconcat("#", args, NULL);
			channel = channel_find(client->server, str);
			g_free(str);
		}
		client->channel = channel == NULL ? NULL :
                        mobile_channel_find(channel);
	} else if (strcmp(cmd, "msg") == 0) {
		/* send message to active channel */
		MOBILE_MSG_REC *rec;

		if (client->channel == NULL)
			return FALSE;

		str = g_strdup_printf("%s %s %s",
				      client->channel->channel->name, args,
				      settings_get_str("mobile_msgappend"));
		rec = g_new0(MOBILE_MSG_REC, 1);
		rec->message = g_strdup_printf("%s> %s", client->channel->channel->server->nick, args);
		mobile_msg_add(client->channel, rec);

		signal_emit("command msg", 3, str, client->server,
			    client->channel->channel);
		g_free(str);
	} else if (strcmp(cmd, "page") == 0) {
		/* send page to server, close connection */
		send_client_page(client);
		return TRUE;
	} else if (strcmp(cmd, "quit") == 0) {
                /* close connection */
		return TRUE;
	}

	return FALSE;
}

static void sig_mobile_input(CLIENT_REC *client)
{
	char tmpbuf[1024], *str, *cmd, *args;
	int ret, recvlen, disconnect;

	disconnect = FALSE;
	recvlen = net_receive(client->handle, tmpbuf, sizeof(tmpbuf));
	do {
		ret = line_split(tmpbuf, recvlen, &str, &client->buffer);
		recvlen = 0;

		if (ret == -1) {
			/* connection lost */
			disconnect = TRUE;
			break;
		}

		if (ret == 0)
			break;

		if (!client->connected) {
                        /* not connected yet, check password */
			const char *pass = settings_get_str("mobile_password");

			if (strcmp(str, pass) != 0) {
				disconnect = TRUE;
				break;
			}

			client->connected = TRUE;
			continue;
		}

		cmd = g_strdup(str);
		args = strchr(cmd, ' ');
		if (args != NULL) *args++ = '\0'; else args = "";
		g_strdown(cmd);

		disconnect = handle_client_cmd(client, cmd, args);

		g_free(cmd);
	} while (!disconnect);

	if (disconnect)
		client_disconnect(client);
}

static void sig_mobile_listen(void)
{
        GIOChannel *handle;
	IPADDR ip;
	char host[MAX_IP_LEN];
	int port;

	/* accept connection */
	handle = net_accept(listen_handle, &ip, &port);
	if (handle == NULL)
		return;

	if (client != NULL) {
		/* already got one client .. allow only one at a time. */
		net_disconnect(handle);
		return;
	}
	net_ip2host(&ip, host);

	if (strcmp(host, settings_get_str("mobile_webserver")) != 0) {
		/* allow connections only from web server */
		net_disconnect(handle);
		return;
	}

	client = g_new0(CLIENT_REC, 1);
	client->handle = handle;
	client->tag = g_input_add(handle, G_INPUT_READ,
				  (GInputFunction) sig_mobile_input, client);

	/*printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		  "Mobile: Client connecting...");*/
}

static void sig_channel_destroyed(CHANNEL_REC *channel)
{
	MOBILE_CHANNEL_REC *rec;

	rec = mobile_channel_find(channel);
	if (rec != NULL) mobile_channel_destroy(rec);
}

static void sig_message_public(SERVER_REC *server, const char *msg,
			       const char *nick, const char *address,
			       const char *target)
{
	CHANNEL_REC *channel;
	MOBILE_CHANNEL_REC *mchannel;
	MOBILE_MSG_REC *rec;

	channel = channel_find(server, target);
	if (channel == NULL) return;

	mchannel = mobile_channel_find(channel);
	if (mchannel == NULL) return;

	rec = g_new0(MOBILE_MSG_REC, 1);
        rec->personal = nick_match_msg(channel, msg, server->nick);
	rec->message = g_strdup_printf("%s> %s", nick, msg);
	mobile_msg_add(mchannel, rec);
}

void mobile_init(void)
{
	int port;

	srand(time(NULL));
	cache_counter = rand()%0x1000;

	settings_add_int("mobile", "mobile_port", 7070);
	settings_add_str("mobile", "mobile_webserver", "127.0.0.1");
	settings_add_str("mobile", "mobile_password", "");
	settings_add_str("mobile", "mobile_msgappend", "[via irssi-WAP]");

	if (*settings_get_str("mobile_password") == '\0') {
		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			  "Mobile: Password not set, you should set it with "
			  "/SET mobile_password <password>");
	}

	mobile_channels = NULL;
	g_slist_foreach(channels, (GFunc) mobile_channel_create, NULL);

	client = NULL;
	port = settings_get_int("mobile_port");
	listen_handle = net_listen(NULL, &port);

	if (listen_handle == NULL) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR,
			  "Mobile: Listen in port %d failed: %s",
			  port, errno);
		listen_tag = -1;
		return;
	}

        listen_tag = g_input_add(listen_handle, G_INPUT_READ,
				 (GInputFunction) sig_mobile_listen, NULL);

	signal_add("channel created", (SIGNAL_FUNC) mobile_channel_create);
	signal_add("channel destroyed", (SIGNAL_FUNC) sig_channel_destroyed);
	signal_add("message public", (SIGNAL_FUNC) sig_message_public);

        module_register("mobile", "core");
}

void mobile_deinit(void)
{
	if (listen_tag != -1) g_source_remove(listen_tag);
	if (listen_handle != NULL) net_disconnect(listen_handle);

	if (client != NULL)
		client_disconnect(client);

	while (mobile_channels != NULL)
		mobile_channel_destroy(mobile_channels->data);

	signal_remove("channel created", (SIGNAL_FUNC) mobile_channel_create);
	signal_remove("channel destroyed", (SIGNAL_FUNC) sig_channel_destroyed);
	signal_remove("message public", (SIGNAL_FUNC) sig_message_public);
}
