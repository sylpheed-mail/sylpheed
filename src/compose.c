/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkhandlebox.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtktable.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkdnd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
/* #include <sys/utsname.h> */
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif

#include "intl.h"
#include "main.h"
#include "mainwindow.h"
#include "compose.h"
#include "addressbook.h"
#include "folderview.h"
#include "procmsg.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "send_message.h"
#include "imap.h"
#include "news.h"
#include "customheader.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "action.h"
#include "account.h"
#include "filesel.h"
#include "procheader.h"
#include "procmime.h"
#include "statusbar.h"
#include "about.h"
#include "base64.h"
#include "quoted-printable.h"
#include "codeconv.h"
#include "utils.h"
#include "gtkutils.h"
#include "socket.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "gtkshruler.h"
#include "folder.h"
#include "addr_compl.h"
#include "quote_fmt.h"
#include "template.h"
#include "undo.h"

#if USE_GPGME
#  include "rfc2015.h"
#endif

typedef enum
{
	COL_MIMETYPE = 0,
	COL_SIZE     = 1,
	COL_NAME     = 2
} AttachColumnPos;

#define N_ATTACH_COLS		3

typedef enum
{
	COMPOSE_ACTION_MOVE_BEGINNING_OF_LINE,
	COMPOSE_ACTION_MOVE_FORWARD_CHARACTER,
	COMPOSE_ACTION_MOVE_BACKWARD_CHARACTER,
	COMPOSE_ACTION_MOVE_FORWARD_WORD,
	COMPOSE_ACTION_MOVE_BACKWARD_WORD,
	COMPOSE_ACTION_MOVE_END_OF_LINE,
	COMPOSE_ACTION_MOVE_NEXT_LINE,
	COMPOSE_ACTION_MOVE_PREVIOUS_LINE,
	COMPOSE_ACTION_DELETE_FORWARD_CHARACTER,
	COMPOSE_ACTION_DELETE_BACKWARD_CHARACTER,
	COMPOSE_ACTION_DELETE_FORWARD_WORD,
	COMPOSE_ACTION_DELETE_BACKWARD_WORD,
	COMPOSE_ACTION_DELETE_LINE,
	COMPOSE_ACTION_DELETE_LINE_N,
	COMPOSE_ACTION_DELETE_TO_LINE_END
} ComposeAction;

#define B64_LINE_SIZE		57
#define B64_BUFFSIZE		77

#define MAX_REFERENCES_LEN	999

static GdkColor quote_color = {0, 0, 0, 0xbfff};

static GList *compose_list = NULL;

static Compose *compose_create			(PrefsAccount	*account,
						 ComposeMode	 mode);
static void compose_connect_changed_callbacks	(Compose	*compose);
static void compose_toolbar_create		(Compose	*compose,
						 GtkWidget	*container);
static GtkWidget *compose_account_option_menu_create
						(Compose	*compose);
static void compose_set_template_menu		(Compose	*compose);
static void compose_template_apply		(Compose	*compose,
						 Template	*tmpl,
						 gboolean	 replace);
static void compose_destroy			(Compose	*compose);

static void compose_entry_show			(Compose	*compose,
						 ComposeEntryType type);
static GtkEntry *compose_get_entry		(Compose	*compose,
						 ComposeEntryType type);
static void compose_entries_set			(Compose	*compose,
						 const gchar	*mailto);
static void compose_entries_set_from_item	(Compose	*compose,
						 FolderItem	*item,
						 ComposeMode	 mode);
static gint compose_parse_header		(Compose	*compose,
						 MsgInfo	*msginfo);
static gchar *compose_parse_references		(const gchar	*ref,
						 const gchar	*msgid);

static gchar *compose_quote_fmt			(Compose	*compose,
						 MsgInfo	*msginfo,
						 const gchar	*fmt,
						 const gchar	*qmark,
						 const gchar	*body);

static void compose_reply_set_entry		(Compose	*compose,
						 MsgInfo	*msginfo,
						 ComposeMode	 mode);
static void compose_reedit_set_entry		(Compose	*compose,
						 MsgInfo	*msginfo);
static void compose_insert_sig			(Compose	*compose,
						 gboolean	 replace);
static gchar *compose_get_signature_str		(Compose	*compose);
static void compose_insert_file			(Compose	*compose,
						 const gchar	*file);
static void compose_attach_append		(Compose	*compose,
						 const gchar	*file,
						 const gchar	*filename,
						 const gchar	*content_type);
static void compose_attach_parts		(Compose	*compose,
						 MsgInfo	*msginfo);
static void compose_wrap_line			(Compose	*compose);
static void compose_wrap_line_all		(Compose	*compose);
static void compose_wrap_line_all_full		(Compose	*compose,
						 gboolean	 autowrap);
static void compose_set_title			(Compose	*compose);
static void compose_select_account		(Compose	*compose,
						 PrefsAccount	*account,
						 gboolean	 init);

static gboolean compose_check_for_valid_recipient
						(Compose	*compose);
static gboolean compose_check_entries		(Compose	*compose);

static gint compose_send			(Compose	*compose);
static gint compose_write_to_file		(Compose	*compose,
						 const gchar	*file,
						 gboolean	 is_draft);
static gint compose_write_body_to_file		(Compose	*compose,
						 const gchar	*file);
static gint compose_redirect_write_to_file	(Compose	*compose,
						 const gchar	*file);
static gint compose_remove_reedit_target	(Compose	*compose);
static gint compose_queue			(Compose	*compose,
						 const gchar	*file);
static void compose_write_attach		(Compose	*compose,
						 FILE		*fp);
static gint compose_write_headers		(Compose	*compose,
						 FILE		*fp,
						 const gchar	*charset,
						 EncodingType	 encoding,
						 gboolean	 is_draft);
static gint compose_redirect_write_headers	(Compose	*compose,
						 FILE		*fp);

static void compose_convert_header		(gchar		*dest,
						 gint		 len,
						 gchar		*src,
						 gint		 header_len,
						 gboolean	 addr_field);
static void compose_generate_msgid		(Compose	*compose,
						 gchar		*buf,
						 gint		 len);

static void compose_attach_info_free		(AttachInfo	*ainfo);
static void compose_attach_remove_selected	(Compose	*compose);

static void compose_attach_property		(Compose	*compose);
static void compose_attach_property_create	(gboolean	*cancelled);
static void attach_property_ok			(GtkWidget	*widget,
						 gboolean	*cancelled);
static void attach_property_cancel		(GtkWidget	*widget,
						 gboolean	*cancelled);
static gint attach_property_delete_event	(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gboolean	*cancelled);
static gboolean attach_property_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gboolean	*cancelled);

static void compose_exec_ext_editor		(Compose	   *compose);
static gint compose_exec_ext_editor_real	(const gchar	   *file);
static gboolean compose_ext_editor_kill		(Compose	   *compose);
static void compose_input_cb			(gpointer	    data,
						 gint		    source,
						 GdkInputCondition  condition);
static void compose_set_ext_editor_sensitive	(Compose	   *compose,
						 gboolean	    sensitive);

static void compose_undo_state_changed		(UndoMain	*undostruct,
						 gint		 undo_state,
						 gint		 redo_state,
						 gpointer	 data);

static gint calc_cursor_xpos	(GtkTextView	*text,
				 gint		 extra,
				 gint		 char_width);

/* callback functions */

static gboolean compose_edit_size_alloc (GtkEditable	*widget,
					 GtkAllocation	*allocation,
					 GtkSHRuler	*shruler);

static void toolbar_send_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_send_later_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_draft_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_insert_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_attach_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_sig_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_ext_editor_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_linewrap_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_address_cb		(GtkWidget	*widget,
					 gpointer	 data);

static void account_activated		(GtkMenuItem	*menuitem,
					 gpointer	 data);

static void attach_selected		(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event,
					 gpointer	 data);
static gboolean attach_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 gpointer	 data);
static gboolean attach_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);

static void compose_send_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_send_later_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_draft_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_attach_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_insert_file_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_insert_sig_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_close_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_address_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_template_activate_cb(GtkWidget	*widget,
					 gpointer	 data);

static void compose_ext_editor_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static gint compose_delete_cb		(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static void compose_destroy_cb		(GtkWidget	*widget,
					 Compose	*compose);

static void compose_undo_cb		(Compose	*compose);
static void compose_redo_cb		(Compose	*compose);
static void compose_cut_cb		(Compose	*compose);
static void compose_copy_cb		(Compose	*compose);
static void compose_paste_cb		(Compose	*compose);
static void compose_paste_as_quote_cb	(Compose	*compose);
static void compose_allsel_cb		(Compose	*compose);

static void compose_action_cb		(Compose	*compose,
					 ComposeAction	 action);

static void compose_grab_focus_cb	(GtkWidget	*widget,
					 Compose	*compose);

static void compose_changed_cb		(GtkTextBuffer	*textbuf,
					 Compose	*compose);

static void compose_toggle_autowrap_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_toggle_to_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_cc_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_bcc_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_replyto_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_followupto_cb(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_attach_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_ruler_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
#if USE_GPGME
static void compose_toggle_sign_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_encrypt_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
#endif

static void compose_attach_drag_received_cb (GtkWidget		*widget,
					     GdkDragContext	*drag_context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data);
static void compose_insert_drag_received_cb (GtkWidget		*widget,
					     GdkDragContext	*drag_context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data);

static void to_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void newsgroups_activated	(GtkWidget	*widget,
					 Compose	*compose);
static void cc_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void bcc_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void replyto_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void followupto_activated	(GtkWidget	*widget,
					 Compose	*compose);
static void subject_activated		(GtkWidget	*widget,
					 Compose	*compose);

static void text_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void text_inserted		(GtkTextBuffer	*buffer,
					 GtkTextIter	*iter,
					 const gchar	*text,
					 gint		 len,
					 Compose	*compose);

static gboolean compose_send_control_enter	(Compose	*compose);

static GtkItemFactoryEntry compose_popup_entries[] =
{
	{N_("/_Add..."),	NULL, compose_attach_cb, 0, NULL},
	{N_("/_Remove"),	NULL, compose_attach_remove_selected, 0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Properties..."),	NULL, compose_attach_property, 0, NULL}
};

static GtkItemFactoryEntry compose_entries[] =
{
	{N_("/_File"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Send"),			"<control>Return",
						compose_send_cb, 0, NULL},
	{N_("/_File/Send _later"),		"<shift><control>S",
						compose_send_later_cb,  0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/Save to _draft folder"),
						"<shift><control>D", compose_draft_cb, 0, NULL},
	{N_("/_File/Save and _keep editing"),
						"<control>S", compose_draft_cb, 1, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Attach file"),		"<control>M", compose_attach_cb,      0, NULL},
	{N_("/_File/_Insert file"),		"<control>I", compose_insert_file_cb, 0, NULL},
	{N_("/_File/Insert si_gnature"),	"<control>G", compose_insert_sig_cb,  0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Close"),			"<control>W", compose_close_cb, 0, NULL},

	{N_("/_Edit"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/_Undo"),		"<control>Z", compose_undo_cb, 0, NULL},
	{N_("/_Edit/_Redo"),		"<control>Y", compose_redo_cb, 0, NULL},
	{N_("/_Edit/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/Cu_t"),		"<control>X", compose_cut_cb,    0, NULL},
	{N_("/_Edit/_Copy"),		"<control>C", compose_copy_cb,   0, NULL},
	{N_("/_Edit/_Paste"),		"<control>V", compose_paste_cb,  0, NULL},
	{N_("/_Edit/Paste as _quotation"),
					NULL, compose_paste_as_quote_cb, 0, NULL},
	{N_("/_Edit/Select _all"),	"<control>A", compose_allsel_cb, 0, NULL},
	{N_("/_Edit/A_dvanced"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/A_dvanced/Move a character backward"),
					"<control>B",
					compose_action_cb,
					COMPOSE_ACTION_MOVE_BACKWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Move a character forward"),
					"<control>F",
					compose_action_cb,
					COMPOSE_ACTION_MOVE_FORWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Move a word backward"),
					NULL, /* "<alt>B" */
					compose_action_cb,
					COMPOSE_ACTION_MOVE_BACKWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Move a word forward"),
					NULL, /* "<alt>F" */
					compose_action_cb,
					COMPOSE_ACTION_MOVE_FORWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Move to beginning of line"),
					NULL, /* "<control>A" */
					compose_action_cb,
					COMPOSE_ACTION_MOVE_BEGINNING_OF_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Move to end of line"),
					"<control>E",
					compose_action_cb,
					COMPOSE_ACTION_MOVE_END_OF_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Move to previous line"),
					"<control>P",
					compose_action_cb,
					COMPOSE_ACTION_MOVE_PREVIOUS_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Move to next line"),
					"<control>N",
					compose_action_cb,
					COMPOSE_ACTION_MOVE_NEXT_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a character backward"),
					"<control>H",
					compose_action_cb,
					COMPOSE_ACTION_DELETE_BACKWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a character forward"),
					"<control>D",
					compose_action_cb,
					COMPOSE_ACTION_DELETE_FORWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a word backward"),
					NULL, /* "<control>W" */
					compose_action_cb,
					COMPOSE_ACTION_DELETE_BACKWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a word forward"),
					NULL, /* "<alt>D", */
					compose_action_cb,
					COMPOSE_ACTION_DELETE_FORWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Delete line"),
					"<control>U",
					compose_action_cb,
					COMPOSE_ACTION_DELETE_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Delete to end of line"),
					"<control>K",
					compose_action_cb,
					COMPOSE_ACTION_DELETE_TO_LINE_END,
					NULL},
	{N_("/_Edit/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Wrap current paragraph"),
					"<control>L", compose_wrap_line, 0, NULL},
	{N_("/_Edit/Wrap all long _lines"),
					"<control><alt>L", compose_wrap_line_all, 0, NULL},
	{N_("/_Edit/Aut_o wrapping"),	"<shift><control>L", compose_toggle_autowrap_cb, 0, "<ToggleItem>"},
	{N_("/_View"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_To"),		NULL, compose_toggle_to_cb     , 0, "<ToggleItem>"},
	{N_("/_View/_Cc"),		NULL, compose_toggle_cc_cb     , 0, "<ToggleItem>"},
	{N_("/_View/_Bcc"),		NULL, compose_toggle_bcc_cb    , 0, "<ToggleItem>"},
	{N_("/_View/_Reply to"),	NULL, compose_toggle_replyto_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Followup to"),	NULL, compose_toggle_followupto_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/R_uler"),		NULL, compose_toggle_ruler_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Attachment"),	NULL, compose_toggle_attach_cb, 0, "<ToggleItem>"},

	{N_("/_Tools"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Address book"),	"<shift><control>A", compose_address_cb , 0, NULL},
	{N_("/_Tools/_Template"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/Actio_ns"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Edit with e_xternal editor"),
					"<shift><control>X", compose_ext_editor_cb, 0, NULL},
#if USE_GPGME
	{N_("/_Tools/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/PGP Si_gn"),   	NULL, compose_toggle_sign_cb   , 0, "<ToggleItem>"},
	{N_("/_Tools/PGP _Encrypt"),	NULL, compose_toggle_encrypt_cb, 0, "<ToggleItem>"},
#endif /* USE_GPGME */

	{N_("/_Help"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_About"),		NULL, about_show, 0, NULL}
};

static GtkTargetEntry compose_mime_types[] =
{
	{"text/uri-list", 0, 0}
};


void compose_new(PrefsAccount *account, FolderItem *item, const gchar *mailto,
		 GPtrArray *attach_files)
{
	Compose *compose;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	if (!account) account = cur_account;
	g_return_if_fail(account != NULL);

	compose = compose_create(account, COMPOSE_NEW);

	undo_block(compose->undostruct);

	if (prefs_common.auto_sig)
		compose_insert_sig(compose, FALSE);

	text = GTK_TEXT_VIEW(compose->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_place_cursor(buffer, &iter);

	if (account->protocol != A_NNTP) {
		if (mailto && *mailto != '\0') {
			compose_entries_set(compose, mailto);
			gtk_widget_grab_focus(compose->subject_entry);
		} else if (item) {
			compose_entries_set_from_item
				(compose, item, COMPOSE_NEW);
			if (item->auto_to)
				gtk_widget_grab_focus(compose->subject_entry);
			else
				gtk_widget_grab_focus(compose->to_entry);
		} else
			gtk_widget_grab_focus(compose->to_entry);
	} else {
		if (mailto && *mailto != '\0') {
			compose_entry_append(compose, mailto,
					     COMPOSE_ENTRY_NEWSGROUPS);
			gtk_widget_grab_focus(compose->subject_entry);
		} else
			gtk_widget_grab_focus(compose->newsgroups_entry);
	}

	if (attach_files) {
		gint i;
		gchar *file;

		for (i = 0; i < attach_files->len; i++) {
			file = g_ptr_array_index(attach_files, i);
			compose_attach_append(compose, file, file, NULL);
		}
	}

	undo_unblock(compose->undostruct);

	compose_connect_changed_callbacks(compose);

	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);
}

void compose_reply(MsgInfo *msginfo, FolderItem *item, ComposeMode mode,
		   const gchar *body)
{
	Compose *compose;
	PrefsAccount *account;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gboolean quote = FALSE;

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder != NULL);

	if (COMPOSE_QUOTE_MODE(mode) == COMPOSE_WITH_QUOTE)
		quote = TRUE;

	account = account_find_from_item(msginfo->folder);
	if (!account && msginfo->to && prefs_common.reply_account_autosel) {
		gchar *to;
		Xstrdup_a(to, msginfo->to, return);
		extract_address(to);
		account = account_find_from_address(to);
	}
	if (!account) account = cur_account;
	g_return_if_fail(account != NULL);

	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_FORWARDED);
	MSG_SET_PERM_FLAGS(msginfo->flags, MSG_REPLIED);
	if (MSG_IS_IMAP(msginfo->flags))
		imap_msg_set_perm_flags(msginfo, MSG_REPLIED);

	compose = compose_create(account, COMPOSE_REPLY);

	compose->replyinfo = procmsg_msginfo_get_full_info(msginfo);
	if (!compose->replyinfo)
		compose->replyinfo = procmsg_msginfo_copy(msginfo);

	if (compose_parse_header(compose, msginfo) < 0) return;

	undo_block(compose->undostruct);

	compose_reply_set_entry(compose, msginfo, mode);
	if (item)
		compose_entries_set_from_item(compose, item, COMPOSE_REPLY);

	if (quote) {
		gchar *qmark;
		gchar *quote_str;

		if (prefs_common.quotemark && *prefs_common.quotemark)
			qmark = prefs_common.quotemark;
		else
			qmark = "> ";

		quote_str = compose_quote_fmt(compose, compose->replyinfo,
					      prefs_common.quotefmt,
					      qmark, body);
	}

	if (prefs_common.auto_sig)
		compose_insert_sig(compose, FALSE);

	if (quote && prefs_common.linewrap_quote)
		compose_wrap_line_all(compose);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(compose->text));
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_place_cursor(buffer, &iter);

	gtk_widget_grab_focus(compose->text);

	undo_unblock(compose->undostruct);

	compose_connect_changed_callbacks(compose);

	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);
}

void compose_forward(GSList *mlist, FolderItem *item, gboolean as_attach,
		     const gchar *body)
{
	Compose *compose;
	PrefsAccount *account;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GSList *cur;
	MsgInfo *msginfo;

	g_return_if_fail(mlist != NULL);

	msginfo = (MsgInfo *)mlist->data;
	g_return_if_fail(msginfo->folder != NULL);

	account = account_find_from_item(msginfo->folder);
	if (!account) account = cur_account;
	g_return_if_fail(account != NULL);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_REPLIED);
		MSG_SET_PERM_FLAGS(msginfo->flags, MSG_FORWARDED);
	}
	msginfo = (MsgInfo *)mlist->data;
	if (MSG_IS_IMAP(msginfo->flags))
		imap_msg_list_unset_perm_flags(mlist, MSG_REPLIED);

	compose = compose_create(account, COMPOSE_FORWARD);

	undo_block(compose->undostruct);

	compose_entry_set(compose, "Fw: ", COMPOSE_ENTRY_SUBJECT);
	if (mlist->next == NULL && msginfo->subject && *msginfo->subject)
		compose_entry_append(compose, msginfo->subject,
				     COMPOSE_ENTRY_SUBJECT);
	if (item)
		compose_entries_set_from_item(compose, item, COMPOSE_FORWARD);

	text = GTK_TEXT_VIEW(compose->text);
	buffer = gtk_text_view_get_buffer(text);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (as_attach) {
			gchar *msgfile;

			msgfile = procmsg_get_message_file_path(msginfo);
			if (!is_file_exist(msgfile))
				g_warning(_("%s: file not exist\n"), msgfile);
			else
				compose_attach_append(compose, msgfile, msgfile,
						      "message/rfc822");

			g_free(msgfile);
		} else {
			gchar *qmark;
			gchar *quote_str;
			MsgInfo *full_msginfo;

			full_msginfo = procmsg_msginfo_get_full_info(msginfo);
			if (!full_msginfo)
				full_msginfo = procmsg_msginfo_copy(msginfo);

			if (cur != mlist) {
				GtkTextMark *mark;
				mark = gtk_text_buffer_get_insert(buffer);
				gtk_text_buffer_get_iter_at_mark
					(buffer, &iter, mark);
				gtk_text_buffer_insert
					(buffer, &iter, "\n\n", 2);
			}

			if (prefs_common.fw_quotemark &&
			    *prefs_common.fw_quotemark)
				qmark = prefs_common.fw_quotemark;
			else
				qmark = "> ";

			quote_str = compose_quote_fmt(compose, full_msginfo,
						      prefs_common.fw_quotefmt,
						      qmark, body);
			compose_attach_parts(compose, msginfo);

			procmsg_msginfo_free(full_msginfo);

			if (body) break;
		}
	}

	if (prefs_common.auto_sig)
		compose_insert_sig(compose, FALSE);

	if (prefs_common.linewrap_quote)
		compose_wrap_line_all(compose);

	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_place_cursor(buffer, &iter);

	undo_unblock(compose->undostruct);

	compose_connect_changed_callbacks(compose);

	if (account->protocol != A_NNTP)
		gtk_widget_grab_focus(compose->to_entry);
	else
		gtk_widget_grab_focus(compose->newsgroups_entry);

	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);
}

void compose_redirect(MsgInfo *msginfo, FolderItem *item)
{
	Compose *compose;
	PrefsAccount *account;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	FILE *fp;
	gchar buf[BUFFSIZE];

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder != NULL);

	account = account_find_from_item(msginfo->folder);
	if (!account) account = cur_account;
	g_return_if_fail(account != NULL);

	compose = compose_create(account, COMPOSE_REDIRECT);
	compose->targetinfo = procmsg_msginfo_copy(msginfo);

	if (compose_parse_header(compose, msginfo) < 0) return;

	undo_block(compose->undostruct);

	if (msginfo->subject)
		compose_entry_set(compose, msginfo->subject,
				  COMPOSE_ENTRY_SUBJECT);
	compose_entries_set_from_item(compose, item, COMPOSE_REDIRECT);

	text = GTK_TEXT_VIEW(compose->text);
	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	if ((fp = procmime_get_first_text_content(msginfo)) == NULL)
		g_warning(_("Can't get text part\n"));
	else {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			strcrchomp(buf);
			gtk_text_buffer_insert(buffer, &iter, buf, -1);
		}
		fclose(fp);
	}
	compose_attach_parts(compose, msginfo);

	if (account->protocol != A_NNTP)
		gtk_widget_grab_focus(compose->to_entry);
	else
		gtk_widget_grab_focus(compose->newsgroups_entry);

	gtk_text_view_set_editable(text, FALSE);

	undo_unblock(compose->undostruct);

	compose_connect_changed_callbacks(compose);
}

void compose_reedit(MsgInfo *msginfo)
{
	Compose *compose;
	PrefsAccount *account;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	FILE *fp;
	gchar buf[BUFFSIZE];

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder != NULL);

	account = account_find_from_msginfo(msginfo);
	if (!account) account = cur_account;
	g_return_if_fail(account != NULL);

	compose = compose_create(account, COMPOSE_REEDIT);
	compose->targetinfo = procmsg_msginfo_copy(msginfo);

	if (compose_parse_header(compose, msginfo) < 0) return;

	undo_block(compose->undostruct);

	compose_reedit_set_entry(compose, msginfo);

	text = GTK_TEXT_VIEW(compose->text);
	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	if ((fp = procmime_get_first_text_content(msginfo)) == NULL)
		g_warning(_("Can't get text part\n"));
	else {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			strcrchomp(buf);
			gtk_text_buffer_insert(buffer, &iter, buf, -1);
		}
		fclose(fp);
	}
	compose_attach_parts(compose, msginfo);

	gtk_widget_grab_focus(compose->text);

	undo_unblock(compose->undostruct);

	compose_connect_changed_callbacks(compose);

	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);
}

GList *compose_get_compose_list(void)
{
	return compose_list;
}

static void compose_entry_show(Compose *compose, ComposeEntryType type)
{
	GtkItemFactory *ifactory;

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	switch (type) {
	case COMPOSE_ENTRY_CC:
		menu_set_active(ifactory, "/View/Cc", TRUE);
		break;
	case COMPOSE_ENTRY_BCC:
		menu_set_active(ifactory, "/View/Bcc", TRUE);
		break;
	case COMPOSE_ENTRY_REPLY_TO:
		menu_set_active(ifactory, "/View/Reply to", TRUE);
		break;
	case COMPOSE_ENTRY_FOLLOWUP_TO:
		menu_set_active(ifactory, "/View/Followup to", TRUE);
		break;
	case COMPOSE_ENTRY_TO:
		menu_set_active(ifactory, "/View/To", TRUE);
		break;
	default:
		break;
	}
}

static GtkEntry *compose_get_entry(Compose *compose, ComposeEntryType type)
{
	GtkEntry *entry;

	switch (type) {
	case COMPOSE_ENTRY_CC:
		entry = GTK_ENTRY(compose->cc_entry);
		break;
	case COMPOSE_ENTRY_BCC:
		entry = GTK_ENTRY(compose->bcc_entry);
		break;
	case COMPOSE_ENTRY_REPLY_TO:
		entry = GTK_ENTRY(compose->reply_entry);
		break;
	case COMPOSE_ENTRY_SUBJECT:
		entry = GTK_ENTRY(compose->subject_entry);
		break;
	case COMPOSE_ENTRY_NEWSGROUPS:
		entry = GTK_ENTRY(compose->newsgroups_entry);
		break;
	case COMPOSE_ENTRY_FOLLOWUP_TO:
		entry = GTK_ENTRY(compose->followup_entry);
		break;
	case COMPOSE_ENTRY_TO:
	default:
		entry = GTK_ENTRY(compose->to_entry);
		break;
	}

	return entry;
}

void compose_entry_set(Compose *compose, const gchar *text,
		       ComposeEntryType type)
{
	GtkEntry *entry;

	if (!text) return;

	compose_entry_show(compose, type);
	entry = compose_get_entry(compose, type);

	gtk_entry_set_text(entry, text);
}

void compose_entry_append(Compose *compose, const gchar *text,
			  ComposeEntryType type)
{
	GtkEntry *entry;
	const gchar *str;
	gint pos;

	if (!text || *text == '\0') return;

	compose_entry_show(compose, type);
	entry = compose_get_entry(compose, type);

	if (type != COMPOSE_ENTRY_SUBJECT) {
		str = gtk_entry_get_text(entry);
		if (*str != '\0') {
			pos = entry->text_length;
			gtk_editable_insert_text(GTK_EDITABLE(entry),
						 ", ", -1, &pos);
		}
	}

	pos = entry->text_length;
	gtk_editable_insert_text(GTK_EDITABLE(entry), text, -1, &pos);
}

static void compose_entries_set(Compose *compose, const gchar *mailto)
{
	gchar *to = NULL;
	gchar *cc = NULL;
	gchar *bcc = NULL;
	gchar *subject = NULL;
	gchar *body = NULL;

	scan_mailto_url(mailto, &to, &cc, &bcc, &subject, &body);

	if (to)
		compose_entry_set(compose, to, COMPOSE_ENTRY_TO);
	if (cc)
		compose_entry_set(compose, cc, COMPOSE_ENTRY_CC);
	if (bcc)
		compose_entry_set(compose, bcc, COMPOSE_ENTRY_BCC);
	if (subject)
		compose_entry_set(compose, subject, COMPOSE_ENTRY_SUBJECT);
	if (body) {
		GtkTextView *text = GTK_TEXT_VIEW(compose->text);
		GtkTextBuffer *buffer;
		GtkTextMark *mark;
		GtkTextIter iter;

		buffer = gtk_text_view_get_buffer(text);
		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

		gtk_text_buffer_insert(buffer, &iter, body, -1);
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

	g_free(to);
	g_free(cc);
	g_free(bcc);
	g_free(subject);
	g_free(body);
}

static void compose_entries_set_from_item(Compose *compose, FolderItem *item,
					  ComposeMode mode)
{
	g_return_if_fail(item != NULL);

	if (item->auto_to) {
		if (mode != COMPOSE_REPLY || item->use_auto_to_on_reply)
			compose_entry_set(compose, item->auto_to,
					  COMPOSE_ENTRY_TO);
	}
	if (item->auto_cc)
		compose_entry_set(compose, item->auto_cc, COMPOSE_ENTRY_CC);
	if (item->auto_bcc)
		compose_entry_set(compose, item->auto_bcc, COMPOSE_ENTRY_BCC);
	if (item->auto_replyto)
		compose_entry_set(compose, item->auto_replyto,
				  COMPOSE_ENTRY_REPLY_TO);
}

#undef ACTIVATE_MENU

static gint compose_parse_header(Compose *compose, MsgInfo *msginfo)
{
	static HeaderEntry hentry[] = {{"Reply-To:",	NULL, TRUE},
				       {"Cc:",		NULL, TRUE},
				       {"References:",	NULL, FALSE},
				       {"Bcc:",		NULL, TRUE},
				       {"Newsgroups:",  NULL, TRUE},
				       {"Followup-To:", NULL, TRUE},
				       {"List-Post:",   NULL, FALSE},
				       {NULL,		NULL, FALSE}};

	enum
	{
		H_REPLY_TO	= 0,
		H_CC		= 1,
		H_REFERENCES	= 2,
		H_BCC		= 3,
		H_NEWSGROUPS    = 4,
		H_FOLLOWUP_TO	= 5,
		H_LIST_POST     = 6
	};

	FILE *fp;

	g_return_val_if_fail(msginfo != NULL, -1);

	if ((fp = procmsg_open_message(msginfo)) == NULL) return -1;
	procheader_get_header_fields(fp, hentry);
	fclose(fp);

	if (hentry[H_REPLY_TO].body != NULL) {
		conv_unmime_header_overwrite(hentry[H_REPLY_TO].body);
		compose->replyto = hentry[H_REPLY_TO].body;
		hentry[H_REPLY_TO].body = NULL;
	}
	if (hentry[H_CC].body != NULL) {
		conv_unmime_header_overwrite(hentry[H_CC].body);
		compose->cc = hentry[H_CC].body;
		hentry[H_CC].body = NULL;
	}
	if (hentry[H_REFERENCES].body != NULL) {
		if (compose->mode == COMPOSE_REEDIT)
			compose->references = hentry[H_REFERENCES].body;
		else {
			compose->references = compose_parse_references
				(hentry[H_REFERENCES].body, msginfo->msgid);
			g_free(hentry[H_REFERENCES].body);
		}
		hentry[H_REFERENCES].body = NULL;
	}
	if (hentry[H_BCC].body != NULL) {
		if (compose->mode == COMPOSE_REEDIT) {
			conv_unmime_header_overwrite(hentry[H_BCC].body);
			compose->bcc = hentry[H_BCC].body;
		} else
			g_free(hentry[H_BCC].body);
		hentry[H_BCC].body = NULL;
	}
	if (hentry[H_NEWSGROUPS].body != NULL) {
		compose->newsgroups = hentry[H_NEWSGROUPS].body;
		hentry[H_NEWSGROUPS].body = NULL;
	}
	if (hentry[H_FOLLOWUP_TO].body != NULL) {
		conv_unmime_header_overwrite(hentry[H_FOLLOWUP_TO].body);
		compose->followup_to = hentry[H_FOLLOWUP_TO].body;
		hentry[H_FOLLOWUP_TO].body = NULL;
	}
	if (hentry[H_LIST_POST].body != NULL) {
		gchar *to = NULL;

		extract_address(hentry[H_LIST_POST].body);
		if (hentry[H_LIST_POST].body[0] != '\0') {
			scan_mailto_url(hentry[H_LIST_POST].body,
					&to, NULL, NULL, NULL, NULL);
			if (to) {
				g_free(compose->ml_post);
				compose->ml_post = to;
			}
		}
		g_free(hentry[H_LIST_POST].body);
		hentry[H_LIST_POST].body = NULL;
	}

	if (compose->mode == COMPOSE_REEDIT && msginfo->inreplyto)
		compose->inreplyto = g_strdup(msginfo->inreplyto);
	else if (compose->mode != COMPOSE_REEDIT &&
		 msginfo->msgid && *msginfo->msgid) {
		compose->inreplyto = g_strdup(msginfo->msgid);

		if (!compose->references) {
			if (msginfo->inreplyto && *msginfo->inreplyto)
				compose->references =
					g_strdup_printf("<%s>\n\t<%s>",
							msginfo->inreplyto,
							msginfo->msgid);
			else
				compose->references =
					g_strconcat("<", msginfo->msgid, ">",
						    NULL);
		}
	}

	return 0;
}

static gchar *compose_parse_references(const gchar *ref, const gchar *msgid)
{
	GSList *ref_id_list, *cur;
	GString *new_ref;
	gchar *new_ref_str;

	ref_id_list = references_list_append(NULL, ref);
	if (!ref_id_list) return NULL;
	if (msgid && *msgid)
		ref_id_list = g_slist_append(ref_id_list, g_strdup(msgid));

	for (;;) {
		gint len = 0;

		for (cur = ref_id_list; cur != NULL; cur = cur->next)
			/* "<" + Message-ID + ">" + CR+LF+TAB */
			len += strlen((gchar *)cur->data) + 5;

		if (len > MAX_REFERENCES_LEN) {
			/* remove second message-ID */
			if (ref_id_list && ref_id_list->next &&
			    ref_id_list->next->next) {
				g_free(ref_id_list->next->data);
				ref_id_list = g_slist_remove
					(ref_id_list, ref_id_list->next->data);
			} else {
				slist_free_strings(ref_id_list);
				g_slist_free(ref_id_list);
				return NULL;
			}
		} else
			break;
	}

	new_ref = g_string_new("");
	for (cur = ref_id_list; cur != NULL; cur = cur->next) {
		if (new_ref->len > 0)
			g_string_append(new_ref, "\n\t");
		g_string_sprintfa(new_ref, "<%s>", (gchar *)cur->data);
	}

	slist_free_strings(ref_id_list);
	g_slist_free(ref_id_list);

	new_ref_str = new_ref->str;
	g_string_free(new_ref, FALSE);

	return new_ref_str;
}

static gchar *compose_quote_fmt(Compose *compose, MsgInfo *msginfo,
				const gchar *fmt, const gchar *qmark,
				const gchar *body)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	static MsgInfo dummyinfo;
	gchar *quote_str = NULL;
	gchar *buf;
	gchar *p, *lastp;
	gint len;

	if (!msginfo)
		msginfo = &dummyinfo;

	if (qmark != NULL) {
		quote_fmt_init(msginfo, NULL, NULL);
		quote_fmt_scan_string(qmark);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL)
			alertpanel_error(_("Quote mark format error."));
		else
			Xstrdup_a(quote_str, buf, return NULL)
	}

	if (fmt && *fmt != '\0') {
		quote_fmt_init(msginfo, quote_str, body);
		quote_fmt_scan_string(fmt);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL) {
			alertpanel_error(_("Message reply/forward format error."));
			return NULL;
		}
	} else
		buf = "";

	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	for (p = buf; *p != '\0'; ) {
		lastp = strchr(p, '\n');
		len = lastp ? lastp - p + 1 : -1;
		gtk_text_buffer_insert(buffer, &iter, p, len);
		if (lastp)
			p = lastp + 1;
		else
			break;
	}

	return buf;
}

