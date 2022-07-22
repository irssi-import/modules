/*
 dcc-send-limiter.c : Limit the transmit speed of DCC sends

 For irssi 0.8+

 compile:
   gcc dcc-send-limiter.c -fPIC -Wall -shared -g -O -o ~/.irssi/modules/libdcc_send_limiter.so `pkg-config --cflags irssi-1`

 compile (old irssis):
   export IRSSI=~/cvs/irssi
   gcc dcc-send-limiter.c -o ~/.irssi/modules/libdcc_send_limiter.so -g -shared -fPIC -I$IRSSI -I$IRSSI/src -I$IRSSI/src/core -I$IRSSI/src/irc/core -I$IRSSI/src/irc/dcc `pkg-config --cflags glib-2.0` -O

 usage:
   /LOAD dcc_send_limiter

    Copyright (C) 2001 Timo Sirainen

    Modified 2002/12/31 by Piotr Krukowiecki (Happy New Year! ;))
    	* fixed unnecesary lag in sending data when send is resume
    	* sends that were started before the module was loaded 
    	  now are being limited as well

    Modified 2001/07/04 by Martin Persson
    	* updated to only keep track of the last 30 sec

    Modified 2001/07/01 by Martin Persson
    	* added speed send checks
    	* fixed crash when destroying dcc sends 
    	  that didn't contain any module data
    	* fixed crash when initiating dcc send


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

#define MODULE_NAME "irc/dcc/limiter"
#define HAVE_CONFIG_H

#include <irssi/src/common.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/core/network.h>
#include <irssi/src/core/settings.h>

#include <irssi/src/irc/core/irc.h>
#include <irssi/src/irc/dcc/dcc-send.h>

#if !defined(IRSSI_ABI_VERSION) || IRSSI_ABI_VERSION < 32
#define I_INPUT_WRITE G_INPUT_WRITE
#define i_input_add g_input_add
#endif

typedef struct {
	int timeout_tag;

	unsigned long skip_bytes;
	unsigned long starttime;
	unsigned long max_speed;
} MODULE_SEND_DCC_REC;

static void dcc_send_data(SEND_DCC_REC *dcc);

static void reset_dcc_send(SEND_DCC_REC *dcc)
{
	MODULE_SEND_DCC_REC *mdcc;

	if (g_slist_find(dcc_conns, dcc) == NULL) {
		/* the DCC was closed during the wait */
		return;
	}

	mdcc = MODULE_DATA(dcc);
	g_source_remove(mdcc->timeout_tag);
	mdcc->timeout_tag = -1;

	dcc->tagwrite = i_input_add(dcc->handle, I_INPUT_WRITE,
				    (GInputFunction) dcc_send_data, dcc);
}

static int sent_too_much(SEND_DCC_REC *dcc, MODULE_SEND_DCC_REC *mdcc)
{
	GTimeVal gtv;
	unsigned long timediff, curtime;
	unsigned long transfd, speed;

	/* 0 == unlimited speed */
	if (mdcc->max_speed == 0) return 0;

	/* get time difference in milliseconds */
	g_get_current_time(&gtv);
	curtime = (gtv.tv_sec * 1000) + (gtv.tv_usec / 1000);

	transfd = (dcc->transfd - mdcc->skip_bytes);
	timediff = curtime - mdcc->starttime + 1;
	speed = ((transfd * 1000) / timediff);

	/* reset speed counter every 30 seconds */
	if (timediff >= 30000) {
		mdcc->starttime = curtime;
		mdcc->skip_bytes = dcc->transfd;
	}

	return (speed > (mdcc->max_speed * 1024));
}

