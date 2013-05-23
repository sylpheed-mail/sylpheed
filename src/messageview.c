/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
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
#include <gtk/gtknotebook.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkarrow.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "main.h"
#include "messageview.h"
#include "message_search.h"
#include "headerview.h"
#include "textview.h"
#include "imageview.h"
#include "mimeview.h"
#include "menu.h"
#include "about.h"
#include "filesel.h"
#include "sourcewindow.h"
#include "addressbook.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "manage_window.h"
#include "printing.h"
#include "procmsg.h"
#include "procheader.h"
#include "procmime.h"
#include "account.h"
#include "action.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_filter.h"
#include "filter.h"
#include "gtkutils.h"
#include "utils.h"
#include "rfc2015.h"
#include "plugin.h"

static GList *messageview_list = NULL;

static void messageview_set_mime_view_layout
					(MessageView		*messageview);
static void messageview_change_view_type(MessageView		*messageview,
					 MessageType		 type);
static void messageview_set_tool_menu	(MessageView		*messageview,
					 MimeInfo		*mimeinfo);
static void messageview_set_menu_state	(MessageView		*messageview);

static void messageview_set_encoding_menu
					(MessageView		*messageview);

static gint messageview_delete_cb	(GtkWidget		*widget,
					 GdkEventAny		*event,
					 MessageView		*messageview);
static void messageview_size_allocate_cb(GtkWidget		*widget,
					 GtkAllocation		*allocation);
static void messageview_toggle_attach_cb(GtkToggleButton	*toggle,
					 MessageView		*messageview);
static void messageview_switch_page_cb	(GtkNotebook		*notebook,
					 GtkNotebookPage	*page,
					 guint			 page_num,
					 MessageView		*messageview);
static gint messageview_menu_tool_btn_pressed
					(GtkWidget		*widget,
					 GdkEventButton		*event,
					 MessageView		*messageview);
static void messageview_open_file_cb	(GtkWidget		*widget,
					 MessageView		*messageview);
static void messageview_save_all_cb	(GtkWidget		*widget,
					 MessageView		*messageview);
static gboolean key_pressed		(GtkWidget		*widget,
					 GdkEventKey		*event,
					 MessageView		*messageview);

