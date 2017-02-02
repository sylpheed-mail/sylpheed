/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkversion.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtktoolbutton.h>
#include <gtk/gtkseparatortoolitem.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <string.h>

#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "foldersel.h"
#include "summaryview.h"
#include "quick_search.h"
#include "query_search.h"
#include "messageview.h"
#include "mimeview.h"
#include "message_search.h"
#include "headerview.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "folder.h"
#include "inc.h"
#include "rpop3.h"
#include "compose.h"
#include "procmsg.h"
#include "send_message.h"
#include "import.h"
#include "export.h"
#include "prefs_common.h"
#include "prefs_common_dialog.h"
#include "prefs_filter.h"
#include "prefs_actions.h"
#include "prefs_account.h"
#include "prefs_summary_column.h"
#include "prefs_template.h"
#include "prefs_search_folder.h"
#include "prefs_toolbar.h"
#include "plugin_manager.h"
#include "action.h"
#include "account.h"
#include "account_dialog.h"
#include "addressbook.h"
#include "logwindow.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "statusbar.h"
#include "inputdialog.h"
#include "trayicon.h"
#include "printing.h"
#include "utils.h"
#include "gtkutils.h"
#include "codeconv.h"
#include "about.h"
#include "manual.h"
#include "update_check.h"
#include "setup.h"
#include "plugin.h"
#include "version.h"

#define AC_LABEL_WIDTH	240

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

/* list of all instantiated MainWindow */
static GList *mainwin_list = NULL;

static GdkCursor *watch_cursor;

static void main_window_menu_callback_block	(MainWindow	*mainwin);
static void main_window_menu_callback_unblock	(MainWindow	*mainwin);

static void main_window_show_cur_account	(MainWindow	*mainwin);

static void main_window_set_toolbar_button_visibility
						(MainWindow	*mainwin);

static void main_window_set_widgets		(MainWindow	*mainwin,
						 LayoutType	 layout,
						 SeparateType	 type);
static GtkWidget *main_window_toolbar_create	(MainWindow	*mainwin);
static GtkWidget *main_window_toolbar_create_from_list
						(MainWindow	*mainwin,
						 GList		*item_list);
static void main_window_toolbar_toggle_menu_set_active
						(MainWindow	*mainwin,
						 ToolbarStyle	 style);

/* callback functions */
static void toolbar_inc_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_inc_all_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_rpop3_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_send_cb		(GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_compose_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_reply_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_reply_to_all_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_forward_cb		(GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_delete_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_junk_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_notjunk_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_exec_cb		(GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_next_unread_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_prev_unread_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_address_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_search_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_print_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_stop_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_prefs_common_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_prefs_account_cb	(GtkWidget	*widget,
					 gpointer	 data);

static gboolean toolbar_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 gpointer	 data);

static void online_switch_clicked		(GtkWidget	*widget,
						 gpointer	 data);
static gboolean ac_label_button_pressed		(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
static void ac_menu_popup_closed		(GtkMenuShell	*menu_shell,
						 gpointer	 data);

static gboolean main_window_key_pressed		(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);

static gint main_window_close_cb		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);
static gint folder_window_close_cb		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);
static gint message_window_close_cb		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);

static void main_window_size_allocate_cb	(GtkWidget	*widget,
						 GtkAllocation	*allocation,
						 gpointer	 data);
static void folder_window_size_allocate_cb	(GtkWidget	*widget,
						 GtkAllocation	*allocation,
						 gpointer	 data);
static void message_window_size_allocate_cb	(GtkWidget	*widget,
						 GtkAllocation	*allocation,
						 gpointer	 data);

static gboolean main_window_window_state_cb	(GtkWidget		*widget,
						 GdkEventWindowState	*event,
						 gpointer		 data);
static gboolean main_window_visibility_notify_cb(GtkWidget		*widget,
						 GdkEventVisibility	*event,
						 gpointer		 data);

static void new_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void rename_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void move_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void delete_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void update_folderview_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void add_mailbox_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void remove_mailbox_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void rebuild_tree_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void import_mail_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void export_mail_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void empty_trash_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void save_as_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
#if GTK_CHECK_VERSION(2, 10, 0)
static void page_setup_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
#endif
static void print_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toggle_offline_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void app_exit_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void search_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void toggle_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toggle_message_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toggle_toolbar_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toggle_searchbar_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toggle_statusbar_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toolbar_customize_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void change_layout_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void separate_widget_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void addressbook_open_cb	(MainWindow	*mainwin,
				 guint		 action,
				 GtkWidget	*widget);
static void log_window_show_cb	(MainWindow	*mainwin,
				 guint		 action,
				 GtkWidget	*widget);

