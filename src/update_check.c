/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2015 Hiroyuki Yamamoto
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
#include "plugin.h"

static gchar *check_url = NULL;
static gchar *download_url = NULL;
static gchar *jump_url = NULL;
#ifdef USE_UPDATE_CHECK_PLUGIN
static gchar *check_plugin_url = NULL;
static gchar *jump_plugin_url = NULL;
#endif /* USE_UPDATE_CHECK_PLUGIN */

static gint compare_version(gint lmajor, gint lminor, gint lmicro,
			    gint rmajor, gint rminor, gint rmicro)
{
	debug_print("comparing %d.%d.%d <> %d.%d.%d\n",
		    lmajor, lminor, lmicro,
		    rmajor, rminor, rmicro);

	if (lmajor > rmajor)
		return 1;
	if (lmajor < rmajor)
		return -1;
	if (lminor > rminor)
		return 1;
	if (lminor < rminor)
		return -1;
	if (lmicro > rmicro)
		return 1;
	if (lmicro < rmicro)
		return -1;

	return 0;
}

static gboolean compare_sylpheed_version(gint major, gint minor, gint micro,
				const gchar *extra, gboolean remote_is_release,
				gboolean cur_ver_is_release)
{
	debug_print("comparing %d.%d.%d.%s (%s) <> " VERSION " (%s)\n",
		    major, minor, micro, extra ? extra : "",
		    remote_is_release ? "release" : "devel",
		    cur_ver_is_release ? "release" : "devel");

	switch (compare_version(major, minor, micro,
			MAJOR_VERSION, MINOR_VERSION, MICRO_VERSION)) {
	case 1:
		return TRUE;
	case -1:
		return FALSE;
	default:
		break;
	}

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
				if (vers[3] && extra) {
					*extra = g_strdup(vers[3]);
				}
			}
		}
	}
	g_strfreev(vers);
}

#ifdef G_OS_WIN32
static gboolean spawn_update_manager(void)
{
	gchar *src = NULL, *dest = NULL, *quoted_uri = NULL;
	gchar *cmdline[] = {NULL, "/uri", NULL, NULL};
	GError *error = NULL;
	gboolean ret = FALSE;

	src = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S, "update-manager.exe", NULL);
	if (!is_file_exist(src) || get_file_size(src) <= 0) {
		g_warning("update-manager.exe not found.");
		goto finish;
	}
	dest = g_strconcat(g_get_tmp_dir(), G_DIR_SEPARATOR_S, "sylpheed-update-manager.exe", NULL);
	if (copy_file(src, dest, FALSE) < 0) {
		g_warning("Couldn't copy update-manager.exe");
		goto finish;
	}
	quoted_uri = g_strdup_printf("'%s'", download_url);
	cmdline[0] = dest;
	cmdline[2] = quoted_uri;
	if (g_spawn_async
		(NULL, cmdline, NULL,
		 G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
		 NULL, NULL, NULL, &error) == FALSE) {
		g_warning("Couldn't execute update-manager.exe");
		if (error) {
			g_warning("g_spawn_async: %s", error->message);
			g_error_free(error);
		}
	} else {
		ret = TRUE;
	}
finish:
	g_free(src);
	g_free(dest);
	g_free(quoted_uri);
	return ret;
}

#ifdef USE_UPDATE_CHECK_PLUGIN
static gchar *plugin_updater_ini = NULL;

void update_check_spawn_plugin_updater(void)
{
	gchar *exe = NULL, *quoted_ini = NULL;
	gchar *cmdline[] = {NULL, "/ini", NULL, NULL};
	GError *error = NULL;
	gboolean ret = FALSE;

	if (!plugin_updater_ini)
		return;
	if (!is_file_exist(plugin_updater_ini)) {
		g_warning("Not found %s", plugin_updater_ini);
		goto finish;
	}
	exe = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S, "plugin-updater.exe", NULL);
	if (!is_file_exist(exe) || get_file_size(exe) <= 0) {
		g_warning("Not found plugin-updater.exe");
		goto finish;
	}

	quoted_ini = g_strdup_printf("'%s'", plugin_updater_ini);
	cmdline[0] = exe;
	cmdline[2] = quoted_ini;
	if (g_spawn_sync
		(NULL, cmdline, NULL,
		 G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
		 NULL, NULL, NULL, NULL, NULL, &error) == FALSE) {
		g_warning("Couldn't execute plugin-updater.exe");
		debug_print("Couldn't execute plugin-updater.exe\n");
		if (error) {
			g_warning("g_spawn_async: %s", error->message);
			debug_print("g_spawn_async: %s\n", error->message);
			g_error_free(error);
		}
	} else {
		ret = TRUE;
	}
