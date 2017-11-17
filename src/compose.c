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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#ifndef PANGO_ENABLE_ENGINE
#  define PANGO_ENABLE_ENGINE
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtktoolbutton.h>
#include <gtk/gtkseparatortoolitem.h>
#include <gtk/gtktable.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkclipboard.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkimage.h>
#include <gtk/gtktooltips.h>
#include <pango/pango-break.h>

#if USE_GTKSPELL
#  include <gtk/gtkradiomenuitem.h>
#  include <gtkspell/gtkspell.h>
#if USE_ENCHANT
#  include <enchant/enchant.h>
#else
#  include <aspell.h>
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include <signal.h>
#include <errno.h>
#ifdef G_OS_WIN32
#  include <windows.h>
#endif

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
#include "prefs_common_dialog.h"
#include "prefs_account.h"
#include "prefs_toolbar.h"
#include "action.h"
#include "account.h"
#include "account_dialog.h"
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
#include "filter.h"
#include "addr_compl.h"
#include "quote_fmt.h"
#include "template.h"
#include "undo.h"
#include "plugin.h"
#include "md5.h"
#include "inc.h"

#if USE_GPGME
#  include "rfc2015.h"
#endif

enum
{
	COL_MIMETYPE,
	COL_SIZE,
	COL_NAME,
	COL_ATTACH_INFO,
	N_ATTACH_COLS
};

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

#define TEXTVIEW_MARGIN		6

static GdkColor quote_color = {0, 0, 0, 0xbfff};

static GList *compose_list = NULL;

static Compose *compose_create			(PrefsAccount	*account,
						 ComposeMode	 mode);
static Compose *compose_find_window_by_target	(MsgInfo	*msginfo);
static gboolean compose_window_exist		(gint		 x,
						 gint		 y);
static void compose_connect_changed_callbacks	(Compose	*compose);

static GtkWidget *compose_toolbar_create	(Compose	*compose);
static GtkWidget *compose_toolbar_create_from_list
						(Compose	*compose,
						 GList		*item_list);
static void compose_set_toolbar_button_visibility
						(Compose	*compose);

static GtkWidget *compose_account_option_menu_create
						(Compose	*compose,
						 GtkWidget	*hbox);
static GtkWidget *compose_signature_menu_create(Compose		*compose,
						GtkWidget	*hbox);
static void compose_set_out_encoding		(Compose	*compose);
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
static gint compose_parse_source_msg		(Compose	*compose,
						 MsgInfo	*msginfo);

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
						 gboolean	 append,
						 gboolean	 replace,
						 gboolean	 scroll);
static void compose_enable_sig			(Compose	*compose);
static gchar *compose_get_signature_str		(Compose	*compose);

static void compose_insert_file			(Compose	*compose,
						 const gchar	*file,
						 gboolean	 scroll);

static void compose_attach_parts		(Compose	*compose,
						 MsgInfo	*msginfo);

static void compose_wrap_paragraph		(Compose	*compose,
						 GtkTextIter	*par_iter);
static void compose_wrap_all			(Compose	*compose);
static void compose_wrap_all_full		(Compose	*compose,
						 gboolean	 autowrap);

static void compose_set_title			(Compose	*compose);
static void compose_select_account		(Compose	*compose,
						 PrefsAccount	*account,
						 gboolean	 init);
static void compose_update_signature_menu	(Compose	*compose);

static gboolean compose_check_for_valid_recipient
						(Compose	*compose);
static gboolean compose_check_entries		(Compose	*compose);
static gboolean compose_check_attachments	(Compose	*compose);
static gboolean compose_check_recipients	(Compose	*compose);
static gboolean compose_check_activities	(Compose	*compose);

static void compose_add_new_recipients_to_addressbook
						(Compose	*compose);

static gint compose_send_real			(Compose	*compose);
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
static gint compose_write_attach		(Compose	*compose,
						 FILE		*fp,
						 const gchar	*charset);
static gint compose_write_headers		(Compose	*compose,
						 FILE		*fp,
						 const gchar	*charset,
						 const gchar	*body_charset,
						 EncodingType	 encoding,
						 gboolean	 is_draft);
static gint compose_redirect_write_headers	(Compose	*compose,
						 FILE		*fp);

static void compose_convert_header		(Compose	*compose,
						 gchar		*dest,
						 gint		 len,
						 const gchar	*src,
						 gint		 header_len,
						 gboolean	 addr_field,
						 const gchar	*encoding);
static gchar *compose_convert_filename		(Compose	*compose,
						 const gchar	*src,
						 const gchar	*param_name,
						 const gchar	*encoding);
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

static void compose_attach_open			(Compose	*compose);

static void compose_exec_ext_editor		(Compose	*compose);
static gboolean compose_ext_editor_kill		(Compose	*compose);
static void compose_ext_editor_child_exit	(GPid		 pid,
						 gint		 status,
						 gpointer	 data);
static void compose_set_ext_editor_sensitive	(Compose	*compose,
						 gboolean	 sensitive);

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
static void toolbar_prefs_common_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_prefs_account_cb	(GtkWidget	*widget,
					 gpointer	 data);

static gboolean toolbar_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 gpointer	 data);

static void account_activated		(GtkMenuItem	*menuitem,
					 gpointer	 data);
static void sig_combo_changed		(GtkComboBox	*combo,
					 gpointer	 data);

static void attach_selection_changed	(GtkTreeSelection	*selection,
					 gpointer		 data);

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