static void inc_mail_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void inc_all_account_mail_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void inc_stop_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void rpop3_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void send_queue_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void reply_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void open_msg_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void view_source_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void show_all_header_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void move_to_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void copy_to_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void delete_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void mark_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void unmark_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void mark_as_unread_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void mark_as_read_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void mark_thread_as_read_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void mark_all_read_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void junk_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void reedit_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void add_address_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void set_charset_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void thread_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void expand_threads_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void collapse_threads_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void set_display_item_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void sort_summary_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void sort_summary_type_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void attract_by_subject_cb(MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void delete_duplicated_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void concat_partial_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void filter_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void filter_junk_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void execute_summary_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void update_summary_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void open_config_folder_cb(MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void open_attachments_folder_cb
				 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void prev_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void next_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void prev_unread_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void next_unread_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prev_new_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void next_new_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prev_marked_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void next_marked_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prev_labeled_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void next_labeled_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void goto_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void copy_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void allsel_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void select_thread_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void create_filter_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void prefs_common_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void prefs_filter_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void prefs_template_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void plugin_manager_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
#ifndef G_OS_WIN32
static void prefs_actions_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
#endif
static void prefs_account_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void new_account_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void account_selector_menu_cb	 (GtkMenuItem	*menuitem,
					  gpointer	 data);
static void account_receive_menu_cb	 (GtkMenuItem	*menuitem,
					  gpointer	 data);

static void manual_open_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void faq_open_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void help_cmdline_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
#if USE_UPDATE_CHECK
static void update_check_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
#ifdef USE_UPDATE_CHECK_PLUGIN
static void update_check_plugin_cb(MainWindow *mainwin, guint action,
				   GtkWidget *widget);
#endif
#endif

static void scan_tree_func	 (Folder	*folder,
				  FolderItem	*item,
				  gpointer	 data);

static GtkItemFactoryEntry mainwin_entries[] =
{
	{N_("/_File"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Folder"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Folder/Create _new folder..."),
						NULL, new_folder_cb, 0, NULL},
	{N_("/_File/_Folder/_Rename folder..."),NULL, rename_folder_cb, 0, NULL},
	{N_("/_File/_Folder/_Move folder..."),  NULL, move_folder_cb, 0, NULL},
	{N_("/_File/_Folder/_Delete folder"),	NULL, delete_folder_cb, 0, NULL},
	{N_("/_File/_Mailbox"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Mailbox/Add _mailbox..."),	NULL, add_mailbox_cb, 0, NULL},
	{N_("/_File/_Mailbox/_Remove mailbox"),	NULL, remove_mailbox_cb, 0, NULL},
	{N_("/_File/_Mailbox/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Mailbox/_Check for new messages"),
						NULL, update_folderview_cb, 0, NULL},
	{N_("/_File/_Mailbox/Check for new messages in _all mailboxes"),
						NULL, update_folderview_cb, 1, NULL},
	{N_("/_File/_Mailbox/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Mailbox/R_ebuild folder tree"),
						NULL, rebuild_tree_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Import mail data..."),	NULL, import_mail_cb, 0, NULL},
	{N_("/_File/_Export mail data..."),	NULL, export_mail_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/Empty all _trash"),		NULL, empty_trash_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Save as..."),		"<control>S", save_as_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
#if GTK_CHECK_VERSION(2, 10, 0)
	{N_("/_File/Page set_up..."),		NULL, page_setup_cb, 0, NULL},
#endif
	{N_("/_File/_Print..."),		NULL, print_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Work offline"),		NULL, toggle_offline_cb, 0, "<ToggleItem>"},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	/* {N_("/_File/_Close"),		"<alt>W", app_exit_cb, 0, NULL}, */
	{N_("/_File/E_xit"),			"<control>Q", app_exit_cb, 0, NULL},

	{N_("/_Edit"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/_Copy"),			"<control>C", copy_cb, 0, NULL},
	{N_("/_Edit/Select _all"),		"<control>A", allsel_cb, 0, NULL},
	{N_("/_Edit/Select _thread"),		NULL, select_thread_cb, 0, NULL},
	{N_("/_Edit/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Find in current message..."),
						"<control>F", search_cb, 0, NULL},
	{N_("/_Edit/_Search messages..."),	"<shift><control>F", search_cb, 1, NULL},
	{N_("/_Edit/_Quick search"),		"<shift><control>S", search_cb, 2, NULL},

	{N_("/_View"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Show or hi_de"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Show or hi_de/_Folder tree"),
						NULL, toggle_folder_cb, 0, "<ToggleItem>"},
	{N_("/_View/Show or hi_de/_Message view"),
						"V", toggle_message_cb, 0, "<ToggleItem>"},
	{N_("/_View/Show or hi_de/_Toolbar"),
						NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Show or hi_de/_Toolbar/Icon _and text"),
						NULL, toggle_toolbar_cb, TOOLBAR_BOTH, "<RadioItem>"},
	{N_("/_View/Show or hi_de/_Toolbar/Text at the _right of icon"),
						NULL, toggle_toolbar_cb, TOOLBAR_BOTH_HORIZ, "/View/Show or hide/Toolbar/Icon and text"},
	{N_("/_View/Show or hi_de/_Toolbar/_Icon"),
						NULL, toggle_toolbar_cb, TOOLBAR_ICON, "/View/Show or hide/Toolbar/Icon and text"},
	{N_("/_View/Show or hi_de/_Toolbar/_Text"),
						NULL, toggle_toolbar_cb, TOOLBAR_TEXT, "/View/Show or hide/Toolbar/Icon and text"},
	{N_("/_View/Show or hi_de/_Toolbar/_None"),
						NULL, toggle_toolbar_cb, TOOLBAR_NONE, "/View/Show or hide/Toolbar/Icon and text"},
	{N_("/_View/Show or hi_de/_Search bar"),
						NULL, toggle_searchbar_cb, 0, "<ToggleItem>"},
	{N_("/_View/Show or hi_de/Status _bar"),
						NULL, toggle_statusbar_cb, 0, "<ToggleItem>"},
	{N_("/_View/_Customize toolbar..."),	NULL, toolbar_customize_cb, 0, NULL},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Layou_t"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Layou_t/_Normal"),		NULL, change_layout_cb, LAYOUT_NORMAL, "<RadioItem>"},
	{N_("/_View/Layou_t/_Vertical"),	NULL, change_layout_cb, LAYOUT_VERTICAL, "/View/Layout/Normal"},
	{N_("/_View/Separate f_older tree"),	NULL, separate_widget_cb, SEPARATE_FOLDER, "<ToggleItem>"},
	{N_("/_View/Separate _message view"),	NULL, separate_widget_cb, SEPARATE_MESSAGE, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Sort"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Sort/by _number"),		NULL, sort_summary_cb, SORT_BY_NUMBER, "<RadioItem>"},
	{N_("/_View/_Sort/by s_ize"),		NULL, sort_summary_cb, SORT_BY_SIZE, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _date"),		NULL, sort_summary_cb, SORT_BY_DATE, "/View/Sort/by number"},
	{N_("/_View/_Sort/by t_hread date"),	NULL, sort_summary_cb, SORT_BY_TDATE, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _from"),		NULL, sort_summary_cb, SORT_BY_FROM, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _recipient"),	NULL, sort_summary_cb, SORT_BY_TO, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _subject"),	NULL, sort_summary_cb, SORT_BY_SUBJECT, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _color label"),
						NULL, sort_summary_cb, SORT_BY_LABEL, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _mark"),		NULL, sort_summary_cb, SORT_BY_MARK, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _unread"),		NULL, sort_summary_cb, SORT_BY_UNREAD, "/View/Sort/by number"},
	{N_("/_View/_Sort/by a_ttachment"),
						NULL, sort_summary_cb, SORT_BY_MIME, "/View/Sort/by number"},
	{N_("/_View/_Sort/D_on't sort"),	NULL, sort_summary_cb, SORT_BY_NONE, "/View/Sort/by number"},
	{N_("/_View/_Sort/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Sort/Ascending"),		NULL, sort_summary_type_cb, SORT_ASCENDING, "<RadioItem>"},
	{N_("/_View/_Sort/Descending"),		NULL, sort_summary_type_cb, SORT_DESCENDING, "/View/Sort/Ascending"},
	{N_("/_View/_Sort/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Sort/_Attract by subject"),
						NULL, attract_by_subject_cb, 0, NULL},
	{N_("/_View/Th_read view"),		"<control>T", thread_cb, 0, "<ToggleItem>"},
	{N_("/_View/E_xpand all threads"),	NULL, expand_threads_cb, 0, NULL},
	{N_("/_View/Co_llapse all threads"),	NULL, collapse_threads_cb, 0, NULL},
	{N_("/_View/Set display _item..."),	NULL, set_display_item_cb, 0, NULL},

	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Go to/_Prev message"),	"P", prev_cb, 0, NULL},
	{N_("/_View/_Go to/_Next message"),	"N", next_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/P_rev unread message"),
						"<shift>P", prev_unread_cb, 0, NULL},
	{N_("/_View/_Go to/N_ext unread message"),
						"<shift>N", next_unread_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Prev ne_w message"),	NULL, prev_new_cb, 0, NULL},
	{N_("/_View/_Go to/Ne_xt new message"),	NULL, next_new_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Prev _marked message"),
						NULL, prev_marked_cb, 0, NULL},
	{N_("/_View/_Go to/Next m_arked message"),
						NULL, next_marked_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Prev _labeled message"),
						NULL, prev_labeled_cb, 0, NULL},
	{N_("/_View/_Go to/Next la_beled message"),
						NULL, next_labeled_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Other _folder..."),	"G", goto_folder_cb, 0, NULL},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},

#define ENC_SEPARATOR \
	{N_("/_View/Character _encoding/---"),		NULL, NULL, 0, "<Separator>"}
#define ENC_ACTION(action) \
	 NULL, set_charset_cb, action, "/View/Character encoding/Auto detect"

	{N_("/_View/Character _encoding"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Character _encoding/_Auto detect"),
	 NULL, set_charset_cb, C_AUTO, "<RadioItem>"},
	{N_("/_View/Character _encoding/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Character _encoding/7bit ascii (US-ASC_II)"),
	 ENC_ACTION(C_US_ASCII)},
	{N_("/_View/Character _encoding/Unicode (_UTF-8)"),
	 ENC_ACTION(C_UTF_8)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Western European (ISO-8859-_1)"),
	 ENC_ACTION(C_ISO_8859_1)},
	{N_("/_View/Character _encoding/Western European (ISO-8859-15)"),
	 ENC_ACTION(C_ISO_8859_15)},
	{N_("/_View/Character _encoding/Western European (Windows-1252)"),
	 ENC_ACTION(C_WINDOWS_1252)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Central European (ISO-8859-_2)"),
	 ENC_ACTION(C_ISO_8859_2)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/_Baltic (ISO-8859-13)"),
	 ENC_ACTION(C_ISO_8859_13)},
	{N_("/_View/Character _encoding/Baltic (ISO-8859-_4)"),
	 ENC_ACTION(C_ISO_8859_4)},
	{N_("/_View/Character _encoding/Baltic (Windows-1257)"),
	 ENC_ACTION(C_WINDOWS_1257)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Greek (ISO-8859-_7)"),
	 ENC_ACTION(C_ISO_8859_7)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Arabic (ISO-8859-_6)"),
	 ENC_ACTION(C_ISO_8859_6)},
	{N_("/_View/Character _encoding/Arabic (Windows-1256)"),
	 ENC_ACTION(C_CP1256)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Hebrew (ISO-8859-_8)"),
	 ENC_ACTION(C_ISO_8859_8)},
	{N_("/_View/Character _encoding/Hebrew (Windows-1255)"),
	 ENC_ACTION(C_CP1255)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Turkish (ISO-8859-_9)"),
	 ENC_ACTION(C_ISO_8859_9)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Cyrillic (ISO-8859-_5)"),
	 ENC_ACTION(C_ISO_8859_5)},
	{N_("/_View/Character _encoding/Cyrillic (KOI8-_R)"),
	 ENC_ACTION(C_KOI8_R)},
	{N_("/_View/Character _encoding/Cyrillic (KOI8-U)"),
	 ENC_ACTION(C_KOI8_U)},
	{N_("/_View/Character _encoding/Cyrillic (Windows-1251)"),
	 ENC_ACTION(C_CP1251)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Japanese (ISO-2022-_JP)"),
	 ENC_ACTION(C_ISO_2022_JP)},
	{N_("/_View/Character _encoding/Japanese (ISO-2022-JP-2)"),
	 ENC_ACTION(C_ISO_2022_JP_2)},
	{N_("/_View/Character _encoding/Japanese (_EUC-JP)"),
	 ENC_ACTION(C_EUC_JP)},
	{N_("/_View/Character _encoding/Japanese (_Shift__JIS)"),
	 ENC_ACTION(C_SHIFT_JIS)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Simplified Chinese (_GB2312)"),
	 ENC_ACTION(C_GB2312)},
	{N_("/_View/Character _encoding/Simplified Chinese (GBK)"),
	 ENC_ACTION(C_GBK)},
	{N_("/_View/Character _encoding/Traditional Chinese (_Big5)"),
	 ENC_ACTION(C_BIG5)},
	{N_("/_View/Character _encoding/Traditional Chinese (EUC-_TW)"),
	 ENC_ACTION(C_EUC_TW)},
	{N_("/_View/Character _encoding/Chinese (ISO-2022-_CN)"),
	 ENC_ACTION(C_ISO_2022_CN)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Korean (EUC-_KR)"),
	 ENC_ACTION(C_EUC_KR)},
	{N_("/_View/Character _encoding/Korean (ISO-2022-KR)"),
	 ENC_ACTION(C_ISO_2022_KR)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Thai (TIS-620)"),
	 ENC_ACTION(C_TIS_620)},
	{N_("/_View/Character _encoding/Thai (Windows-874)"),
	 ENC_ACTION(C_WINDOWS_874)},

#undef CODESET_SEPARATOR
#undef CODESET_ACTION

	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Open in new _window"),	"<control><alt>N", open_msg_cb, 0, NULL},
	{N_("/_View/Mess_age source"),		"<control>U", view_source_cb, 0, NULL},
	{N_("/_View/All _headers"),	"<control>H", show_all_header_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Update summary"),		"<control><alt>U", update_summary_cb,  0, NULL},

	{N_("/_Message"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Recei_ve"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Recei_ve/Get from _current account"),
						"<control>I",	inc_mail_cb, 0, NULL},
	{N_("/_Message/Recei_ve/Get from _all accounts"),
						"<shift><control>I", inc_all_account_mail_cb, 0, NULL},
	{N_("/_Message/Recei_ve/Stop receivin_g"),
						NULL, inc_stop_cb, 0, NULL},
	{N_("/_Message/Recei_ve/_Remote mailbox..."),
						NULL, rpop3_cb, 0, NULL},
	{N_("/_Message/Recei_ve/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Send queued messages"), NULL, send_queue_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Compose _new message"),	"<control>M",	compose_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Reply"),		"<control>R", 	reply_cb, COMPOSE_REPLY, NULL},
	{N_("/_Message/Repl_y to"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Repl_y to/_all"),	"<shift><control>R", reply_cb, COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/_Message/Repl_y to/_sender"),	NULL, reply_cb, COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/_Message/Repl_y to/mailing _list"),
						"<control>L", reply_cb, COMPOSE_REPLY_TO_LIST, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Forward"),		"<control><alt>F", reply_cb, COMPOSE_FORWARD, NULL},
	{N_("/_Message/For_ward as attachment"),
						"<shift><control><alt>F", reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/_Message/Redirec_t"),		NULL, reply_cb, COMPOSE_REDIRECT, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/M_ove..."),		"<control>O", move_to_cb, 0, NULL},
	{N_("/_Message/_Copy..."),		"<shift><control>O", copy_to_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/_Mark/Set _flag"),	"<shift>asterisk", mark_cb, 0, NULL},
	{N_("/_Message/_Mark/_Unset flag"),	"U", unmark_cb, 0, NULL},
	{N_("/_Message/_Mark/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark/Mark as unr_ead"),	"<shift>exclam", mark_as_unread_cb, 0, NULL},
	{N_("/_Message/_Mark/Mark as rea_d"),
						NULL, mark_as_read_cb, 0, NULL},
	{N_("/_Message/_Mark/Mark _thread as read"),
						NULL, mark_thread_as_read_cb, 0, NULL},
	{N_("/_Message/_Mark/Mark all _read"),	NULL, mark_all_read_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Delete"),		"<control>D", delete_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Set as _junk mail"),	"<control>J", junk_cb, 0, NULL},
	{N_("/_Message/Set as not j_unk mail"),	"<shift><control>J", junk_cb, 1, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Re-_edit"),		NULL, reedit_cb, 0, NULL},

	{N_("/_Tools"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Address book"),		"<shift><control>A", addressbook_open_cb, 0, NULL},
	{N_("/_Tools/Add sender to address boo_k..."),
						NULL, add_address_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Filter all messages in folder"),
						NULL, filter_cb, 0, NULL},
	{N_("/_Tools/Filter _selected messages"),
						NULL, filter_cb, 1, NULL},
	{N_("/_Tools/_Create filter rule"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Create filter rule/_Automatically"),
						NULL, create_filter_cb, FLT_BY_AUTO, NULL},
	{N_("/_Tools/_Create filter rule/by _From"),
						NULL, create_filter_cb, FLT_BY_FROM, NULL},
	{N_("/_Tools/_Create filter rule/by _To"),
						NULL, create_filter_cb, FLT_BY_TO, NULL},
	{N_("/_Tools/_Create filter rule/by _Subject"),
						NULL, create_filter_cb, FLT_BY_SUBJECT, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Filter _junk mails in folder"),
						NULL, filter_junk_cb, 0, NULL},
	{N_("/_Tools/Filter junk _mails in selected messages"),
						NULL, filter_junk_cb, 1, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
#ifndef G_OS_WIN32
	{N_("/_Tools/Actio_ns"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
#endif
	{N_("/_Tools/Delete du_plicated messages"),
						NULL, delete_duplicated_cb,   0, NULL},
	{N_("/_Tools/C_oncatenate separated messages"),
						NULL, concat_partial_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/E_xecute marked process"),	"X", execute_summary_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Op_en configuration folder"),
						NULL, open_config_folder_cb, 0, NULL},
	{N_("/_Tools/Open a_ttachments folder"),NULL, open_attachments_folder_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Log window"),		"<shift><control>L", log_window_show_cb, 0, NULL},

	{N_("/_Configuration"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Configuration/_Common preferences..."),
						NULL, prefs_common_open_cb, 0, NULL},
	{N_("/_Configuration/_Filter settings..."),
						NULL, prefs_filter_open_cb, 0, NULL},
	{N_("/_Configuration/_Template..."),	NULL, prefs_template_open_cb, 0, NULL},
#ifndef G_OS_WIN32
	{N_("/_Configuration/_Actions..."),	NULL, prefs_actions_open_cb, 0, NULL},
#endif
	{N_("/_Configuration/Plug-in _manager..."),
						NULL, plugin_manager_open_cb, 0, NULL},
	{N_("/_Configuration/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Configuration/_Preferences for current account..."),
						NULL, prefs_account_open_cb, 0, NULL},
	{N_("/_Configuration/Create _new account..."),
						NULL, new_account_cb, 0, NULL},
	{N_("/_Configuration/_Edit accounts..."),
						NULL, account_edit_open, 0, NULL},
	{N_("/_Configuration/C_hange current account"),
						NULL, NULL, 0, "<Branch>"},

	{N_("/_Help"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_Manual"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_Manual/_English"),		NULL, manual_open_cb, MANUAL_LANG_EN, NULL},
	{N_("/_Help/_Manual/_Japanese"),	NULL, manual_open_cb, MANUAL_LANG_JA, NULL},
	{N_("/_Help/_FAQ"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_FAQ/_English"),		NULL, faq_open_cb, MANUAL_LANG_EN, NULL},
	{N_("/_Help/_FAQ/_German"),		NULL, faq_open_cb, MANUAL_LANG_DE, NULL},
	{N_("/_Help/_FAQ/_Spanish"),		NULL, faq_open_cb, MANUAL_LANG_ES, NULL},
	{N_("/_Help/_FAQ/_French"),		NULL, faq_open_cb, MANUAL_LANG_FR, NULL},
	{N_("/_Help/_FAQ/_Italian"),		NULL, faq_open_cb, MANUAL_LANG_IT, NULL},
	{N_("/_Help/_Command line options"),	NULL, help_cmdline_cb, 0, NULL},
#if USE_UPDATE_CHECK
	{N_("/_Help/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Help/_Update check..."),		NULL, update_check_cb, 0, NULL},
#ifdef USE_UPDATE_CHECK_PLUGIN
	{N_("/_Help/Update check of _plug-ins..."),
						NULL, update_check_plugin_cb, 0, NULL},
#endif
#endif
	{N_("/_Help/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Help/_About"),			NULL, about_show, 0, NULL}
};

MainWindow *main_window_create(SeparateType type)
{
	MainWindow *mainwin;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *toolbar;
	GtkWidget *hbox_spc;
	GtkWidget *vbox_body;
	GtkWidget *statusbar;
	GtkWidget *progressbar;
	GtkWidget *statuslabel;
	GtkWidget *online_hbox;
	GtkWidget *online_switch;
	GtkWidget *online_pixmap;
	GtkWidget *offline_pixmap;
	GtkTooltips *online_tip;
#if !GTK_CHECK_VERSION(2, 6, 0)
	GtkWidget *spacer_hbox;
#endif
	GtkWidget *ac_button;
	GtkWidget *ac_label;

	TrayIcon *tray_icon;

	FolderView *folderview;
	SummaryView *summaryview;
	MessageView *messageview;
	GdkColormap *colormap;
	GdkColor color[3];
	gboolean success[3];
	guint n_menu_entries;
	GtkItemFactory *ifactory;
	GtkWidget *ac_menu;
	GtkWidget *menuitem;
	gint i;

	static GdkGeometry geometry;

	debug_print(_("Creating main window...\n"));
	mainwin = g_new0(MainWindow, 1);

	/* main window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), PROG_VERSION);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_window_set_wmclass(GTK_WINDOW(window), "main_window", "Sylpheed");
	gtk_widget_add_events(window, GDK_VISIBILITY_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(main_window_key_pressed), mainwin);

	if (!geometry.min_height) {
		geometry.min_width = 320;
		geometry.min_height = 200;
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(main_window_close_cb), mainwin);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	/* menu bar */
	n_menu_entries = sizeof(mainwin_entries) / sizeof(mainwin_entries[0]);
	menubar = menubar_create(window, mainwin_entries, 
				 n_menu_entries, "<Main>", mainwin);
	gtk_widget_show(menubar);
	gtk_widget_set_size_request(menubar, 300, -1);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);
	ifactory = gtk_item_factory_from_widget(menubar);

	/* toolbar */
	mainwin->toolbar_tip = gtk_tooltips_new();
	toolbar = main_window_toolbar_create(mainwin);
	gtk_widget_set_size_request(toolbar, 300, -1);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox_spc);
	gtk_widget_set_size_request(hbox_spc, -1, BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_spc, FALSE, FALSE, 0);

	/* vbox that contains body */
	vbox_body = gtk_vbox_new(FALSE, BORDER_WIDTH);
	gtk_widget_show(vbox_body);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_body), 0);
	gtk_box_pack_start(GTK_BOX(vbox), vbox_body, TRUE, TRUE, 0);

	statusbar = statusbar_create();
	gtk_box_pack_end(GTK_BOX(vbox_body), statusbar, FALSE, FALSE, 0);

	progressbar = gtk_progress_bar_new();
	gtk_widget_set_size_request(progressbar, 120, 1);
	gtk_box_pack_start(GTK_BOX(statusbar), progressbar, FALSE, FALSE, 0);

	statuslabel = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(statusbar), statuslabel, FALSE, FALSE, 0);

	online_hbox = gtk_hbox_new(FALSE, 0);

	online_pixmap = stock_pixbuf_widget(statusbar, STOCK_PIXMAP_ONLINE);
	offline_pixmap = stock_pixbuf_widget(statusbar, STOCK_PIXMAP_OFFLINE);
	gtk_box_pack_start(GTK_BOX(online_hbox), online_pixmap,
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(online_hbox), offline_pixmap,
			   FALSE, FALSE, 0);

	online_switch = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(online_switch), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS(online_switch, GTK_CAN_FOCUS);
#ifdef G_OS_WIN32
	gtk_widget_set_size_request(online_switch, 34, 20);
#else
	gtk_widget_set_size_request(online_switch, 34, 24);
#endif
	gtk_container_add(GTK_CONTAINER(online_switch), online_hbox);
	g_signal_connect(G_OBJECT(online_switch), "clicked",
			 G_CALLBACK(online_switch_clicked), mainwin);
	gtk_box_pack_start(GTK_BOX(statusbar), online_switch, FALSE, FALSE, 0);

	online_tip = gtk_tooltips_new();

#if !GTK_CHECK_VERSION(2, 6, 0)
	spacer_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(statusbar), spacer_hbox, FALSE, FALSE, 0);
#endif

	ac_button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(ac_button), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS(ac_button, GTK_CAN_FOCUS);
	gtk_widget_set_size_request(ac_button, -1, 1);
	gtk_box_pack_end(GTK_BOX(statusbar), ac_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(ac_button), "button_press_event",
			 G_CALLBACK(ac_label_button_pressed), mainwin);

	ac_label = gtk_label_new("");
	gtk_container_add(GTK_CONTAINER(ac_button), ac_label);

	gtk_widget_show_all(statusbar);

	tray_icon = trayicon_create(mainwin);
	if (tray_icon && prefs_common.show_trayicon)
		trayicon_show(tray_icon);

	/* create views */
	mainwin->folderview  = folderview  = folderview_create();
	mainwin->summaryview = summaryview = summary_create();
	mainwin->messageview = messageview = messageview_create();
	mainwin->logwin      = log_window_create();

	quick_search_create(summaryview);

	folderview->mainwin      = mainwin;
	folderview->summaryview  = summaryview;

	summaryview->mainwin     = mainwin;
	summaryview->folderview  = folderview;
	summaryview->messageview = messageview;
	summaryview->window      = window;

	messageview->statusbar   = statusbar;
	messageview->mainwin     = mainwin;

	mainwin->window         = window;
	mainwin->vbox           = vbox;
	mainwin->menubar        = menubar;
	mainwin->menu_factory   = ifactory;
	mainwin->toolbar        = toolbar;
	mainwin->vbox_body      = vbox_body;
	mainwin->statusbar      = statusbar;
	mainwin->progressbar    = progressbar;
	mainwin->statuslabel    = statuslabel;
	mainwin->online_switch  = online_switch;
	mainwin->online_pixmap  = online_pixmap;
	mainwin->offline_pixmap = offline_pixmap;
	mainwin->online_tip     = online_tip;
	mainwin->ac_button      = ac_button;
	mainwin->ac_label       = ac_label;

	mainwin->tray_icon      = tray_icon;

	/* set context IDs for status bar */
	mainwin->mainwin_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Main Window");
	mainwin->folderview_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Folder View");
	mainwin->summaryview_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Summary View");
	mainwin->messageview_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Message View");

	messageview->statusbar_cid = mainwin->messageview_cid;

	/* allocate colors for summary view and folder view */
	summaryview->color_marked.red = summaryview->color_marked.green = 0;
	summaryview->color_marked.blue = (guint16)65535;

	summaryview->color_dim.red = summaryview->color_dim.green =
		summaryview->color_dim.blue = COLOR_DIM;

	folderview->color_new.red = (guint16)55000;
	folderview->color_new.green = folderview->color_new.blue = 15000;

	folderview->color_noselect.red = folderview->color_noselect.green =
		folderview->color_noselect.blue = COLOR_DIM;

	color[0] = summaryview->color_marked;
	color[1] = summaryview->color_dim;
	color[2] = folderview->color_new;

	colormap = gdk_window_get_colormap(window->window);
	gdk_colormap_alloc_colors(colormap, color, 3, FALSE, TRUE, success);
	for (i = 0; i < 3; i++) {
		if (success[i] == FALSE)
			g_warning(_("MainWindow: color allocation %d failed\n"), i);
	}

	if (prefs_common.layout_type == LAYOUT_VERTICAL)
		messageview->visible = TRUE;
	else
		messageview->visible = prefs_common.msgview_visible;

	main_window_set_widgets(mainwin, prefs_common.layout_type, type);

	if (prefs_common.mainwin_maximized)
		gtk_window_maximize(GTK_WINDOW(window));

	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(main_window_size_allocate_cb), mainwin);
	g_signal_connect(G_OBJECT(window), "window_state_event",
			 G_CALLBACK(main_window_window_state_cb), mainwin);
	g_signal_connect(G_OBJECT(window), "visibility_notify_event",
			 G_CALLBACK(main_window_visibility_notify_cb), mainwin);

	/* set menu items */
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Character encoding/Auto detect");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);

	main_window_toolbar_toggle_menu_set_active
		(mainwin, prefs_common.toolbar_style);

	gtk_widget_hide(GTK_WIDGET_PTR(summaryview->qsearch));
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Show or hide/Search bar");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       prefs_common.show_searchbar);

	gtk_widget_hide(mainwin->statusbar);
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Show or hide/Status bar");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       prefs_common.show_statusbar);

	/* set account selection menu */
	ac_menu = gtk_item_factory_get_widget
		(ifactory, "/Configuration/Change current account");
	g_signal_connect(G_OBJECT(ac_menu), "selection_done",
			 G_CALLBACK(ac_menu_popup_closed), mainwin);
	mainwin->ac_menu = ac_menu;

	main_window_set_toolbar_sensitive(mainwin);
	main_window_set_toolbar_button_visibility(mainwin);

	/* create actions menu */
#ifndef G_OS_WIN32
	action_update_mainwin_menu(ifactory, mainwin);
#endif

	/* initialize online switch */
	if (prefs_common.startup_online_mode == 1)
		prefs_common.online_mode = TRUE;
	else if (prefs_common.startup_online_mode == 0)
		prefs_common.online_mode = FALSE;
	prefs_common.online_mode = !prefs_common.online_mode;
	online_switch_clicked(online_switch, mainwin);

	/* show main window */
	gtk_widget_show(mainwin->window);

#if !GTK_CHECK_VERSION(2, 6, 0)
	{
		gint w, h;
		gdk_drawable_get_size
			(GDK_DRAWABLE(GTK_STATUSBAR(statusbar)->grip_window),
			 &w, &h);
		gtk_widget_set_size_request(spacer_hbox, w, -1);
	}
#endif

	/* initialize views */
	folderview_init(folderview);
	summary_init(summaryview);
	messageview_init(messageview);
	log_window_init(mainwin->logwin);

	mainwin->lock_count = 0;
	mainwin->menu_lock_count = 0;
	mainwin->cursor_count = 0;

	mainwin->window_hidden = FALSE;
	mainwin->window_obscured = FALSE;

	if (!watch_cursor)
		watch_cursor = gdk_cursor_new(GDK_WATCH);

	mainwin_list = g_list_append(mainwin_list, mainwin);

	debug_print(_("done.\n"));

	return mainwin;
}

void main_window_cursor_wait(MainWindow *mainwin)
{

	if (mainwin->cursor_count == 0)
		gdk_window_set_cursor(mainwin->window->window, watch_cursor);

	mainwin->cursor_count++;

	gdk_flush();
}

void main_window_cursor_normal(MainWindow *mainwin)
{
	if (mainwin->cursor_count)
		mainwin->cursor_count--;

	if (mainwin->cursor_count == 0)
		gdk_window_set_cursor(mainwin->window->window, NULL);

	gdk_flush();
}

/* lock / unlock the user-interface */
void main_window_lock(MainWindow *mainwin)
{
	if (mainwin->lock_count == 0)
		gtk_widget_set_sensitive(mainwin->ac_button, FALSE);

	mainwin->lock_count++;

	main_window_set_menu_sensitive(mainwin);
	main_window_set_toolbar_sensitive(mainwin);
}

void main_window_unlock(MainWindow *mainwin)
{
	if (mainwin->lock_count)
		mainwin->lock_count--;

	main_window_set_menu_sensitive(mainwin);
	main_window_set_toolbar_sensitive(mainwin);

	if (mainwin->lock_count == 0)
		gtk_widget_set_sensitive(mainwin->ac_button, TRUE);
}

static void main_window_menu_callback_block(MainWindow *mainwin)
{
	mainwin->menu_lock_count++;
}

static void main_window_menu_callback_unblock(MainWindow *mainwin)
{
	if (mainwin->menu_lock_count)
		mainwin->menu_lock_count--;
}

void main_window_reflect_prefs_all(void)
{
	MainWindow *mainwin;

	mainwin = main_window_get();

	main_window_show_cur_account(mainwin);
	main_window_set_menu_sensitive(mainwin);
	main_window_set_toolbar_sensitive(mainwin);
	main_window_set_toolbar_button_visibility(mainwin);

	if (mainwin->tray_icon) {
		if (prefs_common.show_trayicon)
			trayicon_show(mainwin->tray_icon);
		else {
			/* trayicon is automatically replaced by new one */
			trayicon_hide(mainwin->tray_icon);
		}
	}

	folderview_reflect_prefs(mainwin->folderview);

	messageview_reflect_prefs(mainwin->messageview);

	headerview_set_visibility(mainwin->messageview->headerview,
				  prefs_common.display_header_pane);

	textview_reflect_prefs(mainwin->messageview->textview);
	textview_reflect_prefs(mainwin->messageview->mimeview->textview);

	summary_redisplay_msg(mainwin->summaryview);
}

void main_window_set_summary_column(void)
{
	summary_set_column_order(main_window_get()->summaryview);
}

static void main_window_set_account_selector_menu(MainWindow *mainwin,
						  GList *account_list)
{
	GList *cur_ac, *cur_item;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;

	/* destroy all previous menu item */
	cur_item = GTK_MENU_SHELL(mainwin->ac_menu)->children;
	while (cur_item != NULL) {
		GList *next = cur_item->next;
		gtk_widget_destroy(GTK_WIDGET(cur_item->data));
		cur_item = next;
	}

	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;

		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name
			 ? ac_prefs->account_name : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_append(GTK_MENU(mainwin->ac_menu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_selector_menu_cb),
				 ac_prefs);
	}
}

static void main_window_set_account_receive_menu(MainWindow *mainwin,
						 GList *account_list)
{
	GList *cur_ac, *cur_item;
	GtkWidget *menu;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;

	menu = gtk_item_factory_get_widget(mainwin->menu_factory,
					   "/Message/Receive");

	/* search for separator */
	for (cur_item = GTK_MENU_SHELL(menu)->children; cur_item != NULL;
	     cur_item = cur_item->next) {
		if (GTK_BIN(cur_item->data)->child == NULL) {
			cur_item = cur_item->next;
			break;
		}
	}

	/* destroy all previous menu item */
	while (cur_item != NULL) {
		GList *next = cur_item->next;
		gtk_widget_destroy(GTK_WIDGET(cur_item->data));
		cur_item = next;
	}

	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;

		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name ? ac_prefs->account_name
			 : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_append(GTK_MENU(menu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_receive_menu_cb),
				 ac_prefs);
	}
}

void main_window_set_account_menu(GList *account_list)
{
	MainWindow *mainwin;

	mainwin = main_window_get();
	main_window_set_account_selector_menu(mainwin, account_list);
	main_window_set_account_receive_menu(mainwin, account_list);
}

static void main_window_show_cur_account(MainWindow *mainwin)
{
	gchar *buf;
	gchar *ac_name;

	ac_name = g_strdup(cur_account
			   ? (cur_account->account_name
			      ? cur_account->account_name : _("Untitled"))
			   : _("none"));

	if (cur_account)
		buf = g_strdup_printf("%s - %s", ac_name, PROG_VERSION);
	else
		buf = g_strdup(PROG_VERSION);
	gtk_window_set_title(GTK_WINDOW(mainwin->window), buf);
	g_free(buf);

	gtk_label_set_text(GTK_LABEL(mainwin->ac_label), ac_name);
	gtk_widget_queue_resize(mainwin->ac_button);

	g_free(ac_name);
}

void main_window_change_cur_account(void)
{
	MainWindow *mainwin;

	mainwin = main_window_get();
	main_window_show_cur_account(mainwin);
	main_window_set_menu_sensitive(mainwin);
	main_window_set_toolbar_sensitive(mainwin);
}

MainWindow *main_window_get(void)
{
	if (mainwin_list)
		return (MainWindow *)mainwin_list->data;
	else
		return NULL;
}

GtkWidget *main_window_get_folder_window(MainWindow *mainwin)
{
	switch (mainwin->type) {
	case SEPARATE_FOLDER:
		return mainwin->win.sep_folder.folderwin;
	case SEPARATE_BOTH:
		return mainwin->win.sep_both.folderwin;
	default:
		return NULL;
	}
}

GtkWidget *main_window_get_message_window(MainWindow *mainwin)
{
	switch (mainwin->type) {
	case SEPARATE_MESSAGE:
		return mainwin->win.sep_message.messagewin;
	case SEPARATE_BOTH:
		return mainwin->win.sep_both.messagewin;
	default:
		return NULL;
	}
}

void main_window_hide(MainWindow *mainwin)
{
	GtkWidget *folder_wid  = GTK_WIDGET_PTR(mainwin->folderview);
	GtkWidget *summary_wid = GTK_WIDGET_PTR(mainwin->summaryview);
	GtkWidget *message_wid = GTK_WIDGET_PTR(mainwin->messageview);
	GtkWidget *qsearch_wid = GTK_WIDGET_PTR(mainwin->summaryview->qsearch);
	GtkWidget *vbox_summary = qsearch_wid->parent;

	/* remove widgets from those containers */
	gtkut_container_remove
		(GTK_CONTAINER(folder_wid->parent), folder_wid);
	gtkut_container_remove
		(GTK_CONTAINER(summary_wid->parent), summary_wid);
	gtkut_container_remove
		(GTK_CONTAINER(message_wid->parent), message_wid);
	gtkut_container_remove
		(GTK_CONTAINER(qsearch_wid->parent), qsearch_wid);

	/* clean containers */
	switch (mainwin->type) {
	case SEPARATE_NONE:
		if (!mainwin->win.sep_none.vpaned->parent)
			gtk_widget_destroy(mainwin->win.sep_none.vpaned);
		gtk_widget_destroy(mainwin->win.sep_none.hpaned);
		mainwin->win.sep_none.hpaned = NULL;
		mainwin->win.sep_none.vpaned = NULL;
		break;
	case SEPARATE_FOLDER:
		gtk_widget_destroy(mainwin->win.sep_folder.folderwin);
		if (!mainwin->win.sep_folder.vpaned->parent)
			gtk_widget_destroy(mainwin->win.sep_folder.vpaned);
		gtk_widget_destroy(vbox_summary);
		mainwin->win.sep_folder.folderwin = NULL;
		mainwin->win.sep_folder.vpaned = NULL;
		break;
	case SEPARATE_MESSAGE:
		gtk_widget_destroy(mainwin->win.sep_message.messagewin);
		gtk_widget_destroy(mainwin->win.sep_message.hpaned);
		mainwin->win.sep_message.messagewin = NULL;
		mainwin->win.sep_message.hpaned = NULL;
		break;
	case SEPARATE_BOTH:
		gtk_widget_destroy(vbox_summary);
		gtk_widget_destroy(mainwin->win.sep_both.folderwin);
		gtk_widget_destroy(mainwin->win.sep_both.messagewin);
		mainwin->win.sep_both.folderwin = NULL;
		mainwin->win.sep_both.messagewin = NULL;
		break;
	}

	gtk_widget_hide(mainwin->window);
}

void main_window_change_layout(MainWindow *mainwin, LayoutType layout,
			       SeparateType type)
{
	GtkWidget *folder_wid  = GTK_WIDGET_PTR(mainwin->folderview);
	GtkWidget *summary_wid = GTK_WIDGET_PTR(mainwin->summaryview);
	GtkWidget *message_wid = GTK_WIDGET_PTR(mainwin->messageview);
	GtkWidget *qsearch_wid = GTK_WIDGET_PTR(mainwin->summaryview->qsearch);
	GtkWidget *focus_widget;

	debug_print("Changing window layout type (layout: %d -> %d, separation: %d -> %d)\n", prefs_common.layout_type, layout, mainwin->type, type);

	if (prefs_common.layout_type == layout && mainwin->type == type)
		return;

	/* keep previous focus */
	focus_widget = gtk_window_get_focus(GTK_WINDOW(mainwin->window));

	gtk_widget_ref(folder_wid);
	gtk_widget_ref(summary_wid);
	gtk_widget_ref(message_wid);
	gtk_widget_ref(qsearch_wid);

	main_window_hide(mainwin);
	main_window_set_widgets(mainwin, layout, type);
	gtk_widget_show(mainwin->window);
	if (focus_widget)
		gtk_widget_grab_focus(focus_widget);

	gtk_widget_unref(folder_wid);
	gtk_widget_unref(summary_wid);
	gtk_widget_unref(message_wid);
	gtk_widget_unref(qsearch_wid);
}

void main_window_toggle_message_view(MainWindow *mainwin)
{
	SummaryView *summaryview = mainwin->summaryview;
	union CompositeWin *cwin = &mainwin->win;
	GtkWidget *vpaned = NULL;
	GtkWidget *container = NULL;
	GtkWidget *msgwin = NULL;
	gboolean use_vlayout = (prefs_common.layout_type == LAYOUT_VERTICAL);

	switch (mainwin->type) {
	case SEPARATE_NONE:
		vpaned = cwin->sep_none.vpaned;
		container = GTK_WIDGET_PTR(summaryview->qsearch)->parent;
		break;
	case SEPARATE_FOLDER:
		vpaned = cwin->sep_folder.vpaned;
		container = GTK_WIDGET_PTR(summaryview->qsearch)->parent;
		break;
	case SEPARATE_MESSAGE:
		msgwin = mainwin->win.sep_message.messagewin;
		break;
	case SEPARATE_BOTH:
		msgwin = mainwin->win.sep_both.messagewin;
		break;
	}

	if (msgwin) {
		/* separate message view */
		if (GTK_WIDGET_VISIBLE(msgwin)) {
			gtk_widget_hide(msgwin);
			mainwin->messageview->visible = FALSE;
			summaryview->displayed = NULL;
		} else {
			gtk_widget_show(msgwin);
			mainwin->messageview->visible = TRUE;
		}
	} else if (vpaned->parent != NULL) {
		/* hide message view */
		mainwin->messageview->visible = FALSE;
		summaryview->displayed = NULL;
		gtk_widget_ref(vpaned);
		gtkut_container_remove(GTK_CONTAINER(container), vpaned);
		gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), container);
		if (!use_vlayout)
			gtk_widget_hide(summaryview->hseparator);
	} else {
		/* show message view */
		mainwin->messageview->visible = TRUE;
		gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), vpaned);
		gtk_container_add(GTK_CONTAINER(container), vpaned);
		gtk_widget_unref(vpaned);
		if (!use_vlayout)
			gtk_widget_show(summaryview->hseparator);
	}

	if (messageview_is_visible(mainwin->messageview))
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      use_vlayout ? GTK_ARROW_RIGHT : GTK_ARROW_DOWN,
			      GTK_SHADOW_OUT);
	else
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      use_vlayout ? GTK_ARROW_LEFT : GTK_ARROW_UP,
			      GTK_SHADOW_OUT);

	if (mainwin->messageview->visible == FALSE)
		messageview_clear(mainwin->messageview);

	main_window_set_menu_sensitive(mainwin);

	prefs_common.msgview_visible = mainwin->messageview->visible;

	gtk_widget_grab_focus(summaryview->treeview);
}

void main_window_get_size(MainWindow *mainwin)
{
	GtkAllocation *allocation;
	gboolean vlayout = (prefs_common.layout_type == LAYOUT_VERTICAL);

	allocation = &(GTK_WIDGET_PTR(mainwin->summaryview)->allocation);
	if (allocation->width > 1 && allocation->height > 1) {
		if (vlayout) {
			if (!(mainwin->type & SEPARATE_MESSAGE) &&
			    messageview_is_visible(mainwin->messageview))
				prefs_common.summaryview_vwidth = allocation->width;
			prefs_common.summaryview_vheight = allocation->height;
		} else {
			if (!prefs_common.mainwin_maximized) {
				prefs_common.summaryview_width = allocation->width;
				prefs_common.mainview_width = allocation->width;
			}
			if ((mainwin->type == SEPARATE_NONE ||
			     mainwin->type == SEPARATE_FOLDER) &&
			    messageview_is_visible(mainwin->messageview))
				prefs_common.summaryview_height = allocation->height;
		}
	}

	if (prefs_common.mainwin_maximized) {
		allocation = &(GTK_WIDGET_PTR(mainwin->folderview)->allocation);
		if (allocation->width > 1 && allocation->height > 1)
			prefs_common.folderview_width  = allocation->width;
		return;
	}

	allocation = &mainwin->window->allocation;
	if (allocation->width > 1 && allocation->height > 1) {
		prefs_common.mainview_height = allocation->height;
		prefs_common.mainwin_width   = allocation->width;
		prefs_common.mainwin_height  = allocation->height;
	}

	allocation = &(GTK_WIDGET_PTR(mainwin->folderview)->allocation);
	if (allocation->width > 1 && allocation->height > 1) {
		prefs_common.folderview_width  = allocation->width;
		prefs_common.folderview_height = allocation->height;
	}

	allocation = &(GTK_WIDGET_PTR(mainwin->messageview)->allocation);
	if (allocation->width > 1 && allocation->height > 1) {
		if (vlayout) {
			prefs_common.msgview_vwidth = allocation->width;
			prefs_common.msgview_vheight = allocation->height;
		} else {
			prefs_common.msgview_width = allocation->width;
			prefs_common.msgview_height = allocation->height;
		}
	}

#if 0
	debug_print("summaryview size: %d x %d\n",
		    prefs_common.summaryview_width,
		    prefs_common.summaryview_height);
	debug_print("mainwin size: %d x %d\n",
		    prefs_common.mainwin_width,
		    prefs_common.mainwin_height);
	debug_print("folderview size: %d x %d\n",
		    prefs_common.folderview_width,
		    prefs_common.folderview_height);
	debug_print("messageview size: %d x %d\n",
		    prefs_common.msgview_width,
		    prefs_common.msgview_height);
#endif
}

void main_window_get_position(MainWindow *mainwin)
{
	gint x, y;
	GtkWidget *window;

	if (prefs_common.mainwin_maximized || mainwin->window_hidden)
		return;

	gtkut_widget_get_uposition(mainwin->window, &x, &y);

	prefs_common.mainview_x = x;
	prefs_common.mainview_y = y;
	prefs_common.mainwin_x = x;
	prefs_common.mainwin_y = y;

	debug_print("main window position: %d, %d\n", x, y);

	window = main_window_get_folder_window(mainwin);
	if (window) {
		gtkut_widget_get_uposition(window, &x, &y);
		prefs_common.folderwin_x = x;
		prefs_common.folderwin_y = y;
		debug_print("folder window position: %d, %d\n", x, y);
	}
	window = main_window_get_message_window(mainwin);
	if (window) {
		gtkut_widget_get_uposition(window, &x, &y);
		prefs_common.main_msgwin_x = x;
		prefs_common.main_msgwin_y = y;
		debug_print("message window position: %d, %d\n", x, y);
	}
}

void main_window_progress_on(MainWindow *mainwin)
{
	gtk_progress_set_show_text(GTK_PROGRESS(mainwin->progressbar), TRUE);
	gtk_progress_set_format_string(GTK_PROGRESS(mainwin->progressbar), "");
}

void main_window_progress_off(MainWindow *mainwin)
{
	gtk_progress_set_show_text(GTK_PROGRESS(mainwin->progressbar), FALSE);
	gtk_progress_bar_update(GTK_PROGRESS_BAR(mainwin->progressbar), 0.0);
	gtk_progress_set_format_string(GTK_PROGRESS(mainwin->progressbar), "");
}

void main_window_progress_set(MainWindow *mainwin, gint cur, gint total)
{
	gchar buf[32];

	g_snprintf(buf, sizeof(buf), "%d / %d", cur, total);
	gtk_progress_set_format_string(GTK_PROGRESS(mainwin->progressbar), buf);
	gtk_progress_bar_update(GTK_PROGRESS_BAR(mainwin->progressbar),
				(cur == 0 && total == 0) ? 0 :
				(gfloat)cur / (gfloat)total);
}

void main_window_progress_show(gint cur, gint total)
{
	MainWindow *mainwin;

	mainwin = main_window_get();

	if (total > 0) {
		gtk_progress_set_show_text(GTK_PROGRESS(mainwin->progressbar),
					   TRUE);
		main_window_progress_set(mainwin, cur, total);
	} else
		main_window_progress_off(mainwin);
}

void main_window_toggle_online(MainWindow *mainwin, gboolean online)
{
	if (prefs_common.online_mode != online)
		online_switch_clicked(mainwin->online_switch, mainwin);
}

gboolean main_window_toggle_online_if_offline(MainWindow *mainwin)
{
	if (!prefs_common.online_mode) {
		if (alertpanel(_("Offline"),
			       _("You are offline. Go online?"),
			       GTK_STOCK_YES, GTK_STOCK_NO, NULL)
		    == G_ALERTDEFAULT)
			main_window_toggle_online(mainwin, TRUE);
	}

	return prefs_common.online_mode;
}

void main_window_empty_trash(MainWindow *mainwin, gboolean confirm)
{
	GList *list;

	if (!procmsg_trash_messages_exist())
		return;

	if (confirm) {
		if (alertpanel(_("Empty all trash"),
			       _("Delete all messages in trash folders?"),
			       GTK_STOCK_YES, GTK_STOCK_NO, NULL)
		    != G_ALERTDEFAULT)
			return;
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	procmsg_empty_all_trash();
	statusbar_pop_all();
	trayicon_set_tooltip(NULL);
	trayicon_set_notify(FALSE);

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder;

		folder = list->data;
		if (folder->trash)
			folderview_update_item(folder->trash, TRUE);
	}

	if (mainwin->summaryview->folder_item &&
	    mainwin->summaryview->folder_item->stype == F_TRASH)
		gtk_widget_grab_focus(mainwin->folderview->treeview);
}

void main_window_add_mailbox(MainWindow *mainwin)
{
	gchar *path;
	Folder *folder;

	path = input_dialog_with_filesel
		(_("Add mailbox"),
		 _("Specify the location of mailbox.\n"
		   "If the existing mailbox is specified, it will be\n"
		   "scanned automatically."),
		 "Mail", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	if (!path) return;
	if (folder_find_from_path(path)) {
		alertpanel_error(_("The mailbox `%s' already exists."), path);
		g_free(path);
		return;
	}
	if (!strcmp(path, "Mail"))
		folder = folder_new(F_MH, _("Mailbox"), path);
	else
		folder = folder_new(F_MH, g_basename(path), path);
	g_free(path);

	if (folder->klass->create_tree(folder) < 0) {
		alertpanel_error(_("Creation of the mailbox failed.\n"
				   "Maybe some files already exist, or you don't have the permission to write there."));
		folder_destroy(folder);
		return;
	}

	folder_add(folder);
	folder_set_ui_func(folder, scan_tree_func, mainwin);
	folder->klass->scan_tree(folder);
	folder_set_ui_func(folder, NULL, NULL);

	folderview_set(mainwin->folderview);
}

void main_window_send_queue(MainWindow *mainwin)
{
	GList *list;

	if (inc_is_active())
		return;
	if (!main_window_toggle_online_if_offline(mainwin))
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
	main_window_set_menu_sensitive(mainwin);
	main_window_set_toolbar_sensitive(mainwin);
}

typedef enum
{
	M_UNLOCKED            = 1 << 0,
	M_MSG_EXIST           = 1 << 1,
	M_TARGET_EXIST        = 1 << 2,
	M_SINGLE_TARGET_EXIST = 1 << 3,
	M_EXEC                = 1 << 4,
	M_ALLOW_REEDIT        = 1 << 5,
	M_HAVE_ACCOUNT        = 1 << 6,
	M_THREADED	      = 1 << 7,
	M_UNTHREADED	      = 1 << 8,
	M_ALLOW_DELETE	      = 1 << 9,
	M_INC_ACTIVE	      = 1 << 10,
	M_ENABLE_JUNK	      = 1 << 11,
	M_HAVE_QUEUED_MSG     = 1 << 12,
	M_POP3_ACCOUNT	      = 1 << 13,

	M_FOLDER_NEWOK	      = 1 << 17,
	M_FOLDER_RENOK	      = 1 << 18,
	M_FOLDER_MOVEOK	      = 1 << 19,
	M_FOLDER_DELOK	      = 1 << 20,
	M_MBOX_ADDOK	      = 1 << 21,
	M_MBOX_RMOK	      = 1 << 22,
	M_MBOX_CHKOK	      = 1 << 23,
	M_MBOX_CHKALLOK	      = 1 << 24,
	M_MBOX_REBUILDOK      = 1 << 25
} SensitiveCond;

static SensitiveCond main_window_get_current_state(MainWindow *mainwin)
{
	SensitiveCond state = 0;
	SummarySelection selection;
	GList *list;
	FolderItem *item = mainwin->summaryview->folder_item;

	selection = summary_get_selection_type(mainwin->summaryview);

	if (mainwin->lock_count == 0)
		state |= M_UNLOCKED;
	if (selection != SUMMARY_NONE)
		state |= M_MSG_EXIST;
	if (item && item->path && item->parent && !item->no_select) {
		state |= M_EXEC;
		if (item->threaded)
			state |= M_THREADED;
		else
			state |= M_UNTHREADED;	
		if (FOLDER_TYPE(item->folder) != F_NEWS)
			state |= M_ALLOW_DELETE;
	}
	if (selection == SUMMARY_SELECTED_SINGLE ||
	    selection == SUMMARY_SELECTED_MULTIPLE)
		state |= M_TARGET_EXIST;
	if (selection == SUMMARY_SELECTED_SINGLE)
		state |= M_SINGLE_TARGET_EXIST;
	if (selection == SUMMARY_SELECTED_SINGLE &&
	    FOLDER_ITEM_IS_SENT_FOLDER(item))
		state |= M_ALLOW_REEDIT;
	if (cur_account) {
		state |= M_HAVE_ACCOUNT;
		if (cur_account->protocol == A_POP3)
			state |= M_POP3_ACCOUNT;
	}

	if (inc_is_active())
		state |= M_INC_ACTIVE;

	if (prefs_common.enable_junk)
		state |= M_ENABLE_JUNK;

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;
		if (folder->queue && folder->queue->total > 0) {
			state |= M_HAVE_QUEUED_MSG;
			break;
		}
	}

	item = folderview_get_selected_item(mainwin->folderview);
	if (item) {
		state |= M_FOLDER_NEWOK;
		if (item->parent == NULL) {
			state |= M_MBOX_RMOK;
			state |= M_MBOX_CHKOK;
		}
		if (FOLDER_IS_LOCAL(item->folder) ||
		    FOLDER_TYPE(item->folder) == F_IMAP) {
			if (item->parent == NULL)
				state |= M_MBOX_REBUILDOK;
			else if (item->stype == F_NORMAL) {
				state |= M_FOLDER_RENOK;
				state |= M_FOLDER_DELOK;
				if (item->folder->klass->move_folder)
					state |= M_FOLDER_MOVEOK;
			}
		} else if (FOLDER_TYPE(item->folder) == F_NEWS) {
			if (item->parent != NULL)
				state |= M_FOLDER_DELOK;
		}
	}
	state |= M_MBOX_ADDOK;
	state |= M_MBOX_CHKALLOK;

	return state;
}

void main_window_set_toolbar_sensitive(MainWindow *mainwin)
{
	SensitiveCond state;
	gboolean sensitive, prev_sensitive;
	gint n;
	gint i = 0;

	struct {
		GtkWidget *widget;
		SensitiveCond cond;
	} entry[20];

#define SET_WIDGET_COND(w, c)	\
{				\
	entry[i].widget = w;	\
	entry[i].cond = c;	\
	i++;			\
}

	SET_WIDGET_COND(mainwin->get_btn, M_HAVE_ACCOUNT|M_UNLOCKED);
	SET_WIDGET_COND(mainwin->getall_btn, M_HAVE_ACCOUNT|M_UNLOCKED);
	SET_WIDGET_COND(mainwin->rpop3_btn,
			M_HAVE_ACCOUNT|M_UNLOCKED|M_POP3_ACCOUNT);
	SET_WIDGET_COND(mainwin->send_btn,
			M_HAVE_ACCOUNT|M_UNLOCKED|M_HAVE_QUEUED_MSG);
	SET_WIDGET_COND(mainwin->compose_btn, M_HAVE_ACCOUNT);
	SET_WIDGET_COND(mainwin->reply_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->reply_combo ?
			GTK_WIDGET_PTR(mainwin->reply_combo) : NULL,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->replyall_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->fwd_btn, M_HAVE_ACCOUNT|M_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->fwd_combo ? GTK_WIDGET_PTR(mainwin->fwd_combo)
			: NULL,
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->delete_btn,
			M_TARGET_EXIST|M_ALLOW_DELETE);
	SET_WIDGET_COND(mainwin->junk_btn,
			M_TARGET_EXIST|M_ALLOW_DELETE|M_ENABLE_JUNK);
	SET_WIDGET_COND(mainwin->notjunk_btn,
			M_TARGET_EXIST|M_ALLOW_DELETE|M_ENABLE_JUNK);
	SET_WIDGET_COND(mainwin->exec_btn, M_MSG_EXIST|M_EXEC);
	SET_WIDGET_COND(mainwin->next_btn, M_MSG_EXIST);
	SET_WIDGET_COND(mainwin->prev_btn, M_MSG_EXIST);
	SET_WIDGET_COND(mainwin->print_btn, M_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->stop_btn, M_INC_ACTIVE);
	SET_WIDGET_COND(mainwin->prefs_common_btn, M_UNLOCKED);
	SET_WIDGET_COND(mainwin->prefs_account_btn, M_HAVE_ACCOUNT|M_UNLOCKED);

#undef SET_WIDGET_COND

	state = main_window_get_current_state(mainwin);

	n = sizeof(entry) / sizeof(entry[0]);
	for (i = 0; i < n; i++) {
		if (entry[i].widget) {
			prev_sensitive =
				GTK_WIDGET_IS_SENSITIVE(entry[i].widget);
			sensitive = ((entry[i].cond & state) == entry[i].cond);
			if (prev_sensitive != sensitive) {
				/* workaround for GTK+ bug (#56070) */
				if (!prev_sensitive)
					gtk_widget_hide(entry[i].widget);
				gtk_widget_set_sensitive(entry[i].widget,
							 sensitive);
				if (!prev_sensitive)
					gtk_widget_show(entry[i].widget);
			}
		}
	}
}

static void main_window_set_toolbar_button_visibility(MainWindow *mainwin)
{
	GtkToolbarStyle style = GTK_TOOLBAR_BOTH_HORIZ;

	if (prefs_common.toolbar_style == TOOLBAR_NONE)
		style = -1;
	else if (prefs_common.toolbar_style == TOOLBAR_ICON)
		style = GTK_TOOLBAR_ICONS;
	else if (prefs_common.toolbar_style == TOOLBAR_TEXT)
		style = GTK_TOOLBAR_TEXT;
	else if (prefs_common.toolbar_style == TOOLBAR_BOTH)
		style = GTK_TOOLBAR_BOTH;
	else if (prefs_common.toolbar_style == TOOLBAR_BOTH_HORIZ)
		style = GTK_TOOLBAR_BOTH_HORIZ;

	if (style != -1) {
		gtk_toolbar_set_style(GTK_TOOLBAR(mainwin->toolbar), style);
		gtk_widget_show(mainwin->toolbar);
		gtk_widget_queue_resize(mainwin->toolbar);
	} else
		gtk_widget_hide(mainwin->toolbar);

#if 0
	if (mainwin->junk_btn) {
		if (prefs_common.enable_junk)
			gtk_widget_show(mainwin->junk_btn);
		else
			gtk_widget_hide(mainwin->junk_btn);
	}

	if (mainwin->exec_btn) {
		if (prefs_common.immediate_exec)
			gtk_widget_hide(mainwin->exec_btn);
		else
			gtk_widget_show(mainwin->exec_btn);
	}
#endif
}

void main_window_set_menu_sensitive(MainWindow *mainwin)
{
	GtkItemFactory *ifactory = mainwin->menu_factory;
	SensitiveCond state;
	gboolean sensitive;
	GtkWidget *menu;
	GtkWidget *menuitem;
	FolderItem *item;
	gchar *menu_path;
	gint i;
	GList *cur_item;

	static const struct {
		gchar *const entry;
		SensitiveCond cond;
	} entry[] = {
		{"/File/Folder/Create new folder...", M_UNLOCKED|M_FOLDER_NEWOK},
		{"/File/Folder/Rename folder..."    , M_UNLOCKED|M_FOLDER_RENOK},
		{"/File/Folder/Move folder..."      , M_UNLOCKED|M_FOLDER_MOVEOK},
		{"/File/Folder/Delete folder"       , M_UNLOCKED|M_FOLDER_DELOK},
		{"/File/Mailbox/Add mailbox..."     , M_UNLOCKED|M_MBOX_ADDOK},
		{"/File/Mailbox/Remove mailbox"     , M_UNLOCKED|M_MBOX_RMOK},
		{"/File/Mailbox/Check for new messages"
						    , M_UNLOCKED|M_MBOX_CHKOK},
		{"/File/Mailbox/Check for new messages in all mailboxes"
						    , M_UNLOCKED|M_MBOX_CHKALLOK},
		{"/File/Mailbox/Rebuild folder tree", M_UNLOCKED|M_MBOX_REBUILDOK},
		{"/File/Import mail data..."        , M_UNLOCKED},
		{"/File/Export mail data..."        , M_UNLOCKED},
		{"/File/Empty all trash"            , M_UNLOCKED},

		{"/File/Save as..."  , M_SINGLE_TARGET_EXIST},
		{"/File/Print..."    , M_TARGET_EXIST},
		{"/File/Work offline", M_UNLOCKED},
		/* {"/File/Close"    , M_UNLOCKED}, */
		{"/File/Exit"        , M_UNLOCKED},

		{"/Edit/Select thread"             , M_SINGLE_TARGET_EXIST},

		{"/View/Sort"                      , M_EXEC},
		{"/View/Thread view"               , M_EXEC},
		{"/View/Expand all threads"        , M_MSG_EXIST},
		{"/View/Collapse all threads"      , M_MSG_EXIST},
		{"/View/Go to/Prev message"        , M_MSG_EXIST},
		{"/View/Go to/Next message"        , M_MSG_EXIST},
		{"/View/Go to/Prev unread message" , M_MSG_EXIST},
		{"/View/Go to/Next unread message" , M_MSG_EXIST},
		{"/View/Go to/Prev new message"    , M_MSG_EXIST},
		{"/View/Go to/Next new message"    , M_MSG_EXIST},
		{"/View/Go to/Prev marked message" , M_MSG_EXIST},
		{"/View/Go to/Next marked message" , M_MSG_EXIST},
		{"/View/Go to/Prev labeled message", M_MSG_EXIST},
		{"/View/Go to/Next labeled message", M_MSG_EXIST},
		{"/View/Open in new window"        , M_SINGLE_TARGET_EXIST},
		{"/View/All headers"          , M_SINGLE_TARGET_EXIST},
		{"/View/Message source"            , M_SINGLE_TARGET_EXIST},

		{"/Message/Receive/Get from current account"
						 , M_HAVE_ACCOUNT|M_UNLOCKED},
		{"/Message/Receive/Get from all accounts"
						 , M_HAVE_ACCOUNT|M_UNLOCKED},
		{"/Message/Receive/Stop receiving"
						 , M_INC_ACTIVE},
		{"/Message/Receive/Remote mailbox..."
						 , M_HAVE_ACCOUNT|M_UNLOCKED|M_POP3_ACCOUNT},
		{"/Message/Send queued messages" , M_HAVE_ACCOUNT|M_UNLOCKED|M_HAVE_QUEUED_MSG},

		{"/Message/Compose new message"  , M_HAVE_ACCOUNT},
		{"/Message/Reply"                , M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Message/Reply to"             , M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Message/Forward"              , M_HAVE_ACCOUNT|M_TARGET_EXIST},
		{"/Message/Forward as attachment", M_HAVE_ACCOUNT|M_TARGET_EXIST},
		{"/Message/Redirect"             , M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Message/Move..."              , M_TARGET_EXIST|M_ALLOW_DELETE},
		{"/Message/Copy..."              , M_TARGET_EXIST|M_EXEC},
		{"/Message/Delete"               , M_TARGET_EXIST|M_ALLOW_DELETE},
		{"/Message/Mark"                 , M_TARGET_EXIST},
		{"/Message/Set as junk mail"     , M_TARGET_EXIST|M_ALLOW_DELETE|M_ENABLE_JUNK},
		{"/Message/Set as not junk mail" , M_TARGET_EXIST|M_ALLOW_DELETE|M_ENABLE_JUNK},
		{"/Message/Re-edit"              , M_HAVE_ACCOUNT|M_ALLOW_REEDIT},

		{"/Tools/Add sender to address book...", M_SINGLE_TARGET_EXIST},
		{"/Tools/Filter all messages in folder", M_MSG_EXIST|M_EXEC},
		{"/Tools/Filter selected messages"     , M_TARGET_EXIST|M_EXEC},
		{"/Tools/Create filter rule"           , M_SINGLE_TARGET_EXIST|M_UNLOCKED},
		{"/Tools/Filter junk mails in folder"  , M_MSG_EXIST|M_EXEC|M_ENABLE_JUNK},
		{"/Tools/Filter junk mails in selected messages", M_TARGET_EXIST|M_EXEC|M_ENABLE_JUNK},
#ifndef G_OS_WIN32
		{"/Tools/Actions"                      , M_TARGET_EXIST|M_UNLOCKED},
#endif
		{"/Tools/Execute marked process"       , M_MSG_EXIST|M_EXEC},
		{"/Tools/Delete duplicated messages"   , M_MSG_EXIST|M_ALLOW_DELETE},
		{"/Tools/Concatenate separated messages"
						       , M_TARGET_EXIST|M_UNLOCKED|M_ALLOW_DELETE},

		{"/Configuration/Common preferences...", M_UNLOCKED},
		{"/Configuration/Filter settings...", M_UNLOCKED},
		{"/Configuration/Preferences for current account...", M_UNLOCKED},
		{"/Configuration/Create new account...", M_UNLOCKED},
		{"/Configuration/Edit accounts...", M_UNLOCKED},
		{"/Configuration/Change current account", M_UNLOCKED},

		{NULL, 0}
	};

	state = main_window_get_current_state(mainwin);

	for (i = 0; entry[i].entry != NULL; i++) {
		sensitive = ((entry[i].cond & state) == entry[i].cond);
		menu_set_sensitive(ifactory, entry[i].entry, sensitive);
	}

	menu = gtk_item_factory_get_widget(ifactory, "/Message/Receive");

	/* search for separator */
	for (cur_item = GTK_MENU_SHELL(menu)->children; cur_item != NULL;
	     cur_item = cur_item->next) {
		if (GTK_BIN(cur_item->data)->child == NULL) {
			cur_item = cur_item->next;
			break;
		}
	}

	for (; cur_item != NULL; cur_item = cur_item->next) {
		gtk_widget_set_sensitive(GTK_WIDGET(cur_item->data),
					 (M_UNLOCKED & state) != 0);
	}

	main_window_menu_callback_block(mainwin);

#define SET_CHECK_MENU_ACTIVE(path, active) \
{ \
	menuitem = gtk_item_factory_get_widget(ifactory, path); \
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), active); \
}

	SET_CHECK_MENU_ACTIVE("/View/Show or hide/Message view",
			      messageview_is_visible(mainwin->messageview));

	item = mainwin->summaryview->folder_item;
	menu_path = "/View/Sort/Don't sort";
	if (item) {
		switch (item->sort_key) {
		case SORT_BY_NUMBER:
			menu_path = "/View/Sort/by number"; break;
		case SORT_BY_SIZE:
			menu_path = "/View/Sort/by size"; break;
		case SORT_BY_DATE:
			menu_path = "/View/Sort/by date"; break;
		case SORT_BY_TDATE:
			menu_path = "/View/Sort/by thread date"; break;
		case SORT_BY_FROM:
			menu_path = "/View/Sort/by from"; break;
		case SORT_BY_TO:
			menu_path = "/View/Sort/by recipient"; break;
		case SORT_BY_SUBJECT:
			menu_path = "/View/Sort/by subject"; break;
		case SORT_BY_LABEL:
			menu_path = "/View/Sort/by color label"; break;
		case SORT_BY_MARK:
			menu_path = "/View/Sort/by mark"; break;
		case SORT_BY_UNREAD:
			menu_path = "/View/Sort/by unread"; break;
		case SORT_BY_MIME:
			menu_path = "/View/Sort/by attachment"; break;
		case SORT_BY_NONE:
		default:
			menu_path = "/View/Sort/Don't sort"; break;
		}
	}
	SET_CHECK_MENU_ACTIVE(menu_path, TRUE);

	if (!item || item->sort_type == SORT_ASCENDING) {
		SET_CHECK_MENU_ACTIVE("/View/Sort/Ascending", TRUE);
	} else {
		SET_CHECK_MENU_ACTIVE("/View/Sort/Descending", TRUE);
	}

	if (item && item->sort_key != SORT_BY_NONE) {
		menu_set_sensitive(ifactory, "/View/Sort/Ascending", TRUE);
		menu_set_sensitive(ifactory, "/View/Sort/Descending", TRUE);
		menu_set_sensitive(ifactory, "/View/Sort/Attract by subject",
				   FALSE);
	} else {
		menu_set_sensitive(ifactory, "/View/Sort/Ascending", FALSE);
		menu_set_sensitive(ifactory, "/View/Sort/Descending", FALSE);
		menu_set_sensitive(ifactory, "/View/Sort/Attract by subject",
				   (item != NULL));
	}

	SET_CHECK_MENU_ACTIVE("/View/All headers",
			      mainwin->messageview->textview->show_all_headers);
	SET_CHECK_MENU_ACTIVE("/View/Thread view", (state & M_THREADED) != 0);

#undef SET_CHECK_MENU_ACTIVE

	main_window_menu_callback_unblock(mainwin);
}

