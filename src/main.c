/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2016 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkrc.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkaccelmap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef G_OS_UNIX
#  include <signal.h>
#endif

#if HAVE_LOCALE_H
#  include <locale.h>
#endif

#if USE_GPGME
#  include <gpgme.h>
#endif

#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_actions.h"
#include "prefs_display_header.h"
#include "account.h"
#include "account_dialog.h"
#include "procmsg.h"
#include "procheader.h"
#include "filter.h"
#include "send_message.h"
#include "inc.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "statusbar.h"
#include "addressbook.h"
#include "addrindex.h"
#include "compose.h"
#include "logwindow.h"
#include "folder.h"
#include "setup.h"
#include "sylmain.h"
#include "utils.h"
#include "gtkutils.h"
#include "socket.h"
#include "stock_pixmap.h"
#include "trayicon.h"
#include "notificationwindow.h"
#include "plugin.h"
#include "plugin_manager.h"
#include "foldersel.h"
#include "update_check.h"
#include "colorlabel.h"

#if USE_GPGME
#  include "rfc2015.h"
#endif
#if USE_SSL
#  include "ssl.h"
#  include "sslmanager.h"
#endif

#ifdef G_OS_WIN32
#  include <windows.h>
#  include <pbt.h>
#  include <fcntl.h>
#  include <conio.h>
#endif

#include "version.h"

gchar *prog_version;

#ifdef G_OS_WIN32
static gboolean init_console_done = FALSE;
#endif

static gint lock_socket = -1;
static gint lock_socket_tag = 0;
static GIOChannel *lock_ch = NULL;
static gchar *instance_id = NULL;

#if USE_THREADS
static GThread *main_thread;
#endif

static struct RemoteCmd {
	gboolean receive;
	gboolean receive_all;
	gboolean compose;
	const gchar *compose_mailto;
	GPtrArray *attach_files;
	gboolean send;
	gboolean status;
	gboolean status_full;
	GPtrArray *status_folders;
	GPtrArray *status_full_folders;
	gchar *open_msg;
	gboolean configdir;
	gboolean safe_mode;
	gboolean exit;
	gboolean restart;
	gchar *argv0;
#ifdef G_OS_WIN32
	gushort ipcport;
#endif
} cmd;

#define STATUSBAR_PUSH(mainwin, str) \
{ \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar), \
			   mainwin->mainwin_cid, str); \
	gtkut_widget_draw_now(mainwin->statusbar); \
}

#define STATUSBAR_POP(mainwin) \
{ \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar), \
			  mainwin->mainwin_cid); \
}

#if defined(G_OS_WIN32) || defined(__APPLE__)
static void fix_font_setting		(void);
#endif

static void parse_cmd_opt		(int		 argc,
					 char		*argv[]);

static void app_init			(void);
static void parse_gtkrc_files		(void);
static void setup_rc_dir		(void);
static void check_gpg			(void);
static void set_log_handlers		(gboolean	 enable);
static void register_system_events	(void);
static void plugin_init			(void);

static gchar *get_socket_name		(void);
static gint prohibit_duplicate_launch	(void);
static gint lock_socket_remove		(void);
static gboolean lock_socket_input_cb	(GIOChannel	*source,
					 GIOCondition	 condition,
					 gpointer	 data);

static void remote_command_exec		(void);

#if !defined(G_OS_WIN32) && !defined(__APPLE__)
static void migrate_old_config		(void);
#endif

static void open_compose_new		(const gchar	*address,
					 GPtrArray	*attach_files);
static void open_message		(const gchar	*path);

static void send_queue			(void);

#define MAKE_DIR_IF_NOT_EXIST(dir)					\
{									\
	if (!is_dir_exist(dir)) {					\
		if (is_file_exist(dir)) {				\
			alertpanel_warning				\
				(_("File `%s' already exists.\n"	\
				   "Can't create folder."),		\
				 dir);					\
			exit(1);					\
		}							\
		if (make_dir(dir) < 0)					\
			exit(1);					\
	}								\
}

#define CHDIR_EXIT_IF_FAIL(dir, val)	\
{					\
	if (change_dir(dir) < 0)	\
		exit(val);		\
}

static void load_cb(GObject *obj, GModule *module, gpointer data)
{
	debug_print("load_cb: %p (%s), %p\n", module, module ? g_module_name(module) : "(null)", data);
}

int main(int argc, char *argv[])
{
	MainWindow *mainwin;
	FolderView *folderview;
	GdkPixbuf *icon;
#ifdef G_OS_WIN32
	GList *iconlist = NULL;
#endif
	GObject *syl_app;
	PrefsAccount *new_account = NULL;
	gboolean first_run = FALSE;
	gchar *path;

	app_init();
	parse_cmd_opt(argc, argv);

	/* check and create (unix domain) socket for remote operation */
	lock_socket = prohibit_duplicate_launch();
	if (lock_socket < 0) return 0;

	if (cmd.status || cmd.status_full) {
		puts("0 Sylpheed not running.");
		lock_socket_remove();
		return 0;
	}

#if USE_THREADS
	gdk_threads_enter();
#endif
	gtk_set_locale();
	gtk_init(&argc, &argv);

	syl_app = syl_app_create();

	gdk_rgb_init();
	gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());

	parse_gtkrc_files();
	setup_rc_dir();

	if (is_file_exist("sylpheed.log")) {
		if (rename_force("sylpheed.log", "sylpheed.log.bak") < 0)
			FILE_OP_ERROR("sylpheed.log", "rename");
	}
	set_log_file("sylpheed.log");

	set_ui_update_func(gtkut_events_flush);
	set_progress_func(main_window_progress_show);
	set_input_query_password_func(input_dialog_query_password);
#if USE_SSL
	ssl_init();
	ssl_set_verify_func(ssl_manager_verify_cert);
#endif

	CHDIR_EXIT_IF_FAIL(get_home_dir(), 1);

	prefs_common_read_config();
	filter_set_addressbook_func(addressbook_has_address);
	filter_read_config();
	prefs_actions_read_config();
	prefs_display_header_read_config();
	colorlabel_read_config();

	prefs_common.user_agent_str = g_strdup_printf
		("%s (GTK+ %d.%d.%d; %s)",
		 prog_version,
		 gtk_major_version, gtk_minor_version, gtk_micro_version,
		 TARGET_ALIAS);

#if defined(G_OS_WIN32) || defined(__APPLE__)
	fix_font_setting();
#endif

	gtkut_stock_button_set_set_reverse(!prefs_common.comply_gnome_hig);

	check_gpg();

	sock_set_io_timeout(prefs_common.io_timeout_secs);

	gtkut_widget_init();

	gtkut_get_dpi();

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "icons", NULL);
	if (is_dir_exist(path)) {
		debug_print("icon theme dir: %s\n", path);
		stock_pixbuf_set_theme_dir(path);
	} else {
		g_free(path);
		if (g_path_is_absolute(THEMEDIR))
			path = g_strconcat(THEMEDIR, NULL);
		else
			path = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S, THEMEDIR, NULL);
		if (is_dir_exist(path)) {
			debug_print("icon theme dir: %s\n", path);
			stock_pixbuf_set_theme_dir(path);
		}
	}
	g_free(path);

#ifdef G_OS_WIN32
	stock_pixbuf_gdk(NULL, STOCK_PIXMAP_SYLPHEED_32, &icon);
	iconlist = g_list_append(iconlist, icon);
	stock_pixbuf_gdk(NULL, STOCK_PIXMAP_SYLPHEED_SMALL, &icon);
	iconlist = g_list_append(iconlist, icon);
	gtk_window_set_default_icon_list(iconlist);
	g_list_free(iconlist);
#else
	stock_pixbuf_gdk(NULL, STOCK_PIXMAP_SYLPHEED, &icon);
	gtk_window_set_default_icon(icon);