static void compose_attach_open_cb	(gpointer	 data,
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

static void compose_set_encoding_cb	(gpointer	 data,
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

static gint compose_window_state_cb	(GtkWidget		*widget,
					 GdkEventWindowState	*event,
					 gpointer		 data);

static void compose_undo_cb		(Compose	*compose);
static void compose_redo_cb		(Compose	*compose);
static void compose_cut_cb		(Compose	*compose);
static void compose_copy_cb		(Compose	*compose);
static void compose_paste_cb		(Compose	*compose);
static void compose_paste_as_quote_cb	(Compose	*compose);
static void compose_allsel_cb		(Compose	*compose);

static void compose_grab_focus_cb	(GtkWidget	*widget,
					 Compose	*compose);

#if USE_GPGME
static void compose_signing_toggled	(GtkWidget	*widget,
					 Compose	*compose);
static void compose_encrypt_toggled	(GtkWidget	*widget,
					 Compose	*compose);
#endif

#if 0
static void compose_attach_toggled	(GtkWidget	*widget,
					 Compose	*compose);
#endif

static void compose_buffer_changed_cb	(GtkTextBuffer	*textbuf,
					 Compose	*compose);
static void compose_changed_cb		(GtkEditable	*editable,
					 Compose	*compose);

static void compose_wrap_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
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
static void compose_toggle_ruler_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_attach_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_customize_toolbar_cb(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_toggle_mdn_cb	(gpointer	 data,
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

#if USE_GTKSPELL
static void compose_set_spell_lang_menu (Compose	*compose);
static void compose_change_spell_lang_menu
					(Compose	*compose,
					 const gchar	*lang);
static void compose_toggle_spell_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_set_spell_lang_cb	(GtkWidget	*widget,
					 gpointer	 data);
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

static void text_inserted		(GtkTextBuffer	*buffer,
					 GtkTextIter	*iter,
					 const gchar	*text,
					 gint		 len,
					 Compose	*compose);

static gboolean autosave_timeout	(gpointer	 data);


static GtkItemFactoryEntry compose_popup_entries[] =
{
	{N_("/_Open"),		NULL, compose_attach_open_cb, 0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Add..."),	NULL, compose_attach_cb, 0, NULL},
	{N_("/_Remove"),	NULL, compose_attach_remove_selected, 0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Properties..."),	NULL, compose_attach_property, 0, NULL}
};

static GtkItemFactoryEntry compose_entries[] =
{
	{N_("/_File"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Send"),			"<shift><control>E",
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
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/Insert si_gnature"),	"<control>G", compose_insert_sig_cb,  0, NULL},
	{N_("/_File/A_ppend signature"),	"<shift><control>G", compose_insert_sig_cb,  1, NULL},
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
	{N_("/_Edit/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Wrap current paragraph"),
					"<control>L", compose_wrap_cb, 0, NULL},
	{N_("/_Edit/Wrap all long _lines"),
					"<control><alt>L", compose_wrap_cb, 1, NULL},
	{N_("/_Edit/Aut_o wrapping"),	"<shift><control>L", compose_toggle_autowrap_cb, 0, "<ToggleItem>"},
	{N_("/_View"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_To"),		NULL, compose_toggle_to_cb     , 0, "<ToggleItem>"},
	{N_("/_View/_Cc"),		NULL, compose_toggle_cc_cb     , 0, "<ToggleItem>"},
	{N_("/_View/_Bcc"),		NULL, compose_toggle_bcc_cb    , 0, "<ToggleItem>"},
	{N_("/_View/_Reply-To"),	NULL, compose_toggle_replyto_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Followup-To"),	NULL, compose_toggle_followupto_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/R_uler"),		NULL, compose_toggle_ruler_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Attachment"),	NULL, compose_toggle_attach_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Cu_stomize toolbar..."),
					NULL, compose_customize_toolbar_cb, 0, NULL},
	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},

#define ENC_ACTION(action) \
	NULL, compose_set_encoding_cb, action, \
	"/View/Character encoding/Automatic"

	{N_("/_View/Character _encoding"), NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Character _encoding/_Automatic"),
			NULL, compose_set_encoding_cb, C_AUTO, "<RadioItem>"},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/7bit ascii (US-ASC_II)"),
	 ENC_ACTION(C_US_ASCII)},
	{N_("/_View/Character _encoding/Unicode (_UTF-8)"),
	 ENC_ACTION(C_UTF_8)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Western European (ISO-8859-_1)"),
	 ENC_ACTION(C_ISO_8859_1)},
	{N_("/_View/Character _encoding/Western European (ISO-8859-15)"),
	 ENC_ACTION(C_ISO_8859_15)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Central European (ISO-8859-_2)"),
	 ENC_ACTION(C_ISO_8859_2)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/_Baltic (ISO-8859-13)"),
	 ENC_ACTION(C_ISO_8859_13)},
	{N_("/_View/Character _encoding/Baltic (ISO-8859-_4)"),
	 ENC_ACTION(C_ISO_8859_4)},
	{N_("/_View/Character _encoding/Baltic (Windows-1257)"),
	 ENC_ACTION(C_WINDOWS_1257)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Greek (ISO-8859-_7)"),
	 ENC_ACTION(C_ISO_8859_7)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Arabic (ISO-8859-_6)"),
	 ENC_ACTION(C_ISO_8859_6)},
	{N_("/_View/Character _encoding/Arabic (Windows-1256)"),
	 ENC_ACTION(C_WINDOWS_1256)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Hebrew (ISO-8859-_8)"),
	 ENC_ACTION(C_ISO_8859_8)},
	{N_("/_View/Character _encoding/Hebrew (Windows-1255)"),
	 ENC_ACTION(C_WINDOWS_1255)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Turkish (ISO-8859-_9)"),
	 ENC_ACTION(C_ISO_8859_9)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Cyrillic (ISO-8859-_5)"),
	 ENC_ACTION(C_ISO_8859_5)},
	{N_("/_View/Character _encoding/Cyrillic (KOI8-_R)"),
	 ENC_ACTION(C_KOI8_R)},
	{N_("/_View/Character _encoding/Cyrillic (KOI8-U)"),
	 ENC_ACTION(C_KOI8_U)},
	{N_("/_View/Character _encoding/Cyrillic (Windows-1251)"),
	 ENC_ACTION(C_WINDOWS_1251)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Japanese (ISO-2022-_JP)"),
	 ENC_ACTION(C_ISO_2022_JP)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Simplified Chinese (_GB2312)"),
	 ENC_ACTION(C_GB2312)},
	{N_("/_View/Character _encoding/Simplified Chinese (GBK)"),
	 ENC_ACTION(C_GBK)},
	{N_("/_View/Character _encoding/Traditional Chinese (_Big5)"),
	 ENC_ACTION(C_BIG5)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Korean (EUC-_KR)"),
	 ENC_ACTION(C_EUC_KR)},
	{N_("/_View/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_View/Character _encoding/Thai (TIS-620)"),
	 ENC_ACTION(C_TIS_620)},
	{N_("/_View/Character _encoding/Thai (Windows-874)"),
	 ENC_ACTION(C_WINDOWS_874)},

	{N_("/_Tools"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Address book"),	"<shift><control>A", compose_address_cb , 0, NULL},
	{N_("/_Tools/_Template"),	NULL, NULL, 0, "<Branch>"},
#ifndef G_OS_WIN32
	{N_("/_Tools/Actio_ns"),	NULL, NULL, 0, "<Branch>"},
#endif
	{N_("/_Tools/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Edit with e_xternal editor"),
					"<shift><control>X", compose_ext_editor_cb, 0, NULL},
	{N_("/_Tools/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Request _disposition notification"),   	NULL, compose_toggle_mdn_cb   , 0, "<ToggleItem>"},

#if USE_GPGME
	{N_("/_Tools/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/PGP Si_gn"),   	NULL, compose_toggle_sign_cb   , 0, "<ToggleItem>"},
	{N_("/_Tools/PGP _Encrypt"),	NULL, compose_toggle_encrypt_cb, 0, "<ToggleItem>"},
#endif /* USE_GPGME */

#if USE_GTKSPELL
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Check spell"),		NULL, compose_toggle_spell_cb, 0, "<ToggleItem>"},
	{N_("/_Tools/_Set spell language"),	NULL, NULL, 0, "<Branch>"},
#endif /* USE_GTKSPELL */

	{N_("/_Help"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_About"),		NULL, about_show, 0, NULL}
};

enum
{
	DRAG_TYPE_RFC822,
	DRAG_TYPE_URI_LIST,

	N_DRAG_TYPES
};

static GtkTargetEntry compose_drag_types[] =
{
	{"message/rfc822", GTK_TARGET_SAME_APP, DRAG_TYPE_RFC822},
	{"text/uri-list", 0, DRAG_TYPE_URI_LIST}
};


Compose *compose_new(PrefsAccount *account, FolderItem *item,
		     const gchar *mailto, GPtrArray *attach_files)
{
	Compose *compose;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	if (!account) account = cur_account;
	g_return_val_if_fail(account != NULL, NULL);

	compose = compose_create(account, COMPOSE_NEW);

	undo_block(compose->undostruct);

	if (prefs_common.auto_sig)
		compose_insert_sig(compose, TRUE, FALSE, FALSE);

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
			gchar *utf8file;

			file = g_ptr_array_index(attach_files, i);
			utf8file = conv_filename_to_utf8(file);
			compose_attach_append(compose, file, utf8file, NULL);
			g_free(utf8file);
		}
	}

	undo_unblock(compose->undostruct);

	compose_connect_changed_callbacks(compose);
	compose_set_title(compose);

	syl_plugin_signal_emit("compose-created", compose);

	if (prefs_common.enable_autosave && prefs_common.autosave_itv > 0)
		compose->autosave_tag =
			g_timeout_add_full(G_PRIORITY_LOW,
					   prefs_common.autosave_itv * 60 * 1000,
					   autosave_timeout, compose, NULL);
	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);

	return compose;
}

Compose *compose_reply(MsgInfo *msginfo, FolderItem *item, ComposeMode mode,
		       const gchar *body)
{
	Compose *compose;
	PrefsAccount *account = NULL;
	MsgInfo *replyinfo;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gboolean quote = FALSE;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if (COMPOSE_QUOTE_MODE(mode) == COMPOSE_WITH_QUOTE)
		quote = TRUE;

	if (msginfo->folder)
		account = account_find_from_item_property(msginfo->folder);
	if (!account && msginfo->to && prefs_common.reply_account_autosel) {
		gchar *to;
		to = g_strdup(msginfo->to);
		extract_address(to);
		account = account_find_from_address(to);
		g_free(to);
	}
	if (!account && msginfo->folder && msginfo->folder->folder)
		account = msginfo->folder->folder->account;
	if (!account)
		account = cur_account;
	g_return_val_if_fail(account != NULL, NULL);

	compose = compose_create(account, COMPOSE_REPLY);

	replyinfo = procmsg_msginfo_get_full_info(msginfo);
	if (!replyinfo)
		replyinfo = procmsg_msginfo_copy(msginfo);
	if (msginfo->folder) {
		gchar *id;
		id = folder_item_get_identifier(msginfo->folder);
		if (id) {
			compose->reply_target = g_strdup_printf
				("%s/%u", id, msginfo->msgnum);
			g_free(id);
		}
	}

	if (compose_parse_header(compose, msginfo) < 0)
		return compose;

	undo_block(compose->undostruct);

	compose_reply_set_entry(compose, msginfo, mode);
	if (item)
		compose_entries_set_from_item(compose, item, COMPOSE_REPLY);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(compose->text));

	if (account->sig_before_quote && prefs_common.auto_sig) {
		GtkTextMark *mark;
		compose_insert_sig(compose, TRUE, FALSE, FALSE);
		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

	if (quote) {
		compose_quote_fmt(compose, replyinfo,
				  prefs_common.quotefmt,
				  prefs_common.quotemark, body);
	}

	if (!account->sig_before_quote && prefs_common.auto_sig)
		compose_insert_sig(compose, TRUE, FALSE, FALSE);

	if (quote && prefs_common.linewrap_quote)
		compose_wrap_all(compose);

	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_place_cursor(buffer, &iter);

	gtk_widget_grab_focus(compose->text);

	undo_unblock(compose->undostruct);

	compose_connect_changed_callbacks(compose);
	compose_set_title(compose);

#if USE_GPGME
	if (rfc2015_is_available() && account->encrypt_reply &&
	    MSG_IS_ENCRYPTED(replyinfo->flags)) {
		GtkItemFactory *ifactory;

		ifactory = gtk_item_factory_from_widget(compose->menubar);
		menu_set_active(ifactory, "/Tools/PGP Encrypt", TRUE);
	}
#endif

	procmsg_msginfo_free(replyinfo);

	syl_plugin_signal_emit("compose-created", compose);

	if (prefs_common.enable_autosave && prefs_common.autosave_itv > 0)
		compose->autosave_tag =
			g_timeout_add_full(G_PRIORITY_LOW,
					   prefs_common.autosave_itv * 60 * 1000,
					   autosave_timeout, compose, NULL);
	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);

	return compose;
}

Compose *compose_forward(GSList *mlist, FolderItem *item, gboolean as_attach,
			 const gchar *body)
{
	Compose *compose;
	PrefsAccount *account = NULL;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GSList *cur;
	MsgInfo *msginfo;
	GString *forward_targets;

	g_return_val_if_fail(mlist != NULL, NULL);

	msginfo = (MsgInfo *)mlist->data;

	if (msginfo->folder)
		account = account_find_from_item(msginfo->folder);
	if (!account) account = cur_account;
	g_return_val_if_fail(account != NULL, NULL);

	compose = compose_create(account, COMPOSE_FORWARD);

	forward_targets = g_string_new("");
	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (msginfo->folder) {
			gchar *id;
			id = folder_item_get_identifier(msginfo->folder);
			if (id) {
				if (forward_targets->len > 0)
					g_string_append(forward_targets,
							"\n ");
				g_string_append_printf(forward_targets, "%s/%u",
						       id, msginfo->msgnum);
				g_free(id);
			}
		}
	}
	if (forward_targets->len > 0)
		compose->forward_targets = g_string_free(forward_targets,
							 FALSE);
	else
		g_string_free(forward_targets, TRUE);

	undo_block(compose->undostruct);

	compose_entry_set(compose, "Fw: ", COMPOSE_ENTRY_SUBJECT);
	if (mlist->next == NULL && msginfo->subject && *msginfo->subject)
		compose_entry_append(compose, msginfo->subject,
				     COMPOSE_ENTRY_SUBJECT);
	if (item)
		compose_entries_set_from_item(compose, item, COMPOSE_FORWARD);

	text = GTK_TEXT_VIEW(compose->text);
	buffer = gtk_text_view_get_buffer(text);

	if (account->sig_before_quote && prefs_common.auto_sig) {
		GtkTextMark *mark;
		compose_insert_sig(compose, TRUE, FALSE, FALSE);
		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

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

			compose_quote_fmt(compose, full_msginfo,
					  prefs_common.fw_quotefmt,
					  prefs_common.fw_quotemark, body);
			compose_attach_parts(compose, msginfo);

			procmsg_msginfo_free(full_msginfo);

			if (body) break;
		}
	}

	if (!account->sig_before_quote && prefs_common.auto_sig)
		compose_insert_sig(compose, TRUE, FALSE, FALSE);

	if (prefs_common.linewrap_quote)
		compose_wrap_all(compose);

	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_place_cursor(buffer, &iter);

	undo_unblock(compose->undostruct);

	compose_connect_changed_callbacks(compose);
	compose_set_title(compose);

	if (account->protocol != A_NNTP)
		gtk_widget_grab_focus(compose->to_entry);
	else
		gtk_widget_grab_focus(compose->newsgroups_entry);

	syl_plugin_signal_emit("compose-created", compose);

	if (prefs_common.enable_autosave && prefs_common.autosave_itv > 0)
		compose->autosave_tag =
			g_timeout_add_full(G_PRIORITY_LOW,
					   prefs_common.autosave_itv * 60 * 1000,
					   autosave_timeout, compose, NULL);
	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);

	return compose;
}

Compose *compose_redirect(MsgInfo *msginfo, FolderItem *item)
{
	Compose *compose;
	PrefsAccount *account;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	FILE *fp;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(msginfo != NULL, NULL);
	g_return_val_if_fail(msginfo->folder != NULL, NULL);

	account = account_find_from_item(msginfo->folder);
	if (!account) account = cur_account;
	g_return_val_if_fail(account != NULL, NULL);

	compose = compose_create(account, COMPOSE_REDIRECT);
	compose->targetinfo = procmsg_msginfo_copy(msginfo);

	if (compose_parse_header(compose, msginfo) < 0)
		return compose;

	undo_block(compose->undostruct);

	if (msginfo->subject)
		compose_entry_set(compose, msginfo->subject,
				  COMPOSE_ENTRY_SUBJECT);
	compose_entries_set_from_item(compose, item, COMPOSE_REDIRECT);

	text = GTK_TEXT_VIEW(compose->text);
	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	if ((fp = procmime_get_first_text_content(msginfo, NULL)) == NULL)
		g_warning(_("Can't get text part\n"));
	else {
		gboolean prev_autowrap = compose->autowrap;

		compose->autowrap = FALSE;
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			strcrchomp(buf);
			gtk_text_buffer_insert(buffer, &iter, buf, -1);
		}
		compose->autowrap = prev_autowrap;
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
	compose_set_title(compose);

	syl_plugin_signal_emit("compose-created", compose);

	return compose;
}

Compose *compose_reedit(MsgInfo *msginfo)
{
	Compose *compose;
	PrefsAccount *account;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	FILE *fp;
	gchar buf[BUFFSIZE];
	const gchar *str;
	GtkWidget *focus_widget = NULL;
	GtkItemFactory *ifactory;

	g_return_val_if_fail(msginfo != NULL, NULL);
	g_return_val_if_fail(msginfo->folder != NULL, NULL);

	account = account_find_from_msginfo(msginfo);
	if (!account) account = cur_account;
	g_return_val_if_fail(account != NULL, NULL);

	if (msginfo->folder->stype == F_DRAFT ||
	    msginfo->folder->stype == F_QUEUE) {
		compose = compose_find_window_by_target(msginfo);
		if (compose) {
			debug_print
				("compose_reedit(): existing window found.\n");
			gtk_window_present(GTK_WINDOW(compose->window));
			return compose;
		}
	}

	compose = compose_create(account, COMPOSE_REEDIT);
	compose->targetinfo = procmsg_msginfo_copy(msginfo);

	if (compose_parse_header(compose, msginfo) < 0)
		return compose;
	compose_parse_source_msg(compose, msginfo);

	undo_block(compose->undostruct);

	compose_reedit_set_entry(compose, msginfo);

	text = GTK_TEXT_VIEW(compose->text);
	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	if ((fp = procmime_get_first_text_content(msginfo, NULL)) == NULL)
		g_warning(_("Can't get text part\n"));
	else {
		gboolean prev_autowrap = compose->autowrap;

		compose->autowrap = FALSE;
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			strcrchomp(buf);
			gtk_text_buffer_insert(buffer, &iter, buf, -1);
		}
		compose_enable_sig(compose);
		compose->autowrap = prev_autowrap;
		fclose(fp);
	}
	compose_attach_parts(compose, msginfo);

	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_place_cursor(buffer, &iter);

	if (account->protocol != A_NNTP) {
		str = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
		if (!str || *str == '\0')
			focus_widget = compose->to_entry;
	} else {
		str = gtk_entry_get_text(GTK_ENTRY(compose->newsgroups_entry));
		if (!str || *str == '\0')
			focus_widget = compose->newsgroups_entry;
	}
	if (!focus_widget) {
		str = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
		if (!str || *str == '\0')
			focus_widget = compose->subject_entry;
	}
	if (focus_widget)
		gtk_widget_grab_focus(focus_widget);
	else
		gtk_widget_grab_focus(compose->text);

	undo_unblock(compose->undostruct);

	compose_connect_changed_callbacks(compose);
	compose_set_title(compose);

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	if (compose->use_mdn) {
		menu_set_active(ifactory, "/Tools/Request disposition notification", TRUE);
	}
	menu_set_active(ifactory, "/Edit/Auto wrapping", compose->autowrap);
#if USE_GTKSPELL
	menu_set_active(ifactory, "/Tools/Check spell", compose->check_spell);
	compose_change_spell_lang_menu(compose, compose->spell_lang);
#endif
#if USE_GPGME
	menu_set_active(ifactory, "/Tools/PGP Sign", compose->use_signing);
	menu_set_active(ifactory, "/Tools/PGP Encrypt", compose->use_encryption);
#endif

	syl_plugin_signal_emit("compose-created", compose);

	if (prefs_common.enable_autosave && prefs_common.autosave_itv > 0)
		compose->autosave_tag =
			g_timeout_add_full(G_PRIORITY_LOW,
					   prefs_common.autosave_itv * 60 * 1000,
					   autosave_timeout, compose, NULL);
	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);

	return compose;
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
		menu_set_active(ifactory, "/View/Reply-To", TRUE);
		break;
	case COMPOSE_ENTRY_FOLLOWUP_TO:
		menu_set_active(ifactory, "/View/Followup-To", TRUE);
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

gchar *compose_entry_get_text(Compose *compose, ComposeEntryType type)
{
	GtkEntry *entry;

	entry = compose_get_entry(compose, type);
	return gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
}

static void compose_entries_set(Compose *compose, const gchar *mailto)
{
	gchar *to = NULL;
	gchar *cc = NULL;
	gchar *subject = NULL;
	gchar *inreplyto = NULL;
	gchar *body = NULL;

	scan_mailto_url(mailto, &to, &cc, NULL, &subject, &inreplyto, &body);

	if (to)
		compose_entry_set(compose, to, COMPOSE_ENTRY_TO);
	if (cc)
		compose_entry_set(compose, cc, COMPOSE_ENTRY_CC);
	if (subject)
		compose_entry_set(compose, subject, COMPOSE_ENTRY_SUBJECT);
	if (inreplyto) {
		if (strchr(inreplyto, '<'))
			extract_parenthesis(inreplyto, '<', '>');
		remove_space(inreplyto);
		compose->inreplyto = g_strdup(inreplyto);
	}
	if (body) {
		GtkTextView *text = GTK_TEXT_VIEW(compose->text);
		GtkTextBuffer *buffer;
		GtkTextMark *mark;
		GtkTextIter iter;
		gboolean prev_autowrap = compose->autowrap;

		compose->autowrap = FALSE;

		buffer = gtk_text_view_get_buffer(text);
		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

		gtk_text_buffer_insert(buffer, &iter, body, -1);
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);

		compose->autowrap = prev_autowrap;
		if (compose->autowrap)
			compose_wrap_all(compose);
	}

	g_free(to);
	g_free(cc);
	g_free(subject);
	g_free(inreplyto);
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
				       {"Content-Type:",NULL, FALSE},
				       {NULL,		NULL, FALSE}};

	enum
	{
		H_REPLY_TO	= 0,
		H_CC		= 1,
		H_REFERENCES	= 2,
		H_BCC		= 3,
		H_NEWSGROUPS    = 4,
		H_FOLLOWUP_TO	= 5,
		H_LIST_POST     = 6,
		H_CONTENT_TYPE  = 7
	};

	FILE *fp;
	gchar *charset = NULL;

	g_return_val_if_fail(msginfo != NULL, -1);

	if ((fp = procmsg_open_message(msginfo)) == NULL) return -1;
	procheader_get_header_fields(fp, hentry);
	fclose(fp);

	if (hentry[H_CONTENT_TYPE].body != NULL) {
		procmime_scan_content_type_str(hentry[H_CONTENT_TYPE].body,
					       NULL, &charset, NULL, NULL);
		g_free(hentry[H_CONTENT_TYPE].body);
		hentry[H_CONTENT_TYPE].body = NULL;
	}
	if (hentry[H_REPLY_TO].body != NULL) {
		if (hentry[H_REPLY_TO].body[0] != '\0') {
			compose->replyto =
				conv_unmime_header(hentry[H_REPLY_TO].body,
						   charset);
		}
		g_free(hentry[H_REPLY_TO].body);
		hentry[H_REPLY_TO].body = NULL;
	}
	if (hentry[H_CC].body != NULL) {
		compose->cc = conv_unmime_header(hentry[H_CC].body, charset);
		g_free(hentry[H_CC].body);
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
		if (compose->mode == COMPOSE_REEDIT)
			compose->bcc =
				conv_unmime_header(hentry[H_BCC].body, charset);
		g_free(hentry[H_BCC].body);
		hentry[H_BCC].body = NULL;
	}
	if (hentry[H_NEWSGROUPS].body != NULL) {
		compose->newsgroups = hentry[H_NEWSGROUPS].body;
		hentry[H_NEWSGROUPS].body = NULL;
	}
	if (hentry[H_FOLLOWUP_TO].body != NULL) {
		if (hentry[H_FOLLOWUP_TO].body[0] != '\0') {
			compose->followup_to =
				conv_unmime_header(hentry[H_FOLLOWUP_TO].body,
						   charset);
		}
		g_free(hentry[H_FOLLOWUP_TO].body);
		hentry[H_FOLLOWUP_TO].body = NULL;
	}
	if (hentry[H_LIST_POST].body != NULL) {
		gchar *to = NULL;

		extract_address(hentry[H_LIST_POST].body);
		if (hentry[H_LIST_POST].body[0] != '\0') {
			scan_mailto_url(hentry[H_LIST_POST].body,
					&to, NULL, NULL, NULL, NULL, NULL);
			if (to) {
				g_free(compose->ml_post);
				compose->ml_post = to;
			}
		}
		g_free(hentry[H_LIST_POST].body);
		hentry[H_LIST_POST].body = NULL;
	}

	g_free(charset);

	if (compose->mode == COMPOSE_REEDIT) {
		if (msginfo->inreplyto && *msginfo->inreplyto)
			compose->inreplyto = g_strdup(msginfo->inreplyto);
		return 0;
	}

	if (msginfo->msgid && *msginfo->msgid)
		compose->inreplyto = g_strdup(msginfo->msgid);

	if (!compose->references) {
		if (msginfo->msgid && *msginfo->msgid) {
			if (msginfo->inreplyto && *msginfo->inreplyto)
				compose->references =
					g_strdup_printf("<%s>\n\t<%s>",
							msginfo->inreplyto,
							msginfo->msgid);
			else
				compose->references =
					g_strconcat("<", msginfo->msgid, ">",
						    NULL);
		} else if (msginfo->inreplyto && *msginfo->inreplyto) {
			compose->references =
				g_strconcat("<", msginfo->inreplyto, ">",
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

static gint compose_parse_source_msg(Compose *compose, MsgInfo *msginfo)
{
	static HeaderEntry hentry[] = {{"X-Sylpheed-Reply:", NULL, FALSE},
				       {"X-Sylpheed-Forward:", NULL, FALSE},
				       {"REP:", NULL, FALSE},
				       {"FWD:", NULL, FALSE},
				       {"Disposition-Notification-To:", NULL, FALSE},
				       {"X-Sylpheed-Compose-AutoWrap:", NULL, FALSE},
				       {"X-Sylpheed-Compose-CheckSpell:", NULL, FALSE},
				       {"X-Sylpheed-Compose-SpellLang:", NULL, FALSE},
				       {"X-Sylpheed-Compose-UseSigning:", NULL, FALSE},
				       {"X-Sylpheed-Compose-UseEncryption:", NULL, FALSE},
				       {NULL, NULL, FALSE}};

	enum
	{
		H_X_SYLPHEED_REPLY = 0,
		H_X_SYLPHEED_FORWARD = 1,
		H_REP = 2,
		H_FWD = 3,
		H_MDN = 4,
		H_X_SYLPHEED_COMPOSE_AUTOWRAP = 5,
		H_X_SYLPHEED_COMPOSE_CHECKSPELL = 6,
		H_X_SYLPHEED_COMPOSE_SPELLLANG = 7,
		H_X_SYLPHEED_COMPOSE_USESIGNING = 8,
		H_X_SYLPHEED_COMPOSE_USEENCRYPTION = 9
	};

	gchar *file;
	FILE *fp;
	gchar *str;
	gchar buf[BUFFSIZE];
	gint hnum;

	g_return_val_if_fail(msginfo != NULL, -1);

	file = procmsg_get_message_file(msginfo);

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	while ((hnum = procheader_get_one_field(buf, sizeof(buf), fp, hentry))
	       != -1) {
		str = buf + strlen(hentry[hnum].name);
		while (g_ascii_isspace(*str))
			++str;
		if ((hnum == H_X_SYLPHEED_REPLY || hnum == H_REP) &&
		    !compose->reply_target) {
			compose->reply_target = g_strdup(str);
		} else if ((hnum == H_X_SYLPHEED_FORWARD || hnum == H_FWD) &&
			   !compose->forward_targets) {
			compose->forward_targets = g_strdup(str);
		} else if (hnum == H_MDN) {
			compose->use_mdn = TRUE;
		} else if (hnum == H_X_SYLPHEED_COMPOSE_AUTOWRAP) {
			if (g_ascii_strcasecmp(str, "TRUE") == 0)
				compose->autowrap = TRUE;
			else
				compose->autowrap = FALSE;
#if USE_GTKSPELL
		} else if (hnum == H_X_SYLPHEED_COMPOSE_CHECKSPELL) {
			if (g_ascii_strcasecmp(str, "TRUE") == 0)
				compose->check_spell = TRUE;
			else
				compose->check_spell = FALSE;
		} else if (hnum == H_X_SYLPHEED_COMPOSE_SPELLLANG) {
			g_free(compose->spell_lang);
			compose->spell_lang = g_strdup(str);
#endif
#if USE_GPGME
		} else if (hnum == H_X_SYLPHEED_COMPOSE_USESIGNING) {
			if (g_ascii_strcasecmp(str, "TRUE") == 0)
				compose->use_signing = TRUE;
			else
				compose->use_signing = FALSE;
		} else if (hnum == H_X_SYLPHEED_COMPOSE_USEENCRYPTION) {
			if (g_ascii_strcasecmp(str, "TRUE") == 0)
				compose->use_encryption = TRUE;
			else
				compose->use_encryption = FALSE;
#endif
		}
	}

	fclose(fp);
	g_free(file);

	return 0;
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
	gboolean prev_autowrap;

	if (!msginfo)
		msginfo = &dummyinfo;

	if (qmark == NULL || *qmark == '\0')
		qmark = "> ";

	quote_fmt_init(msginfo, NULL, NULL);
	quote_fmt_scan_string(qmark);
	quote_fmt_parse();

	buf = quote_fmt_get_buffer();
	if (buf == NULL)
		alertpanel_error(_("Quote mark format error."));
	else
		quote_str = g_strdup(buf);

	if (fmt && *fmt != '\0') {
		quote_fmt_init(msginfo, quote_str, body);
		quote_fmt_scan_string(fmt);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL) {
			alertpanel_error(_("Message reply/forward format error."));
			g_free(quote_str);
			return NULL;
		}
	} else
		buf = "";

	g_free(quote_str);

	prev_autowrap = compose->autowrap;
	compose->autowrap = FALSE;

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

	compose->autowrap = prev_autowrap;
	if (compose->autowrap)
		compose_wrap_all(compose);

	return buf;
}

static void compose_reply_set_entry(Compose *compose, MsgInfo *msginfo,
				    ComposeMode mode)
{
	GSList *cc_list = NULL;
	GSList *cur;
	GHashTable *to_table;
	gboolean to_all = FALSE, to_ml = FALSE, ignore_replyto = FALSE;
	gchar *from_str = NULL, *to_str = NULL, *cc_str = NULL,
	      *replyto_str = NULL;
	gboolean address_only = prefs_common.reply_address_only;

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

	if (address_only) {
		from_str = extract_addresses(msginfo->from);
		to_str = extract_addresses(msginfo->to);
		cc_str = extract_addresses(compose->cc);
		replyto_str = extract_addresses(compose->replyto);
	} else {
		from_str = g_strdup(msginfo->from);
		to_str = g_strdup(msginfo->to);
		cc_str = g_strdup(compose->cc);
		replyto_str = g_strdup(compose->replyto);
	}

	if (compose->account->protocol != A_NNTP) {
		if (to_ml && compose->ml_post) {
			/* don't reply to list for confirmation request etc. */
			if ((!to_str ||
			     !strcasestr_with_skip_quote(to_str,
							 compose->ml_post)) &&
			    (!cc_str ||
			     !strcasestr_with_skip_quote(cc_str,
							 compose->ml_post)))
				to_ml = FALSE;
		}
		if (to_ml && compose->ml_post) {
			compose_entry_set(compose, compose->ml_post,
					  COMPOSE_ENTRY_TO);
			if (replyto_str &&
			    !address_equal(replyto_str, compose->ml_post))
				compose_entry_set(compose, replyto_str,
						  COMPOSE_ENTRY_CC);
		} else if (prefs_common.inherit_recipient_on_self_reply &&
			   address_equal(compose->account->address, from_str)) {
			compose_entry_set(compose, to_str, COMPOSE_ENTRY_TO);
			if (to_all) {
				compose_entry_set(compose, cc_str,
						  COMPOSE_ENTRY_CC);
				to_all = FALSE;
			}
		} else {
			compose_entry_set(compose, 
					  (replyto_str && !ignore_replyto)
					  ? replyto_str
					  : from_str ? from_str : "",
					  COMPOSE_ENTRY_TO);
		}
	} else {
		if (ignore_replyto) {
			compose_entry_set(compose, from_str ? from_str : "",
					  COMPOSE_ENTRY_TO);
		} else {
			if (to_all) {
				compose_entry_set
					(compose, 
					 (replyto_str && !ignore_replyto)
					 ? replyto_str
					 : from_str ? from_str : "",
					 COMPOSE_ENTRY_TO);
			}
			compose_entry_set(compose,
					  compose->followup_to ? compose->followup_to
					  : compose->newsgroups ? compose->newsgroups
					  : "",
					  COMPOSE_ENTRY_NEWSGROUPS);
		}
	}

	if (msginfo->subject && *msginfo->subject) {
		gchar *buf;
		gchar *p;

		buf = g_strdup(msginfo->subject);

		if (msginfo->folder && msginfo->folder->trim_compose_subject)
			trim_subject(buf);

		while (!g_ascii_strncasecmp(buf, "Re:", 3)) {
			p = buf + 3;
			while (g_ascii_isspace(*p)) p++;
			memmove(buf, p, strlen(p) + 1);
		}

		compose_entry_set(compose, "Re: ", COMPOSE_ENTRY_SUBJECT);
		compose_entry_append(compose, buf, COMPOSE_ENTRY_SUBJECT);

		g_free(buf);
	} else
		compose_entry_set(compose, "Re: ", COMPOSE_ENTRY_SUBJECT);

	if (!compose->replyto && to_ml && compose->ml_post)
		goto done;
	if (!to_all)
		goto done;

	if (replyto_str && from_str)
		cc_list = address_list_append_orig(cc_list, from_str);
	cc_list = address_list_append_orig(cc_list, to_str);
	cc_list = address_list_append_orig(cc_list, cc_str);

	to_table = g_hash_table_new(g_str_hash, g_str_equal);
	if (replyto_str) {
		gchar *replyto;

		replyto = g_strdup(replyto_str);
		extract_address(replyto);
		g_hash_table_insert(to_table, replyto, GINT_TO_POINTER(1));
	} else if (from_str) {
		gchar *from;

		from = g_strdup(from_str);
		extract_address(from);
		g_hash_table_insert(to_table, from, GINT_TO_POINTER(1));
	}
	if (compose->account->address)
		g_hash_table_insert(to_table,
				    g_strdup(compose->account->address),
				    GINT_TO_POINTER(1));

	/* remove duplicate addresses */
	for (cur = cc_list; cur != NULL; ) {
		gchar *addr = (gchar *)cur->data;
		GSList *next = cur->next;
		gchar *addr_;

		addr_ = g_strdup(addr);
		extract_address(addr_);
		if (g_hash_table_lookup(to_table, addr_) != NULL) {
			cc_list = g_slist_remove(cc_list, addr);
			g_free(addr_);
			g_free(addr);
		} else
			g_hash_table_insert(to_table, addr_, cur);

		cur = next;
	}
	hash_free_strings(to_table);
	g_hash_table_destroy(to_table);

	if (cc_list) {
		for (cur = cc_list; cur != NULL; cur = cur->next)
			compose_entry_append(compose, (gchar *)cur->data,
					     COMPOSE_ENTRY_CC);
		slist_free_strings(cc_list);
		g_slist_free(cc_list);
	}

done:
	g_free(from_str);
	g_free(to_str);
	g_free(cc_str);
	g_free(replyto_str);
}

static void compose_reedit_set_entry(Compose *compose, MsgInfo *msginfo)
{
	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(compose->account != NULL);

	compose_entry_set(compose, msginfo->to     , COMPOSE_ENTRY_TO);
	compose_entry_set(compose, compose->cc     , COMPOSE_ENTRY_CC);
	compose_entry_set(compose, compose->bcc    , COMPOSE_ENTRY_BCC);
	compose_entry_set(compose, compose->replyto, COMPOSE_ENTRY_REPLY_TO);
	if (compose->account->protocol == A_NNTP) {
		compose_entry_set(compose, compose->newsgroups,
				  COMPOSE_ENTRY_NEWSGROUPS);
		compose_entry_set(compose, compose->followup_to,
				  COMPOSE_ENTRY_FOLLOWUP_TO);
	}
	compose_entry_set(compose, msginfo->subject, COMPOSE_ENTRY_SUBJECT);
}

static void compose_insert_sig(Compose *compose, gboolean append,
			       gboolean replace, gboolean scroll)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	gchar *sig_str;
	gboolean prev_autowrap;

	debug_print("compose_insert_sig: append:%d replace:%d scroll:%d\n",
		    append, replace, scroll);

	g_return_if_fail(compose->account != NULL);

	prev_autowrap = compose->autowrap;
	compose->autowrap = FALSE;

	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	if (replace) {
		GtkTextIter start_iter, end_iter;

		gtk_text_buffer_get_start_iter(buffer, &start_iter);
		gtk_text_buffer_get_end_iter(buffer, &iter);

		while (gtk_text_iter_begins_tag
			(&start_iter, compose->sig_tag) ||
		       gtk_text_iter_forward_to_tag_toggle
			(&start_iter, compose->sig_tag)) {
			end_iter = start_iter;
			if (gtk_text_iter_forward_to_tag_toggle
				(&end_iter, compose->sig_tag)) {
				gtk_text_buffer_delete
					(buffer, &start_iter, &end_iter);
				iter = start_iter;
			}
		}
	}

	if (scroll) {
		if (append)
			gtk_text_buffer_get_end_iter(buffer, &iter);
		else
			gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	}

	sig_str = compose_get_signature_str(compose);
	if (sig_str) {
		if (!replace)
			gtk_text_buffer_insert(buffer, &iter, "\n\n", 2);
		else if (!gtk_text_iter_starts_line(&iter))
			gtk_text_buffer_insert(buffer, &iter, "\n", 1);
		gtk_text_buffer_insert_with_tags
			(buffer, &iter, sig_str, -1, compose->sig_tag, NULL);
		g_free(sig_str);
		if (scroll)
			gtk_text_buffer_place_cursor(buffer, &iter);
	}

	compose->autowrap = prev_autowrap;
	if (compose->autowrap)
		compose_wrap_all(compose);

	if (scroll)
		gtk_text_view_scroll_mark_onscreen(text, mark);
}

static void compose_enable_sig(Compose *compose)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter, start, end;
	gchar *sig_str;

	g_return_if_fail(compose->account != NULL);

	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_start_iter(buffer, &iter);

	sig_str = compose_get_signature_str(compose);
	if (!sig_str)
		return;

	if (gtkut_text_buffer_find(buffer, &iter, sig_str, TRUE, &start)) {
		end = start;
		gtk_text_iter_forward_chars(&end, g_utf8_strlen(sig_str, -1));
		gtk_text_buffer_apply_tag(buffer, compose->sig_tag,
					  &start, &end);
	}

	g_free(sig_str);
}

static gchar *compose_get_signature_str(Compose *compose)
{
	gchar *sig_path;
	gchar *sig_body = NULL;
	gchar *utf8_sig_body = NULL;
	gchar *utf8_sig_str = NULL;

	g_return_val_if_fail(compose->account != NULL, NULL);

	if (compose->account->sig_type == SIG_DIRECT) {
		gchar *sig_text;
		gchar *p, *sp;
		gint sig_index;

		sig_index = gtk_combo_box_get_active(GTK_COMBO_BOX(compose->sig_combo));
		if (sig_index > 0)
			sig_text = compose->account->sig_texts[sig_index];
		else
			sig_text = compose->account->sig_text;
		if (!sig_text)
			return NULL;

		sp = sig_text;
		p = sig_body = g_malloc(strlen(sig_text) + 1);
		while (*sp) {
			if (*sp == '\\' && *(sp + 1) == 'n') {
				*p++ = '\n';
				sp += 2;
			} else
				*p++ = *sp++;
		}
		*p = '\0';

		if (prefs_common.sig_sep) {
			utf8_sig_str = g_strconcat(prefs_common.sig_sep, "\n",
						   sig_body, NULL);
			g_free(sig_body);
		} else
			utf8_sig_str = sig_body;

		return utf8_sig_str;
	}

	if (!compose->account->sig_path)
		return NULL;

	if (g_path_is_absolute(compose->account->sig_path) ||
	    compose->account->sig_type == SIG_COMMAND)
		sig_path = g_strdup(compose->account->sig_path);
	else {
#ifdef G_OS_WIN32
		sig_path = g_strconcat(get_rc_dir(),
#else
		sig_path = g_strconcat(get_home_dir(),
#endif
				       G_DIR_SEPARATOR_S,
				       compose->account->sig_path, NULL);
	}

	if (compose->account->sig_type == SIG_FILE) {
		if (!is_file_or_fifo_exist(sig_path)) {
			debug_print("can't open signature file: %s\n",
				    sig_path);
			g_free(sig_path);
			return NULL;
		}
	}

	if (compose->account->sig_type == SIG_COMMAND)
		sig_body = get_command_output(sig_path);
	else {
		gchar *tmp;

		tmp = file_read_to_str(sig_path);
		if (!tmp)
			return NULL;
		sig_body = normalize_newlines(tmp);
		g_free(tmp);
	}
	g_free(sig_path);

	if (sig_body) {
		gint error = 0;

		utf8_sig_body = conv_codeset_strdup_full
			(sig_body, conv_get_locale_charset_str(),
			 CS_INTERNAL, &error);
		if (!utf8_sig_body || error != 0) {
			if (g_utf8_validate(sig_body, -1, NULL) == TRUE) {
				g_free(utf8_sig_body);
				utf8_sig_body = conv_utf8todisp(sig_body, NULL);
			}
		} else {
			g_free(sig_body);
			sig_body = utf8_sig_body;
			utf8_sig_body = conv_utf8todisp(sig_body, NULL);
		}
		g_free(sig_body);
	}

	if (prefs_common.sig_sep) {
		utf8_sig_str = g_strconcat(prefs_common.sig_sep, "\n",
					   utf8_sig_body, NULL);
		g_free(utf8_sig_body);
	} else
		utf8_sig_str = utf8_sig_body;

	return utf8_sig_str;
}

static void compose_insert_file(Compose *compose, const gchar *file,
				gboolean scroll)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	const gchar *cur_encoding;
	gchar buf[BUFFSIZE];
	gint len;
	FILE *fp;
	gboolean prev_autowrap;
	CharSet enc;
	gchar *tmp_file = NULL;

	g_return_if_fail(file != NULL);

	enc = conv_check_file_encoding(file);
	if (enc == C_UTF_16 || enc == C_UTF_16BE || enc == C_UTF_16LE) {
		tmp_file = get_tmp_file();
		if (conv_copy_file(file, tmp_file, conv_get_charset_str(enc)) < 0) {
			g_warning("compose_insert_file: Cannot convert UTF-16 file %s to UTF-8\n", file);
			g_free(tmp_file);
			tmp_file = NULL;
		}
	}

	if (tmp_file) {
		if ((fp = g_fopen(tmp_file, "rb")) == NULL) {
			FILE_OP_ERROR(tmp_file, "fopen");
			g_unlink(tmp_file);
			g_free(tmp_file);
			return;
		}
	} else {
		if ((fp = g_fopen(file, "rb")) == NULL) {
			FILE_OP_ERROR(file, "fopen");
			return;
		}
	}

	prev_autowrap = compose->autowrap;
	compose->autowrap = FALSE;

	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	cur_encoding = conv_get_locale_charset_str();

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *str;
		gint error = 0;

		if (enc == C_UTF_8) {
			str = conv_utf8todisp(buf, NULL);
		} else {
			str = conv_codeset_strdup_full(buf, cur_encoding,
						       CS_INTERNAL, &error);
			if (!str || error != 0) {
				if (g_utf8_validate(buf, -1, NULL) == TRUE) {
					g_free(str);
					str = g_strdup(buf);
				}
			}
			if (!str) continue;
		}

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
	if (tmp_file) {
		g_unlink(tmp_file);
		g_free(tmp_file);
	}

	compose->autowrap = prev_autowrap;
	if (compose->autowrap)
		compose_wrap_all(compose);

	if (scroll)
		gtk_text_view_scroll_mark_onscreen(text, mark);
}

void compose_attach_append(Compose *compose, const gchar *file,
			   const gchar *filename,
			   const gchar *content_type)
{
	GtkTreeIter iter;
	AttachInfo *ainfo;
	FILE *fp;
	off_t size;

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
		manage_window_focus_in(compose->window, NULL, NULL);
		alertpanel_notice(_("File %s is empty."), file);
		return;
	}
	if ((fp = g_fopen(file, "rb")) == NULL) {
		manage_window_focus_in(compose->window, NULL, NULL);
		alertpanel_error(_("Can't read %s."), file);
		return;
	}
	fclose(fp);

	if (!compose->use_attach) {
		GtkItemFactory *ifactory;

		ifactory = gtk_item_factory_from_widget(compose->menubar);
		menu_set_active(ifactory, "/View/Attachment", TRUE);
	}

	ainfo = g_new0(AttachInfo, 1);
	ainfo->file = g_strdup(file);

	if (content_type) {
		ainfo->content_type = g_strdup(content_type);
		if (!g_ascii_strcasecmp(content_type, "message/rfc822")) {
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
			if (!g_ascii_strncasecmp(content_type, "text", 4))
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
		} else if (!g_ascii_strncasecmp(ainfo->content_type, "text", 4))
			ainfo->encoding =
				procmime_get_encoding_for_text_file(file);
		else
			ainfo->encoding = ENC_BASE64;
		ainfo->name = g_strdup(g_basename(filename ? filename : file));	
	}
	ainfo->size = size;

	gtk_list_store_append(compose->attach_store, &iter);
	gtk_list_store_set(compose->attach_store, &iter,
			   COL_MIMETYPE, ainfo->content_type,
			   COL_SIZE, to_human_readable(ainfo->size),
			   COL_NAME, ainfo->name,
			   COL_ATTACH_INFO, ainfo,
			   -1);

	syl_plugin_signal_emit("compose-attach-changed", compose);
}