static void compose_reply_set_entry(Compose *compose, MsgInfo *msginfo,
				    ComposeMode mode)
{
	GSList *cc_list = NULL;
	GSList *cur;
	gchar *from = NULL;
	gchar *replyto = NULL;
	GHashTable *to_table;
	gboolean to_all = FALSE, to_ml = FALSE, ignore_replyto = FALSE;

	g_return_if_fail(compose->account != NULL);
	g_return_if_fail(msginfo != NULL);

	switch (COMPOSE_MODE(mode)) {
	case COMPOSE_REPLY_TO_SENDER:
		ignore_replyto = TRUE;
		break;
	case COMPOSE_REPLY_TO_ALL:
		to_all = TRUE;
		break;
	case COMPOSE_REPLY_TO_LIST:
		to_ml = TRUE;
		break;
	default:
		break;
	}

	if (compose->account->protocol != A_NNTP) {
		if (!compose->replyto && to_ml && compose->ml_post)
			compose_entry_set(compose, compose->ml_post,
					  COMPOSE_ENTRY_TO);
		else
			compose_entry_set(compose, 
				 (compose->replyto && !ignore_replyto)
				 ? compose->replyto
				 : msginfo->from ? msginfo->from : "",
				 COMPOSE_ENTRY_TO);
	} else {
		if (ignore_replyto) {
			compose_entry_set(compose,
					  msginfo->from ? msginfo->from : "",
					  COMPOSE_ENTRY_TO);
		} else {
			compose_entry_set(compose,
					  compose->followup_to ? compose->followup_to
					  : compose->newsgroups ? compose->newsgroups
					  : "",
					  COMPOSE_ENTRY_NEWSGROUPS);
		}
	}

	if (msginfo->subject && *msginfo->subject) {
		gchar *buf;
		guchar *p;

		buf = g_strdup(msginfo->subject);

		if (msginfo->folder && msginfo->folder->trim_compose_subject)
			trim_subject(buf);

		while (!strncasecmp(buf, "Re:", 3)) {
			p = buf + 3;
			while (isspace(*p)) p++;
			memmove(buf, p, strlen(p) + 1);
		}

		compose_entry_set(compose, "Re: ", COMPOSE_ENTRY_SUBJECT);
		compose_entry_append(compose, buf, COMPOSE_ENTRY_SUBJECT);

		g_free(buf);
	} else
		compose_entry_set(compose, "Re: ", COMPOSE_ENTRY_SUBJECT);

	if (!compose->replyto && to_ml && compose->ml_post) return;
	if (!to_all || compose->account->protocol == A_NNTP) return;

	if (compose->replyto) {
		Xstrdup_a(replyto, compose->replyto, return);
		extract_address(replyto);
	}
	if (msginfo->from) {
		Xstrdup_a(from, msginfo->from, return);
		extract_address(from);
	}

	if (replyto && from)
		cc_list = address_list_append(cc_list, from);
	cc_list = address_list_append(cc_list, msginfo->to);
	cc_list = address_list_append(cc_list, compose->cc);

	to_table = g_hash_table_new(g_str_hash, g_str_equal);
	if (replyto)
		g_hash_table_insert(to_table, replyto, GINT_TO_POINTER(1));
	if (compose->account)
		g_hash_table_insert(to_table, compose->account->address,
				    GINT_TO_POINTER(1));

	/* remove address on To: and that of current account */
	for (cur = cc_list; cur != NULL; ) {
		GSList *next = cur->next;

		if (g_hash_table_lookup(to_table, cur->data) != NULL)
			cc_list = g_slist_remove(cc_list, cur->data);
		else
			g_hash_table_insert(to_table, cur->data, cur);

		cur = next;
	}
	g_hash_table_destroy(to_table);

	if (cc_list) {
		for (cur = cc_list; cur != NULL; cur = cur->next)
			compose_entry_append(compose, (gchar *)cur->data,
					     COMPOSE_ENTRY_CC);
		slist_free_strings(cc_list);
		g_slist_free(cc_list);
	}
}

static void compose_reedit_set_entry(Compose *compose, MsgInfo *msginfo)
{
	g_return_if_fail(msginfo != NULL);

	compose_entry_set(compose, msginfo->to     , COMPOSE_ENTRY_TO);
	compose_entry_set(compose, compose->cc     , COMPOSE_ENTRY_CC);
	compose_entry_set(compose, compose->bcc    , COMPOSE_ENTRY_BCC);
	compose_entry_set(compose, compose->replyto, COMPOSE_ENTRY_REPLY_TO);
	compose_entry_set(compose, msginfo->subject, COMPOSE_ENTRY_SUBJECT);
}

static void compose_insert_sig(Compose *compose, gboolean replace)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	gint cur_pos;

	g_return_if_fail(compose->account != NULL);

	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	cur_pos = gtk_text_iter_get_offset(&iter);

	if (replace)
		gtk_text_buffer_get_end_iter(buffer, &iter);

	if (replace && compose->sig_str) {
		GtkTextIter first_iter, start_iter, end_iter;
		gboolean found;

		gtk_text_buffer_get_start_iter(buffer, &first_iter);

		if (compose->sig_str[0] == '\0')
			found = FALSE;
		else
			found = gtk_text_iter_forward_search
				(&first_iter, compose->sig_str,
				 GTK_TEXT_SEARCH_TEXT_ONLY,
				 &start_iter, &end_iter, NULL);

		if (found) {
			gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
			iter = start_iter;
		}
	}

	g_free(compose->sig_str);
	compose->sig_str = compose_get_signature_str(compose);
	if (compose->sig_str) {
		if (!replace)
			gtk_text_buffer_insert(buffer, &iter, "\n\n", 2);
		gtk_text_buffer_insert(buffer, &iter, compose->sig_str, -1);
	} else
		compose->sig_str = g_strdup("");

	if (cur_pos > gtk_text_buffer_get_char_count(buffer))
		cur_pos = gtk_text_buffer_get_char_count(buffer);

	gtk_text_buffer_get_iter_at_offset(buffer, &iter, cur_pos);
	gtk_text_buffer_move_mark(buffer, mark, &iter);
}

static gchar *compose_get_signature_str(Compose *compose)
{
	gchar *sig_body = NULL;
	gchar *sig_str = NULL;
	gchar *utf8_sig_str = NULL;

	g_return_val_if_fail(compose->account != NULL, NULL);

	if (!compose->account->sig_path)
		return NULL;

	if (compose->account->sig_type == SIG_FILE) {
		if (!is_file_or_fifo_exist(compose->account->sig_path)) {
			g_warning("can't open signature file: %s\n",
				  compose->account->sig_path);
			return NULL;
		}
	}

	if (compose->account->sig_type == SIG_COMMAND)
		sig_body = get_command_output(compose->account->sig_path);
	else {
		gchar *tmp;

		tmp = file_read_to_str(compose->account->sig_path);
		if (!tmp)
			return NULL;
		sig_body = normalize_newlines(tmp);
		g_free(tmp);
	}

	if (prefs_common.sig_sep) {
		sig_str = g_strconcat(prefs_common.sig_sep, "\n", sig_body,
				      NULL);
		g_free(sig_body);
	} else
		sig_str = sig_body;

	if (sig_str) {
		utf8_sig_str = conv_codeset_strdup
			(sig_str,
			 conv_get_locale_charset_str(),
			 conv_get_internal_charset_str());
		g_free(sig_str);
	}

	return utf8_sig_str;
}

static void compose_insert_file(Compose *compose, const gchar *file)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	const gchar *cur_encoding;
	gchar buf[BUFFSIZE];
	gint len;
	FILE *fp;

	g_return_if_fail(file != NULL);

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return;
	}

	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	cur_encoding = conv_get_locale_charset_str();

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *str;

		str = conv_codeset_strdup
			(buf, cur_encoding, conv_get_internal_charset_str());
		if (!str) continue;

		/* strip <CR> if DOS/Windows file,
		   replace <CR> with <LF> if Macintosh file. */
		strcrchomp(str);
		len = strlen(str);
		if (len > 0 && str[len - 1] != '\n') {
			while (--len >= 0)
				if (str[len] == '\r') str[len] = '\n';
		}

		gtk_text_buffer_insert(buffer, &iter, str, -1);
		g_free(str);
	}

	fclose(fp);
}