static void save_as_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
#if GTK_CHECK_VERSION(2, 10, 0)
static void page_setup_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
#endif
static void print_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void close_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void copy_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void allsel_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void search_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void set_charset_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void view_source_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void show_all_header_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void reply_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void reedit_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void addressbook_open_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void add_address_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void create_filter_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void about_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static GtkItemFactoryEntry msgview_entries[] =
{
	{N_("/_File"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Save as..."),	NULL, save_as_cb, 0, NULL},
	{N_("/_File/---"),		NULL, NULL, 0, "<Separator>"},
#if GTK_CHECK_VERSION(2, 10, 0)
	{N_("/_File/Page set_up..."),	NULL, page_setup_cb, 0, NULL},
#endif
	{N_("/_File/_Print..."),	NULL, print_cb, 0, NULL},
	{N_("/_File/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Close"),		NULL, close_cb, 0, NULL},

	{N_("/_Edit"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/_Copy"),		NULL, copy_cb, 0, NULL},
	{N_("/_Edit/Select _all"),	NULL, allsel_cb, 0, NULL},
	{N_("/_Edit/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Find in current message..."),
					NULL, search_cb, 0, NULL},

	{N_("/_View"),			NULL, NULL, 0, "<Branch>"},

#define ENC_SEPARATOR \
	{N_("/_View/Character _encoding/---"),	NULL, NULL, 0, "<Separator>"}
#define ENC_ACTION(action) \
	NULL, set_charset_cb, action, "/View/Character encoding/Auto detect"

	{N_("/_View/Character _encoding"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Character _encoding/_Auto detect"),
					NULL, set_charset_cb, C_AUTO, "<RadioItem>"},
	ENC_SEPARATOR,
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

#undef ENC_SEPARATOR
#undef ENC_ACTION

	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Mess_age source"),	NULL, view_source_cb, 0, NULL},
	{N_("/_View/All _headers"),
					NULL, show_all_header_cb, 0, "<ToggleItem>"},

	{N_("/_Message"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Compose _new message"),
					NULL, compose_cb, 0, NULL},
	{N_("/_Message/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Reply"),	NULL, reply_cb, COMPOSE_REPLY, NULL},
	{N_("/_Message/Repl_y to/_all"),
					NULL, reply_cb, COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/_Message/Repl_y to/_sender"),
					NULL, reply_cb, COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/_Message/Repl_y to/mailing _list"),
					NULL, reply_cb, COMPOSE_REPLY_TO_LIST, NULL},
	{N_("/_Message/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Forward"),	NULL, reply_cb, COMPOSE_FORWARD, NULL},
	{N_("/_Message/For_ward as attachment"),
					NULL, reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/_Message/Redirec_t"),	NULL, reply_cb, COMPOSE_REDIRECT, NULL},
	{N_("/_Message/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Re-_edit"),	NULL, reedit_cb, 0, NULL},

	{N_("/_Tools"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Address book"),	NULL, addressbook_open_cb, 0, NULL},
	{N_("/_Tools/Add sender to address boo_k"),
					NULL, add_address_cb, 0, NULL},
	{N_("/_Tools/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Create filter rule"),
					NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Create filter rule/_Automatically"),
					NULL, create_filter_cb, FLT_BY_AUTO, NULL},
	{N_("/_Tools/_Create filter rule/by _From"),
					NULL, create_filter_cb, FLT_BY_FROM, NULL},
	{N_("/_Tools/_Create filter rule/by _To"),
					NULL, create_filter_cb, FLT_BY_TO, NULL},
	{N_("/_Tools/_Create filter rule/by _Subject"),
					NULL, create_filter_cb, FLT_BY_SUBJECT, NULL},
#ifndef G_OS_WIN32
	{N_("/_Tools/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Actio_ns"),	NULL, NULL, 0, "<Branch>"},
#endif

	{N_("/_Help"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_About"),		NULL, about_cb, 0, NULL}
};


MessageView *messageview_create(void)
{
	MessageView *messageview;
	GtkWidget *vbox;
	GtkWidget *notebook;
	GtkWidget *hbox;
	GtkWidget *toolbar_vbox;
	GtkWidget *toolbar_hbox;
	GtkWidget *image;
	GtkWidget *arrow;
	GtkWidget *mime_toggle_btn;
	GtkWidget *menu_tool_btn;
	GtkWidget *tool_menu;
	GtkWidget *menuitem;
	HeaderView *headerview;
	TextView *textview;
	MimeView *mimeview;
	GtkTooltips *tip;

	debug_print(_("Creating message view...\n"));
	messageview = g_new0(MessageView, 1);

	messageview->type = MVIEW_TEXT;

	headerview = headerview_create();

	textview = textview_create();
	textview->messageview = messageview;

	mimeview = mimeview_create();
	mimeview->textview = textview_create();
	mimeview->textview->messageview = messageview;
	mimeview->imageview = imageview_create();
	mimeview->imageview->messageview = messageview;
	mimeview->messageview = messageview;

	notebook = gtk_notebook_new();
	gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
	gtk_widget_show(notebook);

	gtk_container_add(GTK_CONTAINER(notebook), GTK_WIDGET_PTR(textview));
	gtk_notebook_set_tab_label_text
		(GTK_NOTEBOOK(notebook), GTK_WIDGET_PTR(textview), _("Text"));

	gtk_container_add(GTK_CONTAINER(notebook), GTK_WIDGET_PTR(mimeview));
	gtk_notebook_set_tab_label_text(GTK_NOTEBOOK(notebook),
					GTK_WIDGET_PTR(mimeview),
					_("Attachments"));

	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
	gtk_widget_show_all(notebook);

	/* Attachment toolbar */
	toolbar_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(toolbar_vbox);

	toolbar_hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(toolbar_hbox);
	gtk_box_pack_start(GTK_BOX(toolbar_vbox), toolbar_hbox, TRUE, FALSE, 0);

	mime_toggle_btn = gtk_toggle_button_new();
	gtk_widget_show(mime_toggle_btn);
	gtk_box_pack_start(GTK_BOX(toolbar_hbox), mime_toggle_btn,
			   FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(mime_toggle_btn), "toggled",
			 G_CALLBACK(messageview_toggle_attach_cb), messageview);

	image = stock_pixbuf_widget_scale(NULL, STOCK_PIXMAP_MAIL_ATTACH, 18, 18);
	gtk_widget_show(image);
	gtk_container_add(GTK_CONTAINER(mime_toggle_btn), image);

	tip = gtk_tooltips_new();
	gtk_tooltips_set_tip(tip, mime_toggle_btn,
			     _("Switch to attachment list view"), NULL);

	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show(arrow);
	gtk_widget_set_size_request(arrow, 5, -1);

	menu_tool_btn = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(menu_tool_btn), arrow);
	GTK_WIDGET_UNSET_FLAGS(menu_tool_btn, GTK_CAN_FOCUS);
	gtk_widget_show(menu_tool_btn);
	gtk_box_pack_start(GTK_BOX(toolbar_hbox), menu_tool_btn,
			   FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(menu_tool_btn), "button_press_event",
			 G_CALLBACK(messageview_menu_tool_btn_pressed),
			 messageview);

	tool_menu = gtk_menu_new();
	MENUITEM_ADD(tool_menu, menuitem, NULL, 0);
	MENUITEM_ADD_WITH_MNEMONIC(tool_menu, menuitem, _("Save _all attachments..."), 0);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(messageview_save_all_cb), messageview);
	gtk_widget_show_all(tool_menu);

	gtk_widget_hide(toolbar_vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(hbox), toolbar_vbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET_PTR(headerview), TRUE, TRUE, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	/* to remove without destroyed */
	gtk_widget_ref(GTK_WIDGET_PTR(mimeview->textview));
	gtk_widget_ref(GTK_WIDGET_PTR(mimeview->imageview));

	g_signal_connect(G_OBJECT(notebook), "switch_page",
			 G_CALLBACK(messageview_switch_page_cb), messageview);

	messageview->vbox        = vbox;

	messageview->notebook    = notebook;

	messageview->hbox            = hbox;
	messageview->toolbar_vbox    = toolbar_vbox;
	messageview->mime_toggle_btn = mime_toggle_btn;
	messageview->menu_tool_btn   = menu_tool_btn;
	messageview->tool_menu       = tool_menu;

	messageview->new_window  = FALSE;
	messageview->window      = NULL;
	messageview->window_vbox = NULL;
	messageview->body_vbox   = NULL;

	messageview->headerview  = headerview;
	messageview->textview    = textview;
	messageview->mimeview    = mimeview;

	messageview->statusbar     = NULL;
	messageview->statusbar_cid = 0;

	messageview->current_page = 0;

	return messageview;
}

MessageView *messageview_create_with_new_window(void)
{
	MessageView *msgview;
	GtkWidget *window;
	GtkWidget *window_vbox;
	GtkWidget *body_vbox;
	GtkWidget *vspacer;
	GtkWidget *menubar;
	GtkItemFactory *ifactory;
	GtkWidget *statusbar;
	guint n_menu_entries;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Message View - Sylpheed"));
	gtk_window_set_wmclass(GTK_WINDOW(window), "message_view", "Sylpheed");
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_size_request(window, prefs_common.msgwin_width,
				    prefs_common.msgwin_height);

	msgview = messageview_create();

	window_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), window_vbox);

	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(messageview_size_allocate_cb),
			 msgview);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(messageview_delete_cb), msgview);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), msgview);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	n_menu_entries = sizeof(msgview_entries) / sizeof (msgview_entries[0]);
	menubar = menubar_create(window, msgview_entries, n_menu_entries,
				 "<MessageView>", msgview);
#if 0
	menu_factory_copy_rc("<Main>", "<MessageView>");
#endif
	gtk_box_pack_start(GTK_BOX(window_vbox), menubar, FALSE, TRUE, 0);

	vspacer = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(window_vbox), vspacer, FALSE, TRUE,
			   BORDER_WIDTH);

	body_vbox = gtk_vbox_new(FALSE, BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(window_vbox), body_vbox, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(body_vbox), GTK_WIDGET_PTR(msgview),
			   TRUE, TRUE, 0);
	gtk_widget_grab_focus(msgview->textview->text);

	statusbar = gtk_statusbar_new();
	gtk_box_pack_end(GTK_BOX(body_vbox), statusbar, FALSE, FALSE, 0);
	msgview->statusbar = statusbar;
	msgview->statusbar_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Message View");

	msgview->new_window = TRUE;
	msgview->window = window;
	msgview->window_vbox = window_vbox;
	msgview->body_vbox = body_vbox;
	msgview->menubar = menubar;
	msgview->menu_locked = FALSE;
	msgview->visible = TRUE;

	gtk_widget_show_all(window);
	gtk_widget_hide(msgview->toolbar_vbox);

	messageview_init(msgview);

	messageview_set_encoding_menu(msgview);

	ifactory = gtk_item_factory_from_widget(menubar);
#ifndef G_OS_WIN32
	action_update_msgview_menu(ifactory, msgview);
#endif

	messageview_list = g_list_append(messageview_list, msgview);

	return msgview;
}

void messageview_init(MessageView *messageview)
{
	headerview_init(messageview->headerview);
	textview_init(messageview->textview);
	mimeview_init(messageview->mimeview);
	/* messageview_set_font(messageview); */
}

void messageview_reflect_prefs(MessageView *messageview)
{
	if (messageview->type == MVIEW_MIME)
		messageview_set_mime_view_layout(messageview);
}

GList *messageview_get_window_list(void)
{
	return messageview_list;
}

gint messageview_show(MessageView *messageview, MsgInfo *msginfo,
		      gboolean all_headers)
{
	gchar *file;
	MimeInfo *mimeinfo;

	g_return_val_if_fail(msginfo != NULL, -1);

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) {
		messageview_change_view_type(messageview, MVIEW_TEXT);
		textview_show_error(messageview->textview);
		return -1;
	}

	file = procmsg_get_message_file_path(msginfo);
	if (!file) {
		g_warning("can't get message file path.\n");
		procmime_mimeinfo_free_all(mimeinfo);
		messageview_change_view_type(messageview, MVIEW_TEXT);
		textview_show_error(messageview->textview);
		return -1;
	}

	if (messageview->msginfo != msginfo) {
		procmsg_msginfo_free(messageview->msginfo);
		messageview->msginfo = procmsg_msginfo_get_full_info(msginfo);
		if (!messageview->msginfo)
			messageview->msginfo = procmsg_msginfo_copy(msginfo);
	}
	procmime_mimeinfo_free_all(messageview->mimeinfo);
	messageview->mimeinfo = mimeinfo;
	g_free(messageview->file);
	messageview->file = file;

	if (messageview->window && msginfo->subject) {
		gchar *title;

		title = g_strconcat(msginfo->subject, " - Sylpheed", NULL);
		gtk_window_set_title(GTK_WINDOW(messageview->window), title);
		g_free(title);
	}
	headerview_show(messageview->headerview, messageview->msginfo);

	textview_set_all_headers(messageview->textview, all_headers);
	textview_set_all_headers(messageview->mimeview->textview, all_headers);

	if (mimeinfo->mime_type != MIME_TEXT &&
	    (prefs_common.html_only_as_attach ||
	     mimeinfo->mime_type != MIME_TEXT_HTML)) {
		messageview_change_view_type(messageview, MVIEW_MIME);
		mimeview_show_message(messageview->mimeview, mimeinfo, file);
		messageview_set_tool_menu(messageview, mimeinfo);
	} else {
		messageview_change_view_type(messageview, MVIEW_TEXT);
		textview_show_message(messageview->textview, mimeinfo, file);
	}

	if (messageview->new_window)
		messageview_set_menu_state(messageview);

	syl_plugin_signal_emit("messageview-show", messageview, msginfo,
			       all_headers);

	return 0;
}

static void messageview_set_mime_view_layout(MessageView *messageview)
{
	if (prefs_common.attach_toolbutton_pos == 0)
		gtk_box_set_child_packing(GTK_BOX(messageview->hbox), messageview->toolbar_vbox, FALSE, FALSE, 0, GTK_PACK_START);
	else
		gtk_box_set_child_packing(GTK_BOX(messageview->hbox), messageview->toolbar_vbox, FALSE, FALSE, 0, GTK_PACK_END);

	if (prefs_common.show_attach_tab)
		gtk_widget_hide(messageview->toolbar_vbox);
	else
		gtk_widget_show(messageview->toolbar_vbox);

	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(messageview->notebook),
				   prefs_common.show_attach_tab);
}

static void messageview_change_view_type(MessageView *messageview,
					 MessageType type)
{
	GtkWidget *notebook = messageview->notebook;

	if (messageview->type == type) return;

	if (type == MVIEW_MIME) {
		messageview_set_mime_view_layout(messageview);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),
					      messageview->current_page);
		if (messageview->current_page == 0)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(messageview->mime_toggle_btn), FALSE);
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(messageview->mime_toggle_btn), TRUE);
	} else if (type == MVIEW_TEXT) {
		gint current_page = messageview->current_page;

		gtk_widget_hide(messageview->toolbar_vbox);
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
		messageview->current_page = current_page;
		mimeview_clear(messageview->mimeview);
	} else
		return;

	messageview->type = type;
}

