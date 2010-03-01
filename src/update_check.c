/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_UPDATE_CHECK

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <stdlib.h>
#include <unistd.h>

#include "update_check.h"
#include "manage_window.h"
#include "inc.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "prefs_common.h"
#include "socket.h"
#include "utils.h"
#include "version.h"


static gboolean compare_version(gint major, gint minor, gint micro,
				const gchar *extra, gboolean remote_is_release,
				gboolean cur_ver_is_release)
{
	debug_print("comparing %d.%d.%d.%s (%s) <> " VERSION " (%s)\n",
		    major, minor, micro, extra ? extra : "",
		    remote_is_release ? "release" : "devel",
		    cur_ver_is_release ? "release" : "devel");

	if (major > MAJOR_VERSION)
		return TRUE;
	if (major < MAJOR_VERSION)
		return FALSE;
	if (minor > MINOR_VERSION)
		return TRUE;
	if (minor < MINOR_VERSION)
		return FALSE;
	if (micro > MICRO_VERSION)
		return TRUE;
	if (micro < MICRO_VERSION)
		return FALSE;

	/* compare extra version
	   3.0.0.a  (rel) > 3.0.0       (rel)
	   3.0.0    (rel) > 3.0.0.betaX (dev)
	   3.0.0.a  (rel) > 3.0.0.betaX (dev)
	   3.0.0.rc (dev) > 3.0.0.betaX (dev) */
	if (remote_is_release && !cur_ver_is_release)
		return TRUE;
	if (!remote_is_release && cur_ver_is_release)
		return FALSE;

	if (extra) {
		if (strcmp(extra, EXTRA_VERSION) > 0)
			return TRUE;
	}

	return FALSE;
}

static void parse_version_string(const gchar *ver, gint *major, gint *minor,
				 gint *micro, gchar **extra)
{
	gchar **vers;

	vers = g_strsplit(ver, ".", -1);
	if (vers[0]) {
		*major = atoi(vers[0]);
		if (vers[1]) {
			*minor = atoi(vers[1]);
			if (vers[2]) {
				*micro = atoi(vers[2]);
				if (vers[3]) {
					*extra = g_strdup(vers[3]);
				}
			}
		}
	}
	g_strfreev(vers);
}

static void update_dialog(const gchar *new_ver, gboolean manual)
{
	gchar buf[1024];
	AlertValue val;

	if (new_ver)
		g_snprintf(buf, sizeof(buf), "%s\n\n%s -> %s",
			   _("A newer version of Sylpheed has been found.\n"
			     "Upgrade now?"),
			   VERSION, new_ver);
	else
		g_snprintf(buf, sizeof(buf), "%s",
			   _("A newer version of Sylpheed has been found.\n"
			     "Upgrade now?"));

	val = alertpanel_full(_("New version found"), buf,
			      ALERT_QUESTION,
			      G_ALERTDEFAULT,
			      manual ? FALSE : TRUE,
			      GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	if ((val & G_ALERT_VALUE_MASK) == G_ALERTDEFAULT) {
		open_uri(HOMEPAGE_URI, prefs_common.uri_cmd);
	}
	if (val & G_ALERTDISABLE) {
		prefs_common.auto_update_check = FALSE;
	}
}

static gint child_stdout;

static void update_check_cb(GPid pid, gint status, gpointer data)
{
	gchar **lines;
	gchar *key, *val, *p;
	gchar *new_ver = NULL;
	gint i;
#ifdef DEVEL_VERSION
	gboolean cur_ver_is_release = FALSE;
#else
	gboolean cur_ver_is_release = TRUE;
#endif
	gboolean result = FALSE;
	gboolean got_version = FALSE;
	gboolean show_dialog_always = (gboolean)data;
	gchar buf[BUFFSIZE];
	ssize_t size;

	debug_print("update_check_cb\n");

	if (!child_stdout) {
		g_spawn_close_pid(pid);
		return;
	}

	size = read(child_stdout, buf, sizeof(buf) - 1);
	if (size < 0) {
		fd_close(child_stdout);
		child_stdout = 0;
		g_spawn_close_pid(pid);
		return;
	}
	buf[size] = '\0';

	fd_close(child_stdout);
	child_stdout = 0;
	g_spawn_close_pid(pid);

	lines = g_strsplit(buf, "\n", -1);

	for (i = 0; lines[i] != NULL; i++) {
		gint major = 0, minor = 0, micro = 0;
		gchar *extra = NULL;

		debug_print("update_check: %s\n", lines[i]);
		p = strchr(lines[i], '=');
		if (!p) continue;
		key = g_strndup(lines[i], p - lines[i]);
		val = p + 1;

		if (!strcmp(key, "RELEASE")) {
			parse_version_string(val, &major, &minor, &micro,
					     &extra);
			result = compare_version(major, minor, micro, extra,
						 TRUE, cur_ver_is_release);
		} else if (!cur_ver_is_release && !strcmp(key, "DEVEL")) {
			parse_version_string(val, &major, &minor, &micro,
					     &extra);
			result = compare_version(major, minor, micro, extra,
						 FALSE, cur_ver_is_release);
		}

		if (major + minor + micro != 0)
			got_version = TRUE;

		if (result) {
			new_ver = g_strdup_printf("%d.%d.%d%s", major, minor, micro, extra ? extra : "");
			debug_print("update_check: new ver: %s\n", new_ver);
			g_free(extra);
			g_free(key);
			break;
		}
		g_free(extra);
		g_free(key);
	}

	g_strfreev(lines);

	gdk_threads_enter();

	if (!gtkut_window_modal_exist() && !inc_is_active()) {
		if (result)
			update_dialog(new_ver, show_dialog_always);
		else if (show_dialog_always) {
			if (got_version)
				alertpanel_message(_("Information"),
						   _("Sylpheed is already the latest version."),
						   ALERT_NOTICE);
			else
				alertpanel_error(_("Couldn't get the version information."));
		}
	} else {
		debug_print("update_check_cb: modal dialog exists or incorporation is active. Disabling update dialog.\n");
	}
	g_free(new_ver);

	gdk_threads_leave();
}

void update_check(gboolean show_dialog_always)
{
	gchar *cmdline[8] = {"curl", "--silent", "--max-time", "10"};
	GPid pid;
	GError *error = NULL;

	if (child_stdout > 0) {
		debug_print("update check is in progress\n");
		return;
	}

	child_stdout = 0;

	debug_print("update_check: getting latest version from http://sylpheed.sraoss.jp/version.txt\n");

	cmdline[4] = "http://sylpheed.sraoss.jp/version.txt?";
	if (prefs_common.use_http_proxy && prefs_common.http_proxy_host &&
	    prefs_common.http_proxy_host[0] != '\0') {
		cmdline[5] = "--proxy";
		cmdline[6] = prefs_common.http_proxy_host;
		cmdline[7] = NULL;
	} else
		cmdline[5] = NULL;

	if (g_spawn_async_with_pipes
		(NULL, cmdline, NULL,
		 G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
		 NULL, NULL, &pid,
		 NULL, &child_stdout, NULL, &error) == FALSE) {
		g_warning("Couldn't execute curl");
		if (error) {
			g_warning("g_spawn_async_with_pipes: %s",
				  error->message);
			g_error_free(error);
		}
		return;
	}
	if (pid == 0) {
		g_warning("Couldn't get PID of child process");
		if (child_stdout) {
			fd_close(child_stdout);
			child_stdout = 0;
		}
		return;
	}

	g_child_watch_add(pid, update_check_cb, (gpointer)show_dialog_always);
}

#endif /* USE_UPDATE_CHECK */