static void compose_attach_parts(Compose *compose, MsgInfo *msginfo)
{
	MimeInfo *mimeinfo;
	MimeInfo *child;
	gchar *infile;
	gchar *outfile;

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) return;

	infile = procmsg_get_message_file_path(msginfo);

	child = mimeinfo;
	while (child != NULL) {
		if (child->children || child->mime_type == MIME_MULTIPART)
			goto next;
		if (child->mime_type != MIME_MESSAGE_RFC822 &&
		    child->mime_type != MIME_IMAGE &&
		    child->mime_type != MIME_AUDIO &&
		    child->mime_type != MIME_VIDEO &&
		    !child->filename && !child->name)
			goto next;

		outfile = procmime_get_tmp_file_name(child);
		if (procmime_get_part(outfile, infile, child) < 0) {
			g_warning(_("Can't get the part of multipart message."));
			g_free(outfile);
			goto next;
		}

		compose_attach_append
			(compose, outfile,
			 child->filename ? child->filename : child->name,
			 child->content_type);

		g_free(outfile);

next:
		if (child->mime_type == MIME_MESSAGE_RFC822)
			child = child->next;
		else
			child = procmime_mimeinfo_next(child);
	}

	g_free(infile);
	procmime_mimeinfo_free_all(mimeinfo);
}

#define INDENT_CHARS	">|#"

typedef enum {
	WAIT_FOR_INDENT_CHAR,
	WAIT_FOR_INDENT_CHAR_OR_SPACE,
} IndentState;

/* return indent length, we allow:
   indent characters followed by indent characters or spaces/tabs,
   alphabets and numbers immediately followed by indent characters,
   and the repeating sequences of the above
   If quote ends with multiple spaces, only the first one is included. */
static gchar *compose_get_quote_str(GtkTextBuffer *buffer,
				    const GtkTextIter *start, gint *len)
{
	GtkTextIter iter = *start;
	gunichar wc;
	gchar ch[6];
	gint clen;
	IndentState state = WAIT_FOR_INDENT_CHAR;
	gboolean is_space;
	gboolean is_indent;
	gint alnum_count = 0;
	gint space_count = 0;
	gint quote_len = 0;

	while (!gtk_text_iter_ends_line(&iter)) {
		wc = gtk_text_iter_get_char(&iter);
		if (g_unichar_iswide(wc))
			break;
		clen = g_unichar_to_utf8(wc, ch);
		if (clen != 1)
			break;

		is_indent = strchr(INDENT_CHARS, ch[0]) ? TRUE : FALSE;
		is_space = g_unichar_isspace(wc);

		if (state == WAIT_FOR_INDENT_CHAR) {
			if (!is_indent && !g_unichar_isalnum(wc))
				break;
			if (is_indent) {
				quote_len += alnum_count + space_count + 1;
				alnum_count = space_count = 0;
				state = WAIT_FOR_INDENT_CHAR_OR_SPACE;
			} else
				alnum_count++;
		} else if (state == WAIT_FOR_INDENT_CHAR_OR_SPACE) {
			if (!is_indent && !is_space && !g_unichar_isalnum(wc))
				break;
			if (is_space)
				space_count++;
			else if (is_indent) {
				quote_len += alnum_count + space_count + 1;
				alnum_count = space_count = 0;
			} else {
				alnum_count++;
				state = WAIT_FOR_INDENT_CHAR;
			}
		}

		gtk_text_iter_forward_char(&iter);
	}

	if (quote_len > 0 && space_count > 0)
		quote_len++;

	if (len)
		*len = quote_len;

	if (quote_len > 0) {
		iter = *start;
		gtk_text_iter_forward_chars(&iter, quote_len);
		return gtk_text_buffer_get_text(buffer, start, &iter, FALSE);
	}

	return NULL;
}

/* return TRUE if the line is itemized */
static gboolean compose_is_itemized(GtkTextBuffer *buffer,
				    const GtkTextIter *start)
{
	GtkTextIter iter = *start;
	gunichar wc;
	gchar ch[6];
	gint clen;

	if (gtk_text_iter_ends_line(&iter))
		return FALSE;

	while (1) {
		wc = gtk_text_iter_get_char(&iter);
		if (!g_unichar_isspace(wc))
			break;
		gtk_text_iter_forward_char(&iter);
		if (gtk_text_iter_ends_line(&iter))
			return FALSE;
	}

	clen = g_unichar_to_utf8(wc, ch);

	/* (1), 2), 3. etc. */
	if ((clen == 1 && ch[0] == '(') || g_unichar_isdigit(wc)) {
		gboolean digit_appeared = FALSE;

		if (ch[0] == '(')
			gtk_text_iter_forward_char(&iter);

		while (1) {
			wc = gtk_text_iter_get_char(&iter);
			clen = g_unichar_to_utf8(wc, ch);
			if (g_unichar_isdigit(wc)) {
				digit_appeared = TRUE;
				gtk_text_iter_forward_char(&iter);
				if (gtk_text_iter_ends_line(&iter))
					return FALSE;
			} else if (clen == 1 &&
				   (ch[0] == ')' || ch[0] == '.')) {
				if (!digit_appeared)
					return FALSE;
				gtk_text_iter_forward_char(&iter);
				if (gtk_text_iter_ends_line(&iter))
					return TRUE;
				wc = gtk_text_iter_get_char(&iter);
				if (g_unichar_isspace(wc))
					return TRUE;
				else
					return FALSE;
			} else
				return FALSE;
		}
	}

	if (clen != 1)
		return FALSE;
	if (!strchr("*-+", ch[0]))
		return FALSE;

	gtk_text_iter_forward_char(&iter);
	if (gtk_text_iter_ends_line(&iter))
		return FALSE;
	wc = gtk_text_iter_get_char(&iter);
	if (g_unichar_isspace(wc))
		return TRUE;
	else if (ch[0] == '-') {
		/* -- */
		clen = g_unichar_to_utf8(wc, ch);
		if (clen == 1 && ch[0] == '-')
			return TRUE;
	}

	return FALSE;
}

static gboolean compose_get_line_break_pos(GtkTextBuffer *buffer,
					   const GtkTextIter *start,
					   GtkTextIter *break_pos,
					   gint max_col,
					   gint quote_len)
{
	GtkTextIter iter = *start, line_end = *start;
	PangoLogAttr *attrs;
	gchar *str;
	gchar *p;
	gint len;
	gint i;
	gint col = 0;
	gint pos = 0;
	gboolean can_break = FALSE;
	gboolean do_break = FALSE;
	gboolean prev_dont_break = FALSE;

	gtk_text_iter_forward_to_line_end(&line_end);
	str = gtk_text_buffer_get_text(buffer, &iter, &line_end, FALSE);
	len = g_utf8_strlen(str, -1);
	/* g_print("breaking line: %d: %s (len = %d)\n",
		gtk_text_iter_get_line(&iter), str, len); */
	attrs = g_new(PangoLogAttr, len + 1);

	pango_default_break(str, -1, NULL, attrs, len + 1);

	p = str;

	/* skip quote and leading spaces */
	for (i = 0; *p != '\0' && i < len; i++) {
		gunichar wc;

		wc = g_utf8_get_char(p);
		if (i >= quote_len && !g_unichar_isspace(wc))
			break;
		if (g_unichar_iswide(wc))
			col += 2;
		else if (*p == '\t')
			col += 8;
		else
			col++;
		p = g_utf8_next_char(p);
	}

	for (; *p != '\0' && i < len; i++) {
		PangoLogAttr *attr = attrs + i;
		gunichar wc;
		gint uri_len;

		if (attr->is_line_break && can_break && !prev_dont_break)
			pos = i;

		/* don't wrap URI */
		if ((uri_len = get_uri_len(p)) > 0) {
			col += uri_len;
			if (pos > 0 && col > max_col) {
				do_break = TRUE;
				break;
			}
			i += uri_len - 1;
			p += uri_len;
			can_break = TRUE;
			continue;
		}

		wc = g_utf8_get_char(p);
		if (g_unichar_iswide(wc)) {
			col += 2;
			if (prev_dont_break && can_break && attr->is_line_break)
				pos = i;
		} else if (*p == '\t')
			col += 8;
		else
			col++;
		if (pos > 0 && col > max_col) {
			do_break = TRUE;
			break;
		}

		if (*p == '-' || *p == '/')
			prev_dont_break = TRUE;
		else
			prev_dont_break = FALSE;

		p = g_utf8_next_char(p);
		can_break = TRUE;
	}

	debug_print("compose_get_line_break_pos(): do_break = %d, pos = %d, col = %d\n", do_break, pos, col);

	g_free(attrs);
	g_free(str);

	*break_pos = *start;
	gtk_text_iter_set_line_offset(break_pos, pos);

	return do_break;
}

static gboolean compose_join_next_line(GtkTextBuffer *buffer,
				       GtkTextIter *iter,
				       const gchar *quote_str)
{
	GtkTextIter iter_ = *iter, cur, prev, next, end;
	PangoLogAttr attrs[3];
	gchar *str;
	gchar *next_quote_str;
	gunichar wc1, wc2;
	gint quote_len;
	gboolean keep_cursor = FALSE;

	if (!gtk_text_iter_forward_line(&iter_) ||
	    gtk_text_iter_ends_line(&iter_))
		return FALSE;

	next_quote_str = compose_get_quote_str(buffer, &iter_, &quote_len);

	if ((quote_str || next_quote_str) &&
	    strcmp2(quote_str, next_quote_str) != 0) {
		g_free(next_quote_str);
		return FALSE;
	}
	g_free(next_quote_str);

	end = iter_;
	if (quote_len > 0) {
		gtk_text_iter_forward_chars(&end, quote_len);
		if (gtk_text_iter_ends_line(&end))
			return FALSE;
	}

	/* don't join itemized lines */
	if (compose_is_itemized(buffer, &end))
		return FALSE;

	/* delete quote str */
	if (quote_len > 0)
		gtk_text_buffer_delete(buffer, &iter_, &end);

	/* delete linebreak and extra spaces */
	prev = cur = iter_;
	while (gtk_text_iter_backward_char(&cur)) {
		wc1 = gtk_text_iter_get_char(&cur);
		if (!g_unichar_isspace(wc1))
			break;
		prev = cur;
	}
	next = cur = iter_;
	while (!gtk_text_iter_ends_line(&cur)) {
		wc1 = gtk_text_iter_get_char(&cur);
		if (!g_unichar_isspace(wc1))
			break;
		gtk_text_iter_forward_char(&cur);
		next = cur;
	}
	if (!gtk_text_iter_equal(&prev, &next)) {
		GtkTextMark *mark;

		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &cur, mark);
		if (gtk_text_iter_equal(&prev, &cur))
			keep_cursor = TRUE;
		gtk_text_buffer_delete(buffer, &prev, &next);
	}
	iter_ = prev;

	/* insert space if required */
	gtk_text_iter_backward_char(&prev);
	wc1 = gtk_text_iter_get_char(&prev);
	wc2 = gtk_text_iter_get_char(&next);
	gtk_text_iter_forward_char(&next);
	str = gtk_text_buffer_get_text(buffer, &prev, &next, FALSE);
	pango_default_break(str, -1, NULL, attrs, 3);
	if (!attrs[1].is_line_break ||
	    (!g_unichar_iswide(wc1) || !g_unichar_iswide(wc2))) {
		gtk_text_buffer_insert(buffer, &iter_, " ", 1);
		if (keep_cursor) {
			gtk_text_iter_backward_char(&iter_);
			gtk_text_buffer_place_cursor(buffer, &iter_);
		}
	}
	g_free(str);

	*iter = iter_;
	return TRUE;
}

static void compose_wrap_paragraph(Compose *compose, GtkTextIter *par_iter)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter, break_pos;
	gchar *quote_str = NULL;
	gint quote_len;
	gboolean wrap_quote = prefs_common.linewrap_quote;
	gboolean prev_autowrap = compose->autowrap;

	compose->autowrap = FALSE;

	buffer = gtk_text_view_get_buffer(text);

	if (par_iter) {
		iter = *par_iter;
	} else {
		GtkTextMark *mark;
		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	}

	/* move to paragraph start */
	gtk_text_iter_set_line_offset(&iter, 0);
	if (gtk_text_iter_ends_line(&iter)) {
		while (gtk_text_iter_ends_line(&iter) &&
		       gtk_text_iter_forward_line(&iter))
			;
	} else {
		while (gtk_text_iter_backward_line(&iter)) {
			if (gtk_text_iter_ends_line(&iter)) {
				gtk_text_iter_forward_line(&iter);
				break;
			}
		}
	}

	/* go until paragraph end (empty line) */
	while (!gtk_text_iter_ends_line(&iter)) {
		quote_str = compose_get_quote_str(buffer, &iter, &quote_len);
		if (quote_str) {
			if (!wrap_quote) {
				gtk_text_iter_forward_line(&iter);
				g_free(quote_str);
				continue;
			}
			debug_print("compose_wrap_paragraph(): quote_str = '%s'\n", quote_str);
		}

		if (compose_get_line_break_pos(buffer, &iter, &break_pos,
					       prefs_common.linewrap_len,
					       quote_len)) {
			GtkTextIter prev, next, cur;

			gtk_text_buffer_insert(buffer, &break_pos, "\n", 1);

			/* remove trailing spaces */
			cur = break_pos;
			gtk_text_iter_backward_char(&cur);
			prev = next = cur;
			while (!gtk_text_iter_starts_line(&cur)) {
				gunichar wc;

				gtk_text_iter_backward_char(&cur);
				wc = gtk_text_iter_get_char(&cur);
				if (!g_unichar_isspace(wc))
					break;
				prev = cur;
			}
			if (!gtk_text_iter_equal(&prev, &next)) {
				gtk_text_buffer_delete(buffer, &prev, &next);
				break_pos = next;
				gtk_text_iter_forward_char(&break_pos);
			}

			if (quote_str)
				gtk_text_buffer_insert(buffer, &break_pos,
						       quote_str, -1);

			iter = break_pos;
			compose_join_next_line(buffer, &iter, quote_str);

			/* move iter to current line start */
			gtk_text_iter_set_line_offset(&iter, 0);
		} else {
			/* move iter to next line start */
			iter = break_pos;
			gtk_text_iter_forward_line(&iter);
		}

		g_free(quote_str);
	}

	if (par_iter)
		*par_iter = iter;

	compose->autowrap = prev_autowrap;
}

static void compose_wrap_all(Compose *compose)
{
	compose_wrap_all_full(compose, FALSE);
}

static void compose_wrap_all_full(Compose *compose, gboolean autowrap)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_view_get_buffer(text);

	gtk_text_buffer_get_start_iter(buffer, &iter);
	while (!gtk_text_iter_is_end(&iter))
		compose_wrap_paragraph(compose, &iter);
}

static void compose_set_title(Compose *compose)
{
	gchar *str;
	gchar *edited;
	const gchar *subject;

	subject = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	if (!subject || subject[0] == '\0')
		subject = _("(No Subject)");

	edited = compose->modified ? " *" : "";
	str = g_strdup_printf(_("%s - Compose%s"), subject, edited);
	gtk_window_set_title(GTK_WINDOW(compose->window), str);
	g_free(str);
}