finish:
	g_free(exe);
	g_free(quoted_ini);
	g_free(plugin_updater_ini);
	plugin_updater_ini = NULL;
}
#endif /* USE_UPDATE_CHECK_PLUGIN */
#endif /* G_OS_WIN32 */

#ifdef G_OS_WIN32
static void set_default_download_url(void)
{
	gchar buf[1024];
	const gchar *os;

#ifdef G_OS_WIN32
	os = "win";
#else
	if (strstr(TARGET_ALIAS, "linux"))
		os = "linux";
	else
		os = "other";
#endif

#ifdef DEVEL_VERSION
	g_snprintf(buf, sizeof(buf), "%s?ver=%s&os=%s&dev=t",
		   DOWNLOAD_URI, VERSION, os);
#else
	g_snprintf(buf, sizeof(buf), "%s?ver=%s&os=%s",
		   DOWNLOAD_URI, VERSION, os);
#endif
	update_check_set_download_url(buf);
}
#endif

static void update_dialog(const gchar *new_ver, const gchar *disp_ver,
			  gboolean manual)
{
	gchar buf[1024];
	AlertValue val;

	if (!jump_url)
		update_check_set_jump_url(HOMEPAGE_URI);

	if (new_ver) {
		if (disp_ver)
			g_snprintf(buf, sizeof(buf), "%s\n\n%s\n(%s -> %s)",
				   _("A newer version of Sylpheed has been found.\n"
				     "Upgrade now?"),
				   disp_ver, VERSION, new_ver);
		else
			g_snprintf(buf, sizeof(buf), "%s\n\n%s -> %s",
				   _("A newer version of Sylpheed has been found.\n"
				     "Upgrade now?"),
				   VERSION, new_ver);
	} else
		g_snprintf(buf, sizeof(buf), "%s",
			   _("A newer version of Sylpheed has been found.\n"
			     "Upgrade now?"));

	val = alertpanel_full(_("New version found"), buf,
			      ALERT_QUESTION,
			      G_ALERTDEFAULT,
			      manual ? FALSE : TRUE,
			      GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	if ((val & G_ALERT_VALUE_MASK) == G_ALERTDEFAULT) {
#ifdef G_OS_WIN32
		if (!download_url)
			set_default_download_url();
		if (!spawn_update_manager())
			open_uri(jump_url, prefs_common.uri_cmd);
#else
		open_uri(jump_url, prefs_common.uri_cmd);
#endif
	}
	if (val & G_ALERTDISABLE) {
		prefs_common.auto_update_check = FALSE;
	}
}

static gint child_stdout;

static gchar *read_child_stdout_and_close(gint fd)
{
	GIOChannel *ch;
	gchar *str = NULL;
	gsize len = 0;

	ch = g_io_channel_unix_new(fd);
	g_io_channel_read_to_end(ch, &str, &len, NULL);
	g_io_channel_shutdown(ch, TRUE, NULL);
	g_io_channel_unref(ch);

	return str;
}

static void close_child_stdout(gint fd)
{
#ifdef G_OS_WIN32
	GIOChannel *ch;

	ch = g_io_channel_win32_new_fd(fd);
	g_io_channel_shutdown(ch, TRUE, NULL);
	g_io_channel_unref(ch);
#else
	fd_close(fd);
#endif
}

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
	gboolean rel_result = FALSE;
	gboolean dev_result = FALSE;
	gboolean show_dialog_always = GPOINTER_TO_INT(data);
	gchar *str;
	gchar *disp_rel_ver = NULL;
	gchar *disp_dev_ver = NULL;

	debug_print("update_check_cb\n");

	if (!child_stdout) {
		g_spawn_close_pid(pid);
		return;
	}

	str = read_child_stdout_and_close(child_stdout);
	child_stdout = 0;
	g_spawn_close_pid(pid);

	if (!str) {
		return;
	}

	lines = g_strsplit(str, "\n", -1);

	for (i = 0; lines[i] != NULL; i++) {
		gint major = 0, minor = 0, micro = 0;
		gchar *extra = NULL;

		debug_print("update_check: %s\n", lines[i]);
		p = strchr(lines[i], '=');
		if (!p) continue;
		key = g_strndup(lines[i], p - lines[i]);
		val = p + 1;

		if (!disp_rel_ver && !strcmp(key, "DISP_RELEASE")) {
			disp_rel_ver = g_strdup(val);
		} else if (!cur_ver_is_release && !disp_dev_ver &&
			   !strcmp(key, "DISP_DEVEL")) {
			disp_dev_ver = g_strdup(val);
		}

		if (!result) {
			if (!strcmp(key, "RELEASE")) {
				parse_version_string(val, &major, &minor, &micro,
						     &extra);
				result = compare_sylpheed_version(major, minor, micro, extra,
								  TRUE, cur_ver_is_release);
				rel_result = result;
			} else if (!cur_ver_is_release && !strcmp(key, "DEVEL")) {
				parse_version_string(val, &major, &minor, &micro,
						     &extra);
				result = compare_sylpheed_version(major, minor, micro, extra,
								  FALSE, cur_ver_is_release);
				dev_result = result;
			}

			if (major + minor + micro != 0)
				got_version = TRUE;

			if (result) {
				new_ver = g_strdup_printf("%d.%d.%d%s", major, minor, micro, extra ? extra : "");
				debug_print("update_check: new ver: %s (%s)\n", new_ver, rel_result ? "release version" : "devel version");
			}
		}

		g_free(extra);
		g_free(key);
	}

	g_strfreev(lines);
	g_free(str);

	gdk_threads_enter();

	if (!gtkut_window_modal_exist() && !inc_is_active()) {
		if (result) {
			if (rel_result)
				update_dialog(new_ver, disp_rel_ver, show_dialog_always);
			else
				update_dialog(new_ver, disp_dev_ver, show_dialog_always);
		} else if (show_dialog_always) {
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

	g_free(disp_rel_ver);
	g_free(disp_dev_ver);
	g_free(new_ver);

	gdk_threads_leave();
}

static void spawn_curl(gchar *url, GChildWatchFunc func, gpointer data)
{
	gchar *cmdline[8] = {"curl", "--silent", "--max-time", "10"};
	gint argc = 4;
	GPid pid;
	GError *error = NULL;

	if (child_stdout > 0) {
		debug_print("update check is in progress\n");
		return;
	}

	child_stdout = 0;

	debug_print("spawn_curl: getting from %s\n", url);

	cmdline[argc++] = url;
	if (prefs_common.use_http_proxy && prefs_common.http_proxy_host &&
	    prefs_common.http_proxy_host[0] != '\0') {
		cmdline[argc++] = "--proxy";
		cmdline[argc++] = prefs_common.http_proxy_host;
	}
	cmdline[argc++] = NULL;

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
			close_child_stdout(child_stdout);
			child_stdout = 0;
		}
		return;
	}

	g_child_watch_add(pid, func, data);
}