#endif

	mainwin = main_window_create
		(prefs_common.sep_folder | prefs_common.sep_msg << 1);
	folderview = mainwin->folderview;

	/* register the callback of socket input */
	if (lock_socket > 0) {
		lock_ch = g_io_channel_unix_new(lock_socket);
		lock_socket_tag = g_io_add_watch(lock_ch,
						 G_IO_IN|G_IO_PRI|G_IO_ERR,
						 lock_socket_input_cb, mainwin);
	}

	set_log_handlers(TRUE);

	account_read_config_all();
	account_set_menu();
	main_window_reflect_prefs_all();

	if (folder_read_list() < 0) {
		first_run = TRUE;
		setup_mailbox();
		folder_write_list();
	}
	if (!account_get_list()) {
		new_account = setup_account();
	}

	account_set_menu();
	main_window_reflect_prefs_all();

	account_set_missing_folder();
	folder_set_missing_folders();
	folderview_set(folderview);
	if (new_account && new_account->folder)
		folder_write_list();

	addressbook_read_file();

	register_system_events();

	inc_autocheck_timer_init(mainwin);

	plugin_init();

	g_signal_emit_by_name(syl_app, "init-done");

	if (first_run) {
		setup_import_data();
		setup_import_addressbook();
	}

	remote_command_exec();

#if USE_UPDATE_CHECK
	if (prefs_common.auto_update_check)
		update_check(FALSE);
#endif

	gtk_main();
#if USE_THREADS
	gdk_threads_leave();
#endif

	return 0;
}

static void init_console(void)
{
#ifdef G_OS_WIN32
	gint fd;
	FILE *fp;

	if (init_console_done)
		return;

	if (!AllocConsole()) {
		g_warning("AllocConsole() failed\n");
		return;
	}

	fd = _open_osfhandle((glong)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
	_dup2(fd, 1);
	fp = _fdopen(fd, "w");
	*stdout = *fp;
	setvbuf(stdout, NULL, _IONBF, 0);
	fd = _open_osfhandle((glong)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
	_dup2(fd, 2);
	fp = _fdopen(fd, "w");
	*stderr = *fp;
	setvbuf(stderr, NULL, _IONBF, 0);

	init_console_done = TRUE;
#endif
}

static void cleanup_console(void)
{
#ifdef G_OS_WIN32
	FreeConsole();
#endif
}

#ifdef G_OS_WIN32
static void read_ini_file(void)
{
	static gushort ipcport = REMOTE_CMD_PORT;
	static gchar *confdir = NULL;

	static PrefParam param[] = {
		{"ipcport", "50215", &ipcport, P_USHORT},
		{"configdir", NULL, &confdir, P_STRING},

		{NULL, NULL, NULL, P_OTHER}
	};

	gchar *file;

	file = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S, "sylpheed.ini",
			   NULL);
	if (!is_file_exist(file)) {
		g_free(file);
		return;
	}

	prefs_read_config(param, "Sylpheed", file,
			  conv_get_locale_charset_str());
	g_free(file);

	cmd.ipcport = ipcport;
	if (confdir) {
		set_rc_dir(confdir);
		g_free(confdir);
		confdir = NULL;
		cmd.configdir = TRUE;
	}
}
#endif /* G_OS_WIN32 */

#if defined(G_OS_WIN32) || defined(__APPLE__)
static void fix_font_setting(void)
{
	gchar *str = NULL;
	gchar **strv = NULL;
	gchar *style = NULL;
	gchar *size = NULL;
	gchar *suffix = NULL;

	if (!conv_is_ja_locale())
		return;

	if (prefs_common.textfont &&
	    strcmp(prefs_common.textfont, DEFAULT_MESSAGE_FONT) != 0) {
		guint len;

		if (gtkut_font_can_load(prefs_common.textfont)) {
			debug_print("font '%s' load ok\n", prefs_common.textfont);
			return;
		}
		debug_print("font '%s' load failed\n", prefs_common.textfont);
		debug_print("fixing prefs_common.textfont setting\n");

		strv = g_strsplit(prefs_common.textfont, " ", -1);
		len = g_strv_length(strv);
		if (len > 0) {
			if (g_ascii_isdigit(strv[len - 1][0]))
				size = g_strdup(strv[len - 1]);
			if (len > 2) {
				if (g_ascii_strcasecmp(strv[len - 2], "Bold") == 0)
					style = g_strdup(strv[len - 2]);
			}
		}
		g_strfreev(strv);
	}

	if (style && size)
		suffix = g_strconcat(style, " ", size, NULL);
	else if (size)
		suffix = g_strdup(size);
	else
		suffix = g_strdup("12");
	g_free(style);	
	g_free(size);
#ifdef G_OS_WIN32
	str = g_strconcat("MS Gothic ", suffix, NULL);
#else /* __APPLE__ */
	str = g_strconcat("Hiragino Kaku Gothic Pro ", suffix, NULL);
#endif

	if (!gtkut_font_can_load(str)) {
#ifdef G_OS_WIN32
		debug_print("font '%s' load failed\n", str);
		g_free(str);
		str = g_strconcat("\xef\xbc\xad\xef\xbc\xb3 \xe3\x82\xb4\xe3\x82\xb7\xe3\x83\x83\xe3\x82\xaf ", suffix, NULL);
		if (!gtkut_font_can_load(str)) {
			debug_print("font '%s' load failed\n", str);
			g_free(str);
			str = NULL;
		}
#else /* __APPLE__ */
		debug_print("font '%s' load failed\n", str);
		g_free(str);
		str = NULL;
#endif
	}

	if (str) {
		debug_print("font '%s' load ok\n", str);
		g_free(prefs_common.textfont);
		prefs_common.textfont = g_strdup(str);
		g_free(str);
	} else
		g_warning("failed to load text font!");

	g_free(suffix);
}
#endif

static void parse_cmd_opt(int argc, char *argv[])
{
	gint i;

	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--debug", 7)) {
			init_console();
			set_debug_mode(TRUE);
		} else if (!strncmp(argv[i], "--receive-all", 13))
			cmd.receive_all = TRUE;
		else if (!strncmp(argv[i], "--receive", 9))
			cmd.receive = TRUE;
		else if (!strncmp(argv[i], "--compose", 9)) {
			const gchar *p = argv[i + 1];

			cmd.compose = TRUE;
			cmd.compose_mailto = NULL;
			if (p && *p != '\0' && *p != '-') {
				if (!strncmp(p, "mailto:", 7))
					cmd.compose_mailto = p + 7;
				else
					cmd.compose_mailto = p;
				i++;
			}
		} else if (!strncmp(argv[i], "--attach", 8)) {
			const gchar *p = argv[i + 1];
			gchar *file;

			while (p && *p != '\0' && *p != '-') {
				if (!cmd.attach_files)
					cmd.attach_files = g_ptr_array_new();
				if (!g_path_is_absolute(p))
					file = g_strconcat(get_startup_dir(),
							   G_DIR_SEPARATOR_S,
							   p, NULL);
				else
					file = g_strdup(p);
				g_ptr_array_add(cmd.attach_files, file);
				i++;
				p = argv[i + 1];
			}
		} else if (!strncmp(argv[i], "--send", 6)) {
			cmd.send = TRUE;
		} else if (!strncmp(argv[i], "--version", 9)) {
			puts("Sylpheed version " VERSION);
			exit(0);
		} else if (!strncmp(argv[i], "--status-full", 13)) {
			const gchar *p = argv[i + 1];

			cmd.status_full = TRUE;
			while (p && *p != '\0' && *p != '-') {
				if (!cmd.status_full_folders)
					cmd.status_full_folders =
						g_ptr_array_new();
				g_ptr_array_add(cmd.status_full_folders,
						g_strdup(p));
				i++;
				p = argv[i + 1];
			}
		} else if (!strncmp(argv[i], "--status", 8)) {
			const gchar *p = argv[i + 1];

			cmd.status = TRUE;
			while (p && *p != '\0' && *p != '-') {
				if (!cmd.status_folders)
					cmd.status_folders = g_ptr_array_new();
				g_ptr_array_add(cmd.status_folders,
						g_strdup(p));
				i++;
				p = argv[i + 1];
			}
		} else if (!strncmp(argv[i], "--open", 6)) {
			const gchar *p = argv[i + 1];

			if (p && *p != '\0' && *p != '-') {
				if (cmd.open_msg)
					g_free(cmd.open_msg);
				cmd.open_msg = g_locale_to_utf8
					(p, -1, NULL, NULL, NULL);
				i++;
			}
		} else if (!strncmp(argv[i], "--configdir", 11)) {
			const gchar *p = argv[i + 1];

			if (p && *p != '\0' && *p != '-') {
				/* this must only be done at startup */
#ifdef G_OS_WIN32
				gchar *utf8dir;

				utf8dir = g_locale_to_utf8
					(p, -1, NULL, NULL, NULL);
				if (utf8dir) {
					set_rc_dir(utf8dir);
					g_free(utf8dir);
				} else
					set_rc_dir(p);
#else
				set_rc_dir(p);
#endif
				cmd.configdir = TRUE;
				i++;
			}
#ifdef G_OS_WIN32
		} else if (!strncmp(argv[i], "--ipcport", 9)) {
			if (argv[i + 1]) {
				cmd.ipcport = atoi(argv[i + 1]);
				i++;
			}
#endif
		} else if (!strncmp(argv[i], "--instance-id", 13)) {
			if (argv[i + 1]) {
				instance_id = g_locale_to_utf8
					(argv[i + 1], -1, NULL, NULL, NULL);
				i++;
			}
		} else if (!strncmp(argv[i], "--safe-mode", 11)) {
			cmd.safe_mode = TRUE;
		} else if (!strncmp(argv[i], "--dpi", 5)) {
			if (argv[i + 1]) {
				gdouble dpi;

				dpi = strtod(argv[i + 1], NULL);
				if (dpi > 0.0)
					gtkut_set_dpi(dpi);
				i++;
			}
		} else if (!strncmp(argv[i], "--exit", 6)) {
			cmd.exit = TRUE;
		} else if (!strncmp(argv[i], "--help", 6)) {
			init_console();

			g_print(_("Usage: %s [OPTIONS ...] [URL]\n"),
				g_basename(argv[0]));

			g_print("%s\n", _("  --compose [mailto URL] open composition window"));
			g_print("%s\n", _("  --attach file1 [file2]...\n"
				"                         open composition window with specified files\n"
				"                         attached"));
			g_print("%s\n", _("  --receive              receive new messages"));
			g_print("%s\n", _("  --receive-all          receive new messages of all accounts"));
			g_print("%s\n", _("  --send                 send all queued messages"));
			g_print("%s\n", _("  --status [folder]...   show the total number of messages"));
			g_print("%s\n", _("  --status-full [folder]...\n"
				"                         show the status of each folder"));
			g_print("%s\n", _("  --open folderid/msgnum open existing message in a new window"));
			g_print("%s\n", _("  --open <file URL>      open an rfc822 message file in a new window"));
			g_print("%s\n", _("  --configdir dirname    specify directory which stores configuration files"));
#ifdef G_OS_WIN32
			g_print("%s\n", _("  --ipcport portnum      specify port for IPC remote commands"));
#endif
			g_print("%s\n", _("  --dpi dpinum           force DPI"));
			g_print("%s\n", _("  --exit                 exit Sylpheed"));
			g_print("%s\n", _("  --debug                debug mode"));
			g_print("%s\n", _("  --safe-mode            safe mode"));
			g_print("%s\n", _("  --help                 display this help and exit"));
			g_print("%s\n", _("  --version              output version information and exit"));

#ifdef G_OS_WIN32
			g_print("\n");
			g_print(_("Press any key..."));
			_getch();
#endif

			cleanup_console();
			exit(1);
		} else {
			/* file or URL */
			const gchar *p = argv[i];

			if (p && *p != '\0') {
				if (!strncmp(p, "mailto:", 7)) {
					cmd.compose = TRUE;
					cmd.compose_mailto = p + 7;
				} else {
					if (cmd.open_msg)
						g_free(cmd.open_msg);
					cmd.open_msg = g_locale_to_utf8
						(p, -1, NULL, NULL, NULL);
				}
			}
		}
	}

	if (cmd.attach_files && cmd.compose == FALSE) {
		cmd.compose = TRUE;
		cmd.compose_mailto = NULL;
	}

	cmd.argv0 = g_locale_to_utf8(argv[0], -1, NULL, NULL, NULL);
	if (!cmd.argv0)
		cmd.argv0 = g_strdup(argv[0]);
}