void main_window_popup(MainWindow *mainwin)
{
	gtkut_window_popup(mainwin->window);

	switch (mainwin->type) {
	case SEPARATE_FOLDER:
		if (prefs_common.folderview_visible)
			gtkut_window_popup(mainwin->win.sep_folder.folderwin);
		break;
	case SEPARATE_MESSAGE:
		if (messageview_is_visible(mainwin->messageview))
			gtkut_window_popup(mainwin->win.sep_message.messagewin);
		break;
	case SEPARATE_BOTH:
		if (prefs_common.folderview_visible)
			gtkut_window_popup(mainwin->win.sep_both.folderwin);
		if (messageview_is_visible(mainwin->messageview))
			gtkut_window_popup(mainwin->win.sep_both.messagewin);
		break;
	default:
		break;
	}
}

static void main_window_set_widgets(MainWindow *mainwin, LayoutType layout,
				    SeparateType type)
{
	GtkWidget *folderwin = NULL;
	GtkWidget *messagewin = NULL;
	GtkWidget *hpaned;
	GtkWidget *vpaned;
	GtkWidget *vbox_body = mainwin->vbox_body;
	GtkWidget *vbox_summary;
	GtkItemFactory *ifactory = mainwin->menu_factory;
	GtkWidget *menuitem;
	gboolean use_vlayout = (layout == LAYOUT_VERTICAL);

	debug_print("Setting main window widgets...\n");

	gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->folderview),
				    prefs_common.folderview_width,
				    prefs_common.folderview_height);
	if (use_vlayout) {
		gtk_widget_set_size_request
			(GTK_WIDGET_PTR(mainwin->summaryview),
			 prefs_common.summaryview_vwidth,
			 prefs_common.summaryview_vheight);
		gtk_widget_set_size_request
			(GTK_WIDGET_PTR(mainwin->messageview),
			 prefs_common.msgview_vwidth,
			 prefs_common.msgview_vheight);
	} else {
		gtk_widget_set_size_request
			(GTK_WIDGET_PTR(mainwin->summaryview),
			 prefs_common.summaryview_width,
			 prefs_common.summaryview_height);
		gtk_widget_set_size_request
			(GTK_WIDGET_PTR(mainwin->messageview),
			 prefs_common.msgview_width,
			 prefs_common.msgview_height);
	}

	/* create separated window(s) if needed */
	if (type & SEPARATE_FOLDER) {
		folderwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(folderwin),
				     _("Sylpheed - Folder View"));
		gtk_window_set_wmclass(GTK_WINDOW(folderwin),
				       "folder_view", "Sylpheed");
		gtk_window_set_policy(GTK_WINDOW(folderwin),
				      TRUE, TRUE, FALSE);
		gtkut_window_move(GTK_WINDOW(folderwin),
				  prefs_common.folderwin_x,
				  prefs_common.folderwin_y);
		gtk_container_set_border_width(GTK_CONTAINER(folderwin), 0);
		g_signal_connect(G_OBJECT(folderwin), "delete_event",
				 G_CALLBACK(folder_window_close_cb), mainwin);
		gtk_container_add(GTK_CONTAINER(folderwin),
				  GTK_WIDGET_PTR(mainwin->folderview));
		gtk_widget_realize(folderwin);
		if (prefs_common.folderview_visible)
			gtk_widget_show(folderwin);
	}
	if (type & SEPARATE_MESSAGE) {
		messagewin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(messagewin),
				     _("Sylpheed - Message View"));
		gtk_window_set_wmclass(GTK_WINDOW(messagewin),
				       "message_view", "Sylpheed");
		gtk_window_set_policy(GTK_WINDOW(messagewin),
				      TRUE, TRUE, FALSE);
		gtkut_window_move(GTK_WINDOW(messagewin),
				  prefs_common.main_msgwin_x,
				  prefs_common.main_msgwin_y);
		gtk_container_set_border_width(GTK_CONTAINER(messagewin), 0);
		g_signal_connect(G_OBJECT(messagewin), "delete_event",
				 G_CALLBACK(message_window_close_cb), mainwin);
		gtk_container_add(GTK_CONTAINER(messagewin),
				  GTK_WIDGET_PTR(mainwin->messageview));
		gtk_widget_realize(messagewin);
		if (messageview_is_visible(mainwin->messageview))
			gtk_widget_show(messagewin);
	}

	vbox_summary = gtk_vbox_new(FALSE, 1);
	gtk_box_pack_start(GTK_BOX(vbox_summary),
			   GTK_WIDGET_PTR(mainwin->summaryview->qsearch),
			   FALSE, FALSE, 0);
	gtk_widget_show(vbox_summary);

	switch (type) {
	case SEPARATE_NONE:
		hpaned = gtk_hpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox_body), hpaned, TRUE, TRUE, 0);
		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));
		gtk_paned_add2(GTK_PANED(hpaned), vbox_summary);
		gtk_widget_show(hpaned);
		gtk_widget_queue_resize(hpaned);

		if (use_vlayout) {
			vpaned = gtk_hpaned_new();
			gtk_widget_hide(mainwin->summaryview->hseparator);
		} else
			vpaned = gtk_vpaned_new();
		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_add1(GTK_PANED(vpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
			gtk_box_pack_start(GTK_BOX(vbox_summary), vpaned,
					   TRUE, TRUE, 0);
			if (!use_vlayout)
				gtk_widget_show
					(mainwin->summaryview->hseparator);
		} else {
			gtk_box_pack_start(GTK_BOX(vbox_summary),
					   GTK_WIDGET_PTR(mainwin->summaryview),
					   TRUE, TRUE, 0);
			gtk_widget_ref(vpaned);
			gtk_widget_hide(mainwin->summaryview->hseparator);
		}
		gtk_paned_add2(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));
		gtk_widget_show(vpaned);
		gtk_widget_queue_resize(vpaned);

		mainwin->win.sep_none.hpaned = hpaned;
		mainwin->win.sep_none.vpaned = vpaned;

		break;
	case SEPARATE_FOLDER:
		gtk_box_pack_start(GTK_BOX(vbox_body), vbox_summary,
				   TRUE, TRUE, 0);

		if (use_vlayout) {
			vpaned = gtk_hpaned_new();
			gtk_widget_hide(mainwin->summaryview->hseparator);
		} else
			vpaned = gtk_vpaned_new();
		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_add1(GTK_PANED(vpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
			gtk_box_pack_start(GTK_BOX(vbox_summary), vpaned,
					   TRUE, TRUE, 0);
			if (!use_vlayout)
				gtk_widget_show
					(mainwin->summaryview->hseparator);
		} else {
			gtk_box_pack_start(GTK_BOX(vbox_summary),
					   GTK_WIDGET_PTR(mainwin->summaryview),
					   TRUE, TRUE, 0);
			gtk_widget_ref(vpaned);
			gtk_widget_hide(mainwin->summaryview->hseparator);
		}
		gtk_paned_add2(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));
		gtk_widget_show(vpaned);
		gtk_widget_queue_resize(vpaned);

		mainwin->win.sep_folder.folderwin = folderwin;
		mainwin->win.sep_folder.vpaned    = vpaned;

		break;
	case SEPARATE_MESSAGE:
		hpaned = gtk_hpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox_body), hpaned, TRUE, TRUE, 0);
		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));
		gtk_paned_add2(GTK_PANED(hpaned), vbox_summary);
		gtk_box_pack_start(GTK_BOX(vbox_summary),
				   GTK_WIDGET_PTR(mainwin->summaryview),
				   TRUE, TRUE, 0);
		gtk_widget_hide(mainwin->summaryview->hseparator);
		gtk_widget_show(hpaned);
		gtk_widget_queue_resize(hpaned);

		mainwin->win.sep_message.messagewin = messagewin;
		mainwin->win.sep_message.hpaned     = hpaned;

		break;
	case SEPARATE_BOTH:
		gtk_box_pack_start(GTK_BOX(vbox_body), vbox_summary,
				   TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(vbox_summary),
				   GTK_WIDGET_PTR(mainwin->summaryview),
				   TRUE, TRUE, 0);
		gtk_widget_hide(mainwin->summaryview->hseparator);

		mainwin->win.sep_both.folderwin = folderwin;
		mainwin->win.sep_both.messagewin = messagewin;

		break;
	}

	if (messageview_is_visible(mainwin->messageview))
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      use_vlayout ? GTK_ARROW_RIGHT : GTK_ARROW_DOWN,
			      GTK_SHADOW_OUT);
	else
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      use_vlayout ? GTK_ARROW_LEFT : GTK_ARROW_UP,
			      GTK_SHADOW_OUT);

	gtkut_window_move(GTK_WINDOW(mainwin->window), 
			  prefs_common.mainwin_x, prefs_common.mainwin_y);

	gtk_widget_queue_resize(vbox_body);
	gtk_widget_queue_resize(mainwin->vbox);
	gtk_widget_queue_resize(mainwin->window);

	mainwin->type = type;
	prefs_common.layout_type = layout;

	/* toggle menu state */
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Show or hide/Folder tree");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       (type & SEPARATE_FOLDER) == 0 ? TRUE :
				       prefs_common.folderview_visible);
	gtk_widget_set_sensitive(menuitem, ((type & SEPARATE_FOLDER) != 0));
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Show or hide/Message view");
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM(menuitem),
		 messageview_is_visible(mainwin->messageview));

	if (layout == LAYOUT_NORMAL) {
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Layout/Normal");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					       TRUE);
	} else if (layout == LAYOUT_VERTICAL) {
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Layout/Vertical");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					       TRUE);
	}

	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Separate folder tree");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       ((type & SEPARATE_FOLDER) != 0));
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Separate message view");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       ((type & SEPARATE_MESSAGE) != 0));

	if (folderwin) {
		g_signal_connect
			(G_OBJECT(folderwin), "size_allocate",
			 G_CALLBACK(folder_window_size_allocate_cb), mainwin);
	}
	if (messagewin) {
		g_signal_connect
			(G_OBJECT(messagewin), "size_allocate",
			 G_CALLBACK(message_window_size_allocate_cb), mainwin);
	}

	debug_print("done.\n");
}