static void messageview_set_tool_menu(MessageView *messageview,
				      MimeInfo *mimeinfo)
{
	MimeInfo *partinfo;
	const gchar *name;
	GtkWidget *menuitem;
	GList *cur_item, *next = NULL;
	gint pos = 0;
	gboolean exist;

	cur_item = GTK_MENU_SHELL(messageview->tool_menu)->children;
	while (cur_item != NULL) {
		next = cur_item->next;
		if (GTK_BIN(cur_item->data)->child == NULL)
			break;
		gtk_widget_destroy(GTK_WIDGET(cur_item->data));
		cur_item = next;
	}

	for (partinfo = mimeinfo; partinfo != NULL;
	     partinfo = procmime_mimeinfo_next(partinfo)) {
		if (partinfo->filename || partinfo->name) {
			name = g_basename(partinfo->filename ? partinfo->filename : partinfo->name);
			if (*name == '\0')
				name = "mimetmp";
			menuitem = gtk_menu_item_new_with_label(name);
			gtk_widget_show(menuitem);
			gtk_menu_shell_insert(GTK_MENU_SHELL(messageview->tool_menu), menuitem, pos++);
			g_object_set_data(G_OBJECT(menuitem), "tool-menu-mimeinfo", partinfo);
			g_signal_connect(G_OBJECT(menuitem), "activate",
					 G_CALLBACK(messageview_open_file_cb),
					 messageview);
		}
	}

	exist = (pos != 0);
	for (cur_item = next; cur_item != NULL; cur_item = cur_item->next) {
		menuitem = GTK_WIDGET(cur_item->data);
		gtk_widget_set_sensitive(menuitem, exist);
	}
}