static void compose_attach_append(Compose *compose, const gchar *file,
				  const gchar *filename,
				  const gchar *content_type)
{
	AttachInfo *ainfo;
	gchar *text[N_ATTACH_COLS];
	FILE *fp;
	off_t size;
	gint row;

	g_return_if_fail(file != NULL);
	g_return_if_fail(*file != '\0');

	if (!is_file_exist(file)) {
		g_warning(_("File %s doesn't exist\n"), file);
		return;
	}
	if ((size = get_file_size(file)) < 0) {
		g_warning(_("Can't get file size of %s\n"), file);
		return;
	}
	if (size == 0) {
		alertpanel_notice(_("File %s is empty."), file);
		return;
	}
	if ((fp = fopen(file, "rb")) == NULL) {
		alertpanel_error(_("Can't read %s."), file);
		return;
	}
	fclose(fp);

	compose_changed_cb(NULL, compose);

	if (!compose->use_attach) {
		GtkItemFactory *ifactory;

		ifactory = gtk_item_factory_from_widget(compose->menubar);
		menu_set_active(ifactory, "/View/Attachment", TRUE);
	}

	ainfo = g_new0(AttachInfo, 1);
	ainfo->file = g_strdup(file);

	if (content_type) {
		ainfo->content_type = g_strdup(content_type);
		if (!strcasecmp(content_type, "message/rfc822")) {
			MsgInfo *msginfo;
			MsgFlags flags = {0, 0};
			const gchar *name;

			if (procmime_get_encoding_for_text_file(file) == ENC_7BIT)
				ainfo->encoding = ENC_7BIT;
			else
				ainfo->encoding = ENC_8BIT;

			msginfo = procheader_parse_file(file, flags, FALSE);
			if (msginfo && msginfo->subject)
				name = msginfo->subject;
			else
				name = g_basename(filename ? filename : file);

			ainfo->name = g_strdup_printf(_("Message: %s"), name);

			procmsg_msginfo_free(msginfo);
		} else {
			if (!g_strncasecmp(content_type, "text", 4))
				ainfo->encoding = procmime_get_encoding_for_text_file(file);
			else
				ainfo->encoding = ENC_BASE64;
			ainfo->name = g_strdup
				(g_basename(filename ? filename : file));
		}
	} else {
		ainfo->content_type = procmime_get_mime_type(file);
		if (!ainfo->content_type) {
			ainfo->content_type =
				g_strdup("application/octet-stream");
			ainfo->encoding = ENC_BASE64;
		} else if (!g_strncasecmp(ainfo->content_type, "text", 4))
			ainfo->encoding =
				procmime_get_encoding_for_text_file(file);
		else
			ainfo->encoding = ENC_BASE64;
		ainfo->name = g_strdup(g_basename(filename ? filename : file));	
	}
	ainfo->size = size;

	text[COL_MIMETYPE] = ainfo->content_type;
	text[COL_SIZE] = to_human_readable(size);
	text[COL_NAME] = ainfo->name;

	row = gtk_clist_append(GTK_CLIST(compose->attach_clist), text);
	gtk_clist_set_row_data(GTK_CLIST(compose->attach_clist), row, ainfo);
}

#define IS_FIRST_PART_TEXT(info) \
	((info->mime_type == MIME_TEXT || info->mime_type == MIME_TEXT_HTML) || \
	 (info->mime_type == MIME_MULTIPART && info->content_type && \
	  !strcasecmp(info->content_type, "multipart/alternative") && \
	  (info->children && \
	   (info->children->mime_type == MIME_TEXT || \
	    info->children->mime_type == MIME_TEXT_HTML))))

static void compose_attach_parts(Compose *compose, MsgInfo *msginfo)
{
	MimeInfo *mimeinfo;
	MimeInfo *child;
	gchar *infile;
	gchar *outfile;

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) return;

	/* skip first text (presumably message body) */
	child = mimeinfo->children;
	if (!child || IS_FIRST_PART_TEXT(mimeinfo)) {
		procmime_mimeinfo_free_all(mimeinfo);
		return;
	}
	if (IS_FIRST_PART_TEXT(child))
		child = child->next;

	infile = procmsg_get_message_file_path(msginfo);

	while (child != NULL) {
		if (child->children || child->mime_type == MIME_MULTIPART) {
			child = procmime_mimeinfo_next(child);
			continue;
		}

		outfile = procmime_get_tmp_file_name(child);
		if (procmime_get_part(outfile, infile, child) < 0)
			g_warning(_("Can't get the part of multipart message."));
		else
			compose_attach_append
				(compose, outfile,
				 child->filename ? child->filename : child->name,
				 child->content_type);

		child = child->next;
	}

	g_free(infile);
	procmime_mimeinfo_free_all(mimeinfo);
}

#undef IS_FIRST_PART_TEXT

#define CHAR_BUF_SIZE	8

#define GET_CHAR(iter_p, buf, len)					     \
{									     \
	GtkTextIter end_iter;						     \
	gchar *tmp;							     \
	end_iter = *iter_p;						     \
									     \
	gtk_text_iter_forward_char(&end_iter);				     \
	tmp = gtk_text_buffer_get_text(textbuf, iter_p, &end_iter, FALSE);   \
	if (tmp) {							     \
		glong items_read, items_witten;				     \
		GError *error = NULL;					     \
		gunichar *wide_char;					     \
									     \
		strncpy2(buf, tmp, CHAR_BUF_SIZE);			     \
		wide_char = g_utf8_to_ucs4(tmp, -1,			     \
					   &items_read, &items_witten,	     \
					   &error);			     \
		if (error != NULL) {					     \
			g_warning("%s\n", error->message);		     \
			g_error_free(error);				     \
		}							     \
		len = wide_char && g_unichar_iswide(*wide_char) ? 2 : 1;     \
		g_free(wide_char);					     \
	} else {							     \
		buf[0] = '\0';						     \
		len = 1;						     \
	}								     \
	g_free(tmp);							     \
}

#define INDENT_CHARS	">|#"
#define SPACE_CHARS	" \t"

#warning FIXME_GTK2
static void compose_wrap_line(Compose *compose)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *textbuf;
	GtkTextMark *mark;
	GtkTextIter insert_iter, iter;
	gint ch_len, last_ch_len;
	gchar cbuf[CHAR_BUF_SIZE], last_ch;
	guint text_len;
	gint p_start, p_end;
	gint line_pos, cur_pos;
	gint line_len, cur_len;
	gboolean line_end;
	gboolean quoted;

	textbuf = gtk_text_view_get_buffer(text);
	text_len = gtk_text_buffer_get_char_count(textbuf);
	mark = gtk_text_buffer_get_insert(textbuf);
	gtk_text_buffer_get_iter_at_mark(textbuf, &insert_iter, mark);
	cur_pos = gtk_text_iter_get_offset(&insert_iter);
	GET_CHAR(&insert_iter, cbuf, ch_len);

	if ((ch_len == 1 && *cbuf == '\n') || cur_pos == text_len) {
		GtkTextIter prev_iter;

		if (cur_pos == 0)
			return; /* on the paragraph mark */
		prev_iter = insert_iter;
		gtk_text_iter_backward_char(&prev_iter);
		GET_CHAR(&prev_iter, cbuf, ch_len);
		if (ch_len == 1 && *cbuf == '\n')
			return; /* on the paragraph mark */
	}

	/* find paragraph start. */
	line_end = quoted = FALSE;
	for (iter = insert_iter; gtk_text_iter_backward_char(&iter); ) {
		GET_CHAR(&iter, cbuf, ch_len);
		if (ch_len == 1 && *cbuf == '\n') {
			if (quoted)
				return; /* quoted part */
			if (line_end) {
				gtk_text_iter_forward_chars(&iter, 2);
				break;
			}
			line_end = TRUE;
		} else {
			if (ch_len == 1 && strchr(INDENT_CHARS, *cbuf))
				quoted = TRUE;
			else if (ch_len != 1 || !isspace(*(guchar *)cbuf))
				quoted = FALSE;

			line_end = FALSE;
		}
	}
	p_start = gtk_text_iter_get_offset(&iter);

	/* find paragraph end. */
	line_end = FALSE;
	for (iter = insert_iter; gtk_text_iter_forward_char(&iter); ) {
		GET_CHAR(&iter, cbuf, ch_len);
		if (ch_len == 1 && *cbuf == '\n') {
			if (line_end) {
				gtk_text_iter_backward_char(&iter);
				break;
			}
			line_end = TRUE;
		} else {
			if (line_end && ch_len == 1 &&
			    strchr(INDENT_CHARS, *cbuf))
				return; /* quoted part */

			line_end = FALSE;
		}
	}
	p_end = gtk_text_iter_get_offset(&iter);

	if (p_end >= text_len)
		p_end = text_len;

	if (p_start >= p_end)
		return;

	line_len = cur_len = 0;
	last_ch_len = 0;
	last_ch = '\0';
	line_pos = p_start;
	for (cur_pos = p_start; cur_pos < p_end; cur_pos++) {
		gboolean space = FALSE;

		gtk_text_buffer_get_iter_at_offset(textbuf, &iter, cur_pos);
		GET_CHAR(&iter, cbuf, ch_len);

		if (ch_len < 0) {
			cbuf[0] = '\0';
			ch_len = 1;
		}

		if (ch_len == 1 && isspace(*(guchar *)cbuf))
			space = TRUE;

		if (ch_len == 1 && *cbuf == '\n') {
			gboolean replace = FALSE;
			GtkTextIter next_iter = iter;

			gtk_text_iter_forward_char(&next_iter);

			if (last_ch_len == 1 && !isspace((guchar)last_ch)) {
				if (cur_pos + 1 < p_end) {
					GET_CHAR(&next_iter, cbuf, ch_len);
					if (ch_len == 1 &&
					    !isspace(*(guchar *)cbuf))
						replace = TRUE;
				}
			}
			gtk_text_buffer_delete(textbuf, &iter, &next_iter);
			if (replace) {
				gtk_text_buffer_insert(textbuf, &iter, " ", 1);
				space = TRUE;
			} else {
				p_end--;
				cur_pos--;
				gtk_text_buffer_get_iter_at_offset
					(textbuf, &iter, cur_pos);
				continue;
			}
		}

		last_ch_len = ch_len;
		last_ch = *cbuf;

		if (space) {
			line_pos = cur_pos + 1;
			line_len = cur_len + ch_len;
		}

		gtk_text_buffer_get_iter_at_offset(textbuf, &iter, line_pos);

		if (cur_len + ch_len > prefs_common.linewrap_len &&
		    line_len > 0) {
			gint tlen = ch_len;
			GtkTextIter prev_iter = iter;

			gtk_text_iter_backward_char(&prev_iter);

			if (ch_len == 1 && isspace(*(guchar *)cbuf)) {
				gtk_text_buffer_delete
					(textbuf, &prev_iter, &iter);
				iter = prev_iter;
				p_end--;
				cur_pos--;
				line_pos--;
				cur_len--;
				line_len--;
			}
			ch_len = tlen;

			gtk_text_buffer_insert(textbuf, &iter, "\n", 1);
			p_end++;
			cur_pos++;
			line_pos++;
			cur_len = cur_len - line_len + ch_len;
			line_len = 0;
			continue;
		}

		if (ch_len > 1) {
			line_pos = cur_pos + 1;
			line_len = cur_len + ch_len;
		}
		cur_len += ch_len;
	}
}

#undef WRAP_DEBUG
#ifdef WRAP_DEBUG
/* Darko: used when I debug wrapping */
void dump_text(GtkTextBuffer *textbuf, int pos, int tlen, int breakoncr)
{
	gint clen;
	gchar cbuf[CHAR_BUF_SIZE];

	printf("%d [", pos);
	gtk_text_buffer_get_iter_at_offset(textbuf, &iter, pos);
	gtk_text_buffer_get_iter_at_offset(textbuf, &end_iter, pos + tlen);
	for (; gtk_text_iter_forward_char(&iter) &&
	     gtk_text_iter_compare(&iter, &end_iter) < 0; ) {
		GET_CHAR(&iter, cbuf, clen);
		if (clen < 0) break;
		if (breakoncr && clen == 1 && cbuf[0] == '\n')
			break;
		fwrite(cbuf, clen, 1, stdout);
	}
	printf("]\n");
}
#endif

typedef enum {
	WAIT_FOR_SPACE,
	WAIT_FOR_INDENT_CHAR,
	WAIT_FOR_INDENT_CHAR_OR_SPACE
} IndentState;

/* return indent length, we allow:
   > followed by spaces/tabs
   | followed by spaces/tabs
   uppercase characters immediately followed by >,
   and the repeating sequences of the above */
/* return indent length */
static guint get_indent_length(GtkTextBuffer *textbuf, guint start_pos,
			       guint text_len)
{
	guint i_len = 0;
	guint i, ch_len, alnum_cnt = 0;
	IndentState state = WAIT_FOR_INDENT_CHAR;
	gchar cbuf[CHAR_BUF_SIZE];
	gboolean is_space;
	gboolean is_indent;

#warning FIXME_GTK2 use GtkTextIter
	for (i = start_pos; i < text_len; i++) {
		GtkTextIter iter;

		gtk_text_buffer_get_iter_at_offset(textbuf, &iter, i);
		GET_CHAR(&iter, cbuf, ch_len);
		if (ch_len > 1)
			break;

		if (cbuf[0] == '\n')
			break;

		is_indent = strchr(INDENT_CHARS, cbuf[0]) ? TRUE : FALSE;
		is_space = strchr(SPACE_CHARS, cbuf[0]) ? TRUE : FALSE;

		switch (state) {
		case WAIT_FOR_SPACE:
			if (is_space == FALSE)
				goto out;
			state = WAIT_FOR_INDENT_CHAR_OR_SPACE;
			break;
		case WAIT_FOR_INDENT_CHAR_OR_SPACE:
			if (is_indent == FALSE && is_space == FALSE &&
			    !isupper((guchar)cbuf[0]))
				goto out;
			if (is_space == TRUE) {
				alnum_cnt = 0;
				state = WAIT_FOR_INDENT_CHAR_OR_SPACE;
			} else if (is_indent == TRUE) {
				alnum_cnt = 0;
				state = WAIT_FOR_SPACE;
			} else {
				alnum_cnt++;
				state = WAIT_FOR_INDENT_CHAR;
			}
			break;
		case WAIT_FOR_INDENT_CHAR:
			if (is_indent == FALSE && !isupper((guchar)cbuf[0]))
				goto out;
			if (is_indent == TRUE) {
				alnum_cnt = 0;
				state = WAIT_FOR_SPACE;
			} else {
				alnum_cnt++;
			}
			break;
		}

		i_len++;
	}

out:
	if ((i_len > 0) && (state == WAIT_FOR_INDENT_CHAR))
		i_len -= alnum_cnt;

	return i_len;
}

/* insert quotation string when line was wrapped */
static guint ins_quote(GtkTextBuffer *textbuf, GtkTextIter *iter,
		       guint indent_len,
		       guint prev_line_pos, guint text_len,
		       gchar *quote_fmt)
{
	guint ins_len = 0;

	if (indent_len) {
		GtkTextIter iter1, iter2;
		gchar *text;

		gtk_text_buffer_get_iter_at_offset(textbuf, &iter1,
						   prev_line_pos);
		gtk_text_buffer_get_iter_at_offset(textbuf, &iter2,
						   prev_line_pos + indent_len);
		text = gtk_text_buffer_get_text(textbuf, &iter1, &iter2, FALSE);
		if (!text) return 0;

		gtk_text_buffer_insert(textbuf, iter, text, -1);
		ins_len = g_utf8_strlen(text, -1);

		g_free(text);
	}

	return ins_len;
}

/* check if we should join the next line */
static gboolean join_next_line_is_needed(GtkTextBuffer *textbuf,
					 guint start_pos, guint tlen,
					 guint prev_ilen, gboolean autowrap)
{
	guint indent_len, ch_len;
	gboolean do_join = FALSE;
	gchar cbuf[CHAR_BUF_SIZE];

	indent_len = get_indent_length(textbuf, start_pos, tlen);

	if ((autowrap || indent_len > 0) && indent_len == prev_ilen) {
		GtkTextIter iter;

		gtk_text_buffer_get_iter_at_offset(textbuf, &iter,
						   start_pos + indent_len);
		GET_CHAR(&iter, cbuf, ch_len);
		if (ch_len > 0 && (cbuf[0] != '\n'))
			do_join = TRUE;
	}

	return do_join;
}

static void compose_wrap_line_all(Compose *compose)
{
	compose_wrap_line_all_full(compose, FALSE);
}

static void compose_wrap_line_all_full(Compose *compose, gboolean autowrap)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *textbuf;
	GtkTextIter iter, end_iter;
	guint tlen;
	guint line_pos = 0, cur_pos = 0, p_pos = 0;
	gint line_len = 0, cur_len = 0;
	gint ch_len;
	gboolean is_new_line = TRUE, do_delete = FALSE;
	guint i_len = 0;
	gboolean linewrap_quote = TRUE;
	gboolean set_editable_pos = FALSE;
	gint editable_pos = 0;
	guint linewrap_len = prefs_common.linewrap_len;
	gchar *qfmt = prefs_common.quotemark;
	gchar cbuf[CHAR_BUF_SIZE];

	textbuf = gtk_text_view_get_buffer(text);
	tlen = gtk_text_buffer_get_char_count(textbuf);

	for (; cur_pos < tlen; cur_pos++) {
		/* mark position of new line - needed for quotation wrap */
		if (is_new_line) {
			if (linewrap_quote)
				i_len = get_indent_length
					(textbuf, cur_pos, tlen);

			is_new_line = FALSE;
			p_pos = cur_pos;
		}

		gtk_text_buffer_get_iter_at_offset(textbuf, &iter, cur_pos);
		GET_CHAR(&iter, cbuf, ch_len);

		/* fix line length for tabs */
		if (ch_len == 1 && *cbuf == '\t') {
			guint tab_width = 8;
			guint tab_offset = line_len % tab_width;

			if (tab_offset) {
				line_len += tab_width - tab_offset - 1;
				cur_len = line_len;
			}
		}

		/* we have encountered line break */
		if (ch_len == 1 && *cbuf == '\n') {
			gint clen;
			gchar cb[CHAR_BUF_SIZE];

			/* should we join the next line */
			if ((autowrap || i_len != cur_len) && do_delete &&
			    join_next_line_is_needed
				(textbuf, cur_pos + 1, tlen, i_len, autowrap))
				do_delete = TRUE;
			else
				do_delete = FALSE;

			/* skip delete if it is continuous URL */
			if (do_delete && (line_pos - p_pos <= i_len) &&
			    gtkut_text_buffer_is_uri_string(textbuf, line_pos,
							    tlen))
				do_delete = FALSE;

			/* should we delete to perform smart wrapping */
			if (line_len < linewrap_len && do_delete) {
				/* get rid of newline */
				gtk_text_buffer_get_iter_at_offset
					(textbuf, &iter, cur_pos);
				end_iter = iter;
				gtk_text_iter_forward_char(&end_iter);
				gtk_text_buffer_delete
					(textbuf, &iter, &end_iter);
				tlen--;

				/* if text starts with quote fmt or with
				   indent string, delete them */
				if (i_len) {
					guint ilen;
					ilen =  gtkut_text_buffer_str_compare_n
						(textbuf, cur_pos, p_pos, i_len,
						 tlen);
					if (ilen) {
						end_iter = iter;
						gtk_text_iter_forward_chars
							(&end_iter, ilen);
						gtk_text_buffer_delete
							(textbuf,
							 &iter, &end_iter);
						tlen -= ilen;
					}
				}

				gtk_text_buffer_get_iter_at_offset
					(textbuf, &iter, cur_pos);
				GET_CHAR(&iter, cb, clen);
				/* insert space between the next line */
				if (cur_pos > 0) {
					gint clen_prev;
					gchar cb_prev[MB_LEN_MAX];
					GtkTextIter iter_prev = iter;

					gtk_text_iter_backward_char(&iter_prev);
					GET_CHAR(&iter_prev, cb_prev,
						 clen_prev);
					if ((clen_prev != clen && clen > 1) ||
					    (clen == 1 &&
					     !isspace((guchar)cb[0]))) {
						gtk_text_buffer_insert
							(textbuf, &iter,
							 " ", 1);
						tlen++;
					}
				}

				/* and start over with current line */
				cur_pos = p_pos - 1;
				line_pos = cur_pos;
				line_len = cur_len = 0;
				do_delete = FALSE;
				is_new_line = TRUE;
				/* move beginning of line if we are on
				   linebreak */
#warning FIXME_GTK2
				gtk_text_buffer_get_iter_at_offset
					(textbuf, &iter, line_pos);
				GET_CHAR(&iter, cb, clen);
				if (clen == 1 && *cb == '\n')
					line_pos++;
				continue;
			}

			/* mark new line beginning */
			line_pos = cur_pos + 1;
			line_len = cur_len = 0;
			do_delete = FALSE;
			is_new_line = TRUE;
			continue;
		}

		if (ch_len < 0) {
			cbuf[0] = '\0';
			ch_len = 1;
		}

		/* possible line break */
		if (ch_len == 1 && isspace(*(guchar *)cbuf)) {
			line_pos = cur_pos + 1;
			line_len = cur_len + ch_len;
		}

		/* are we over wrapping length set in preferences ? */
		if (cur_len + ch_len > linewrap_len) {
			gint clen;

			/* force wrapping if it is one long word but not URL */
			if (line_pos - p_pos <= i_len)
                        	if (!gtkut_text_buffer_is_uri_string
				    (textbuf, line_pos, tlen))
					line_pos = cur_pos - 1;

			gtk_text_buffer_get_iter_at_offset
				(textbuf, &iter, line_pos);
			GET_CHAR(&iter, cbuf, clen);

			/* if next character is space delete it */
			if (clen == 1 && isspace(*(guchar *)cbuf)) {
				if (p_pos + i_len != line_pos ||
                            	    !gtkut_text_buffer_is_uri_string
					(textbuf, line_pos, tlen)) {
					/* workaround for correct cursor
					   position */
					if (set_editable_pos == FALSE) {
						GtkTextMark *mark;

						mark = gtk_text_buffer_get_insert(textbuf);
						gtk_text_buffer_get_iter_at_mark(textbuf, &iter, mark);
						editable_pos = gtk_text_iter_get_offset(&iter);
						if (editable_pos == line_pos)
							set_editable_pos = TRUE;
					}
					gtk_text_buffer_get_iter_at_offset
						(textbuf, &iter, line_pos);
					end_iter = iter;
					gtk_text_iter_backward_char(&end_iter);
					gtk_text_buffer_delete
						(textbuf, &iter, &end_iter);
					tlen--;
					cur_pos--;
					line_pos--;
					cur_len--;
					line_len--;
				}
			}

			/* if it is URL at beginning of line don't wrap */
			if (p_pos + i_len == line_pos &&
			    gtkut_text_buffer_is_uri_string(textbuf,
							    line_pos, tlen)) {
				continue;
			}

			/* insert CR */
			gtk_text_buffer_get_iter_at_offset
				(textbuf, &iter, line_pos);
			gtk_text_buffer_insert(textbuf, &iter, "\n", 1);
			tlen++;
			line_pos++;
			/* for loop will increase it */
			cur_pos = line_pos - 1;
			/* start over with current line */
			is_new_line = TRUE;
			line_len = cur_len = 0;
			if (autowrap || i_len > 0)
				do_delete = TRUE;
			else
				do_delete = FALSE;

			/* should we insert quotation ? */
			if (linewrap_quote && i_len) {
				/* only if line is not already quoted  */
				if (!gtkut_text_buffer_str_compare
					(textbuf, line_pos, tlen, qfmt)) {
					guint ins_len;

					if (line_pos - p_pos > i_len) {
						ins_len = ins_quote
							(textbuf, &iter, i_len,
							 p_pos, tlen, qfmt);
						tlen += ins_len;
					}
				}
			}
			continue;
		}

		if (ch_len > 1) {
			line_pos = cur_pos + 1;
			line_len = cur_len + ch_len;
		}
		/* advance to next character in buffer */
		cur_len += ch_len;
	}

	if (set_editable_pos && editable_pos <= tlen) {
		gtk_text_buffer_get_iter_at_offset
			(textbuf, &iter, editable_pos);
		gtk_text_buffer_place_cursor(textbuf, &iter);
	}
}