/* input function: DCC SEND - we're ready to send more data */
static void dcc_send_data(SEND_DCC_REC *dcc)
{
	MODULE_SEND_DCC_REC *mdcc;
	char buffer[512];
	int ret, max_speed;
	GTimeVal gtv;

	mdcc = MODULE_DATA(dcc);

	max_speed = settings_get_int("dcc_send_top_speed");
	if (max_speed != mdcc->max_speed) {
		/* speed setting has changed, calculate speed from current position 
		   instead of from the start to eliminate speed boosts/slowdowns */

		mdcc->max_speed = max_speed;
		mdcc->skip_bytes = dcc->transfd;

		g_get_current_time(&gtv);
		mdcc->starttime = (gtv.tv_sec * 1000) + (gtv.tv_usec / 1000);
	}

	if (sent_too_much(dcc, mdcc)) {
		/* disable calling this function for 1/10th of a second. */
		g_source_remove(dcc->tagwrite);
		dcc->tagwrite = -1;
		mdcc->timeout_tag =
			g_timeout_add(100, (GSourceFunc) reset_dcc_send, dcc);
		return;
	}

	ret = read(dcc->fhandle, buffer, sizeof(buffer));
	if (ret <= 0) {
		/* no need to call this function anymore..
		   in fact it just eats all the cpu.. */
		dcc->waitforend = TRUE;
		g_source_remove(dcc->tagwrite);
		dcc->tagwrite = -1;
		return;
	}

	ret = net_transmit(dcc->handle, buffer, ret);
	if (ret > 0) dcc->transfd += ret;
	dcc->gotalldata = FALSE;

	lseek(dcc->fhandle, dcc->transfd, SEEK_SET);

	signal_emit("dcc transfer update", 1, dcc);
}

static void sig_dcc_connected(SEND_DCC_REC *dcc)
{
	MODULE_SEND_DCC_REC *mdcc;
	GTimeVal gtv;

	if (!IS_DCC_SEND(dcc))
		return;

	mdcc = g_new0(MODULE_SEND_DCC_REC, 1);
	MODULE_DATA_SET(dcc, mdcc);
	mdcc->timeout_tag = -1;
	mdcc->skip_bytes = dcc->transfd; /* now it works correct with dcc resume - doesn't wait 30s with sending data */
	mdcc->max_speed = settings_get_int("dcc_send_top_speed");

	/* get starttime in milliseconds */
	g_get_current_time(&gtv);
	mdcc->starttime = (gtv.tv_sec * 1000) + (gtv.tv_usec / 1000);

	g_source_remove(dcc->tagwrite);
	dcc->tagwrite = i_input_add(dcc->handle, I_INPUT_WRITE,
					(GInputFunction) dcc_send_data, dcc);
}

static void sig_dcc_destroyed(SEND_DCC_REC *dcc)
{
	MODULE_SEND_DCC_REC *mdcc;

	if (!IS_DCC_SEND(dcc))
		return;

	mdcc = MODULE_DATA(dcc);
	if (mdcc != NULL) {
		if (mdcc->timeout_tag != -1)
			g_source_remove(mdcc->timeout_tag);

		g_free(mdcc);
	}
}

void dcc_send_limiter_init(void)
{
	GSList *tmp;
	settings_add_int("dcc", "dcc_send_top_speed", 30);

	signal_add_last("dcc connected", (SIGNAL_FUNC) sig_dcc_connected);
	signal_add_first("dcc destroyed", (SIGNAL_FUNC) sig_dcc_destroyed);

	/* Limit already existing sends */
	for (tmp = dcc_conns; tmp != NULL; tmp = tmp->next) {
		SEND_DCC_REC *dcc = tmp->data;
		if (!IS_DCC_SEND(dcc)) continue;
		if (!dcc_is_connected(dcc)) continue;

		sig_dcc_connected(dcc);
	}

        module_register("dcc_send_limiter", "core");
}

void dcc_send_limiter_deinit(void)
{
	signal_remove("dcc connected", (SIGNAL_FUNC) sig_dcc_connected);
	signal_remove("dcc destroyed", (SIGNAL_FUNC) sig_dcc_destroyed);
}

#ifdef MODULE_ABICHECK
MODULE_ABICHECK(dcc_send_limiter)
#else

#ifdef IRSSI_ABI_VERSION
void dcc_send_limiter_abicheck(int *version)
{
    *version = IRSSI_ABI_VERSION;
}
#endif

#endif