static gint get_queued_message_num(void)
{
	FolderItem *queue;

	queue = folder_get_default_queue();
	if (!queue) return -1;

	folder_item_scan(queue);
	return queue->total;
}

#if USE_THREADS
/* enables recursive locking with gdk_thread_enter / gdk_threads_leave */
static GStaticRecMutex syl_mutex = G_STATIC_REC_MUTEX_INIT;

static void thread_enter_func(void)
{
	g_static_rec_mutex_lock(&syl_mutex);
#if 0
	syl_mutex_lock_count++;
	if (syl_mutex_lock_count > 1)
		g_print("enter: syl_mutex_lock_count: %d\n", syl_mutex_lock_count);
#endif
}

static void thread_leave_func(void)
{
#if 0
	syl_mutex_lock_count--;
	if (syl_mutex_lock_count > 0)
		g_print("leave: syl_mutex_lock_count: %d\n", syl_mutex_lock_count);
#endif
	g_static_rec_mutex_unlock(&syl_mutex);
}

static void event_loop_iteration_func(void)
{
	if (g_thread_self() != main_thread) {
		g_fprintf(stderr, "event_loop_iteration_func called from non-main thread (%p)\n", g_thread_self());
		g_usleep(10000);
		return;
	}
	gtk_main_iteration();
}
#endif

static void app_init(void)
{
#if USE_THREADS
	if (!g_thread_supported())
		g_thread_init(NULL);
	if (!g_thread_supported())
		g_error("g_thread is not supported by glib.");
	else {
		gdk_threads_set_lock_functions(thread_enter_func,
					       thread_leave_func);
		gdk_threads_init();
		main_thread = g_thread_self();
	}
#endif
	syl_init();

#if USE_THREADS
	set_event_loop_func(event_loop_iteration_func);
#endif
	prog_version = PROG_VERSION;

#ifdef G_OS_WIN32
	read_ini_file();
#endif
}

static void parse_gtkrc_files(void)
{
	gchar *userrc;

	/* parse gtkrc files */
	userrc = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S, ".gtkrc-2.0",
			     NULL);
	gtk_rc_parse(userrc);
	g_free(userrc);
	userrc = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S, ".gtk",
			     G_DIR_SEPARATOR_S, "gtkrc-2.0", NULL);
	gtk_rc_parse(userrc);
	g_free(userrc);
	userrc = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "gtkrc", NULL);
	gtk_rc_parse(userrc);
	g_free(userrc);

	userrc = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MENU_RC, NULL);
	gtk_accel_map_load(userrc);
	g_free(userrc);
}

