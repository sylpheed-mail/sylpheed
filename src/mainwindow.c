/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
#include <gtk/gtkhandlebox.h>
#include <gtk/gtktoolbar.h>
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
#include "summary_search.h"
#include "messageview.h"
#include "mimeview.h"
#include "message_search.h"
#include "headerview.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "folder.h"
#include "inc.h"
#include "compose.h"
#include "procmsg.h"
#include "import.h"
#include "export.h"
#include "prefs_common.h"
#include "prefs_filter.h"
#include "prefs_actions.h"
#include "prefs_account.h"
#include "prefs_summary_column.h"
#include "prefs_template.h"
#include "action.h"
#include "account.h"
#include "addressbook.h"
#include "logwindow.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "statusbar.h"
#include "inputdialog.h"
#include "utils.h"
#include "gtkutils.h"
#include "codeconv.h"
#include "about.h"
#include "manual.h"
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

static void main_window_set_widgets		(MainWindow	*mainwin,
						 SeparateType	 type);
static void main_window_toolbar_create		(MainWindow	*mainwin,
						 GtkWidget	*container);

/* callback functions */
static void toolbar_inc_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_inc_all_cb		(GtkWidget	*widget,
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
static void toolbar_exec_cb		(GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_next_unread_cb	(GtkWidget	*widget,
					 gpointer	 data);

#if 0
static void toolbar_prefs_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_account_cb		(GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_account_button_pressed	(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
#endif

static void toolbar_child_attached		(GtkWidget	*widget,
						 GtkWidget	*child,
						 gpointer	 data);
static void toolbar_child_detached		(GtkWidget	*widget,
						 GtkWidget	*child,
						 gpointer	 data);

static void online_switch_clicked		(GtkWidget	*widget,
						 gpointer	 data);
static void ac_label_button_pressed		(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
static void ac_menu_popup_closed		(GtkMenuShell	*menu_shell,
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

static void new_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void rename_folder_cb	 (MainWindow	*mainwin,
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

static void import_mbox_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void export_mbox_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void empty_trash_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void save_as_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
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
static void toggle_statusbar_cb	 (MainWindow	*mainwin,
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
static void inc_cancel_cb		(MainWindow	*mainwin,
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
static void filter_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void execute_summary_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void update_summary_cb	 (MainWindow	*mainwin,
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
static void prefs_actions_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
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
	{N_("/_File/_Import mbox file..."),	NULL, import_mbox_cb, 0, NULL},
	{N_("/_File/_Export to mbox file..."),	NULL, export_mbox_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/Empty all _trash"),		NULL, empty_trash_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Save as..."),		"<control>S", save_as_cb, 0, NULL},
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
	{N_("/_View/Show or hi_de/_Toolbar/_Icon"),
						NULL, toggle_toolbar_cb, TOOLBAR_ICON, "/View/Show or hide/Toolbar/Icon and text"},
	{N_("/_View/Show or hi_de/_Toolbar/_Text"),
						NULL, toggle_toolbar_cb, TOOLBAR_TEXT, "/View/Show or hide/Toolbar/Icon and text"},
	{N_("/_View/Show or hi_de/_Toolbar/_None"),
						NULL, toggle_toolbar_cb, TOOLBAR_NONE, "/View/Show or hide/Toolbar/Icon and text"},
	{N_("/_View/Show or hi_de/Status _bar"),
						NULL, toggle_statusbar_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Separate f_older tree"),	NULL, separate_widget_cb, SEPARATE_FOLDER, "<ToggleItem>"},
	{N_("/_View/Separate _message view"),	NULL, separate_widget_cb, SEPARATE_MESSAGE, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Sort"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Sort/by _number"),		NULL, sort_summary_cb, SORT_BY_NUMBER, "<RadioItem>"},
	{N_("/_View/_Sort/by s_ize"),		NULL, sort_summary_cb, SORT_BY_SIZE, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _date"),		NULL, sort_summary_cb, SORT_BY_DATE, "/View/Sort/by number"},
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
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Central European (ISO-8859-_2)"),
	 ENC_ACTION(C_ISO_8859_2)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/_Baltic (ISO-8859-13)"),
	 ENC_ACTION(C_ISO_8859_13)},
	{N_("/_View/Character _encoding/Baltic (ISO-8859-_4)"),
	 ENC_ACTION(C_ISO_8859_4)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Greek (ISO-8859-_7)"),
	 ENC_ACTION(C_ISO_8859_7)},
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
	{N_("/_View/Show all _header"),		"<control>H", show_all_header_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Update summary"),		"<control><alt>U", update_summary_cb,  0, NULL},

	{N_("/_Message"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Recei_ve"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Recei_ve/Get from _current account"),
						"<control>I",	inc_mail_cb, 0, NULL},
	{N_("/_Message/Recei_ve/Get from _all accounts"),
						"<shift><control>I", inc_all_account_mail_cb, 0, NULL},
	{N_("/_Message/Recei_ve/Cancel receivin_g"),
						NULL, inc_cancel_cb, 0, NULL},
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
	{N_("/_Message/_Delete"),		"<control>D", delete_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/_Mark/_Mark"),		"<shift>asterisk", mark_cb, 0, NULL},
	{N_("/_Message/_Mark/_Unmark"),		"U", unmark_cb, 0, NULL},
	{N_("/_Message/_Mark/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark/Mark as unr_ead"),	"<shift>exclam", mark_as_unread_cb, 0, NULL},
	{N_("/_Message/_Mark/Mark as rea_d"),
						NULL, mark_as_read_cb, 0, NULL},
	{N_("/_Message/_Mark/Mark all _read"),	NULL, mark_all_read_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Set as _junk mail"),	"<control>J", junk_cb, 0, NULL},
	{N_("/_Message/Set as not j_unk mail"),	"<shift><control>J", junk_cb, 1, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Re-_edit"),		NULL, reedit_cb, 0, NULL},

	{N_("/_Tools"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Address book"),		"<shift><control>A", addressbook_open_cb, 0, NULL},
	{N_("/_Tools/Add sender to address boo_k"),
						NULL, add_address_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Filter all messages in folder"),
						NULL, filter_cb, 0, NULL},
	{N_("/_Tools/Filter _selected messages"),
						NULL, filter_cb, 1, NULL},
	{N_("/_Tools/_Create filter rule"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Create filter rule/_Automatically"),
						NULL, create_filter_cb, FILTER_BY_AUTO, NULL},
	{N_("/_Tools/_Create filter rule/by _From"),
						NULL, create_filter_cb, FILTER_BY_FROM, NULL},
	{N_("/_Tools/_Create filter rule/by _To"),
						NULL, create_filter_cb, FILTER_BY_TO, NULL},
	{N_("/_Tools/_Create filter rule/by _Subject"),
						NULL, create_filter_cb, FILTER_BY_SUBJECT, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Actio_ns"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Delete du_plicated messages"),
						NULL, delete_duplicated_cb,   0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/E_xecute"),		"X", execute_summary_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Log window"),		"<shift><control>L", log_window_show_cb, 0, NULL},

	{N_("/_Configuration"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Configuration/_Common preferences..."),
						NULL, prefs_common_open_cb, 0, NULL},
	{N_("/_Configuration/_Filter setting..."),
						NULL, prefs_filter_open_cb, 0, NULL},
	{N_("/_Configuration/_Template..."),	NULL, prefs_template_open_cb, 0, NULL},
	{N_("/_Configuration/_Actions..."),	NULL, prefs_actions_open_cb, 0, NULL},
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
	{N_("/_Help/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Help/_About"),			NULL, about_show, 0, NULL}
};

MainWindow *main_window_create(SeparateType type)
{
	MainWindow *mainwin;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *handlebox;
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
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);
	ifactory = gtk_item_factory_from_widget(menubar);

	handlebox = gtk_handle_box_new();
	gtk_widget_show(handlebox);
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(handlebox), "child_attached",
			 G_CALLBACK(toolbar_child_attached), mainwin);
	g_signal_connect(G_OBJECT(handlebox), "child_detached",
			 G_CALLBACK(toolbar_child_detached), mainwin);

	main_window_toolbar_create(mainwin, handlebox);

	/* vbox that contains body */
	vbox_body = gtk_vbox_new(FALSE, BORDER_WIDTH);
	gtk_widget_show(vbox_body);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_body), BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(vbox), vbox_body, TRUE, TRUE, 0);

	statusbar = statusbar_create();
	gtk_box_pack_end(GTK_BOX(vbox_body), statusbar, FALSE, FALSE, 0);

	progressbar = gtk_progress_bar_new();
	gtk_widget_set_size_request(progressbar, 120, 1);
	gtk_box_pack_start(GTK_BOX(statusbar), progressbar, FALSE, FALSE, 0);

	statuslabel = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(statusbar), statuslabel, FALSE, FALSE, 0);

	online_hbox = gtk_hbox_new(FALSE, 0);

	online_pixmap = stock_pixmap_widget(statusbar, STOCK_PIXMAP_ONLINE);
	offline_pixmap = stock_pixmap_widget(statusbar, STOCK_PIXMAP_OFFLINE);
	gtk_box_pack_start(GTK_BOX(online_hbox), online_pixmap,
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(online_hbox), offline_pixmap,
			   FALSE, FALSE, 0);

	online_switch = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(online_switch), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS(online_switch, GTK_CAN_FOCUS);
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

	/* create views */
	mainwin->folderview  = folderview  = folderview_create();
	mainwin->summaryview = summaryview = summary_create();
	mainwin->messageview = messageview = messageview_create();
	mainwin->logwin      = log_window_create();

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
	mainwin->handlebox      = handlebox;
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

	messageview->visible = prefs_common.msgview_visible;

	main_window_set_widgets(mainwin, type);

	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(main_window_size_allocate_cb), mainwin);

	/* set menu items */
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Character encoding/Auto detect");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);

	switch (prefs_common.toolbar_style) {
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
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);

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

	/* create actions menu */
	action_update_mainwin_menu(ifactory, mainwin);

	/* initialize online switch */
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
	GList *cur;
	MainWindow *mainwin;

	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;

		main_window_show_cur_account(mainwin);
		main_window_set_menu_sensitive(mainwin);
		main_window_set_toolbar_sensitive(mainwin);

		if (prefs_common.enable_junk)
			gtk_widget_show(mainwin->junk_btn);
		else
			gtk_widget_hide(mainwin->junk_btn);

		if (prefs_common.immediate_exec)
			gtk_widget_hide(mainwin->exec_btn);
		else
			gtk_widget_show(mainwin->exec_btn);

		headerview_set_visibility(mainwin->messageview->headerview,
					  prefs_common.display_header_pane);

		textview_reflect_prefs(mainwin->messageview->textview);
		textview_reflect_prefs(mainwin->messageview->mimeview->textview);

		summary_redisplay_msg(mainwin->summaryview);
	}
}

void main_window_set_summary_column(void)
{
	GList *cur;
	MainWindow *mainwin;

	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;
		summary_set_column_order(mainwin->summaryview);
	}
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
	GList *cur;
	MainWindow *mainwin;

	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;
		main_window_set_account_selector_menu(mainwin, account_list);
		main_window_set_account_receive_menu(mainwin, account_list);
	}
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

MainWindow *main_window_get(void)
{
	return (MainWindow *)mainwin_list->data;
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

void main_window_separation_change(MainWindow *mainwin, SeparateType type)
{
	GtkWidget *folder_wid  = GTK_WIDGET_PTR(mainwin->folderview);
	GtkWidget *summary_wid = GTK_WIDGET_PTR(mainwin->summaryview);
	GtkWidget *message_wid = GTK_WIDGET_PTR(mainwin->messageview);

	debug_print(_("Changing window separation type from %d to %d\n"),
		    mainwin->type, type);

	if (mainwin->type == type) return;

	/* remove widgets from those containers */
	gtk_widget_ref(folder_wid);
	gtk_widget_ref(summary_wid);
	gtk_widget_ref(message_wid);
	gtkut_container_remove
		(GTK_CONTAINER(folder_wid->parent), folder_wid);
	gtkut_container_remove
		(GTK_CONTAINER(summary_wid->parent), summary_wid);
	gtkut_container_remove
		(GTK_CONTAINER(message_wid->parent), message_wid);

	/* clean containers */
	switch (mainwin->type) {
	case SEPARATE_NONE:
		gtk_widget_destroy(mainwin->win.sep_none.hpaned);
		break;
	case SEPARATE_FOLDER:
		gtk_widget_destroy(mainwin->win.sep_folder.vpaned);
		gtk_widget_destroy(mainwin->win.sep_folder.folderwin);
		break;
	case SEPARATE_MESSAGE:
		gtk_widget_destroy(mainwin->win.sep_message.hpaned);
		gtk_widget_destroy(mainwin->win.sep_message.messagewin);
		break;
	case SEPARATE_BOTH:
		gtk_widget_destroy(mainwin->win.sep_both.messagewin);
		gtk_widget_destroy(mainwin->win.sep_both.folderwin);
		break;
	}

	gtk_widget_hide(mainwin->window);
	main_window_set_widgets(mainwin, type);
	gtk_widget_show(mainwin->window);

	gtk_widget_unref(folder_wid);
	gtk_widget_unref(summary_wid);
	gtk_widget_unref(message_wid);
}

void main_window_toggle_message_view(MainWindow *mainwin)
{
	SummaryView *summaryview = mainwin->summaryview;
	union CompositeWin *cwin = &mainwin->win;
	GtkWidget *vpaned = NULL;
	GtkWidget *container = NULL;
	GtkWidget *msgwin = NULL;

	switch (mainwin->type) {
	case SEPARATE_NONE:
		vpaned = cwin->sep_none.vpaned;
		container = cwin->sep_none.hpaned;
		break;
	case SEPARATE_FOLDER:
		vpaned = cwin->sep_folder.vpaned;
		container = mainwin->vbox_body;
		break;
	case SEPARATE_MESSAGE:
		msgwin = mainwin->win.sep_message.messagewin;
		break;
	case SEPARATE_BOTH:
		msgwin = mainwin->win.sep_both.messagewin;
		break;
	}

	if (msgwin) {
		if (GTK_WIDGET_VISIBLE(msgwin)) {
			gtk_widget_hide(msgwin);
			mainwin->messageview->visible = FALSE;
			summaryview->displayed = NULL;
		} else {
			gtk_widget_show(msgwin);
			mainwin->messageview->visible = TRUE;
		}
	} else if (vpaned->parent != NULL) {
		mainwin->messageview->visible = FALSE;
		summaryview->displayed = NULL;
		gtk_widget_ref(vpaned);
		gtkut_container_remove(GTK_CONTAINER(container), vpaned);
		gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), container);
	} else {
		mainwin->messageview->visible = TRUE;
		gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), vpaned);
		gtk_container_add(GTK_CONTAINER(container), vpaned);
		gtk_widget_unref(vpaned);
	}

	if (messageview_is_visible(mainwin->messageview))
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	else
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      GTK_ARROW_UP, GTK_SHADOW_OUT);

	if (mainwin->messageview->visible == FALSE)
		messageview_clear(mainwin->messageview);

	main_window_set_menu_sensitive(mainwin);

	prefs_common.msgview_visible = mainwin->messageview->visible;

	gtk_widget_grab_focus(summaryview->treeview);
}

void main_window_get_size(MainWindow *mainwin)
{
	GtkAllocation *allocation;

	allocation = &(GTK_WIDGET_PTR(mainwin->summaryview)->allocation);

	if (allocation->width > 1 && allocation->height > 1) {
		prefs_common.summaryview_width = allocation->width;

		if ((mainwin->type == SEPARATE_NONE ||
		     mainwin->type == SEPARATE_FOLDER) &&
		    messageview_is_visible(mainwin->messageview))
			prefs_common.summaryview_height = allocation->height;

		prefs_common.mainview_width = allocation->width;
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
		prefs_common.msgview_width = allocation->width;
		prefs_common.msgview_height = allocation->height;
	}

#if 0
	debug_print("summaryview size: %d x %d\n",
		    prefs_common.summaryview_width,
		    prefs_common.summaryview_height);
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

	if (confirm) {
		if (alertpanel(_("Empty all trash"),
			       _("Empty messages in all trash?"),
			       GTK_STOCK_YES, GTK_STOCK_NO, NULL)
		    != G_ALERTDEFAULT)
			return;
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	procmsg_empty_all_trash();
	statusbar_pop_all();

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

	path = input_dialog(_("Add mailbox"),
			    _("Input the location of mailbox.\n"
			      "If the existing mailbox is specified, it will be\n"
			      "scanned automatically."),
			    "Mail");
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

	M_FOLDER_NEWOK	      = 1 << 17,
	M_FOLDER_RENOK	      = 1 << 18,
	M_FOLDER_DELOK	      = 1 << 19,
	M_MBOX_ADDOK	      = 1 << 20,
	M_MBOX_RMOK	      = 1 << 21,
	M_MBOX_CHKOK	      = 1 << 22,
	M_MBOX_CHKALLOK	      = 1 << 23,
	M_MBOX_REBUILDOK      = 1 << 24
} SensitiveCond;

static SensitiveCond main_window_get_current_state(MainWindow *mainwin)
{
	SensitiveCond state = 0;
	SummarySelection selection;
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
	    (item &&
	     (item->stype == F_OUTBOX || item->stype == F_DRAFT ||
	      item->stype == F_QUEUE)))
		state |= M_ALLOW_REEDIT;
	if (cur_account)
		state |= M_HAVE_ACCOUNT;

	if (inc_is_active())
		state |= M_INC_ACTIVE;

	if (prefs_common.enable_junk)
		state |= M_ENABLE_JUNK;

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
	gboolean sensitive;
	gint i = 0;

	struct {
		GtkWidget *widget;
		SensitiveCond cond;
	} entry[13];

#define SET_WIDGET_COND(w, c)	\
{				\
	entry[i].widget = w;	\
	entry[i].cond = c;	\
	i++;			\
}

	SET_WIDGET_COND(mainwin->get_btn, M_HAVE_ACCOUNT|M_UNLOCKED);
	SET_WIDGET_COND(mainwin->getall_btn, M_HAVE_ACCOUNT|M_UNLOCKED);
	SET_WIDGET_COND(mainwin->compose_btn, M_HAVE_ACCOUNT);
	SET_WIDGET_COND(mainwin->reply_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(GTK_WIDGET_PTR(mainwin->reply_combo),
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->replyall_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->fwd_btn, M_HAVE_ACCOUNT|M_TARGET_EXIST);
	SET_WIDGET_COND(GTK_WIDGET_PTR(mainwin->fwd_combo),
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
#if 0
	SET_WIDGET_COND(mainwin->prefs_btn, M_UNLOCKED);
	SET_WIDGET_COND(mainwin->account_btn, M_UNLOCKED);
#endif
	SET_WIDGET_COND(mainwin->next_btn, M_MSG_EXIST);
	SET_WIDGET_COND(mainwin->delete_btn,
			M_TARGET_EXIST|M_ALLOW_DELETE);
	SET_WIDGET_COND(mainwin->junk_btn,
			M_TARGET_EXIST|M_ALLOW_DELETE|M_ENABLE_JUNK);
	SET_WIDGET_COND(mainwin->exec_btn, M_MSG_EXIST|M_EXEC);
	SET_WIDGET_COND(NULL, 0);

#undef SET_WIDGET_COND

	state = main_window_get_current_state(mainwin);

	for (i = 0; entry[i].widget != NULL; i++) {
		sensitive = ((entry[i].cond & state) == entry[i].cond);
		gtk_widget_set_sensitive(entry[i].widget, sensitive);
	}
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
		{"/File/Folder/Delete folder"       , M_UNLOCKED|M_FOLDER_DELOK},
		{"/File/Mailbox/Add mailbox..."     , M_UNLOCKED|M_MBOX_ADDOK},
		{"/File/Mailbox/Remove mailbox"     , M_UNLOCKED|M_MBOX_RMOK},
		{"/File/Mailbox/Check for new messages"
						    , M_UNLOCKED|M_MBOX_CHKOK},
		{"/File/Mailbox/Check for new messages in all mailboxes"
						    , M_UNLOCKED|M_MBOX_CHKALLOK},
		{"/File/Mailbox/Rebuild folder tree", M_UNLOCKED|M_MBOX_REBUILDOK},
		{"/File/Import mbox file..."        , M_UNLOCKED},
		{"/File/Export to mbox file..."     , M_UNLOCKED},
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
		{"/View/Show all header"           , M_SINGLE_TARGET_EXIST},
		{"/View/Message source"            , M_SINGLE_TARGET_EXIST},

		{"/Message/Receive/Get from current account"
						 , M_HAVE_ACCOUNT|M_UNLOCKED},
		{"/Message/Receive/Get from all accounts"
						 , M_HAVE_ACCOUNT|M_UNLOCKED},
		{"/Message/Receive/Cancel receiving"
						 , M_INC_ACTIVE},

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

		{"/Tools/Add sender to address book"   , M_SINGLE_TARGET_EXIST},
		{"/Tools/Filter all messages in folder", M_MSG_EXIST|M_EXEC},
		{"/Tools/Filter selected messages"     , M_TARGET_EXIST|M_EXEC},
		{"/Tools/Create filter rule"           , M_SINGLE_TARGET_EXIST|M_UNLOCKED},
		{"/Tools/Actions"                      , M_TARGET_EXIST|M_UNLOCKED},
		{"/Tools/Execute"                      , M_MSG_EXIST|M_EXEC},
		{"/Tools/Delete duplicated messages"   , M_MSG_EXIST|M_ALLOW_DELETE},

		{"/Configuration", M_UNLOCKED},

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

	SET_CHECK_MENU_ACTIVE("/View/Show all header",
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
		gtkut_window_popup(mainwin->win.sep_folder.folderwin);
		break;
	case SEPARATE_MESSAGE:
		gtkut_window_popup(mainwin->win.sep_message.messagewin);
		break;
	case SEPARATE_BOTH:
		gtkut_window_popup(mainwin->win.sep_both.folderwin);
		gtkut_window_popup(mainwin->win.sep_both.messagewin);
		break;
	default:
		break;
	}
}

static void main_window_set_widgets(MainWindow *mainwin, SeparateType type)
{
	GtkWidget *folderwin = NULL;
	GtkWidget *messagewin = NULL;
	GtkWidget *hpaned;
	GtkWidget *vpaned;
	GtkWidget *vbox_body = mainwin->vbox_body;
	GtkItemFactory *ifactory = mainwin->menu_factory;
	GtkWidget *menuitem;

	debug_print("Setting widgets... ");

	gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->folderview),
				    prefs_common.folderview_width,
				    prefs_common.folderview_height);
	gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->summaryview),
				    prefs_common.summaryview_width,
				    prefs_common.summaryview_height);
	gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->messageview),
				    prefs_common.msgview_width,
				    prefs_common.msgview_height);

	/* create separated window(s) if needed */
	if (type & SEPARATE_FOLDER) {
		folderwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(folderwin),
				     _("Sylpheed - Folder View"));
		gtk_window_set_wmclass(GTK_WINDOW(folderwin),
				       "folder_view", "Sylpheed");
		gtk_window_set_policy(GTK_WINDOW(folderwin),
				      TRUE, TRUE, FALSE);
		gtk_widget_set_uposition(folderwin, prefs_common.folderwin_x,
					 prefs_common.folderwin_y);
		gtk_container_set_border_width(GTK_CONTAINER(folderwin),
					       BORDER_WIDTH);
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
		gtk_widget_set_uposition(messagewin, prefs_common.main_msgwin_x,
					 prefs_common.main_msgwin_y);
		gtk_container_set_border_width(GTK_CONTAINER(messagewin),
					       BORDER_WIDTH);
		g_signal_connect(G_OBJECT(messagewin), "delete_event",
				 G_CALLBACK(message_window_close_cb), mainwin);
		gtk_container_add(GTK_CONTAINER(messagewin),
				  GTK_WIDGET_PTR(mainwin->messageview));
		gtk_widget_realize(messagewin);
		if (messageview_is_visible(mainwin->messageview))
			gtk_widget_show(messagewin);
	}

	switch (type) {
	case SEPARATE_NONE:
		hpaned = gtk_hpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox_body), hpaned, TRUE, TRUE, 0);
		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));
		gtk_widget_show(hpaned);
		gtk_widget_queue_resize(hpaned);

		vpaned = gtk_vpaned_new();
		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_add2(GTK_PANED(hpaned), vpaned);
			gtk_paned_add1(GTK_PANED(vpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
		} else {
			gtk_paned_add2(GTK_PANED(hpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
			gtk_widget_ref(vpaned);
		}
		gtk_paned_add2(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));
		gtk_widget_show(vpaned);
		gtk_widget_queue_resize(vpaned);

		mainwin->win.sep_none.hpaned = hpaned;
		mainwin->win.sep_none.vpaned = vpaned;
		break;
	case SEPARATE_FOLDER:
		vpaned = gtk_vpaned_new();
		if (messageview_is_visible(mainwin->messageview)) {
			gtk_box_pack_start(GTK_BOX(vbox_body), vpaned,
					   TRUE, TRUE, 0);
			gtk_paned_add1(GTK_PANED(vpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
		} else {
			gtk_box_pack_start(GTK_BOX(vbox_body),
					   GTK_WIDGET_PTR(mainwin->summaryview),
					   TRUE, TRUE, 0);
			gtk_widget_ref(vpaned);
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
		gtk_paned_add2(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->summaryview));
		gtk_widget_show(hpaned);
		gtk_widget_queue_resize(hpaned);

		mainwin->win.sep_message.messagewin = messagewin;
		mainwin->win.sep_message.hpaned     = hpaned;

		break;
	case SEPARATE_BOTH:
		gtk_box_pack_start(GTK_BOX(vbox_body),
				   GTK_WIDGET_PTR(mainwin->summaryview),
				   TRUE, TRUE, 0);

		mainwin->win.sep_both.folderwin = folderwin;
		mainwin->win.sep_both.messagewin = messagewin;

		break;
	}

	if (messageview_is_visible(mainwin->messageview))
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	else
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      GTK_ARROW_UP, GTK_SHADOW_OUT);

	gtk_widget_set_uposition(mainwin->window,
				 prefs_common.mainwin_x,
				 prefs_common.mainwin_y);

	gtk_widget_queue_resize(vbox_body);
	gtk_widget_queue_resize(mainwin->vbox);
	gtk_widget_queue_resize(mainwin->window);

	mainwin->type = type;

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

static void main_window_toolbar_create(MainWindow *mainwin,
				       GtkWidget *container)
{
	GtkWidget *toolbar;
	GtkWidget *icon_wid;
	GtkWidget *get_btn;
	GtkWidget *getall_btn;
	GtkWidget *send_btn;
	GtkWidget *compose_btn;
	GtkWidget *reply_btn;
	ComboButton *reply_combo;
	GtkWidget *replyall_btn;
	GtkWidget *fwd_btn;
	ComboButton *fwd_combo;
#if 0
	GtkWidget *prefs_btn;
	GtkWidget *account_btn;
#endif
	GtkWidget *next_btn;
	GtkWidget *delete_btn;
	GtkWidget *junk_btn;
	GtkWidget *exec_btn;

	gint n_entries;

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
				    GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
				  GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_container_add(GTK_CONTAINER(container), toolbar);
	gtk_widget_set_size_request(toolbar, 1, -1);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_RECEIVE);
	get_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Get"),
					  _("Incorporate new mail"),
					  "Get",
					  icon_wid,
					  G_CALLBACK(toolbar_inc_cb),
					  mainwin);
	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_RECEIVE_ALL);
	getall_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					     _("Get all"),
					     _("Incorporate new mail of all accounts"),
					     "Get all",
					     icon_wid,
					     G_CALLBACK(toolbar_inc_all_cb),
					     mainwin);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_SEND);
	send_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Send"),
					   _("Send queued message(s)"),
					   "Send",
					   icon_wid,
					   G_CALLBACK(toolbar_send_cb),
					   mainwin);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_COMPOSE);
	compose_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					      _("Compose"),
					      _("Compose new message"),
					      "New",
					      icon_wid,
					      G_CALLBACK(toolbar_compose_cb),
					      mainwin);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_REPLY);
	reply_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					    _("Reply"),
					    _("Reply to the message"),
					    "Reply",
					    icon_wid,
					    G_CALLBACK(toolbar_reply_cb),
					    mainwin);

	n_entries = sizeof(reply_entries) / sizeof(reply_entries[0]);
	reply_combo = gtkut_combo_button_create(reply_btn,
						reply_entries, n_entries,
						"<Reply>", mainwin);
	gtk_button_set_relief(GTK_BUTTON(reply_combo->arrow), GTK_RELIEF_NONE);
	gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar),
				  GTK_WIDGET_PTR(reply_combo),
				  _("Reply to the message"), "Reply");

	icon_wid = stock_pixmap_widget
		(container, STOCK_PIXMAP_MAIL_REPLY_TO_ALL);
	replyall_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					       _("Reply all"),
					       _("Reply to all"),
					       "Reply to all",
					       icon_wid,
					       G_CALLBACK(toolbar_reply_to_all_cb),
					       mainwin);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_FORWARD);
	fwd_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Forward"),
					  _("Forward the message"),
					  "Fwd",
					  icon_wid,
					  G_CALLBACK(toolbar_forward_cb),
					  mainwin);

	n_entries = sizeof(forward_entries) / sizeof(forward_entries[0]);
	fwd_combo = gtkut_combo_button_create(fwd_btn,
					      forward_entries, n_entries,
					      "<Forward>", mainwin);
	gtk_button_set_relief(GTK_BUTTON(fwd_combo->arrow), GTK_RELIEF_NONE);
	gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar),
				  GTK_WIDGET_PTR(fwd_combo),
				  _("Forward the message"), "Fwd");

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = gtk_image_new_from_stock(GTK_STOCK_DELETE,
					    GTK_ICON_SIZE_SMALL_TOOLBAR);
	delete_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Delete"),
					  _("Delete the message"),
					  "Delete",
					  icon_wid,
					  G_CALLBACK(toolbar_delete_cb),
					  mainwin);

	icon_wid = gtk_image_new_from_stock(GTK_STOCK_CANCEL,
					    GTK_ICON_SIZE_SMALL_TOOLBAR);
	junk_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Junk"),
					   _("Set as junk mail"),
					   "Junk",
					   icon_wid,
					   G_CALLBACK(toolbar_junk_cb),
					   mainwin);

	icon_wid = gtk_image_new_from_stock(GTK_STOCK_EXECUTE,
					    GTK_ICON_SIZE_SMALL_TOOLBAR);
	exec_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Execute"),
					   _("Execute marked process"),
					   "Execute",
					   icon_wid,
					   G_CALLBACK(toolbar_exec_cb),
					   mainwin);

	icon_wid = gtk_image_new_from_stock(GTK_STOCK_GO_DOWN,
					    GTK_ICON_SIZE_SMALL_TOOLBAR);
	next_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Next"),
					   _("Next unread message"),
					   "Next unread",
					   icon_wid,
					   G_CALLBACK(toolbar_next_unread_cb),
					   mainwin);