#undef GET_CHAR
#undef CHAR_BUF_SIZE

static void compose_set_title(Compose *compose)
{
	gchar *str;
	gchar *edited;

	edited = compose->modified ? _(" [Edited]") : "";
	if (compose->account && compose->account->address)
		str = g_strdup_printf(_("%s - Compose message%s"),
				      compose->account->address, edited);
	else
		str = g_strdup_printf(_("Compose message%s"), edited);
	gtk_window_set_title(GTK_WINDOW(compose->window), str);
	g_free(str);
}

static void compose_select_account(Compose *compose, PrefsAccount *account,
				   gboolean init)
{
	GtkItemFactory *ifactory;

	g_return_if_fail(account != NULL);

	compose->account = account;

	compose_set_title(compose);

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	if (account->protocol == A_NNTP) {
		gtk_widget_show(compose->newsgroups_hbox);
		gtk_widget_show(compose->newsgroups_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 2, 4);
		compose->use_newsgroups = TRUE;

		menu_set_active(ifactory, "/View/To", FALSE);
		menu_set_sensitive(ifactory, "/View/To", TRUE);
		menu_set_active(ifactory, "/View/Cc", FALSE);
		menu_set_sensitive(ifactory, "/View/Cc", TRUE);
		menu_set_sensitive(ifactory, "/View/Followup to", TRUE);
	} else {
		gtk_widget_hide(compose->newsgroups_hbox);
		gtk_widget_hide(compose->newsgroups_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 2, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_newsgroups = FALSE;

		menu_set_active(ifactory, "/View/To", TRUE);
		menu_set_sensitive(ifactory, "/View/To", FALSE);
		menu_set_active(ifactory, "/View/Cc", TRUE);
		menu_set_sensitive(ifactory, "/View/Cc", FALSE);
		menu_set_active(ifactory, "/View/Followup to", FALSE);
		menu_set_sensitive(ifactory, "/View/Followup to", FALSE);
	}

	if (account->set_autocc) {
		compose_entry_show(compose, COMPOSE_ENTRY_CC);
		if (account->auto_cc && compose->mode != COMPOSE_REEDIT)
			compose_entry_set(compose, account->auto_cc,
					  COMPOSE_ENTRY_CC);
	}
	if (account->set_autobcc) {
		compose_entry_show(compose, COMPOSE_ENTRY_BCC);
		if (account->auto_bcc && compose->mode != COMPOSE_REEDIT)
			compose_entry_set(compose, account->auto_bcc,
					  COMPOSE_ENTRY_BCC);
	}
	if (account->set_autoreplyto) {
		compose_entry_show(compose, COMPOSE_ENTRY_REPLY_TO);
		if (account->auto_replyto && compose->mode != COMPOSE_REEDIT)
			compose_entry_set(compose, account->auto_replyto,
					  COMPOSE_ENTRY_REPLY_TO);
	}

#if USE_GPGME
	if (account->default_sign)
		menu_set_active(ifactory, "/Tools/PGP Sign", TRUE);
	if (account->default_encrypt)
		menu_set_active(ifactory, "/Tools/PGP Encrypt", TRUE);
#endif /* USE_GPGME */

	if (!init && compose->mode != COMPOSE_REDIRECT && prefs_common.auto_sig)
		compose_insert_sig(compose, TRUE);
}

static gboolean compose_check_for_valid_recipient(Compose *compose)
{
	const gchar *to_raw = "", *cc_raw = "", *bcc_raw = "";
	const gchar *newsgroups_raw = "";
	gchar *to, *cc, *bcc;
	gchar *newsgroups;

	if (compose->use_to)
		to_raw = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
	if (compose->use_cc)
		cc_raw = gtk_entry_get_text(GTK_ENTRY(compose->cc_entry));
	if (compose->use_bcc)
		bcc_raw = gtk_entry_get_text(GTK_ENTRY(compose->bcc_entry));
	if (compose->use_newsgroups)
		newsgroups_raw = gtk_entry_get_text
			(GTK_ENTRY(compose->newsgroups_entry));

	Xstrdup_a(to, to_raw, return FALSE);
	Xstrdup_a(cc, cc_raw, return FALSE);
	Xstrdup_a(bcc, bcc_raw, return FALSE);
	Xstrdup_a(newsgroups, newsgroups_raw, return FALSE);
	g_strstrip(to);
	g_strstrip(cc);
	g_strstrip(bcc);
	g_strstrip(newsgroups);

	if (*to == '\0' && *cc == '\0' && *bcc == '\0' && *newsgroups == '\0')
		return FALSE;
	else
		return TRUE;
}

static gboolean compose_check_entries(Compose *compose)
{
	const gchar *str;

	if (compose_check_for_valid_recipient(compose) == FALSE) {
		alertpanel_error(_("Recipient is not specified."));
		return FALSE;
	}

	str = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	if (*str == '\0') {
		AlertValue aval;

		aval = alertpanel(_("Send"),
				  _("Subject is empty. Send it anyway?"),
				  _("Yes"), _("No"), NULL);
		if (aval != G_ALERTDEFAULT)
			return FALSE;
	}

	return TRUE;
}

static gint compose_send(Compose *compose)
{
	gchar tmp[MAXPATHLEN + 1];
	gint ok = 0;
	static gboolean lock = FALSE;

	if (lock) return 1;

	g_return_val_if_fail(compose->account != NULL, -1);

	lock = TRUE;

	if (compose_check_entries(compose) == FALSE) {
		lock = FALSE;
		return 1;
	}

	if (!main_window_toggle_online_if_offline(main_window_get())) {
		lock = FALSE;
		return 1;
	}

	/* write to temporary file */
	g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg.%p",
		   get_tmp_dir(), G_DIR_SEPARATOR, compose);

	if (compose->mode == COMPOSE_REDIRECT) {
		if (compose_redirect_write_to_file(compose, tmp) < 0) {
			lock = FALSE;
			return -1;
		}
	} else {
		if (prefs_common.linewrap_at_send)
			compose_wrap_line_all(compose);

		if (compose_write_to_file(compose, tmp, FALSE) < 0) {
			lock = FALSE;
			return -1;
		}
	}

	if (!compose->to_list && !compose->newsgroup_list) {
		g_warning(_("can't get recipient list."));
		unlink(tmp);
		lock = FALSE;
		return -1;
	}

	if (compose->to_list) {
		PrefsAccount *ac;

		if (compose->account->protocol != A_NNTP)
			ac = compose->account;
		else {
			ac = account_find_from_address(compose->account->address);
			if (!ac) {
				if (cur_account && cur_account->protocol != A_NNTP)
					ac = cur_account;
				else
					ac = account_get_default();
			}
			if (!ac || ac->protocol == A_NNTP) {
				alertpanel_error(_("Account for sending mail is not specified.\n"
						   "Please select a mail account before sending."));
				unlink(tmp);
				lock = FALSE;
				return -1;
			}
		}
		ok = send_message(tmp, ac, compose->to_list);
		statusbar_pop_all();
	}

	if (ok == 0 && compose->newsgroup_list) {
		ok = news_post(FOLDER(compose->account->folder), tmp);
		if (ok < 0) {
			alertpanel_error(_("Error occurred while posting the message to %s ."),
					 compose->account->nntp_server);
			unlink(tmp);
			lock = FALSE;
			return -1;
		}
	}

	if (ok == 0) {
		if (compose->mode == COMPOSE_REEDIT) {
			compose_remove_reedit_target(compose);
			if (compose->targetinfo)
				folderview_update_item
					(compose->targetinfo->folder, TRUE);
		}
		/* save message to outbox */
		if (prefs_common.savemsg) {
			FolderItem *outbox;

			outbox = account_get_special_folder
				(compose->account, F_OUTBOX);
			if (procmsg_save_to_outbox(outbox, tmp, FALSE) < 0)
				alertpanel_error
					(_("Can't save the message to outbox."));
			else
				folderview_update_item(outbox, TRUE);
		}
	}

	unlink(tmp);
	lock = FALSE;
	return ok;
}

#if USE_GPGME
/* interfaces to rfc2015 to keep out the prefs stuff there.
 * returns 0 on success and -1 on error. */
static gint compose_create_signers_list(Compose *compose, GSList **pkey_list)
{
	const gchar *key_id = NULL;
	GSList *key_list;

	switch (compose->account->sign_key) {
	case SIGN_KEY_DEFAULT:
		*pkey_list = NULL;
		return 0;
	case SIGN_KEY_BY_FROM:
		key_id = compose->account->address;
		break;
	case SIGN_KEY_CUSTOM:
		key_id = compose->account->sign_key_id;
		break;
	default:
		break;
	}

	key_list = rfc2015_create_signers_list(key_id);

	if (!key_list) {
		alertpanel_error(_("Could not find any key associated with "
				   "currently selected key id `%s'."), key_id);
		return -1;
	}

	*pkey_list = key_list;
	return 0;
}

/* clearsign message body text */
static gint compose_clearsign_text(Compose *compose, gchar **text)
{
	GSList *key_list;
	gchar *tmp_file;

	tmp_file = get_tmp_file();
	if (str_write_to_file(*text, tmp_file) < 0) {
		g_free(tmp_file);
		return -1;
	}

	if (compose_create_signers_list(compose, &key_list) < 0 ||
	    rfc2015_clearsign(tmp_file, key_list) < 0) {
		unlink(tmp_file);
		g_free(tmp_file);
		return -1;
	}

	g_free(*text);
	*text = file_read_to_str(tmp_file);
	unlink(tmp_file);
	g_free(tmp_file);
	if (*text == NULL)
		return -1;

	return 0;
}
#endif /* USE_GPGME */

static gint compose_write_to_file(Compose *compose, const gchar *file,
				  gboolean is_draft)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	FILE *fp;
	size_t len;
	gchar *chars;
	gchar *buf;
	gchar *canon_buf;
	const gchar *out_codeset;
	EncodingType encoding;

	if ((fp = fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, file) < 0) {
		FILE_OP_ERROR(file, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	/* get all composed text */
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(compose->text));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	chars = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	if (is_ascii_str(chars)) {
		buf = chars;
		chars = NULL;
		out_codeset = CS_US_ASCII;
		encoding = ENC_7BIT;
	} else {
		const gchar *src_codeset;

		out_codeset = conv_get_outgoing_charset_str();
		if (!strcasecmp(out_codeset, CS_US_ASCII))
			out_codeset = CS_ISO_8859_1;

		if (prefs_common.encoding_method == CTE_BASE64)
			encoding = ENC_BASE64;
		else if (prefs_common.encoding_method == CTE_QUOTED_PRINTABLE)
			encoding = ENC_QUOTED_PRINTABLE;
		else if (prefs_common.encoding_method == CTE_8BIT)
			encoding = ENC_8BIT;
		else
			encoding = procmime_get_encoding_for_charset(out_codeset);

#if USE_GPGME
		if (!is_draft &&
		    compose->use_signing && !compose->account->clearsign &&
		    encoding == ENC_8BIT)
			encoding = ENC_BASE64;
#endif

		src_codeset = conv_get_internal_charset_str();

		debug_print("src encoding = %s, out encoding = %s, transfer encoding = %s\n",
			    src_codeset, out_codeset, procmime_get_encoding_str(encoding));

		buf = conv_codeset_strdup(chars, src_codeset, out_codeset);
		if (!buf) {
			AlertValue aval;
			gchar *msg;

			msg = g_strdup_printf(_("Can't convert the character encoding of the message from\n"
						"%s to %s.\n"
						"Send it anyway?"), src_codeset, out_codeset);
			aval = alertpanel
				(_("Error"), msg, _("Yes"), _("+No"), NULL);
			g_free(msg);

			if (aval != G_ALERTDEFAULT) {
				g_free(chars);
				fclose(fp);
				unlink(file);
				return -1;
			} else {
				buf = chars;
				out_codeset = src_codeset;
				chars = NULL;
			}
		}
	}
	g_free(chars);

	canon_buf = canonicalize_str(buf);
	g_free(buf);
	buf = canon_buf;

#if USE_GPGME
	if (!is_draft && compose->use_signing && compose->account->clearsign) {
		if (compose_clearsign_text(compose, &buf) < 0) {
			g_warning("clearsign failed\n");
			fclose(fp);
			unlink(file);
			g_free(buf);
			return -1;
		}
	}
#endif

	/* write headers */
	if (compose_write_headers
		(compose, fp, out_codeset, encoding, is_draft) < 0) {
		g_warning(_("can't write headers\n"));
		fclose(fp);
		unlink(file);
		g_free(buf);
		return -1;
	}

	if (compose->use_attach &&
	    GTK_CLIST(compose->attach_clist)->row_list) {
#if USE_GPGME
            /* This prolog message is ignored by mime software and
             * because it would make our signing/encryption task
             * tougher, we don't emit it in that case */
            if (!compose->use_signing && !compose->use_encryption)
#endif
		fputs("This is a multi-part message in MIME format.\n", fp);

		fprintf(fp, "\n--%s\n", compose->boundary);
		fprintf(fp, "Content-Type: text/plain; charset=%s\n",
			out_codeset);
#if USE_GPGME
		if (compose->use_signing && !compose->account->clearsign)
			fprintf(fp, "Content-Disposition: inline\n");
#endif
		fprintf(fp, "Content-Transfer-Encoding: %s\n",
			procmime_get_encoding_str(encoding));
		fputc('\n', fp);
	}

	/* write body */
	len = strlen(buf);
	if (encoding == ENC_BASE64) {
		gchar outbuf[B64_BUFFSIZE];
		gint i, l;

		for (i = 0; i < len; i += B64_LINE_SIZE) {
			l = MIN(B64_LINE_SIZE, len - i);
			base64_encode(outbuf, buf + i, l);
			fputs(outbuf, fp);
			fputc('\n', fp);
		}
	} else if (encoding == ENC_QUOTED_PRINTABLE) {
		gchar *outbuf;
		size_t outlen;

		outbuf = g_malloc(len * 4);
		qp_encode_line(outbuf, buf);
		outlen = strlen(outbuf);
		if (fwrite(outbuf, sizeof(gchar), outlen, fp) != outlen) {
			FILE_OP_ERROR(file, "fwrite");
			fclose(fp);
			unlink(file);
			g_free(outbuf);
			g_free(buf);
			return -1;
		}
		g_free(outbuf);
	} else if (fwrite(buf, sizeof(gchar), len, fp) != len) {
		FILE_OP_ERROR(file, "fwrite");
		fclose(fp);
		unlink(file);
		g_free(buf);
		return -1;
	}
	g_free(buf);

	if (compose->use_attach &&
	    GTK_CLIST(compose->attach_clist)->row_list)
		compose_write_attach(compose, fp);

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		unlink(file);
		return -1;
	}

#if USE_GPGME
	if (is_draft) {
		uncanonicalize_file_replace(file);
		return 0;
	}

	if ((compose->use_signing && !compose->account->clearsign) ||
	    compose->use_encryption) {
		if (canonicalize_file_replace(file) < 0) {
			unlink(file);
			return -1;
		}
	}

	if (compose->use_signing && !compose->account->clearsign) {
		GSList *key_list;

		if (compose_create_signers_list(compose, &key_list) < 0 ||
		    rfc2015_sign(file, key_list) < 0) {
			unlink(file);
			return -1;
		}
	}
	if (compose->use_encryption) {
		if (rfc2015_encrypt(file, compose->to_list,
				    compose->account->ascii_armored) < 0) {
			unlink(file);
			return -1;
		}
	}
#endif /* USE_GPGME */

	uncanonicalize_file_replace(file);

	return 0;
}

static gint compose_write_body_to_file(Compose *compose, const gchar *file)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	FILE *fp;
	size_t len;
	gchar *chars, *tmp;

	if ((fp = fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, file) < 0) {
		FILE_OP_ERROR(file, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(compose->text));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	tmp = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

	chars = conv_codeset_strdup(tmp,
				    conv_get_internal_charset_str(),
				    conv_get_locale_charset_str());

	g_free(tmp);

	if (!chars) {
		fclose(fp);
		unlink(file);
		return -1;
	}

	/* write body */
	len = strlen(chars);
	if (fwrite(chars, sizeof(gchar), len, fp) != len) {
		FILE_OP_ERROR(file, "fwrite");
		g_free(chars);
		fclose(fp);
		unlink(file);
		return -1;
	}

	g_free(chars);

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		unlink(file);
		return -1;
	}
	return 0;
}

