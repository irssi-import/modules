/*
 theme-indent.c : Overrides irssi's default indenting with getting
 		  indentation strings from theme.

 compile:
   export IRSSI=~/src/irssi-0.7.99
   gcc theme-indent.c -Wall -g -o ~/.irssi/modules/libtheme_indent.so -shared -I$IRSSI/src -I$IRSSI/src/fe-common/core -I$IRSSI/src/core -I$IRSSI/src/fe-text `glib-config --cflags`

 usage:
   /LOAD theme_indent

   In theme file:
    indent_default = "default indentation text";
    indent_<level> = "level-specific indentation"; (eg. level_hilight, ...)
    + indent_own_public, indent_own_msg

   So, ircii-like "+" indenting would come if you just added into your
   theme file, inside the abstracts = { .. } block:

    indent_default = "+";


    Copyright (C) 2001 Timo Sirainen

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

#define MODULE_NAME "fe-text/theme-indent"

#include "common.h"
#include "modules.h"
#include "signals.h"
#include "levels.h"

#include "themes.h"
#include "gui-printtext.h"

static char *default_indent_text, *own_public_text, *own_msg_text;
static char *level_texts[32];

static int indent(TEXT_BUFFER_VIEW_REC *view, LINE_REC *line, int ypos)
{
        char *text;
	int level, i, count;

	level = line->info.level;

	text = NULL;
	if (level & MSGLEVEL_NOHILIGHT) {
		if (level & MSGLEVEL_MSGS) {
			text = own_msg_text;
		} else if (level & MSGLEVEL_PUBLIC) {
			text = own_public_text;
		}
	}

	if (text == NULL) {
	        text = default_indent_text;
	        for (count = 0, i = (MSGLEVEL_ALL+1)/2; i >= 1; i /= 2, count++) {
			if (level & i) {
	                        if (level_texts[count] != NULL)
					text = level_texts[count];
	                        break;
			}
		}
	}

	if (ypos != -1 && text != NULL)
		term_addstr(view->window, text);

	return text == NULL ? 0 : strlen(text);
}

static void get_theme_text(const char *format, char **dest)
{
	g_free_and_null(*dest);
	*dest = theme_format_expand(current_theme, format);
	if (**dest == '\0')
		g_free_and_null(*dest);
}

static void read_settings(void)
{
        char *name, *format;
	int i, count;

	if (current_theme == NULL)
                return;

        for (i = 0; i < 32; i++)
		g_free_not_null(level_texts[i]);


	get_theme_text("{indent_default}", &default_indent_text);
	get_theme_text("{indent_own_public}", &own_public_text);
	get_theme_text("{indent_own_msg}", &own_msg_text);

        for (count = 0, i = (MSGLEVEL_ALL+1)/2; i >= 1; i /= 2, count++) {
		name = bits2level(i);
                g_strdown(name);

		format = g_strdup_printf("{indent_%s}", name);
		get_theme_text(format, &level_texts[count]);
		g_free(format);

		g_free(name);
	}
}

void theme_indent_init(void)
{
        default_indent_text = NULL;
        own_public_text = NULL;
        own_msg_text = NULL;
        memset(level_texts, 0, sizeof(level_texts));

	signal_add("setup changed", (SIGNAL_FUNC) read_settings);
	read_settings();

	gui_register_indent_func("theme", indent);
	gui_set_default_indent("theme");

	module_register("theme_indent", "core");
}

void theme_indent_deinit(void)
{
	int i;

	gui_unregister_indent_func("theme", indent);

        for (i = 0; i < 32; i++)
		g_free_not_null(level_texts[i]);
        g_free_not_null(default_indent_text);
        g_free_not_null(own_public_text);
        g_free_not_null(own_msg_text);

        signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
}