static GtkItemFactoryEntry reply_entries[] =
{
	{N_("/_Reply"),			NULL, reply_cb, COMPOSE_REPLY, NULL},
	{N_("/Reply to _all"),		NULL, reply_cb, COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/Reply to _sender"),	NULL, reply_cb, COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/Reply to mailing _list"),	NULL, reply_cb, COMPOSE_REPLY_TO_LIST, NULL}
};

static GtkItemFactoryEntry forward_entries[] =
{
	{N_("/_Forward"),		NULL, reply_cb, COMPOSE_FORWARD, NULL},
	{N_("/For_ward as attachment"), NULL, reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/Redirec_t"),		NULL, reply_cb, COMPOSE_REDIRECT, NULL}
};

static PrefsToolbarItem items[] =
{
	{T_GET,			TRUE,	toolbar_inc_cb},
	{T_GET_ALL,		TRUE,	toolbar_inc_all_cb},
	{T_SEND_QUEUE,		TRUE,	toolbar_send_cb},
	{T_COMPOSE,		TRUE,	toolbar_compose_cb},
	{T_REPLY,		TRUE,	toolbar_reply_cb},
	{T_REPLY_ALL,		TRUE,	toolbar_reply_to_all_cb},
	{T_FORWARD,		TRUE,	toolbar_forward_cb},
	{T_DELETE,		FALSE,	toolbar_delete_cb},
	{T_JUNK,		TRUE,	toolbar_junk_cb},
	{T_NOTJUNK,		FALSE,	toolbar_notjunk_cb},
	{T_NEXT,		FALSE,	toolbar_next_unread_cb},
	{T_PREV,		FALSE,	toolbar_prev_unread_cb},
	{T_SEARCH,		FALSE,	toolbar_search_cb},
	{T_PRINT,		FALSE,	toolbar_print_cb},
	{T_STOP,		FALSE,	toolbar_stop_cb},
	{T_ADDRESS_BOOK,	FALSE,	toolbar_address_cb},
	{T_EXECUTE,		FALSE,	toolbar_exec_cb},
	{T_COMMON_PREFS,	FALSE,	toolbar_prefs_common_cb},
	{T_ACCOUNT_PREFS,	FALSE,	toolbar_prefs_account_cb},
	{T_REMOTE_MAILBOX,	FALSE,	toolbar_rpop3_cb},

	{-1, FALSE, NULL}
};

static GtkWidget *main_window_toolbar_create(MainWindow *mainwin)
{
	GtkWidget *toolbar;
	const gchar *setting;
	GList *item_list;

	if (prefs_common.main_toolbar_setting &&
	    *prefs_common.main_toolbar_setting != '\0')
		setting = prefs_common.main_toolbar_setting;
	else
		setting = prefs_toolbar_get_default_main_setting_name_list();

	item_list = prefs_toolbar_get_item_list_from_name_list(setting);
	toolbar = main_window_toolbar_create_from_list(mainwin, item_list);
	g_list_free(item_list);

	return toolbar;
}

static GtkWidget *main_window_toolbar_create_from_list(MainWindow *mainwin,
						       GList *item_list)
{
	GtkWidget *toolbar;
	GtkWidget *icon_wid;
	GtkToolItem *toolitem;
	GtkToolItem *comboitem;
	ComboButton *combo;
	gint n_entries;
	gint i;
	GList *cur;

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
				    GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	g_signal_connect(G_OBJECT(toolbar), "button_press_event",
			 G_CALLBACK(toolbar_button_pressed), mainwin);

	items[0].data = &mainwin->get_btn;
	items[1].data = &mainwin->getall_btn;
	items[2].data = &mainwin->send_btn;
	items[3].data = &mainwin->compose_btn;
	items[4].data = &mainwin->reply_btn;
	items[5].data = &mainwin->replyall_btn;
	items[6].data = &mainwin->fwd_btn;
	items[7].data = &mainwin->delete_btn;
	items[8].data = &mainwin->junk_btn;
	items[9].data = &mainwin->notjunk_btn;
	items[10].data = &mainwin->next_btn;
	items[11].data = &mainwin->prev_btn;
	items[12].data = &mainwin->search_btn;
	items[13].data = &mainwin->print_btn;
	items[14].data = &mainwin->stop_btn;
	items[15].data = &mainwin->address_btn;
	items[16].data = &mainwin->exec_btn;
	items[17].data = &mainwin->prefs_common_btn;
	items[18].data = &mainwin->prefs_account_btn;
	items[19].data = &mainwin->rpop3_btn;
	for (i = 0; i <= 19; i++)
		*(GtkWidget **)items[i].data = NULL;
	mainwin->reply_combo = NULL;
	mainwin->fwd_combo = NULL;

	for (cur = item_list; cur != NULL; cur = cur->next) {
		const PrefsDisplayItem *ditem = cur->data;
		PrefsToolbarItem *item;
		gint width;

		if (ditem->id == T_SEPARATOR) {
			toolitem = gtk_separator_tool_item_new();
			gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
			continue;
		}

		for (item = items; item->id != -1; item++) {
			if (ditem->id == item->id)
				break;
		}
		if (item->id == -1)
			continue;

		icon_wid = stock_pixbuf_widget_for_toolbar(ditem->icon);
		toolitem = gtk_tool_button_new(icon_wid, gettext(ditem->label));
		if (ditem->description) {
			gtk_tool_item_set_tooltip(toolitem,
						  mainwin->toolbar_tip,
						  gettext(ditem->description),
						  ditem->name);
		}

		gtkut_get_str_size(GTK_WIDGET(toolitem), gettext(ditem->label),
				   &width, NULL);
		gtk_tool_item_set_homogeneous
			(toolitem, width < 52 ? TRUE : FALSE);
		gtk_tool_item_set_is_important(toolitem, item->is_important);

		gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);

		g_signal_connect(G_OBJECT(toolitem), "clicked",
				 G_CALLBACK(item->callback), mainwin);
		g_signal_connect(G_OBJECT(GTK_BIN(toolitem)->child),
				 "button_press_event",
				 G_CALLBACK(toolbar_button_pressed), mainwin);

		if (ditem->id == T_REPLY) {
			n_entries = sizeof(reply_entries) /
				sizeof(reply_entries[0]);
			combo = gtkut_combo_button_create
				(GTK_WIDGET(toolitem),
				 reply_entries, n_entries, "<Reply>", mainwin);
			gtk_button_set_relief(GTK_BUTTON(combo->arrow),
					      GTK_RELIEF_NONE);

			comboitem = gtk_tool_item_new();
			gtk_tool_item_set_homogeneous(comboitem, FALSE);
			gtk_container_add(GTK_CONTAINER(comboitem),
					  GTK_WIDGET_PTR(combo));
			if (ditem->description) {
				gtk_tool_item_set_tooltip
					(comboitem, mainwin->toolbar_tip,
					 gettext(ditem->description),
					 ditem->name);
			}

			gtk_toolbar_insert(GTK_TOOLBAR(toolbar), comboitem, -1);

			mainwin->reply_combo = combo;
		} else if (ditem->id == T_FORWARD) {
			n_entries = sizeof(forward_entries) /
				sizeof(forward_entries[0]);
			combo = gtkut_combo_button_create
				(GTK_WIDGET(toolitem),
				 forward_entries, n_entries, "<Forward>",
				 mainwin);
			gtk_button_set_relief(GTK_BUTTON(combo->arrow),
					      GTK_RELIEF_NONE);

			comboitem = gtk_tool_item_new();
			gtk_tool_item_set_homogeneous(comboitem, FALSE);
			gtk_container_add(GTK_CONTAINER(comboitem),
					  GTK_WIDGET_PTR(combo));
			if (ditem->description) {
				gtk_tool_item_set_tooltip
					(comboitem, mainwin->toolbar_tip,
					 gettext(ditem->description),
					 ditem->name);
			}

			gtk_toolbar_insert(GTK_TOOLBAR(toolbar), comboitem, -1);

			mainwin->fwd_combo = combo;
		}

		*(GtkWidget **)item->data = GTK_WIDGET(toolitem);
	}

	gtk_widget_show_all(toolbar);

	return toolbar;
}