static gint compose_redirect_write_to_file(Compose *compose, const gchar *file)
{
	FILE *fp;
	FILE *fdest;
	size_t len;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(file != NULL, -1);
	g_return_val_if_fail(compose->account != NULL, -1);
	g_return_val_if_fail(compose->account->address != NULL, -1);
	g_return_val_if_fail(compose->mode == COMPOSE_REDIRECT, -1);
	g_return_val_if_fail(compose->targetinfo != NULL, -1);

	if ((fp = procmsg_open_message(compose->targetinfo)) == NULL)
		return -1;

	if ((fdest = fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		fclose(fp);
		return -1;
	}

	if (change_file_mode_rw(fdest, file) < 0) {
		FILE_OP_ERROR(file, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) == 0) {
		if (g_strncasecmp(buf, "Return-Path:",
				   strlen("Return-Path:")) == 0 ||
		    g_strncasecmp(buf, "Delivered-To:",
				  strlen("Delivered-To:")) == 0 ||
		    g_strncasecmp(buf, "Received:",
				  strlen("Received:")) == 0 ||
		    g_strncasecmp(buf, "Subject:",
				  strlen("Subject:")) == 0 ||
		    g_strncasecmp(buf, "X-UIDL:",
				  strlen("X-UIDL:")) == 0)
			continue;

		if (fputs(buf, fdest) == EOF)
			goto error;

#if 0
		if (g_strncasecmp(buf, "From:", strlen("From:")) == 0) {
			fputs("\n (by way of ", fdest);
			if (compose->account->name) {
				compose_convert_header(buf, sizeof(buf),
						       compose->account->name,
						       strlen(" (by way of "),
						       FALSE);
				fprintf(fdest, "%s <%s>", buf,
					compose->account->address);
			} else
				fputs(compose->account->address, fdest);
			fputs(")", fdest);
		}
#endif

		if (fputs("\n", fdest) == EOF)
			goto error;
	}

	compose_redirect_write_headers(compose, fdest);

	while ((len = fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		if (fwrite(buf, sizeof(gchar), len, fdest) != len) {
			FILE_OP_ERROR(file, "fwrite");
			goto error;
		}
	}

	fclose(fp);
	if (fclose(fdest) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		unlink(file);
		return -1;
	}

	return 0;
error:
	fclose(fp);
	fclose(fdest);
	unlink(file);

	return -1;
}

static gint compose_remove_reedit_target(Compose *compose)
{
	FolderItem *item;
	MsgInfo *msginfo = compose->targetinfo;

	g_return_val_if_fail(compose->mode == COMPOSE_REEDIT, -1);
	if (!msginfo) return -1;

	item = msginfo->folder;
	g_return_val_if_fail(item != NULL, -1);

	folder_item_scan(item);
	if (procmsg_msg_exist(msginfo) &&
	    (item->stype == F_DRAFT || item->stype == F_QUEUE)) {
		if (folder_item_remove_msg(item, msginfo) < 0) {
			g_warning(_("can't remove the old message\n"));
			return -1;
		}
	}

	return 0;
}

static gint compose_queue(Compose *compose, const gchar *file)
{
	FolderItem *queue;
	gchar *tmp;
	FILE *fp, *src_fp;
	GSList *cur;
	gchar buf[BUFFSIZE];
	gint num;
	MsgFlags flag = {0, 0};

	debug_print(_("queueing message...\n"));
	g_return_val_if_fail(compose->to_list != NULL ||
			     compose->newsgroup_list != NULL,
			     -1);
	g_return_val_if_fail(compose->account != NULL, -1);

	tmp = g_strdup_printf("%s%cqueue.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);
	if ((fp = fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		g_free(tmp);
		return -1;
	}
	if ((src_fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		fclose(fp);
		unlink(tmp);
		g_free(tmp);
		return -1;
	}
	if (change_file_mode_rw(fp, tmp) < 0) {
		FILE_OP_ERROR(tmp, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	/* queueing variables */
	fprintf(fp, "AF:\n");
	fprintf(fp, "NF:0\n");
	fprintf(fp, "PS:10\n");
	fprintf(fp, "SRH:1\n");
	fprintf(fp, "SFN:\n");
	fprintf(fp, "DSR:\n");
	if (compose->msgid)
		fprintf(fp, "MID:<%s>\n", compose->msgid);
	else
		fprintf(fp, "MID:\n");
	fprintf(fp, "CFG:\n");
	fprintf(fp, "PT:0\n");
	fprintf(fp, "S:%s\n", compose->account->address);
	fprintf(fp, "RQ:\n");
	if (compose->account->smtp_server)
		fprintf(fp, "SSV:%s\n", compose->account->smtp_server);
	else
		fprintf(fp, "SSV:\n");
	if (compose->account->nntp_server)
		fprintf(fp, "NSV:%s\n", compose->account->nntp_server);
	else
		fprintf(fp, "NSV:\n");
	fprintf(fp, "SSH:\n");
	if (compose->to_list) {
		fprintf(fp, "R:<%s>", (gchar *)compose->to_list->data);
		for (cur = compose->to_list->next; cur != NULL;
		     cur = cur->next)
			fprintf(fp, ",<%s>", (gchar *)cur->data);
		fprintf(fp, "\n");
	} else
		fprintf(fp, "R:\n");
	/* Sylpheed account ID */
	fprintf(fp, "AID:%d\n", compose->account->account_id);
	fprintf(fp, "\n");

	while (fgets(buf, sizeof(buf), src_fp) != NULL) {
		if (fputs(buf, fp) == EOF) {
			FILE_OP_ERROR(tmp, "fputs");
			fclose(fp);
			fclose(src_fp);
			unlink(tmp);
			g_free(tmp);
			return -1;
		}
	}

	fclose(src_fp);
	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmp, "fclose");
		unlink(tmp);
		g_free(tmp);
		return -1;
	}

	queue = account_get_special_folder(compose->account, F_QUEUE);
	if (!queue) {
		g_warning(_("can't find queue folder\n"));
		unlink(tmp);
		g_free(tmp);
		return -1;
	}
	folder_item_scan(queue);
	if ((num = folder_item_add_msg(queue, tmp, &flag, TRUE)) < 0) {
		g_warning(_("can't queue the message\n"));
		unlink(tmp);
		g_free(tmp);
		return -1;
	}
	g_free(tmp);

	if (compose->mode == COMPOSE_REEDIT) {
		compose_remove_reedit_target(compose);
		if (compose->targetinfo &&
		    compose->targetinfo->folder != queue)
			folderview_update_item
				(compose->targetinfo->folder, TRUE);
	}

	folder_item_scan(queue);
	folderview_update_item(queue, TRUE);

	return 0;
}

static void compose_write_attach(Compose *compose, FILE *fp)
{
	AttachInfo *ainfo;
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	gint row;
	FILE *attach_fp;
	gchar filename[BUFFSIZE];
	gint len;

	for (row = 0; (ainfo = gtk_clist_get_row_data(clist, row)) != NULL;
	     row++) {
		if ((attach_fp = fopen(ainfo->file, "rb")) == NULL) {
			g_warning("Can't open file %s\n", ainfo->file);
			continue;
		}

		fprintf(fp, "\n--%s\n", compose->boundary);

		if (!g_strcasecmp(ainfo->content_type, "message/rfc822")) {
			fprintf(fp, "Content-Type: %s\n", ainfo->content_type);
			fprintf(fp, "Content-Disposition: inline\n");
		} else {
			compose_convert_header(filename, sizeof(filename),
					       ainfo->name, 12, FALSE);
			fprintf(fp, "Content-Type: %s;\n"
				    " name=\"%s\"\n",
				ainfo->content_type, filename);
			fprintf(fp, "Content-Disposition: attachment;\n"
				    " filename=\"%s\"\n", filename);
		}

		fprintf(fp, "Content-Transfer-Encoding: %s\n\n",
			procmime_get_encoding_str(ainfo->encoding));

		if (ainfo->encoding == ENC_BASE64) {
			gchar inbuf[B64_LINE_SIZE], outbuf[B64_BUFFSIZE];
			FILE *tmp_fp = attach_fp;
			gchar *tmp_file = NULL;
			ContentType content_type;

			content_type =
				procmime_scan_mime_type(ainfo->content_type);
			if (content_type == MIME_TEXT ||
			    content_type == MIME_TEXT_HTML ||
			    content_type == MIME_MESSAGE_RFC822) {
				tmp_file = get_tmp_file();
				if (canonicalize_file(ainfo->file, tmp_file) < 0) {
					g_free(tmp_file);
					fclose(attach_fp);
					continue;
				}
				if ((tmp_fp = fopen(tmp_file, "rb")) == NULL) {
					FILE_OP_ERROR(tmp_file, "fopen");
					unlink(tmp_file);
					g_free(tmp_file);
					fclose(attach_fp);
					continue;
				}
			}

			while ((len = fread(inbuf, sizeof(gchar),
					    B64_LINE_SIZE, tmp_fp))
			       == B64_LINE_SIZE) {
				base64_encode(outbuf, inbuf, B64_LINE_SIZE);
				fputs(outbuf, fp);
				fputc('\n', fp);
			}
			if (len > 0 && feof(tmp_fp)) {
				base64_encode(outbuf, inbuf, len);
				fputs(outbuf, fp);
				fputc('\n', fp);
			}

			if (tmp_file) {
				fclose(tmp_fp);
				unlink(tmp_file);
				g_free(tmp_file);
			}
		} else if (ainfo->encoding == ENC_QUOTED_PRINTABLE) {
			gchar inbuf[BUFFSIZE], outbuf[BUFFSIZE * 4];

			while (fgets(inbuf, sizeof(inbuf), attach_fp) != NULL) {
				qp_encode_line(outbuf, inbuf);
				fputs(outbuf, fp);
			}
		} else {
			gchar buf[BUFFSIZE];

			while (fgets(buf, sizeof(buf), attach_fp) != NULL) {
				strcrchomp(buf);
				fputs(buf, fp);
			}
		}

		fclose(attach_fp);
	}

	fprintf(fp, "\n--%s--\n", compose->boundary);
}

#define QUOTE_IF_REQUIRED(out, str)			\
{							\
	if (*str != '"' && strpbrk(str, ",.[]<>")) {	\
		gchar *__tmp;				\
		gint len;				\
							\
		len = strlen(str) + 3;			\
		Xalloca(__tmp, len, return -1);		\
		g_snprintf(__tmp, len, "\"%s\"", str);	\
		out = __tmp;				\
	} else {					\
		Xstrdup_a(out, str, return -1);		\
	}						\
}

#define PUT_RECIPIENT_HEADER(header, str)				     \
{									     \
	if (*str != '\0') {						     \
		gchar *dest;						     \
									     \
		Xstrdup_a(dest, str, return -1);			     \
		g_strstrip(dest);					     \
		if (*dest != '\0') {					     \
			compose->to_list = address_list_append		     \
				(compose->to_list, dest);		     \
			compose_convert_header				     \
				(buf, sizeof(buf), dest, strlen(header) + 2, \
				 TRUE);					     \
			fprintf(fp, "%s: %s\n", header, buf);		     \
		}							     \
	}								     \
}

#define IS_IN_CUSTOM_HEADER(header) \
	(compose->account->add_customhdr && \
	 custom_header_find(compose->account->customhdr_list, header) != NULL)

static gint compose_write_headers(Compose *compose, FILE *fp,
				  const gchar *charset, EncodingType encoding,
				  gboolean is_draft)
{
	gchar buf[BUFFSIZE];
	const gchar *entry_str;
	gchar *str;
	gchar *name;
	/* struct utsname utsbuf; */

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(charset != NULL, -1);
	g_return_val_if_fail(compose->account != NULL, -1);
	g_return_val_if_fail(compose->account->address != NULL, -1);

	/* Date */
	if (compose->account->add_date) {
		get_rfc822_date(buf, sizeof(buf));
		fprintf(fp, "Date: %s\n", buf);
	}

	/* From */
	if (compose->account->name && *compose->account->name) {
		compose_convert_header
			(buf, sizeof(buf), compose->account->name,
			 strlen("From: "), TRUE);
		QUOTE_IF_REQUIRED(name, buf);
		fprintf(fp, "From: %s <%s>\n",
			name, compose->account->address);
	} else
		fprintf(fp, "From: %s\n", compose->account->address);

	slist_free_strings(compose->to_list);
	g_slist_free(compose->to_list);
	compose->to_list = NULL;

	/* To */
	if (compose->use_to) {
		entry_str = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
		PUT_RECIPIENT_HEADER("To", entry_str);
	}

	slist_free_strings(compose->newsgroup_list);
	g_slist_free(compose->newsgroup_list);
	compose->newsgroup_list = NULL;

	/* Newsgroups */
	if (compose->use_newsgroups) {
		entry_str = gtk_entry_get_text
			(GTK_ENTRY(compose->newsgroups_entry));
		if (*entry_str != '\0') {
			Xstrdup_a(str, entry_str, return -1);
			g_strstrip(str);
			remove_space(str);
			if (*str != '\0') {
				compose->newsgroup_list =
					newsgroup_list_append
						(compose->newsgroup_list, str);
				compose_convert_header(buf, sizeof(buf), str,
						       strlen("Newsgroups: "),
						       TRUE);
				fprintf(fp, "Newsgroups: %s\n", buf);
			}
		}
	}

	/* Cc */
	if (compose->use_cc) {
		entry_str = gtk_entry_get_text(GTK_ENTRY(compose->cc_entry));
		PUT_RECIPIENT_HEADER("Cc", entry_str);
	}

	/* Bcc */
	if (compose->use_bcc) {
		entry_str = gtk_entry_get_text(GTK_ENTRY(compose->bcc_entry));
		PUT_RECIPIENT_HEADER("Bcc", entry_str);
	}

	if (!is_draft && !compose->to_list && !compose->newsgroup_list)
		return -1;

	/* Subject */
	entry_str = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	if (*entry_str != '\0' && !IS_IN_CUSTOM_HEADER("Subject")) {
		Xstrdup_a(str, entry_str, return -1);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(buf, sizeof(buf), str,
					       strlen("Subject: "), FALSE);
			fprintf(fp, "Subject: %s\n", buf);
		}
	}

	/* Message-ID */
	if (compose->account->gen_msgid) {
		compose_generate_msgid(compose, buf, sizeof(buf));
		fprintf(fp, "Message-Id: <%s>\n", buf);
		compose->msgid = g_strdup(buf);
	}

	/* In-Reply-To */
	if (compose->inreplyto && compose->to_list)
		fprintf(fp, "In-Reply-To: <%s>\n", compose->inreplyto);

	/* References */
	if (compose->references)
		fprintf(fp, "References: %s\n", compose->references);

	/* Followup-To */
	if (compose->use_followupto && !IS_IN_CUSTOM_HEADER("Followup-To")) {
		entry_str = gtk_entry_get_text
			(GTK_ENTRY(compose->followup_entry));
		if (*entry_str != '\0') {
			Xstrdup_a(str, entry_str, return -1);
			g_strstrip(str);
			remove_space(str);
			if (*str != '\0') {
				compose_convert_header(buf, sizeof(buf), str,
						       strlen("Followup-To: "),
						       TRUE);
				fprintf(fp, "Followup-To: %s\n", buf);
			}
		}
	}

	/* Reply-To */
	if (compose->use_replyto && !IS_IN_CUSTOM_HEADER("Reply-To")) {
		entry_str = gtk_entry_get_text(GTK_ENTRY(compose->reply_entry));
		if (*entry_str != '\0') {
			Xstrdup_a(str, entry_str, return -1);
			g_strstrip(str);
			if (*str != '\0') {
				compose_convert_header(buf, sizeof(buf), str,
						       strlen("Reply-To: "),
						       TRUE);
				fprintf(fp, "Reply-To: %s\n", buf);
			}
		}
	}

	/* Organization */
	if (compose->account->organization &&
	    !IS_IN_CUSTOM_HEADER("Organization")) {
		compose_convert_header(buf, sizeof(buf),
				       compose->account->organization,
				       strlen("Organization: "), FALSE);
		fprintf(fp, "Organization: %s\n", buf);
	}

	/* Program version and system info */
	/* uname(&utsbuf); */
	if (compose->to_list && !IS_IN_CUSTOM_HEADER("X-Mailer")) {
		fprintf(fp, "X-Mailer: %s (GTK+ %d.%d.%d; %s)\n",
			prog_version,
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			TARGET_ALIAS);
			/* utsbuf.sysname, utsbuf.release, utsbuf.machine); */
	}
	if (compose->newsgroup_list && !IS_IN_CUSTOM_HEADER("X-Newsreader")) {
		fprintf(fp, "X-Newsreader: %s (GTK+ %d.%d.%d; %s)\n",
			prog_version,
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			TARGET_ALIAS);
			/* utsbuf.sysname, utsbuf.release, utsbuf.machine); */
	}

	/* custom headers */
	if (compose->account->add_customhdr) {
		GSList *cur;

		for (cur = compose->account->customhdr_list; cur != NULL;
		     cur = cur->next) {
			CustomHeader *chdr = (CustomHeader *)cur->data;

			if (strcasecmp(chdr->name, "Date")         != 0 &&
			    strcasecmp(chdr->name, "From")         != 0 &&
			    strcasecmp(chdr->name, "To")           != 0 &&
			 /* strcasecmp(chdr->name, "Sender")       != 0 && */
			    strcasecmp(chdr->name, "Message-Id")   != 0 &&
			    strcasecmp(chdr->name, "In-Reply-To")  != 0 &&
			    strcasecmp(chdr->name, "References")   != 0 &&
			    strcasecmp(chdr->name, "Mime-Version") != 0 &&
			    strcasecmp(chdr->name, "Content-Type") != 0 &&
			    strcasecmp(chdr->name, "Content-Transfer-Encoding")
			    != 0) {
				compose_convert_header
					(buf, sizeof(buf),
					 chdr->value ? chdr->value : "",
					 strlen(chdr->name) + 2, FALSE);
				fprintf(fp, "%s: %s\n", chdr->name, buf);
			}
		}
	}

	/* MIME */
	fprintf(fp, "Mime-Version: 1.0\n");
	if (compose->use_attach &&
	    GTK_CLIST(compose->attach_clist)->row_list) {
		compose->boundary = generate_mime_boundary(NULL);
		fprintf(fp,
			"Content-Type: multipart/mixed;\n"
			" boundary=\"%s\"\n", compose->boundary);
	} else {
		fprintf(fp, "Content-Type: text/plain; charset=%s\n", charset);
#if USE_GPGME
		if (compose->use_signing && !compose->account->clearsign)
			fprintf(fp, "Content-Disposition: inline\n");
#endif
		fprintf(fp, "Content-Transfer-Encoding: %s\n",
			procmime_get_encoding_str(encoding));
	}

	/* X-Sylpheed header */
	if (is_draft)
		fprintf(fp, "X-Sylpheed-Account-Id: %d\n",
			compose->account->account_id);

	/* separator between header and body */
	fputs("\n", fp);

	return 0;
}

static gint compose_redirect_write_headers(Compose *compose, FILE *fp)
{
	gchar buf[BUFFSIZE];
	const gchar *entry_str;
	gchar *str;

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(compose->account != NULL, -1);
	g_return_val_if_fail(compose->account->address != NULL, -1);

	/* Resent-Date */
	get_rfc822_date(buf, sizeof(buf));
	fprintf(fp, "Resent-Date: %s\n", buf);

	/* Resent-From */
	if (compose->account->name) {
		compose_convert_header
			(buf, sizeof(buf), compose->account->name,
			 strlen("Resent-From: "), TRUE);
		fprintf(fp, "Resent-From: %s <%s>\n",
			buf, compose->account->address);
	} else
		fprintf(fp, "Resent-From: %s\n", compose->account->address);

	slist_free_strings(compose->to_list);
	g_slist_free(compose->to_list);
	compose->to_list = NULL;

	/* Resent-To */
	if (compose->use_to) {
		entry_str = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
		PUT_RECIPIENT_HEADER("Resent-To", entry_str);
	}
	if (compose->use_cc) {
		entry_str = gtk_entry_get_text(GTK_ENTRY(compose->cc_entry));
		PUT_RECIPIENT_HEADER("Resent-Cc", entry_str);
	}
	if (compose->use_bcc) {
		entry_str = gtk_entry_get_text(GTK_ENTRY(compose->bcc_entry));
		PUT_RECIPIENT_HEADER("Bcc", entry_str);
	}

	slist_free_strings(compose->newsgroup_list);
	g_slist_free(compose->newsgroup_list);
	compose->newsgroup_list = NULL;

	/* Newsgroups */
	if (compose->use_newsgroups) {
		entry_str = gtk_entry_get_text
			(GTK_ENTRY(compose->newsgroups_entry));
		if (*entry_str != '\0') {
			Xstrdup_a(str, entry_str, return -1);
			g_strstrip(str);
			remove_space(str);
			if (*str != '\0') {
				compose->newsgroup_list =
					newsgroup_list_append
						(compose->newsgroup_list, str);
				compose_convert_header(buf, sizeof(buf), str,
						       strlen("Newsgroups: "),
						       TRUE);
				fprintf(fp, "Newsgroups: %s\n", buf);
			}
		}
	}

	if (!compose->to_list && !compose->newsgroup_list)
		return -1;

	/* Subject */
	entry_str = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	if (*entry_str != '\0') {
		Xstrdup_a(str, entry_str, return -1);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(buf, sizeof(buf), str,
					       strlen("Subject: "), FALSE);
			fprintf(fp, "Subject: %s\n", buf);
		}
	}

	/* Resent-Message-Id */
	if (compose->account->gen_msgid) {
		compose_generate_msgid(compose, buf, sizeof(buf));
		fprintf(fp, "Resent-Message-Id: <%s>\n", buf);
		compose->msgid = g_strdup(buf);
	}

	/* Followup-To */
	if (compose->use_followupto) {
		entry_str = gtk_entry_get_text
			(GTK_ENTRY(compose->followup_entry));
		if (*entry_str != '\0') {
			Xstrdup_a(str, entry_str, return -1);
			g_strstrip(str);
			remove_space(str);
			if (*str != '\0') {
				compose_convert_header(buf, sizeof(buf), str,
						       strlen("Followup-To: "),
						       TRUE);
				fprintf(fp, "Followup-To: %s\n", buf);
			}
		}
	}

	/* Resent-Reply-To */
	if (compose->use_replyto) {
		entry_str = gtk_entry_get_text(GTK_ENTRY(compose->reply_entry));
		if (*entry_str != '\0') {
			Xstrdup_a(str, entry_str, return -1);
			g_strstrip(str);
			if (*str != '\0') {
				compose_convert_header
					(buf, sizeof(buf), str,
					 strlen("Resent-Reply-To: "), TRUE);
				fprintf(fp, "Resent-Reply-To: %s\n", buf);
			}
		}
	}

	fputs("\n", fp);

	return 0;
}

#undef IS_IN_CUSTOM_HEADER

static void compose_convert_header(gchar *dest, gint len, gchar *src,
				   gint header_len, gboolean addr_field)
{
	g_return_if_fail(src != NULL);
	g_return_if_fail(dest != NULL);

	if (len < 1) return;

	g_strchomp(src);
	conv_encode_header(dest, len, src, header_len, addr_field);
}

static void compose_generate_msgid(Compose *compose, gchar *buf, gint len)
{
	struct tm *lt;
	time_t t;
	gchar *addr;

	t = time(NULL);
	lt = localtime(&t);

	if (compose->account && compose->account->address &&
	    *compose->account->address) {
		if (strchr(compose->account->address, '@'))
			addr = g_strdup(compose->account->address);
		else
			addr = g_strconcat(compose->account->address, "@",
					   get_domain_name(), NULL);
	} else
		addr = g_strconcat(g_get_user_name(), "@", get_domain_name(),
				   NULL);

	g_snprintf(buf, len, "%04d%02d%02d%02d%02d%02d.%08x.%s",
		   lt->tm_year + 1900, lt->tm_mon + 1,
		   lt->tm_mday, lt->tm_hour,
		   lt->tm_min, lt->tm_sec,
		   (guint)random(), addr);

	debug_print(_("generated Message-ID: %s\n"), buf);

	g_free(addr);
}

static void compose_add_entry_field(GtkWidget *table, GtkWidget **hbox,
				    GtkWidget **entry, gint *count,
				    const gchar *label_str,
				    gboolean is_addr_entry)
{
	GtkWidget *label;

	if (GTK_TABLE(table)->nrows < (*count) + 1)
		gtk_table_resize(GTK_TABLE(table), (*count) + 1, 2);

	*hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new
		(prefs_common.trans_hdr ? gettext(label_str) : label_str);
	gtk_box_pack_end(GTK_BOX(*hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), *hbox, 0, 1, *count, (*count) + 1,
			 GTK_FILL, 0, 2, 0);
	*entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(*entry), MAX_ENTRY_LENGTH);
	gtk_table_attach_defaults
		(GTK_TABLE(table), *entry, 1, 2, *count, (*count) + 1);
	if (GTK_TABLE(table)->nrows > (*count) + 1)
		gtk_table_set_row_spacing(GTK_TABLE(table), *count, 4);

	if (is_addr_entry)
		address_completion_register_entry(GTK_ENTRY(*entry));

	(*count)++;
}

static Compose *compose_create(PrefsAccount *account, ComposeMode mode)
{
	Compose   *compose;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *handlebox;

	GtkWidget *vbox2;

	GtkWidget *table_vbox;
	GtkWidget *label;
	GtkWidget *from_optmenu_hbox;
	GtkWidget *to_entry;
	GtkWidget *to_hbox;
	GtkWidget *newsgroups_entry;
	GtkWidget *newsgroups_hbox;
	GtkWidget *subject_entry;
	GtkWidget *cc_entry;
	GtkWidget *cc_hbox;
	GtkWidget *bcc_entry;
	GtkWidget *bcc_hbox;
	GtkWidget *reply_entry;
	GtkWidget *reply_hbox;
	GtkWidget *followup_entry;
	GtkWidget *followup_hbox;

	GtkWidget *paned;

	GtkWidget *attach_scrwin;
	GtkWidget *attach_clist;

	GtkWidget *edit_vbox;
	GtkWidget *ruler_hbox;
	GtkWidget *ruler;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;

	GtkWidget *table;
	GtkWidget *hbox;

	UndoMain *undostruct;

	gchar *titles[N_ATTACH_COLS];
	guint n_menu_entries;
	GtkStyle  *style, *new_style;
	GdkColormap *cmap;
	GdkColor color[1];
	gboolean success[1];
	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	GtkItemFactory *ifactory;
	GtkWidget *tmpl_menu;
	gint n_entries;
	gint count = 0;
	gint i;

	static GdkGeometry geometry;

	g_return_val_if_fail(account != NULL, NULL);

	debug_print(_("Creating compose window...\n"));
	compose = g_new0(Compose, 1);

	titles[COL_MIMETYPE] = _("MIME type");
	titles[COL_SIZE]     = _("Size");
	titles[COL_NAME]     = _("Name");

	compose->account = account;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_size_request(window, -1, prefs_common.compose_height);
	gtk_window_set_wmclass(GTK_WINDOW(window), "compose", "Sylpheed");

	if (!geometry.max_width) {
		geometry.max_width = gdk_screen_width();
		geometry.max_height = gdk_screen_height();
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL,
				      &geometry, GDK_HINT_MAX_SIZE);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(compose_delete_cb), compose);
	g_signal_connect(G_OBJECT(window), "destroy",
			 G_CALLBACK(compose_destroy_cb), compose);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	n_menu_entries = sizeof(compose_entries) / sizeof(compose_entries[0]);
	menubar = menubar_create(window, compose_entries,
				 n_menu_entries, "<Compose>", compose);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	handlebox = gtk_handle_box_new();
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);

	compose_toolbar_create(compose, handlebox);

	vbox2 = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), BORDER_WIDTH);

	table_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), table_vbox, FALSE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(table_vbox),
				       BORDER_WIDTH * 2);

	table = gtk_table_new(8, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(table_vbox), table, FALSE, TRUE, 0);

	/* option menu for selecting accounts */
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(prefs_common.trans_hdr ? _("From:") : "From:");
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, count, count + 1,
			 GTK_FILL, 0, 2, 0);
	from_optmenu_hbox = compose_account_option_menu_create(compose);
	gtk_table_attach_defaults(GTK_TABLE(table), from_optmenu_hbox,
				  1, 2, count, count + 1);
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 4);
	count++;

	/* header labels and entries */
	compose_add_entry_field(table, &to_hbox, &to_entry, &count,
				"To:", TRUE); 
	compose_add_entry_field(table, &newsgroups_hbox, &newsgroups_entry,
				&count, "Newsgroups:", FALSE);
	compose_add_entry_field(table, &cc_hbox, &cc_entry, &count,
				"Cc:", TRUE);
	compose_add_entry_field(table, &bcc_hbox, &bcc_entry, &count,
				"Bcc:", TRUE);
	compose_add_entry_field(table, &reply_hbox, &reply_entry, &count,
				"Reply-To:", TRUE);
	compose_add_entry_field(table, &followup_hbox, &followup_entry, &count,
				"Followup-To:", FALSE);
	compose_add_entry_field(table, &hbox, &subject_entry, &count,
				"Subject:", FALSE);

	gtk_table_set_col_spacings(GTK_TABLE(table), 4);

	g_signal_connect(G_OBJECT(to_entry), "activate",
			 G_CALLBACK(to_activated), compose);
	g_signal_connect(G_OBJECT(newsgroups_entry), "activate",
			 G_CALLBACK(newsgroups_activated), compose);
	g_signal_connect(G_OBJECT(cc_entry), "activate",
			 G_CALLBACK(cc_activated), compose);
	g_signal_connect(G_OBJECT(bcc_entry), "activate",
			 G_CALLBACK(bcc_activated), compose);
	g_signal_connect(G_OBJECT(reply_entry), "activate",
			 G_CALLBACK(replyto_activated), compose);
	g_signal_connect(G_OBJECT(followup_entry), "activate",
			 G_CALLBACK(followupto_activated), compose);
	g_signal_connect(G_OBJECT(subject_entry), "activate",
			 G_CALLBACK(subject_activated), compose);

	g_signal_connect(G_OBJECT(to_entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
	g_signal_connect(G_OBJECT(newsgroups_entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
	g_signal_connect(G_OBJECT(cc_entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
	g_signal_connect(G_OBJECT(bcc_entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
	g_signal_connect(G_OBJECT(reply_entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
	g_signal_connect(G_OBJECT(followup_entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
	g_signal_connect(G_OBJECT(subject_entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);

	/* attachment list */
	attach_scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(attach_scrwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_widget_set_size_request(attach_scrwin, -1, 80);

	attach_clist = gtk_clist_new_with_titles(N_ATTACH_COLS, titles);
	gtk_clist_set_column_justification(GTK_CLIST(attach_clist), COL_SIZE,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_width(GTK_CLIST(attach_clist), COL_MIMETYPE, 240);
	gtk_clist_set_column_width(GTK_CLIST(attach_clist), COL_SIZE, 64);
	gtk_clist_set_selection_mode(GTK_CLIST(attach_clist),
				     GTK_SELECTION_EXTENDED);
	for (i = 0; i < N_ATTACH_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS
			(GTK_CLIST(attach_clist)->column[i].button,
			 GTK_CAN_FOCUS);
	gtk_container_add(GTK_CONTAINER(attach_scrwin), attach_clist);

	g_signal_connect(G_OBJECT(attach_clist), "select_row",
			 G_CALLBACK(attach_selected), compose);
	g_signal_connect(G_OBJECT(attach_clist), "button_press_event",
			 G_CALLBACK(attach_button_pressed), compose);
	g_signal_connect(G_OBJECT(attach_clist), "key_press_event",
			 G_CALLBACK(attach_key_pressed), compose);

	/* drag and drop */
	gtk_drag_dest_set(attach_clist,
			  GTK_DEST_DEFAULT_ALL, compose_mime_types, 1,
			  GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(attach_clist), "drag_data_received",
			 G_CALLBACK(compose_attach_drag_received_cb),
			 compose);

	/* pane between attach clist and text */
	paned = gtk_vpaned_new();
	gtk_paned_add1(GTK_PANED(paned), attach_scrwin);
	gtk_widget_ref(paned);
	gtk_widget_show_all(paned);

	edit_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), edit_vbox, TRUE, TRUE, 0);

	/* ruler */
	ruler_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(edit_vbox), ruler_hbox, FALSE, FALSE, 0);

	ruler = gtk_shruler_new();
	gtk_ruler_set_range(GTK_RULER(ruler), 0.0, 100.0, 1.0, 100.0);
	gtk_box_pack_start(GTK_BOX(ruler_hbox), ruler, TRUE, TRUE,
			   BORDER_WIDTH);

	/* text widget */
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(edit_vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_size_request(scrolledwin, prefs_common.compose_width,
				    -1);

	text = gtk_text_view_new();
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_text_buffer_add_selection_clipboard(buffer, clipboard);
	/* GTK_STEXT(text)->default_tab_width = 8; */
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	g_signal_connect(G_OBJECT(text), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
#warning FIXME_GTK2
#if 0
	g_signal_connect(G_OBJECT(text), "activate",
			 G_CALLBACK(text_activated), compose);
#endif
	g_signal_connect(G_OBJECT(buffer), "insert_text",
			 G_CALLBACK(text_inserted), compose);
	g_signal_connect_after(G_OBJECT(text), "size_allocate",
			       G_CALLBACK(compose_edit_size_alloc),
			       ruler);

	/* drag and drop */
	gtk_drag_dest_set(text, GTK_DEST_DEFAULT_ALL, compose_mime_types, 1,
			  GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(text), "drag_data_received",
			 G_CALLBACK(compose_insert_drag_received_cb),
			 compose);

	gtk_widget_show_all(vbox);

	style = gtk_widget_get_style(text);
	new_style = gtk_style_copy(style);

#warning FIXME_GTK2 use normal API for setting font
	if (prefs_common.textfont) {
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_from_string
			(prefs_common.textfont);
		if (font_desc) {
			if (new_style->font_desc)
				pango_font_description_free
					(new_style->font_desc);
			new_style->font_desc = font_desc;
		}
	}

	gtk_widget_set_style(text, new_style);

	color[0] = quote_color;
	cmap = gdk_window_get_colormap(window->window);
	gdk_colormap_alloc_colors(cmap, color, 1, FALSE, TRUE, success);
	if (success[0] == FALSE) {
		g_warning("Compose: color allocation failed.\n");
		style = gtk_widget_get_style(text);
		quote_color = style->black;
	}

	n_entries = sizeof(compose_popup_entries) /
		sizeof(compose_popup_entries[0]);
	popupmenu = menu_create_items(compose_popup_entries, n_entries,
				      "<Compose>", &popupfactory,
				      compose);

	ifactory = gtk_item_factory_from_widget(menubar);
	menu_set_sensitive(ifactory, "/Edit/Undo", FALSE);
	menu_set_sensitive(ifactory, "/Edit/Redo", FALSE);

	tmpl_menu = gtk_item_factory_get_item(ifactory, "/Tools/Template");

	gtk_widget_hide(bcc_hbox);
	gtk_widget_hide(bcc_entry);
	gtk_widget_hide(reply_hbox);
	gtk_widget_hide(reply_entry);
	gtk_widget_hide(followup_hbox);
	gtk_widget_hide(followup_entry);
	gtk_widget_hide(ruler_hbox);
	gtk_table_set_row_spacing(GTK_TABLE(table), 4, 0);
	gtk_table_set_row_spacing(GTK_TABLE(table), 5, 0);
	gtk_table_set_row_spacing(GTK_TABLE(table), 6, 0);

	if (account->protocol == A_NNTP) {
		gtk_widget_hide(to_hbox);
		gtk_widget_hide(to_entry);
		gtk_widget_hide(cc_hbox);
		gtk_widget_hide(cc_entry);
		gtk_table_set_row_spacing(GTK_TABLE(table), 1, 0);
		gtk_table_set_row_spacing(GTK_TABLE(table), 3, 0);
	} else {
		gtk_widget_hide(newsgroups_hbox);
		gtk_widget_hide(newsgroups_entry);
		gtk_table_set_row_spacing(GTK_TABLE(table), 2, 0);
	}

	switch (prefs_common.toolbar_style) {
	case TOOLBAR_NONE:
		gtk_widget_hide(handlebox);
		break;
	case TOOLBAR_ICON:
		gtk_toolbar_set_style(GTK_TOOLBAR(compose->toolbar),
				      GTK_TOOLBAR_ICONS);
		break;
	case TOOLBAR_TEXT:
		gtk_toolbar_set_style(GTK_TOOLBAR(compose->toolbar),
				      GTK_TOOLBAR_TEXT);
		break;
	case TOOLBAR_BOTH:
		gtk_toolbar_set_style(GTK_TOOLBAR(compose->toolbar),
				      GTK_TOOLBAR_BOTH);
		break;
	}

	undostruct = undo_init(text);
	undo_set_change_state_func(undostruct, &compose_undo_state_changed,
				   menubar);

	address_completion_start(window);

	compose->window        = window;
	compose->vbox	       = vbox;
	compose->menubar       = menubar;
	compose->handlebox     = handlebox;

	compose->vbox2	       = vbox2;

	compose->table_vbox       = table_vbox;
	compose->table	          = table;
	compose->to_hbox          = to_hbox;
	compose->to_entry         = to_entry;
	compose->newsgroups_hbox  = newsgroups_hbox;
	compose->newsgroups_entry = newsgroups_entry;
	compose->subject_entry    = subject_entry;
	compose->cc_hbox          = cc_hbox;
	compose->cc_entry         = cc_entry;
	compose->bcc_hbox         = bcc_hbox;
	compose->bcc_entry        = bcc_entry;
	compose->reply_hbox       = reply_hbox;
	compose->reply_entry      = reply_entry;
	compose->followup_hbox    = followup_hbox;
	compose->followup_entry   = followup_entry;

	compose->paned = paned;

	compose->attach_scrwin = attach_scrwin;
	compose->attach_clist  = attach_clist;

	compose->edit_vbox     = edit_vbox;
	compose->ruler_hbox    = ruler_hbox;
	compose->ruler         = ruler;
	compose->scrolledwin   = scrolledwin;
	compose->text	       = text;

	compose->focused_editable = NULL;

	compose->popupmenu    = popupmenu;
	compose->popupfactory = popupfactory;

	compose->tmpl_menu = tmpl_menu;

	compose->mode = mode;

	compose->targetinfo = NULL;
	compose->replyinfo  = NULL;

	compose->replyto     = NULL;
	compose->cc	     = NULL;
	compose->bcc	     = NULL;
	compose->followup_to = NULL;

	compose->ml_post     = NULL;

	compose->inreplyto   = NULL;
	compose->references  = NULL;
	compose->msgid       = NULL;
	compose->boundary    = NULL;

	compose->autowrap       = prefs_common.autowrap;

	compose->use_to         = FALSE;
	compose->use_cc         = FALSE;
	compose->use_bcc        = FALSE;
	compose->use_replyto    = FALSE;
	compose->use_newsgroups = FALSE;
	compose->use_followupto = FALSE;
	compose->use_attach     = FALSE;

#if USE_GPGME
	compose->use_signing    = FALSE;
	compose->use_encryption = FALSE;
#endif /* USE_GPGME */

	compose->modified = FALSE;

	compose->paste_as_quotation = FALSE;

	compose->to_list        = NULL;
	compose->newsgroup_list = NULL;

	compose->undostruct = undostruct;

	compose->sig_str = NULL;

	compose->exteditor_file    = NULL;
	compose->exteditor_pid     = -1;
	compose->exteditor_readdes = -1;
	compose->exteditor_tag     = -1;

	compose_select_account(compose, account, TRUE);

	menu_set_active(ifactory, "/Edit/Auto wrapping", prefs_common.autowrap);
	menu_set_active(ifactory, "/View/Ruler", prefs_common.show_ruler);

	if (mode == COMPOSE_REDIRECT) {
		menu_set_sensitive(ifactory, "/File/Save to draft folder", FALSE);
		menu_set_sensitive(ifactory, "/File/Save and keep editing", FALSE);
		menu_set_sensitive(ifactory, "/File/Attach file", FALSE);
		menu_set_sensitive(ifactory, "/File/Insert file", FALSE);
		menu_set_sensitive(ifactory, "/File/Insert signature", FALSE);
		menu_set_sensitive(ifactory, "/Edit/Cut", FALSE);
		menu_set_sensitive(ifactory, "/Edit/Paste", FALSE);
		menu_set_sensitive(ifactory, "/Edit/Wrap current paragraph", FALSE);
		menu_set_sensitive(ifactory, "/Edit/Wrap all long lines", FALSE);
		menu_set_sensitive(ifactory, "/Edit/Auto wrapping", FALSE);
		menu_set_sensitive(ifactory, "/Edit/Advanced", FALSE);
		menu_set_sensitive(ifactory, "/View/Attachment", FALSE);
		menu_set_sensitive(ifactory, "/Tools/Template", FALSE);
		menu_set_sensitive(ifactory, "/Tools/Actions", FALSE);
		menu_set_sensitive(ifactory, "/Tools/Edit with external editor", FALSE);
#if USE_GPGME
		menu_set_sensitive(ifactory, "/Tools/PGP Sign", FALSE);
		menu_set_sensitive(ifactory, "/Tools/PGP Encrypt", FALSE);
#endif /* USE_GPGME */

		gtk_widget_set_sensitive(compose->insert_btn, FALSE);
		gtk_widget_set_sensitive(compose->attach_btn, FALSE);
		gtk_widget_set_sensitive(compose->sig_btn, FALSE);
		gtk_widget_set_sensitive(compose->exteditor_btn, FALSE);
		gtk_widget_set_sensitive(compose->linewrap_btn, FALSE);

		menu_set_sensitive_all(GTK_MENU_SHELL(compose->popupmenu),
				       FALSE);
	}

	addressbook_set_target_compose(compose);
	action_update_compose_menu(ifactory, compose);
	compose_set_template_menu(compose);

	compose_list = g_list_append(compose_list, compose);

	gtk_widget_show(window);

	return compose;
}

static void compose_connect_changed_callbacks(Compose *compose)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer(text);

	g_signal_connect(G_OBJECT(buffer), "changed",
			 G_CALLBACK(compose_changed_cb), compose);
	g_signal_connect(G_OBJECT(compose->to_entry), "changed",
			 G_CALLBACK(compose_changed_cb), compose);
	g_signal_connect(G_OBJECT(compose->newsgroups_entry), "changed",
			 G_CALLBACK(compose_changed_cb), compose);
	g_signal_connect(G_OBJECT(compose->cc_entry), "changed",
			 G_CALLBACK(compose_changed_cb), compose);
	g_signal_connect(G_OBJECT(compose->bcc_entry), "changed",
			 G_CALLBACK(compose_changed_cb), compose);
	g_signal_connect(G_OBJECT(compose->reply_entry), "changed",
			 G_CALLBACK(compose_changed_cb), compose);
	g_signal_connect(G_OBJECT(compose->followup_entry), "changed",
			 G_CALLBACK(compose_changed_cb), compose);
	g_signal_connect(G_OBJECT(compose->subject_entry), "changed",
			 G_CALLBACK(compose_changed_cb), compose);
}

static void compose_toolbar_create(Compose *compose, GtkWidget *container)
{
	GtkWidget *toolbar;
	GtkWidget *icon_wid;
	GtkWidget *send_btn;
	GtkWidget *sendl_btn;
	GtkWidget *draft_btn;
	GtkWidget *insert_btn;
	GtkWidget *attach_btn;
	GtkWidget *sig_btn;
	GtkWidget *exteditor_btn;
	GtkWidget *linewrap_btn;
	GtkWidget *addrbook_btn;

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
				    GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	gtk_container_add(GTK_CONTAINER(container), toolbar);
	gtk_widget_set_size_request(toolbar, 1, -1);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_SEND);
	send_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Send"),
					   _("Send message"),
					   "Send",
					   icon_wid,
					   G_CALLBACK(toolbar_send_cb),
					   compose);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_SEND_QUEUE);
	sendl_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Send later"),
					   _("Put into queue folder and send later"),
					   "Send later",
					   icon_wid,
					   G_CALLBACK(toolbar_send_later_cb),
					   compose);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL);
	draft_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					    _("Draft"),
					    _("Save to draft folder"),
					    "Draft",
					    icon_wid,
					    G_CALLBACK(toolbar_draft_cb),
					    compose);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_PASTE);
	insert_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					     _("Insert"),
					     _("Insert file"),
					     "Insert",
					     icon_wid,
					     G_CALLBACK(toolbar_insert_cb),
					     compose);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_ATTACH);
	attach_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					     _("Attach"),
					     _("Attach file"),
					     "Attach",
					     icon_wid,
					     G_CALLBACK(toolbar_attach_cb),
					     compose);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL);
	sig_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Signature"),
					  _("Insert signature"),
					  "Signature",
					  icon_wid,
					  G_CALLBACK(toolbar_sig_cb), compose);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_COMPOSE);
	exteditor_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
						_("Editor"),
						_("Edit with external editor"),
						"Editor",
						icon_wid,
						G_CALLBACK(toolbar_ext_editor_cb),
						compose);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_LINEWRAP);
	linewrap_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					       _("Linewrap"),
					       _("Wrap all long lines"),
					       "Linewrap",
					       icon_wid,
					       G_CALLBACK(toolbar_linewrap_cb),
					       compose);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_ADDRESS_BOOK);
	addrbook_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					       _("Address"),
					       _("Address book"),
					       "Address",
					       icon_wid,
					       G_CALLBACK(toolbar_address_cb),
					       compose);

	compose->toolbar       = toolbar;
	compose->send_btn      = send_btn;
	compose->sendl_btn     = sendl_btn;
	compose->draft_btn     = draft_btn;
	compose->insert_btn    = insert_btn;
	compose->attach_btn    = attach_btn;
	compose->sig_btn       = sig_btn;
	compose->exteditor_btn = exteditor_btn;
	compose->linewrap_btn  = linewrap_btn;
	compose->addrbook_btn  = addrbook_btn;

	gtk_widget_show_all(toolbar);
}