static void messageview_set_menu_state(MessageView *messageview)
{
	GtkItemFactory *ifactory;
	GtkWidget *menuitem;

	messageview->menu_locked = TRUE;

	ifactory = gtk_item_factory_from_widget(messageview->menubar);
	menuitem = gtk_item_factory_get_widget
		(ifactory, "/View/All headers");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       messageview->textview->show_all_headers);

	if (messageview->msginfo &&
	    FOLDER_ITEM_IS_SENT_FOLDER(messageview->msginfo->folder))
		menu_set_sensitive(ifactory, "/Message/Re-edit", TRUE);
	else
		menu_set_sensitive(ifactory, "/Message/Re-edit", FALSE);

	messageview->menu_locked = FALSE;
}

static void messageview_set_encoding_menu(MessageView *messageview)
{
	GtkItemFactoryEntry *entry;
	GtkItemFactory *ifactory;
	CharSet encoding;
	gchar *path, *p, *q;
	GtkWidget *item;

	encoding = conv_get_charset_from_str(prefs_common.force_charset);
	ifactory = gtk_item_factory_from_widget(messageview->menubar);

	for (entry = msgview_entries; entry->callback != view_source_cb;
	     entry++) {
		if (entry->callback == set_charset_cb &&
		    (CharSet)entry->callback_action == encoding) {
			p = q = path = g_strdup(entry->path);
			while (*p) {
				if (*p == '_') {
					if (p[1] == '_') {
						p++;
						*q++ = '_';
					}
				} else
					*q++ = *p;
				p++;
			}
			*q = '\0';
			item = gtk_item_factory_get_item(ifactory, path);
			gtk_widget_activate(item);
			g_free(path);
			break;
		}
	}
}