static void setup_rc_dir(void)
{
#if !defined(G_OS_WIN32) && !defined(__APPLE__)
	CHDIR_EXIT_IF_FAIL(get_home_dir(), 1);

	/* backup if old rc file exists */
	if (!cmd.configdir && is_file_exist(RC_DIR)) {
		if (rename_force(RC_DIR, RC_DIR ".bak") < 0)
			FILE_OP_ERROR(RC_DIR, "rename");
	}

	/* migration from ~/.sylpheed to ~/.sylpheed-2.0 */
	if (!cmd.configdir && !is_dir_exist(RC_DIR)) {
		const gchar *envstr;
		AlertValue val;

		/* check for filename encoding */
		if (conv_get_locale_charset() != C_UTF_8) {
			envstr = g_getenv("G_FILENAME_ENCODING");
			if (!envstr)
				envstr = g_getenv("G_BROKEN_FILENAMES");
			if (!envstr) {
				val = alertpanel(_("Filename encoding"),
						 _("The locale encoding is not UTF-8, but the environmental variable G_FILENAME_ENCODING is not set.\n"
						   "If the locale encoding is used for file name or directory name, it will not work correctly.\n"
						   "In that case, you must set the following environmental variable (see README for detail):\n"
						   "\n"
						   "\tG_FILENAME_ENCODING=@locale\n"
						   "\n"
						   "Continue?"),
						 GTK_STOCK_OK, GTK_STOCK_QUIT,
						 NULL);
				if (G_ALERTDEFAULT != val)
					exit(1);
			}
		}

		if (make_dir(RC_DIR) < 0)
			exit(1);
		if (is_dir_exist(OLD_RC_DIR))
			migrate_old_config();
	}
#endif /* !G_OS_WIN32 && !__APPLE__ */

	syl_setup_rc_dir();
}

static void app_restart(void)
{
	gchar *cmdline;
	GError *error = NULL;
#ifdef G_OS_WIN32
	if (cmd.configdir) {
		cmdline = g_strdup_printf("\"%s\"%s --configdir \"%s\" --ipcport %d",
					  cmd.argv0,
					  get_debug_mode() ? " --debug" : "",
					  get_rc_dir(),
					  cmd.ipcport);
	} else {
		cmdline = g_strdup_printf("\"%s\"%s --ipcport %d",
					  cmd.argv0,
					  get_debug_mode() ? " --debug" : "",
					  cmd.ipcport);
	}
#else
	if (cmd.configdir) {
		cmdline = g_strdup_printf("\"%s\"%s --configdir \"%s\"",
					  cmd.argv0,
					  get_debug_mode() ? " --debug" : "",
					  get_rc_dir());
	} else {
		cmdline = g_strdup_printf("\"%s\"%s",
					  cmd.argv0,
					  get_debug_mode() ? " --debug" : "");
	}
#endif
	if (!g_spawn_command_line_async(cmdline, &error)) {
		alertpanel_error("restart failed\n'%s'\n%s", cmdline, error->message);
		g_error_free(error);
	}
	g_free(cmdline);
}

void app_will_restart(gboolean force)
{
	cmd.restart = TRUE;
	app_will_exit(force);
	/* canceled */
	cmd.restart = FALSE;
}

void app_will_exit(gboolean force)
{
	MainWindow *mainwin;
	gchar *filename;
	static gboolean on_exit = FALSE;
	GList *cur;

	if (on_exit)
		return;
	on_exit = TRUE;

	mainwin = main_window_get();

	if (!force && compose_get_compose_list()) {
		if (alertpanel(_("Notice"),
			       _("Composing message exists. Really quit?"),
			       GTK_STOCK_OK, GTK_STOCK_CANCEL, NULL)
		    != G_ALERTDEFAULT) {
			on_exit = FALSE;
			return;
		}
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	if (!force &&
	    prefs_common.warn_queued_on_exit && get_queued_message_num() > 0) {
		if (alertpanel(_("Queued messages"),
			       _("Some unsent messages are queued. Exit now?"),
			       GTK_STOCK_OK, GTK_STOCK_CANCEL, NULL)
		    != G_ALERTDEFAULT) {
			on_exit = FALSE;
			return;
		}
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	if (force)
		g_signal_emit_by_name(syl_app_get(), "app-force-exit");
	g_signal_emit_by_name(syl_app_get(), "app-exit");

	inc_autocheck_timer_remove();

	if (prefs_common.clean_on_exit)
		main_window_empty_trash(mainwin,
					!force && prefs_common.ask_on_clean);

	for (cur = account_get_list(); cur != NULL; cur = cur->next) {
		PrefsAccount *ac = (PrefsAccount *)cur->data;
		if (ac->protocol == A_IMAP4 && ac->imap_clear_cache_on_exit &&
		    ac->folder)
			procmsg_remove_all_cached_messages(FOLDER(ac->folder));
	}

	syl_plugin_unload_all();

	trayicon_destroy(mainwin->tray_icon);

	/* save all state before exiting */
	summary_write_cache(mainwin->summaryview);
	main_window_get_size(mainwin);
	main_window_get_position(mainwin);
	syl_save_all_state();
	addressbook_export_to_file();

	filename = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MENU_RC, NULL);
	gtk_accel_map_save(filename);
	g_free(filename);

	/* remove temporary files, close log file, socket cleanup */
#if USE_SSL
	ssl_done();
#endif
	syl_cleanup();
	lock_socket_remove();

#ifdef USE_UPDATE_CHECK_PLUGIN
#ifdef G_OS_WIN32
	cur = gtk_window_list_toplevels();
	g_list_foreach(cur, (GFunc)gtk_widget_hide, NULL);
	g_list_free(cur);
	update_check_spawn_plugin_updater();
#endif
#endif

	cleanup_console();

	if (gtk_main_level() > 0)
		gtk_main_quit();

	if (cmd.restart)
		app_restart();

	exit(0);
}

#if 0
#if USE_GPGME
static void idle_function_for_gpgme(void)
{
	while (gtk_events_pending())
		gtk_main_iteration();
}
#endif /* USE_GPGME */
#endif /* 0 */

static void check_gpg(void)
{
#if USE_GPGME
	const gchar *version;
	gpgme_error_t err = 0;

	version = gpgme_check_version("1.0.0");
	if (version) {
		debug_print("GPGME Version: %s\n", version);
		err = gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP);
		if (err)
			debug_print("gpgme_engine_check_version: %s\n",
				    gpgme_strerror(err));
	}

	if (version && !err) {
		/* Also does some gpgme init */
	        gpgme_engine_info_t engineInfo;

#if HAVE_LOCALE_H
		gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
		gpgme_set_locale(NULL, LC_MESSAGES,
				 setlocale(LC_MESSAGES, NULL));
#endif

		if (!gpgme_get_engine_info(&engineInfo)) {
			while (engineInfo) {
				debug_print("GPGME Protocol: %s\n      Version: %s\n",
					    gpgme_get_protocol_name
						(engineInfo->protocol),
					    engineInfo->version ?
					    engineInfo->version : "(unknown)");
				engineInfo = engineInfo->next;
			}
		}

		procmsg_set_decrypt_message_func
			(rfc2015_open_message_decrypted);
		procmsg_set_auto_decrypt_message(TRUE);
	} else {
		rfc2015_disable_all();

		if (prefs_common.gpg_warning) {
			AlertValue val;

			val = alertpanel_message_with_disable
				(_("Warning"),
				 _("GnuPG is not installed properly, or its version is too old.\n"
				   "OpenPGP support disabled."),
				 ALERT_WARNING);
			if (val & G_ALERTDISABLE)
				prefs_common.gpg_warning = FALSE;
		}
	}
	/* FIXME: This function went away.  We can either block until gpgme
	 * operations finish (currently implemented) or register callbacks
	 * with the gtk main loop via the gpgme io callback interface instead.
	 *
	 * gpgme_register_idle(idle_function_for_gpgme);
	 */
#endif
}