static void compose_select_account(Compose *compose, PrefsAccount *account,
				   gboolean init)
{
	GtkItemFactory *ifactory;
	PrefsAccount *prev_account;

	g_return_if_fail(account != NULL);

	prev_account = compose->account;
	compose->account = account;

	compose_set_title(compose);

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	if (account->protocol == A_NNTP &&
	    (init || prev_account->protocol != A_NNTP)) {
		gtk_widget_show(compose->newsgroups_hbox);
		gtk_widget_show(compose->newsgroups_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 2, 4);
		compose->use_newsgroups = TRUE;

		menu_set_active(ifactory, "/View/To", FALSE);
		menu_set_sensitive(ifactory, "/View/To", TRUE);
		menu_set_active(ifactory, "/View/Cc", FALSE);
		menu_set_sensitive(ifactory, "/View/Followup-To", TRUE);
	} else if (account->protocol != A_NNTP &&
		   (init || prev_account->protocol == A_NNTP)) {
		gtk_widget_hide(compose->newsgroups_hbox);
		gtk_widget_hide(compose->newsgroups_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 2, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_newsgroups = FALSE;

		menu_set_active(ifactory, "/View/To", TRUE);
		menu_set_sensitive(ifactory, "/View/To", FALSE);
		menu_set_active(ifactory, "/View/Cc", TRUE);
		menu_set_active(ifactory, "/View/Followup-To", FALSE);
		menu_set_sensitive(ifactory, "/View/Followup-To", FALSE);
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
	if (rfc2015_is_available()) {
		if (account->default_sign)
			menu_set_active(ifactory, "/Tools/PGP Sign", TRUE);
		if (account->default_encrypt)
			menu_set_active(ifactory, "/Tools/PGP Encrypt", TRUE);
	}
#endif /* USE_GPGME */

	compose_update_signature_menu(compose);

	if (!init && compose->mode != COMPOSE_REDIRECT && prefs_common.auto_sig)
		compose_insert_sig(compose, TRUE, TRUE, FALSE);
}

static void compose_update_signature_menu(Compose *compose)
{
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreeIter iter;
	gboolean valid;
	gint i;
	gchar *name;

	if (!compose->account)
		return;

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(compose->sig_combo));
	store = GTK_LIST_STORE(model);
	valid = gtk_tree_model_get_iter_first(model, &iter);

	for (i = 0; valid && i < sizeof(compose->account->sig_names) /
		sizeof(compose->account->sig_names[0]); i++) {
		if (compose->account->sig_names[i] &&
		    compose->account->sig_names[i][0] != '\0') {
			name = g_strdup_printf
				("%s", compose->account->sig_names[i]);
		} else {
			name = g_strdup_printf(_("Signature %d"), i + 1);
		}
		gtk_list_store_set(store, &iter, 0, name, -1);
		g_free(name);
		valid = gtk_tree_model_iter_next(model, &iter);
	}

	g_signal_handlers_block_by_func(G_OBJECT(compose->sig_combo),
					G_CALLBACK(sig_combo_changed), compose);
	gtk_combo_box_set_active(GTK_COMBO_BOX(compose->sig_combo), 0);
	g_signal_handlers_unblock_by_func(G_OBJECT(compose->sig_combo),
					  G_CALLBACK(sig_combo_changed),
					  compose);

	if (compose->account->sig_type != SIG_DIRECT) {
		gtk_widget_set_sensitive(compose->sig_combo, FALSE);
	} else {
		gtk_widget_set_sensitive(compose->sig_combo, TRUE);
	}
}

static gboolean compose_check_for_valid_recipient(Compose *compose)
{
	const gchar *to_raw = "", *cc_raw = "", *bcc_raw = "";
	const gchar *newsgroups_raw = "";
	gchar *to, *cc, *bcc;
	gchar *newsgroups;
	gboolean valid;

	if (compose->use_to)
		to_raw = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
	if (compose->use_cc)
		cc_raw = gtk_entry_get_text(GTK_ENTRY(compose->cc_entry));
	if (compose->use_bcc)
		bcc_raw = gtk_entry_get_text(GTK_ENTRY(compose->bcc_entry));
	if (compose->use_newsgroups)
		newsgroups_raw = gtk_entry_get_text
			(GTK_ENTRY(compose->newsgroups_entry));

	if (*to_raw == '\0' && *cc_raw == '\0' && *bcc_raw == '\0' &&
	    *newsgroups_raw == '\0')
		return FALSE;

	to = g_strstrip(g_strdup(to_raw));
	cc = g_strstrip(g_strdup(cc_raw));
	bcc = g_strstrip(g_strdup(bcc_raw));
	newsgroups = g_strstrip(g_strdup(newsgroups_raw));

	if (*to == '\0' && *cc == '\0' && *bcc == '\0' && *newsgroups == '\0')
		valid = FALSE;
	else
		valid = TRUE;

	g_free(newsgroups);
	g_free(bcc);
	g_free(cc);
	g_free(to);

	return valid;
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

		aval = alertpanel(_("Empty subject"),
				  _("Subject is empty. Send it anyway?"),
				  GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (aval != G_ALERTDEFAULT)
			return FALSE;
	}

	return TRUE;
}

static gboolean compose_check_attachments(Compose *compose)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter, line_end;
	gchar *line;
	gchar **strv;
	gint i;
	gboolean attach_found = FALSE;
	gboolean valid = TRUE;

	if (!prefs_common.check_attach)
		return TRUE;
	if (!prefs_common.check_attach_str)
		return TRUE;

	if (compose->use_attach &&
	    gtk_tree_model_iter_n_children
		(GTK_TREE_MODEL(compose->attach_store), NULL) > 0)
		return TRUE;

	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_start_iter(buffer, &iter);
	line_end = iter;

	strv = g_strsplit(prefs_common.check_attach_str, ",", -1);
	for (i = 0; strv[i] != NULL; i++)
		g_strstrip(strv[i]);

	while (valid) {
		valid = gtk_text_iter_forward_to_line_end(&line_end);
		line = gtk_text_buffer_get_text(buffer, &iter, &line_end,
						FALSE);
		iter = line_end;
		if (get_quote_level(line) != -1)
			continue;

		for (i = 0; strv[i] != NULL; i++) {
			if (strv[i][0] == '\0')
				continue;
			if (strcasestr(line, strv[i])) {
				attach_found = TRUE;
				valid = FALSE;
				break;
			}
		}

		g_free(line);
	}

	g_strfreev(strv);

	if (attach_found) {
		AlertValue aval;

		aval = alertpanel(_("Attachment is missing"),
				  _("There is no attachment. Send it without attachments?"),
				  GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (aval != G_ALERTDEFAULT)
			return FALSE;
	}

	return TRUE;
}

static gint check_recp_delete_event(GtkWidget *widget, GdkEventAny *event,
				    gint *state)
{
	*state = GTK_RESPONSE_CANCEL;
	return TRUE;
}

static gboolean check_recp_key_pressed(GtkWidget *widget, GdkEventKey *event,
				       gint *state)
{
	if (event && event->keyval == GDK_Escape) {
		*state = GTK_RESPONSE_CANCEL;
		return TRUE;
	}
	return FALSE;
}

static void check_recp_ok(GtkWidget *widget, gint *state)
{
	*state = GTK_RESPONSE_OK;
}

static void check_recp_cancel(GtkWidget *widget, gint *state)
{
	*state = GTK_RESPONSE_CANCEL;
}

static gboolean compose_check_recipients(Compose *compose)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *vbox2;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *entry;
	gchar buf[1024];
	const gchar *text;
	GtkWidget *scrwin;
	GtkWidget *treeview;
	GtkTreeStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeIter iter, parent;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	static PangoFontDescription *font_desc;
	GtkStyle *style;

	GSList *cur, *to_list = NULL;
	gboolean check_recp = FALSE;
	gint state = 0;
 
	g_return_val_if_fail(compose->account != NULL, FALSE);
	g_return_val_if_fail(compose->account->address != NULL, FALSE);

	if (!prefs_common.check_recipients)
		return TRUE;

	if (prefs_common.check_recp_exclude) {
		gchar **strv;
		gint i;

		strv = g_strsplit(prefs_common.check_recp_exclude, ",", -1);
		for (i = 0; strv[i] != NULL; i++)
			g_strstrip(strv[i]);

		if (compose->use_to) {
			text = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
			to_list = address_list_append_orig(NULL, text);
		}
		if (compose->use_cc) {
			text = gtk_entry_get_text(GTK_ENTRY(compose->cc_entry));
			to_list = address_list_append_orig(to_list, text);
		}
		if (compose->use_bcc) {
			text = gtk_entry_get_text(GTK_ENTRY(compose->bcc_entry));
			to_list = address_list_append_orig(to_list, text);
		}

		for (cur = to_list; cur != NULL; cur = cur->next) {
			for (i = 0; strv[i] != NULL; i++) {
				if (strv[i][0] == '\0')
					continue;
				if (strcasestr((gchar *)cur->data, strv[i]))
					break;
			}
			if (!strv[i]) {
				/* not found in exclude list */
				check_recp = TRUE;
				break;
			}
		}

		slist_free_strings(to_list);
		g_slist_free(to_list);
		to_list = NULL;
		g_strfreev(strv);
	} else
		check_recp = TRUE;

	if (!check_recp)
		return TRUE;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_title(GTK_WINDOW(window), _("Check recipients"));
	gtk_window_set_position(GTK_WINDOW(window),
				GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_widget_set_size_request(window, 480 * gtkut_get_dpi_multiplier(), -1);
	gtk_widget_realize(window);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(check_recp_delete_event), &state);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(check_recp_key_pressed), &state);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	image = gtk_image_new_from_stock
		(GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.0);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);

	label = gtk_label_new(_("Check recipients"));
	gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

	if (!font_desc) {
		gint size;

		size = pango_font_description_get_size
			(label->style->font_desc);
		font_desc = pango_font_description_new();
		pango_font_description_set_weight
			(font_desc, PANGO_WEIGHT_BOLD);
		pango_font_description_set_size
			(font_desc, size * PANGO_SCALE_LARGE);
	}
	if (font_desc)
		gtk_widget_modify_font(label, font_desc);

	label = gtk_label_new
		(_("Really send this mail to the following addresses?"));
	gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	GTK_WIDGET_UNSET_FLAGS(label, GTK_CAN_FOCUS);

	table = gtk_table_new(2, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 4);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(prefs_common.trans_hdr ? _("From:")
			      : "From:");
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
                         GTK_FILL, 0, 2, 0);
	entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_ENTRY_LENGTH);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	style = gtk_widget_get_style(window);
	gtk_widget_modify_base(entry, GTK_STATE_NORMAL,
			       &style->bg[GTK_STATE_NORMAL]);
	gtk_table_attach_defaults
                (GTK_TABLE(table), entry, 1, 2, 0, 1);

	if (compose->account->name && *compose->account->name) {
		g_snprintf(buf, sizeof(buf), "%s <%s>",
			   compose->account->name, compose->account->address);
		gtk_entry_set_text(GTK_ENTRY(entry), buf);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), compose->account->address);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(prefs_common.trans_hdr ? _("Subject:")
			      : "Subject:");
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
                         GTK_FILL, 0, 2, 0);
	entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_ENTRY_LENGTH);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	style = gtk_widget_get_style(window);
	gtk_widget_modify_base(entry, GTK_STATE_NORMAL,
			       &style->bg[GTK_STATE_NORMAL]);
	gtk_table_attach_defaults
                (GTK_TABLE(table), entry, 1, 2, 1, 2);

	text = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	gtk_entry_set_text(GTK_ENTRY(entry), text);

	scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrwin),
					    GTK_SHADOW_IN);
	gtk_widget_set_size_request(scrwin, -1, 180 * gtkut_get_dpi_multiplier());
	gtk_box_pack_start(GTK_BOX(vbox), scrwin, TRUE, TRUE, 0);

	store = gtk_tree_store_new(1, G_TYPE_STRING);
	if (compose->use_to) {
		text = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
		to_list = address_list_append_orig(NULL, text);
		if (to_list) {
			gtk_tree_store_append(store, &parent, NULL);
			gtk_tree_store_set(store, &parent, 0,
					   prefs_common.trans_hdr ?
					   _("To:") : "To:", -1);
			for (cur = to_list; cur != NULL; cur = cur->next) {
				gtk_tree_store_append(store, &iter, &parent);
				gtk_tree_store_set(store, &iter, 0,
						   (gchar *)cur->data, -1);
			}
			slist_free_strings(to_list);
			g_slist_free(to_list);
		}
	}
	if (compose->use_cc) {
		text = gtk_entry_get_text(GTK_ENTRY(compose->cc_entry));
		to_list = address_list_append_orig(NULL, text);
		if (to_list) {
			gtk_tree_store_append(store, &parent, NULL);
			gtk_tree_store_set(store, &parent, 0,
					   prefs_common.trans_hdr ?
					   _("Cc:") : "Cc:", -1);
			for (cur = to_list; cur != NULL; cur = cur->next) {
				gtk_tree_store_append(store, &iter, &parent);
				gtk_tree_store_set(store, &iter, 0,
						   (gchar *)cur->data, -1);
			}
			slist_free_strings(to_list);
			g_slist_free(to_list);
		}
	}
	if (compose->use_bcc) {
		text = gtk_entry_get_text(GTK_ENTRY(compose->bcc_entry));
		to_list = address_list_append_orig(NULL, text);
		if (to_list) {
			gtk_tree_store_append(store, &parent, NULL);
			gtk_tree_store_set(store, &parent, 0,
					   prefs_common.trans_hdr ?
					   _("Bcc:") : "Bcc:", -1);
			for (cur = to_list; cur != NULL; cur = cur->next) {
				gtk_tree_store_append(store, &iter, &parent);
				gtk_tree_store_set(store, &iter, 0,
						   (gchar *)cur->data, -1);
			}
			slist_free_strings(to_list);
			g_slist_free(to_list);
		}
	}

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);

	gtk_container_add(GTK_CONTAINER(scrwin), treeview);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Address"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview));

	gtkut_stock_button_set_create(&hbbox, &ok_btn, _("_Send"),
				      &cancel_btn, GTK_STOCK_CANCEL,
				      NULL, NULL);
        gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
        gtk_widget_grab_default(ok_btn);
	gtk_widget_grab_focus(ok_btn);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(check_recp_ok), &state);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(check_recp_cancel), &state);

	manage_window_set_transient(GTK_WINDOW(window));

	gtk_widget_show_all(window);

	while (state == 0)
		gtk_main_iteration();

	gtk_widget_destroy(window);

	if (state == GTK_RESPONSE_OK)
		return TRUE;

	return FALSE;
}

static gboolean compose_check_activities(Compose *compose)
{
	if (inc_is_active()) {
		alertpanel_notice(_("Checking for new messages is currently running.\n"
				    "Please try again later."));
		return FALSE;
	}

	return TRUE;
}

static void compose_add_new_recipients_to_addressbook(Compose *compose)
{
	GSList *to_list = NULL, *cur;
	const gchar *text;

	if (compose->use_to) {
		text = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
		to_list = address_list_append_orig(NULL, text);
	}
	if (compose->use_cc) {
		text = gtk_entry_get_text(GTK_ENTRY(compose->cc_entry));
		to_list = address_list_append_orig(to_list, text);
	}
	if (compose->use_bcc) {
		text = gtk_entry_get_text(GTK_ENTRY(compose->bcc_entry));
		to_list = address_list_append_orig(to_list, text);
	}

	for (cur = to_list; cur != NULL; cur = cur->next) {
		gchar *orig_addr = cur->data;
		gchar *name, *addr;

		name = procheader_get_fromname(orig_addr);
		addr = g_strdup(orig_addr);
		extract_address(addr);
		if (!g_ascii_strcasecmp(name, addr)) {
			g_free(name);
			name = NULL;
		}

		if (addressbook_has_address(addr))
			debug_print("compose_add_new_recipients_to_addressbook: address <%s> already registered.\n", addr);
		else
			addressbook_add_contact_autoreg(name, addr, NULL);

		g_free(addr);
		g_free(name);
	}

	slist_free_strings(to_list);
	g_slist_free(to_list);
}

void compose_lock(Compose *compose)
{
	compose->lock_count++;
}

void compose_unlock(Compose *compose)
{
	if (compose->lock_count > 0)
		compose->lock_count--;
}

void compose_block_modified(Compose *compose)
{
	compose->block_modified = TRUE;
}

void compose_unblock_modified(Compose *compose)
{
	compose->block_modified = FALSE;
}

#define C_LOCK()			\
{					\
	inc_lock();			\
	compose_lock(compose);		\
}

#define C_UNLOCK()			\
{					\
	compose_unlock(compose);	\
	inc_unlock();			\
}

static gint compose_send_real(Compose *compose)
{
	gchar tmp[MAXPATHLEN + 1];
	gint ok = 0;
	gboolean cancel = FALSE;

	if (compose->lock_count > 0)
		return 1;

	g_return_val_if_fail(compose->account != NULL, -1);

	C_LOCK();

	if (compose_check_entries(compose) == FALSE) {
		C_UNLOCK();
		return 1;
	}
	if (compose_check_attachments(compose) == FALSE) {
		C_UNLOCK();
		return 1;
	}
	if (compose_check_recipients(compose) == FALSE) {
		C_UNLOCK();
		return 1;
	}
	if (compose_check_activities(compose) == FALSE) {
		C_UNLOCK();
		return 1;
	}

	if (!main_window_toggle_online_if_offline(main_window_get())) {
		C_UNLOCK();
		return 1;
	}

	/* write to temporary file */
	g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg.%p",
		   get_tmp_dir(), G_DIR_SEPARATOR, compose);

	if (compose->mode == COMPOSE_REDIRECT) {
		if (compose_redirect_write_to_file(compose, tmp) < 0) {
			C_UNLOCK();
			return -1;
		}
	} else {
		if (compose_write_to_file(compose, tmp, FALSE) < 0) {
			C_UNLOCK();
			return -1;
		}
	}

	if (!compose->to_list && !compose->newsgroup_list) {
		g_warning(_("can't get recipient list."));
		g_unlink(tmp);
		C_UNLOCK();
		return 1;
	}

	syl_plugin_signal_emit("compose-send", compose, compose->mode, 0,
			       tmp, compose->to_list, &cancel);
	if (cancel) {
		g_unlink(tmp);
		C_UNLOCK();
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
				g_unlink(tmp);
				C_UNLOCK();
				return -1;
			}
		}

		/* POP before SMTP requires inc to be unlocked.
		   send_message() also locks inc internally. */
		inc_unlock();
		ok = send_message(tmp, ac, compose->to_list);
		inc_lock();

		statusbar_pop_all();
	}

	if (ok == 0 && compose->newsgroup_list) {
		ok = news_post(FOLDER(compose->account->folder), tmp);
		if (ok < 0) {
			alertpanel_error(_("Error occurred while posting the message to %s ."),
					 compose->account->nntp_server);
			g_unlink(tmp);
			C_UNLOCK();
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

		if (compose->reply_target)
			send_message_set_reply_flag(compose->reply_target,
						    compose->inreplyto);
		else if (compose->forward_targets)
			send_message_set_forward_flags
				(compose->forward_targets);

		/* save message to outbox */
		if (prefs_common.savemsg) {
			FolderItem *outbox;
			gboolean drop_done = FALSE;

			/* filter sent message */
			if (prefs_common.filter_sent) {
				FilterInfo *fltinfo;

				fltinfo = filter_info_new();
				fltinfo->account = compose->account;
				fltinfo->flags.perm_flags = 0;
				fltinfo->flags.tmp_flags = MSG_RECEIVED;

				filter_apply(prefs_common.fltlist, tmp,
					     fltinfo);

				drop_done = fltinfo->drop_done;
				folderview_update_all_updated(TRUE);
				filter_info_free(fltinfo);
			}

			if (!drop_done) {
				outbox = account_get_special_folder
					(compose->account, F_OUTBOX);
				if (procmsg_save_to_outbox(outbox, tmp) < 0) {
					alertpanel_error
						(_("Sending of message was completed, but the message could not be saved to outbox."));
					ok = -2;
				} else
					folderview_update_item(outbox, TRUE);
			}
		}

		/* Add recipients to addressbook automatically */
		if (prefs_common.recipients_autoreg) {
			compose_add_new_recipients_to_addressbook(compose);
		}
	}

	g_unlink(tmp);
	C_UNLOCK();

	return ok;
}

#if USE_GPGME
static const gchar *compose_get_self_key_id(Compose *compose)
{
	const gchar *key_id = NULL;

	switch (compose->account->sign_key) {
	case SIGN_KEY_DEFAULT:
		break;
	case SIGN_KEY_BY_FROM:
		key_id = compose->account->address;
		break;
	case SIGN_KEY_CUSTOM:
		key_id = compose->account->sign_key_id;
		break;
	default:
		break;
	}

	return key_id;
}

/* interfaces to rfc2015 to keep out the prefs stuff there.
 * returns 0 on success and -1 on error. */
