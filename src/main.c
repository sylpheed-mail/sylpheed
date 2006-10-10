/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto
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
#include "filter.h"
#include "send_message.h"
#include "inc.h"
#include "import.h"
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
#include "utils.h"
#include "gtkutils.h"
#include "socket.h"
#include "stock_pixmap.h"
#include "trayicon.h"

#if USE_GPGME
#  include "rfc2015.h"
#endif
#if USE_SSL
#  include "ssl.h"
#endif

#ifdef G_OS_WIN32
#  include <windows.h>
#  include <fcntl.h>
#endif

#include "version.h"

gchar *prog_version;

static gint lock_socket = -1;
static gint lock_socket_tag = 0;
static GIOChannel *lock_ch = NULL;

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
	gboolean configdir;
	gboolean exit;
} cmd;

static void parse_cmd_opt		(int		 argc,
					 char		*argv[]);

static void app_init			(void);
static void parse_gtkrc_files		(void);
static void setup_rc_dir		(void);
static void check_gpg			(void);
static void set_log_handlers		(gboolean	 enable);

static gchar *get_socket_name		(void);
static gint prohibit_duplicate_launch	(void);
static gint lock_socket_remove		(void);
static gboolean lock_socket_input_cb	(GIOChannel	*source,
					 GIOCondition	 condition,
					 gpointer	 data);

static void remote_command_exec		(void);
static void migrate_old_config		(void);

static void open_compose_new		(const gchar	*address,
					 GPtrArray	*attach_files);

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


int main(int argc, char *argv[])
{
	MainWindow *mainwin;
	FolderView *folderview;
	GdkPixbuf *icon;

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

	gtk_set_locale();
	gtk_init(&argc, &argv);

	gdk_rgb_init();
	gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());

#if USE_THREADS || USE_LDAP
	if (!g_thread_supported())
		g_thread_init(NULL);
	if (!g_thread_supported())
		g_error(_("g_thread is not supported by glib.\n"));
#endif

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

	CHDIR_EXIT_IF_FAIL(get_home_dir(), 1);

	prefs_common_read_config();
	filter_read_config();
	prefs_actions_read_config();
	prefs_display_header_read_config();

	gtkut_stock_button_set_set_reverse(!prefs_common.comply_gnome_hig);

	check_gpg();

	sock_set_io_timeout(prefs_common.io_timeout_secs);

	gtkut_widget_init();

#ifdef G_OS_WIN32
	stock_pixbuf_gdk(NULL, STOCK_PIXMAP_SYLPHEED_SMALL, &icon);
#else
	stock_pixbuf_gdk(NULL, STOCK_PIXMAP_SYLPHEED, &icon);
#endif
	gtk_window_set_default_icon(icon);

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
		setup(mainwin);
		folder_write_list();
	}
	if (!account_get_list()) {
		account_edit_open();
		account_add();
	}

	account_set_missing_folder();
	folder_set_missing_folders();
	folderview_set(folderview);

	addressbook_read_file();

	inc_autocheck_timer_init(mainwin);

	remote_command_exec();

	gtk_main();

	return 0;
}

static void init_console(void)
{
#ifdef G_OS_WIN32
	gint fd;
	FILE *fp;
	static gboolean init_console_done = FALSE;

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
				if (*p != G_DIR_SEPARATOR)
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
		} else if (!strncmp(argv[i], "--exit", 6)) {
			cmd.exit = TRUE;
		} else if (!strncmp(argv[i], "--help", 6)) {
			init_console();

			g_print(_("Usage: %s [OPTION]...\n"),
				g_basename(argv[0]));

			g_print("%s\n", _("  --compose [address]    open composition window"));
			g_print("%s\n", _("  --attach file1 [file2]...\n"
				"                         open composition window with specified files\n"
				"                         attached"));
			g_print("%s\n", _("  --receive              receive new messages"));
			g_print("%s\n", _("  --receive-all          receive new messages of all accounts"));
			g_print("%s\n", _("  --send                 send all queued messages"));
			g_print("%s\n", _("  --status [folder]...   show the total number of messages"));
			g_print("%s\n", _("  --status-full [folder]...\n"
				"                         show the status of each folder"));
			g_print("%s\n", _("  --configdir dirname    specify directory which stores configuration files"));
			g_print("%s\n", _("  --exit                 exit Sylpheed"));
			g_print("%s\n", _("  --debug                debug mode"));
			g_print("%s\n", _("  --help                 display this help and exit"));
			g_print("%s\n", _("  --version              output version information and exit"));

#ifdef G_OS_WIN32
			g_print("\n");
			g_print(_("Press any key..."));
			_getch();
#endif

			cleanup_console();
			exit(1);
		}
	}

	if (cmd.attach_files && cmd.compose == FALSE) {
		cmd.compose = TRUE;
		cmd.compose_mailto = NULL;
	}
}