static void default_log_func(const gchar *log_domain, GLogLevelFlags log_level,
			     const gchar *message, gpointer user_data)
{
	gchar *prefix = "";
	gchar *file_prefix = "";
	LogType level = LOG_NORMAL;
	gchar *str;
	const gchar *message_;

	switch (log_level) {
	case G_LOG_LEVEL_ERROR:
		prefix = "ERROR";
		file_prefix = "*** ";
		level = LOG_ERROR;
		break;
	case G_LOG_LEVEL_CRITICAL:
		prefix = "CRITICAL";
		file_prefix = "** ";
		level = LOG_WARN;
		break;
	case G_LOG_LEVEL_WARNING:
		prefix = "WARNING";
		file_prefix = "** ";
		level = LOG_WARN;
		break;
	case G_LOG_LEVEL_MESSAGE:
		prefix = "Message";
		file_prefix = "* ";
		level = LOG_MSG;
		break;
	case G_LOG_LEVEL_INFO:
		prefix = "INFO";
		file_prefix = "* ";
		level = LOG_MSG;
		break;
	case G_LOG_LEVEL_DEBUG:
		prefix = "DEBUG";
		break;
	default:
		prefix = "LOG";
		break;
	}

	if (!message)
		message_ = "(NULL) message";
	else
		message_ = message;
	if (log_domain)
		str = g_strconcat(log_domain, "-", prefix, ": ", message_, "\n",
				  NULL);
	else
		str = g_strconcat(prefix, ": ", message_, "\n", NULL);
	log_window_append(str, level);
	log_write(str, file_prefix);
	g_free(str);

	g_log_default_handler(log_domain, log_level, message, user_data);
}

static void set_log_handlers(gboolean enable)
{
#if GLIB_CHECK_VERSION(2, 6, 0)
	if (enable)
		g_log_set_default_handler(default_log_func, NULL);
	else
		g_log_set_default_handler(g_log_default_handler, NULL);
#else
	static guint handler_id[4] = {0, 0, 0, 0};

	if (enable) {
		handler_id[0] = g_log_set_handler
			("GLib", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
			 | G_LOG_FLAG_RECURSION, default_log_func, NULL);
		handler_id[1] = g_log_set_handler
			("Gtk", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
			 | G_LOG_FLAG_RECURSION, default_log_func, NULL);
		handler_id[2] = g_log_set_handler
			("LibSylph", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
			 | G_LOG_FLAG_RECURSION, default_log_func, NULL);
		handler_id[3] = g_log_set_handler
			("Sylpheed", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
			 | G_LOG_FLAG_RECURSION, default_log_func, NULL);
	} else {
		g_log_remove_handler("GLib", handler_id[0]);
		g_log_remove_handler("Gtk", handler_id[1]);
		g_log_remove_handler("LibSylph", handler_id[2]);
		g_log_remove_handler("Sylpheed", handler_id[3]);
		handler_id[0] = 0;
		handler_id[1] = 0;
		handler_id[2] = 0;
		handler_id[3] = 0;
	}
#endif
}

#ifdef G_OS_WIN32

#if !GTK_CHECK_VERSION(2, 14, 0)
static UINT taskbar_created_msg;
#endif

static BOOL WINAPI
ctrl_handler(DWORD dwctrltype)
{
	log_print("ctrl_handler: received %d\n", dwctrltype);
	app_will_exit(TRUE);

	return TRUE;
}

static LRESULT CALLBACK
wndproc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message) {
	case WM_POWERBROADCAST:
		debug_print("WM_POWERBROADCAST received: wparam = %d\n",
			    wparam);
		if (wparam == PBT_APMSUSPEND || wparam == PBT_APMSTANDBY) {
			debug_print("suspend now\n");
			inc_autocheck_timer_remove();
		} else if (wparam == PBT_APMRESUMESUSPEND ||
			   wparam == PBT_APMRESUMESTANDBY) {
			debug_print("resume now\n");
			inc_autocheck_timer_set();
		}
		break;
	case WM_ENDSESSION:
		if (wparam == 1) {
			log_print("WM_ENDSESSION received: system is quitting\n");
			app_will_exit(TRUE);
		}
		break;
	default:
#if !GTK_CHECK_VERSION(2, 14, 0)
		if (message == taskbar_created_msg) {
			debug_print("TaskbarCreated received\n");

			/* recreate tray icon */
			{
				MainWindow *mainwin = main_window_get();
				if (mainwin && mainwin->tray_icon &&
				    gtk_status_icon_get_visible(mainwin->tray_icon->status_icon)) {
					trayicon_hide(mainwin->tray_icon);
					trayicon_show(mainwin->tray_icon);
				}
			}
		}
#endif
		break;
	}

	return DefWindowProc(hwnd, message, wparam, lparam);
}

static void register_system_events(void)
{
	WNDCLASS wclass;
	static HWND hwnd = NULL;
	static BOOL ctrl_handler_set = FALSE;
	ATOM klass;
	HINSTANCE hmodule = GetModuleHandle(NULL);

	if (init_console_done && !ctrl_handler_set) {
		debug_print("register_system_events(): SetConsoleCtrlHandler\n");
		ctrl_handler_set = SetConsoleCtrlHandler(ctrl_handler, TRUE);
		if (!ctrl_handler_set)
			g_warning("SetConsoleCtrlHandler() failed\n");
	}

	if (hwnd)
		return;

#if !GTK_CHECK_VERSION(2, 14, 0)
	taskbar_created_msg = RegisterWindowMessage("TaskbarCreated");
#endif

	debug_print("register_system_events(): RegisterClass\n");

	memset(&wclass, 0, sizeof(WNDCLASS));
	wclass.lpszClassName = "sylpheed-observer";
	wclass.lpfnWndProc   = wndproc;
	wclass.hInstance     = hmodule;

	klass = RegisterClass(&wclass);
	if (!klass)
		return;

	hwnd = CreateWindow(MAKEINTRESOURCE(klass), NULL, WS_POPUP,
			    0, 0, 1, 1, NULL, NULL, hmodule, NULL);
	if (!hwnd)
		UnregisterClass(MAKEINTRESOURCE(klass), hmodule);
}
#else /* G_OS_WIN32 */
static void sig_handler(gint signum)
{
	debug_print("signal %d received\n", signum);

	switch (signum) {
	case SIGHUP:
	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		app_will_exit(TRUE);
		break;
	default:
		break;
	}
}

static void register_system_events(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	sa.sa_flags = SA_RESTART;

	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGHUP);
	sigaddset(&sa.sa_mask, SIGINT);
	sigaddset(&sa.sa_mask, SIGTERM);
	sigaddset(&sa.sa_mask, SIGQUIT);
	sigaddset(&sa.sa_mask, SIGPIPE);

	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
}
#endif

#define ADD_SYM(sym)	syl_plugin_add_symbol(#sym, sym)