static gint compose_create_signers_list(Compose *compose, GSList **pkey_list)
{
	const gchar *key_id = NULL;
	GSList *key_list;

	key_id = compose_get_self_key_id(compose);
	if (!key_id) {
		*pkey_list = NULL;
		return 0;
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

static GSList *compose_create_encrypt_recipients_list(Compose *compose)
{
	GSList *recp_list = NULL;
	const gchar *key_id = NULL;

	g_return_val_if_fail(compose->to_list != NULL, NULL);

	recp_list = g_slist_copy(compose->to_list);
	if (compose->account->encrypt_to_self) {
		key_id = compose_get_self_key_id(compose);
		recp_list = g_slist_append(recp_list, (gpointer)key_id);
	}

	return recp_list;
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

	if (compose_create_signers_list(compose, &key_list) < 0) {
		g_unlink(tmp_file);
		g_free(tmp_file);
		return -1;
	}
	if (rfc2015_clearsign(tmp_file, key_list) < 0) {
		alertpanel_error(_("Can't sign the message."));
		g_unlink(tmp_file);
		g_free(tmp_file);
		return -1;
	}

	g_free(*text);
	*text = file_read_to_str(tmp_file);
	g_unlink(tmp_file);
	g_free(tmp_file);
	if (*text == NULL)
		return -1;

	return 0;
}

static gint compose_encrypt_armored(Compose *compose, gchar **text)
{
	gchar *tmp_file;
	GSList *recp_list;

	tmp_file = get_tmp_file();
	if (str_write_to_file(*text, tmp_file) < 0) {
		g_free(tmp_file);
		return -1;
	}

	recp_list = compose_create_encrypt_recipients_list(compose);

	if (rfc2015_encrypt_armored(tmp_file, recp_list) < 0) {
		alertpanel_error(_("Can't encrypt the message."));
		g_slist_free(recp_list);
		g_unlink(tmp_file);
		g_free(tmp_file);
		return -1;
	}

	g_slist_free(recp_list);
	g_free(*text);
	*text = file_read_to_str(tmp_file);
	g_unlink(tmp_file);
	g_free(tmp_file);
	if (*text == NULL)
		return -1;

	return 0;
}

static gint compose_encrypt_sign_armored(Compose *compose, gchar **text)
{
	GSList *key_list;
	GSList *recp_list;
	gchar *tmp_file;

	tmp_file = get_tmp_file();
	if (str_write_to_file(*text, tmp_file) < 0) {
		g_free(tmp_file);
		return -1;
	}

	if (compose_create_signers_list(compose, &key_list) < 0) {
		g_unlink(tmp_file);
		g_free(tmp_file);
		return -1;
	}

	recp_list = compose_create_encrypt_recipients_list(compose);

	if (rfc2015_encrypt_sign_armored(tmp_file, recp_list, key_list) < 0) {
		alertpanel_error(_("Can't encrypt or sign the message."));
		g_slist_free(recp_list);
		g_unlink(tmp_file);
		g_free(tmp_file);
		return -1;
	}

	g_slist_free(recp_list);
	g_free(*text);
	*text = file_read_to_str(tmp_file);
	g_unlink(tmp_file);
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
	GtkTreeModel *model = GTK_TREE_MODEL(compose->attach_store);
	FILE *fp;
	size_t len;
	gchar *chars;
	gchar *buf;
	gchar *canon_buf;
	const gchar *out_charset;
	const gchar *body_charset;
	const gchar *src_charset = CS_INTERNAL;
	EncodingType encoding;
	gint line;
#if USE_GPGME
	gboolean use_pgpmime_encryption = FALSE;
	gboolean use_pgpmime_signing = FALSE;
#endif

	if ((fp = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, file) < 0) {
		FILE_OP_ERROR(file, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	/* get outgoing charset */
	out_charset = conv_get_charset_str(compose->out_encoding);
	if (!out_charset)
		out_charset = conv_get_outgoing_charset_str();
	if (!g_ascii_strcasecmp(out_charset, CS_US_ASCII))
		out_charset = CS_ISO_8859_1;
	body_charset = out_charset;

	/* get all composed text */
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(compose->text));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	chars = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	if (is_ascii_str(chars)) {
		buf = chars;
		chars = NULL;
		body_charset = CS_US_ASCII;
		encoding = ENC_7BIT;
	} else {
		gint error = 0;

		buf = conv_codeset_strdup_full
			(chars, src_charset, body_charset, &error);
		if (!buf || error != 0) {
			AlertValue aval = G_ALERTDEFAULT;
			gchar *msg;

			g_free(buf);

			if (!is_draft) {
				msg = g_strdup_printf(_("Can't convert the character encoding of the message body from %s to %s.\n"
							"\n"
							"Send it as %s anyway?"),
						      src_charset, body_charset,
						      src_charset);
				aval = alertpanel_full
					(_("Code conversion error"), msg, ALERT_ERROR,
					 G_ALERTALTERNATE,
					 FALSE, GTK_STOCK_YES, GTK_STOCK_NO, NULL);
				g_free(msg);
			}

			if (aval != G_ALERTDEFAULT) {
				g_free(chars);
				fclose(fp);
				g_unlink(file);
				return -1;
			} else {
				buf = chars;
				out_charset = body_charset = src_charset;
				chars = NULL;
			}
		}

		if (prefs_common.encoding_method == CTE_BASE64)
			encoding = ENC_BASE64;
		else if (prefs_common.encoding_method == CTE_QUOTED_PRINTABLE)
			encoding = ENC_QUOTED_PRINTABLE;
		else if (prefs_common.encoding_method == CTE_8BIT)
			encoding = ENC_8BIT;
		else
			encoding = procmime_get_encoding_for_charset
				(body_charset);
	}
	g_free(chars);

	canon_buf = canonicalize_str(buf);
	g_free(buf);
	buf = canon_buf;

#if USE_GPGME
	if (compose->use_signing && !compose->account->clearsign)
		use_pgpmime_signing = TRUE;
	if (compose->use_encryption && compose->account->ascii_armored) {
		use_pgpmime_encryption = FALSE;
		use_pgpmime_signing = FALSE;
	}
	if (compose->use_encryption && !compose->account->ascii_armored)
		use_pgpmime_encryption = TRUE;

	/* protect trailing spaces */
	if (rfc2015_is_available() && !is_draft && use_pgpmime_signing) {
		if (encoding == ENC_7BIT) {
			if (!g_ascii_strcasecmp(body_charset, CS_ISO_2022_JP)) {
				gchar *tmp;
				tmp = strchomp_all(buf);
				g_free(buf);
				buf = tmp;
			} else
				encoding = ENC_QUOTED_PRINTABLE;
		} else if (encoding == ENC_8BIT) {
			encoding = procmime_get_encoding_for_str(buf);
			if (encoding == ENC_7BIT)
				encoding = ENC_QUOTED_PRINTABLE;
		}
	}

	if (rfc2015_is_available() && !is_draft) {
		if ((compose->use_encryption &&
		     compose->account->ascii_armored) ||
		    (compose->use_signing && compose->account->clearsign)) {
			/* MIME encoding doesn't fit with cleartext signature */
			if (encoding == ENC_QUOTED_PRINTABLE || encoding == ENC_BASE64)
				encoding = ENC_8BIT;

		}
	}
#endif

	debug_print("src encoding = %s, out encoding = %s, "
		    "body encoding = %s, transfer encoding = %s\n",
		    src_charset, out_charset, body_charset,
		    procmime_get_encoding_str(encoding));

	/* check for line length limit */
	if (!is_draft &&
	    encoding != ENC_QUOTED_PRINTABLE && encoding != ENC_BASE64 &&
	    check_line_length(buf, 1000, &line) < 0) {
		AlertValue aval;
		gchar *msg;

		msg = g_strdup_printf
			(_("Line %d exceeds the line length limit (998 bytes).\n"
			   "The contents of the message might be broken on the way to the delivery.\n"
			   "\n"
			   "Send it anyway?"), line + 1);
		aval = alertpanel_full(_("Line length limit"),
				       msg, ALERT_WARNING,
				       G_ALERTALTERNATE, FALSE,
				       GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (aval != G_ALERTDEFAULT) {
			g_free(msg);
			fclose(fp);
			g_unlink(file);
			g_free(buf);
			return -1;
		}
	}

	/* write headers */
	if (compose_write_headers(compose, fp, out_charset,
				  body_charset, encoding, is_draft) < 0) {
		g_warning("can't write headers\n");
		fclose(fp);
		g_unlink(file);
		g_free(buf);
		return -1;
	}

#if USE_GPGME
	/* do ascii-armor encryption and/or clearsign */
	if (rfc2015_is_available() && !is_draft) {
		gint ret;

		if (compose->use_encryption && compose->account->ascii_armored) {
			if (compose->use_signing)
				ret = compose_encrypt_sign_armored(compose, &buf);
			else
				ret = compose_encrypt_armored(compose, &buf);
			if (ret < 0) {
				g_warning("ascii-armored encryption failed\n");
				fclose(fp);
				g_unlink(file);
				g_free(buf);
				return -1;
			}
		} else if (compose->use_signing && compose->account->clearsign) {
			if (compose_clearsign_text(compose, &buf) < 0) {
				g_warning("clearsign failed\n");
				fclose(fp);
				g_unlink(file);
				g_free(buf);
				return -1;
			}
		}
	}
#endif

	if (compose->use_attach &&
	    gtk_tree_model_iter_n_children(model, NULL) > 0) {
#if USE_GPGME
            /* This prolog message is ignored by mime software and
             * because it would make our signing/encryption task
             * tougher, we don't emit it in that case */
            if (!rfc2015_is_available() ||
		(!compose->use_signing && !compose->use_encryption))
#endif
		fputs("This is a multi-part message in MIME format.\n", fp);

		fprintf(fp, "\n--%s\n", compose->boundary);
		fprintf(fp, "Content-Type: text/plain; charset=%s\n",
			body_charset);
#if USE_GPGME
		if (rfc2015_is_available() && use_pgpmime_signing)
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
			base64_encode(outbuf, (guchar *)buf + i, l);
			fputs(outbuf, fp);
			fputc('\n', fp);
		}
	} else if (encoding == ENC_QUOTED_PRINTABLE) {
		gchar *outbuf;
		size_t outlen;

		outbuf = g_malloc(len * 4);
		qp_encode_line(outbuf, (guchar *)buf);
		outlen = strlen(outbuf);
		if (fwrite(outbuf, sizeof(gchar), outlen, fp) != outlen) {
			FILE_OP_ERROR(file, "fwrite");
			fclose(fp);
			g_unlink(file);
			g_free(outbuf);
			g_free(buf);
			return -1;
		}
		g_free(outbuf);
	} else if (fwrite(buf, sizeof(gchar), len, fp) != len) {
		FILE_OP_ERROR(file, "fwrite");
		fclose(fp);
		g_unlink(file);
		g_free(buf);
		return -1;
	}
	g_free(buf);

	if (compose->use_attach &&
	    gtk_tree_model_iter_n_children(model, NULL) > 0) {
		if (compose_write_attach(compose, fp, out_charset) < 0) {
			fclose(fp);
			g_unlink(file);
			return -1;
		}
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		g_unlink(file);
		return -1;
	}

#if USE_GPGME
	if (!rfc2015_is_available() || is_draft) {
		uncanonicalize_file_replace(file);
		return 0;
	}

	if (use_pgpmime_signing || use_pgpmime_encryption) {
		if (canonicalize_file_replace(file) < 0) {
			g_unlink(file);
			return -1;
		}
	}

	if (use_pgpmime_signing && !use_pgpmime_encryption) {
		GSList *key_list;

		if (compose_create_signers_list(compose, &key_list) < 0) {
			g_unlink(file);
			return -1;
		}
		if (rfc2015_sign(file, key_list) < 0) {
			alertpanel_error(_("Can't sign the message."));
			g_unlink(file);
			return -1;
		}
	} else if (use_pgpmime_encryption) {
		GSList *key_list;
		GSList *recp_list;

		if (compose->use_bcc) {
			const gchar *text;
			gchar *bcc;
			AlertValue aval;

			text = gtk_entry_get_text(GTK_ENTRY(compose->bcc_entry));
			if (*text != '\0') {
				bcc = g_strdup(text);
				g_strstrip(bcc);
				if (*bcc != '\0') {
					aval = alertpanel_full
						(_("Encrypting with Bcc"),
						 _("This message has Bcc recipients. If this message is encrypted, all Bcc recipients will be visible by examing the encryption key list, leading to loss of confidentiality.\n"
						   "\n"
						   "Send it anyway?"),
						 ALERT_WARNING, G_ALERTDEFAULT, FALSE,
						 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
					if (aval != G_ALERTDEFAULT) {
						g_free(bcc);
						g_unlink(file);
						return -1;
					}
				}
				g_free(bcc);
			}
		}

		recp_list = compose_create_encrypt_recipients_list(compose);

		if (use_pgpmime_signing) {
			if (compose_create_signers_list
				(compose, &key_list) < 0) {
				g_unlink(file);
				return -1;
			}

			if (rfc2015_encrypt_sign(file, recp_list, key_list) < 0) {
				alertpanel_error(_("Can't encrypt or sign the message."));
				g_slist_free(recp_list);
				g_unlink(file);
				return -1;
			}
		} else if (rfc2015_encrypt(file, recp_list) < 0) {
			alertpanel_error(_("Can't encrypt the message."));
			g_slist_free(recp_list);
			g_unlink(file);
			return -1;
		}

		g_slist_free(recp_list);
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

	if ((fp = g_fopen(file, "wb")) == NULL) {
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

	chars = conv_codeset_strdup
		(tmp, CS_INTERNAL, conv_get_locale_charset_str());

	g_free(tmp);

	if (!chars) {
		fclose(fp);
		g_unlink(file);
		return -1;
	}

	/* write body */
	len = strlen(chars);
	if (fwrite(chars, sizeof(gchar), len, fp) != len) {
		FILE_OP_ERROR(file, "fwrite");
		g_free(chars);
		fclose(fp);
		g_unlink(file);
		return -1;
	}

	g_free(chars);

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		g_unlink(file);
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

	if ((fdest = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		fclose(fp);
		return -1;
	}

	if (change_file_mode_rw(fdest, file) < 0) {
		FILE_OP_ERROR(file, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) == 0) {
		if (g_ascii_strncasecmp(buf, "Return-Path:",
					strlen("Return-Path:")) == 0 ||
		    g_ascii_strncasecmp(buf, "Delivered-To:",
					strlen("Delivered-To:")) == 0 ||
		    g_ascii_strncasecmp(buf, "Received:",
					strlen("Received:")) == 0 ||
		    g_ascii_strncasecmp(buf, "Subject:",
					strlen("Subject:")) == 0 ||
		    g_ascii_strncasecmp(buf, "X-UIDL:",
					strlen("X-UIDL:")) == 0)
			continue;

		if (fputs(buf, fdest) == EOF)
			goto error;

#if 0
		if (g_ascii_strncasecmp(buf, "From:", strlen("From:")) == 0) {
			fputs("\n (by way of ", fdest);
			if (compose->account->name) {
				compose_convert_header(compose,
						       buf, sizeof(buf),
						       compose->account->name,
						       strlen(" (by way of "),
						       FALSE, NULL);
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
		g_unlink(file);
		return -1;
	}

	return 0;
error:
	fclose(fp);
	fclose(fdest);
	g_unlink(file);

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
	MsgFlags flag = {0, MSG_QUEUED};

	debug_print(_("queueing message...\n"));
	g_return_val_if_fail(compose->to_list != NULL ||
			     compose->newsgroup_list != NULL,
			     -1);
	g_return_val_if_fail(compose->account != NULL, -1);

	tmp = g_strdup_printf("%s%cqueue.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);
	if ((fp = g_fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		g_free(tmp);
		return -1;
	}
	if ((src_fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		fclose(fp);
		g_unlink(tmp);
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
	/* Reply target */
	if (compose->reply_target)
		fprintf(fp, "REP:%s\n", compose->reply_target);
	/* Forward target */
	if (compose->forward_targets)
		fprintf(fp, "FWD:%s\n", compose->forward_targets);
	fprintf(fp, "\n");

	while (fgets(buf, sizeof(buf), src_fp) != NULL) {
		if (fputs(buf, fp) == EOF) {
			FILE_OP_ERROR(tmp, "fputs");
			fclose(fp);
			fclose(src_fp);
			g_unlink(tmp);
			g_free(tmp);
			return -1;
		}
	}

	fclose(src_fp);
	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmp, "fclose");
		g_unlink(tmp);
		g_free(tmp);
		return -1;
	}

	queue = account_get_special_folder(compose->account, F_QUEUE);
	if (!queue) {
		g_warning(_("can't find queue folder\n"));
		g_unlink(tmp);
		g_free(tmp);
		return -1;
	}
	folder_item_scan(queue);
	if ((num = folder_item_add_msg(queue, tmp, &flag, TRUE)) < 0) {
		g_warning(_("can't queue the message\n"));
		g_unlink(tmp);
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

	/* Add recipients to addressbook automatically */
	if (prefs_common.recipients_autoreg) {
		compose_add_new_recipients_to_addressbook(compose);
	}

	main_window_set_menu_sensitive(main_window_get());
	main_window_set_toolbar_sensitive(main_window_get());

	return 0;
}

static gint compose_write_attach(Compose *compose, FILE *fp,
				 const gchar *charset)
{
	GtkTreeModel *model = GTK_TREE_MODEL(compose->attach_store);
	GtkTreeIter iter;
	gboolean valid;
	AttachInfo *ainfo;
	FILE *attach_fp;
	gint len;
	EncodingType encoding;
	ContentType content_type;
	gchar *tmp_file = NULL;
	const gchar *src_file;
	FILE *src_fp;
	FILE *tmp_fp = NULL;

	for (valid = gtk_tree_model_get_iter_first(model, &iter); valid;
	     valid = gtk_tree_model_iter_next(model, &iter)) {
		gtk_tree_model_get(model, &iter, COL_ATTACH_INFO, &ainfo, -1);

		if (!is_file_exist(ainfo->file)) {
			alertpanel_error(_("File %s doesn't exist."),
					 ainfo->file);
			return -1;
		}
		if (get_file_size(ainfo->file) <= 0) {
			alertpanel_error(_("File %s is empty."), ainfo->file);
			return -1;
		}
		if ((attach_fp = g_fopen(ainfo->file, "rb")) == NULL) {
			alertpanel_error(_("Can't open file %s."), ainfo->file);
			return -1;
		}

		fprintf(fp, "\n--%s\n", compose->boundary);

		encoding = ainfo->encoding;

		if (!g_ascii_strncasecmp(ainfo->content_type, "message/", 8)) {
			fprintf(fp, "Content-Type: %s\n", ainfo->content_type);
			fprintf(fp, "Content-Disposition: inline\n");

			/* message/... shouldn't be encoded */
			if (encoding == ENC_QUOTED_PRINTABLE ||
			    encoding == ENC_BASE64)
				encoding = ENC_8BIT;
		} else {
			if (prefs_common.mime_fencoding_method ==
			    FENC_RFC2231) {
				gchar *param;

				param = compose_convert_filename
					(compose, ainfo->name, "name", charset);
				fprintf(fp, "Content-Type: %s;\n"
					    "%s\n",
					ainfo->content_type, param);
				g_free(param);
				param = compose_convert_filename
					(compose, ainfo->name, "filename",
					 charset);
				fprintf(fp, "Content-Disposition: attachment;\n"
					    "%s\n", param);
				g_free(param);
			} else {
				gchar filename[BUFFSIZE];

				compose_convert_header(compose, filename,
						       sizeof(filename),
						       ainfo->name, 12, FALSE,
						       charset);
				fprintf(fp, "Content-Type: %s;\n"
					    " name=\"%s\"\n",
					ainfo->content_type, filename);
				fprintf(fp, "Content-Disposition: attachment;\n"
					    " filename=\"%s\"\n", filename);
			}

#if USE_GPGME
			/* force encoding to protect trailing spaces */
			if (rfc2015_is_available() && compose->use_signing &&
			    !compose->account->clearsign) {
				if (encoding == ENC_7BIT)
					encoding = ENC_QUOTED_PRINTABLE;
				else if (encoding == ENC_8BIT)
					encoding = ENC_BASE64;
			}
#endif
		}

		fprintf(fp, "Content-Transfer-Encoding: %s\n\n",
			procmime_get_encoding_str(encoding));

		content_type = procmime_scan_mime_type(ainfo->content_type);

		if (content_type == MIME_TEXT || content_type == MIME_TEXT_HTML) {
			CharSet enc;

			enc = conv_check_file_encoding(ainfo->file);
			if (enc == C_UTF_16 || enc == C_UTF_16BE || enc == C_UTF_16LE) {
				tmp_file = get_tmp_file();
				if (conv_copy_file(ainfo->file, tmp_file, conv_get_charset_str(enc)) < 0) {
					g_warning("compose_write_attach: Cannot convert UTF-16 file %s to UTF-8", ainfo->file);
					g_free(tmp_file);
					tmp_file = NULL;
				}
			}
		}

		if (tmp_file) {
			src_file = tmp_file;
		} else {
			src_file = ainfo->file;
		}

		if (encoding == ENC_BASE64) {
			gchar inbuf[B64_LINE_SIZE], outbuf[B64_BUFFSIZE];
			gchar *canon_file = NULL;

			if (content_type == MIME_TEXT ||
			    content_type == MIME_TEXT_HTML ||
			    content_type == MIME_MESSAGE_RFC822) {
				canon_file = get_tmp_file();
				if (canonicalize_file(src_file, canon_file) < 0) {
					g_free(canon_file);
					if (tmp_file) {
						g_unlink(tmp_file);
						g_free(tmp_file);
					}
					fclose(attach_fp);
					return -1;
				}
				if ((tmp_fp = g_fopen(canon_file, "rb")) == NULL) {
					FILE_OP_ERROR(canon_file, "fopen");
					g_unlink(canon_file);
					g_free(canon_file);
					if (tmp_file) {
						g_unlink(tmp_file);
						g_free(tmp_file);
					}
					fclose(attach_fp);
					return -1;
				}
			}

			if (tmp_fp) {
				src_fp = tmp_fp;
			} else {
				src_fp = attach_fp;
			}

			while ((len = fread(inbuf, sizeof(gchar),
					    B64_LINE_SIZE, src_fp))
			       == B64_LINE_SIZE) {
				base64_encode(outbuf, (guchar *)inbuf,
					      B64_LINE_SIZE);
				fputs(outbuf, fp);
				fputc('\n', fp);
			}
			if (len > 0 && feof(src_fp)) {
				base64_encode(outbuf, (guchar *)inbuf, len);
				fputs(outbuf, fp);
				fputc('\n', fp);
			}

			if (tmp_fp) {
				fclose(tmp_fp);
				tmp_fp = NULL;
			}
			if (canon_file) {
				g_unlink(canon_file);
				g_free(canon_file);
			}
		} else {
			if (tmp_file) {
				if ((tmp_fp = g_fopen(tmp_file, "rb")) == NULL) {
					FILE_OP_ERROR(tmp_file, "fopen");
					g_unlink(tmp_file);
					g_free(tmp_file);
					fclose(attach_fp);
					return -1;
				}
				src_fp = tmp_fp;
			} else {
				src_fp = attach_fp;
			}

			if (encoding == ENC_QUOTED_PRINTABLE) {
				gchar inbuf[BUFFSIZE], outbuf[BUFFSIZE * 4];

				while (fgets(inbuf, sizeof(inbuf), src_fp) != NULL) {
					qp_encode_line(outbuf, (guchar *)inbuf);
					fputs(outbuf, fp);
				}
			} else {
				gchar buf[BUFFSIZE];

				while (fgets(buf, sizeof(buf), src_fp) != NULL) {
					strcrchomp(buf);
					fputs(buf, fp);
				}
			}

			if (tmp_fp) {
				fclose(tmp_fp);
				tmp_fp = NULL;
			}
		}

		if (tmp_file) {
			g_unlink(tmp_file);
			g_free(tmp_file);
			tmp_file = NULL;
		}
		fclose(attach_fp);
	}

	fprintf(fp, "\n--%s--\n", compose->boundary);
	return 0;
}

#define QUOTE_REQUIRED(str) \
	(*str != '"' && strpbrk(str, ",.[]<>") != NULL)

#define PUT_RECIPIENT_HEADER(header, str)				     \
{									     \
	if (*str != '\0') {						     \
		gchar *dest;						     \
									     \
		dest = g_strdup(str);					     \
		g_strstrip(dest);					     \
		if (*dest != '\0') {					     \
			compose->to_list = address_list_append		     \
				(compose->to_list, dest);		     \
			compose_convert_header				     \
				(compose, buf, sizeof(buf), dest,	     \
				 strlen(header) + 2, TRUE, charset);	     \
			fprintf(fp, "%s: %s\n", header, buf);		     \
		}							     \
		g_free(dest);						     \
	}								     \
}

#define IS_IN_CUSTOM_HEADER(header) \
	(compose->account->add_customhdr && \
	 custom_header_find(compose->account->customhdr_list, header) != NULL)

static gint compose_write_headers(Compose *compose, FILE *fp,
				  const gchar *charset,
				  const gchar *body_charset,
				  EncodingType encoding, gboolean is_draft)
{
	gchar buf[BUFFSIZE];
	const gchar *entry_str;
	gchar *str;

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
			(compose, buf, sizeof(buf), compose->account->name,
			 strlen("From: "), TRUE, charset);
		if (QUOTE_REQUIRED(buf))
			fprintf(fp, "From: \"%s\" <%s>\n",
				buf, compose->account->address);
		else
			fprintf(fp, "From: %s <%s>\n",
				buf, compose->account->address);
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
			str = g_strdup(entry_str);
			g_strstrip(str);
			remove_space(str);
			if (*str != '\0') {
				compose->newsgroup_list =
					newsgroup_list_append
						(compose->newsgroup_list, str);
				compose_convert_header(compose,
						       buf, sizeof(buf), str,
						       strlen("Newsgroups: "),
						       FALSE, charset);
				fprintf(fp, "Newsgroups: %s\n", buf);
			}
			g_free(str);
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
		str = g_strdup(entry_str);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(compose, buf, sizeof(buf), str,
					       strlen("Subject: "), FALSE,
					       charset);
			fprintf(fp, "Subject: %s\n", buf);
		}
		g_free(str);
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
			str = g_strdup(entry_str);
			g_strstrip(str);
			remove_space(str);
			if (*str != '\0') {
				compose_convert_header(compose,
						       buf, sizeof(buf), str,
						       strlen("Followup-To: "),
						       FALSE, charset);
				fprintf(fp, "Followup-To: %s\n", buf);
			}
			g_free(str);
		}
	}

	/* Reply-To */
	if (compose->use_replyto && !IS_IN_CUSTOM_HEADER("Reply-To")) {
		entry_str = gtk_entry_get_text(GTK_ENTRY(compose->reply_entry));
		if (*entry_str != '\0') {
			str = g_strdup(entry_str);
			g_strstrip(str);
			if (*str != '\0') {
				compose_convert_header(compose,
						       buf, sizeof(buf), str,
						       strlen("Reply-To: "),
						       TRUE, charset);
				fprintf(fp, "Reply-To: %s\n", buf);
			}
			g_free(str);
		}
	}

	/* Disposition-Notification-To */
	if (compose->use_mdn &&
	    !IS_IN_CUSTOM_HEADER("Disposition-Notification-To")) {
		fprintf(fp, "Disposition-Notification-To: %s\n",
			compose->account->address);
	}

	/* Organization */
	if (compose->account->organization &&
	    !IS_IN_CUSTOM_HEADER("Organization")) {
		compose_convert_header(compose, buf, sizeof(buf),
				       compose->account->organization,
				       strlen("Organization: "), FALSE,
				       charset);
		fprintf(fp, "Organization: %s\n", buf);
	}

	/* Program version and system info */
	if (prefs_common.user_agent_str) {
		if (compose->to_list && !IS_IN_CUSTOM_HEADER("X-Mailer")) {
			fprintf(fp, "X-Mailer: %s\n",
				prefs_common.user_agent_str);
		}
		if (compose->newsgroup_list && !IS_IN_CUSTOM_HEADER("X-Newsreader")) {
			fprintf(fp, "X-Newsreader: %s\n",
				prefs_common.user_agent_str);
		}
	}

	/* custom headers */
	if (compose->account->add_customhdr) {
		GSList *cur;

		for (cur = compose->account->customhdr_list; cur != NULL;
		     cur = cur->next) {
			CustomHeader *chdr = (CustomHeader *)cur->data;

			if (g_ascii_strcasecmp(chdr->name, "Date") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "From") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "To") != 0 &&
			 /* g_ascii_strcasecmp(chdr->name, "Sender") != 0 && */
			    g_ascii_strcasecmp(chdr->name, "Message-Id") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "In-Reply-To") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "References") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "Mime-Version") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "Content-Type") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "Content-Transfer-Encoding") != 0) {
				compose_convert_header
					(compose, buf, sizeof(buf),
					 chdr->value ? chdr->value : "",
					 strlen(chdr->name) + 2, FALSE,
					 charset);
				fprintf(fp, "%s: %s\n", chdr->name, buf);
			}
		}
	}

	/* MIME */
	fprintf(fp, "Mime-Version: 1.0\n");
	if (compose->use_attach &&
	    gtk_tree_model_iter_n_children
		(GTK_TREE_MODEL(compose->attach_store), NULL) > 0) {
		compose->boundary = generate_mime_boundary(NULL);
		fprintf(fp,
			"Content-Type: multipart/mixed;\n"
			" boundary=\"%s\"\n", compose->boundary);
	} else {
		fprintf(fp, "Content-Type: text/plain; charset=%s\n",
			body_charset);
#if USE_GPGME
		if (rfc2015_is_available() &&
		    compose->use_signing && !compose->account->clearsign)
			fprintf(fp, "Content-Disposition: inline\n");
#endif
		fprintf(fp, "Content-Transfer-Encoding: %s\n",
			procmime_get_encoding_str(encoding));
	}

	/* X-Sylpheed headers */
	if (is_draft) {
		fprintf(fp, "X-Sylpheed-Account-Id: %d\n",
			compose->account->account_id);
		if (compose->reply_target)
			fprintf(fp, "X-Sylpheed-Reply: %s\n",
				compose->reply_target);
		else if (compose->forward_targets)
			fprintf(fp, "X-Sylpheed-Forward: %s\n",
				compose->forward_targets);
		fprintf(fp, "X-Sylpheed-Compose-AutoWrap: %s\n",
			compose->autowrap ? "TRUE" : "FALSE");
#if USE_GTKSPELL
		fprintf(fp, "X-Sylpheed-Compose-CheckSpell: %s\n",
			compose->check_spell ? "TRUE" : "FALSE");
		if (compose->spell_lang)
			fprintf(fp, "X-Sylpheed-Compose-SpellLang: %s\n",
				compose->spell_lang);
#endif
#if USE_GPGME
		fprintf(fp, "X-Sylpheed-Compose-UseSigning: %s\n",
			compose->use_signing ? "TRUE" : "FALSE");
		fprintf(fp, "X-Sylpheed-Compose-UseEncryption: %s\n",
			compose->use_encryption ? "TRUE" : "FALSE");
#endif
	}

	/* separator between header and body */
	fputs("\n", fp);

	return 0;
}

static gint compose_redirect_write_headers(Compose *compose, FILE *fp)
{
	gchar buf[BUFFSIZE];
	const gchar *entry_str;
	gchar *str;
	const gchar *charset = NULL;

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(compose->account != NULL, -1);
	g_return_val_if_fail(compose->account->address != NULL, -1);

	/* Resent-Date */
	get_rfc822_date(buf, sizeof(buf));
	fprintf(fp, "Resent-Date: %s\n", buf);

	/* Resent-From */
	if (compose->account->name) {
		compose_convert_header
			(compose, buf, sizeof(buf), compose->account->name,
			 strlen("Resent-From: "), TRUE, NULL);
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
			str = g_strdup(entry_str);
			g_strstrip(str);
			remove_space(str);
			if (*str != '\0') {
				compose->newsgroup_list =
					newsgroup_list_append
						(compose->newsgroup_list, str);
				compose_convert_header(compose,
						       buf, sizeof(buf), str,
						       strlen("Newsgroups: "),
						       FALSE, NULL);
				fprintf(fp, "Newsgroups: %s\n", buf);
			}
			g_free(str);
		}
	}

	if (!compose->to_list && !compose->newsgroup_list)
		return -1;

	/* Subject */
	entry_str = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	if (*entry_str != '\0') {
		str = g_strdup(entry_str);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(compose, buf, sizeof(buf), str,
					       strlen("Subject: "), FALSE,
					       NULL);
			fprintf(fp, "Subject: %s\n", buf);
		}
		g_free(str);
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
			str = g_strdup(entry_str);
			g_strstrip(str);
			remove_space(str);
			if (*str != '\0') {
				compose_convert_header(compose,
						       buf, sizeof(buf), str,
						       strlen("Followup-To: "),
						       FALSE, NULL);
				fprintf(fp, "Followup-To: %s\n", buf);
			}
			g_free(str);
		}
	}

	/* Resent-Reply-To */
	if (compose->use_replyto) {
		entry_str = gtk_entry_get_text(GTK_ENTRY(compose->reply_entry));
		if (*entry_str != '\0') {
			str = g_strdup(entry_str);
			g_strstrip(str);
			if (*str != '\0') {
				compose_convert_header
					(compose, buf, sizeof(buf), str,
					 strlen("Resent-Reply-To: "), TRUE,
					 NULL);
				fprintf(fp, "Resent-Reply-To: %s\n", buf);
			}
			g_free(str);
		}
	}

	fputs("\n", fp);

	return 0;
}