static void main_window_toolbar_toggle_menu_set_active(MainWindow *mainwin,
						       ToolbarStyle style)
{
	GtkWidget *menuitem = NULL;
	GtkItemFactory *ifactory = mainwin->menu_factory;

	switch (style) {
	case TOOLBAR_NONE:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/None");
		break;
	case TOOLBAR_ICON:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Icon");
		break;
	case TOOLBAR_TEXT:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Text");
		break;
	case TOOLBAR_BOTH:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Icon and text");
		break;
	case TOOLBAR_BOTH_HORIZ:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Text at the right of icon");
		break;
	}

	if (menuitem)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					       TRUE);
}

/* callback functions */

static void toolbar_inc_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	inc_mail_cb(mainwin, 0, NULL);
}

static void toolbar_inc_all_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	inc_all_account_mail_cb(mainwin, 0, NULL);
}

static void toolbar_rpop3_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	rpop3_cb(mainwin, 0, NULL);
}

static void toolbar_send_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	send_queue_cb(mainwin, 0, NULL);
}

static void toolbar_compose_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	compose_cb(mainwin, 0, NULL);
}

static void toolbar_reply_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (prefs_common.default_reply_list)
		reply_cb(mainwin, COMPOSE_REPLY_TO_LIST, NULL);
	else
		reply_cb(mainwin, COMPOSE_REPLY, NULL);
}