#if 0
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_PREFERENCES);
	prefs_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					    _("Prefs"),
					    _("Common preferences"),
					    "Prefs",
					    icon_wid,
					    G_CALLBACK(toolbar_prefs_cb),
					    mainwin);
	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_PROPERTIES);
	account_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					      _("Account"),
					      _("Account setting"),
					      "Account",
					      icon_wid,
					      G_CALLBACK(toolbar_account_cb),
					      mainwin);
	g_signal_connect(G_OBJECT(account_btn), "button_press_event",
			 G_CALLBACK(toolbar_account_button_pressed), mainwin);
#endif

	mainwin->toolbar      = toolbar;
	mainwin->get_btn      = get_btn;
	mainwin->getall_btn   = getall_btn;
	mainwin->compose_btn  = compose_btn;
	mainwin->reply_btn    = reply_btn;
	mainwin->reply_combo  = reply_combo;
	mainwin->replyall_btn = replyall_btn;
	mainwin->fwd_btn      = fwd_btn;
	mainwin->fwd_combo    = fwd_combo;
	mainwin->send_btn     = send_btn;
#if 0
	mainwin->prefs_btn    = prefs_btn;
	mainwin->account_btn  = account_btn;
#endif
	mainwin->next_btn     = next_btn;
	mainwin->delete_btn   = delete_btn;
	mainwin->junk_btn     = junk_btn;
	mainwin->exec_btn     = exec_btn;

	gtk_widget_show_all(toolbar);
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