static gint get_queued_message_num(void)
{
	FolderItem *queue;

	queue = folder_get_default_queue();
	if (!queue) return -1;

	folder_item_scan(queue);
	return queue->total;
}

static void app_init(void)
{
#ifdef G_OS_WIN32
	gchar *newpath;
	const gchar *lang_env;

	/* disable locale variable such as "LANG=1041" */

#define DISABLE_DIGIT_LOCALE(envstr)			\
{							\
	lang_env = g_getenv(envstr);			\
	if (lang_env && g_ascii_isdigit(lang_env[0]))	\
		g_unsetenv(envstr);			\
}

	DISABLE_DIGIT_LOCALE("LC_ALL");
	DISABLE_DIGIT_LOCALE("LANG");
	DISABLE_DIGIT_LOCALE("LC_CTYPE");
	DISABLE_DIGIT_LOCALE("LC_MESSAGES");

#undef DISABLE_DIGIT_LOCALE
#endif

	setlocale(LC_ALL, "");

	prog_version = PROG_VERSION;
	set_startup_dir();

#ifdef G_OS_WIN32
	/* include startup directory into %PATH% for GSpawn */
	newpath = g_strconcat(get_startup_dir(), ";", g_getenv("PATH"), NULL);
	g_setenv("PATH", newpath, TRUE);
	g_free(newpath);
#endif

	if (g_path_is_absolute(LOCALEDIR))
		bindtextdomain(PACKAGE, LOCALEDIR);
	else {
		gchar *locale_dir;

		locale_dir = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S,
					 LOCALEDIR, NULL);
#ifdef G_OS_WIN32
		{
			gchar *locale_dir_;

			locale_dir_ = g_locale_from_utf8(locale_dir, -1,
							 NULL, NULL, NULL);
			if (locale_dir_) {
				g_free(locale_dir);
				locale_dir = locale_dir_;
			}
		}
#endif
		bindtextdomain(PACKAGE, locale_dir);
		g_free(locale_dir);
	}

	bind_textdomain_codeset(PACKAGE, CS_UTF_8);
	textdomain(PACKAGE);

	sock_init();
#if USE_SSL
	ssl_init();
#endif

#ifdef G_OS_UNIX
	/* ignore SIGPIPE signal for preventing sudden death of program */
	signal(SIGPIPE, SIG_IGN);
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
#ifndef G_OS_WIN32
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
#endif /* !G_OS_WIN32 */

	if (!is_dir_exist(get_rc_dir())) {
		if (make_dir_hier(get_rc_dir()) < 0)
			exit(1);
	}

	MAKE_DIR_IF_NOT_EXIST(get_mail_base_dir());

	CHDIR_EXIT_IF_FAIL(get_rc_dir(), 1);

	MAKE_DIR_IF_NOT_EXIST(get_imap_cache_dir());
	MAKE_DIR_IF_NOT_EXIST(get_news_cache_dir());
	MAKE_DIR_IF_NOT_EXIST(get_mime_tmp_dir());
	MAKE_DIR_IF_NOT_EXIST(get_tmp_dir());
	MAKE_DIR_IF_NOT_EXIST(UIDL_DIR);

	/* remove temporary files */
	remove_all_files(get_tmp_dir());
	remove_all_files(get_mime_tmp_dir());
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

	trayicon_destroy(mainwin->tray_icon);

	/* save all state before exiting */
	folder_write_list();
	summary_write_cache(mainwin->summaryview);

	main_window_get_size(mainwin);
	main_window_get_position(mainwin);
	prefs_common_write_config();
	filter_write_config();
	account_write_config_all();
	addressbook_export_to_file();

	filename = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MENU_RC, NULL);
	gtk_accel_map_save(filename);
	g_free(filename);

	/* remove temporary files */
	remove_all_files(get_tmp_dir());
	remove_all_files(get_mime_tmp_dir());

	set_log_handlers(FALSE);
	close_log_file();
	lock_socket_remove();