static GtkWidget *compose_account_option_menu_create(Compose *compose)
{
	GList *accounts;
	GtkWidget *hbox;
	GtkWidget *optmenu;
	GtkWidget *menu;
	gint num = 0, def_menu = 0;

	accounts = account_get_list();
	g_return_val_if_fail(accounts != NULL, NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, FALSE, FALSE, 0);
	menu = gtk_menu_new();

	for (; accounts != NULL; accounts = accounts->next, num++) {
		PrefsAccount *ac = (PrefsAccount *)accounts->data;
		GtkWidget *menuitem;
		gchar *name;

		if (ac == compose->account) def_menu = num;

		if (ac->name)
			name = g_strdup_printf("%s: %s <%s>",
					       ac->account_name,
					       ac->name, ac->address);
		else
			name = g_strdup_printf("%s: %s",
					       ac->account_name, ac->address);
		MENUITEM_ADD(menu, menuitem, name, ac);
		g_free(name);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_activated),
				 compose);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), def_menu);

	return hbox;
}

static void compose_set_template_menu(Compose *compose)
{
	GSList *tmpl_list, *cur;
	GtkWidget *menu;
	GtkWidget *item;

	tmpl_list = template_get_config();

	menu = gtk_menu_new();

	for (cur = tmpl_list; cur != NULL; cur = cur->next) {
		Template *tmpl = (Template *)cur->data;

		item = gtk_menu_item_new_with_label(tmpl->name);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(compose_template_activate_cb),
				 compose);
		g_object_set_data(G_OBJECT(item), "template", tmpl);
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(compose->tmpl_menu), menu);
}

void compose_reflect_prefs_all(void)
{
	GList *cur;
	Compose *compose;

	for (cur = compose_list; cur != NULL; cur = cur->next) {
		compose = (Compose *)cur->data;
		compose_set_template_menu(compose);
	}
}