void messageview_clear(MessageView *messageview)
{
	messageview_set_tool_menu(messageview, NULL);
	g_free(messageview->file);
	messageview->file = NULL;
	procmime_mimeinfo_free_all(messageview->mimeinfo);
	messageview->mimeinfo = NULL;
	procmsg_msginfo_free(messageview->msginfo);
	messageview->msginfo = NULL;
	messageview_change_view_type(messageview, MVIEW_TEXT);
	headerview_clear(messageview->headerview);
	textview_clear(messageview->textview);
	mimeview_clear(messageview->mimeview);
}

void messageview_destroy(MessageView *messageview)
{
	GtkWidget *textview  = GTK_WIDGET_PTR(messageview->mimeview->textview);
	GtkWidget *imageview = GTK_WIDGET_PTR(messageview->mimeview->imageview);

	messageview_list = g_list_remove(messageview_list, messageview);

	headerview_destroy(messageview->headerview);
	textview_destroy(messageview->textview);
	mimeview_destroy(messageview->mimeview);

	procmsg_msginfo_free(messageview->msginfo);

	gtk_widget_destroy(messageview->tool_menu);
	if (messageview->window)
		gtk_widget_destroy(messageview->window);

	g_free(messageview);

	gtk_widget_unref(textview);
	gtk_widget_unref(imageview);
}