#if USE_SSL
	ssl_done();
#endif

	cleanup_console();
	sock_cleanup();

	if (gtk_main_level() > 0)
		gtk_main_quit();
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
	if (gpgme_check_version("0.4.5") &&
	    !gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP)) {
		/* Also does some gpgme init */
	        gpgme_engine_info_t engineInfo;

		gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
		gpgme_set_locale(NULL, LC_MESSAGES,
				 setlocale(LC_MESSAGES, NULL));

		if (!gpgme_get_engine_info(&engineInfo)) {
			while (engineInfo) {
				debug_print("GpgME Protocol: %s\n      Version: %s\n",
					    gpgme_get_protocol_name
						(engineInfo->protocol),
					    engineInfo->version ?
					    engineInfo->version : "(unknown)");
				engineInfo = engineInfo->next;
			}
		}

		procmsg_set_decrypt_message_func
			(rfc2015_open_message_decrypted);
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

static gchar *get_socket_name(void)
{
	static gchar *filename = NULL;

	if (filename == NULL) {
		filename = g_strdup_printf("%s%csylpheed-%d",
					   g_get_tmp_dir(), G_DIR_SEPARATOR,
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

	hmutex = CreateMutexA(NULL, FALSE, "Sylpheed");
	if (!hmutex) {
		g_warning("cannot create Mutex\n");
		return -1;
	}
	if (GetLastError() != ERROR_ALREADY_EXISTS) {
		sock = fd_open_inet(REMOTE_CMD_PORT);
		if (sock < 0)
			return 0;
		return sock;
	}

	sock = fd_connect_inet(REMOTE_CMD_PORT);
	if (sock < 0)
		return -1;
#else
	gchar *path;

	path = get_socket_name();
	sock = fd_connect_unix(path);
	if (sock < 0) {
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
			fd_gets(sock, buf, sizeof(buf));
			if (!strncmp(buf, ".\n", 2)) break;
			fputs(buf, stdout);
		}
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
		fd_gets(sock, buf, sizeof(buf));
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

	fd = g_io_channel_unix_get_fd(source);
	sock = fd_accept(fd);
	fd_gets(sock, buf, sizeof(buf));

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
		inc_all_account_mail(mainwin, FALSE);
	} else if (!strncmp(buf, "receive", 7)) {
		main_window_popup(mainwin);
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
	} else if (!strncmp(buf, "exit", 4)) {
		app_will_exit(TRUE);
	}

	fd_close(sock);

	return TRUE;
}

static void remote_command_exec(void)
{
	MainWindow *mainwin;

	mainwin = main_window_get();

	if (cmd.receive_all)
		inc_all_account_mail(mainwin, FALSE);
	else if (prefs_common.chk_on_startup)
		inc_all_account_mail(mainwin, TRUE);
	else if (cmd.receive)
		inc_mail(mainwin);

	if (cmd.compose)
		open_compose_new(cmd.compose_mailto, cmd.attach_files);
	if (cmd.attach_files) {
		ptr_array_free_strings(cmd.attach_files);
		g_ptr_array_free(cmd.attach_files, TRUE);
		cmd.attach_files = NULL;
	}
	if (cmd.send)
		send_queue();
	if (cmd.status_folders) {
		g_ptr_array_free(cmd.status_folders, TRUE);
		cmd.status_folders = NULL;
	}
	if (cmd.status_full_folders) {
		g_ptr_array_free(cmd.status_full_folders, TRUE);
		cmd.status_full_folders = NULL;
	}
	if (cmd.exit) {
		app_will_exit(TRUE);
		exit(0);
	}
}

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

static void open_compose_new(const gchar *address, GPtrArray *attach_files)
{
	gchar *utf8addr = NULL;
#ifdef G_OS_WIN32
	GPtrArray *utf8files = NULL;
#endif

	if (address) {
		utf8addr = g_locale_to_utf8(address, -1, NULL, NULL, NULL);
		if (utf8addr)
			g_strstrip(utf8addr);
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

static void send_queue(void)
{
	GList *list;

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
}