static void plugin_init(void)
{
	MainWindow *mainwin;
	gchar *path;

	mainwin = main_window_get();

	STATUSBAR_PUSH(mainwin, _("Loading plug-ins..."));

	if (syl_plugin_init_lib() != 0) {
		STATUSBAR_POP(mainwin);
		return;
	}

	if (cmd.safe_mode) {
		debug_print("plugin_init: safe mode enabled, skipping plug-in loading.\n");
		STATUSBAR_POP(mainwin);
		return;
	}

	ADD_SYM(prog_version);
	ADD_SYM(app_will_exit);

	ADD_SYM(main_window_lock);
	ADD_SYM(main_window_unlock);
	ADD_SYM(main_window_get);
	ADD_SYM(main_window_popup);

	syl_plugin_add_symbol("main_window_menu_factory",
			      mainwin->menu_factory);
	syl_plugin_add_symbol("main_window_toolbar", mainwin->toolbar);
	syl_plugin_add_symbol("main_window_statusbar", mainwin->statusbar);

	ADD_SYM(folderview_get);
	ADD_SYM(folderview_add_sub_widget);
	ADD_SYM(folderview_select);
	ADD_SYM(folderview_unselect);
	ADD_SYM(folderview_select_next_unread);
	ADD_SYM(folderview_get_selected_item);
	ADD_SYM(folderview_check_new);
	ADD_SYM(folderview_check_new_item);
	ADD_SYM(folderview_check_new_all);
	ADD_SYM(folderview_update_item);
	ADD_SYM(folderview_update_item_foreach);
	ADD_SYM(folderview_update_all_updated);
	ADD_SYM(folderview_check_new_selected);

	syl_plugin_add_symbol("folderview_mail_popup_factory",
			      mainwin->folderview->mail_factory);
	syl_plugin_add_symbol("folderview_imap_popup_factory",
			      mainwin->folderview->imap_factory);
	syl_plugin_add_symbol("folderview_news_popup_factory",
			      mainwin->folderview->news_factory);

	syl_plugin_add_symbol("summaryview", mainwin->summaryview);
	syl_plugin_add_symbol("summaryview_popup_factory",
			      mainwin->summaryview->popupfactory);

	ADD_SYM(summary_select_by_msgnum);
	ADD_SYM(summary_select_by_msginfo);
	ADD_SYM(summary_lock);
	ADD_SYM(summary_unlock);
	ADD_SYM(summary_is_locked);
	ADD_SYM(summary_is_read_locked);
	ADD_SYM(summary_write_lock);
	ADD_SYM(summary_write_unlock);
	ADD_SYM(summary_is_write_locked);
	ADD_SYM(summary_get_current_folder);
	ADD_SYM(summary_get_selection_type);
	ADD_SYM(summary_get_selected_msg_list);
	ADD_SYM(summary_get_msg_list);
	ADD_SYM(summary_show_queued_msgs);
	ADD_SYM(summary_redisplay_msg);
	ADD_SYM(summary_open_msg);
	ADD_SYM(summary_view_source);
	ADD_SYM(summary_reedit);
	ADD_SYM(summary_update_selected_rows);
	ADD_SYM(summary_update_by_msgnum);

	ADD_SYM(messageview_create_with_new_window);
	ADD_SYM(messageview_show);

	ADD_SYM(compose_new);
	ADD_SYM(compose_reply);
	ADD_SYM(compose_forward);
	ADD_SYM(compose_redirect);
	ADD_SYM(compose_reedit);
	ADD_SYM(compose_entry_set);
	ADD_SYM(compose_entry_append);
	ADD_SYM(compose_entry_get_text);
	ADD_SYM(compose_lock);
	ADD_SYM(compose_unlock);
	ADD_SYM(compose_get_toolbar);
	ADD_SYM(compose_get_misc_hbox);
	ADD_SYM(compose_get_textview);
	ADD_SYM(compose_attach_append);
	ADD_SYM(compose_attach_remove_all);
	ADD_SYM(compose_get_attach_list);
	ADD_SYM(compose_send);

	ADD_SYM(foldersel_folder_sel);
	ADD_SYM(foldersel_folder_sel_full);

	ADD_SYM(input_dialog);
	ADD_SYM(input_dialog_with_invisible);

	ADD_SYM(manage_window_set_transient);
	ADD_SYM(manage_window_signals_connect);
	ADD_SYM(manage_window_get_focus_window);

	ADD_SYM(inc_mail);
	ADD_SYM(inc_is_active);
	ADD_SYM(inc_lock);
	ADD_SYM(inc_unlock);

#if USE_UPDATE_CHECK
	ADD_SYM(update_check);
	ADD_SYM(update_check_set_check_url);
	ADD_SYM(update_check_get_check_url);
	ADD_SYM(update_check_set_download_url);
	ADD_SYM(update_check_get_download_url);
	ADD_SYM(update_check_set_jump_url);
	ADD_SYM(update_check_get_jump_url);
#ifdef USE_UPDATE_CHECK_PLUGIN
	ADD_SYM(update_check_set_check_plugin_url);
	ADD_SYM(update_check_get_check_plugin_url);
	ADD_SYM(update_check_set_jump_plugin_url);
	ADD_SYM(update_check_get_jump_plugin_url);
#endif /* USE_UPDATE_CHECK_PLUGIN */
#endif

	ADD_SYM(alertpanel_full);
	ADD_SYM(alertpanel);
	ADD_SYM(alertpanel_message);
	ADD_SYM(alertpanel_message_with_disable);

	ADD_SYM(send_message);
	ADD_SYM(send_message_queue_all);
	ADD_SYM(send_message_set_reply_flag);
	ADD_SYM(send_message_set_forward_flags);

	ADD_SYM(notification_window_open);
	ADD_SYM(notification_window_set_message);
	ADD_SYM(notification_window_close);

	syl_plugin_signal_connect("plugin-load", G_CALLBACK(load_cb), NULL);

	/* loading plug-ins from user plug-in directory */
	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, PLUGIN_DIR, NULL);
	syl_plugin_load_all(path);
	g_free(path);

	/* loading plug-ins from system plug-in directory */
#ifdef G_OS_WIN32
	path = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S, PLUGIN_DIR,
			   NULL);
	syl_plugin_load_all(path);
	g_free(path);
#elif defined(__APPLE__)
	path = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S,
			   "Contents" G_DIR_SEPARATOR_S
			   "Resources" G_DIR_SEPARATOR_S
			   "lib" G_DIR_SEPARATOR_S
			   "sylpheed" G_DIR_SEPARATOR_S
			   PLUGIN_DIR,
			   NULL);
	syl_plugin_load_all(path);
	g_free(path);
#else
	syl_plugin_load_all(PLUGINDIR);
#endif

	STATUSBAR_POP(mainwin);
}

static gchar *get_socket_name(void)
{
	static gchar *filename = NULL;

	if (filename == NULL) {
		filename = g_strdup_printf("%s%c%s-%d",
					   g_get_tmp_dir(), G_DIR_SEPARATOR,
					   instance_id ? instance_id : "sylpheed",
#if HAVE_GETUID
					   getuid());
#else
					   0);
#endif
	}

	return filename;
}

static gint prohibit_duplicate_launch(void)
{
	gint sock;

#ifdef G_OS_WIN32
	HANDLE hmutex;
	const gchar *ins_id = instance_id ? instance_id : "Sylpheed";
	gushort port = cmd.ipcport ? cmd.ipcport : REMOTE_CMD_PORT;

	debug_print("prohibit_duplicate_launch: checking mutex: %s\n", ins_id);
	hmutex = CreateMutexA(NULL, FALSE, ins_id);
	if (!hmutex) {
		g_warning("cannot create Mutex: %s\n", ins_id);
		return -1;
	}
	if (GetLastError() != ERROR_ALREADY_EXISTS) {
		debug_print("prohibit_duplicate_launch: creating socket: port %d\n", port);
		sock = fd_open_inet(port);
		if (sock < 0)
			return 0;
		return sock;
	}

	debug_print("prohibit_duplicate_launch: connecting to socket: port %d\n", port);
	sock = fd_connect_inet(port);
	if (sock < 0)
		return -1;
#else
	gchar *path;

	path = get_socket_name();
	debug_print("prohibit_duplicate_launch: checking socket: %s\n", path);
	sock = fd_connect_unix(path);
	if (sock < 0) {
		debug_print("prohibit_duplicate_launch: creating socket: %s\n", path);
		g_unlink(path);
		return fd_open_unix(path);
	}
#endif

	/* remote command mode */

	debug_print(_("another Sylpheed is already running.\n"));

	if (cmd.receive_all)
		fd_write_all(sock, "receive_all\n", 12);
	else if (cmd.receive)
		fd_write_all(sock, "receive\n", 8);
	else if (cmd.compose && cmd.attach_files) {
		gchar *str, *compose_str;
		gint i;

		if (cmd.compose_mailto)
			compose_str = g_strdup_printf("compose_attach %s\n",
						      cmd.compose_mailto);
		else
			compose_str = g_strdup("compose_attach\n");

		fd_write_all(sock, compose_str, strlen(compose_str));
		g_free(compose_str);

		for (i = 0; i < cmd.attach_files->len; i++) {
			str = g_ptr_array_index(cmd.attach_files, i);
			fd_write_all(sock, str, strlen(str));
			fd_write_all(sock, "\n", 1);
		}

		fd_write_all(sock, ".\n", 2);
	} else if (cmd.compose) {
		gchar *compose_str;

		if (cmd.compose_mailto)
			compose_str = g_strdup_printf
				("compose %s\n", cmd.compose_mailto);
		else
			compose_str = g_strdup("compose\n");

		fd_write_all(sock, compose_str, strlen(compose_str));
		g_free(compose_str);
	} else if (cmd.send) {
		fd_write_all(sock, "send\n", 5);
	} else if (cmd.status || cmd.status_full) {
		gchar buf[BUFFSIZE];
		gint i;
		const gchar *command;
		GPtrArray *folders;
		gchar *folder;

		command = cmd.status_full ? "status-full\n" : "status\n";
		folders = cmd.status_full ? cmd.status_full_folders :
			cmd.status_folders;

		fd_write_all(sock, command, strlen(command));
		for (i = 0; folders && i < folders->len; ++i) {
			folder = g_ptr_array_index(folders, i);
			fd_write_all(sock, folder, strlen(folder));
			fd_write_all(sock, "\n", 1);
		}
		fd_write_all(sock, ".\n", 2);
		for (;;) {
			if (fd_gets(sock, buf, sizeof(buf)) <= 0) break;
			if (!strncmp(buf, ".\n", 2)) break;
			fputs(buf, stdout);
		}
	} else if (cmd.open_msg) {
		gchar *str;

		str = g_strdup_printf("open %s\n", cmd.open_msg);
		fd_write_all(sock, str, strlen(str));
		g_free(str);
	} else if (cmd.exit) {
		fd_write_all(sock, "exit\n", 5);
	} else {
#ifdef G_OS_WIN32
		HWND hwnd;

		fd_write_all(sock, "popup\n", 6);
		if (fd_read(sock, (gchar *)&hwnd, sizeof(hwnd)) == sizeof(hwnd))
			SetForegroundWindow(hwnd);
#else
		fd_write_all(sock, "popup\n", 6);
#endif
	}

	fd_close(sock);
	return -1;
}