#ifdef USE_UPDATE_CHECK_PLUGIN
struct download_plugin_info {
	const gchar *filename;
	const SylPluginInfo* info;
	gchar *url;
	gint major, minor, micro;
};

static void download_plugin_info_free(struct download_plugin_info *pinfo)
{
	if (!pinfo)
		return;
	g_free(pinfo->url);
	g_free(pinfo);
}

static GHashTable *get_plugin_version_table(void)
{
	GSList *list, *cur;
	SylPluginInfo *info;
	GModule *module;
	struct download_plugin_info *pinfo;
	GHashTable *plugin_version_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)download_plugin_info_free);

	list = syl_plugin_get_module_list();
	for (cur = list; cur != NULL; cur = cur->next) {
		module = (GModule *)cur->data;

		info = syl_plugin_get_info(module);
		if (info) {
			pinfo = g_new0(struct download_plugin_info, 1);
			pinfo->filename = g_module_name(module);
			pinfo->info = info;
			g_hash_table_insert(plugin_version_table, info->name, pinfo);
		} else {
			debug_print("info not found: %s\n", g_module_name(module));
		}
	}

	return plugin_version_table;
}

#ifdef G_OS_WIN32
static gboolean write_plugin_updater_ini(GSList *list)
{
	guint num, h = 12;
	struct download_plugin_info *pinfo;
	GString *ini = g_string_new("[Settings]\n");
	GSList *cur;
	gchar *basename, *p;
	gboolean ret = TRUE;

	num = g_slist_length(list);
	g_string_append_printf(ini, "NumFields=%d\n", num+1);

	num = 0;
	g_string_append_printf(ini, "\n[Field %d]\n", ++num);
	g_string_append_printf(ini, "Type=GroupBox\n");
	g_string_append_printf(ini, "Left=0\n");
	g_string_append_printf(ini, "Right=-1\n");
	g_string_append_printf(ini, "Top=0\n");
	g_string_append_printf(ini, "Bottom=-5\n");
	g_string_append_printf(ini, "Text=\" Select update plugins \"\n");

	for (cur = list; cur != NULL; cur = cur->next) {
		pinfo = cur->data;
		g_string_append_printf(ini, "\n[Field %d]\n", ++num);
		g_string_append_printf(ini, "Type=checkbox\n");
		g_string_append_printf(ini, "Text=%s  %s -> %d.%d.%d\n",
				       pinfo->info->name, pinfo->info->version,
				       pinfo->major, pinfo->minor, pinfo->micro);
		g_string_append_printf(ini, "Left=10\n");
		g_string_append_printf(ini, "Right=-10\n");
		g_string_append_printf(ini, "Top=%u\n", (h+=5));
		g_string_append_printf(ini, "Bottom=%u\n", (h+=8));
		g_string_append_printf(ini, "State=1\n");
		g_string_append_printf(ini, "URL=%s\n", pinfo->url);
		g_string_append_printf(ini, "name=%s\n", pinfo->info->name);

		basename = g_path_get_basename(pinfo->filename);
		p = strrchr(basename, '.');
		if (p) *p = '\0'; /* cut ".dll" */
		g_string_append_printf(ini, "basename=%s\n", basename);
		g_free(basename);
	}

	debug_print("write_plugin_updater_ini:\n%s\n", ini->str);

	plugin_updater_ini = g_strconcat(g_get_tmp_dir(), G_DIR_SEPARATOR_S, "sylpheed-plugin-updater.ini", NULL);
	if (str_write_to_file(ini->str, plugin_updater_ini) < 0) {
		g_free(plugin_updater_ini);
		plugin_updater_ini = NULL;
		ret = FALSE;
	}

	g_string_free(ini, TRUE);
	return ret;
}
#endif /* G_OS_WIN32 */