void messageview_quote_color_set(void)
{
}

void messageview_set_font(MessageView *messageview)
{
	textview_set_font(messageview->textview, NULL);
}

TextView *messageview_get_current_textview(MessageView *messageview)
{
	TextView *text = NULL;

	if (messageview->type == MVIEW_TEXT)
		text = messageview->textview;
	else if (messageview->type == MVIEW_MIME) {
		if (gtk_notebook_get_current_page
			(GTK_NOTEBOOK(messageview->notebook)) == 0)
			text = messageview->textview;
		else if (messageview->mimeview->type == MIMEVIEW_TEXT)
			text = messageview->mimeview->textview;
	}

	return text;
}

MimeInfo *messageview_get_selected_mime_part(MessageView *messageview)
{
	if (messageview->type == MVIEW_MIME)
		return mimeview_get_selected_part(messageview->mimeview);

	return NULL;
}

void messageview_copy_clipboard(MessageView *messageview)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
	if (text) {
		GtkTextView *textview = GTK_TEXT_VIEW(text->text);
		GtkTextBuffer *buffer;
		GtkClipboard *clipboard;

		buffer = gtk_text_view_get_buffer(textview);
		clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
		gtk_text_buffer_copy_clipboard(buffer, clipboard);
	}
}

void messageview_select_all(MessageView *messageview)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
	if (text) {
		GtkTextView *textview = GTK_TEXT_VIEW(text->text);
		GtkTextBuffer *buffer;
		GtkTextIter start, end;

		buffer = gtk_text_view_get_buffer(textview);
		gtk_text_buffer_get_bounds(buffer, &start, &end);
		gtk_text_buffer_select_range(buffer, &start, &end);
	}
}

void messageview_set_position(MessageView *messageview, gint pos)
{
	textview_set_position(messageview->textview, pos);
}

gboolean messageview_search_string(MessageView *messageview, const gchar *str,
				   gboolean case_sens)
{
	return textview_search_string(messageview->textview, str, case_sens);
	return FALSE;
}

gboolean messageview_search_string_backward(MessageView *messageview,
					    const gchar *str,
					    gboolean case_sens)
{
	return textview_search_string_backward(messageview->textview,
					       str, case_sens);
	return FALSE;
}

gboolean messageview_is_visible(MessageView *messageview)
{
	return messageview->visible;
}