static void toolbar_reply_to_all_cb	(GtkWidget	*widget,
					 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, COMPOSE_REPLY_TO_ALL, NULL);
}

static void toolbar_forward_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, COMPOSE_FORWARD, NULL);
}

static void toolbar_delete_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	summary_delete(mainwin->summaryview);
}

static void toolbar_junk_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	summary_junk(mainwin->summaryview);
}

static void toolbar_notjunk_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	summary_not_junk(mainwin->summaryview);
}

static void toolbar_exec_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	summary_execute(mainwin->summaryview);
}

static void toolbar_next_unread_cb	(GtkWidget	*widget,
					 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	next_unread_cb(mainwin, 0, NULL);
}

static void toolbar_prev_unread_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	prev_unread_cb(mainwin, 0, NULL);
}

static void toolbar_address_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	addressbook_open_cb(mainwin, 0, NULL);
}

static void toolbar_search_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	search_cb(mainwin, 1, NULL);
}

static void toolbar_print_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	print_cb(mainwin, 0, NULL);
}

static void toolbar_stop_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	inc_stop_cb(mainwin, 0, NULL);
}

static void toolbar_prefs_common_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	prefs_common_open_cb(mainwin, 0, NULL);
}

static void toolbar_prefs_account_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	prefs_account_open_cb(mainwin, 0, NULL);
}