#undef IS_IN_CUSTOM_HEADER

static void compose_convert_header(Compose *compose, gchar *dest, gint len,
				   const gchar *src, gint header_len,
				   gboolean addr_field, const gchar *encoding)
{
	gchar *src_;

	g_return_if_fail(src != NULL);
	g_return_if_fail(dest != NULL);

	if (len < 1) return;

	if (addr_field)
		src_ = normalize_address_field(src);
	else
		src_ = g_strdup(src);
	g_strchomp(src_);
	if (!encoding)
		encoding = conv_get_charset_str(compose->out_encoding);

	conv_encode_header(dest, len, src_, header_len, addr_field, encoding);

	g_free(src_);
}

static gchar *compose_convert_filename(Compose *compose, const gchar *src,
				       const gchar *param_name,
				       const gchar *encoding)
{
	gchar *str;

	g_return_val_if_fail(src != NULL, NULL);

	if (!encoding)
		encoding = conv_get_charset_str(compose->out_encoding);

	str = conv_encode_filename(src, param_name, encoding);

	return str;
}

static void compose_generate_msgid(Compose *compose, gchar *buf, gint len)
{
	struct tm *lt;
	time_t t;
	const gchar *addr, *p;
	gchar *addr_left;
	gchar *addr_right;
	gchar hash_str[64];
	SMD5 *md5;
	gchar *md5str;

	t = time(NULL);
	lt = localtime(&t);

	if (compose->account && compose->account->address &&
	    *compose->account->address) {
		addr = compose->account->address;
		if ((p = strchr(addr, '@'))) {
			addr_left = g_strndup(addr, p - addr);
			addr_right = g_strdup(p + 1);
		} else {
			addr_left = g_strdup(addr);
			addr_right = g_strdup(get_domain_name());
		}
	} else {
		addr_left = g_strdup(g_get_user_name());
		addr_right = g_strdup(get_domain_name());
	}

	g_snprintf(hash_str, sizeof(hash_str), "%08x%s",
		   g_random_int(), addr_left);
	md5 = s_gnet_md5_new((guchar *)hash_str, strlen(hash_str));
	md5str = s_gnet_md5_get_string(md5);

	g_snprintf(buf, len, "%04d%02d%02d%02d%02d%02d.%.24s@%s",
		   lt->tm_year + 1900, lt->tm_mon + 1,
		   lt->tm_mday, lt->tm_hour,
		   lt->tm_min, lt->tm_sec,
		   md5str, addr_right);

	g_free(md5str);
	s_gnet_md5_delete(md5);
	g_free(addr_right);
	g_free(addr_left);

	debug_print("generated Message-ID: %s\n", buf);
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

	if (is_addr_entry && prefs_common.enable_address_completion)
		address_completion_register_entry(GTK_ENTRY(*entry));

	(*count)++;
}

static Compose *compose_create(PrefsAccount *account, ComposeMode mode)
{
	Compose   *compose;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *toolbar;

	GtkWidget *vbox2;

	GtkWidget *table_vbox;
	GtkWidget *table;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *from_optmenu_hbox;
	GtkWidget *sig_combo;
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

#if USE_GPGME
	GtkWidget *misc_hbox;
	GtkWidget *signing_chkbtn;
	GtkWidget *encrypt_chkbtn;
#endif /* USE_GPGME */
#if 0
	GtkWidget *attach_img;
	GtkWidget *attach_toggle;
#endif

	GtkWidget *paned;

	GtkWidget *attach_scrwin;
	GtkWidget *attach_treeview;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	GtkWidget *edit_vbox;
	GtkWidget *ruler_hbox;
	GtkWidget *ruler;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;
	GtkTextTag *sig_tag;

#if USE_GTKSPELL
	GtkWidget *spell_menu;
#endif /* USE_GTKSPELL */

	UndoMain *undostruct;

	guint n_menu_entries;
	GdkColormap *cmap;
	GdkColor color[1];
	gboolean success[1];
	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	GtkItemFactory *ifactory;
	GtkWidget *tmpl_menu;
	gint n_entries;
	gint count = 0;

#ifndef G_OS_WIN32
	static GdkGeometry geometry;
#endif

	g_return_val_if_fail(account != NULL, NULL);

	debug_print(_("Creating compose window...\n"));
	compose = g_new0(Compose, 1);

	compose->account = account;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(window), "compose", "Sylpheed");
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_size_request(window, -1, prefs_common.compose_height);
	gtkut_window_move(GTK_WINDOW(window), prefs_common.compose_x,
			  prefs_common.compose_y);

#ifndef G_OS_WIN32
	if (!geometry.max_width) {
		geometry.max_width = gdk_screen_width();
		geometry.max_height = gdk_screen_height();
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL,
				      &geometry, GDK_HINT_MAX_SIZE);
#endif

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(compose_delete_cb), compose);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	n_menu_entries = sizeof(compose_entries) / sizeof(compose_entries[0]);
	menubar = menubar_create(window, compose_entries,
				 n_menu_entries, "<Compose>", compose);
	gtk_widget_set_size_request(menubar, 300, -1);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	compose->toolbar_tip = gtk_tooltips_new();
	g_object_ref_sink(compose->toolbar_tip);
	toolbar = compose_toolbar_create(compose);
	gtk_widget_set_size_request(toolbar, 300, -1);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), BORDER_WIDTH);

	table_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), table_vbox, FALSE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(table_vbox), BORDER_WIDTH);

	table = gtk_table_new(8, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(table_vbox), table, FALSE, TRUE, 0);

	/* option menu for selecting accounts */
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(prefs_common.trans_hdr ? _("From:") : "From:");
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, count, count + 1,
			 GTK_FILL, 0, 2, 0);

	from_optmenu_hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), from_optmenu_hbox,
				  1, 2, count, count + 1);
	//gtk_table_attach(GTK_TABLE(table), from_optmenu_hbox,
			 //1, 2, count, count + 1, GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);
	gtk_widget_set_size_request(from_optmenu_hbox, 300, -1);
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 4);
	count++;
	compose_account_option_menu_create(compose, from_optmenu_hbox);
	sig_combo = compose_signature_menu_create(compose, from_optmenu_hbox);

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

#ifndef __APPLE__
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
#endif

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

#if 0
	attach_img = stock_pixbuf_widget(window, STOCK_PIXMAP_CLIP);
	attach_toggle = gtk_toggle_button_new();
	GTK_WIDGET_UNSET_FLAGS(attach_toggle, GTK_CAN_FOCUS);
	gtk_container_add(GTK_CONTAINER(attach_toggle), attach_img);
	gtk_box_pack_start(GTK_BOX(misc_hbox), attach_toggle, FALSE, FALSE, 8);
	g_signal_connect(G_OBJECT(attach_toggle), "toggled",
			 G_CALLBACK(compose_attach_toggled), compose);
#endif

#if USE_GPGME
	misc_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), misc_hbox, FALSE, FALSE, 0);

	signing_chkbtn = gtk_check_button_new_with_label(_("PGP Sign"));
	GTK_WIDGET_UNSET_FLAGS(signing_chkbtn, GTK_CAN_FOCUS);
	gtk_box_pack_start(GTK_BOX(misc_hbox), signing_chkbtn, FALSE, FALSE, 8);
	encrypt_chkbtn = gtk_check_button_new_with_label(_("PGP Encrypt"));
	GTK_WIDGET_UNSET_FLAGS(encrypt_chkbtn, GTK_CAN_FOCUS);
	gtk_box_pack_start(GTK_BOX(misc_hbox), encrypt_chkbtn, FALSE, FALSE, 8);

	g_signal_connect(G_OBJECT(signing_chkbtn), "toggled",
			 G_CALLBACK(compose_signing_toggled), compose);
	g_signal_connect(G_OBJECT(encrypt_chkbtn), "toggled",
			 G_CALLBACK(compose_encrypt_toggled), compose);
#endif /* USE_GPGME */

	/* attachment list */
	attach_scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(attach_scrwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(attach_scrwin),
					    GTK_SHADOW_IN);
	gtk_widget_set_size_request(attach_scrwin, -1, 80 * gtkut_get_dpi_multiplier());

	store = gtk_list_store_new(N_ATTACH_COLS, G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_POINTER);

	attach_treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(attach_treeview), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(attach_treeview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(attach_treeview),
					COL_NAME);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(attach_treeview), FALSE);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(attach_treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	gtk_container_add(GTK_CONTAINER(attach_scrwin), attach_treeview);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Data type"), renderer, "text", COL_MIMETYPE, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 240);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(attach_treeview), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "xalign", 1.0, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Size"), renderer, "text", COL_SIZE, NULL);
	gtk_tree_view_column_set_alignment(column, 1.0);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 64);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(attach_treeview), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Name"), renderer, "text", COL_NAME, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(attach_treeview), column);

	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(attach_selection_changed), compose);
	g_signal_connect(G_OBJECT(attach_treeview), "button_press_event",
			 G_CALLBACK(attach_button_pressed), compose);
	g_signal_connect(G_OBJECT(attach_treeview), "key_press_event",
			 G_CALLBACK(attach_key_pressed), compose);

	/* drag and drop */
	gtk_drag_dest_set(window,
			  GTK_DEST_DEFAULT_ALL, compose_drag_types,
			  N_DRAG_TYPES, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(window), "drag-data-received",
			 G_CALLBACK(compose_attach_drag_received_cb),
			 compose);

	/* pane between attach tree view and text */
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
	gtk_box_pack_start(GTK_BOX(ruler_hbox), ruler, TRUE, TRUE, 0);

	/* text widget */
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(edit_vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_size_request(scrolledwin, prefs_common.compose_width,
				    -1);

	text = gtk_text_view_new();
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), TEXTVIEW_MARGIN);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), TEXTVIEW_MARGIN);
	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_text_buffer_add_selection_clipboard(buffer, clipboard);
	sig_tag = gtk_text_buffer_create_tag(buffer, "signature", NULL);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	gtk_shruler_set_start_pos(GTK_SHRULER(ruler),
				  text->style->xthickness + TEXTVIEW_MARGIN);

	g_signal_connect(G_OBJECT(text), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
	g_signal_connect(G_OBJECT(buffer), "insert_text",
			 G_CALLBACK(text_inserted), compose);
	g_signal_connect_after(G_OBJECT(text), "size_allocate",
			       G_CALLBACK(compose_edit_size_alloc),
			       ruler);

	/* drag and drop */
	gtk_drag_dest_set(text, GTK_DEST_DEFAULT_ALL, compose_drag_types,
			  N_DRAG_TYPES, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(text), "drag-data-received",
			 G_CALLBACK(compose_insert_drag_received_cb),
			 compose);

	gtk_widget_show_all(vbox);

	if (prefs_common.textfont) {
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_from_string
			(prefs_common.textfont);
		if (font_desc) {
			gtk_widget_modify_font(text, font_desc);
			pango_font_description_free(font_desc);
		}
	}

	gtk_text_view_set_pixels_above_lines
		(GTK_TEXT_VIEW(text),
		 prefs_common.line_space - prefs_common.line_space / 2);
	gtk_text_view_set_pixels_below_lines
		(GTK_TEXT_VIEW(text), prefs_common.line_space / 2);
	gtk_text_view_set_pixels_inside_wrap
		(GTK_TEXT_VIEW(text), prefs_common.line_space);

	n_entries = sizeof(compose_popup_entries) /
		sizeof(compose_popup_entries[0]);
	popupmenu = menu_create_items(compose_popup_entries, n_entries,
				      "<Compose>", &popupfactory,
				      compose);

	ifactory = gtk_item_factory_from_widget(menubar);
	menu_set_sensitive(ifactory, "/Edit/Undo", FALSE);
	menu_set_sensitive(ifactory, "/Edit/Redo", FALSE);

	tmpl_menu = gtk_item_factory_get_item(ifactory, "/Tools/Template");

#if USE_GTKSPELL
	spell_menu = gtk_item_factory_get_item
		(ifactory, "/Tools/Set spell language");
#endif

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

#if USE_GPGME
	if (!rfc2015_is_available())
		gtk_widget_hide(misc_hbox);
#endif

	undostruct = undo_init(text);
	undo_set_change_state_func(undostruct, &compose_undo_state_changed,
				   menubar);

	address_completion_start(window);

	compose->window        = window;
	compose->vbox	       = vbox;
	compose->menubar       = menubar;
	compose->toolbar       = toolbar;

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

	compose->sig_combo        = sig_combo;

	/* compose->attach_toggle = attach_toggle; */
#if USE_GPGME
	compose->misc_hbox      = misc_hbox;
	compose->signing_chkbtn = signing_chkbtn;
	compose->encrypt_chkbtn = encrypt_chkbtn;
#endif /* USE_GPGME */

	compose->paned = paned;

	compose->attach_scrwin   = attach_scrwin;
	compose->attach_treeview = attach_treeview;
	compose->attach_store    = store;

	compose->edit_vbox     = edit_vbox;
	compose->ruler_hbox    = ruler_hbox;
	compose->ruler         = ruler;
	compose->scrolledwin   = scrolledwin;
	compose->text	       = text;

#ifdef USE_GTKSPELL
	compose->check_spell = prefs_common.check_spell;
	compose->spell_lang  = g_strdup(prefs_common.spell_lang);
	compose->spell_menu  = spell_menu;
	compose->dict_list   = NULL;
#endif /* USE_GTKSPELL */

	compose->focused_editable = NULL;

	compose->popupmenu    = popupmenu;
	compose->popupfactory = popupfactory;

	compose->tmpl_menu = tmpl_menu;

	compose->mode = mode;

	compose->targetinfo      = NULL;
	compose->reply_target    = NULL;
	compose->forward_targets = NULL;

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

	compose->out_encoding   = C_AUTO;

	compose->use_mdn        = FALSE;

#if USE_GPGME
	compose->use_signing    = FALSE;
	compose->use_encryption = FALSE;
#endif /* USE_GPGME */

	compose->modified = FALSE;

	compose->to_list        = NULL;
	compose->newsgroup_list = NULL;

	compose->undostruct = undostruct;

	compose->sig_tag = sig_tag;

	compose->exteditor_file = NULL;
	compose->exteditor_pid  = 0;
	compose->exteditor_tag  = 0;

	compose->autosave_tag = 0;

	compose->window_maximized = prefs_common.compose_maximized;

	compose->block_modified = FALSE;

	compose_set_toolbar_button_visibility(compose);

	compose_select_account(compose, account, TRUE);

	if (prefs_common.compose_maximized)
		gtk_window_maximize(GTK_WINDOW(window));

	g_signal_connect(G_OBJECT(window), "window_state_event",
			 G_CALLBACK(compose_window_state_cb), compose);

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
		menu_set_sensitive(ifactory, "/View/Attachment", FALSE);
		menu_set_sensitive(ifactory, "/Tools/Template", FALSE);
#ifndef G_OS_WIN32
		menu_set_sensitive(ifactory, "/Tools/Actions", FALSE);
#endif
		menu_set_sensitive(ifactory, "/Tools/Edit with external editor", FALSE);
#if USE_GPGME
		menu_set_sensitive(ifactory, "/Tools/PGP Sign", FALSE);
		menu_set_sensitive(ifactory, "/Tools/PGP Encrypt", FALSE);
#endif /* USE_GPGME */
#if USE_GTKSPELL
		menu_set_sensitive(ifactory, "/Tools/Check spell", FALSE);
		menu_set_sensitive(ifactory, "/Tools/Set spell language",
				   FALSE);
#endif

		if (compose->insert_btn)
			gtk_widget_set_sensitive(compose->insert_btn, FALSE);
		if (compose->attach_btn)
			gtk_widget_set_sensitive(compose->attach_btn, FALSE);
		if (compose->sig_btn)
			gtk_widget_set_sensitive(compose->sig_btn, FALSE);
		if (compose->exteditor_btn)
			gtk_widget_set_sensitive(compose->exteditor_btn, FALSE);
		if (compose->linewrap_btn)
			gtk_widget_set_sensitive(compose->linewrap_btn, FALSE);

		/* gtk_widget_set_sensitive(compose->attach_toggle, FALSE); */

		menu_set_sensitive_all(GTK_MENU_SHELL(compose->popupmenu),
				       FALSE);
	}

#if USE_GPGME
	if (!rfc2015_is_available()) {
		menu_set_sensitive(ifactory, "/Tools/PGP Sign", FALSE);
		menu_set_sensitive(ifactory, "/Tools/PGP Encrypt", FALSE);
	}
#endif /* USE_GPGME */

	compose_set_out_encoding(compose);
	addressbook_set_target_compose(compose);
#ifndef G_OS_WIN32
	action_update_compose_menu(ifactory, compose);
#endif
	compose_set_template_menu(compose);

#if USE_GTKSPELL
	compose_set_spell_lang_menu(compose);
	if (mode != COMPOSE_REDIRECT)
		menu_set_active(ifactory, "/Tools/Check spell",
				prefs_common.check_spell);
#endif

	compose_list = g_list_append(compose_list, compose);

	gtk_widget_show(window);

	color[0] = quote_color;
	cmap = gdk_window_get_colormap(window->window);
	gdk_colormap_alloc_colors(cmap, color, 1, FALSE, TRUE, success);
	if (success[0] == FALSE) {
		GtkStyle *style;

		g_warning("Compose: color allocation failed.\n");
		style = gtk_widget_get_style(text);
		quote_color = style->black;
	}

	return compose;
}

static Compose *compose_find_window_by_target(MsgInfo *msginfo)
{
	GList *cur;
	Compose *compose;

	g_return_val_if_fail(msginfo != NULL, NULL);

	for (cur = compose_list; cur != NULL; cur = cur->next) {
		compose = cur->data;
		if (procmsg_msginfo_equal(compose->targetinfo, msginfo))
			return compose;
	}

	return NULL;
}

static gboolean compose_window_exist(gint x, gint y)
{
	GList *cur;
	Compose *compose;
	gint x_, y_;

	for (cur = compose_list; cur != NULL; cur = cur->next) {
		compose = cur->data;
		gtkut_widget_get_uposition(compose->window, &x_, &y_);
		if (x == x_ && y == y_)
			return TRUE;
	}

	return FALSE;
}

static void compose_connect_changed_callbacks(Compose *compose)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer(text);

	g_signal_connect(G_OBJECT(buffer), "changed",
			 G_CALLBACK(compose_buffer_changed_cb), compose);
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

static PrefsToolbarItem items[] =
{
	{T_SEND,		TRUE, toolbar_send_cb},
	{T_SEND_LATER,		TRUE, toolbar_send_later_cb},
	{T_DRAFT,		TRUE, toolbar_draft_cb},
	{T_INSERT_FILE,		FALSE, toolbar_insert_cb},
	{T_ATTACH_FILE,		FALSE, toolbar_attach_cb},
	{T_SIGNATURE,		FALSE, toolbar_sig_cb},
	{T_EDITOR,		FALSE, toolbar_ext_editor_cb},
	{T_LINEWRAP,		FALSE, toolbar_linewrap_cb},
	{T_ADDRESS_BOOK,	FALSE, toolbar_address_cb},
	{T_COMMON_PREFS,	FALSE, toolbar_prefs_common_cb},
	{T_ACCOUNT_PREFS,	FALSE, toolbar_prefs_account_cb},

	{-1, FALSE, NULL}
};

static GtkWidget *compose_toolbar_create(Compose *compose)
{
	GtkWidget *toolbar;
	const gchar *setting;
	GList *item_list;

	if (prefs_common.compose_toolbar_setting &&
	    *prefs_common.compose_toolbar_setting != '\0')
		setting = prefs_common.compose_toolbar_setting;
	else
		setting = prefs_toolbar_get_default_compose_setting_name_list();

	item_list = prefs_toolbar_get_item_list_from_name_list(setting);
	toolbar = compose_toolbar_create_from_list(compose, item_list);
	g_list_free(item_list);

	return toolbar;
}