#if 0
static void toolbar_prefs_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	prefs_common_open();
}

static void toolbar_account_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	prefs_account_open_cb(mainwin, 0, NULL);
}

static void toolbar_account_button_pressed(GtkWidget *widget,
					   GdkEventButton *event,
					   gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (!event) return;
	if (event->button != 3) return;

	gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
	g_object_set_data(G_OBJECT(mainwin->ac_menu), "menu_button", widget);

	gtk_menu_popup(GTK_MENU(mainwin->ac_menu), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
}
#endif

static void toolbar_child_attached(GtkWidget *widget, GtkWidget *child,
				   gpointer data)
{
	gtk_widget_set_size_request(child, 1, -1);
}

static void toolbar_child_detached(GtkWidget *widget, GtkWidget *child,
				   gpointer data)
{
	gtk_widget_set_size_request(child, -1, -1);
}

static void online_switch_clicked(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menuitem;

	menuitem = gtk_item_factory_get_item(mainwin->menu_factory,
					     "/File/Work offline");

	if (prefs_common.online_mode == TRUE) {
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

static void ac_label_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (!event) return;

	gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
	g_object_set_data(G_OBJECT(mainwin->ac_menu), "menu_button", widget);

	gtk_menu_popup(GTK_MENU(mainwin->ac_menu), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
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

static void import_mbox_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	import_mbox(mainwin->summaryview->folder_item);
}

static void export_mbox_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	export_mbox(mainwin->summaryview->folder_item);
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

static void print_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_print(mainwin->summaryview);
}

static void toggle_offline_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
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

	app_will_exit(widget, mainwin);
}

static void search_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (action == 1)
		summary_search(mainwin->summaryview);
	else
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
	switch ((ToolbarStyle)action) {
	case TOOLBAR_NONE:
		gtk_widget_hide(mainwin->handlebox);
	case TOOLBAR_ICON:
		gtk_toolbar_set_style(GTK_TOOLBAR(mainwin->toolbar),
				      GTK_TOOLBAR_ICONS);
		break;
	case TOOLBAR_TEXT:
		gtk_toolbar_set_style(GTK_TOOLBAR(mainwin->toolbar),
				      GTK_TOOLBAR_TEXT);
		break;
	case TOOLBAR_BOTH:
		gtk_toolbar_set_style(GTK_TOOLBAR(mainwin->toolbar),
				      GTK_TOOLBAR_BOTH);
		break;
	}

	if (action != TOOLBAR_NONE) {
		gtk_widget_show(mainwin->handlebox);
		gtk_widget_queue_resize(mainwin->handlebox);
	}

	mainwin->toolbar_style = (ToolbarStyle)action;
	prefs_common.toolbar_style = (ToolbarStyle)action;
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

static void separate_widget_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	SeparateType type;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		type = mainwin->type | action;
	else
		type = mainwin->type & ~action;

	main_window_separation_change(mainwin, type);

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

static void inc_cancel_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	inc_cancel_all();
}