static gint lock_socket_remove(void)
{
#ifndef G_OS_WIN32
	gchar *filename;
#endif

	if (lock_socket < 0) return -1;

	if (lock_socket_tag > 0)
		g_source_remove(lock_socket_tag);
	if (lock_ch) {
		g_io_channel_shutdown(lock_ch, FALSE, NULL);
		g_io_channel_unref(lock_ch);
		lock_ch = NULL;
	}

#ifndef G_OS_WIN32
	filename = get_socket_name();
	debug_print("lock_socket_remove: removing socket: %s\n", filename);
	g_unlink(filename);
#endif

	return 0;
}

static GPtrArray *get_folder_item_list(gint sock)
{
	gchar buf[BUFFSIZE];
	FolderItem *item;
	GPtrArray *folders = NULL;

	for (;;) {
		if (fd_gets(sock, buf, sizeof(buf)) <= 0) break;
		if (!strncmp(buf, ".\n", 2)) break;
		strretchomp(buf);
		if (!folders) folders = g_ptr_array_new();
		item = folder_find_item_from_identifier(buf);
		if (item)
			g_ptr_array_add(folders, item);
		else
			g_warning("no such folder: %s\n", buf);
	}

	return folders;
}

static gboolean lock_socket_input_cb(GIOChannel *source, GIOCondition condition,
				     gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gint fd, sock;
	gchar buf[BUFFSIZE];

#if USE_THREADS
	gdk_threads_enter();
#endif

	fd = g_io_channel_unix_get_fd(source);
	sock = fd_accept(fd);
	if (fd_gets(sock, buf, sizeof(buf)) <= 0) {
		fd_close(sock);
#if USE_THREADS
		gdk_threads_leave();
#endif
		return TRUE;
	}

	if (!strncmp(buf, "popup", 5)) {
#ifdef G_OS_WIN32
		HWND hwnd;

		hwnd = (HWND)gdk_win32_drawable_get_handle
			(GDK_DRAWABLE(mainwin->window->window));
		fd_write(sock, (gchar *)&hwnd, sizeof(hwnd));
		if (mainwin->window_hidden)
			main_window_popup(mainwin);
#else
		main_window_popup(mainwin);
#endif
	} else if (!strncmp(buf, "receive_all", 11)) {
		main_window_popup(mainwin);
		if (!gtkut_window_modal_exist())
			inc_all_account_mail(mainwin, FALSE);
	} else if (!strncmp(buf, "receive", 7)) {
		main_window_popup(mainwin);
		if (!gtkut_window_modal_exist())
			inc_mail(mainwin);
	} else if (!strncmp(buf, "compose_attach", 14)) {
		GPtrArray *files;
		gchar *mailto;

		mailto = g_strdup(buf + strlen("compose_attach") + 1);
		files = g_ptr_array_new();
		while (fd_gets(sock, buf, sizeof(buf)) > 0) {
			if (buf[0] == '.' && buf[1] == '\n') break;
			strretchomp(buf);
			g_ptr_array_add(files, g_strdup(buf));
		}
		open_compose_new(mailto, files);
		ptr_array_free_strings(files);
		g_ptr_array_free(files, TRUE);
		g_free(mailto);
	} else if (!strncmp(buf, "compose", 7)) {
		open_compose_new(buf + strlen("compose") + 1, NULL);
	} else if (!strncmp(buf, "send", 4)) {
		send_queue();
	} else if (!strncmp(buf, "status-full", 11) ||
		   !strncmp(buf, "status", 6)) {
		gchar *status;
		GPtrArray *folders;

		folders = get_folder_item_list(sock);
		status = folder_get_status
			(folders, !strncmp(buf, "status-full", 11));
		fd_write_all(sock, status, strlen(status));
		fd_write_all(sock, ".\n", 2);
		g_free(status);
		if (folders) g_ptr_array_free(folders, TRUE);
	} else if (!strncmp(buf, "open", 4)) {
		strretchomp(buf);
		if (strlen(buf) < 6 || buf[4] != ' ') {
			fd_close(sock);
#if USE_THREADS
			gdk_threads_leave();
#endif
			return TRUE;
		}
		open_message(buf + 5);
	} else if (!strncmp(buf, "exit", 4)) {
		fd_close(sock);
		app_will_exit(TRUE);
	}

	fd_close(sock);

#if USE_THREADS
	gdk_threads_leave();
#endif

	return TRUE;
}

static void remote_command_exec(void)
{
	MainWindow *mainwin;

	mainwin = main_window_get();

	if (prefs_common.open_inbox_on_startup) {
		FolderItem *item;
		PrefsAccount *ac;

		ac = account_get_default();
		if (!ac)
			ac = cur_account;
		item = ac && ac->inbox
			? folder_find_item_from_identifier(ac->inbox)
			: folder_get_default_inbox();
		folderview_select(mainwin->folderview, item);
	}

	if (!gtkut_window_modal_exist()) {
		if (cmd.compose)
			open_compose_new(cmd.compose_mailto, cmd.attach_files);

		if (cmd.open_msg)
			open_message(cmd.open_msg);

		if (cmd.receive_all)
			inc_all_account_mail(mainwin, FALSE);
		else if (prefs_common.chk_on_startup)
			inc_all_account_mail(mainwin, TRUE);
		else if (cmd.receive)
			inc_mail(mainwin);

		if (cmd.send)
			send_queue();
	}

	if (cmd.attach_files) {
		ptr_array_free_strings(cmd.attach_files);
		g_ptr_array_free(cmd.attach_files, TRUE);
		cmd.attach_files = NULL;
	}
	if (cmd.status_folders) {
		g_ptr_array_free(cmd.status_folders, TRUE);
		cmd.status_folders = NULL;
	}
	if (cmd.status_full_folders) {
		g_ptr_array_free(cmd.status_full_folders, TRUE);
		cmd.status_full_folders = NULL;
	}
	if (cmd.open_msg) {
		g_free(cmd.open_msg);
		cmd.open_msg = NULL;
	}
	if (cmd.exit) {
		app_will_exit(TRUE);
	}
}