static void toolbar_toggle(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	ToolbarStyle style;

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
		return;

	style = (ToolbarStyle)g_object_get_data(G_OBJECT(widget), MENU_VAL_ID);
	main_window_toolbar_toggle_menu_set_active(mainwin, style);
}

static void toolbar_customize(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gint *visible_items;
	GList *item_list = NULL;
	GtkWidget *toolbar;
	gint ret;
	const gchar *setting;

	if (prefs_common.main_toolbar_setting &&
	    *prefs_common.main_toolbar_setting != '\0')
		setting = prefs_common.main_toolbar_setting;
	else
		setting = prefs_toolbar_get_default_main_setting_name_list();
	visible_items = prefs_toolbar_get_id_list_from_name_list(setting);
	ret = prefs_toolbar_open(TOOLBAR_MAIN, visible_items, &item_list);
	g_free(visible_items);

	if (ret == 0) {
		gtk_widget_destroy(mainwin->toolbar);
		toolbar = main_window_toolbar_create_from_list(mainwin,
							       item_list);
		gtk_widget_set_size_request(toolbar, 300, -1);
		gtk_box_pack_start(GTK_BOX(mainwin->vbox), toolbar,
				   FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(mainwin->vbox), toolbar, 1);
		mainwin->toolbar = toolbar;
		main_window_set_toolbar_sensitive(mainwin);
		main_window_set_toolbar_button_visibility(mainwin);
		g_free(prefs_common.main_toolbar_setting);
		prefs_common.main_toolbar_setting =
			prefs_toolbar_get_name_list_from_item_list(item_list);
		g_list_free(item_list);
		prefs_common_write_config();

		syl_plugin_signal_emit("main-window-toolbar-changed");
	}
}

static gboolean toolbar_button_pressed(GtkWidget *widget, GdkEventButton *event,
				       gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menu;
	GtkWidget *menuitem;

	if (!event) return FALSE;
	if (event->button != 3) return FALSE;

	menu = gtk_menu_new();
	gtk_widget_show(menu);

#define SET_TOOLBAR_MENU(text, style, widget)				\
{									\
	MENUITEM_ADD_RADIO(menu, menuitem, widget, text, style);	\
	if (prefs_common.toolbar_style == style)			\
		gtk_check_menu_item_set_active				\
			(GTK_CHECK_MENU_ITEM(menuitem), TRUE);		\
	g_signal_connect(G_OBJECT(menuitem), "activate",		\
			 G_CALLBACK(toolbar_toggle), mainwin);		\
}

	SET_TOOLBAR_MENU(_("Icon _and text"), TOOLBAR_BOTH, NULL);
	SET_TOOLBAR_MENU(_("Text at the _right of icon"), TOOLBAR_BOTH_HORIZ,
			 menuitem);
	SET_TOOLBAR_MENU(_("_Icon"), TOOLBAR_ICON, menuitem);
	SET_TOOLBAR_MENU(_("_Text"), TOOLBAR_TEXT, menuitem);
	SET_TOOLBAR_MENU(_("_None"), TOOLBAR_NONE, menuitem);
	MENUITEM_ADD(menu, menuitem, NULL, NULL);
	MENUITEM_ADD_WITH_MNEMONIC(menu, menuitem, _("_Customize toolbar..."),
				   0);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(toolbar_customize), mainwin);

	g_signal_connect(G_OBJECT(menu), "selection_done",
			 G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		       event->button, event->time);

	return TRUE;
}

static void online_switch_clicked(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menuitem;

	debug_print("Toggle online mode: %d -> %d\n", prefs_common.online_mode,
		    !prefs_common.online_mode);

	menuitem = gtk_item_factory_get_item(mainwin->menu_factory,
					     "/File/Work offline");

	if (prefs_common.online_mode == TRUE) {
		if (folder_remote_folder_active_session_exist()) {
			debug_print("Active session exist. Cancelling online switch.\n");
			return;
		}

		prefs_common.online_mode = FALSE;
		gtk_widget_hide(mainwin->online_pixmap);
		gtk_widget_show(mainwin->offline_pixmap);
		gtk_tooltips_set_tip
			(mainwin->online_tip, mainwin->online_switch,
			 _("You are offline. Click the icon to go online."),
			 NULL);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					       TRUE);
		inc_autocheck_timer_remove();
		folder_remote_folder_destroy_all_sessions();
	} else {
		prefs_common.online_mode = TRUE;
		gtk_widget_hide(mainwin->offline_pixmap);
		gtk_widget_show(mainwin->online_pixmap);
		gtk_tooltips_set_tip
			(mainwin->online_tip, mainwin->online_switch,
			 _("You are online. Click the icon to go offline."),
			 NULL);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					       FALSE);
		inc_autocheck_timer_set();
	}
}

static gboolean ac_label_button_pressed(GtkWidget *widget,
					GdkEventButton *event, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (!event) return FALSE;

	gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
	g_object_set_data(G_OBJECT(mainwin->ac_menu), "menu_button", widget);

	gtk_menu_popup(GTK_MENU(mainwin->ac_menu), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);

	return TRUE;
}

static void ac_menu_popup_closed(GtkMenuShell *menu_shell, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *button;

	button = g_object_get_data(G_OBJECT(menu_shell), "menu_button");
	if (!button) return;
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_object_set_data(G_OBJECT(mainwin->ac_menu), "menu_button", NULL);
	manage_window_focus_in(mainwin->window, NULL, NULL);
}

static gboolean main_window_key_pressed(GtkWidget *widget, GdkEventKey *event,
					gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (!mainwin)
		return FALSE;

	if (!GTK_WIDGET_HAS_FOCUS(mainwin->summaryview->qsearch->entry))
		return FALSE;

	/* g_print("keyval: %d, state: %d\n", event->keyval, event->state); */
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0)
		return FALSE;
	if (event->keyval == GDK_Tab || event->keyval == GDK_KP_Tab ||
	    event->keyval == GDK_ISO_Left_Tab)
		return FALSE;

	gtk_window_propagate_key_event(GTK_WINDOW(widget), event);

	return TRUE;
}

static gint main_window_close_cb(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->lock_count == 0)
		app_exit_cb(data, 0, widget);

	return TRUE;
}

static gint folder_window_close_cb(GtkWidget *widget, GdkEventAny *event,
				   gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menuitem;

	menuitem = gtk_item_factory_get_item
		(mainwin->menu_factory, "/View/Show or hide/Folder tree");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), FALSE);

	return TRUE;
}

static gint message_window_close_cb(GtkWidget *widget, GdkEventAny *event,
				    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menuitem;

	menuitem = gtk_item_factory_get_item
		(mainwin->menu_factory, "/View/Show or hide/Message view");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), FALSE);

	return TRUE;
}

static void main_window_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation,
					 gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	main_window_get_size(mainwin);
}

static void folder_window_size_allocate_cb(GtkWidget *widget,
					   GtkAllocation *allocation,
					   gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	main_window_get_size(mainwin);
}

static void message_window_size_allocate_cb(GtkWidget *widget,
					    GtkAllocation *allocation,
					    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	main_window_get_size(mainwin);
}

static gboolean main_window_window_state_cb(GtkWidget *widget,
					    GdkEventWindowState *event,
					    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gboolean minimized = FALSE;

	debug_print("main_window_window_state_cb\n");

	if ((event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) != 0) {
		if ((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0) {
			debug_print("main_window_window_state_cb: maximized\n");
			prefs_common.mainwin_maximized = TRUE;
		} else {
			debug_print("main_window_window_state_cb: unmaximized\n");
#ifdef G_OS_WIN32
			if ((event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) != 0 &&
			    (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) != 0) {
				debug_print("main_window_window_state_cb: unmaximized by minimize\n");
				/* don't change mainwin_maximized */
			} else {
				prefs_common.mainwin_maximized = FALSE;
			}
#else
			prefs_common.mainwin_maximized = FALSE;
#endif
		}
	}
	if ((event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) != 0) {
		if ((event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) != 0) {
			debug_print("main_window_window_state_cb: iconified\n");
			minimized = TRUE;
			mainwin->window_hidden = TRUE;
		} else {
			debug_print("main_window_window_state_cb: deiconified\n");
			mainwin->window_hidden = FALSE;
		}
	}
	if ((event->changed_mask & GDK_WINDOW_STATE_WITHDRAWN) != 0) {
		if ((event->new_window_state & GDK_WINDOW_STATE_WITHDRAWN) != 0) {
			debug_print("main_window_window_state_cb: withdrawn\n");
			mainwin->window_hidden = TRUE;
		} else {
			debug_print("main_window_window_state_cb: unwithdrawn\n");
			mainwin->window_hidden = FALSE;
		}
	}

#ifdef G_OS_WIN32
	if (minimized &&
	    prefs_common.show_trayicon && prefs_common.minimize_to_tray) {
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(widget), TRUE);
		debug_print("main_window_window_state_cb: hide window\n");
		gtk_widget_hide(widget);
#else
	if (mainwin->window_hidden &&
	    prefs_common.show_trayicon && prefs_common.minimize_to_tray) {
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(widget), TRUE);
#endif
	} else if (!mainwin->window_hidden) {
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(widget), FALSE);
#ifdef G_OS_WIN32
		if (prefs_common.mainwin_maximized)
			gtk_window_maximize(GTK_WINDOW(widget));
#endif
	}

	return FALSE;
}

static gboolean main_window_visibility_notify_cb(GtkWidget *widget,
						 GdkEventVisibility *event,
						 gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	mainwin->window_obscured =
		(event->state == GDK_VISIBILITY_FULLY_OBSCURED ||
		 event->state == GDK_VISIBILITY_PARTIAL) ? TRUE : FALSE;

	return FALSE;
}

static void new_folder_cb(MainWindow *mainwin, guint action,
			  GtkWidget *widget)
{
	folderview_new_folder(mainwin->folderview);
}

static void rename_folder_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	folderview_rename_folder(mainwin->folderview);
}

static void move_folder_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	folderview_move_folder(mainwin->folderview);
}

static void delete_folder_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	folderview_delete_folder(mainwin->folderview);
}

static void add_mailbox_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	main_window_add_mailbox(mainwin);
}

static void remove_mailbox_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	folderview_remove_mailbox(mainwin->folderview);
}

static void update_folderview_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	if (action == 0)
		folderview_check_new_selected(mainwin->folderview);
	else
		folderview_check_new_all();
}

static void rebuild_tree_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	folderview_rebuild_tree(mainwin->folderview);
}

static void import_mail_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	import_mail(mainwin->summaryview->folder_item);
	main_window_popup(mainwin);
}

static void export_mail_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	export_mail(mainwin->summaryview->folder_item);
	main_window_popup(mainwin);
}

static void empty_trash_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	main_window_empty_trash(mainwin, TRUE);
}

static void save_as_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	MessageView *messageview = mainwin->messageview;

	if (messageview_get_selected_mime_part(messageview) &&
	    GTK_WIDGET_HAS_FOCUS(messageview->mimeview->treeview))
		mimeview_save_as(messageview->mimeview);
	else
		summary_save_as(mainwin->summaryview);
}

#if GTK_CHECK_VERSION(2, 10, 0)
static void page_setup_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	printing_page_setup_gtk();
}
#endif

static void print_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_print(mainwin->summaryview);
}

static void toggle_offline_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	if (GTK_CHECK_MENU_ITEM(widget)->active &&
	    folder_remote_folder_active_session_exist()) {
		debug_print("Active session exist. Cancelling online switch.\n");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget),
					       FALSE);
		return;
	}

	main_window_toggle_online
		(mainwin, !GTK_CHECK_MENU_ITEM(widget)->active);
}

static void app_exit_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (prefs_common.confirm_on_exit) {
		if (alertpanel(_("Exit"), _("Exit this program?"),
			       GTK_STOCK_OK, GTK_STOCK_CANCEL, NULL)
		    != G_ALERTDEFAULT)
			return;
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	app_will_exit(FALSE);
}

static void search_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (action == 1) {
		FolderItem *item;

		item = folderview_get_selected_item(mainwin->folderview);
		if (item && item->stype == F_VIRTUAL)
			prefs_search_folder_open(item);
		else
			query_search(item);
	} else if (action == 2) {
		if (!prefs_common.show_searchbar) {
			GtkWidget *menuitem;

			menuitem = gtk_item_factory_get_item
				(mainwin->menu_factory,
				 "/View/Show or hide/Search bar");
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
		}
		gtk_widget_grab_focus(mainwin->summaryview->qsearch->entry);
	} else
		message_search(mainwin->messageview);
}

static void toggle_folder_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	gboolean active;

	active = GTK_CHECK_MENU_ITEM(widget)->active;

	switch (mainwin->type) {
	case SEPARATE_NONE:
	case SEPARATE_MESSAGE:
#if 0
		if (active)
			gtk_widget_show(GTK_WIDGET_PTR(mainwin->folderview));
		else
			gtk_widget_hide(GTK_WIDGET_PTR(mainwin->folderview));
#endif
		break;
	case SEPARATE_FOLDER:
		if (active)
			gtk_widget_show(mainwin->win.sep_folder.folderwin);
		else
			gtk_widget_hide(mainwin->win.sep_folder.folderwin);
		break;
	case SEPARATE_BOTH:
		if (active)
			gtk_widget_show(mainwin->win.sep_both.folderwin);
		else
			gtk_widget_hide(mainwin->win.sep_both.folderwin);
		break;
	}

	prefs_common.folderview_visible = active;
}