void messageview_save_as(MessageView *messageview)
{
	gchar *filename = NULL;
	MsgInfo *msginfo;
	gchar *src, *dest;
	FileselFileType types[4] = {{NULL, NULL}};
	gint selected_type = 0;
	gint result;
	gboolean all_headers;

	if (!messageview->msginfo) return;
	msginfo = messageview->msginfo;

	if (msginfo->subject && *msginfo->subject) {
		filename = g_strdup_printf("%s.eml", msginfo->subject);
	} else {
		filename = g_strdup_printf("%u.eml", msginfo->msgnum);
	}
	subst_for_filename(filename);

	types[0].type = _("Original (EML/RFC 822)");
	types[0].ext = "eml";
	types[1].type = _("Text");
	types[1].ext = "txt";
	types[2].type = _("Text (UTF-8)");
	types[2].ext = "txt";
	dest = filesel_save_as_type(filename, types, prefs_common.save_file_type, &selected_type);

	g_free(filename);
	if (!dest)
		return;

	all_headers = messageview->textview->show_all_headers;

	if (selected_type == 1) {
		result = procmsg_save_message_as_text(msginfo, dest, conv_get_locale_charset_str(), all_headers);
	} else if (selected_type == 2) {
		result = procmsg_save_message_as_text(msginfo, dest, NULL, all_headers);
	} else {
		src = procmsg_get_message_file(msginfo);
		result = copy_file(src, dest, TRUE);
		g_free(src);
	}

	if (result < 0) {
		gchar *utf8_dest;

		utf8_dest = conv_filename_to_utf8(dest);
		alertpanel_error(_("Can't save the file `%s'."),
				 g_basename(utf8_dest));
		g_free(utf8_dest);
	}

	g_free(dest);

	prefs_common.save_file_type = selected_type;
}

static gint messageview_delete_cb(GtkWidget *widget, GdkEventAny *event,
				  MessageView *messageview)
{
	messageview_destroy(messageview);
	return TRUE;
}

static void messageview_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	g_return_if_fail(allocation != NULL);

	prefs_common.msgwin_width  = allocation->width;
	prefs_common.msgwin_height = allocation->height;
}

static void messageview_toggle_attach_cb(GtkToggleButton *toggle,
					 MessageView *messageview)
{
	if (gtk_toggle_button_get_active(toggle))
		gtk_notebook_set_current_page(GTK_NOTEBOOK(messageview->notebook), 1);
	else
		gtk_notebook_set_current_page(GTK_NOTEBOOK(messageview->notebook), 0);
}

static void messageview_switch_page_cb(GtkNotebook *notebook,
				       GtkNotebookPage *page, guint page_num,
				       MessageView *messageview)
{
	if (messageview->current_page != page_num) {
		messageview->current_page = page_num;
		if (page_num == 0)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(messageview->mime_toggle_btn), FALSE);
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(messageview->mime_toggle_btn), TRUE);
	}
}

static gint messageview_menu_tool_btn_pressed(GtkWidget *widget,
					      GdkEventButton *event,
					      MessageView *messageview)
{
	if (!event)
		return FALSE;

	gtk_menu_popup(GTK_MENU(messageview->tool_menu), NULL, NULL, 
		       menu_button_position, messageview->mime_toggle_btn,
		       event->button, event->time);

	return TRUE;
}

static void messageview_open_file_cb(GtkWidget *widget, MessageView *messageview)
{
	MimeInfo *partinfo;

	partinfo = g_object_get_data(G_OBJECT(widget), "tool-menu-mimeinfo");
	if (partinfo)
		mimeview_launch_part(messageview->mimeview, partinfo);
}

static void messageview_save_all_cb(GtkWidget *widget, MessageView *messageview)
{
	mimeview_save_all(messageview->mimeview);
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    MessageView *messageview)
{
	if (event && event->keyval == GDK_Escape && messageview->window) {
		messageview_destroy(messageview);
		return TRUE;
	}
	return FALSE;
}

static void save_as_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	messageview_save_as(messageview);
}

#if GTK_CHECK_VERSION(2, 10, 0)
static void page_setup_cb(gpointer data, guint action, GtkWidget *widget)
{
	printing_page_setup_gtk();
}
#endif

static void print_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;

	if (!messageview->msginfo) return;

	printing_print_message(messageview->msginfo,
			       messageview->textview->show_all_headers);
}

static void close_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	messageview_destroy(messageview);
}

static void copy_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	messageview_copy_clipboard(messageview);
}