static GtkWidget *compose_toolbar_create_from_list(Compose *compose,
						   GList *item_list)
{
	GtkWidget *toolbar;
	GtkWidget *icon_wid;
	GtkToolItem *toolitem;
	gint i;
	GList *cur;

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
				    GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	g_signal_connect(G_OBJECT(toolbar), "button_press_event",
			 G_CALLBACK(toolbar_button_pressed), compose);

	items[0].data = &compose->send_btn;
	items[1].data = &compose->sendl_btn;
	items[2].data = &compose->draft_btn;
	items[3].data = &compose->insert_btn;
	items[4].data = &compose->attach_btn;
	items[5].data = &compose->sig_btn;
	items[6].data = &compose->exteditor_btn;
	items[7].data = &compose->linewrap_btn;
	items[8].data = &compose->addrbook_btn;
	items[9].data = &compose->prefs_common_btn;
	items[10].data = &compose->prefs_account_btn;
	for (i = 0; i <= 10; i++)
		*(GtkWidget **)items[i].data = NULL;

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
						  compose->toolbar_tip,
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
				 G_CALLBACK(item->callback), compose);
		g_signal_connect(G_OBJECT(GTK_BIN(toolitem)->child),
				 "button_press_event",
				 G_CALLBACK(toolbar_button_pressed), compose);

		*(GtkWidget **)item->data = GTK_WIDGET(toolitem);
	}

	gtk_widget_show_all(toolbar);

	return toolbar;
}

static void compose_set_toolbar_button_visibility(Compose *compose)
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
		gtk_toolbar_set_style(GTK_TOOLBAR(compose->toolbar), style);
		gtk_widget_show(compose->toolbar);
		gtk_widget_queue_resize(compose->toolbar);
	} else
		gtk_widget_hide(compose->toolbar);
}

static GtkWidget *compose_account_option_menu_create(Compose *compose,
						     GtkWidget *hbox)
{
	GList *accounts;
	GtkWidget *optmenu;
	GtkWidget *menu;
	gint num = 0, def_menu = 0;

	accounts = account_get_list();
	g_return_val_if_fail(accounts != NULL, NULL);

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

	return optmenu;
}

static GtkWidget *compose_signature_menu_create(Compose *compose,
						GtkWidget *hbox)
{
	GtkWidget *label;
	GtkWidget *combo;
	gint i;

	label = gtk_label_new(_("Signature:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

	combo = gtk_combo_box_new_text();
	gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 0);

	for (i = 0; i < 10; i++) {
		gchar buf[256];
		g_snprintf(buf, sizeof(buf), _("Signature %d"), i + 1);
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), buf);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	g_signal_connect(GTK_COMBO_BOX(combo), "changed",
			 G_CALLBACK(sig_combo_changed), compose);

	gtk_widget_set_tooltip_text(combo, _("Change signature"));

	return combo;
}

static void compose_set_out_encoding(Compose *compose)
{
	GtkItemFactoryEntry *entry;
	GtkItemFactory *ifactory;
	CharSet out_encoding;
	gchar *path, *p, *q;
	GtkWidget *item;

	out_encoding = conv_get_charset_from_str(prefs_common.outgoing_charset);
	ifactory = gtk_item_factory_from_widget(compose->menubar);

	for (entry = compose_entries; entry->callback != compose_address_cb;
	     entry++) {
		if (entry->callback == compose_set_encoding_cb &&
		    (CharSet)entry->callback_action == out_encoding) {
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

#if USE_GTKSPELL
#if USE_ENCHANT
static void ench_dict_desc_cb(const char *const lang_tag,
			      const char *const provider_name,
			      const char *const provider_desc,
			      const char *const provider_file,
			      void *user_data)
{
	GSList **dict_list = (GSList **)user_data;
	*dict_list = g_slist_append(*dict_list, g_strdup((gchar*)lang_tag));
}

static void compose_set_spell_lang_menu(Compose *compose)
{
	EnchantBroker *eb;
	GSList *dict_list = NULL, *menu_list = NULL, *cur;
	GtkWidget *menu;
	gboolean lang_set = FALSE;

	eb = enchant_broker_init();
	enchant_broker_list_dicts(eb, ench_dict_desc_cb, &dict_list);
	enchant_broker_free(eb);

	for (cur = dict_list; cur != NULL; cur = cur->next) {
		if (compose->spell_lang != NULL &&
		    g_ascii_strcasecmp(compose->spell_lang,
				       (gchar *)cur->data) == 0)
			lang_set = TRUE;
	}
#else  /* !USE_ENCHANT */
static void compose_set_spell_lang_menu(Compose *compose)
{
	AspellConfig *config;
	AspellDictInfoList *dlist;
	AspellDictInfoEnumeration *dels;
	const AspellDictInfo *entry;
	GSList *dict_list = NULL, *menu_list = NULL, *cur;
	GtkWidget *menu;
	gboolean lang_set = FALSE;

	config = new_aspell_config();
	dlist = get_aspell_dict_info_list(config);
	delete_aspell_config(config);

	dels = aspell_dict_info_list_elements(dlist);
	while ((entry = aspell_dict_info_enumeration_next(dels)) != 0) {
		dict_list = g_slist_append(dict_list, g_strdup(entry->name));
		if (compose->spell_lang != NULL &&
		    g_ascii_strcasecmp(compose->spell_lang, entry->name) == 0)
			lang_set = TRUE;
	}
	delete_aspell_dict_info_enumeration(dels);
#endif /* USE_ENCHANT */

	compose->dict_list = dict_list;

	menu = gtk_menu_new();

	for (cur = dict_list; cur != NULL; cur = cur->next) {
		gchar *dict = (gchar *)cur->data;
		GtkWidget *item;

		if (dict == NULL) continue;

		item = gtk_radio_menu_item_new_with_label(menu_list, dict);
		menu_list = gtk_radio_menu_item_get_group
			(GTK_RADIO_MENU_ITEM(item));
		if (compose->spell_lang != NULL &&
		    g_ascii_strcasecmp(compose->spell_lang, dict) == 0)
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(item), TRUE);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(compose_set_spell_lang_cb),
				 compose);     
		g_object_set_data(G_OBJECT(item), "spell-lang", dict);
		gtk_widget_show(item);

		if (!lang_set && g_ascii_strcasecmp("en", dict) == 0) {
			g_free(compose->spell_lang);
			compose->spell_lang = g_strdup("en");
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(item), TRUE);
		}
	}

	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(compose->spell_menu), menu);
}

static void compose_change_spell_lang_menu(Compose *compose, const gchar *lang)
{
	GtkWidget *menu;
	GtkWidget *def_item = NULL;
	GList *cur_item;
	const gchar *dict;

	if (!lang)
		return;

	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(compose->spell_menu));
	for (cur_item = GTK_MENU_SHELL(menu)->children; cur_item != NULL;
	     cur_item = cur_item->next) {
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(cur_item->data)))
			def_item = GTK_WIDGET(cur_item->data);
		dict = g_object_get_data(G_OBJECT(cur_item->data), "spell-lang");
		if (dict && !g_ascii_strcasecmp(dict, lang)) {
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(cur_item->data), TRUE);
			return;
		}
	}

	if (def_item) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(def_item),
					       TRUE);
		compose_set_spell_lang_cb(def_item, compose);
	}
}
#endif /* USE_GTKSPELL */

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

		if (compose->autosave_tag > 0) {
			g_source_remove(compose->autosave_tag);
			compose->autosave_tag = 0;
		}

		compose_set_template_menu(compose);

		if (prefs_common.enable_autosave &&
		    prefs_common.autosave_itv > 0 &&
		    compose->mode != COMPOSE_REDIRECT)
			compose->autosave_tag =
				g_timeout_add_full
					(G_PRIORITY_LOW,
					 prefs_common.autosave_itv * 60 * 1000,
					 autosave_timeout, compose, NULL);
	}
}

GtkWidget *compose_get_toolbar(Compose *compose)
{
	g_return_val_if_fail(compose != NULL, NULL);
	return compose->toolbar;
}

GtkWidget *compose_get_misc_hbox(Compose *compose)
{
	g_return_val_if_fail(compose != NULL, NULL);
	return compose->misc_hbox;
}

GtkWidget *compose_get_textview(Compose *compose)
{
	g_return_val_if_fail(compose != NULL, NULL);
	return compose->text;
}

static void compose_template_apply(Compose *compose, Template *tmpl,
				   gboolean replace)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	gchar *parsed_str = NULL;

	if (!tmpl || !tmpl->value) return;

	buffer = gtk_text_view_get_buffer(text);

	if (tmpl->to && *tmpl->to != '\0')
		compose_entry_set(compose, tmpl->to, COMPOSE_ENTRY_TO);
	if (tmpl->cc && *tmpl->cc != '\0')
		compose_entry_set(compose, tmpl->cc, COMPOSE_ENTRY_CC);
	if (tmpl->bcc && *tmpl->bcc != '\0')
		compose_entry_set(compose, tmpl->bcc, COMPOSE_ENTRY_BCC);
	if (tmpl->replyto && *tmpl->replyto != '\0')
		compose_entry_set(compose, tmpl->replyto,
				  COMPOSE_ENTRY_REPLY_TO);
	if (tmpl->subject && *tmpl->subject != '\0')
		compose_entry_set(compose, tmpl->subject,
				  COMPOSE_ENTRY_SUBJECT);

	if (replace)
		gtk_text_buffer_set_text(buffer, "", 0);

	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	if (compose->reply_target) {
		FolderItem *item;
		gint num;
		MsgInfo *msginfo = NULL;
		MsgInfo *full_msginfo;

		item = folder_find_item_and_num_from_id(compose->reply_target,
							&num);
		if (item && num > 0) {
			msginfo = folder_item_get_msginfo(item, num);
			if (msginfo) {
				full_msginfo = procmsg_msginfo_get_full_info(msginfo);
				if (full_msginfo) {
					procmsg_msginfo_free(msginfo);
					msginfo = full_msginfo;
				}
			}
		}
		parsed_str = compose_quote_fmt(compose, msginfo,
					       tmpl->value,
					       prefs_common.quotemark, NULL);
		procmsg_msginfo_free(msginfo);
	} else if (compose->forward_targets) {
		FolderItem *item;
		gint num;
		gchar **targets;
		gint i;
		MsgInfo *msginfo;
		MsgInfo *full_msginfo;

		targets = g_strsplit(compose->forward_targets, "\n", 0);

		for (i = 0; targets[i] != NULL; i++) {
			g_strstrip(targets[i]);
			item = folder_find_item_and_num_from_id(targets[i], &num);
			if (!item || num <= 0)
				continue;
			msginfo = procmsg_get_msginfo(item, num);
			if (!msginfo)
				continue;
			full_msginfo = procmsg_msginfo_get_full_info(msginfo);
			parsed_str = compose_quote_fmt(compose, full_msginfo ? full_msginfo : msginfo,
						       tmpl->value,
						       prefs_common.fw_quotemark, NULL);
			procmsg_msginfo_free(full_msginfo);
			procmsg_msginfo_free(msginfo);
		}
		g_strfreev(targets);
	} else {
		parsed_str = compose_quote_fmt(compose, NULL, tmpl->value,
					       NULL, NULL);
	}

	if (replace && parsed_str && prefs_common.auto_sig)
		compose_insert_sig(compose, TRUE, FALSE, FALSE);

	if (replace && parsed_str) {
		gtk_text_buffer_get_start_iter(buffer, &iter);
		gtk_text_buffer_place_cursor(buffer, &iter);
	}

	if (parsed_str)
		compose_changed_cb(NULL, compose);
}

static void compose_destroy(Compose *compose)
{
	GtkTreeModel *model = GTK_TREE_MODEL(compose->attach_store);
	GtkTreeIter iter;
	gboolean valid;
	AttachInfo *ainfo;
	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;

	compose_list = g_list_remove(compose_list, compose);

	if (compose->autosave_tag > 0)
		g_source_remove(compose->autosave_tag);

	syl_plugin_signal_emit("compose-destroy", compose);

	/* NOTE: address_completion_end() does nothing with the window
	 * however this may change. */
	address_completion_end(compose->window);

#if USE_GTKSPELL
	slist_free_strings(compose->dict_list);
	g_slist_free(compose->dict_list);
	g_free(compose->spell_lang);
#endif

	slist_free_strings(compose->to_list);
	g_slist_free(compose->to_list);
	slist_free_strings(compose->newsgroup_list);
	g_slist_free(compose->newsgroup_list);

	procmsg_msginfo_free(compose->targetinfo);
	g_free(compose->reply_target);
	g_free(compose->forward_targets);
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

	if (compose->exteditor_file) {
		g_unlink(compose->exteditor_file);
		g_free(compose->exteditor_file);
	}

	for (valid = gtk_tree_model_get_iter_first(model, &iter); valid;
	     valid = gtk_tree_model_iter_next(model, &iter)) {
		gtk_tree_model_get(model, &iter, COL_ATTACH_INFO, &ainfo, -1);
		compose_attach_info_free(ainfo);
	}

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(NULL);

	prefs_common.compose_maximized = compose->window_maximized;
	if (!prefs_common.compose_maximized) {
		gtkut_widget_get_uposition(compose->window,
					   &prefs_common.compose_x,
					   &prefs_common.compose_y);
		prefs_common.compose_width =
			compose->scrolledwin->allocation.width;
		prefs_common.compose_height =
			compose->window->allocation.height;
	}

	if (!gtk_widget_get_parent(compose->paned))
		gtk_widget_destroy(compose->paned);
	gtk_widget_destroy(compose->popupmenu);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(compose->text));
	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_text_buffer_remove_selection_clipboard(buffer, clipboard);

	g_object_unref(compose->toolbar_tip);

	gtk_widget_destroy(compose->window);

	g_free(compose);
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
	GtkTreeModel *model = GTK_TREE_MODEL(compose->attach_store);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *rows, *cur;
	AttachInfo *ainfo;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(compose->attach_treeview));

	rows = gtk_tree_selection_get_selected_rows(selection, NULL);

	/* delete from below so that GtkTreePath doesn't point wrong row */
	rows = g_list_reverse(rows);

	for (cur = rows; cur != NULL; cur = cur->next) {
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)cur->data);
		gtk_tree_model_get(model, &iter, COL_ATTACH_INFO, &ainfo, -1);
		compose_attach_info_free(ainfo);
		gtk_list_store_remove(compose->attach_store, &iter);
		gtk_tree_path_free((GtkTreePath *)cur->data);
	}

	g_list_free(rows);

	syl_plugin_signal_emit("compose-attach-changed", compose);
}

void compose_attach_remove_all(Compose *compose)
{
	GtkTreeModel *model = GTK_TREE_MODEL(compose->attach_store);
	GtkTreeIter iter;
	AttachInfo *ainfo;

	g_return_if_fail(compose != NULL);

	if (gtk_tree_model_get_iter_first(model, &iter) == FALSE)
		return;

	do {
		gtk_tree_model_get(model, &iter, COL_ATTACH_INFO, &ainfo, -1);
		compose_attach_info_free(ainfo);
	} while (gtk_tree_model_iter_next(model, &iter) == TRUE);

	gtk_list_store_clear(GTK_LIST_STORE(model));

	syl_plugin_signal_emit("compose-attach-changed", compose);
}

GSList *compose_get_attach_list(Compose *compose)
{
	GtkTreeModel *model = GTK_TREE_MODEL(compose->attach_store);
	GtkTreeIter iter;
	gboolean valid;
	AttachInfo *ainfo;
	GSList *alist = NULL;

	if (!compose->use_attach)
		return NULL;

	for (valid = gtk_tree_model_get_iter_first(model, &iter); valid;
	     valid = gtk_tree_model_iter_next(model, &iter)) {
		gtk_tree_model_get(model, &iter, COL_ATTACH_INFO, &ainfo, -1);
		alist = g_slist_append(alist, ainfo);
	}

	return alist;
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
	GtkTreeModel *model = GTK_TREE_MODEL(compose->attach_store);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *rows;
	AttachInfo *ainfo;
	gchar *path = NULL;
	GtkOptionMenu *optmenu;
	static gboolean cancelled;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(compose->attach_treeview));

	rows = gtk_tree_selection_get_selected_rows(selection, NULL);

	if (!rows)
		return;

	gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)rows->data);
	gtk_tree_model_get(model, &iter, COL_ATTACH_INFO, &ainfo, -1);

	g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(rows);

	compose_attach_property_create(&cancelled);
	gtk_widget_grab_focus(attach_prop.ok_btn);
	gtk_widget_show(attach_prop.window);
	manage_window_focus_in(compose->window, NULL, NULL);
	manage_window_set_transient(GTK_WINDOW(attach_prop.window));

	optmenu = GTK_OPTION_MENU(attach_prop.encoding_optmenu);
	if (ainfo->encoding == ENC_UNKNOWN)
		gtk_option_menu_set_history(optmenu, ENC_BASE64);
	else
		gtk_option_menu_set_history(optmenu, ainfo->encoding);

	if (ainfo->file)
		path = conv_filename_to_utf8(ainfo->file);

	gtk_entry_set_text(GTK_ENTRY(attach_prop.mimetype_entry),
			   ainfo->content_type ? ainfo->content_type : "");
	gtk_entry_set_text(GTK_ENTRY(attach_prop.path_entry), path ? path : "");
	gtk_entry_set_text(GTK_ENTRY(attach_prop.filename_entry),
			   ainfo->name ? ainfo->name : "");

	g_free(path);

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

		if (cancelled == TRUE)
			break;

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
			file = conv_filename_from_utf8(entry_text);
			if (!is_file_exist(file) ||
			    (size = get_file_size(file)) <= 0) {
				alertpanel_error
					(_("File doesn't exist or is empty."));
				g_free(file);
				g_free(cnttype);
				continue;
			}
			g_free(ainfo->file);
			ainfo->file = file;
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
		if (size)
			ainfo->size = size;

		gtk_list_store_set(compose->attach_store, &iter,
				   COL_MIMETYPE, ainfo->content_type,
				   COL_SIZE, to_human_readable(ainfo->size),
				   COL_NAME, ainfo->name,
				   -1);
		break;
	}

	gtk_widget_destroy(attach_prop.window);
	memset(&attach_prop, 0, sizeof(attach_prop));
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

	debug_print("Creating attach property window...\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, 480 * gtkut_get_dpi_multiplier(), -1);
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

static void compose_attach_open(Compose *compose)
{
	GtkTreeModel *model = GTK_TREE_MODEL(compose->attach_store);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *rows;
	AttachInfo *ainfo = NULL;
#ifdef G_OS_WIN32
	DWORD dwtype;
#endif

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(compose->attach_treeview));

	rows = gtk_tree_selection_get_selected_rows(selection, NULL);

	if (!rows)
		return;

	gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)rows->data);
	gtk_tree_model_get(model, &iter, COL_ATTACH_INFO, &ainfo, -1);

	g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(rows);

	if (!ainfo || !ainfo->file)
		return;

	if (!is_file_exist(ainfo->file)) {
		alertpanel_error(_("File not exist."));
		return;
	}

#ifdef G_OS_WIN32
	if (g_file_test(ainfo->file, G_FILE_TEST_IS_EXECUTABLE) ||
	    str_has_suffix_case(ainfo->file, ".scr") ||
	    str_has_suffix_case(ainfo->file, ".pif") ||
	    GetBinaryType(ainfo->file, &dwtype)) {
		alertpanel_full
			(_("Opening executable file"),
			 _("This is an executable file. Opening executable file is restricted for security.\n"
			   "If you want to launch it, save it to somewhere and make sure it is not an virus or something like a malicious program."),
			 ALERT_WARNING, G_ALERTDEFAULT, FALSE,
			 GTK_STOCK_OK, NULL, NULL);
		return;
	}
	execute_open_file(ainfo->file, ainfo->content_type);
#else
	procmime_execute_open_file(ainfo->file, ainfo->content_type);
#endif
}

static void compose_exec_ext_editor(Compose *compose)
{
	gchar *tmp;
	GPid pid;
	static gchar *def_cmd = "emacs %s";
	gchar buf[1024];
	gchar **cmdline;
	GError *error = NULL;

	tmp = g_strdup_printf("%s%ctmpmsg-%p.txt", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);

	if (compose_write_body_to_file(compose, tmp) < 0) {
		g_warning("Coundn't write to file: %s\n", tmp);
		g_free(tmp);
		return;
	}
#ifdef G_OS_WIN32
	if (canonicalize_file_replace(tmp) < 0) {
		g_warning("Coundn't write to file: %s\n", tmp);
		g_free(tmp);
		return;
	}
#endif

	if (prefs_common.ext_editor_cmd &&
	    str_find_format_times(prefs_common.ext_editor_cmd, 's') == 1)
		g_snprintf(buf, sizeof(buf), prefs_common.ext_editor_cmd, tmp);
	else {
		if (prefs_common.ext_editor_cmd)
			g_warning(_("External editor command line is invalid: `%s'\n"),
				  prefs_common.ext_editor_cmd);
		g_snprintf(buf, sizeof(buf), def_cmd, tmp);
	}

	cmdline = strsplit_with_quote(buf, " ", 1024);

	if (g_spawn_async(NULL, cmdline, NULL,
			  G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
			  NULL, NULL, &pid, &error) == FALSE) {
		g_warning("Couldn't execute external editor: %s\n", buf);
		if (error) {
			g_warning("g_spawn_async(): %s\n", error->message);
			g_error_free(error);
		}
		g_strfreev(cmdline);
		g_unlink(tmp);
		g_free(tmp);
		return;
	}
	if (pid == 0) {
		g_warning("Couldn't get PID of external editor\n");
		g_strfreev(cmdline);
		g_unlink(tmp);
		g_free(tmp);
		return;
	}

	g_strfreev(cmdline);

	compose_set_ext_editor_sensitive(compose, FALSE);

	debug_print("compose_exec_ext_editor(): pid: %d file: %s\n", pid, tmp);

	compose->exteditor_file = tmp;
	compose->exteditor_pid = pid;
	compose->exteditor_tag =
		g_child_watch_add(pid, compose_ext_editor_child_exit, compose);
}