static void toggle_message_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	gboolean active;

	active = GTK_CHECK_MENU_ITEM(widget)->active;

	if (active != messageview_is_visible(mainwin->messageview))
		summary_toggle_view(mainwin->summaryview);
}

static void toggle_toolbar_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	mainwin->toolbar_style = (ToolbarStyle)action;
	prefs_common.toolbar_style = (ToolbarStyle)action;

	main_window_set_toolbar_button_visibility(mainwin);
}

static void toggle_searchbar_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(mainwin->summaryview->qsearch->hbox);
		prefs_common.show_searchbar = TRUE;
	} else {
		gtk_widget_hide(mainwin->summaryview->qsearch->hbox);
		summary_qsearch_reset(mainwin->summaryview);
		prefs_common.show_searchbar = FALSE;
	}
}

static void toggle_statusbar_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(mainwin->statusbar);
		prefs_common.show_statusbar = TRUE;
	} else {
		gtk_widget_hide(mainwin->statusbar);
		prefs_common.show_statusbar = FALSE;
	}
}

static void toolbar_customize_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	toolbar_customize(widget, mainwin);
}

static void change_layout_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	LayoutType type = action;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		main_window_change_layout(mainwin, type, mainwin->type);
}

static void separate_widget_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	SeparateType type;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		type = mainwin->type | action;
	else
		type = mainwin->type & ~action;

	main_window_change_layout(mainwin, prefs_common.layout_type, type);

	prefs_common.sep_folder = (type & SEPARATE_FOLDER)  != 0;
	prefs_common.sep_msg    = (type & SEPARATE_MESSAGE) != 0;
}

static void addressbook_open_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	addressbook_open(NULL);
}

static void log_window_show_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	log_window_show(mainwin->logwin);
}

static void inc_mail_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	inc_mail(mainwin);
}

static void inc_all_account_mail_cb(MainWindow *mainwin, guint action,
				    GtkWidget *widget)
{
	inc_all_account_mail(mainwin, FALSE);
}

static void inc_stop_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	inc_cancel_all();
}

static void rpop3_cb(MainWindow	*mainwin, guint action, GtkWidget *widget)
{
	rpop3_account(cur_account);
}

static void send_queue_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	main_window_send_queue(mainwin);
}

static void compose_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	PrefsAccount *ac = NULL;
	FolderItem *item = mainwin->summaryview->folder_item;

	if (item) {
		ac = account_find_from_item(item);
		if (ac && ac->protocol == A_NNTP &&
		    FOLDER_TYPE(item->folder) == F_NEWS) {
			compose_new(ac, item, item->path, NULL);
			return;
		}
	}

	compose_new(ac, item, NULL, NULL);
}

static void reply_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_reply(mainwin->summaryview, (ComposeMode)action);
}

static void move_to_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_move_to(mainwin->summaryview);
}

static void copy_to_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_copy_to(mainwin->summaryview);
}

static void delete_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_delete(mainwin->summaryview);
}

static void open_msg_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_open_msg(mainwin->summaryview);
}

static void view_source_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_view_source(mainwin->summaryview);
}

static void show_all_header_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	if (mainwin->menu_lock_count) return;
	summary_display_msg_selected(mainwin->summaryview, FALSE,
				     GTK_CHECK_MENU_ITEM(widget)->active);
}

static void mark_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_mark(mainwin->summaryview);
}

static void unmark_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_unmark(mainwin->summaryview);
}

static void mark_as_unread_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	summary_mark_as_unread(mainwin->summaryview);
}

static void mark_as_read_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_mark_as_read(mainwin->summaryview);
}

static void mark_thread_as_read_cb(MainWindow *mainwin, guint action,
				   GtkWidget *widget)
{
	summary_mark_thread_as_read(mainwin->summaryview);
}

static void mark_all_read_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	summary_mark_all_read(mainwin->summaryview);
}

static void junk_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (action == 0)
		summary_junk(mainwin->summaryview);
	else
		summary_not_junk(mainwin->summaryview);
}

static void reedit_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_reedit(mainwin->summaryview);
}

static void add_address_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_add_address(mainwin->summaryview);
}

static void set_charset_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	const gchar *str;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		str = conv_get_charset_str((CharSet)action);
		g_free(prefs_common.force_charset);
		prefs_common.force_charset = str ? g_strdup(str) : NULL;

		summary_redisplay_msg(mainwin->summaryview);

		debug_print("forced charset: %s\n",
			    str ? str : "Auto-Detect");
	}
}

static void thread_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (mainwin->menu_lock_count) return;
	if (!mainwin->summaryview->folder_item) return;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		summary_thread_build(mainwin->summaryview);
	else
		summary_unthread(mainwin->summaryview);
}

static void expand_threads_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	summary_expand_threads(mainwin->summaryview);
}

static void collapse_threads_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	summary_collapse_threads(mainwin->summaryview);
}

static void set_display_item_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	prefs_summary_column_open
		(FOLDER_ITEM_IS_SENT_FOLDER(mainwin->summaryview->folder_item));
}

static void sort_summary_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	FolderItem *item = mainwin->summaryview->folder_item;
	GtkWidget *menuitem;

	if (mainwin->menu_lock_count) return;

	if (GTK_CHECK_MENU_ITEM(widget)->active && item) {
		menuitem = gtk_item_factory_get_item
			(mainwin->menu_factory, "/View/Sort/Ascending");
		summary_sort(mainwin->summaryview, (FolderSortKey)action,
			     GTK_CHECK_MENU_ITEM(menuitem)->active
			     ? SORT_ASCENDING : SORT_DESCENDING);
	}
}

static void sort_summary_type_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	FolderItem *item = mainwin->summaryview->folder_item;

	if (mainwin->menu_lock_count) return;

	if (GTK_CHECK_MENU_ITEM(widget)->active && item)
		summary_sort(mainwin->summaryview,
			     item->sort_key, (FolderSortType)action);
}

static void attract_by_subject_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	summary_attract_by_subject(mainwin->summaryview);
}

static void delete_duplicated_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	summary_delete_duplicated(mainwin->summaryview);
}

static void concat_partial_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	GSList *mlist;
	gchar *file;
	FolderItem *item;

	if (summary_is_locked(mainwin->summaryview))
		return;

	item = mainwin->summaryview->folder_item;
	if (!item)
		return;
	mlist = summary_get_selected_msg_list(mainwin->summaryview);
	if (!mlist)
		return;

	file = get_tmp_file();
	if (procmsg_concat_partial_messages(mlist, file) == 0) {
		folder_item_add_msg(item, file, NULL, TRUE);
		summary_show_queued_msgs(mainwin->summaryview);
	} else {
		alertpanel_error
			(_("The selected messages could not be combined."));
	}
	g_free(file);

	g_slist_free(mlist);
}

static void filter_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_filter(mainwin->summaryview, (gboolean)action);
}

static void filter_junk_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_filter_junk(mainwin->summaryview, (gboolean)action);
}

static void execute_summary_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	summary_execute(mainwin->summaryview);
}

static void update_summary_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	if (!mainwin->summaryview->folder_item) return;

	summary_show(mainwin->summaryview, mainwin->summaryview->folder_item,
		     TRUE);
}

static void open_config_folder_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	execute_open_file(get_rc_dir(), NULL);
}

static void open_attachments_folder_cb(MainWindow *mainwin, guint action,
				       GtkWidget *widget)
{
	execute_open_file(get_mime_tmp_dir(), NULL);
}

static void prev_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	MessageView *messageview = mainwin->messageview;

	if (messageview_get_selected_mime_part(messageview) &&
	    GTK_WIDGET_HAS_FOCUS(messageview->mimeview->treeview) &&
	    mimeview_step(messageview->mimeview, GTK_SCROLL_STEP_BACKWARD))
		return;

	if (summary_step(mainwin->summaryview, GTK_SCROLL_STEP_BACKWARD))
		summary_mark_displayed_read(mainwin->summaryview, NULL);
}

static void next_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	MessageView *messageview = mainwin->messageview;

	if (messageview_get_selected_mime_part(messageview) &&
	    GTK_WIDGET_HAS_FOCUS(messageview->mimeview->treeview) &&
	    mimeview_step(messageview->mimeview, GTK_SCROLL_STEP_FORWARD))
		return;

	if (summary_step(mainwin->summaryview, GTK_SCROLL_STEP_FORWARD))
		summary_mark_displayed_read(mainwin->summaryview, NULL);
}

static void prev_unread_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_prev_unread(mainwin->summaryview);
}

static void next_unread_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_next_unread(mainwin->summaryview);
}

static void prev_new_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_select_prev_new(mainwin->summaryview);
}

static void next_new_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_select_next_new(mainwin->summaryview);
}

static void prev_marked_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_prev_marked(mainwin->summaryview);
}

static void next_marked_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_next_marked(mainwin->summaryview);
}

static void prev_labeled_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_select_prev_labeled(mainwin->summaryview);
}

static void next_labeled_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_select_next_labeled(mainwin->summaryview);
}

static void goto_folder_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	FolderItem *to_folder;

	to_folder = foldersel_folder_sel_full(NULL, FOLDER_SEL_ALL, NULL,
					      _("Select folder to open"));

	if (to_folder)
		folderview_select(mainwin->folderview, to_folder);
}

static void copy_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	messageview_copy_clipboard(mainwin->messageview);
}

static void allsel_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	MessageView *msgview = mainwin->messageview;

	if (GTK_WIDGET_HAS_FOCUS(mainwin->summaryview->treeview))
		summary_select_all(mainwin->summaryview);
	else if (GTK_WIDGET_HAS_FOCUS(mainwin->summaryview->qsearch->entry))
		gtk_editable_select_region
			(GTK_EDITABLE(mainwin->summaryview->qsearch->entry),
			 0, -1);
	else if (messageview_is_visible(msgview) &&
		 (GTK_WIDGET_HAS_FOCUS(msgview->textview->text) ||
		  GTK_WIDGET_HAS_FOCUS(msgview->mimeview->textview->text)))
		messageview_select_all(msgview);
}

static void select_thread_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	summary_select_thread(mainwin->summaryview);
}

static void create_filter_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	summary_filter_open(mainwin->summaryview, (FilterCreateType)action);
}

static void prefs_common_open_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	prefs_common_open();
}

static void prefs_filter_open_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	prefs_filter_open(NULL, NULL, NULL);
}

static void prefs_template_open_cb(MainWindow *mainwin, guint action,
				   GtkWidget *widget)
{
	prefs_template_open();
}

static void plugin_manager_open_cb(MainWindow *mainwin, guint action,
				   GtkWidget *widget)
{
	plugin_manager_open();
}

#ifndef G_OS_WIN32
static void prefs_actions_open_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	prefs_actions_open(mainwin);
}
#endif

static void prefs_account_open_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	if (!cur_account) {
		new_account_cb(mainwin, 0, widget);
	} else {
		account_open(cur_account);
	}
}

static void new_account_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	PrefsAccount *ac;

	if (compose_get_compose_list()) {
		alertpanel_notice(_("Some composing windows are open.\n"
				    "Please close all the composing windows before editing the accounts."));
		return;
	}

	if ((ac = setup_account())) {
		account_set_menu();
		main_window_reflect_prefs_all();
		account_set_missing_folder();
		folderview_set(mainwin->folderview);
		if (ac->folder)
			folder_write_list();
	}
}

static void account_selector_menu_cb(GtkMenuItem *menuitem, gpointer data)
{
	cur_account = (PrefsAccount *)data;
	main_window_change_cur_account();
}

static void account_receive_menu_cb(GtkMenuItem *menuitem, gpointer data)
{
	PrefsAccount *account = (PrefsAccount *)data;

	inc_account_mail(main_window_get(), account);
}

static void manual_open_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	manual_open((ManualLang)action);
}

static void faq_open_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	faq_open((ManualLang)action);
}

static GtkWidget *help_cmdline_window;

static void help_cmdline_ok(GtkWidget *button)
{
	gtk_widget_destroy(gtk_widget_get_toplevel(button));
}

static gboolean help_cmdline_key_pressed(GtkWidget *widget, GdkEventKey *event,
					 gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		gtk_widget_destroy(widget);
		return TRUE;
	}
	return FALSE;
}

static gboolean help_cmdline_deleted(GtkWidget *widget, GdkEventAny *event,
				     gpointer data)
{
	return FALSE;
}

static void help_cmdline_destroyed(GtkWidget *widget, gpointer data)
{
	help_cmdline_window = NULL;
}

static void help_command_line_show(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;

	if (help_cmdline_window) {
		gtk_window_present(GTK_WINDOW(help_cmdline_window));
		return;
	}

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Command line options"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
	help_cmdline_window = window;

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	vbox2 = gtk_vbox_new(FALSE, 8);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 8);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);

	label = gtk_label_new(_("Usage: sylpheed [OPTION]..."));
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	hbox = gtk_hbox_new(FALSE, 16);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("--compose [address]\n"
				"--attach file1 [file2]...\n"
				"--receive\n"
				"--receive-all\n"
				"--send\n"
				"--status [folder]...\n"
				"--status-full [folder]...\n"
				"--open folderid/msgnum\n"
				"--open <file URL>\n"
				"--configdir dirname\n"
				"--exit\n"
				"--debug\n"
				"--safe-mode\n"
				"--help\n"
				"--version"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	label = gtk_label_new(_("open composition window\n"
				"open composition window with specified files attached\n"
				"receive new messages\n"
				"receive new messages of all accounts\n"
				"send all queued messages\n"
				"show the total number of messages\n"
				"show the status of each folder\n"
				"open message in new window\n"
				"open an rfc822 message file in a new window\n"
				"specify directory which stores configuration files\n"
				"exit Sylpheed\n"
				"debug mode\n"
				"safe mode\n"
				"display this help and exit\n"
				"output version information and exit"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

#ifdef G_OS_WIN32
	label = gtk_label_new(_("Windows-only option:"));
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	hbox = gtk_hbox_new(FALSE, 32);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("--ipcport portnum"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	label = gtk_label_new(_("specify port for IPC remote commands"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
#endif

	gtkut_stock_button_set_create(&hbbox, &ok_btn, GTK_STOCK_OK,
				      NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(help_cmdline_ok), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(help_cmdline_key_pressed), NULL);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(help_cmdline_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "destroy",
			 G_CALLBACK(help_cmdline_destroyed), NULL);

	gtk_widget_show_all(window);
}

static void help_cmdline_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	help_command_line_show();
}

#if USE_UPDATE_CHECK
static void update_check_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	update_check(TRUE);
}

#ifdef USE_UPDATE_CHECK_PLUGIN
static void update_check_plugin_cb(MainWindow *mainwin, guint action,
				   GtkWidget *widget)
{
	update_check_plugin(TRUE);
}
#endif
#endif

static void scan_tree_func(Folder *folder, FolderItem *item, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gchar *str;

	if (item->path)
		str = g_strdup_printf(_("Scanning folder %s%c%s ..."),
				      LOCAL_FOLDER(folder)->rootpath,
				      G_DIR_SEPARATOR,
				      item->path);
	else
		str = g_strdup_printf(_("Scanning folder %s ..."),
				      LOCAL_FOLDER(folder)->rootpath);

	STATUSBAR_PUSH(mainwin, str);
	STATUSBAR_POP(mainwin);
	g_free(str);
}