static void allsel_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	messageview_select_all(messageview);
}

static void search_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	message_search(messageview);
}

static void set_charset_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	const gchar *charset;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		charset = conv_get_charset_str((CharSet)action);
		g_free(messageview->forced_charset);
		messageview->forced_charset = g_strdup(charset);
		if (messageview->msginfo)
			messageview_show(messageview, messageview->msginfo,
					 FALSE);
	}
}

static void view_source_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	SourceWindow *srcwin;

	if (!messageview->msginfo) return;

	srcwin = source_window_create();
	source_window_show_msg(srcwin, messageview->msginfo);
	source_window_show(srcwin);
}

static void show_all_header_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *msginfo = messageview->msginfo;

	if (!msginfo) return;
	if (messageview->menu_locked) return;

	messageview->msginfo = NULL;
	messageview_show(messageview, msginfo,
			 GTK_CHECK_MENU_ITEM(widget)->active);
	procmsg_msginfo_free(msginfo);
}

static void compose_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	PrefsAccount *ac = NULL;
	FolderItem *item = NULL;

	if (messageview->msginfo)
		item = messageview->msginfo->folder;

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

static void reply_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	GSList *mlist = NULL;
	MsgInfo *msginfo;
	gchar *text = NULL;
	ComposeMode mode = (ComposeMode)action;
	gchar *prev_force_charset;

	msginfo = messageview->msginfo;
	mlist = g_slist_append(NULL, msginfo);

	text = gtkut_text_view_get_selection
		(GTK_TEXT_VIEW(messageview->textview->text));
	if (text && *text == '\0') {
		g_free(text);
		text = NULL;
	}

	if (!COMPOSE_QUOTE_MODE(mode))
		mode |= prefs_common.reply_with_quote
			? COMPOSE_WITH_QUOTE : COMPOSE_WITHOUT_QUOTE;

	prev_force_charset = prefs_common.force_charset;
	prefs_common.force_charset = messageview->forced_charset;

	switch (COMPOSE_MODE(mode)) {
	case COMPOSE_REPLY:
	case COMPOSE_REPLY_TO_SENDER:
	case COMPOSE_REPLY_TO_ALL:
	case COMPOSE_REPLY_TO_LIST:
		compose_reply(msginfo, msginfo->folder, mode, text);
		break;
	case COMPOSE_FORWARD:
		compose_forward(mlist, msginfo->folder, FALSE, text);
		break;
	case COMPOSE_FORWARD_AS_ATTACH:
		compose_forward(mlist, msginfo->folder, TRUE, NULL);
		break;
	case COMPOSE_REDIRECT:
		compose_redirect(msginfo, msginfo->folder);
		break;
	default:
		g_warning("messageview.c: reply_cb(): invalid mode: %d\n",
			  mode);
	}

	prefs_common.force_charset = prev_force_charset;

	/* summary_set_marks_selected(summaryview); */
	g_free(text);
	g_slist_free(mlist);
}

static void reedit_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *msginfo;

	if (!messageview->msginfo) return;
	msginfo = messageview->msginfo;
	if (!msginfo->folder) return;
	if (msginfo->folder->stype != F_OUTBOX &&
	    msginfo->folder->stype != F_DRAFT &&
	    msginfo->folder->stype != F_QUEUE) return;

	compose_reedit(msginfo);
}

static void addressbook_open_cb(gpointer data, guint action, GtkWidget *widget)
{
	addressbook_open(NULL);
}

static void add_address_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *msginfo;
	gchar *from;

	if (!messageview->msginfo) return;
	msginfo = messageview->msginfo;
	from = g_strdup(msginfo->from);
	eliminate_address_comment(from);
	extract_address(from);
	addressbook_add_contact(msginfo->fromname, from, NULL);
	g_free(from);
}

static void create_filter_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	gchar *header = NULL;
	gchar *key = NULL;

	if (!messageview->msginfo) return;

	filter_get_keyword_from_msg(messageview->msginfo, &header, &key,
				    (FilterCreateType)action);
	prefs_filter_open(messageview->msginfo, header, key);

	g_free(header);
	g_free(key);
}

static void about_cb(gpointer data, guint action, GtkWidget *widget)
{
	about_show();
}