static void compose_template_apply(Compose *compose, Template *tmpl,
				   gboolean replace)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	gchar *qmark;
	gchar *parsed_str;

	if (!tmpl || !tmpl->value) return;

	buffer = gtk_text_view_get_buffer(text);

	if (tmpl->to && *tmpl->to != '\0')
		compose_entry_set(compose, tmpl->to, COMPOSE_ENTRY_TO);
	if (tmpl->cc && *tmpl->cc != '\0')
		compose_entry_set(compose, tmpl->cc, COMPOSE_ENTRY_CC);
	if (tmpl->subject && *tmpl->subject != '\0')
		compose_entry_set(compose, tmpl->subject, COMPOSE_ENTRY_SUBJECT);

	if (replace)
		gtk_text_buffer_set_text(buffer, "", 0);

	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	if (compose->replyinfo == NULL) {
		parsed_str = compose_quote_fmt(compose, NULL, tmpl->value,
					       NULL, NULL);
	} else {
		if (prefs_common.quotemark && *prefs_common.quotemark)
			qmark = prefs_common.quotemark;
		else
			qmark = "> ";

		parsed_str = compose_quote_fmt(compose, compose->replyinfo,
					       tmpl->value, qmark, NULL);
	}

	if (replace && parsed_str && prefs_common.auto_sig)
		compose_insert_sig(compose, FALSE);

	if (replace && parsed_str) {
		gtk_text_buffer_get_start_iter(buffer, &iter);
		gtk_text_buffer_place_cursor(buffer, &iter);
	}

	if (parsed_str)
		compose_changed_cb(NULL, compose);
}

static void compose_destroy(Compose *compose)
{
	gint row;
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	AttachInfo *ainfo;

	/* NOTE: address_completion_end() does nothing with the window
	 * however this may change. */
	address_completion_end(compose->window);

	slist_free_strings(compose->to_list);
	g_slist_free(compose->to_list);
	slist_free_strings(compose->newsgroup_list);
	g_slist_free(compose->newsgroup_list);

	procmsg_msginfo_free(compose->targetinfo);
	procmsg_msginfo_free(compose->replyinfo);

	g_free(compose->replyto);
	g_free(compose->cc);
	g_free(compose->bcc);
	g_free(compose->newsgroups);
	g_free(compose->followup_to);

	g_free(compose->ml_post);

	g_free(compose->inreplyto);
	g_free(compose->references);
	g_free(compose->msgid);
	g_free(compose->boundary);

	if (compose->undostruct)
		undo_destroy(compose->undostruct);

	g_free(compose->sig_str);

	g_free(compose->exteditor_file);

	for (row = 0; (ainfo = gtk_clist_get_row_data(clist, row)) != NULL;
	     row++)
		compose_attach_info_free(ainfo);

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(NULL);

	prefs_common.compose_width = compose->scrolledwin->allocation.width;
	prefs_common.compose_height = compose->window->allocation.height;

	gtk_widget_destroy(compose->paned);

	g_free(compose);

	compose_list = g_list_remove(compose_list, compose);
}

static void compose_attach_info_free(AttachInfo *ainfo)
{
	g_free(ainfo->file);
	g_free(ainfo->content_type);
	g_free(ainfo->name);
	g_free(ainfo);
}

static void compose_attach_remove_selected(Compose *compose)
{
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	AttachInfo *ainfo;
	gint row;

	while (clist->selection != NULL) {
		row = GPOINTER_TO_INT(clist->selection->data);
		ainfo = gtk_clist_get_row_data(clist, row);
		compose_attach_info_free(ainfo);
		gtk_clist_remove(clist, row);
	}
}

static struct _AttachProperty
{
	GtkWidget *window;
	GtkWidget *mimetype_entry;
	GtkWidget *encoding_optmenu;
	GtkWidget *path_entry;
	GtkWidget *filename_entry;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
} attach_prop;

static void compose_attach_property(Compose *compose)
{
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	AttachInfo *ainfo;
	gint row;
	GtkOptionMenu *optmenu;
	static gboolean cancelled;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);

	ainfo = gtk_clist_get_row_data(clist, row);
	if (!ainfo) return;

	if (!attach_prop.window)
		compose_attach_property_create(&cancelled);
	gtk_widget_grab_focus(attach_prop.ok_btn);
	gtk_widget_show(attach_prop.window);
	manage_window_set_transient(GTK_WINDOW(attach_prop.window));

	optmenu = GTK_OPTION_MENU(attach_prop.encoding_optmenu);
	if (ainfo->encoding == ENC_UNKNOWN)
		gtk_option_menu_set_history(optmenu, ENC_BASE64);
	else
		gtk_option_menu_set_history(optmenu, ainfo->encoding);

	gtk_entry_set_text(GTK_ENTRY(attach_prop.mimetype_entry),
			   ainfo->content_type ? ainfo->content_type : "");
	gtk_entry_set_text(GTK_ENTRY(attach_prop.path_entry),
			   ainfo->file ? ainfo->file : "");
	gtk_entry_set_text(GTK_ENTRY(attach_prop.filename_entry),
			   ainfo->name ? ainfo->name : "");

	for (;;) {
		const gchar *entry_text;
		gchar *text;
		gchar *cnttype = NULL;
		gchar *file = NULL;
		off_t size = 0;
		GtkWidget *menu;
		GtkWidget *menuitem;

		cancelled = FALSE;
		gtk_main();

		if (cancelled == TRUE) {
			gtk_widget_hide(attach_prop.window);
			break;
		}

		entry_text = gtk_entry_get_text
			(GTK_ENTRY(attach_prop.mimetype_entry));
		if (*entry_text != '\0') {
			gchar *p;

			text = g_strstrip(g_strdup(entry_text));
			if ((p = strchr(text, '/')) && !strchr(p + 1, '/')) {
				cnttype = g_strdup(text);
				g_free(text);
			} else {
				alertpanel_error(_("Invalid MIME type."));
				g_free(text);
				continue;
			}
		}

		menu = gtk_option_menu_get_menu(optmenu);
		menuitem = gtk_menu_get_active(GTK_MENU(menu));
		ainfo->encoding = GPOINTER_TO_INT
			(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));

		entry_text = gtk_entry_get_text
			(GTK_ENTRY(attach_prop.path_entry));
		if (*entry_text != '\0') {
			if (is_file_exist(entry_text) &&
			    (size = get_file_size(entry_text)) > 0)
				file = g_strdup(entry_text);
			else {
				alertpanel_error
					(_("File doesn't exist or is empty."));
				g_free(cnttype);
				continue;
			}
		}

		entry_text = gtk_entry_get_text
			(GTK_ENTRY(attach_prop.filename_entry));
		if (*entry_text != '\0') {
			g_free(ainfo->name);
			ainfo->name = g_strdup(entry_text);
		}

		if (cnttype) {
			g_free(ainfo->content_type);
			ainfo->content_type = cnttype;
		}
		if (file) {
			g_free(ainfo->file);
			ainfo->file = file;
		}
		if (size)
			ainfo->size = size;

		gtk_clist_set_text(clist, row, COL_MIMETYPE,
				   ainfo->content_type);
		gtk_clist_set_text(clist, row, COL_SIZE,
				   to_human_readable(ainfo->size));
		gtk_clist_set_text(clist, row, COL_NAME, ainfo->name);

		gtk_widget_hide(attach_prop.window);
		break;
	}
}

#define SET_LABEL_AND_ENTRY(str, entry, top) \
{ \
	label = gtk_label_new(str); \
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), \
			 GTK_FILL, 0, 0, 0); \
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5); \
 \
	entry = gtk_entry_new(); \
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, top, (top + 1), \
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0); \
}

static void compose_attach_property_create(gboolean *cancelled)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *mimetype_entry;
	GtkWidget *hbox;
	GtkWidget *optmenu;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;
	GtkWidget *path_entry;
	GtkWidget *filename_entry;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	debug_print("Creating attach_property window...\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, 480, -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_title(GTK_WINDOW(window), _("Properties"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(attach_property_delete_event),
			 cancelled);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(attach_property_key_pressed),
			 cancelled);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	table = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	SET_LABEL_AND_ENTRY(_("MIME type"), mimetype_entry, 0);

	label = gtk_label_new(_("Encoding"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 1, 2,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new();
	MENUITEM_ADD(optmenu_menu, menuitem, "7bit", ENC_7BIT);
	gtk_widget_set_sensitive(menuitem, FALSE);
	MENUITEM_ADD(optmenu_menu, menuitem, "8bit", ENC_8BIT);
	gtk_widget_set_sensitive(menuitem, FALSE);
	MENUITEM_ADD(optmenu_menu, menuitem, "quoted-printable",
		     ENC_QUOTED_PRINTABLE);
	MENUITEM_ADD(optmenu_menu, menuitem, "base64", ENC_BASE64);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), optmenu_menu);

	SET_LABEL_AND_ENTRY(_("Path"),      path_entry,     2);
	SET_LABEL_AND_ENTRY(_("File name"), filename_entry, 3);

	gtkut_stock_button_set_create(&hbbox, &ok_btn, GTK_STOCK_OK,
				      &cancel_btn, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(attach_property_ok),
			 cancelled);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(attach_property_cancel),
			 cancelled);

	gtk_widget_show_all(vbox);

	attach_prop.window           = window;
	attach_prop.mimetype_entry   = mimetype_entry;
	attach_prop.encoding_optmenu = optmenu;
	attach_prop.path_entry       = path_entry;
	attach_prop.filename_entry   = filename_entry;
	attach_prop.ok_btn           = ok_btn;
	attach_prop.cancel_btn       = cancel_btn;
}

#undef SET_LABEL_AND_ENTRY

static void attach_property_ok(GtkWidget *widget, gboolean *cancelled)
{
	*cancelled = FALSE;
	gtk_main_quit();
}

static void attach_property_cancel(GtkWidget *widget, gboolean *cancelled)
{
	*cancelled = TRUE;
	gtk_main_quit();
}

static gint attach_property_delete_event(GtkWidget *widget, GdkEventAny *event,
					 gboolean *cancelled)
{
	*cancelled = TRUE;
	gtk_main_quit();

	return TRUE;
}

static gboolean attach_property_key_pressed(GtkWidget *widget,
					    GdkEventKey *event,
					    gboolean *cancelled)
{
	if (event && event->keyval == GDK_Escape) {
		*cancelled = TRUE;
		gtk_main_quit();
	}
	return FALSE;
}

static void compose_exec_ext_editor(Compose *compose)
{
	gchar *tmp;
	pid_t pid;
	gint pipe_fds[2];

	tmp = g_strdup_printf("%s%ctmpmsg.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);

	if (pipe(pipe_fds) < 0) {
		perror("pipe");
		g_free(tmp);
		return;
	}

	if ((pid = fork()) < 0) {
		perror("fork");
		g_free(tmp);
		return;
	}

	if (pid != 0) {
		/* close the write side of the pipe */
		close(pipe_fds[1]);

		compose->exteditor_file    = g_strdup(tmp);
		compose->exteditor_pid     = pid;
		compose->exteditor_readdes = pipe_fds[0];

		compose_set_ext_editor_sensitive(compose, FALSE);

		compose->exteditor_tag =
			gdk_input_add(pipe_fds[0], GDK_INPUT_READ,
				      compose_input_cb, compose);
	} else {	/* process-monitoring process */
		pid_t pid_ed;

		if (setpgid(0, 0))
			perror("setpgid");

		/* close the read side of the pipe */
		close(pipe_fds[0]);

		if (compose_write_body_to_file(compose, tmp) < 0) {
			fd_write_all(pipe_fds[1], "2\n", 2);
			_exit(1);
		}

		pid_ed = compose_exec_ext_editor_real(tmp);
		if (pid_ed < 0) {
			fd_write_all(pipe_fds[1], "1\n", 2);
			_exit(1);
		}

		/* wait until editor is terminated */
		waitpid(pid_ed, NULL, 0);

		fd_write_all(pipe_fds[1], "0\n", 2);

		close(pipe_fds[1]);
		_exit(0);
	}

	g_free(tmp);
}

static gint compose_exec_ext_editor_real(const gchar *file)
{
	static gchar *def_cmd = "emacs %s";
	gchar buf[1024];
	gchar *p;
	gchar **cmdline;
	pid_t pid;

	g_return_val_if_fail(file != NULL, -1);

	if ((pid = fork()) < 0) {
		perror("fork");
		return -1;
	}

	if (pid != 0) return pid;

	/* grandchild process */

	if (setpgid(0, getppid()))
		perror("setpgid");

	if (prefs_common.ext_editor_cmd &&
	    (p = strchr(prefs_common.ext_editor_cmd, '%')) &&
	    *(p + 1) == 's' && !strchr(p + 2, '%')) {
		g_snprintf(buf, sizeof(buf), prefs_common.ext_editor_cmd, file);
	} else {
		if (prefs_common.ext_editor_cmd)
			g_warning(_("External editor command line is invalid: `%s'\n"),
				  prefs_common.ext_editor_cmd);
		g_snprintf(buf, sizeof(buf), def_cmd, file);
	}

	cmdline = strsplit_with_quote(buf, " ", 1024);
	execvp(cmdline[0], cmdline);

	perror("execvp");
	g_strfreev(cmdline);

	_exit(1);
}

static gboolean compose_ext_editor_kill(Compose *compose)
{
	pid_t pgid = compose->exteditor_pid * -1;
	gint ret;

	ret = kill(pgid, 0);

	if (ret == 0 || (ret == -1 && EPERM == errno)) {
		AlertValue val;
		gchar *msg;

		msg = g_strdup_printf
			(_("The external editor is still working.\n"
			   "Force terminating the process?\n"
			   "process group id: %d"), -pgid);
		val = alertpanel(_("Notice"), msg, _("Yes"), _("+No"), NULL);
		g_free(msg);

		if (val == G_ALERTDEFAULT) {
			gdk_input_remove(compose->exteditor_tag);
			close(compose->exteditor_readdes);

			if (kill(pgid, SIGTERM) < 0) perror("kill");
			waitpid(compose->exteditor_pid, NULL, 0);

			g_warning(_("Terminated process group id: %d"), -pgid);
			g_warning(_("Temporary file: %s"),
				  compose->exteditor_file);

			compose_set_ext_editor_sensitive(compose, TRUE);

			g_free(compose->exteditor_file);
			compose->exteditor_file    = NULL;
			compose->exteditor_pid     = -1;
			compose->exteditor_readdes = -1;
			compose->exteditor_tag     = -1;
		} else
			return FALSE;
	}

	return TRUE;
}

static void compose_input_cb(gpointer data, gint source,
			     GdkInputCondition condition)
{
	gchar buf[3];
	Compose *compose = (Compose *)data;
	gint i = 0;

	debug_print(_("Compose: input from monitoring process\n"));

	gdk_input_remove(compose->exteditor_tag);

	for (;;) {
		if (read(source, &buf[i], 1) < 1) {
			buf[0] = '3';
			break;
		}
		if (buf[i] == '\n') {
			buf[i] = '\0';
			break;
		}
		i++;
		if (i == sizeof(buf) - 1)
			break;
	}

	waitpid(compose->exteditor_pid, NULL, 0);

	if (buf[0] == '0') {		/* success */
		GtkTextView *text = GTK_TEXT_VIEW(compose->text);
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer(text);

		gtk_text_buffer_set_text(buffer, "", 0);
		compose_insert_file(compose, compose->exteditor_file);
		compose_changed_cb(NULL, compose);

		if (unlink(compose->exteditor_file) < 0)
			FILE_OP_ERROR(compose->exteditor_file, "unlink");
	} else if (buf[0] == '1') {	/* failed */
		g_warning(_("Couldn't exec external editor\n"));
		if (unlink(compose->exteditor_file) < 0)
			FILE_OP_ERROR(compose->exteditor_file, "unlink");
	} else if (buf[0] == '2') {
		g_warning(_("Couldn't write to file\n"));
	} else if (buf[0] == '3') {
		g_warning(_("Pipe read failed\n"));
	}

	close(source);

	compose_set_ext_editor_sensitive(compose, TRUE);

	g_free(compose->exteditor_file);
	compose->exteditor_file    = NULL;
	compose->exteditor_pid     = -1;
	compose->exteditor_readdes = -1;
	compose->exteditor_tag     = -1;
}

static void compose_set_ext_editor_sensitive(Compose *compose,
					     gboolean sensitive)
{
	GtkItemFactory *ifactory;

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	menu_set_sensitive(ifactory, "/File/Send", sensitive);
	menu_set_sensitive(ifactory, "/File/Send later", sensitive);
	menu_set_sensitive(ifactory, "/File/Save to draft folder",
			   sensitive);
	menu_set_sensitive(ifactory, "/File/Insert file", sensitive);
	menu_set_sensitive(ifactory, "/File/Insert signature", sensitive);
	menu_set_sensitive(ifactory, "/Edit/Wrap current paragraph", sensitive);
	menu_set_sensitive(ifactory, "/Edit/Wrap all long lines", sensitive);
	menu_set_sensitive(ifactory, "/Tools/Edit with external editor",
			   sensitive);

	gtk_widget_set_sensitive(compose->text,          sensitive);
	gtk_widget_set_sensitive(compose->send_btn,      sensitive);
	gtk_widget_set_sensitive(compose->sendl_btn,     sensitive);
	gtk_widget_set_sensitive(compose->draft_btn,     sensitive);
	gtk_widget_set_sensitive(compose->insert_btn,    sensitive);
	gtk_widget_set_sensitive(compose->sig_btn,       sensitive);
	gtk_widget_set_sensitive(compose->exteditor_btn, sensitive);
	gtk_widget_set_sensitive(compose->linewrap_btn,  sensitive);
}

/**
 * compose_undo_state_changed:
 *
 * Change the sensivity of the menuentries undo and redo
 **/
static void compose_undo_state_changed(UndoMain *undostruct, gint undo_state,
				       gint redo_state, gpointer data)
{
	GtkWidget *widget = GTK_WIDGET(data);
	GtkItemFactory *ifactory;

	g_return_if_fail(widget != NULL);

	ifactory = gtk_item_factory_from_widget(widget);

	switch (undo_state) {
	case UNDO_STATE_TRUE:
		if (!undostruct->undo_state) {
			debug_print ("Set_undo - Testpoint\n");
			undostruct->undo_state = TRUE;
			menu_set_sensitive(ifactory, "/Edit/Undo", TRUE);
		}
		break;
	case UNDO_STATE_FALSE:
		if (undostruct->undo_state) {
			undostruct->undo_state = FALSE;
			menu_set_sensitive(ifactory, "/Edit/Undo", FALSE);
		}
		break;
	case UNDO_STATE_UNCHANGED:
		break;
	case UNDO_STATE_REFRESH:
		menu_set_sensitive(ifactory, "/Edit/Undo",
				   undostruct->undo_state);
		break;
	default:
		g_warning("Undo state not recognized");
		break;
	}

	switch (redo_state) {
	case UNDO_STATE_TRUE:
		if (!undostruct->redo_state) {
			undostruct->redo_state = TRUE;
			menu_set_sensitive(ifactory, "/Edit/Redo", TRUE);
		}
		break;
	case UNDO_STATE_FALSE:
		if (undostruct->redo_state) {
			undostruct->redo_state = FALSE;
			menu_set_sensitive(ifactory, "/Edit/Redo", FALSE);
		}
		break;
	case UNDO_STATE_UNCHANGED:
		break;
	case UNDO_STATE_REFRESH:
		menu_set_sensitive(ifactory, "/Edit/Redo",
				   undostruct->redo_state);
		break;
	default:
		g_warning("Redo state not recognized");
		break;
	}
}

static gint calc_cursor_xpos(GtkTextView *text, gint extra, gint char_width)
{
#if 0
	gint cursor_pos;

	cursor_pos = (text->cursor_pos_x - extra) / char_width;
	cursor_pos = MAX(cursor_pos, 0);

	return cursor_pos;
#endif
#warning FIXME_GTK2
	return 0;
}

/* callback functions */

/* compose_edit_size_alloc() - called when resized. don't know whether Gtk
 * includes "non-client" (windows-izm) in calculation, so this calculation
 * may not be accurate.
 */
static gboolean compose_edit_size_alloc(GtkEditable *widget,
					GtkAllocation *allocation,
					GtkSHRuler *shruler)
{
	if (prefs_common.show_ruler) {
		gint char_width = 0, char_height = 0;
		gint line_width_in_chars;

		gtkut_get_font_size(GTK_WIDGET(widget),
				    &char_width, &char_height);
		line_width_in_chars =
			(allocation->width - allocation->x) / char_width;

		/* got the maximum */
		gtk_ruler_set_range(GTK_RULER(shruler),
				    0.0, line_width_in_chars,
				    calc_cursor_xpos(GTK_TEXT_VIEW(widget),
						     allocation->x,
						     char_width),
				    /*line_width_in_chars*/ char_width);
	}

	return TRUE;
}

static void toolbar_send_cb(GtkWidget *widget, gpointer data)
{
	compose_send_cb(data, 0, NULL);
}

static void toolbar_send_later_cb(GtkWidget *widget, gpointer data)
{
	compose_send_later_cb(data, 0, NULL);
}

static void toolbar_draft_cb(GtkWidget *widget, gpointer data)
{
	compose_draft_cb(data, 0, NULL);
}

static void toolbar_insert_cb(GtkWidget *widget, gpointer data)
{
	compose_insert_file_cb(data, 0, NULL);
}

static void toolbar_attach_cb(GtkWidget *widget, gpointer data)
{
	compose_attach_cb(data, 0, NULL);
}

static void toolbar_sig_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;

	compose_insert_sig(compose, FALSE);
}

static void toolbar_ext_editor_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;

	compose_exec_ext_editor(compose);
}

static void toolbar_linewrap_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;

	compose_wrap_line_all(compose);
}

static void toolbar_address_cb(GtkWidget *widget, gpointer data)
{
	compose_address_cb(data, 0, NULL);
}

static void account_activated(GtkMenuItem *menuitem, gpointer data)
{
	Compose *compose = (Compose *)data;

	PrefsAccount *ac;

	ac = (PrefsAccount *)g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID);
	g_return_if_fail(ac != NULL);

	if (ac != compose->account)
		compose_select_account(compose, ac, FALSE);
}

static void attach_selected(GtkCList *clist, gint row, gint column,
			    GdkEvent *event, gpointer data)
{
	Compose *compose = (Compose *)data;

	if (event && event->type == GDK_2BUTTON_PRESS)
		compose_attach_property(compose);
}