static void update_plugin_dialog(GString *text, GSList *list)
{
	AlertValue val;

	if (!jump_plugin_url)
		update_check_set_jump_plugin_url(PLUGIN_HOMEPAGE_URI);

	val = alertpanel_full(_("New version found"), text->str,
			      ALERT_QUESTION,
			      G_ALERTDEFAULT,
			      FALSE,
			      GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	if ((val & G_ALERT_VALUE_MASK) == G_ALERTDEFAULT) {
#ifdef G_OS_WIN32
		if (write_plugin_updater_ini(list))
			app_will_restart(TRUE);
#else
		open_uri(jump_plugin_url, prefs_common.uri_cmd);
#endif
	}
}

static void update_check_plugin_cb(GPid pid, gint status, gpointer data)
{
	gchar **lines;
	gchar *key, *val, *p;
	gchar *cur_ver;
	gint i;
	gboolean show_dialog_always = GPOINTER_TO_INT(data);
	gchar *str;
	GHashTable *plugin_version_table = NULL;
	struct download_plugin_info *pinfo = NULL;
	gboolean result = FALSE;
	gboolean got_version = FALSE;
	GString *text = NULL;
	GSList *list = NULL;

	debug_print("update_check_plugin_cb\n");

	if (!child_stdout) {
		g_spawn_close_pid(pid);
		return;
	}

	str = read_child_stdout_and_close(child_stdout);
	child_stdout = 0;
	g_spawn_close_pid(pid);

	if (!str) {
		return;
	}

	lines = g_strsplit(str, "\n", -1);
	plugin_version_table = get_plugin_version_table();
	text = g_string_new(_("Newer version of plug-ins have been found.\n"
			      "Upgrade now?\n"));

	for (i = 0; lines[i] != NULL; i++) {
		gint new_major = 0, new_minor = 0, new_micro = 0;
		gint cur_major = 0, cur_minor = 0, cur_micro = 0;
		debug_print("update_check_plugin: %s\n", lines[i]);
		p = strchr(lines[i], '=');
		if (!p) continue;
		key = g_strndup(lines[i], p - lines[i]);
		val = p + 1;

		parse_version_string(val, &new_major, &new_minor, &new_micro, NULL);
		if (new_major + new_minor + new_micro != 0) {
			got_version = TRUE;
		}

		pinfo = g_hash_table_lookup(plugin_version_table, key);
		if (pinfo && (cur_ver = pinfo->info->version)) {
			parse_version_string(cur_ver, &cur_major, &cur_minor, &cur_micro, NULL);
			if (0 < compare_version(new_major, new_minor, new_micro,
						cur_major, cur_minor, cur_micro)) {
				g_string_append_printf(text, "\n - %s  %d.%d.%d -> %d.%d.%d", key,
						       cur_major, cur_minor, cur_micro,
						       new_major, new_minor, new_micro);

				result = TRUE;

				debug_print("val = %s\n", val);
				p = strchr(val, ',');
				if (p) {
					struct download_plugin_info *pinfo2 = g_new0(struct download_plugin_info, 1);
					pinfo2->filename = pinfo->filename;
					pinfo2->info = pinfo->info;
					pinfo2->url = g_strdup(p + 1); /* skip ',' */
					pinfo2->major = new_major;
					pinfo2->minor = new_minor;
					pinfo2->micro = new_micro;
					list = g_slist_append(list, pinfo2);
				}
			}
		}

		g_free(key);
	}

	g_hash_table_destroy(plugin_version_table);
	g_strfreev(lines);
	g_free(str);

	debug_print("%s\n", text->str);

	gdk_threads_enter();

	if (!gtkut_window_modal_exist() && !inc_is_active()) {
		if (result) {
			update_plugin_dialog(text, list);
			list = NULL;
		} else if (show_dialog_always) {
			if (got_version)
				alertpanel_message(_("Information"),
						   _("All Sylpheed plug-ins are already the latest version."),
						   ALERT_NOTICE);
			else
				alertpanel_error(_("Couldn't get the version information of plug-ins."));
		}
	} else {
		debug_print("update_check_plugin_cb: modal dialog exists or incorporation is active. Disabling update dialog.\n");
	}

	g_string_free(text, TRUE);
	g_slist_foreach(list, (GFunc)download_plugin_info_free, NULL);
	g_slist_free(list);

	gdk_threads_leave();
}

void update_check_plugin(gboolean show_dialog_always)
{
	gchar buf[1024];

	if (!check_plugin_url) {
#ifdef G_OS_WIN32
		g_snprintf(buf, sizeof(buf), "%s?ver=%s&os=win", PLUGIN_VERSION_URI, VERSION);
#else
		if (strstr(TARGET_ALIAS, "linux"))
			g_snprintf(buf, sizeof(buf), "%s?ver=%s&os=linux", PLUGIN_VERSION_URI, VERSION);
		else
			g_snprintf(buf, sizeof(buf), "%s?ver=%s&os=other", PLUGIN_VERSION_URI, VERSION);
#endif
		update_check_set_check_plugin_url(buf);
	}
	spawn_curl(check_plugin_url, update_check_plugin_cb, GINT_TO_POINTER(show_dialog_always));
}
#endif /* USE_UPDATE_CHECK_PLUGIN */

void update_check(gboolean show_dialog_always)
{
	gchar buf[1024];

	if (!check_url) {
#ifdef G_OS_WIN32
		g_snprintf(buf, sizeof(buf), "%s?ver=%s&os=win", VERSION_URI, VERSION);
#else
		if (strstr(TARGET_ALIAS, "linux"))
			g_snprintf(buf, sizeof(buf), "%s?ver=%s&os=linux", VERSION_URI, VERSION);
		else
			g_snprintf(buf, sizeof(buf), "%s?ver=%s&os=other", VERSION_URI, VERSION);
#endif
		update_check_set_check_url(buf);
	}
	spawn_curl(check_url, update_check_cb, GINT_TO_POINTER(show_dialog_always));
}

void update_check_set_check_url(const gchar *url)
{
	if (check_url)
		g_free(check_url);
	check_url = g_strdup(url);

	if (url)
		debug_print("update_check_set_check_url: check URL was set to: %s\n", url);
	else
		debug_print("update_check_set_check_url: check URL was unset.\n");
}

const gchar *update_check_get_check_url(void)
{
	return check_url;
}

void update_check_set_download_url(const gchar *url)
{
	if (download_url)
		g_free(download_url);
	download_url = g_strdup(url);

	if (url)
		debug_print("update_check_set_download_url: download URL was set to: %s\n", url);
	else
		debug_print("update_check_set_download_url: download URL was unset.\n");
}

const gchar *update_check_get_download_url(void)
{
	return download_url;
}

void update_check_set_jump_url(const gchar *url)
{
	if (jump_url)
		g_free(jump_url);
	jump_url = g_strdup(url);
}

const gchar *update_check_get_jump_url(void)
{
	return jump_url;
}

#ifdef USE_UPDATE_CHECK_PLUGIN
void update_check_set_check_plugin_url(const gchar *url)
{
	if (check_plugin_url)
		g_free(check_plugin_url);
	check_plugin_url = g_strdup(url);
}

const gchar *update_check_get_check_plugin_url(void)
{
	return check_plugin_url;
}

void update_check_set_jump_plugin_url(const gchar *url)
{
	if (jump_plugin_url)
		g_free(jump_plugin_url);
	jump_plugin_url = g_strdup(url);
}

const gchar *update_check_get_jump_plugin_url(void)
{
	return jump_plugin_url;
}
#endif /* USE_UPDATE_CHECK_PLUGIN */

#endif /* USE_UPDATE_CHECK */