static void send_queue_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	GList *list;

	if (!main_window_toggle_online_if_offline(mainwin))
		return;

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (folder->queue) {
			gint ret;

			ret = procmsg_send_queue(folder->queue,
						 prefs_common.savemsg,
						 prefs_common.filter_sent);
			statusbar_pop_all();
			if (ret > 0)
				folder_item_scan(folder->queue);
		}
	}

	folderview_update_all_updated(TRUE);
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
	prefs_summary_column_open();
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

static void filter_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_filter(mainwin->summaryview, (gboolean)action);
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

static void prev_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	MessageView *messageview = mainwin->messageview;

	if (messageview_get_selected_mime_part(messageview) &&
	    GTK_WIDGET_HAS_FOCUS(messageview->mimeview->treeview) &&
	    mimeview_step(messageview->mimeview, GTK_SCROLL_STEP_BACKWARD))
		return;

	summary_step(mainwin->summaryview, GTK_SCROLL_STEP_BACKWARD);
}

static void next_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	MessageView *messageview = mainwin->messageview;

	if (messageview_get_selected_mime_part(messageview) &&
	    GTK_WIDGET_HAS_FOCUS(messageview->mimeview->treeview) &&
	    mimeview_step(messageview->mimeview, GTK_SCROLL_STEP_FORWARD))
		return;

	summary_step(mainwin->summaryview, GTK_SCROLL_STEP_FORWARD);
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

	to_folder = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL);

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
	summary_filter_open(mainwin->summaryview, (PrefsFilterType)action);
}

static void prefs_common_open_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	prefs_common_open();
}

static void prefs_filter_open_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	prefs_filter_open(NULL, NULL);
}

static void prefs_template_open_cb(MainWindow *mainwin, guint action,
				   GtkWidget *widget)
{
	prefs_template_open();
}

static void prefs_actions_open_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	prefs_actions_open(mainwin);
}

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
	account_edit_open();
	if (!compose_get_compose_list()) account_add();
}

static void account_selector_menu_cb(GtkMenuItem *menuitem, gpointer data)
{
	cur_account = (PrefsAccount *)data;
	main_window_reflect_prefs_all();
}

static void account_receive_menu_cb(GtkMenuItem *menuitem, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)mainwin_list->data;
	PrefsAccount *account = (PrefsAccount *)data;

	inc_account_mail(mainwin, account);
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