static gboolean attach_button_pressed(GtkWidget *widget, GdkEventButton *event,
				      gpointer data)
{
	Compose *compose = (Compose *)data;
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	gint row, column;

	if (!event) return FALSE;

	if (event->button == 3) {
		if ((clist->selection && !clist->selection->next) ||
		    !clist->selection) {
			gtk_clist_unselect_all(clist);
			if (gtk_clist_get_selection_info(clist,
							 event->x, event->y,
							 &row, &column)) {
				gtk_clist_select_row(clist, row, column);
				gtkut_clist_set_focus_row(clist, row);
			}
		}
		gtk_menu_popup(GTK_MENU(compose->popupmenu), NULL, NULL,
			       NULL, NULL, event->button, event->time);
	}

	return FALSE;
}

static gboolean attach_key_pressed(GtkWidget *widget, GdkEventKey *event,
				   gpointer data)
{
	Compose *compose = (Compose *)data;

	if (!event) return FALSE;

	switch (event->keyval) {
	case GDK_Delete:
		compose_attach_remove_selected(compose);
		break;
	}

	return FALSE;
}

static void compose_send_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	gint val;

	val = compose_send(compose);

	if (val == 0) gtk_widget_destroy(compose->window);
}

static void compose_send_later_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	FolderItem *queue;
	gchar tmp[MAXPATHLEN + 1];

	if (compose_check_entries(compose) == FALSE)
		return;

	queue = account_get_special_folder(compose->account, F_QUEUE);
	if (!queue) {
		g_warning("can't find queue folder\n");
		return;
	}
	if (!FOLDER_IS_LOCAL(queue->folder) &&
	    !main_window_toggle_online_if_offline(main_window_get()))
		return;

	g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg.%p",
		   get_tmp_dir(), G_DIR_SEPARATOR, compose);

	if (compose->mode == COMPOSE_REDIRECT) {
		if (compose_redirect_write_to_file(compose, tmp) < 0) {
			alertpanel_error(_("Can't queue the message."));
			return;
		}
	} else {
		if (prefs_common.linewrap_at_send)
			compose_wrap_line_all(compose);

		if (compose_write_to_file(compose, tmp, FALSE) < 0) {
			alertpanel_error(_("Can't queue the message."));
			return;
		}
	}

	if (compose_queue(compose, tmp) < 0) {
		alertpanel_error(_("Can't queue the message."));
		return;
	}

	if (unlink(tmp) < 0)
		FILE_OP_ERROR(tmp, "unlink");

	gtk_widget_destroy(compose->window);
}

static void compose_draft_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	FolderItem *draft;
	gchar *tmp;
	gint msgnum;
	MsgFlags flag = {0, 0};
	static gboolean lock = FALSE;

	if (lock) return;

	draft = account_get_special_folder(compose->account, F_DRAFT);
	g_return_if_fail(draft != NULL);

	lock = TRUE;

	tmp = g_strdup_printf("%s%cdraft.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);

	if (compose_write_to_file(compose, tmp, TRUE) < 0) {
		g_free(tmp);
		lock = FALSE;
		return;
	}

	folder_item_scan(draft);
	if ((msgnum = folder_item_add_msg(draft, tmp, &flag, TRUE)) < 0) {
		unlink(tmp);
		g_free(tmp);
		lock = FALSE;
		return;
	}
	g_free(tmp);
	draft->mtime = 0;	/* force updating */

	if (compose->mode == COMPOSE_REEDIT) {
		compose_remove_reedit_target(compose);
		if (compose->targetinfo &&
		    compose->targetinfo->folder != draft)
			folderview_update_item(compose->targetinfo->folder,
					       TRUE);
	}

	folder_item_scan(draft);
	folderview_update_item(draft, TRUE);

	lock = FALSE;

	/* 0: quit editing  1: keep editing */
	if (action == 0)
		gtk_widget_destroy(compose->window);
	else {
		struct stat s;
		gchar *path;

		path = folder_item_fetch_msg(draft, msgnum);
		g_return_if_fail(path != NULL);
		if (stat(path, &s) < 0) {
			FILE_OP_ERROR(path, "stat");
			g_free(path);
			lock = FALSE;
			return;
		}
		g_free(path);

		procmsg_msginfo_free(compose->targetinfo);
		compose->targetinfo = g_new0(MsgInfo, 1);
		compose->targetinfo->msgnum = msgnum;
		compose->targetinfo->size = s.st_size;
		compose->targetinfo->mtime = s.st_mtime;
		compose->targetinfo->folder = draft;
		compose->mode = COMPOSE_REEDIT;
	}
}

static void compose_attach_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	const gchar *file;

	file = filesel_select_file(_("Select file"), NULL);

	if (file && *file)
		compose_attach_append(compose, file, file, NULL);
}

static void compose_insert_file_cb(gpointer data, guint action,
				   GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	const gchar *file;

	file = filesel_select_file(_("Select file"), NULL);

	if (file && *file)
		compose_insert_file(compose, file);
}

static void compose_insert_sig_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose_insert_sig(compose, FALSE);
}

static gint compose_delete_cb(GtkWidget *widget, GdkEventAny *event,
			      gpointer data)
{
	compose_close_cb(data, 0, NULL);
	return TRUE;
}

static void compose_close_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	AlertValue val;

	if (compose->exteditor_tag != -1) {
		if (!compose_ext_editor_kill(compose))
			return;
	}

	if (compose->modified) {
		val = alertpanel(_("Discard message"),
				 _("This message has been modified. discard it?"),
				 _("Discard"), _("to Draft"), _("Cancel"));

		switch (val) {
		case G_ALERTDEFAULT:
			break;
		case G_ALERTALTERNATE:
			compose_draft_cb(data, 0, NULL);
			return;
		default:
			return;
		}
	}

	gtk_widget_destroy(compose->window);
}

static void compose_address_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	addressbook_open(compose);
}

static void compose_template_activate_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;
	Template *tmpl;
	gchar *msg;
	AlertValue val;

	tmpl = g_object_get_data(G_OBJECT(widget), "template");
	g_return_if_fail(tmpl != NULL);

	msg = g_strdup_printf(_("Do you want to apply the template `%s' ?"),
			      tmpl->name);
	val = alertpanel(_("Apply template"), msg,
			 _("Replace"), _("Insert"), _("Cancel"));
	g_free(msg);

	if (val == G_ALERTDEFAULT)
		compose_template_apply(compose, tmpl, TRUE);
	else if (val == G_ALERTALTERNATE)
		compose_template_apply(compose, tmpl, FALSE);
}

static void compose_ext_editor_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose_exec_ext_editor(compose);
}

static void compose_destroy_cb(GtkWidget *widget, Compose *compose)
{
	compose_destroy(compose);
}

static void compose_undo_cb(Compose *compose)
{
	undo_undo(compose->undostruct);
}

static void compose_redo_cb(Compose *compose)
{
	undo_redo(compose->undostruct);
}

static void compose_cut_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable)) {
		if (GTK_IS_EDITABLE(compose->focused_editable)) {
			gtk_editable_cut_clipboard
				(GTK_EDITABLE(compose->focused_editable));
		} else if (GTK_IS_TEXT_VIEW(compose->focused_editable)) {
			GtkTextView *text = GTK_TEXT_VIEW(compose->text);
			GtkTextBuffer *buffer;
			GtkClipboard *clipboard;

			buffer = gtk_text_view_get_buffer(text);
			clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

			gtk_text_buffer_cut_clipboard(buffer, clipboard, TRUE);
		}
	}
}

static void compose_copy_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable)) {
		if (GTK_IS_EDITABLE(compose->focused_editable)) {
			gtk_editable_copy_clipboard
				(GTK_EDITABLE(compose->focused_editable));
		} else if (GTK_IS_TEXT_VIEW(compose->focused_editable)) {
			GtkTextView *text = GTK_TEXT_VIEW(compose->text);
			GtkTextBuffer *buffer;
			GtkClipboard *clipboard;

			buffer = gtk_text_view_get_buffer(text);
			clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

			gtk_text_buffer_copy_clipboard(buffer, clipboard);
		}
	}
}

static void compose_paste_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable)) {
		if (GTK_IS_EDITABLE(compose->focused_editable)) {
			gtk_editable_paste_clipboard
				(GTK_EDITABLE(compose->focused_editable));
		} else if (GTK_IS_TEXT_VIEW(compose->focused_editable)) {
			GtkTextView *text = GTK_TEXT_VIEW(compose->text);
			GtkTextBuffer *buffer;
			GtkClipboard *clipboard;

			buffer = gtk_text_view_get_buffer(text);
			clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

			gtk_text_buffer_paste_clipboard(buffer, clipboard,
							NULL, TRUE);
		}
	}
}

static void compose_paste_as_quote_cb(Compose *compose)
{
	compose->paste_as_quotation = TRUE;

	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable)) {
		if (GTK_IS_EDITABLE(compose->focused_editable)) {
			gtk_editable_paste_clipboard
				(GTK_EDITABLE(compose->focused_editable));
		} else if (GTK_IS_TEXT_VIEW(compose->focused_editable)) {
			GtkTextView *text = GTK_TEXT_VIEW(compose->text);
			GtkTextBuffer *buffer;
			GtkClipboard *clipboard;

			buffer = gtk_text_view_get_buffer(text);
			clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

			gtk_text_buffer_paste_clipboard(buffer, clipboard,
							NULL, TRUE);
		}
	}

	compose->paste_as_quotation = FALSE;
}

static void compose_allsel_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable)) {
		if (GTK_IS_EDITABLE(compose->focused_editable)) {
			gtk_editable_select_region
				(GTK_EDITABLE(compose->focused_editable),
				 0, -1);
		} else if (GTK_IS_TEXT_VIEW(compose->focused_editable)) {
			GtkTextView *text = GTK_TEXT_VIEW(compose->text);
			GtkTextBuffer *buffer;
			GtkTextIter iter;

			buffer = gtk_text_view_get_buffer(text);
			gtk_text_buffer_get_start_iter(buffer, &iter);
			gtk_text_buffer_place_cursor(buffer, &iter);
			gtk_text_buffer_get_end_iter(buffer, &iter);
			gtk_text_buffer_move_mark_by_name
				(buffer, "selection_bound", &iter);
		}
	}
}

static void textview_move_beginning_of_line(GtkTextView *text)
{
}

static void textview_move_forward_character(GtkTextView *text)
{
}

static void textview_move_backward_character(GtkTextView *text)
{
}

static void textview_move_forward_word(GtkTextView *text)
{
}

static void textview_move_backward_word(GtkTextView *text)
{
}

static void textview_move_end_of_line(GtkTextView *text)
{
}

static void textview_move_next_line(GtkTextView *text)
{
}

static void textview_move_previous_line(GtkTextView *text)
{
}

static void textview_delete_forward_character(GtkTextView *text)
{
}

static void textview_delete_backward_character(GtkTextView *text)
{
}

static void textview_delete_forward_word(GtkTextView *text)
{
}

static void textview_delete_backward_word(GtkTextView *text)
{
}

static void textview_delete_line(GtkTextView *text)
{
}

static void textview_delete_to_line_end(GtkTextView *text)
{
}

static void compose_action_cb(Compose *compose, ComposeAction action)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	static struct {
		void (*do_action) (GtkTextView *text);
	} action_table[] = {
		{textview_move_beginning_of_line},
		{textview_move_forward_character},
		{textview_move_backward_character},
		{textview_move_forward_word},
		{textview_move_backward_word},
		{textview_move_end_of_line},
		{textview_move_next_line},
		{textview_move_previous_line},
		{textview_delete_forward_character},
		{textview_delete_backward_character},
		{textview_delete_forward_word},
		{textview_delete_backward_word},
		{textview_delete_line},
		{NULL}, /* textview_delete_line_n */
		{textview_delete_to_line_end}
	};

	if (!GTK_WIDGET_HAS_FOCUS(text)) return;

	if (action >= COMPOSE_ACTION_MOVE_BEGINNING_OF_LINE &&
	    action <= COMPOSE_ACTION_DELETE_TO_LINE_END)
		action_table[action].do_action(text);
}

static void compose_grab_focus_cb(GtkWidget *widget, Compose *compose)
{
	if (GTK_IS_EDITABLE(widget) || GTK_IS_TEXT_VIEW(widget))
		compose->focused_editable = widget;
}

static void compose_changed_cb(GtkTextBuffer *textbuf, Compose *compose)
{
	if (compose->modified == FALSE) {
		compose->modified = TRUE;
		compose_set_title(compose);
	}
}

static void compose_toggle_autowrap_cb(gpointer data, guint action,
				       GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose->autowrap = GTK_CHECK_MENU_ITEM(widget)->active;
	if (compose->autowrap)
		compose_wrap_line_all_full(compose, TRUE);
}

static void compose_toggle_to_cb(gpointer data, guint action,
				 GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->to_hbox);
		gtk_widget_show(compose->to_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 1, 4);
		compose->use_to = TRUE;
	} else {
		gtk_widget_hide(compose->to_hbox);
		gtk_widget_hide(compose->to_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 1, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_to = FALSE;
	}

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(compose);
}

static void compose_toggle_cc_cb(gpointer data, guint action,
				 GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->cc_hbox);
		gtk_widget_show(compose->cc_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 3, 4);
		compose->use_cc = TRUE;
	} else {
		gtk_widget_hide(compose->cc_hbox);
		gtk_widget_hide(compose->cc_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 3, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_cc = FALSE;
	}

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(compose);
}

static void compose_toggle_bcc_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->bcc_hbox);
		gtk_widget_show(compose->bcc_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 4, 4);
		compose->use_bcc = TRUE;
	} else {
		gtk_widget_hide(compose->bcc_hbox);
		gtk_widget_hide(compose->bcc_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 4, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_bcc = FALSE;
	}

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(compose);
}

static void compose_toggle_replyto_cb(gpointer data, guint action,
				      GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->reply_hbox);
		gtk_widget_show(compose->reply_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 5, 4);
		compose->use_replyto = TRUE;
	} else {
		gtk_widget_hide(compose->reply_hbox);
		gtk_widget_hide(compose->reply_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 5, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_replyto = FALSE;
	}
}

static void compose_toggle_followupto_cb(gpointer data, guint action,
					 GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->followup_hbox);
		gtk_widget_show(compose->followup_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 6, 4);
		compose->use_followupto = TRUE;
	} else {
		gtk_widget_hide(compose->followup_hbox);
		gtk_widget_hide(compose->followup_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 6, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_followupto = FALSE;
	}
}

static void compose_toggle_attach_cb(gpointer data, guint action,
				     GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_ref(compose->edit_vbox);

		gtkut_container_remove(GTK_CONTAINER(compose->vbox2),
		 		       compose->edit_vbox);
		gtk_paned_add2(GTK_PANED(compose->paned), compose->edit_vbox);
		gtk_box_pack_start(GTK_BOX(compose->vbox2), compose->paned,
				   TRUE, TRUE, 0);
		gtk_widget_show(compose->paned);

		gtk_widget_unref(compose->edit_vbox);
		gtk_widget_unref(compose->paned);

		compose->use_attach = TRUE;
	} else {
		gtk_widget_ref(compose->paned);
		gtk_widget_ref(compose->edit_vbox);

		gtkut_container_remove(GTK_CONTAINER(compose->vbox2),
		  		       compose->paned);
		gtkut_container_remove(GTK_CONTAINER(compose->paned),
		  		       compose->edit_vbox);
		gtk_box_pack_start(GTK_BOX(compose->vbox2),
				   compose->edit_vbox, TRUE, TRUE, 0);

		gtk_widget_unref(compose->edit_vbox);

		compose->use_attach = FALSE;
	}
}

#if USE_GPGME
static void compose_toggle_sign_cb(gpointer data, guint action,
				   GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->use_signing = TRUE;
	else
		compose->use_signing = FALSE;
}

static void compose_toggle_encrypt_cb(gpointer data, guint action,
				      GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->use_encryption = TRUE;
	else
		compose->use_encryption = FALSE;
}
#endif /* USE_GPGME */

static void compose_toggle_ruler_cb(gpointer data, guint action,
				    GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->ruler_hbox);
		prefs_common.show_ruler = TRUE;
	} else {
		gtk_widget_hide(compose->ruler_hbox);
		gtk_widget_queue_resize(compose->edit_vbox);
		prefs_common.show_ruler = FALSE;
	}
}

static void compose_attach_drag_received_cb (GtkWidget		*widget,
					     GdkDragContext	*drag_context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data)
{
	Compose *compose = (Compose *)user_data;
	GList *list, *tmp;

	list = uri_list_extract_filenames((const gchar *)data->data);
	for (tmp = list; tmp != NULL; tmp = tmp->next)
		compose_attach_append
			(compose, (const gchar *)tmp->data,
			 (const gchar *)tmp->data, NULL);
	if (list) compose_changed_cb(NULL, compose);
	list_free_strings(list);
	g_list_free(list);
}

static void compose_insert_drag_received_cb (GtkWidget		*widget,
					     GdkDragContext	*drag_context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data)
{
	Compose *compose = (Compose *)user_data;
	GList *list, *tmp;

	list = uri_list_extract_filenames((const gchar *)data->data);
	for (tmp = list; tmp != NULL; tmp = tmp->next)
		compose_insert_file(compose, (const gchar *)tmp->data);
	list_free_strings(list);
	g_list_free(list);
}

static void to_activated(GtkWidget *widget, Compose *compose)
{
	if (compose_send_control_enter(compose)) return;

	if (GTK_WIDGET_VISIBLE(compose->newsgroups_entry))
		gtk_widget_grab_focus(compose->newsgroups_entry);
	else if (GTK_WIDGET_VISIBLE(compose->cc_entry))
		gtk_widget_grab_focus(compose->cc_entry);
	else if (GTK_WIDGET_VISIBLE(compose->bcc_entry))
		gtk_widget_grab_focus(compose->bcc_entry);
	else if (GTK_WIDGET_VISIBLE(compose->reply_entry))
		gtk_widget_grab_focus(compose->reply_entry);
	else if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->subject_entry);
}

static void newsgroups_activated(GtkWidget *widget, Compose *compose)
{
	if (compose_send_control_enter(compose)) return;

	if (GTK_WIDGET_VISIBLE(compose->cc_entry))
		gtk_widget_grab_focus(compose->cc_entry);
	else if (GTK_WIDGET_VISIBLE(compose->bcc_entry))
		gtk_widget_grab_focus(compose->bcc_entry);
	else if (GTK_WIDGET_VISIBLE(compose->reply_entry))
		gtk_widget_grab_focus(compose->reply_entry);
	else if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->subject_entry);
}

static void cc_activated(GtkWidget *widget, Compose *compose)
{
	if (compose_send_control_enter(compose)) return;

	if (GTK_WIDGET_VISIBLE(compose->bcc_entry))
		gtk_widget_grab_focus(compose->bcc_entry);
	else if (GTK_WIDGET_VISIBLE(compose->reply_entry))
		gtk_widget_grab_focus(compose->reply_entry);
	else if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->subject_entry);
}

static void bcc_activated(GtkWidget *widget, Compose *compose)
{
	if (compose_send_control_enter(compose)) return;

	if (GTK_WIDGET_VISIBLE(compose->reply_entry))
		gtk_widget_grab_focus(compose->reply_entry);
	else if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->subject_entry);
}

static void replyto_activated(GtkWidget *widget, Compose *compose)
{
	if (compose_send_control_enter(compose)) return;

	if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->subject_entry);
}

static void followupto_activated(GtkWidget *widget, Compose *compose)
{
	if (compose_send_control_enter(compose)) return;

	gtk_widget_grab_focus(compose->subject_entry);
}

static void subject_activated(GtkWidget *widget, Compose *compose)
{
	if (compose_send_control_enter(compose)) return;

	gtk_widget_grab_focus(compose->text);
}

static void text_activated(GtkWidget *widget, Compose *compose)
{
	compose_send_control_enter(compose);
}

static void text_inserted(GtkTextBuffer *buffer, GtkTextIter *iter,
			  const gchar *text, gint len, Compose *compose)
{
	g_return_if_fail(text != NULL);

	g_signal_handlers_block_by_func(G_OBJECT(buffer),
					G_CALLBACK(text_inserted),
					compose);

	if (compose->paste_as_quotation) {
		gchar *new_text;
		gchar *qmark;

		if (len < 0)
			len = strlen(text);
		new_text = g_strndup(text, len);
		if (prefs_common.quotemark && *prefs_common.quotemark)
			qmark = prefs_common.quotemark;
		else
			qmark = "> ";
		gtk_text_buffer_place_cursor(buffer, iter);
		compose_quote_fmt(compose, NULL, "%Q", qmark, new_text);
		g_free(new_text);
	} else
		gtk_text_buffer_insert(buffer, iter, text, len);

	if (compose->autowrap)
		compose_wrap_line_all_full(compose, TRUE);

	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
					  G_CALLBACK(text_inserted),
					  compose);
	g_signal_stop_emission_by_name(G_OBJECT(buffer), "insert-text");
}

static gboolean compose_send_control_enter(Compose *compose)
{
	GdkEvent *ev;
	GdkEventKey *kev;
	GtkItemFactory *ifactory;
	GtkAccelKey *accel;
	GtkWidget *send_menu;
	GSList *list;
	GdkModifierType ignored_mods =
		(GDK_LOCK_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK |
		 GDK_MOD4_MASK | GDK_MOD5_MASK);

	ev = gtk_get_current_event();
	if (ev->type != GDK_KEY_PRESS) return FALSE;

	kev = (GdkEventKey *)ev;
	if (!(kev->keyval == GDK_Return && (kev->state & GDK_CONTROL_MASK)))
		return FALSE;

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	send_menu = gtk_item_factory_get_widget(ifactory, "/File/Send");
	list = gtk_accel_groups_from_object(G_OBJECT(send_menu));
	if (!list)
		return FALSE;

	accel = (GtkAccelKey *)list->data;
	if (accel && accel->accel_key == kev->keyval &&
	    (accel->accel_mods & ~ignored_mods) ==
	    (kev->state & ~ignored_mods)) {
		compose_send_cb(compose, 0, NULL);
		return TRUE;
	}

	return FALSE;
}