static gboolean compose_ext_editor_kill(Compose *compose)
{
#ifdef G_OS_WIN32
	DWORD exitcode;
#endif
	gint ret;

	g_return_val_if_fail(compose->exteditor_pid != 0, TRUE);

#ifdef G_OS_WIN32
	ret = GetExitCodeProcess(compose->exteditor_pid, &exitcode);

	if (ret && exitcode == STILL_ACTIVE) {
#else
	ret = kill(compose->exteditor_pid, 0);

	if (ret == 0 || (ret == -1 && EPERM == errno)) {
#endif
		AlertValue val;
		gchar *msg;

		msg = g_strdup_printf
			(_("The external editor is still working.\n"
			   "Force terminating the process (pid: %d)?\n"),
			 compose->exteditor_pid);
		val = alertpanel_full(_("Notice"), msg, ALERT_NOTICE,
				      G_ALERTALTERNATE, FALSE,
				      GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		g_free(msg);

		if (val != G_ALERTDEFAULT)
			return FALSE;
	}

	if (compose->exteditor_tag != 0) {
		g_source_remove(compose->exteditor_tag);
		compose->exteditor_tag = 0;
	}

	if (compose->exteditor_pid != 0) {
#ifdef G_OS_WIN32
		if (TerminateProcess(compose->exteditor_pid, 1) == 0)
			g_warning("TerminateProcess() failed: %d\n",
				  GetLastError());
#else
		if (kill(compose->exteditor_pid, SIGTERM) < 0)
			perror("kill");
#endif
		g_message("Terminated process group id: %d\n",
			  compose->exteditor_pid);
		g_message("Temporary file: %s\n",
			  compose->exteditor_file);
		compose->exteditor_pid = 0;
	}

	if (compose->exteditor_file) {
		g_unlink(compose->exteditor_file);
		g_free(compose->exteditor_file);
		compose->exteditor_file = NULL;
	}

	compose_set_ext_editor_sensitive(compose, TRUE);

	return TRUE;
}

static void compose_ext_editor_child_exit(GPid pid, gint status, gpointer data)
{
	Compose *compose = (Compose *)data;
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;

	debug_print("Compose: child exit (pid: %d status: %d)\n", pid, status);

	compose_lock(compose);

	g_spawn_close_pid(pid);

	buffer = gtk_text_view_get_buffer(text);

	gtk_text_buffer_set_text(buffer, "", 0);
	compose_insert_file(compose, compose->exteditor_file, FALSE);
	compose_enable_sig(compose);

	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_place_cursor(buffer, &iter);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_view_scroll_mark_onscreen(text, mark);

	compose_changed_cb(NULL, compose);

	if (g_unlink(compose->exteditor_file) < 0)
		FILE_OP_ERROR(compose->exteditor_file, "unlink");

	compose_set_ext_editor_sensitive(compose, TRUE);
	gtk_window_present(GTK_WINDOW(compose->window));

	g_free(compose->exteditor_file);
	compose->exteditor_file = NULL;
	compose->exteditor_pid  = 0;
	compose->exteditor_tag  = 0;

	compose_unlock(compose);
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
	menu_set_sensitive(ifactory, "/File/Append signature", sensitive);
	menu_set_sensitive(ifactory, "/Edit/Wrap current paragraph", sensitive);
	menu_set_sensitive(ifactory, "/Edit/Wrap all long lines", sensitive);
	menu_set_sensitive(ifactory, "/Tools/Edit with external editor",
			   sensitive);

#define SET_SENS(w) \
	if (compose->w) \
		gtk_widget_set_sensitive(compose->w, sensitive);

	SET_SENS(text);
	SET_SENS(send_btn);
	SET_SENS(sendl_btn);
	SET_SENS(draft_btn);
	SET_SENS(insert_btn);
	SET_SENS(sig_btn);
	SET_SENS(exteditor_btn);
	SET_SENS(linewrap_btn);

#undef SET_SENS
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
	compose_send((Compose *)data, TRUE);
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

	compose_insert_sig(compose, TRUE, TRUE, TRUE);
}

static void toolbar_ext_editor_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;

	compose_exec_ext_editor(compose);
}

static void toolbar_linewrap_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;

	compose_wrap_all(compose);
}

static void toolbar_address_cb(GtkWidget *widget, gpointer data)
{
	compose_address_cb(data, 0, NULL);
}

static void toolbar_prefs_common_cb(GtkWidget *widget, gpointer data)
{
	prefs_common_open();
}

static void toolbar_prefs_account_cb(GtkWidget *widget, gpointer data)
{
	account_open(cur_account);
}

static void toolbar_customize(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;
	gint *visible_items;
	GList *item_list = NULL;
	GtkWidget *toolbar;
	gint ret;
	const gchar *setting;

	if (prefs_common.compose_toolbar_setting &&
	    *prefs_common.compose_toolbar_setting != '\0')
		setting = prefs_common.compose_toolbar_setting;
	else
		setting = prefs_toolbar_get_default_compose_setting_name_list();
	visible_items = prefs_toolbar_get_id_list_from_name_list(setting);
	ret = prefs_toolbar_open(TOOLBAR_COMPOSE, visible_items, &item_list);
	g_free(visible_items);

	if (ret == 0) {
		gtk_widget_destroy(compose->toolbar);
		toolbar = compose_toolbar_create_from_list(compose, item_list);
		gtk_widget_set_size_request(toolbar, 300, -1);
		gtk_box_pack_start(GTK_BOX(compose->vbox), toolbar,
				   FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(compose->vbox), toolbar, 1);
		compose->toolbar = toolbar;
		compose_set_toolbar_button_visibility(compose);
		g_free(prefs_common.compose_toolbar_setting);
		prefs_common.compose_toolbar_setting =
			prefs_toolbar_get_name_list_from_item_list(item_list);
		g_list_free(item_list);
		prefs_common_write_config();

		syl_plugin_signal_emit("compose-toolbar-changed", compose);
	}
}

static gboolean toolbar_button_pressed(GtkWidget *widget, GdkEventButton *event,
				       gpointer data)
{
	Compose *compose = (Compose *)data;
	GtkWidget *menu;
	GtkWidget *menuitem;

	if (!event) return FALSE;
	if (event->button != 3) return FALSE;

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	MENUITEM_ADD_WITH_MNEMONIC(menu, menuitem, _("_Customize toolbar..."),
				   0);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(toolbar_customize), compose);
	g_signal_connect(G_OBJECT(menu), "selection_done",
			 G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		       event->button, event->time);

	return TRUE;
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

static void sig_combo_changed(GtkComboBox *combo, gpointer data)
{
	Compose *compose = (Compose *)data;

	if (compose->mode != COMPOSE_REDIRECT && prefs_common.auto_sig)
		compose_insert_sig(compose, TRUE, TRUE, FALSE);
}

static void attach_selection_changed(GtkTreeSelection *selection, gpointer data)
{
}

static gboolean attach_button_pressed(GtkWidget *widget, GdkEventButton *event,
				      gpointer data)
{
	Compose *compose = (Compose *)data;
	GtkTreeView *treeview = GTK_TREE_VIEW(compose->attach_treeview);
	GtkTreeSelection *selection;
	GtkTreePath *path = NULL;

	if (!event) return FALSE;

	gtk_tree_view_get_path_at_pos(treeview, event->x, event->y,
				      &path, NULL, NULL, NULL);

	if (event->button == 2 && path)
		gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);

	if (event->button == 2 ||
	    (event->button == 1 && event->type == GDK_2BUTTON_PRESS)) {
		compose_attach_property(compose);
	} else if (event->button == 3) {
		GList *rows;
		gboolean has_selection = FALSE;

		selection = gtk_tree_view_get_selection(treeview);
		rows = gtk_tree_selection_get_selected_rows(selection, NULL);
		if (rows) {
			has_selection = TRUE;
			g_list_free(rows);
		}
		if (path)
			has_selection = TRUE;
		menu_set_sensitive(compose->popupfactory, "/Open", has_selection);
		menu_set_sensitive(compose->popupfactory, "/Add...", TRUE);
		menu_set_sensitive(compose->popupfactory, "/Remove", has_selection);
		menu_set_sensitive(compose->popupfactory, "/Properties...", has_selection);
		gtk_menu_popup(GTK_MENU(compose->popupmenu), NULL, NULL,
			       NULL, NULL, event->button, event->time);

		if (path &&
		    gtk_tree_selection_path_is_selected(selection, path)) {
			gtk_tree_path_free(path);
			return TRUE;
		}
	}

	gtk_tree_path_free(path);
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

gint compose_send(Compose *compose, gboolean close_on_success)
{
	gint val;

	if (compose->lock_count > 0)
		return 1;

	gtk_widget_set_sensitive(compose->vbox, FALSE);
	val = compose_send_real(compose);
	gtk_widget_set_sensitive(compose->vbox, TRUE);

	if (val == 0 && close_on_success)
		compose_destroy(compose);

	return val;
}

static void compose_send_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose_send(compose, TRUE);
}

static void compose_send_later_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	FolderItem *queue;
	gchar tmp[MAXPATHLEN + 1];
	gboolean cancel = FALSE;

	if (compose->lock_count > 0)
		return;

	C_LOCK();

	if (compose_check_entries(compose) == FALSE) {
		C_UNLOCK();
		return;
	}
	if (compose_check_attachments(compose) == FALSE) {
		C_UNLOCK();
		return;
	}
	if (compose_check_recipients(compose) == FALSE) {
		C_UNLOCK();
		return;
	}

	queue = account_get_special_folder(compose->account, F_QUEUE);
	if (!queue) {
		g_warning("can't find queue folder\n");
		C_UNLOCK();
		return;
	}

	if (!FOLDER_IS_LOCAL(queue->folder)) {
		if (compose_check_activities(compose) == FALSE) {
			C_UNLOCK();
			return;
		}
		if (!main_window_toggle_online_if_offline(main_window_get())) {
			C_UNLOCK();
			return;
		}
	}

	g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg.%p",
		   get_tmp_dir(), G_DIR_SEPARATOR, compose);

	if (compose->mode == COMPOSE_REDIRECT) {
		if (compose_redirect_write_to_file(compose, tmp) < 0) {
			alertpanel_error(_("Can't queue the message."));
			C_UNLOCK();
			return;
		}
	} else {
		if (compose_write_to_file(compose, tmp, FALSE) < 0) {
			alertpanel_error(_("Can't queue the message."));
			C_UNLOCK();
			return;
		}
	}

	if (!compose->to_list && !compose->newsgroup_list) {
		g_warning("can't get recipient list.");
		g_unlink(tmp);
		C_UNLOCK();
		return;
	}

	syl_plugin_signal_emit("compose-send", compose, compose->mode, 1,
			       tmp, compose->to_list, &cancel);
	if (cancel) {
		g_unlink(tmp);
		C_UNLOCK();
		return;
	}

	if (compose_queue(compose, tmp) < 0) {
		alertpanel_error(_("Can't queue the message."));
		g_unlink(tmp);
		C_UNLOCK();
		return;
	}

	if (g_unlink(tmp) < 0)
		FILE_OP_ERROR(tmp, "unlink");

	C_UNLOCK();
	compose_destroy(compose);
}

static void compose_draft_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	FolderItem *draft;
	gchar *tmp;
	gint msgnum;
	MsgFlags flag = {0, 0};

	if (compose->lock_count > 0)
		return;

	draft = account_get_special_folder(compose->account, F_DRAFT);
	g_return_if_fail(draft != NULL);

	C_LOCK();

	if (!FOLDER_IS_LOCAL(draft->folder)) {
		if (compose_check_activities(compose) == FALSE) {
			C_UNLOCK();
			return;
		}
		if (!main_window_toggle_online_if_offline(main_window_get())) {
			C_UNLOCK();
			return;
		}
	}

	tmp = g_strdup_printf("%s%cdraft.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);

	if (compose_write_to_file(compose, tmp, TRUE) < 0) {
		g_free(tmp);
		C_UNLOCK();
		return;
	}

	folder_item_scan(draft);
	if ((msgnum = folder_item_add_msg(draft, tmp, &flag, TRUE)) < 0) {
		g_unlink(tmp);
		g_free(tmp);
		C_UNLOCK();
		return;
	}
	g_free(tmp);

	if (compose->mode == COMPOSE_REEDIT) {
		compose_remove_reedit_target(compose);
		if (compose->targetinfo &&
		    compose->targetinfo->folder != draft)
			folderview_update_item(compose->targetinfo->folder,
					       TRUE);
	}

	folder_item_scan(draft);
	folderview_update_item(draft, TRUE);

	/* 0: quit editing  1: keep editing */
	if (action == 0) {
		C_UNLOCK();
		compose_destroy(compose);
	} else {
		GStatBuf s;
		gchar *path;

		path = folder_item_fetch_msg(draft, msgnum);
		C_UNLOCK();
		g_return_if_fail(path != NULL);
		if (g_stat(path, &s) < 0) {
			FILE_OP_ERROR(path, "stat");
			g_free(path);
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
		compose->modified = FALSE;
		compose_set_title(compose);
	}
}

static void compose_attach_open_cb(gpointer data, guint action,
				   GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose_attach_open(compose);
}

static void compose_attach_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	GSList *files;
	GSList *cur;

	files = filesel_select_files(_("Select files"), NULL,
				     GTK_FILE_CHOOSER_ACTION_OPEN);

	for (cur = files; cur != NULL; cur = cur->next) {
		gchar *file = (gchar *)cur->data;
		gchar *utf8_filename;

		utf8_filename = conv_filename_to_utf8(file);
		compose_attach_append(compose, file, utf8_filename, NULL);
		compose_changed_cb(NULL, compose);
		g_free(utf8_filename);
		g_free(file);
	}

	g_slist_free(files);
}

static void compose_insert_file_cb(gpointer data, guint action,
				   GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	gchar *file;

	file = filesel_select_file(_("Select file"), NULL,
				   GTK_FILE_CHOOSER_ACTION_OPEN);

	if (file && *file)
		compose_insert_file(compose, file, TRUE);

	g_free(file);
}

static void compose_insert_sig_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose_insert_sig(compose, action, TRUE, TRUE);
}

static gint compose_delete_cb(GtkWidget *widget, GdkEventAny *event,
			      gpointer data)
{
	compose_close_cb(data, 0, NULL);
	return TRUE;
}

static gint compose_window_state_cb(GtkWidget *widget,
				    GdkEventWindowState *event,
				    gpointer data)
{
	Compose *compose = (Compose *)data;

	if ((event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) != 0) {
		if ((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0)
			compose->window_maximized = TRUE;
		else
			compose->window_maximized = FALSE;
	}

	return FALSE;
}

static void compose_close_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	AlertValue val;

	if (compose->lock_count > 0)
		return;

	if (compose->exteditor_pid != 0) {
		if (!compose_ext_editor_kill(compose))
			return;
	}

	if (compose->modified) {
		val = alertpanel(_("Save message"),
				 _("This message has been modified. Save it to draft folder?"),
#ifdef G_OS_WIN32
				 GTK_STOCK_SAVE, _("Close _without saving"),
				 GTK_STOCK_CANCEL);
#else
				 GTK_STOCK_SAVE, GTK_STOCK_CANCEL,
				 _("Close _without saving"));
#endif

		switch (val) {
		case G_ALERTDEFAULT:
			compose_draft_cb(data, 0, NULL);
			return;
#ifdef G_OS_WIN32
		case G_ALERTALTERNATE:
#else
		case G_ALERTOTHER:
#endif
			break;
		default:
			return;
		}
	}

	compose_destroy(compose);
}

static void compose_set_encoding_cb(gpointer data, guint action,
				    GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->out_encoding = (CharSet)action;
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
			 _("_Replace"), _("_Insert"), GTK_STOCK_CANCEL);
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

static void compose_undo_cb(Compose *compose)
{
	gboolean prev_autowrap = compose->autowrap;

	compose->autowrap = FALSE;
	undo_undo(compose->undostruct);
	compose->autowrap = prev_autowrap;
}

static void compose_redo_cb(Compose *compose)
{
	gboolean prev_autowrap = compose->autowrap;

	compose->autowrap = FALSE;
	undo_redo(compose->undostruct);
	compose->autowrap = prev_autowrap;
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
			GtkTextMark *mark;
			GtkClipboard *clipboard;

			buffer = gtk_text_view_get_buffer(text);
			mark = gtk_text_buffer_get_insert(buffer);
			clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

			gtk_text_buffer_paste_clipboard(buffer, clipboard,
							NULL, TRUE);

			gtk_text_view_scroll_mark_onscreen(text, mark);
		}
	}
}

static void compose_paste_as_quote_cb(Compose *compose)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkClipboard *clipboard;
	gchar *str = NULL;

	if (!compose->focused_editable ||
	    !GTK_WIDGET_HAS_FOCUS(compose->focused_editable) ||
	    !GTK_IS_TEXT_VIEW(compose->focused_editable))
			return;

	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	str = gtk_clipboard_wait_for_text(clipboard);
	if (!str)
		return;

	compose_quote_fmt(compose, NULL, "%Q", prefs_common.quotemark, str);

	g_free(str);

	gtk_text_view_scroll_mark_onscreen(text, mark);
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

static void compose_grab_focus_cb(GtkWidget *widget, Compose *compose)
{
	if (GTK_IS_EDITABLE(widget) || GTK_IS_TEXT_VIEW(widget))
		compose->focused_editable = widget;
}

#if USE_GPGME
static void compose_signing_toggled(GtkWidget *widget, Compose *compose)
{
	GtkItemFactory *ifactory;

	if (!rfc2015_is_available())
		return;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		compose->use_signing = TRUE;
	else
		compose->use_signing = FALSE;

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	menu_set_active(ifactory, "/Tools/PGP Sign", compose->use_signing);
}

static void compose_encrypt_toggled(GtkWidget *widget, Compose *compose)
{
	GtkItemFactory *ifactory;

	if (!rfc2015_is_available())
		return;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		compose->use_encryption = TRUE;
	else
		compose->use_encryption = FALSE;

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	menu_set_active(ifactory, "/Tools/PGP Encrypt",
			compose->use_encryption);
}
#endif /* USE_GPGME */

#if 0
static void compose_attach_toggled(GtkWidget *widget, Compose *compose)
{
	GtkItemFactory *ifactory;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		compose->use_attach = TRUE;
	else
		compose->use_attach = FALSE;

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	menu_set_active(ifactory, "/View/Attachment", compose->use_attach);
}
#endif

static void compose_buffer_changed_cb(GtkTextBuffer *textbuf, Compose *compose)
{
	if (compose->modified == FALSE && compose->block_modified == FALSE) {
		compose->modified = TRUE;
		compose_set_title(compose);
	}
}

static void compose_changed_cb(GtkEditable *editable, Compose *compose)
{
	if (compose->block_modified == FALSE &&
	    (compose->modified == FALSE ||
	     editable == GTK_EDITABLE(compose->subject_entry))) {
		compose->modified = TRUE;
		compose_set_title(compose);
	}
}

static void compose_wrap_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (action == 1)
		compose_wrap_all(compose);
	else
		compose_wrap_paragraph(compose, NULL);
}

static void compose_toggle_autowrap_cb(gpointer data, guint action,
				       GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose->autowrap = GTK_CHECK_MENU_ITEM(widget)->active;
	if (compose->autowrap)
		compose_wrap_all_full(compose, TRUE);
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

#if 0
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->attach_toggle),
				     compose->use_attach);
#endif

	syl_plugin_signal_emit("compose-attach-changed", compose);
}

static void compose_customize_toolbar_cb(gpointer data, guint action,
					 GtkWidget *widget)
{
	toolbar_customize(widget, data);
}

static void compose_toggle_mdn_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->use_mdn = TRUE;
	else
		compose->use_mdn = FALSE;
}

#if USE_GPGME
static void compose_toggle_sign_cb(gpointer data, guint action,
				   GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (!rfc2015_is_available())
		return;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->use_signing = TRUE;
	else
		compose->use_signing = FALSE;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->signing_chkbtn),
				     compose->use_signing);
}

static void compose_toggle_encrypt_cb(gpointer data, guint action,
				      GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (!rfc2015_is_available())
		return;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->use_encryption = TRUE;
	else
		compose->use_encryption = FALSE;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->encrypt_chkbtn),
				     compose->use_encryption);
}
#endif /* USE_GPGME */

#if USE_GTKSPELL
static void compose_toggle_spell_cb(gpointer data, guint action,
				    GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	GtkSpell *speller;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		debug_print("Spell checking enabled: %s\n",
			    compose->spell_lang ? compose->spell_lang : "(none)");
		speller = gtkspell_new_attach(GTK_TEXT_VIEW(compose->text),
					      compose->spell_lang, NULL);
		compose->check_spell = TRUE;
	} else {
		debug_print("Spell checking disabled\n");
		speller = gtkspell_get_from_text_view
			(GTK_TEXT_VIEW(compose->text));
		if (speller != NULL)
			gtkspell_detach(speller);
		compose->check_spell = FALSE;
	}
}

static void compose_set_spell_lang_cb(GtkWidget *widget,
				      gpointer data)
{
	Compose *compose = (Compose *)data;
	gchar *dict;
	GtkSpell *speller;

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
		return;

	dict = g_object_get_data(G_OBJECT(widget), "spell-lang");

	g_free(compose->spell_lang);
	compose->spell_lang = g_strdup(dict);
 
	speller = gtkspell_get_from_text_view(GTK_TEXT_VIEW(compose->text));
	if (speller != NULL)
		gtkspell_set_language(speller, dict, NULL);

	debug_print("Spell lang set to \"%s\"\n", dict);
}
#endif /* USE_GTKSPELL */

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
	GList *list, *cur;
	gchar *path, *filename;
	gchar *content_type = NULL;

	if (info == DRAG_TYPE_RFC822)
		content_type = "message/rfc822";

	debug_print("compose_attach_drag_received_cb(): received %s\n",
		    (const gchar *)data->data);

	list = uri_list_extract_filenames((const gchar *)data->data);
	for (cur = list; cur != NULL; cur = cur->next) {
		path = (gchar *)cur->data;
		filename = conv_filename_to_utf8(path);
		compose_attach_append(compose, path, filename, content_type);
		compose_changed_cb(NULL, compose);
		g_free(filename);
		g_free(path);
	}
	if (list) compose_changed_cb(NULL, compose);
	g_list_free(list);

	if ((drag_context->actions & GDK_ACTION_MOVE) != 0)
		drag_context->action = 0;
	gtk_drag_finish(drag_context, TRUE, FALSE, time);
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
	static GdkDragContext *context_ = NULL;
	static gint x_ = -1, y_ = -1;
	static guint info_ = N_DRAG_TYPES;
	static guint time_ = G_MAXUINT;

	debug_print("compose_insert_drag_received_cb(): received %s\n",
		    (const gchar *)data->data);

	/* FIXME: somehow drag-data-received signal is emitted twice.
	 * This hack prevents duplicated insertion. */
	if (context_ == drag_context && x_ == x && y_ == y && info_ == info &&
	    time_ == time) {
		debug_print("dup drag-data-received event\n");
		context_ = NULL;
		x_ = y_ = -1;
		info_ = N_DRAG_TYPES;
		time_ = G_MAXUINT;
		return;
	}
	context_ = drag_context;
	x_ = x;
	y_ = y;
	info_ = info;
	time_ = time;

	compose_attach_drag_received_cb(widget, drag_context, x, y, data,
					info, time, user_data);
}

static void to_activated(GtkWidget *widget, Compose *compose)
{
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
	if (GTK_WIDGET_VISIBLE(compose->reply_entry))
		gtk_widget_grab_focus(compose->reply_entry);
	else if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->subject_entry);
}

static void replyto_activated(GtkWidget *widget, Compose *compose)
{
	if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->subject_entry);
}

static void followupto_activated(GtkWidget *widget, Compose *compose)
{
	gtk_widget_grab_focus(compose->subject_entry);
}

static void subject_activated(GtkWidget *widget, Compose *compose)
{
	gtk_widget_grab_focus(compose->text);
}

static void text_inserted(GtkTextBuffer *buffer, GtkTextIter *iter,
			  const gchar *text, gint len, Compose *compose)
{
	GtkTextMark *mark;

	/* pass to the default handler */
	if (!compose->autowrap)
		return;

	g_return_if_fail(text != NULL);

	g_signal_handlers_block_by_func(G_OBJECT(buffer),
					G_CALLBACK(text_inserted),
					compose);

	gtk_text_buffer_insert(buffer, iter, text, len);

	mark = gtk_text_buffer_create_mark(buffer, NULL, iter, FALSE);
	compose_wrap_all_full(compose, TRUE);
	gtk_text_buffer_get_iter_at_mark(buffer, iter, mark);
	gtk_text_buffer_delete_mark(buffer, mark);

	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
					  G_CALLBACK(text_inserted),
					  compose);
	g_signal_stop_emission_by_name(G_OBJECT(buffer), "insert-text");
}

static gboolean autosave_timeout(gpointer data)
{
	Compose *compose = (Compose *)data;

	gdk_threads_enter();

	debug_print("auto-saving...\n");

	if (compose->modified)
		compose_draft_cb(data, 1, NULL);

	gdk_threads_leave();

	return TRUE;
}