#if !defined(G_OS_WIN32) && !defined(__APPLE__)
static void migrate_old_config(void)
{
	GDir *dir;
	const gchar *dir_name;
	GPatternSpec *pspec;

	if (alertpanel(_("Migration of configuration"),
		       _("The previous version of configuration found.\n"
			 "Do you want to migrate it?"),
		       GTK_STOCK_YES, GTK_STOCK_NO, NULL) != G_ALERTDEFAULT)
		return;

	debug_print("Migrating old configuration...\n");

#define COPY_FILE(rcfile)						\
	if (is_file_exist(OLD_RC_DIR G_DIR_SEPARATOR_S rcfile)) {	\
		conv_copy_file(OLD_RC_DIR G_DIR_SEPARATOR_S rcfile,	\
			       RC_DIR G_DIR_SEPARATOR_S rcfile,		\
			       conv_get_locale_charset_str());		\
	}

	COPY_FILE(ACCOUNT_RC);
	COPY_FILE(ACTIONS_RC);
	COPY_FILE(COMMON_RC);
	COPY_FILE(CUSTOM_HEADER_RC);
	COPY_FILE(DISPLAY_HEADER_RC);
	COPY_FILE(FILTER_HEADER_RC);
	COPY_FILE(COMMAND_HISTORY);

#undef COPY_FILE

	if (is_file_exist(OLD_RC_DIR G_DIR_SEPARATOR_S FILTER_LIST))
		copy_file(OLD_RC_DIR G_DIR_SEPARATOR_S FILTER_LIST,
			  RC_DIR G_DIR_SEPARATOR_S FILTER_LIST, FALSE);
	if (is_file_exist(OLD_RC_DIR G_DIR_SEPARATOR_S FOLDER_LIST))
		copy_file(OLD_RC_DIR G_DIR_SEPARATOR_S FOLDER_LIST,
			  RC_DIR G_DIR_SEPARATOR_S FOLDER_LIST, FALSE);
	if (is_file_exist(OLD_RC_DIR G_DIR_SEPARATOR_S "mime.types"))
		copy_file(OLD_RC_DIR G_DIR_SEPARATOR_S "mime.types",
			  RC_DIR G_DIR_SEPARATOR_S "mime.types", FALSE);

	if (is_dir_exist(OLD_RC_DIR G_DIR_SEPARATOR_S TEMPLATE_DIR))
		conv_copy_dir(OLD_RC_DIR G_DIR_SEPARATOR_S TEMPLATE_DIR,
			      RC_DIR G_DIR_SEPARATOR_S TEMPLATE_DIR,
			      conv_get_locale_charset_str());
	if (is_dir_exist(OLD_RC_DIR G_DIR_SEPARATOR_S UIDL_DIR))
		copy_dir(OLD_RC_DIR G_DIR_SEPARATOR_S UIDL_DIR,
			 RC_DIR G_DIR_SEPARATOR_S UIDL_DIR);

	if (!is_file_exist(OLD_RC_DIR G_DIR_SEPARATOR_S ADDRESSBOOK_INDEX_FILE))
		return;

	if ((dir = g_dir_open(OLD_RC_DIR, 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s\n", OLD_RC_DIR);
		return;
	}

	pspec = g_pattern_spec_new("addrbook-*.xml");

	while ((dir_name = g_dir_read_name(dir)) != NULL) {
		if (g_pattern_match_string(pspec, dir_name)) {
			gchar *old_file;
			gchar *new_file;

			old_file = g_strconcat(OLD_RC_DIR G_DIR_SEPARATOR_S,
					       dir_name, NULL);
			new_file = g_strconcat(RC_DIR G_DIR_SEPARATOR_S,
					       dir_name, NULL);
			copy_file(old_file, new_file, FALSE);
			g_free(new_file);
			g_free(old_file);
		}
	}

	g_pattern_spec_free(pspec);
	g_dir_close(dir);
}
#endif /* !G_OS_WIN32 && !__APPLE__ */

static void open_compose_new(const gchar *address, GPtrArray *attach_files)
{
	gchar *utf8addr = NULL;
#ifdef G_OS_WIN32
	GPtrArray *utf8files = NULL;
#endif

	if (gtkut_window_modal_exist())
		return;

	if (address) {
		utf8addr = g_locale_to_utf8(address, -1, NULL, NULL, NULL);
		if (utf8addr)
			g_strstrip(utf8addr);
		debug_print("open compose: %s\n", utf8addr ? utf8addr : "");
	}

#ifdef G_OS_WIN32
	if (attach_files) {
		gint i;
		gchar *file, *utf8file;

		utf8files = g_ptr_array_new();
		for (i = 0; i < attach_files->len; i++) {
			file = g_ptr_array_index(attach_files, i);
			utf8file = g_locale_to_utf8(file, -1, NULL, NULL, NULL);
			if (utf8file)
				g_ptr_array_add(utf8files, utf8file);
		}
	}

	compose_new(NULL, NULL, utf8addr, utf8files);
	if (utf8files) {
		ptr_array_free_strings(utf8files);
		g_ptr_array_free(utf8files, TRUE);
	}
#else
	compose_new(NULL, NULL, utf8addr, attach_files);
#endif

	g_free(utf8addr);
}

static void open_message_file(const gchar *file)
{
	MsgInfo *msginfo;
	MsgFlags flags = {0};
	MessageView *msgview;

	g_return_if_fail(file != NULL);

	debug_print("open message file: %s\n", file);

	if (!is_file_exist(file) || get_file_size(file) <= 0) {
		debug_print("file not found: %s\n", file);
		return;
	}

	msginfo = procheader_parse_file(file, flags, FALSE);
	if (msginfo) {
		msginfo->file_path = g_strdup(file);
		msgview = messageview_create_with_new_window();
		messageview_show(msgview, msginfo, FALSE);
		procmsg_msginfo_free(msginfo);
	} else
		debug_print("cannot open message: %s\n", file);
}

static void open_message(const gchar *path)
{
	gchar *fid;
	gchar *msg;
	gint num;
	FolderItem *item;
	MsgInfo *msginfo;
	MessageView *msgview;
	gchar *file;

	g_return_if_fail(path != NULL);

	if (gtkut_window_modal_exist())
		return;

	debug_print("open message: %s\n", path);

	if (!strncmp(path, "file:", 5)) {
		file = g_filename_from_uri(path, NULL, NULL);
		open_message_file(file);
		g_free(file);
		return;
	} else if (g_path_is_absolute(path)) {
		open_message_file(path);
		return;
	}

	/* relative path, or folder identifier */

	fid = g_path_get_dirname(path);
	msg = g_path_get_basename(path);
	num = to_number(msg);
	item = folder_find_item_from_identifier(fid);

	if (num > 0 && item) {
		debug_print("open folder id: %s (msg %d)\n", fid, num);
		msginfo = folder_item_get_msginfo(item, num);
		if (msginfo) {
			msgview = messageview_create_with_new_window();
			messageview_show(msgview, msginfo, FALSE);
			procmsg_msginfo_free(msginfo);
			g_free(msg);
			g_free(fid);
			return;
		} else
			debug_print("message %d not found\n", num);
	}

	g_free(msg);
	g_free(fid);

	/* relative path */

	file = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S, path, NULL);
	open_message_file(file);
	g_free(file);
}

static void send_queue(void)
{
	GList *list;

	if (gtkut_window_modal_exist())
		return;
	if (!main_window_toggle_online_if_offline(main_window_get()))
		return;

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (folder->queue) {
			gint ret;

			ret = send_message_queue_all(folder->queue,
						     prefs_common.savemsg,
						     prefs_common.filter_sent);
			statusbar_pop_all();
			if (ret > 0)
				folder_item_scan(folder->queue);
		}
	}

	folderview_update_all_updated(TRUE);
	main_window_set_menu_sensitive(main_window_get());
	main_window_set_toolbar_sensitive(main_window_get());
}
