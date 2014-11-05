/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto
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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "main.h"
#include "prefs.h"
#include "prefs_ui.h"
#include "prefs_common.h"
#include "prefs_common_dialog.h"
#include "prefs_display_header.h"
#include "prefs_summary_column.h"
#include "mainwindow.h"
#include "summaryview.h"
#include "messageview.h"
#include "manage_window.h"
#include "foldersel.h"
#include "filesel.h"
#include "inc.h"
#include "menu.h"
#include "colorlabel.h"
#include "codeconv.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"
#include "socket.h"
#include "plugin.h"

static PrefsDialog dialog;

static struct Receive {
	GtkWidget *checkbtn_autochk;
	GtkWidget *spinbtn_autochk;
	GtkObject *spinbtn_autochk_adj;

	GtkWidget *checkbtn_chkonstartup;
	GtkWidget *checkbtn_scan_after_inc;

	GtkWidget *checkbtn_newmsg_notify_window;
	GtkWidget *spinbtn_notifywin;
	GtkObject *spinbtn_notifywin_adj;
#ifdef G_OS_WIN32
	GtkWidget *checkbtn_newmsg_sound;
	GtkWidget *entry_newmsg_sound;
#endif
	GtkWidget *checkbtn_newmsg_notify;
	GtkWidget *entry_newmsg_notify;

#ifndef G_OS_WIN32
	GtkWidget *checkbtn_local;
	GtkWidget *checkbtn_filter_on_inc;
	GtkWidget *entry_spool;
#endif
} receive;

static struct Send {
	GtkWidget *checkbtn_savemsg;
	GtkWidget *checkbtn_filter_sent;
	GtkWidget *checkbtn_recipients_autoreg;
	GtkWidget *checkbtn_show_send_dialog;

	GtkWidget *optmenu_encoding_method;
	GtkWidget *optmenu_mime_fencoding_method;

	GtkWidget *checkbtn_check_attach;
	GtkWidget *entry_check_attach_str;
	GtkWidget *checkbtn_check_recp;
	GtkWidget *entry_check_recp_excl;
} p_send;

static struct Compose {
	GtkWidget *checkbtn_autosig;
	GtkWidget *entry_sigsep;

	GtkWidget *checkbtn_autoextedit;
	GtkWidget *spinbtn_undolevel;
	GtkObject *spinbtn_undolevel_adj;
	GtkWidget *spinbtn_linewrap;
	GtkObject *spinbtn_linewrap_adj;
	GtkWidget *checkbtn_wrapquote;
	GtkWidget *checkbtn_autowrap;

	GtkWidget *checkbtn_autosave;
	GtkWidget *spinbtn_autosave;
	GtkObject *spinbtn_autosave_adj;

	GtkWidget *checkbtn_reply_account_autosel;
	GtkWidget *checkbtn_quote;
	GtkWidget *checkbtn_default_reply_list;
	GtkWidget *checkbtn_inherit_recipient_on_self_reply;
	GtkWidget *checkbtn_reply_address_only;
} compose;

static struct Quote {
	GtkWidget *entry_quotemark;
	GtkWidget *text_quotefmt;

	GtkWidget *entry_fw_quotemark;
	GtkWidget *text_fw_quotefmt;
} quote;

#if USE_GTKSPELL
static struct Spell {
        GtkWidget *chkbtn_enable_spell;
        GtkWidget *entry_spell_lang;
} spell;
#endif

static struct Display {
	GtkWidget *fontbtn_textfont;

	GtkWidget *chkbtn_folder_unread;
	GtkWidget *chkbtn_folder_col_new;
	GtkWidget *chkbtn_folder_col_unread;
	GtkWidget *chkbtn_folder_col_total;
	GtkWidget *entry_ng_abbrev_len;
	GtkWidget *spinbtn_ng_abbrev_len;
	GtkObject *spinbtn_ng_abbrev_len_adj;

	GtkWidget *chkbtn_swapfrom;
	GtkWidget *chkbtn_expand_thread;
	GtkWidget *entry_datefmt;

	GtkWidget *optmenu_dispencoding;
	GtkWidget *optmenu_outencoding;
} display;

static struct Message {
	GtkWidget *chkbtn_enablecol;
	GtkWidget *button_edit_col;
	GtkWidget *chkbtn_disphdrpane;
	GtkWidget *chkbtn_disphdr;
	GtkWidget *chkbtn_html;
	GtkWidget *chkbtn_prefer_html;
	GtkWidget *chkbtn_htmlonly;
	GtkWidget *spinbtn_linespc;
	GtkObject *spinbtn_linespc_adj;

	GtkWidget *chkbtn_smoothscroll;
	GtkWidget *spinbtn_scrollstep;
	GtkObject *spinbtn_scrollstep_adj;
	GtkWidget *chkbtn_halfpage;
} message;

static struct Attach {
	GtkWidget *radiobtn_attach_toolbtn_pos;
	GtkWidget *chkbtn_show_attach_tab;
	GtkWidget *chkbtn_show_files_first;

	GtkWidget *chkbtn_resize_image;
	GtkWidget *chkbtn_inline_image;
} attach;

static struct ColorLabel {
	GtkWidget *entry_color[7];
} colorlabel;

static struct JunkMail {
	GtkWidget *chkbtn_enable_junk;
	GtkWidget *entry_junk_learncmd;
	GtkWidget *entry_nojunk_learncmd;
	GtkWidget *entry_classify_cmd;
	GtkWidget *entry_junkfolder;
	GtkWidget *chkbtn_filter_on_recv;
	GtkWidget *chkbtn_filter_before;
	GtkWidget *chkbtn_delete_on_recv;
	GtkWidget *chkbtn_nofilter_in_book;
	GtkWidget *chkbtn_mark_as_read;
} junk;

#if USE_GPGME
static struct Privacy {
	GtkWidget *checkbtn_auto_check_signatures;
	GtkWidget *checkbtn_gpg_signature_popup;
	GtkWidget *checkbtn_store_passphrase;
	GtkWidget *spinbtn_store_passphrase;
	GtkObject *spinbtn_store_passphrase_adj;
#ifndef G_OS_WIN32
	GtkWidget *checkbtn_passphrase_grab;
#endif
	GtkWidget *checkbtn_gpg_warning;
} privacy;
#endif

static struct Interface {
	GtkWidget *checkbtn_always_show_msg;
	GtkWidget *checkbtn_always_mark_read;
	GtkWidget *checkbtn_openunread;
	GtkWidget *checkbtn_remember_lastsel;
	/* GtkWidget *checkbtn_mark_as_read_on_newwin; */
	GtkWidget *checkbtn_openinbox;
	GtkWidget *checkbtn_openinbox_startup;
	GtkWidget *checkbtn_change_account_on_folder_sel;
	GtkWidget *checkbtn_immedexec;
#ifndef G_OS_WIN32
	GtkWidget *checkbtn_comply_gnome_hig;
#endif
	GtkWidget *checkbtn_show_trayicon;
	GtkWidget *checkbtn_minimize_to_tray;
	GtkWidget *checkbtn_tray_toggle_window;
} iface;

static struct Other {
	GtkWidget *optmenu_recvdialog;
	GtkWidget *checkbtn_no_recv_err_panel;
	GtkWidget *checkbtn_close_recv_dialog;

	GtkWidget *checkbtn_addaddrbyclick;
	GtkWidget *checkbtn_add_address_only;
	GtkWidget *radiobtn_addr_compl;

	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_warnqueued;

	GtkWidget *radiobtn_online_mode;
} other;

static struct Extcmd {
	GtkWidget *uri_combo;
	GtkWidget *uri_entry;
	GtkWidget *exteditor_combo;
	GtkWidget *exteditor_entry;

	GtkWidget *checkbtn_printcmd;
	GtkWidget *printcmd_entry;

	GtkWidget *checkbtn_incext;
	GtkWidget *entry_incext;
	GtkWidget *button_incext;

	GtkWidget *checkbtn_extsend;
	GtkWidget *entry_extsend;
	GtkWidget *button_extsend;
} extcmd;

#if USE_UPDATE_CHECK
static struct UpdateCheck {
	GtkWidget *checkbtn_autoupdate;
	GtkWidget *checkbtn_useproxy;
	GtkWidget *entry_proxyhost;
} update_check;
#endif

static struct Advanced {
	GtkWidget *checkbtn_strict_cache_check;

	GtkWidget *spinbtn_iotimeout;
	GtkObject *spinbtn_iotimeout_adj;
} advanced;

static struct MessageColorButtons {
	GtkWidget *quote_level1_btn;
	GtkWidget *quote_level2_btn;
	GtkWidget *quote_level3_btn;
	GtkWidget *uri_btn;
} color_buttons;

static struct KeybindDialog {
	GtkWidget *window;
	GtkWidget *combo;
} keybind;

static GtkWidget *quote_desc_win;
static GtkWidget *quote_color_win;
static GtkWidget *color_dialog;

static void prefs_common_charset_set_data_from_optmenu	   (PrefParam *pparam);
static void prefs_common_charset_set_optmenu		   (PrefParam *pparam);
static void prefs_common_recv_dialog_set_optmenu	   (PrefParam *pparam);
static void prefs_common_uri_set_data_from_entry	   (PrefParam *pparam);
static void prefs_common_uri_set_entry			   (PrefParam *pparam);

static void prefs_common_addr_compl_set_data_from_radiobtn (PrefParam *pparam);
static void prefs_common_addr_compl_set_radiobtn	   (PrefParam *pparam);

static void prefs_common_attach_toolbtn_pos_set_data_from_radiobtn (PrefParam *pparam);
static void prefs_common_attach_toolbtn_pos_set_radiobtn	   (PrefParam *pparam);

static void prefs_common_online_mode_set_data_from_radiobtn(PrefParam *pparam);
static void prefs_common_online_mode_set_radiobtn	   (PrefParam *pparam);

static PrefsUIData ui_data[] = {
	/* Receive */
	{"autochk_newmail", &receive.checkbtn_autochk,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"autochk_interval", &receive.spinbtn_autochk,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
	{"check_on_startup", &receive.checkbtn_chkonstartup,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"scan_all_after_inc", &receive.checkbtn_scan_after_inc,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"enable_newmsg_notify", &receive.checkbtn_newmsg_notify,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"newmsg_notify_command", &receive.entry_newmsg_notify,
	 prefs_set_data_from_entry, prefs_set_entry},
#ifdef G_OS_WIN32
	{"enable_newmsg_notify_sound", &receive.checkbtn_newmsg_sound,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"newmsg_notify_sound", &receive.entry_newmsg_sound,
	 prefs_set_data_from_entry, prefs_set_entry},
#endif
	{"enable_newmsg_notify_window", &receive.checkbtn_newmsg_notify_window,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"notify_window_period", &receive.spinbtn_notifywin,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

#ifndef G_OS_WIN32
	{"inc_local", &receive.checkbtn_local,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"filter_on_inc_local", &receive.checkbtn_filter_on_inc,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"spool_path", &receive.entry_spool,
	 prefs_set_data_from_entry, prefs_set_entry},
#endif

	/* Send */
	{"save_message", &p_send.checkbtn_savemsg,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"filter_sent_message", &p_send.checkbtn_filter_sent,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"recipients_autoreg", &p_send.checkbtn_recipients_autoreg,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"show_send_dialog", &p_send.checkbtn_show_send_dialog,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"encoding_method", &p_send.optmenu_encoding_method,
	 prefs_set_data_from_optmenu, prefs_set_optmenu},
	{"mime_filename_encoding_method", &p_send.optmenu_mime_fencoding_method,
	 prefs_set_data_from_optmenu, prefs_set_optmenu},
	{"check_attach", &p_send.checkbtn_check_attach,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"check_attach_str", &p_send.entry_check_attach_str,
	 prefs_set_data_from_entry, prefs_set_entry},
	{"check_recipients", &p_send.checkbtn_check_recp,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"check_recp_exclude", &p_send.entry_check_recp_excl,
	 prefs_set_data_from_entry, prefs_set_entry},

	/* {"allow_jisx0201_kana", NULL, NULL, NULL}, */

	/* Compose */
	{"auto_signature", &compose.checkbtn_autosig,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"signature_separator", &compose.entry_sigsep,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"auto_ext_editor", &compose.checkbtn_autoextedit,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"undo_level", &compose.spinbtn_undolevel,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"linewrap_length", &compose.spinbtn_linewrap,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
	{"linewrap_quotation", &compose.checkbtn_wrapquote,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"linewrap_auto", &compose.checkbtn_autowrap,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"enable_autosave", &compose.checkbtn_autosave,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"autosave_interval", &compose.spinbtn_autosave,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"reply_with_quote", &compose.checkbtn_quote,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"reply_account_autoselect",
	 &compose.checkbtn_reply_account_autosel,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"default_reply_list", &compose.checkbtn_default_reply_list,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"inherit_recipient_on_self_reply",
	 &compose.checkbtn_inherit_recipient_on_self_reply,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"reply_address_only",
	 &compose.checkbtn_reply_address_only,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	/* {"show_ruler", NULL, NULL, NULL}, */

	/* Quote */
	{"reply_quote_mark", &quote.entry_quotemark,
	 prefs_set_data_from_entry, prefs_set_entry},
	{"reply_quote_format", &quote.text_quotefmt,
	 prefs_set_data_from_text, prefs_set_text},

	{"forward_quote_mark", &quote.entry_fw_quotemark,
	 prefs_set_data_from_entry, prefs_set_entry},
	{"forward_quote_format", &quote.text_fw_quotefmt,
	 prefs_set_data_from_text, prefs_set_text},

#if USE_GTKSPELL
	/* Spelling */
	{"check_spell", &spell.chkbtn_enable_spell,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"spell_lang",  &spell.entry_spell_lang,
	 prefs_set_data_from_entry, prefs_set_entry},
#endif

	/* Display */
	{"message_font_name", &display.fontbtn_textfont,
	 prefs_set_data_from_fontbtn, prefs_set_fontbtn},

	{"display_folder_unread_num", &display.chkbtn_folder_unread,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"folder_col_show_new", &display.chkbtn_folder_col_new,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"folder_col_show_unread", &display.chkbtn_folder_col_unread,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"folder_col_show_total", &display.chkbtn_folder_col_total,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"newsgroup_abbrev_len", &display.spinbtn_ng_abbrev_len,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	/* Display: Summary View */
	{"enable_swap_from", &display.chkbtn_swapfrom,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"date_format", &display.entry_datefmt,
	 prefs_set_data_from_entry, prefs_set_entry},
	{"expand_thread", &display.chkbtn_expand_thread,
	 prefs_set_data_from_toggle, prefs_set_toggle},

#if 0
	{"enable_rules_hint", NULL, NULL, NULL},
#endif /* 0 */

	/* Message */
	{"enable_color", &message.chkbtn_enablecol,
	 prefs_set_data_from_toggle, prefs_set_toggle},

#if 0
	{"convert_mb_alnum", &message.chkbtn_mbalnum,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#endif
	{"display_header_pane", &message.chkbtn_disphdrpane,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"display_header", &message.chkbtn_disphdr,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"render_html", &message.chkbtn_html,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"alt_prefer_html", &message.chkbtn_prefer_html,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"html_only_as_attach", &message.chkbtn_htmlonly,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"line_space", &message.spinbtn_linespc,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	/* {"textview_cursor_visible", NULL, NULL, NULL}, */

	{"enable_smooth_scroll", &message.chkbtn_smoothscroll,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"scroll_step", &message.spinbtn_scrollstep,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
	{"scroll_half_page", &message.chkbtn_halfpage,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	/* Attachment */
	{"attach_toolbutton_pos", &attach.radiobtn_attach_toolbtn_pos,
	 prefs_common_attach_toolbtn_pos_set_data_from_radiobtn,
	 prefs_common_attach_toolbtn_pos_set_radiobtn},
	{"show_attach_tab", &attach.chkbtn_show_attach_tab,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"show_attached_files_first", &attach.chkbtn_show_files_first,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"resize_image", &attach.chkbtn_resize_image,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"inline_image", &attach.chkbtn_inline_image,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	/* Encoding */
	{"default_encoding", &display.optmenu_dispencoding,
	 prefs_common_charset_set_data_from_optmenu,
	 prefs_common_charset_set_optmenu},
	{"outgoing_charset", &display.optmenu_outencoding,
	 prefs_common_charset_set_data_from_optmenu,
	 prefs_common_charset_set_optmenu},

	/* Junk mail */
	{"enable_junk", &junk.chkbtn_enable_junk,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"junk_learn_command", &junk.entry_junk_learncmd,
	 prefs_set_data_from_entry, prefs_set_entry},
	{"nojunk_learn_command", &junk.entry_nojunk_learncmd,
	 prefs_set_data_from_entry, prefs_set_entry},
	{"junk_classify_command", &junk.entry_classify_cmd,
	 prefs_set_data_from_entry, prefs_set_entry},
	{"junk_folder", &junk.entry_junkfolder,
	 prefs_set_data_from_entry, prefs_set_entry},
	{"filter_junk_on_receive", &junk.chkbtn_filter_on_recv,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"filter_junk_before", &junk.chkbtn_filter_before,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"delete_junk_on_receive", &junk.chkbtn_delete_on_recv,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"nofilter_junk_sender_in_book", &junk.chkbtn_nofilter_in_book,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"mark_junk_as_read", &junk.chkbtn_mark_as_read,
	 prefs_set_data_from_toggle, prefs_set_toggle},

#if USE_GPGME
	/* Privacy */
	{"auto_check_signatures",
	 &privacy.checkbtn_auto_check_signatures,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"gpg_signature_popup", &privacy.checkbtn_gpg_signature_popup,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"store_passphrase", &privacy.checkbtn_store_passphrase,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"store_passphrase_timeout", &privacy.spinbtn_store_passphrase,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
#ifndef G_OS_WIN32
	{"passphrase_grab", &privacy.checkbtn_passphrase_grab,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#endif /* G_OS_WIN32 */
	{"show_gpg_warning", &privacy.checkbtn_gpg_warning,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#endif /* USE_GPGME */

	/* Interface */
	{"always_show_message_when_selected",
	 &iface.checkbtn_always_show_msg,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"always_mark_read_on_show_msg", &iface.checkbtn_always_mark_read,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"open_unread_on_enter", &iface.checkbtn_openunread,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"remember_last_selected", &iface.checkbtn_remember_lastsel,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#if 0
	{"mark_as_read_on_new_window",
	 &iface.checkbtn_mark_as_read_on_newwin,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#endif
	{"open_inbox_on_inc", &iface.checkbtn_openinbox,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"open_inbox_on_startup", &iface.checkbtn_openinbox_startup,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"change_account_on_folder_selection",
	 &iface.checkbtn_change_account_on_folder_sel,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"immediate_execution", &iface.checkbtn_immedexec,
	 prefs_set_data_from_toggle, prefs_set_toggle},

#ifndef G_OS_WIN32
	{"comply_gnome_hig", &iface.checkbtn_comply_gnome_hig,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#endif
	{"show_trayicon", &iface.checkbtn_show_trayicon,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"minimize_to_tray", &iface.checkbtn_minimize_to_tray,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"toggle_window_on_trayicon_click",
	 &iface.checkbtn_tray_toggle_window,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	/* Other */
	{"receive_dialog_mode", &other.optmenu_recvdialog,
	 prefs_set_data_from_optmenu, prefs_common_recv_dialog_set_optmenu},
	{"no_receive_error_panel", &other.checkbtn_no_recv_err_panel,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"close_receive_dialog", &other.checkbtn_close_recv_dialog,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"add_address_by_click", &other.checkbtn_addaddrbyclick,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"always_add_address_only", &other.checkbtn_add_address_only,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"enable_address_completion", &other.radiobtn_addr_compl,
	 prefs_common_addr_compl_set_data_from_radiobtn,
	 prefs_common_addr_compl_set_radiobtn},

	{"confirm_on_exit", &other.checkbtn_confonexit,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"clean_trash_on_exit", &other.checkbtn_cleanonexit,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"ask_on_cleaning", &other.checkbtn_askonclean,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"warn_queued_on_exit", &other.checkbtn_warnqueued,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"startup_online_mode", &other.radiobtn_online_mode,
	 prefs_common_online_mode_set_data_from_radiobtn,
	 prefs_common_online_mode_set_radiobtn},

	/* {"logwindow_line_limit", NULL, NULL, NULL}, */

	/* External commands */
	{"uri_open_command", &extcmd.uri_entry,
	 prefs_common_uri_set_data_from_entry, prefs_common_uri_set_entry},
	{"ext_editor_command", &extcmd.exteditor_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"use_print_command", &extcmd.checkbtn_printcmd,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"print_command", &extcmd.printcmd_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

#ifndef G_OS_WIN32
	{"use_ext_inc", &extcmd.checkbtn_incext,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"ext_inc_path", &extcmd.entry_incext,
	 prefs_set_data_from_entry, prefs_set_entry},
#endif
	{"use_ext_sendmail", &extcmd.checkbtn_extsend,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"ext_sendmail_cmd", &extcmd.entry_extsend,
	 prefs_set_data_from_entry, prefs_set_entry},

	/* Update check */
#if USE_UPDATE_CHECK
	{"auto_update_check", &update_check.checkbtn_autoupdate,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"use_http_proxy", &update_check.checkbtn_useproxy,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"http_proxy_host", &update_check.entry_proxyhost,
	 prefs_set_data_from_entry, prefs_set_entry},
#endif

	/* Advanced */
	{"strict_cache_check", &advanced.checkbtn_strict_cache_check,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"io_timeout_secs", &advanced.spinbtn_iotimeout,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{NULL, NULL, NULL, NULL}
};

/* widget creating functions */
static void prefs_common_create		(void);
static void prefs_receive_create	(void);
static void prefs_send_create		(void);
static void prefs_compose_create	(void);
static GtkWidget *prefs_quote_create	(void);
#if USE_GTKSPELL
static GtkWidget *prefs_spell_create	(void);
#endif
static void prefs_display_create	(void);
static GtkWidget *prefs_message_create	(void);
static GtkWidget *prefs_attach_create	(void);

static GtkWidget *prefs_colorlabel_create	(void);
static void prefs_common_colorlabel_set_dialog	(void);
static void prefs_common_colorlabel_update	(void);

static void prefs_junk_create		(void);
#if USE_GPGME
static void prefs_privacy_create	(void);
#endif
static void prefs_details_create	(void);
static GtkWidget *prefs_other_create	(void);
static GtkWidget *prefs_extcmd_create	(void);
#if USE_UPDATE_CHECK
static GtkWidget *prefs_update_create	(void);
#endif
static GtkWidget *prefs_advanced_create	(void);

static void prefs_common_set_encoding_optmenu	(GtkOptionMenu	*optmenu,
						 gboolean	 outgoing);

static void date_format_ok_btn_clicked		(GtkButton	*button,
						 GtkWidget     **widget);
static void date_format_cancel_btn_clicked	(GtkButton	*button,
						 GtkWidget     **widget);
static gboolean date_format_key_pressed		(GtkWidget	*keywidget,
						 GdkEventKey	*event,
						 GtkWidget     **widget);
static gboolean date_format_on_delete		(GtkWidget	*dialogwidget,
						 GdkEventAny	*event,
						 GtkWidget     **widget);
static void date_format_entry_on_change		(GtkEditable	*editable,
						 GtkLabel	*example);
static void date_format_select_row		(GtkWidget	*date_format_list,
						 gint		 row,
						 gint		 column,
						 GdkEventButton	*event,
						 GtkWidget	*date_format);
static GtkWidget *date_format_create		(GtkButton	*button,
						 void		*data);

static void prefs_quote_description_create	(void);
static gboolean prefs_quote_description_key_pressed
						(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static gboolean prefs_quote_description_deleted	(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);

static void prefs_quote_colors_dialog		(void);
static void prefs_quote_colors_dialog_create	(void);
static gint prefs_quote_colors_deleted		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);
static gboolean prefs_quote_colors_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void quote_color_set_dialog		(GtkWidget	*widget,
						 gpointer	 data);
static void quote_colors_set_dialog_ok		(GtkWidget	*widget,
						 gpointer	 data);
static void quote_colors_set_dialog_cancel	(GtkWidget	*widget,
						 gpointer	 data);
static gboolean quote_colors_set_dialog_key_pressed
						(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void set_button_bg_color			(GtkWidget	*widget,
						 gint		 color);
static void prefs_enable_message_color_toggled	(void);
static void prefs_recycle_colors_toggled	(GtkWidget	*widget);

static void prefs_keybind_select		(void);
static gint prefs_keybind_deleted		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);
static gboolean prefs_keybind_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_keybind_cancel		(void);
static void prefs_keybind_apply_clicked		(GtkWidget	*widget);

static void prefs_common_dispitem_clicked	(void);

static void prefs_common_select_folder_cb	(GtkWidget	*widget,
						 gpointer	 data);

#ifdef G_OS_WIN32
static void prefs_common_file_selection_cb	(GtkButton	*button,
						 gpointer	 data);
#endif

static gint prefs_common_deleted		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);
static gboolean prefs_common_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_common_ok			(void);
static void prefs_common_apply			(void);
static void prefs_common_cancel			(void);


void prefs_common_open(void)
{
	static gboolean ui_registered = FALSE;

	inc_lock();

	if (!ui_registered) {
		prefs_register_ui(prefs_common_get_params(), ui_data);
		ui_registered = TRUE;
	}

	if (!dialog.window) {
		prefs_common_create();
	}

	gtkut_box_set_reverse_order(GTK_BOX(dialog.confirm_area),
				    !prefs_common.comply_gnome_hig);
	manage_window_set_transient(GTK_WINDOW(dialog.window));
	gtk_notebook_set_current_page(GTK_NOTEBOOK(dialog.notebook), 0);
	gtk_widget_grab_focus(dialog.ok_btn);

	prefs_set_dialog(prefs_common_get_params());
	prefs_common_colorlabel_set_dialog();

	gtk_widget_show(dialog.window);

	syl_plugin_signal_emit("prefs-common-open", dialog.window);
}

static void prefs_common_create(void)
{
	gint page = 0;

	debug_print(_("Creating common preferences window...\n"));

	prefs_dialog_create(&dialog);
	gtk_window_set_title (GTK_WINDOW(dialog.window),
			      _("Common Preferences"));
	g_signal_connect (G_OBJECT(dialog.window), "delete_event",
			  G_CALLBACK(prefs_common_deleted), NULL);
	g_signal_connect (G_OBJECT(dialog.window), "key_press_event",
			  G_CALLBACK(prefs_common_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(dialog.window);

	g_signal_connect (G_OBJECT(dialog.ok_btn), "clicked",
			  G_CALLBACK(prefs_common_ok), NULL);
	g_signal_connect (G_OBJECT(dialog.apply_btn), "clicked",
			  G_CALLBACK(prefs_common_apply), NULL);
	g_signal_connect (G_OBJECT(dialog.cancel_btn), "clicked",
			  G_CALLBACK(prefs_common_cancel), NULL);

	/* create all widgets on notebook */
	prefs_receive_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Receive"),   page++);
	prefs_send_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Send"),      page++);
	prefs_compose_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Compose"),   page++);
	prefs_display_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Display"),   page++);
	prefs_junk_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Junk mail"), page++);
#if USE_GPGME
	prefs_privacy_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Privacy"),   page++);
#endif
	prefs_details_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Details"), page++);

	gtk_widget_show_all(dialog.window);
}

static void prefs_receive_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *vbox4;
	GtkWidget *hbox;

	GtkWidget *hbox_autochk;
	GtkWidget *checkbtn_autochk;
	GtkWidget *label_autochk1;
	GtkObject *spinbtn_autochk_adj;
	GtkWidget *spinbtn_autochk;
	GtkWidget *label_autochk2;
	GtkWidget *checkbtn_chkonstartup;
	GtkWidget *checkbtn_scan_after_inc;

	GtkWidget *frame_notify;
	GtkWidget *checkbtn_newmsg_notify_window;
	GtkWidget *hbox_notifywin;
	GtkWidget *hbox_spc;
	GtkWidget *label_notifywin;
	GtkObject *spinbtn_notifywin_adj;
	GtkWidget *spinbtn_notifywin;
	GtkWidget *checkbtn_newmsg_notify;
	GtkWidget *label_newmsg_notify;
	GtkWidget *entry_newmsg_notify;
	GtkWidget *label_notify_cmd_desc;
#ifdef G_OS_WIN32
	GtkWidget *checkbtn_newmsg_sound;
	GtkWidget *label_newmsg_sound;
	GtkWidget *entry_newmsg_sound;
	GtkWidget *btn_filesel;
#endif

#ifndef G_OS_WIN32
	GtkWidget *frame_spool;
	GtkWidget *checkbtn_local;
	GtkWidget *checkbtn_filter_on_inc;
	GtkWidget *label_spool;
	GtkWidget *entry_spool;
#endif

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	/* Auto-checking */
	hbox_autochk = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_autochk);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_autochk, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox_autochk, checkbtn_autochk,
			   _("Auto-check new mail"));

	label_autochk1 = gtk_label_new (_("every"));
	gtk_widget_show (label_autochk1);
	gtk_box_pack_start (GTK_BOX (hbox_autochk), label_autochk1, FALSE, FALSE, 0);

	spinbtn_autochk_adj = gtk_adjustment_new (5, 1, 1000, 1, 10, 0);
	spinbtn_autochk = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_autochk_adj), 1, 0);
	gtk_widget_show (spinbtn_autochk);
	gtk_box_pack_start (GTK_BOX (hbox_autochk), spinbtn_autochk, FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_autochk, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_autochk), TRUE);

	label_autochk2 = gtk_label_new (_("minute(s)"));
	gtk_widget_show (label_autochk2);
	gtk_box_pack_start (GTK_BOX (hbox_autochk), label_autochk2, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_autochk, label_autochk1);
	SET_TOGGLE_SENSITIVITY(checkbtn_autochk, spinbtn_autochk);
	SET_TOGGLE_SENSITIVITY(checkbtn_autochk, label_autochk2);

	PACK_CHECK_BUTTON (vbox2, checkbtn_chkonstartup,
			   _("Check new mail on startup"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_scan_after_inc,
			   _("Update all local folders after incorporation"));

	/* New message notify */
	PACK_FRAME(vbox1, frame_notify, _("New message notification"));

	vbox3 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (frame_notify), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), 8);

	vbox4 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox4);
	gtk_box_pack_start (GTK_BOX (vbox3), vbox4, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox4, checkbtn_newmsg_notify_window,
		 _("Show notification window when new messages arrive"));

	hbox_notifywin = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_notifywin);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox_notifywin, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox_notifywin), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	label_notifywin = gtk_label_new (_("Display window for"));
	gtk_widget_show (label_notifywin);
	gtk_box_pack_start (GTK_BOX (hbox_notifywin), label_notifywin, FALSE, FALSE, 0);

	spinbtn_notifywin_adj = gtk_adjustment_new (10, 0, 1000, 1, 10, 0);
	spinbtn_notifywin = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_notifywin_adj), 1, 0);
	gtk_widget_show (spinbtn_notifywin);
	gtk_box_pack_start (GTK_BOX (hbox_notifywin), spinbtn_notifywin, FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_notifywin, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_notifywin), TRUE);

	label_notifywin = gtk_label_new (_("second(s)"));
	gtk_widget_show (label_notifywin);
	gtk_box_pack_start (GTK_BOX (hbox_notifywin), label_notifywin, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_newmsg_notify_window, hbox_notifywin);

	hbox_notifywin = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_notifywin);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox_notifywin, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox_notifywin), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	label_notifywin = gtk_label_new (_("0: don't auto-close"));
	gtk_widget_show (label_notifywin);
	gtk_box_pack_start (GTK_BOX (hbox_notifywin), label_notifywin, FALSE, FALSE, 0);
	gtkut_widget_set_small_font_size (label_notifywin);

	SET_TOGGLE_SENSITIVITY(checkbtn_newmsg_notify_window, hbox_notifywin);

#ifdef G_OS_WIN32
	PACK_CHECK_BUTTON
		(vbox4, checkbtn_newmsg_sound,
		 _("Play sound when new messages arrive"));

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox, FALSE, FALSE, 0);

	label_newmsg_sound = gtk_label_new (_("Sound file"));
	gtk_widget_show (label_newmsg_sound);
	gtk_box_pack_start (GTK_BOX (hbox), label_newmsg_sound,
			    FALSE, FALSE, 0);

	entry_newmsg_sound = gtk_entry_new ();
	gtk_widget_show (entry_newmsg_sound);
	gtk_box_pack_start (GTK_BOX (hbox), entry_newmsg_sound, TRUE, TRUE, 0);

	btn_filesel = gtk_button_new_with_label("...");
	gtk_box_pack_start (GTK_BOX (hbox), btn_filesel, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(btn_filesel), "clicked",
			 G_CALLBACK(prefs_common_file_selection_cb),
			 entry_newmsg_sound);

	SET_TOGGLE_SENSITIVITY (checkbtn_newmsg_sound, hbox);

	PACK_CHECK_BUTTON
		(vbox3, checkbtn_newmsg_notify,
		 _("Execute command when new messages arrive"));
#else
	PACK_CHECK_BUTTON
		(vbox4, checkbtn_newmsg_notify,
		 _("Execute command when new messages arrive"));
#endif

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox, FALSE, FALSE, 0);

	label_newmsg_notify = gtk_label_new (_("Command"));
	gtk_widget_show (label_newmsg_notify);
	gtk_box_pack_start (GTK_BOX (hbox), label_newmsg_notify,
			    FALSE, FALSE, 0);

	entry_newmsg_notify = gtk_entry_new ();
	gtk_widget_show (entry_newmsg_notify);
	gtk_box_pack_start (GTK_BOX (hbox), entry_newmsg_notify, TRUE, TRUE, 0);

	SET_TOGGLE_SENSITIVITY (checkbtn_newmsg_notify, hbox);

	PACK_SMALL_LABEL
		(vbox3, label_notify_cmd_desc,
		 _("`%d' will be replaced with the number of new messages."));

#ifndef G_OS_WIN32
	PACK_FRAME_WITH_CHECK_BUTTON(vbox1, frame_spool, checkbtn_local,
				     _("Incorporate from local spool"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_spool), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
	SET_TOGGLE_SENSITIVITY (checkbtn_local, vbox2);

	hbox = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox, checkbtn_filter_on_inc,
			   _("Filter on incorporation"));

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	label_spool = gtk_label_new (_("Spool path"));
	gtk_widget_show (label_spool);
	gtk_box_pack_start (GTK_BOX (hbox), label_spool, FALSE, FALSE, 0);

	entry_spool = gtk_entry_new ();
	gtk_widget_show (entry_spool);
	gtk_box_pack_start (GTK_BOX (hbox), entry_spool, TRUE, TRUE, 0);
#endif /* !G_OS_WIN32 */

	receive.checkbtn_autochk    = checkbtn_autochk;
	receive.spinbtn_autochk     = spinbtn_autochk;
	receive.spinbtn_autochk_adj = spinbtn_autochk_adj;

	receive.checkbtn_chkonstartup   = checkbtn_chkonstartup;
	receive.checkbtn_scan_after_inc = checkbtn_scan_after_inc;

	receive.checkbtn_newmsg_notify_window = checkbtn_newmsg_notify_window;
	receive.spinbtn_notifywin       = spinbtn_notifywin;
	receive.spinbtn_notifywin_adj   = spinbtn_notifywin_adj;
#ifdef G_OS_WIN32
	receive.checkbtn_newmsg_sound   = checkbtn_newmsg_sound;
	receive.entry_newmsg_sound      = entry_newmsg_sound;
#endif
	receive.checkbtn_newmsg_notify  = checkbtn_newmsg_notify;
	receive.entry_newmsg_notify     = entry_newmsg_notify;

#ifndef G_OS_WIN32
	receive.checkbtn_local         = checkbtn_local;
	receive.checkbtn_filter_on_inc = checkbtn_filter_on_inc;
	receive.entry_spool            = entry_spool;
#endif
}

static void prefs_send_create(void)
{
	GtkWidget *vbox1;

	GtkWidget *notebook;
	GtkWidget *vbox_tab;

	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *checkbtn_savemsg;
	GtkWidget *checkbtn_filter_sent;
	GtkWidget *checkbtn_recipients_autoreg;
	GtkWidget *checkbtn_show_send_dialog;
	GtkWidget *label;
	GtkWidget *checkbtn_check_attach;
	GtkWidget *entry_check_attach_str;
	GtkWidget *checkbtn_check_recp;
	GtkWidget *entry_check_recp_excl;

	GtkWidget *optmenu_trencoding;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;
	GtkWidget *optmenu_fencoding;
	GtkWidget *label_encoding;
	GtkWidget *label_encoding_desc;
	GtkWidget *label_fencoding;
	GtkWidget *label_fencoding_desc;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(vbox1), notebook, TRUE, TRUE, 0);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("General"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox_tab), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_savemsg,
			   _("Save sent messages to outbox"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_filter_sent,
			   _("Apply filter rules to sent messages"));
	SET_TOGGLE_SENSITIVITY (checkbtn_savemsg, checkbtn_filter_sent);
	PACK_CHECK_BUTTON (vbox2, checkbtn_recipients_autoreg,
			   _("Automatically add recipients to address book"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_show_send_dialog,
			   _("Display send dialog"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox_tab), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_check_attach,
			   _("Notify for missing attachments when the following strings (comma-separated) are found in the message body"));
	gtk_label_set_line_wrap(GTK_LABEL(GTK_BIN(checkbtn_check_attach)->child), TRUE);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	entry_check_attach_str = gtk_entry_new ();
	gtk_widget_show (entry_check_attach_str);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_check_attach_str,
			    TRUE, TRUE, 0);
	label = gtk_label_new (_("(Ex: attach)"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_check_attach, entry_check_attach_str);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox_tab), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_check_recp,
			   _("Confirm recipients before sending"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);
	label = gtk_label_new
		(_("Excluded addresses/domains (comma-separated):"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	entry_check_recp_excl = gtk_entry_new ();
	gtk_widget_show (entry_check_recp_excl);
	gtk_box_pack_start (GTK_BOX (vbox2), entry_check_recp_excl,
                            FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_check_recp, entry_check_recp_excl);

	/* Encoding */

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Encoding"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_tab), hbox1, FALSE, FALSE, 0);

	label_encoding = gtk_label_new (_("Transfer encoding"));
	gtk_widget_show (label_encoding);
	gtk_box_pack_start (GTK_BOX (hbox1), label_encoding, FALSE, FALSE, 0);

	optmenu_trencoding = gtk_option_menu_new ();
	gtk_widget_show (optmenu_trencoding);
	gtk_box_pack_start (GTK_BOX (hbox1), optmenu_trencoding,
			    FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new();

#define SET_MENUITEM(str, data) \
	MENUITEM_ADD(optmenu_menu, menuitem, str, data)

	SET_MENUITEM(_("Automatic"),	 CTE_AUTO);
	SET_MENUITEM("base64",		 CTE_BASE64);
	SET_MENUITEM("quoted-printable", CTE_QUOTED_PRINTABLE);
	SET_MENUITEM("8bit",		 CTE_8BIT);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_trencoding),
				  optmenu_menu);

	PACK_SMALL_LABEL (vbox_tab, label_encoding_desc,
			  _("Specify Content-Transfer-Encoding used when "
			    "message body contains non-ASCII characters."));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_tab), hbox1, FALSE, FALSE, 0);

	label_fencoding = gtk_label_new (_("MIME filename encoding"));
	gtk_widget_show (label_fencoding);
	gtk_box_pack_start (GTK_BOX (hbox1), label_fencoding, FALSE, FALSE, 0);

	optmenu_fencoding = gtk_option_menu_new ();
	gtk_widget_show (optmenu_fencoding);
	gtk_box_pack_start (GTK_BOX (hbox1), optmenu_fencoding,
			    FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new();

	SET_MENUITEM(_("MIME header"), FENC_MIME);
	SET_MENUITEM("RFC 2231", FENC_RFC2231);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_fencoding),
				  optmenu_menu);

#undef SET_MENUITEM

	PACK_SMALL_LABEL
		(vbox_tab, label_fencoding_desc,
		 _("Specify encoding method for MIME filename with "
		   "non-ASCII characters.\n"
		   "MIME header: most popular, but violates RFC 2047\n"
		   "RFC 2231: conforms to standard, but not popular"));


	p_send.checkbtn_savemsg = checkbtn_savemsg;
	p_send.checkbtn_filter_sent = checkbtn_filter_sent;
	p_send.checkbtn_recipients_autoreg = checkbtn_recipients_autoreg;
	p_send.checkbtn_show_send_dialog = checkbtn_show_send_dialog;

	p_send.optmenu_encoding_method = optmenu_trencoding;
	p_send.optmenu_mime_fencoding_method = optmenu_fencoding;

	p_send.checkbtn_check_attach = checkbtn_check_attach;
	p_send.entry_check_attach_str = entry_check_attach_str;
	p_send.checkbtn_check_recp = checkbtn_check_recp;
	p_send.entry_check_recp_excl = entry_check_recp_excl;
}

static void prefs_compose_create(void)
{
	GtkWidget *vbox1;

	GtkWidget *notebook;
	GtkWidget *vbox_tab;

	GtkWidget *hbox1;
	GtkWidget *hbox2;

	GtkWidget *frame_sig;
	GtkWidget *checkbtn_autosig;
	GtkWidget *label_sigsep;
	GtkWidget *entry_sigsep;

	GtkWidget *vbox2;
	GtkWidget *checkbtn_autoextedit;
	GtkWidget *vbox3;
	GtkWidget *hbox3;
	GtkWidget *hbox4;
	GtkWidget *label_undolevel;
	GtkObject *spinbtn_undolevel_adj;
	GtkWidget *spinbtn_undolevel;
	GtkWidget *label_linewrap;
	GtkObject *spinbtn_linewrap_adj;
	GtkWidget *spinbtn_linewrap;
	GtkWidget *checkbtn_wrapquote;
	GtkWidget *checkbtn_autowrap;

	GtkWidget *hbox_autosave;
	GtkWidget *checkbtn_autosave;
	GtkWidget *label_autosave1;
	GtkObject *spinbtn_autosave_adj;
	GtkWidget *spinbtn_autosave;
	GtkWidget *label_autosave2;

	GtkWidget *frame_reply;
	GtkWidget *checkbtn_reply_account_autosel;
	GtkWidget *checkbtn_quote;
	GtkWidget *checkbtn_default_reply_list;
	GtkWidget *checkbtn_inherit_recipient_on_self_reply;
	GtkWidget *checkbtn_reply_address_only;

	GtkWidget *quote_wid;
#if USE_GTKSPELL
	GtkWidget *spell_wid;
#endif

	vbox1 = gtk_vbox_new(FALSE, VSPACING);
	gtk_widget_show(vbox1);
	gtk_container_add(GTK_CONTAINER(dialog.notebook), vbox1);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), VBOX_BORDER);

	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(vbox1), notebook, TRUE, TRUE, 0);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("General"));

	/* signature */

	PACK_FRAME(vbox_tab, frame_sig, _("Signature"));

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_container_add (GTK_CONTAINER (frame_sig), hbox1);
	gtk_container_set_border_width (GTK_CONTAINER (hbox1), 8);

	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox2, FALSE, FALSE, 0);

	label_sigsep = gtk_label_new (_("Signature separator"));
	gtk_widget_show (label_sigsep);
	gtk_box_pack_start (GTK_BOX (hbox2), label_sigsep, FALSE, FALSE, 0);

	entry_sigsep = gtk_entry_new ();
	gtk_widget_show (entry_sigsep);
	gtk_box_pack_start (GTK_BOX (hbox2), entry_sigsep, FALSE, FALSE, 0);
	gtk_widget_set_size_request (entry_sigsep, 64, -1);

	PACK_CHECK_BUTTON (hbox1, checkbtn_autosig, _("Insert automatically"));

	PACK_FRAME(vbox_tab, frame_reply, _("Reply"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_reply), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON (vbox2, checkbtn_reply_account_autosel,
			   _("Automatically select account for replies"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_quote,
			   _("Quote message when replying"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_default_reply_list,
			   _("Reply to mailing list by Reply button"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_inherit_recipient_on_self_reply,
			   _("Inherit recipients on reply to self messages"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_reply_address_only,
			   _("Set only mail address of recipients when replying"));

	/* editor */

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Editor"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (vbox_tab), vbox2);

	PACK_CHECK_BUTTON (vbox2, checkbtn_autoextedit,
			   _("Automatically launch the external editor"));

	PACK_VSPACER (vbox2, vbox3, VSPACING_NARROW_2);

	/* undo */

	hbox3 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox3);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);

	label_undolevel = gtk_label_new (_("Undo level"));
	gtk_widget_show (label_undolevel);
	gtk_box_pack_start (GTK_BOX (hbox3), label_undolevel, FALSE, FALSE, 0);

	spinbtn_undolevel_adj = gtk_adjustment_new (50, 0, 100, 1, 10, 0);
	spinbtn_undolevel = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_undolevel_adj), 1, 0);
	gtk_widget_show (spinbtn_undolevel);
	gtk_box_pack_start (GTK_BOX (hbox3), spinbtn_undolevel, FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_undolevel, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_undolevel), TRUE);

	PACK_VSPACER (vbox2, vbox3, VSPACING_NARROW_2);

	/* line-wrapping */

	hbox3 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox3);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);

	label_linewrap = gtk_label_new (_("Wrap messages at"));
	gtk_widget_show (label_linewrap);
	gtk_box_pack_start (GTK_BOX (hbox3), label_linewrap, FALSE, FALSE, 0);

	spinbtn_linewrap_adj = gtk_adjustment_new (72, 20, 1024, 1, 10, 0);
	spinbtn_linewrap = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_linewrap_adj), 1, 0);
	gtk_widget_show (spinbtn_linewrap);
	gtk_box_pack_start (GTK_BOX (hbox3), spinbtn_linewrap, FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_linewrap, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_linewrap), TRUE);

	label_linewrap = gtk_label_new (_("characters"));
	gtk_widget_show (label_linewrap);
	gtk_box_pack_start (GTK_BOX (hbox3), label_linewrap, FALSE, FALSE, 0);

	PACK_VSPACER (vbox2, vbox3, VSPACING_NARROW_2);

	hbox4 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox4);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox4, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox4, checkbtn_wrapquote, _("Wrap quotation"));

	hbox4 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox4);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox4, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox4, checkbtn_autowrap, _("Wrap on input"));

	PACK_VSPACER (vbox2, vbox3, VSPACING_NARROW_2);

	hbox_autosave = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_autosave);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_autosave, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox_autosave, checkbtn_autosave,
			   _("Auto-save to draft"));

	label_autosave1 = gtk_label_new (_("every"));
	gtk_widget_show (label_autosave1);
	gtk_box_pack_start (GTK_BOX (hbox_autosave), label_autosave1,
			    FALSE, FALSE, 0);

	spinbtn_autosave_adj = gtk_adjustment_new (5, 1, 100, 1, 10, 0);
	spinbtn_autosave = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_autosave_adj), 1, 0);
	gtk_widget_show (spinbtn_autosave);
	gtk_box_pack_start (GTK_BOX (hbox_autosave), spinbtn_autosave,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_autosave, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_autosave), TRUE);

	label_autosave2 = gtk_label_new (_("minute(s)"));
	gtk_widget_show (label_autosave2);
	gtk_box_pack_start (GTK_BOX (hbox_autosave), label_autosave2,
			    FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_autosave, label_autosave1);
	SET_TOGGLE_SENSITIVITY(checkbtn_autosave, spinbtn_autosave);
	SET_TOGGLE_SENSITIVITY(checkbtn_autosave, label_autosave2);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Format"));
	quote_wid = prefs_quote_create();
	gtk_box_pack_start(GTK_BOX(vbox_tab), quote_wid, FALSE, FALSE, 0);

#if USE_GTKSPELL
	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Spell checking"));
	spell_wid = prefs_spell_create();
	gtk_box_pack_start(GTK_BOX(vbox_tab), spell_wid, FALSE, FALSE, 0);
#endif

	compose.checkbtn_autosig = checkbtn_autosig;
	compose.entry_sigsep     = entry_sigsep;

	compose.checkbtn_autoextedit = checkbtn_autoextedit;

	compose.spinbtn_undolevel     = spinbtn_undolevel;
	compose.spinbtn_undolevel_adj = spinbtn_undolevel_adj;

	compose.spinbtn_linewrap     = spinbtn_linewrap;
	compose.spinbtn_linewrap_adj = spinbtn_linewrap_adj;
	compose.checkbtn_wrapquote   = checkbtn_wrapquote;
	compose.checkbtn_autowrap    = checkbtn_autowrap;

	compose.checkbtn_autosave    = checkbtn_autosave;
	compose.spinbtn_autosave     = spinbtn_autosave;
	compose.spinbtn_autosave_adj = spinbtn_autosave_adj;

	compose.checkbtn_quote = checkbtn_quote;
	compose.checkbtn_reply_account_autosel =
		checkbtn_reply_account_autosel;
	compose.checkbtn_default_reply_list = checkbtn_default_reply_list;
	compose.checkbtn_inherit_recipient_on_self_reply =
		checkbtn_inherit_recipient_on_self_reply;
	compose.checkbtn_reply_address_only = checkbtn_reply_address_only;
}

static GtkWidget *prefs_quote_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *frame_quote;
	GtkWidget *vbox_quote;
	GtkWidget *hbox1;
	GtkWidget *hbox2;
	GtkWidget *label_quotemark;
	GtkWidget *entry_quotemark;
	GtkWidget *scrolledwin_quotefmt;
	GtkWidget *text_quotefmt;

	GtkWidget *entry_fw_quotemark;
	GtkWidget *text_fw_quotefmt;

	GtkWidget *btn_quotedesc;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);

	/* reply */

	PACK_FRAME (vbox1, frame_quote, _("Reply format"));

	vbox_quote = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox_quote);
	gtk_container_add (GTK_CONTAINER (frame_quote), vbox_quote);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_quote), 8);

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_quote), hbox1, FALSE, FALSE, 0);

	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox2, FALSE, FALSE, 0);

	label_quotemark = gtk_label_new (_("Quotation mark"));
	gtk_widget_show (label_quotemark);
	gtk_box_pack_start (GTK_BOX (hbox2), label_quotemark, FALSE, FALSE, 0);

	entry_quotemark = gtk_entry_new ();
	gtk_widget_show (entry_quotemark);
	gtk_box_pack_start (GTK_BOX (hbox2), entry_quotemark, FALSE, FALSE, 0);
	gtk_widget_set_size_request (entry_quotemark, 64, -1);

	scrolledwin_quotefmt = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwin_quotefmt);
	gtk_box_pack_start (GTK_BOX (vbox_quote), scrolledwin_quotefmt,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW (scrolledwin_quotefmt),
		 GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type
		(GTK_SCROLLED_WINDOW (scrolledwin_quotefmt), GTK_SHADOW_IN);

	text_quotefmt = gtk_text_view_new ();
	gtk_widget_show (text_quotefmt);
	gtk_container_add(GTK_CONTAINER(scrolledwin_quotefmt), text_quotefmt);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (text_quotefmt), TRUE);
	gtk_widget_set_size_request(text_quotefmt, DEFAULT_ENTRY_WIDTH, 60);

	/* forward */

	PACK_FRAME (vbox1, frame_quote, _("Forward format"));

	vbox_quote = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox_quote);
	gtk_container_add (GTK_CONTAINER (frame_quote), vbox_quote);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_quote), 8);

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_quote), hbox1, FALSE, FALSE, 0);

	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox2, FALSE, FALSE, 0);

	label_quotemark = gtk_label_new (_("Quotation mark"));
	gtk_widget_show (label_quotemark);
	gtk_box_pack_start (GTK_BOX (hbox2), label_quotemark, FALSE, FALSE, 0);

	entry_fw_quotemark = gtk_entry_new ();
	gtk_widget_show (entry_fw_quotemark);
	gtk_box_pack_start (GTK_BOX (hbox2), entry_fw_quotemark,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (entry_fw_quotemark, 64, -1);

	scrolledwin_quotefmt = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwin_quotefmt);
	gtk_box_pack_start (GTK_BOX (vbox_quote), scrolledwin_quotefmt,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW (scrolledwin_quotefmt),
		 GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type
		(GTK_SCROLLED_WINDOW (scrolledwin_quotefmt), GTK_SHADOW_IN);

	text_fw_quotefmt = gtk_text_view_new ();
	gtk_widget_show (text_fw_quotefmt);
	gtk_container_add(GTK_CONTAINER(scrolledwin_quotefmt),
			  text_fw_quotefmt);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (text_fw_quotefmt), TRUE);
	gtk_widget_set_size_request (text_fw_quotefmt, DEFAULT_ENTRY_WIDTH, 60);

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	btn_quotedesc =
		gtk_button_new_with_label (_(" Description of symbols "));
	gtk_widget_show (btn_quotedesc);
	gtk_box_pack_start (GTK_BOX (hbox1), btn_quotedesc, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(btn_quotedesc), "clicked",
			 G_CALLBACK(prefs_quote_description), NULL);

	quote.entry_quotemark    = entry_quotemark;
	quote.text_quotefmt      = text_quotefmt;
	quote.entry_fw_quotemark = entry_fw_quotemark;
	quote.text_fw_quotefmt   = text_fw_quotefmt;

	return vbox1;
}

#if USE_GTKSPELL
static GtkWidget *prefs_spell_create(void)
{
        GtkWidget *vbox1;
	GtkWidget *vbox2;
        GtkWidget *frame;
        GtkWidget *hbox;
        GtkWidget *chkbtn_enable_spell;
        GtkWidget *label;
        GtkWidget *entry_spell_lang;
 
        vbox1 = gtk_vbox_new (FALSE, VSPACING);
        gtk_widget_show (vbox1);
 
        PACK_FRAME_WITH_CHECK_BUTTON(vbox1, frame, chkbtn_enable_spell,
                                     _("Enable Spell checking"));
 
        vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
        gtk_widget_show (vbox2);
        gtk_container_add (GTK_CONTAINER (frame), vbox2);
        gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
        SET_TOGGLE_SENSITIVITY (chkbtn_enable_spell, vbox2);
 
        hbox = gtk_hbox_new (FALSE, 8);
        gtk_widget_show (hbox);
        gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
 
        label = gtk_label_new (_("Default language:"));
        gtk_widget_show (label);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
 
        entry_spell_lang = gtk_entry_new ();
        gtk_widget_show (entry_spell_lang);
        gtk_box_pack_start (GTK_BOX (hbox), entry_spell_lang, TRUE, TRUE, 0);
 
        spell.chkbtn_enable_spell = chkbtn_enable_spell;
        spell.entry_spell_lang = entry_spell_lang;

	return vbox1;
}
#endif /* USE_GTKSPELL */

static void prefs_display_create(void)
{
	GtkWidget *vbox1;

	GtkWidget *notebook;
	GtkWidget *vbox_tab;

	GtkWidget *table1;
	GtkWidget *label_textfont;
	GtkWidget *fontbtn_textfont;
	GtkWidget *frame_folder;
	GtkWidget *chkbtn_folder_unread;
	GtkWidget *label_folder_cols;
	GtkWidget *hbox1;
	GtkWidget *hbox_spc;
	GtkWidget *chkbtn_folder_col_new;
	GtkWidget *chkbtn_folder_col_unread;
	GtkWidget *chkbtn_folder_col_total;
	GtkWidget *label_ng_abbrev;
	GtkWidget *spinbtn_ng_abbrev_len;
	GtkObject *spinbtn_ng_abbrev_len_adj;
	GtkWidget *frame_summary;
	GtkWidget *vbox2;
	GtkWidget *chkbtn_swapfrom;
	GtkWidget *chkbtn_expand_thread;
	GtkWidget *vbox3;
	GtkWidget *label_datefmt;
	GtkWidget *button_datefmt;
	GtkWidget *entry_datefmt;
	GtkWidget *button_dispitem;

	GtkWidget *msg_wid;
	GtkWidget *att_wid;
	GtkWidget *clabel_wid;

	GtkWidget *label_dispencoding;
	GtkWidget *optmenu_dispencoding;
	GtkWidget *label_outencoding;
	GtkWidget *optmenu_outencoding;
	GtkWidget *label_encoding_desc;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(vbox1), notebook, TRUE, TRUE, 0);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("General"));

	table1 = gtk_table_new (1, 2, FALSE);
	gtk_widget_show (table1);
	gtk_box_pack_start(GTK_BOX(vbox_tab), table1, FALSE, FALSE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table1), 8);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 8);

	label_textfont = gtk_label_new (_("Text font"));
	gtk_widget_show (label_textfont);
	gtk_table_attach (GTK_TABLE (table1), label_textfont, 0, 1, 0, 1,
			  GTK_FILL, (GTK_EXPAND | GTK_FILL), 0, 0);

	fontbtn_textfont = gtk_font_button_new ();
	gtk_widget_show (fontbtn_textfont);
	gtk_table_attach (GTK_TABLE (table1), fontbtn_textfont, 1, 2, 0, 1,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	/* ---- Folder View ---- */

	PACK_FRAME(vbox_tab, frame_folder, _("Folder View"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_folder), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON (vbox2, chkbtn_folder_unread,
			   _("Display unread number next to folder name"));

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);
	label_folder_cols = gtk_label_new
		(_("Displaying message number columns in the folder view:"));
	gtk_widget_show(label_folder_cols);
	gtk_box_pack_start(GTK_BOX(vbox2), label_folder_cols, FALSE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(label_folder_cols), 0, 0.5);

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);
	hbox1 = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox1);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox1, FALSE, TRUE, 0);
	hbox_spc = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox_spc);
	gtk_box_pack_start(GTK_BOX(hbox1), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request(hbox_spc, 8, -1);
	PACK_CHECK_BUTTON(hbox1, chkbtn_folder_col_new, _("New"));
	PACK_CHECK_BUTTON(hbox1, chkbtn_folder_col_unread, _("Unread"));
	PACK_CHECK_BUTTON(hbox1, chkbtn_folder_col_total, _("Total"));

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	label_ng_abbrev = gtk_label_new
		(_("Abbreviate newsgroups longer than"));
	gtk_widget_show (label_ng_abbrev);
	gtk_box_pack_start (GTK_BOX (hbox1), label_ng_abbrev, FALSE, FALSE, 0);

	spinbtn_ng_abbrev_len_adj = gtk_adjustment_new (16, 0, 999, 1, 10, 0);
	spinbtn_ng_abbrev_len = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_ng_abbrev_len_adj), 1, 0);
	gtk_widget_show (spinbtn_ng_abbrev_len);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_ng_abbrev_len,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_ng_abbrev_len, 56, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_ng_abbrev_len),
				     TRUE);

	label_ng_abbrev = gtk_label_new
		(_("letters"));
	gtk_widget_show (label_ng_abbrev);
	gtk_box_pack_start (GTK_BOX (hbox1), label_ng_abbrev, FALSE, FALSE, 0);

	/* ---- Summary ---- */

	PACK_FRAME(vbox_tab, frame_summary, _("Summary View"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_summary), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON
		(vbox2, chkbtn_swapfrom,
		 _("Display recipient on `From' column if sender is yourself"));
	PACK_CHECK_BUTTON
		(vbox2, chkbtn_expand_thread, _("Expand threads"));

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	label_datefmt = gtk_label_new (_("Date format"));
	gtk_widget_show (label_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), label_datefmt, FALSE, FALSE, 0);

	entry_datefmt = gtk_entry_new ();
	gtk_widget_show (entry_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_datefmt, TRUE, TRUE, 0);

	button_datefmt = gtk_button_new_with_label ("... ");
	gtk_widget_show (button_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), button_datefmt, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_datefmt), "clicked",
			  G_CALLBACK (date_format_create), NULL);

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	button_dispitem = gtk_button_new_with_label
		(_(" Set display item of summary... "));
	gtk_widget_show (button_dispitem);
	gtk_box_pack_start (GTK_BOX (hbox1), button_dispitem, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (button_dispitem), "clicked",
			  G_CALLBACK (prefs_common_dispitem_clicked), NULL);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Message"));
	msg_wid = prefs_message_create();
	gtk_box_pack_start(GTK_BOX(vbox_tab), msg_wid, FALSE, FALSE, 0);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Attachment"));
	att_wid = prefs_attach_create();
	gtk_box_pack_start(GTK_BOX(vbox_tab), att_wid, FALSE, FALSE, 0);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Color label"));
	clabel_wid = prefs_colorlabel_create();
	gtk_box_pack_start(GTK_BOX(vbox_tab), clabel_wid, FALSE, FALSE, 0);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Encoding"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_tab), hbox1, FALSE, FALSE, 0);

	label_dispencoding = gtk_label_new (_("Default character encoding"));
	gtk_widget_show (label_dispencoding);
	gtk_box_pack_start (GTK_BOX (hbox1), label_dispencoding,
			    FALSE, FALSE, 0);

	optmenu_dispencoding = gtk_option_menu_new ();
	gtk_widget_show (optmenu_dispencoding);
	gtk_box_pack_start (GTK_BOX (hbox1), optmenu_dispencoding,
			    FALSE, FALSE, 0);

	prefs_common_set_encoding_optmenu
		(GTK_OPTION_MENU (optmenu_dispencoding), FALSE);

	PACK_SMALL_LABEL (vbox_tab, label_encoding_desc,
			  _("This is used when displaying messages with missing character encoding."));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_tab), hbox1, FALSE, FALSE, 0);

	label_outencoding = gtk_label_new (_("Outgoing character encoding"));
	gtk_widget_show (label_outencoding);
	gtk_box_pack_start (GTK_BOX (hbox1), label_outencoding,
			    FALSE, FALSE, 0);

	optmenu_outencoding = gtk_option_menu_new ();
	gtk_widget_show (optmenu_outencoding);
	gtk_box_pack_start (GTK_BOX (hbox1), optmenu_outencoding,
			    FALSE, FALSE, 0);

	prefs_common_set_encoding_optmenu
		(GTK_OPTION_MENU (optmenu_outencoding), TRUE);

	PACK_SMALL_LABEL (vbox_tab, label_encoding_desc,
			  _("If `Automatic' is selected, the optimal encoding "
			    "for the current locale will be used."));

	display.fontbtn_textfont = fontbtn_textfont;

	display.chkbtn_folder_unread      = chkbtn_folder_unread;
	display.chkbtn_folder_col_new     = chkbtn_folder_col_new;
	display.chkbtn_folder_col_unread  = chkbtn_folder_col_unread;
	display.chkbtn_folder_col_total   = chkbtn_folder_col_total;
	display.spinbtn_ng_abbrev_len     = spinbtn_ng_abbrev_len;
	display.spinbtn_ng_abbrev_len_adj = spinbtn_ng_abbrev_len_adj;

	display.chkbtn_swapfrom      = chkbtn_swapfrom;
	display.chkbtn_expand_thread = chkbtn_expand_thread;
	display.entry_datefmt        = entry_datefmt;

	display.optmenu_dispencoding = optmenu_dispencoding;
	display.optmenu_outencoding  = optmenu_outencoding;
}

static GtkWidget *prefs_message_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *hbox1;
	GtkWidget *chkbtn_enablecol;
	GtkWidget *button_edit_col;
	GtkWidget *chkbtn_disphdrpane;
	GtkWidget *chkbtn_disphdr;
	GtkWidget *button_edit_disphdr;
	GtkWidget *chkbtn_html;
	GtkWidget *chkbtn_prefer_html;
	GtkWidget *chkbtn_htmlonly;
	GtkWidget *hbox_linespc;
	GtkWidget *label_linespc;
	GtkObject *spinbtn_linespc_adj;
	GtkWidget *spinbtn_linespc;

	GtkWidget *frame_scr;
	GtkWidget *vbox_scr;
	GtkWidget *chkbtn_smoothscroll;
	GtkWidget *hbox_scr;
	GtkWidget *label_scr;
	GtkObject *spinbtn_scrollstep_adj;
	GtkWidget *spinbtn_scrollstep;
	GtkWidget *chkbtn_halfpage;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON (hbox1, chkbtn_enablecol,
			   _("Enable coloration of message"));
	g_signal_connect(G_OBJECT(chkbtn_enablecol), "toggled",
			 G_CALLBACK(prefs_enable_message_color_toggled), NULL);

	button_edit_col = gtk_button_new_with_label (_(" Edit... "));
	gtk_widget_show (button_edit_col);
	gtk_box_pack_end (GTK_BOX (hbox1), button_edit_col, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (button_edit_col), "clicked",
			  G_CALLBACK (prefs_quote_colors_dialog), NULL);

	SET_TOGGLE_SENSITIVITY(chkbtn_enablecol, button_edit_col);

#if 0
	PACK_CHECK_BUTTON
		(vbox2, chkbtn_mbalnum,
		 _("Display multi-byte alphabet and numeric as\n"
		   "ASCII character (Japanese only)"));
	gtk_label_set_justify (GTK_LABEL (GTK_BIN(chkbtn_mbalnum)->child),
			       GTK_JUSTIFY_LEFT);
#endif

	PACK_CHECK_BUTTON(vbox2, chkbtn_disphdrpane,
			  _("Display header pane above message view"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON(hbox1, chkbtn_disphdr,
			  _("Display short headers on message view"));

	button_edit_disphdr = gtk_button_new_with_label (_(" Edit... "));
	gtk_widget_show (button_edit_disphdr);
	gtk_box_pack_end (GTK_BOX (hbox1), button_edit_disphdr,
			  FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (button_edit_disphdr), "clicked",
			  G_CALLBACK (prefs_display_header_open), NULL);

	SET_TOGGLE_SENSITIVITY(chkbtn_disphdr, button_edit_disphdr);

	PACK_CHECK_BUTTON(vbox2, chkbtn_html,
			  _("Render HTML messages as text"));
	PACK_CHECK_BUTTON(vbox2, chkbtn_prefer_html,
			  _("Prefer HTML in multipart/alternative for display"));
	PACK_CHECK_BUTTON(vbox2, chkbtn_htmlonly,
			  _("Treat HTML only messages as attachment"));

#if 0
	PACK_CHECK_BUTTON(vbox2, chkbtn_cursor,
			  _("Display cursor in message view"));
#endif

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	hbox_linespc = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_linespc, FALSE, TRUE, 0);

	label_linespc = gtk_label_new (_("Line space"));
	gtk_widget_show (label_linespc);
	gtk_box_pack_start (GTK_BOX (hbox_linespc), label_linespc,
			    FALSE, FALSE, 0);

	spinbtn_linespc_adj = gtk_adjustment_new (2, 0, 16, 1, 1, 0);
	spinbtn_linespc = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_linespc_adj), 1, 0);
	gtk_widget_show (spinbtn_linespc);
	gtk_box_pack_start (GTK_BOX (hbox_linespc), spinbtn_linespc,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_linespc, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_linespc), TRUE);

	label_linespc = gtk_label_new (_("pixel(s)"));
	gtk_widget_show (label_linespc);
	gtk_box_pack_start (GTK_BOX (hbox_linespc), label_linespc,
			    FALSE, FALSE, 0);

	PACK_FRAME(vbox1, frame_scr, _("Scroll"));

	vbox_scr = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_scr);
	gtk_container_add (GTK_CONTAINER (frame_scr), vbox_scr);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_scr), 8);

	PACK_CHECK_BUTTON(vbox_scr, chkbtn_halfpage, _("Half page"));

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_scr), hbox1, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON(hbox1, chkbtn_smoothscroll, _("Smooth scroll"));

	hbox_scr = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_scr);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_scr, FALSE, FALSE, 0);

	label_scr = gtk_label_new (_("Step"));
	gtk_widget_show (label_scr);
	gtk_box_pack_start (GTK_BOX (hbox_scr), label_scr, FALSE, FALSE, 0);

	spinbtn_scrollstep_adj = gtk_adjustment_new (1, 1, 100, 1, 10, 0);
	spinbtn_scrollstep = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_scrollstep_adj), 1, 0);
	gtk_widget_show (spinbtn_scrollstep);
	gtk_box_pack_start (GTK_BOX (hbox_scr), spinbtn_scrollstep,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_scrollstep, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_scrollstep),
				     TRUE);

	label_scr = gtk_label_new (_("pixel(s)"));
	gtk_widget_show (label_scr);
	gtk_box_pack_start (GTK_BOX (hbox_scr), label_scr, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (chkbtn_smoothscroll, hbox_scr)

	message.chkbtn_enablecol   = chkbtn_enablecol;
	message.button_edit_col    = button_edit_col;
	message.chkbtn_disphdrpane = chkbtn_disphdrpane;
	message.chkbtn_disphdr     = chkbtn_disphdr;
	message.chkbtn_html        = chkbtn_html;
	message.chkbtn_prefer_html = chkbtn_prefer_html;
	message.chkbtn_htmlonly    = chkbtn_htmlonly;
	message.spinbtn_linespc    = spinbtn_linespc;

	message.chkbtn_smoothscroll    = chkbtn_smoothscroll;
	message.spinbtn_scrollstep     = spinbtn_scrollstep;
	message.spinbtn_scrollstep_adj = spinbtn_scrollstep_adj;
	message.chkbtn_halfpage        = chkbtn_halfpage;

	return vbox1;
}

static GtkWidget *prefs_attach_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *label;
	GtkWidget *radiobtn_attach_toolbtn_pos;
	GtkWidget *radiobtn_attach_toolbtn_pos2;
	GtkWidget *chkbtn_show_attach_tab;
	GtkWidget *chkbtn_show_files_first;
	GtkWidget *frame_image;
	GtkWidget *vbox_image;
	GtkWidget *chkbtn_resize_image;
	GtkWidget *chkbtn_inline_image;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new (_("Position of attachment tool button:"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	radiobtn_attach_toolbtn_pos = gtk_radio_button_new_with_label
		(NULL, _("Left"));
	gtk_widget_show (radiobtn_attach_toolbtn_pos);
	gtk_box_pack_start (GTK_BOX(hbox1), radiobtn_attach_toolbtn_pos,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (radiobtn_attach_toolbtn_pos), MENU_VAL_ID,
			   GINT_TO_POINTER (0));

	radiobtn_attach_toolbtn_pos2 = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (radiobtn_attach_toolbtn_pos), _("Right"));
	gtk_widget_show (radiobtn_attach_toolbtn_pos2);
	gtk_box_pack_start (GTK_BOX (hbox1), radiobtn_attach_toolbtn_pos2,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (radiobtn_attach_toolbtn_pos2), MENU_VAL_ID,
			   GINT_TO_POINTER (1));

	PACK_CHECK_BUTTON (vbox2, chkbtn_show_attach_tab,
			   _("Toggle attachment list view with tab"));
	SET_TOGGLE_SENSITIVITY_REV (chkbtn_show_attach_tab, hbox1);

	PACK_CHECK_BUTTON (vbox2, chkbtn_show_files_first,
			   _("Show attached files first on message view"));

	PACK_FRAME(vbox1, frame_image, _("Images"));

	vbox_image = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_image);
	gtk_container_add (GTK_CONTAINER (frame_image), vbox_image);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_image), 8);

	PACK_CHECK_BUTTON(vbox_image, chkbtn_resize_image,
			  _("Resize attached large images to fit in the window"));
	PACK_CHECK_BUTTON(vbox_image, chkbtn_inline_image,
			  _("Display images as inline"));

	attach.chkbtn_show_attach_tab  = chkbtn_show_attach_tab;
	attach.radiobtn_attach_toolbtn_pos = radiobtn_attach_toolbtn_pos;
	attach.chkbtn_show_files_first = chkbtn_show_files_first;

	attach.chkbtn_resize_image = chkbtn_resize_image;
	attach.chkbtn_inline_image = chkbtn_inline_image;

	return vbox1;
}

static GtkWidget *prefs_colorlabel_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *clabel;
	GtkWidget *entry;
	gint i;

	vbox = gtk_vbox_new(FALSE, VSPACING);
	gtk_widget_show(vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

	label = gtk_label_new(_("You can specify label names for each color (Work, TODO etc.)."));
	gtk_widget_show(label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	table = gtk_table_new(7, 2, FALSE);
	gtk_widget_show(table);
	gtk_container_add(GTK_CONTAINER(vbox), table);
	gtk_table_set_row_spacings(GTK_TABLE(table), VSPACING_NARROW);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	for (i = 0; i < 7; i++) {
		clabel = colorlabel_create_color_widget
			(colorlabel_get_color(i));
		gtk_widget_show(clabel);
		gtk_table_attach(GTK_TABLE(table), clabel, 0, 1, i, i + 1,
				 GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 6);
		entry  = gtk_entry_new();
		gtk_widget_show(entry);
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, i, i + 1,
				 GTK_EXPAND | GTK_FILL, 0, 0, 0);
		colorlabel.entry_color[i] = entry;
	}

	return vbox;
}

static void prefs_common_colorlabel_set_dialog(void)
{
	gint i;
	const gchar *text;

	for (i = 0; i < 7; i++) {
		if ((text = colorlabel_get_custom_color_text(i))) {
			gtk_entry_set_text(GTK_ENTRY(colorlabel.entry_color[i]),
					   text);
		} else {
			gtk_entry_set_text(GTK_ENTRY(colorlabel.entry_color[i]),
					   "");
		}
	}
}

static void prefs_common_colorlabel_update(void)
{
	gint i;
	const gchar *text;

	for (i = 0; i < 7; i++) {
		text = gtk_entry_get_text(GTK_ENTRY(colorlabel.entry_color[i]));
		if (text && text[0] != '\0')
			colorlabel_set_color_text(i, text);
		else
			colorlabel_set_color_text(i, NULL);
	}
	colorlabel_update_menu();
}

static const struct {
	gchar *junk_cmd;
	gchar *nojunk_cmd;
	gchar *classify_cmd;
} junk_presets[] = {
#ifdef G_OS_WIN32
	{"bogofilter -N -s -I", "bogofilter -n -S -I", "bogofilter -I"},
	{"bsfilterw -C -s -u", "bsfilterw -c -S -u", "bsfilterw"},
	{"sylfilter -j", "sylfilter -c", "sylfilter"}
#else
	{"bogofilter -N -s -I", "bogofilter -n -S -I", "bogofilter -I"},
	{"bsfilter -C -s -u", "bsfilter -c -S -u", "bsfilter"},
	{"sylfilter -j", "sylfilter -c", "sylfilter"}
#endif
};

enum
{
	JUNK_NONE,
	JUNK_BOGOFILTER,
	JUNK_BSFILTER,
	JUNK_SYLFILTER,

	N_JUNK_ITEMS
};

static void prefs_junk_preset_activated(GtkMenuItem *menuitem, gpointer data)
{
	gint i;

	i = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
	if (i > JUNK_NONE && i < N_JUNK_ITEMS) {
		i--;
		gtk_entry_set_text(GTK_ENTRY(junk.entry_junk_learncmd),
				   junk_presets[i].junk_cmd); 
		gtk_entry_set_text(GTK_ENTRY(junk.entry_nojunk_learncmd),
				   junk_presets[i].nojunk_cmd); 
		gtk_entry_set_text(GTK_ENTRY(junk.entry_classify_cmd),
				   junk_presets[i].classify_cmd); 
	}
}

static void prefs_junk_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *chkbtn_enable_junk;
	GtkWidget *label;
	GtkWidget *optmenu_preset;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkWidget *entry_junk_learncmd;
	GtkWidget *entry_nojunk_learncmd;
	GtkWidget *entry_classify_cmd;
	GtkWidget *vbox3;
	GtkWidget *entry_junkfolder;
	GtkWidget *btn_folder;
	GtkWidget *chkbtn_filter_on_recv;
	GtkWidget *chkbtn_filter_before;
	GtkWidget *chkbtn_delete_on_recv;
	GtkWidget *chkbtn_nofilter_in_book;
	GtkWidget *chkbtn_mark_as_read;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME_WITH_CHECK_BUTTON(vbox1, frame, chkbtn_enable_junk,
				     _("Enable Junk mail control"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
	SET_TOGGLE_SENSITIVITY (chkbtn_enable_junk, vbox2);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Learning command:"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	optmenu_preset = gtk_option_menu_new ();
	gtk_widget_show (optmenu_preset);
	gtk_box_pack_end (GTK_BOX (hbox), optmenu_preset, FALSE, FALSE, 0);

	menu = gtk_menu_new ();
	MENUITEM_ADD (menu, menuitem, _("(Select preset)"), 0);
	MENUITEM_ADD (menu, menuitem, "bogofilter", JUNK_BOGOFILTER);
	g_signal_connect (G_OBJECT (menuitem), "activate",
			  G_CALLBACK (prefs_junk_preset_activated), NULL);
	MENUITEM_ADD (menu, menuitem, "bsfilter", JUNK_BSFILTER);
	g_signal_connect (G_OBJECT (menuitem), "activate",
			  G_CALLBACK (prefs_junk_preset_activated), NULL);
	MENUITEM_ADD (menu, menuitem, "sylfilter", JUNK_SYLFILTER);
	g_signal_connect (G_OBJECT (menuitem), "activate",
			  G_CALLBACK (prefs_junk_preset_activated), NULL);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_preset), menu);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Junk"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry_junk_learncmd = gtk_entry_new ();
	gtk_widget_show (entry_junk_learncmd);
	gtk_box_pack_start (GTK_BOX (hbox), entry_junk_learncmd, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Not Junk"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry_nojunk_learncmd = gtk_entry_new ();
	gtk_widget_show (entry_nojunk_learncmd);
	gtk_box_pack_start (GTK_BOX (hbox), entry_nojunk_learncmd,
			    TRUE, TRUE, 0);

	PACK_VSPACER(vbox2, vbox3, 0);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Classifying command"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry_classify_cmd = gtk_entry_new ();
	gtk_widget_show (entry_classify_cmd);
	gtk_box_pack_start (GTK_BOX (hbox), entry_classify_cmd, TRUE, TRUE, 0);

	PACK_VSPACER(vbox2, vbox3, 0);

	PACK_SMALL_LABEL (vbox2, label,
			  _("To classify junk mails automatically, both junk "
			    "and not junk mails must be learned manually to "
			    "a certain extent."));

	PACK_VSPACER(vbox2, vbox3, 0);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Junk folder"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry_junkfolder = gtk_entry_new ();
	gtk_widget_show (entry_junkfolder);
	gtk_box_pack_start (GTK_BOX (hbox), entry_junkfolder, TRUE, TRUE, 0);

	btn_folder = gtk_button_new_with_label (_(" ... "));
	gtk_widget_show (btn_folder);
	gtk_box_pack_start (GTK_BOX (hbox), btn_folder, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (btn_folder), "clicked",
			  G_CALLBACK (prefs_common_select_folder_cb),
			  entry_junkfolder);

	PACK_VSPACER(vbox2, vbox3, 0);

	PACK_SMALL_LABEL (vbox2, label,
			  _("The messages which are set as junk mail "
			    "will be moved to this folder. If empty, "
			    "the default junk folder will be used."));

	PACK_VSPACER(vbox2, vbox3, 0);

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX(vbox2), vbox3, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox3, chkbtn_filter_on_recv,
		 _("Filter messages classified as junk on receiving"));
	PACK_CHECK_BUTTON
		(vbox3, chkbtn_filter_before,
		 _("Filter junk mails before normal filtering"));
	PACK_CHECK_BUTTON
		(vbox3, chkbtn_delete_on_recv,
		 _("Delete junk mails from server on receiving"));
	SET_TOGGLE_SENSITIVITY (chkbtn_filter_on_recv, chkbtn_filter_before);
	SET_TOGGLE_SENSITIVITY (chkbtn_filter_on_recv, chkbtn_delete_on_recv);

	PACK_CHECK_BUTTON
		(vbox3, chkbtn_nofilter_in_book,
		 _("Do not classify message as junk if sender is in the address book"));
	PACK_CHECK_BUTTON (vbox3, chkbtn_mark_as_read,
			   _("Mark filtered junk mails as read"));

	junk.chkbtn_enable_junk      = chkbtn_enable_junk;
	junk.entry_junk_learncmd     = entry_junk_learncmd;
	junk.entry_nojunk_learncmd   = entry_nojunk_learncmd;
	junk.entry_classify_cmd      = entry_classify_cmd;
	junk.entry_junkfolder        = entry_junkfolder;
	junk.chkbtn_filter_on_recv   = chkbtn_filter_on_recv;
	junk.chkbtn_filter_before    = chkbtn_filter_before;
	junk.chkbtn_delete_on_recv   = chkbtn_delete_on_recv;
	junk.chkbtn_nofilter_in_book = chkbtn_nofilter_in_book;
	junk.chkbtn_mark_as_read     = chkbtn_mark_as_read;
}

#if USE_GPGME
static void prefs_privacy_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *hbox1;
	GtkWidget *hbox_spc;
	GtkWidget *label;
	GtkWidget *checkbtn_auto_check_signatures;
	GtkWidget *checkbtn_gpg_signature_popup;
	GtkWidget *checkbtn_store_passphrase;
	GtkObject *spinbtn_store_passphrase_adj;
	GtkWidget *spinbtn_store_passphrase;
#ifndef G_OS_WIN32
	GtkWidget *checkbtn_passphrase_grab;
#endif
	GtkWidget *checkbtn_gpg_warning;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_auto_check_signatures,
			   _("Automatically check signatures"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_gpg_signature_popup,
			   _("Show signature check result in a popup window"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_store_passphrase,
			   _("Store passphrase in memory temporarily"));

	vbox3 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	label = gtk_label_new (_("Expired after"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	spinbtn_store_passphrase_adj = gtk_adjustment_new (0, 0, 1440, 1, 5, 0);
	spinbtn_store_passphrase = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_store_passphrase_adj), 1, 0);
	gtk_widget_show (spinbtn_store_passphrase);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_store_passphrase, FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_store_passphrase),
				     TRUE);
	gtk_widget_set_size_request (spinbtn_store_passphrase, 64, -1);

	label = gtk_label_new (_("minute(s) "));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	PACK_SMALL_LABEL (hbox1, label,
			  _("Setting to '0' will store the passphrase "
			    "for the whole session."));

	SET_TOGGLE_SENSITIVITY (checkbtn_store_passphrase, vbox3);

	PACK_VSPACER (vbox2, vbox3, VSPACING_NARROW_2);

#ifndef G_OS_WIN32
	PACK_CHECK_BUTTON (vbox2, checkbtn_passphrase_grab,
			   _("Grab input while entering a passphrase"));
#endif

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_gpg_warning,
		 _("Display warning on startup if GnuPG doesn't work"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	privacy.checkbtn_auto_check_signatures
					     = checkbtn_auto_check_signatures;
	privacy.checkbtn_gpg_signature_popup
					     = checkbtn_gpg_signature_popup;
	privacy.checkbtn_store_passphrase    = checkbtn_store_passphrase;
	privacy.spinbtn_store_passphrase     = spinbtn_store_passphrase;
	privacy.spinbtn_store_passphrase_adj = spinbtn_store_passphrase_adj;
#ifndef G_OS_WIN32
	privacy.checkbtn_passphrase_grab     = checkbtn_passphrase_grab;
#endif
	privacy.checkbtn_gpg_warning         = checkbtn_gpg_warning;
}
#endif /* USE_GPGME */

static void prefs_details_create(void)
{
	GtkWidget *vbox1;

	GtkWidget *notebook;
	GtkWidget *vbox_tab;

	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *checkbtn_always_show_msg;
	GtkWidget *checkbtn_always_mark_read;
	GtkWidget *checkbtn_openunread;
	GtkWidget *checkbtn_remember_lastsel;
	/* GtkWidget *checkbtn_mark_as_read_on_newwin; */
	GtkWidget *checkbtn_openinbox;
	GtkWidget *checkbtn_openinbox_startup;
	GtkWidget *checkbtn_change_account_on_folder_sel;
	GtkWidget *checkbtn_immedexec;
	GtkWidget *hbox1;
	GtkWidget *hbox_spc;
	GtkWidget *label;
#ifndef G_OS_WIN32
	GtkWidget *checkbtn_comply_gnome_hig;
#endif
	GtkWidget *checkbtn_show_trayicon;
	GtkWidget *checkbtn_minimize_to_tray;
	GtkWidget *checkbtn_tray_toggle_window;

	GtkWidget *button_keybind;

	GtkWidget *other_wid;
	GtkWidget *extcmd_wid;
#if USE_UPDATE_CHECK
	GtkWidget *update_wid;
#endif
	GtkWidget *advanced_wid;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(vbox1), notebook, TRUE, TRUE, 0);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Interface"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox_tab), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_always_show_msg,
		 _("Always open messages in summary when selected"));

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_always_mark_read,
		 _("Always mark as read when a message is opened"));
	SET_TOGGLE_SENSITIVITY
		(checkbtn_always_show_msg, checkbtn_always_mark_read);

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_openunread,
		 _("Open first unread message when a folder is opened"));
	SET_TOGGLE_SENSITIVITY_REV
		(checkbtn_always_show_msg, checkbtn_openunread);

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_remember_lastsel,
		 _("Remember last selected message"));

#if 0
	PACK_CHECK_BUTTON
		(vbox2, checkbtn_mark_as_read_on_newwin,
		 _("Only mark message as read when opened in new window"));
#endif

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);
	PACK_CHECK_BUTTON
		(hbox1, checkbtn_openinbox,
		 _("Open inbox after receiving new mail"));
	PACK_CHECK_BUTTON
		(hbox1, checkbtn_openinbox_startup, _("Open inbox on startup"));

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_change_account_on_folder_sel,
		 _("Change current account on folder open"));

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox3, checkbtn_immedexec,
		 _("Execute immediately when moving or deleting messages"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	PACK_SMALL_LABEL (hbox1, label,
			  _("Messages will be marked until execution "
			    "if this is turned off."));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox_tab), vbox2, FALSE, FALSE, 0);

#ifndef G_OS_WIN32
	PACK_CHECK_BUTTON (vbox2, checkbtn_comply_gnome_hig,
			   _("Make the order of buttons comply with GNOME HIG"));
#endif
	PACK_CHECK_BUTTON (vbox2, checkbtn_show_trayicon,
			   _("Display tray icon"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_minimize_to_tray,
			   _("Minimize to tray icon"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_tray_toggle_window,
			   _("Toggle window on trayicon click"));
	SET_TOGGLE_SENSITIVITY (checkbtn_show_trayicon,
				checkbtn_minimize_to_tray);
	SET_TOGGLE_SENSITIVITY (checkbtn_show_trayicon,
				checkbtn_tray_toggle_window);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_tab), hbox1, FALSE, FALSE, 0);

	button_keybind = gtk_button_new_with_label (_(" Set key bindings... "));
	gtk_widget_show (button_keybind);
	gtk_box_pack_start (GTK_BOX (hbox1), button_keybind, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_keybind), "clicked",
			  G_CALLBACK (prefs_keybind_select), NULL);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Other"));
	other_wid = prefs_other_create();
	gtk_box_pack_start(GTK_BOX(vbox_tab), other_wid, FALSE, FALSE, 0);

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("External commands"));
	extcmd_wid = prefs_extcmd_create();
	gtk_box_pack_start(GTK_BOX(vbox_tab), extcmd_wid, FALSE, FALSE, 0);

#if USE_UPDATE_CHECK
	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Update"));
	update_wid = prefs_update_create();
	gtk_box_pack_start(GTK_BOX(vbox_tab), update_wid, FALSE, FALSE, 0);
#endif

	APPEND_SUB_NOTEBOOK(notebook, vbox_tab, _("Advanced"));
	advanced_wid = prefs_advanced_create();
	gtk_box_pack_start(GTK_BOX(vbox_tab), advanced_wid, FALSE, FALSE, 0);

	iface.checkbtn_always_show_msg   = checkbtn_always_show_msg;
	iface.checkbtn_always_mark_read  = checkbtn_always_mark_read;
	iface.checkbtn_openunread        = checkbtn_openunread;
	iface.checkbtn_remember_lastsel  = checkbtn_remember_lastsel;
#if 0
	iface.checkbtn_mark_as_read_on_newwin
					     = checkbtn_mark_as_read_on_newwin;
#endif
	iface.checkbtn_openinbox         = checkbtn_openinbox;
	iface.checkbtn_openinbox_startup = checkbtn_openinbox_startup;
	iface.checkbtn_change_account_on_folder_sel =
		checkbtn_change_account_on_folder_sel;
	iface.checkbtn_immedexec         = checkbtn_immedexec;

#ifndef G_OS_WIN32
	iface.checkbtn_comply_gnome_hig  = checkbtn_comply_gnome_hig;
#endif
	iface.checkbtn_show_trayicon      = checkbtn_show_trayicon;
	iface.checkbtn_minimize_to_tray   = checkbtn_minimize_to_tray;
	iface.checkbtn_tray_toggle_window = checkbtn_tray_toggle_window;
}

static GtkWidget *prefs_other_create(void)
{
	GtkWidget *vbox1;

	GtkWidget *frame_recv;
	GtkWidget *vbox_recv;
	GtkWidget *hbox1;
	GtkWidget *label;
	GtkWidget *optmenu_recvdialog;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkWidget *checkbtn_no_recv_err_panel;
	GtkWidget *checkbtn_close_recv_dialog;

	GtkWidget *frame_addr;
	GtkWidget *vbox_addr;
	GtkWidget *checkbtn_addaddrbyclick;
	GtkWidget *checkbtn_add_address_only;
	GtkWidget *vbox_spc;
	GtkWidget *radiobtn_addr_compl;
	GtkWidget *radiobtn_compl_tab;
	GtkWidget *radiobtn_no_compl;

	GtkWidget *frame_exit;
	GtkWidget *vbox_exit;
	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_warnqueued;

	GtkWidget *frame_online;
	GtkWidget *vbox_online;
	GtkWidget *radiobtn_online_mode;
	GtkWidget *radiobtn_start_offline;
	GtkWidget *radiobtn_remember_prev_online;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);

	PACK_FRAME (vbox1, frame_recv, _("Receive dialog"));
	vbox_recv = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_recv);
	gtk_container_add (GTK_CONTAINER (frame_recv), vbox_recv);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_recv), 8);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_recv), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new (_("Show receive dialog"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	optmenu_recvdialog = gtk_option_menu_new ();
	gtk_widget_show (optmenu_recvdialog);
	gtk_box_pack_start (GTK_BOX (hbox1), optmenu_recvdialog,
			    FALSE, FALSE, 0);

	menu = gtk_menu_new ();
	MENUITEM_ADD (menu, menuitem, _("Always"), RECV_DIALOG_ALWAYS);
	MENUITEM_ADD (menu, menuitem, _("Only on manual receiving"),
		      RECV_DIALOG_MANUAL);
	MENUITEM_ADD (menu, menuitem, _("Never"), RECV_DIALOG_NEVER);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_recvdialog), menu);

	PACK_CHECK_BUTTON (vbox_recv, checkbtn_no_recv_err_panel,
			   _("Don't popup error dialog on receive error"));

	PACK_CHECK_BUTTON (vbox_recv, checkbtn_close_recv_dialog,
			   _("Close receive dialog when finished"));

	PACK_FRAME (vbox1, frame_addr, _("Address book"));

	vbox_addr = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_addr);
	gtk_container_add (GTK_CONTAINER (frame_addr), vbox_addr);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_addr), 8);

	PACK_CHECK_BUTTON
		(vbox_addr, checkbtn_addaddrbyclick,
		 _("Add address to destination when double-clicked"));

	PACK_CHECK_BUTTON
		(vbox_addr, checkbtn_add_address_only,
		 _("Set only mail address when entering recipient from address book"));

	PACK_VSPACER (vbox_addr, vbox_spc, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_addr), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new (_("Auto-completion:"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	radiobtn_addr_compl = gtk_radio_button_new_with_label
		(NULL, _("Automatic"));
	gtk_widget_show (radiobtn_addr_compl);
	gtk_box_pack_start (GTK_BOX (hbox1), radiobtn_addr_compl,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (radiobtn_addr_compl), MENU_VAL_ID,
			   GINT_TO_POINTER (0));

	radiobtn_compl_tab = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (radiobtn_addr_compl), _("Start with Tab"));
	gtk_widget_show (radiobtn_compl_tab);
	gtk_box_pack_start (GTK_BOX (hbox1), radiobtn_compl_tab,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (radiobtn_compl_tab), MENU_VAL_ID,
			   GINT_TO_POINTER (1));

	radiobtn_no_compl = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (radiobtn_addr_compl), _("Disable"));
	gtk_widget_show (radiobtn_no_compl);
	gtk_box_pack_start (GTK_BOX (hbox1), radiobtn_no_compl,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (radiobtn_no_compl), MENU_VAL_ID,
			   GINT_TO_POINTER (2));

	PACK_FRAME (vbox1, frame_exit, _("On exit"));

	vbox_exit = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_exit);
	gtk_container_add (GTK_CONTAINER (frame_exit), vbox_exit);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_exit), 8);

	PACK_CHECK_BUTTON (vbox_exit, checkbtn_confonexit,
			   _("Confirm on exit"));

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_exit), hbox1, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox1, checkbtn_cleanonexit,
			   _("Empty trash on exit"));
	PACK_CHECK_BUTTON (hbox1, checkbtn_askonclean,
			   _("Ask before emptying"));
	SET_TOGGLE_SENSITIVITY (checkbtn_cleanonexit, checkbtn_askonclean);

	PACK_CHECK_BUTTON (vbox_exit, checkbtn_warnqueued,
			   _("Warn if there are queued messages"));

	PACK_FRAME (vbox1, frame_online, _("Online mode"));

	vbox_online = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_online);
	gtk_container_add (GTK_CONTAINER (frame_online), vbox_online);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_online), 8);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_online), hbox1, FALSE, FALSE, 0);

	radiobtn_online_mode = gtk_radio_button_new_with_label
		(NULL, _("Start as online"));
	gtk_widget_show (radiobtn_online_mode);
	gtk_box_pack_start (GTK_BOX (hbox1), radiobtn_online_mode,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (radiobtn_online_mode), MENU_VAL_ID,
			   GINT_TO_POINTER (1));

	radiobtn_start_offline = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (radiobtn_online_mode), _("Start as offline"));
	gtk_widget_show (radiobtn_start_offline);
	gtk_box_pack_start (GTK_BOX (hbox1), radiobtn_start_offline,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (radiobtn_start_offline), MENU_VAL_ID,
			   GINT_TO_POINTER (0));

	radiobtn_remember_prev_online = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (radiobtn_online_mode), _("Remember previous mode"));
	gtk_widget_show (radiobtn_remember_prev_online);
	gtk_box_pack_start (GTK_BOX (hbox1), radiobtn_remember_prev_online,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (radiobtn_remember_prev_online), MENU_VAL_ID,
			   GINT_TO_POINTER (2));

	other.optmenu_recvdialog         = optmenu_recvdialog;
	other.checkbtn_no_recv_err_panel = checkbtn_no_recv_err_panel;
	other.checkbtn_close_recv_dialog = checkbtn_close_recv_dialog;

	other.checkbtn_addaddrbyclick    = checkbtn_addaddrbyclick;
	other.checkbtn_add_address_only  = checkbtn_add_address_only;
	other.radiobtn_addr_compl        = radiobtn_addr_compl;

	other.checkbtn_confonexit  = checkbtn_confonexit;
	other.checkbtn_cleanonexit = checkbtn_cleanonexit;
	other.checkbtn_askonclean  = checkbtn_askonclean;
	other.checkbtn_warnqueued  = checkbtn_warnqueued;

	other.radiobtn_online_mode = radiobtn_online_mode;

	return vbox1;
}

static GtkWidget *prefs_extcmd_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *hbox1;

	GtkWidget *ext_frame;
	GtkWidget *ext_table;

	GtkWidget *uri_label;
	GtkWidget *uri_combo;
	GtkWidget *uri_entry;

	GtkWidget *exteditor_label;
	GtkWidget *exteditor_combo;
	GtkWidget *exteditor_entry;

	GtkWidget *frame_printcmd;
	GtkWidget *vbox_printcmd;
	GtkWidget *checkbtn_printcmd;
	GtkWidget *printcmd_label;
	GtkWidget *printcmd_entry;

#ifndef G_OS_WIN32
	GtkWidget *vbox2;
	GtkWidget *frame_incext;
	GtkWidget *checkbtn_incext;
	GtkWidget *label_incext;
	GtkWidget *entry_incext;
#endif
	GtkWidget *frame_extsend;
	GtkWidget *vbox_extsend;
	GtkWidget *checkbtn_extsend;
	GtkWidget *label_extsend;
	GtkWidget *entry_extsend;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);

	PACK_FRAME(vbox1, ext_frame,
		   _("External commands (%s will be replaced with file name / URI)"));

	ext_table = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (ext_table);
	gtk_container_add (GTK_CONTAINER (ext_frame), ext_table);
	gtk_container_set_border_width (GTK_CONTAINER (ext_table), 8);
	gtk_table_set_row_spacings (GTK_TABLE (ext_table), VSPACING_NARROW);
	gtk_table_set_col_spacings (GTK_TABLE (ext_table), 8);

	uri_label = gtk_label_new (_("Web browser"));
	gtk_widget_show(uri_label);
	gtk_table_attach (GTK_TABLE (ext_table), uri_label, 0, 1, 0, 1,
			  GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (uri_label), 1, 0.5);

	uri_combo = gtk_combo_new ();
	gtk_widget_show (uri_combo);
	gtk_table_attach (GTK_TABLE (ext_table), uri_combo, 1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtkut_combo_set_items (GTK_COMBO (uri_combo),
#if defined(G_OS_WIN32) || defined(__APPLE__)
			       _("(Default browser)"),
#else
			       DEFAULT_BROWSER_CMD,
			       "sensible-browser '%s'",
			       "firefox -remote 'openURL(%s,new-window)'",
			       "firefox '%s'",
			       "mozilla -remote 'openURL(%s,new-window)'",
			       "mozilla '%s'",
			       "rxvt -e w3m '%s'",
			       "rxvt -e lynx '%s'",
#endif
			       NULL);
	uri_entry = GTK_COMBO (uri_combo)->entry;

	exteditor_label = gtk_label_new (_("Editor"));
	gtk_widget_show (exteditor_label);
	gtk_table_attach (GTK_TABLE (ext_table), exteditor_label, 0, 1, 1, 2,
			  GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (exteditor_label), 1, 0.5);

	exteditor_combo = gtk_combo_new ();
	gtk_widget_show (exteditor_combo);
	gtk_table_attach (GTK_TABLE (ext_table), exteditor_combo, 1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtkut_combo_set_items (GTK_COMBO (exteditor_combo),
#ifdef G_OS_WIN32
			       "notepad '%s'",
#elif defined(__APPLE__)
			       "open -t '%s'",
#else
			       "gedit %s",
			       "kedit %s",
			       "emacs %s",
			       "xemacs %s",
			       "rxvt -e jed %s",
			       "rxvt -e vi %s",
#endif
			       NULL);
	exteditor_entry = GTK_COMBO (exteditor_combo)->entry;

	PACK_FRAME_WITH_CHECK_BUTTON(vbox1, frame_printcmd, checkbtn_printcmd,
				     _("Use external program for printing"));

	vbox_printcmd = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox_printcmd);
	gtk_container_add (GTK_CONTAINER (frame_printcmd), vbox_printcmd);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_printcmd), 8);
	SET_TOGGLE_SENSITIVITY (checkbtn_printcmd, vbox_printcmd);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_printcmd), hbox1, FALSE, FALSE, 0);

	printcmd_label = gtk_label_new (_("Command"));
	gtk_widget_show (printcmd_label);
	gtk_box_pack_start (GTK_BOX (hbox1), printcmd_label, FALSE, FALSE, 0);

	printcmd_entry = gtk_entry_new ();
	gtk_widget_show (printcmd_entry);
	gtk_box_pack_start (GTK_BOX (hbox1), printcmd_entry, TRUE, TRUE, 0);

#ifndef G_OS_WIN32
	PACK_FRAME_WITH_CHECK_BUTTON(vbox1, frame_incext, checkbtn_incext,
				     _("Use external program for incorporation"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_incext), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
	SET_TOGGLE_SENSITIVITY (checkbtn_incext, vbox2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	label_incext = gtk_label_new (_("Command"));
	gtk_widget_show (label_incext);
	gtk_box_pack_start (GTK_BOX (hbox1), label_incext, FALSE, FALSE, 0);

	entry_incext = gtk_entry_new ();
	gtk_widget_show (entry_incext);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_incext, TRUE, TRUE, 0);
#endif /* !G_OS_WIN32 */

	PACK_FRAME_WITH_CHECK_BUTTON (vbox1, frame_extsend, checkbtn_extsend,
				      _("Use external program for sending"));

	vbox_extsend = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox_extsend);
	gtk_container_add (GTK_CONTAINER (frame_extsend), vbox_extsend);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_extsend), 8);
	SET_TOGGLE_SENSITIVITY(checkbtn_extsend, vbox_extsend);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_extsend), hbox1, FALSE, FALSE, 0);

	label_extsend = gtk_label_new (_("Command"));
	gtk_widget_show (label_extsend);
	gtk_box_pack_start (GTK_BOX (hbox1), label_extsend, FALSE, FALSE, 0);

	entry_extsend = gtk_entry_new ();
	gtk_widget_show (entry_extsend);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_extsend, TRUE, TRUE, 0);

	extcmd.uri_combo = uri_combo;
	extcmd.uri_entry = uri_entry;
	extcmd.checkbtn_printcmd = checkbtn_printcmd;
	extcmd.printcmd_entry = printcmd_entry;

	extcmd.exteditor_combo = exteditor_combo;
	extcmd.exteditor_entry = exteditor_entry;

#ifndef G_OS_WIN32
	extcmd.checkbtn_incext = checkbtn_incext;
	extcmd.entry_incext    = entry_incext;
#endif
	extcmd.checkbtn_extsend = checkbtn_extsend;
	extcmd.entry_extsend    = entry_extsend;

	return vbox1;
}

#if USE_UPDATE_CHECK
static GtkWidget *prefs_update_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *checkbtn_autoupdate;
	GtkWidget *checkbtn_useproxy;
	GtkWidget *label;
	GtkWidget *entry_proxyhost;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);

#ifndef G_OS_WIN32
	label = gtk_label_new (_("Update check requires 'curl' command."));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox1), label, FALSE, FALSE, 0);
#endif

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_autoupdate,
			   _("Enable auto update check"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_useproxy,
			   _("Use HTTP proxy"));

	label = gtk_label_new (_("HTTP proxy host (hostname:port):"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);

	entry_proxyhost = gtk_entry_new ();
	gtk_widget_show (entry_proxyhost);
	gtk_box_pack_start (GTK_BOX (vbox2), entry_proxyhost, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_useproxy, label);
	SET_TOGGLE_SENSITIVITY(checkbtn_useproxy, entry_proxyhost);

	update_check.checkbtn_autoupdate = checkbtn_autoupdate;
	update_check.checkbtn_useproxy = checkbtn_useproxy;
	update_check.entry_proxyhost = entry_proxyhost;

	return vbox1;
}
#endif /* USE_UPDATE_CHECK */

static GtkWidget *prefs_advanced_create(void)
{
	GtkWidget *vbox1;

	GtkWidget *vbox2;
	GtkWidget *checkbtn_strict_cache_check;
	GtkWidget *label;

	GtkWidget *hbox1;
	GtkWidget *label_iotimeout;
	GtkWidget *spinbtn_iotimeout;
	GtkObject *spinbtn_iotimeout_adj;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_strict_cache_check,
			   _("Enable strict checking of the integrity of summary caches"));
	PACK_SMALL_LABEL
		(vbox2, label,
		 _("Enable this if the contents of folders have the possibility of modification by other applications.\n"
		   "This option will degrade the performance of displaying summary."));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_iotimeout = gtk_label_new (_("Socket I/O timeout:"));
	gtk_widget_show (label_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), label_iotimeout, FALSE, FALSE, 0);

	spinbtn_iotimeout_adj = gtk_adjustment_new (60, 0, 1000, 1, 10, 0);
	spinbtn_iotimeout = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_iotimeout_adj), 1, 0);
	gtk_widget_show (spinbtn_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_iotimeout,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_iotimeout, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_iotimeout), TRUE);

	label_iotimeout = gtk_label_new (_("second(s)"));
	gtk_widget_show (label_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), label_iotimeout, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	advanced.checkbtn_strict_cache_check = checkbtn_strict_cache_check;

	advanced.spinbtn_iotimeout     = spinbtn_iotimeout;
	advanced.spinbtn_iotimeout_adj = spinbtn_iotimeout_adj;

	return vbox1;
}

static void prefs_common_set_encoding_optmenu(GtkOptionMenu *optmenu,
					      gboolean outgoing)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_menu_new();

#define SET_MENUITEM(str, data) \
	MENUITEM_ADD(menu, menuitem, str, data)

	if (outgoing) {
		SET_MENUITEM(_("Automatic (Recommended)"), NULL);
	} else {
		SET_MENUITEM(_("Automatic"), NULL);
	}
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("7bit ascii (US-ASCII)"),	 CS_US_ASCII);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Unicode (UTF-8)"),		 CS_UTF_8);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Western European (ISO-8859-1)"),  CS_ISO_8859_1);
	SET_MENUITEM(_("Western European (ISO-8859-15)"), CS_ISO_8859_15);
	if (!outgoing) {
		SET_MENUITEM(_("Western European (Windows-1252)"),
			     CS_WINDOWS_1252);
	}
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Central European (ISO-8859-2)"), CS_ISO_8859_2);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Baltic (ISO-8859-13)"),		 CS_ISO_8859_13);
	SET_MENUITEM(_("Baltic (ISO-8859-4)"),		 CS_ISO_8859_4);
	SET_MENUITEM(_("Baltic (Windows-1257)"),	 CS_WINDOWS_1257);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Greek (ISO-8859-7)"),		 CS_ISO_8859_7);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Arabic (ISO-8859-6)"),		 CS_ISO_8859_6);
	SET_MENUITEM(_("Arabic (Windows-1256)"),	 CS_WINDOWS_1256);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Hebrew (ISO-8859-8)"),		 CS_ISO_8859_8);
	SET_MENUITEM(_("Hebrew (Windows-1255)"),	 CS_WINDOWS_1255);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Turkish (ISO-8859-9)"),		 CS_ISO_8859_9);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Cyrillic (ISO-8859-5)"),	 CS_ISO_8859_5);
	SET_MENUITEM(_("Cyrillic (KOI8-R)"),		 CS_KOI8_R);
	SET_MENUITEM(_("Cyrillic (KOI8-U)"),		 CS_KOI8_U);
	SET_MENUITEM(_("Cyrillic (Windows-1251)"),	 CS_WINDOWS_1251);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Japanese (ISO-2022-JP)"),	 CS_ISO_2022_JP);
	if (!outgoing) {
		SET_MENUITEM(_("Japanese (EUC-JP)"),	 CS_EUC_JP);
		SET_MENUITEM(_("Japanese (Shift_JIS)"),	 CS_SHIFT_JIS);
	}
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Simplified Chinese (GB2312)"),	 CS_GB2312);
	SET_MENUITEM(_("Simplified Chinese (GBK)"),	 CS_GBK);
	SET_MENUITEM(_("Traditional Chinese (Big5)"),	 CS_BIG5);
	if (!outgoing) {
		SET_MENUITEM(_("Traditional Chinese (EUC-TW)"), CS_EUC_TW);
		SET_MENUITEM(_("Chinese (ISO-2022-CN)"),	CS_ISO_2022_CN);
	}
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Korean (EUC-KR)"),		 CS_EUC_KR);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Thai (TIS-620)"),		 CS_TIS_620);
	SET_MENUITEM(_("Thai (Windows-874)"),		 CS_WINDOWS_874);

#undef SET_MENUITEM

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
}

static void date_format_ok_btn_clicked(GtkButton *button, GtkWidget **widget)
{
	GtkWidget *datefmt_sample = NULL;
	gchar *text;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(*widget != NULL);
	g_return_if_fail(display.entry_datefmt != NULL);

	datefmt_sample = GTK_WIDGET(g_object_get_data
				    (G_OBJECT(*widget), "datefmt_sample"));
	g_return_if_fail(datefmt_sample != NULL);

	text = gtk_editable_get_chars(GTK_EDITABLE(datefmt_sample), 0, -1);
	g_free(prefs_common.date_format);
	prefs_common.date_format = text;
	gtk_entry_set_text(GTK_ENTRY(display.entry_datefmt), text);

	gtk_widget_destroy(*widget);
	*widget = NULL;
}

static void date_format_cancel_btn_clicked(GtkButton *button,
					   GtkWidget **widget)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(*widget != NULL);

	gtk_widget_destroy(*widget);
	*widget = NULL;
}

static gboolean date_format_key_pressed(GtkWidget *keywidget,
					GdkEventKey *event,
					GtkWidget **widget)
{
	if (event && event->keyval == GDK_Escape)
		date_format_cancel_btn_clicked(NULL, widget);
	return FALSE;
}

static gboolean date_format_on_delete(GtkWidget *dialogwidget,
				      GdkEventAny *event, GtkWidget **widget)
{
	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(*widget != NULL, FALSE);

	*widget = NULL;
	return FALSE;
}

static void date_format_entry_on_change(GtkEditable *editable,
					GtkLabel *example)
{
	time_t cur_time;
	struct tm *cal_time;
	gchar buffer[100];
	gchar *text;

	cur_time = time(NULL);
	cal_time = localtime(&cur_time);
	buffer[0] = 0;
	text = gtk_editable_get_chars(editable, 0, -1);
	if (text)
		strftime(buffer, sizeof buffer, text, cal_time); 
	g_free(text);

	text = conv_codeset_strdup(buffer, conv_get_locale_charset_str(),
				   CS_UTF_8);
	if (!text)
		text = g_strdup(buffer);
	gtk_label_set_text(example, text);
	g_free(text);
}

static void date_format_select_row(GtkWidget *date_format_list, gint row,
				   gint column, GdkEventButton *event,
				   GtkWidget *date_format)
{
	gint cur_pos;
	gchar *format;
	const gchar *old_format;
	gchar *new_format;
	GtkWidget *datefmt_sample;

	/* only on double click */
	if (!event || event->type != GDK_2BUTTON_PRESS) return;

	datefmt_sample = GTK_WIDGET(g_object_get_data
				    (G_OBJECT(date_format), "datefmt_sample"));

	g_return_if_fail(date_format_list != NULL);
	g_return_if_fail(date_format != NULL);
	g_return_if_fail(datefmt_sample != NULL);

	/* get format from clist */
	gtk_clist_get_text(GTK_CLIST(date_format_list), row, 0, &format);

	cur_pos = gtk_editable_get_position(GTK_EDITABLE(datefmt_sample));
	old_format = gtk_entry_get_text(GTK_ENTRY(datefmt_sample));

	/* insert the format into the text entry */
	new_format = g_malloc(strlen(old_format) + 3);

	strncpy(new_format, old_format, cur_pos);
	new_format[cur_pos] = '\0';
	strcat(new_format, format);
	strcat(new_format, &old_format[cur_pos]);

	gtk_entry_set_text(GTK_ENTRY(datefmt_sample), new_format);
	gtk_editable_set_position(GTK_EDITABLE(datefmt_sample), cur_pos + 2);

	g_free(new_format);
}

static GtkWidget *date_format_create(GtkButton *button, void *data)
{
	static GtkWidget *datefmt_win = NULL;
	GtkWidget *vbox1;
	GtkWidget *scrolledwindow1;
	GtkWidget *datefmt_clist;
	GtkWidget *table;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *label3;
	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *datefmt_entry;

	struct {
		gchar *fmt;
		gchar *txt;
	} time_format[] = {
		{ "%a", NULL },
		{ "%A", NULL },
		{ "%b", NULL },
		{ "%B", NULL },
		{ "%c", NULL },
		{ "%C", NULL },
		{ "%d", NULL },
		{ "%H", NULL },
		{ "%I", NULL },
		{ "%j", NULL },
		{ "%m", NULL },
		{ "%M", NULL },
		{ "%p", NULL },
		{ "%S", NULL },
		{ "%w", NULL },
		{ "%x", NULL },
		{ "%y", NULL },
		{ "%Y", NULL },
		{ "%Z", NULL }
	};

	gchar *titles[2];
	gint i;
	const gint TIME_FORMAT_ELEMS =
		sizeof time_format / sizeof time_format[0];

	time_format[0].txt  = _("the full abbreviated weekday name");
	time_format[1].txt  = _("the full weekday name");
	time_format[2].txt  = _("the abbreviated month name");
	time_format[3].txt  = _("the full month name");
	time_format[4].txt  = _("the preferred date and time for the current locale");
	time_format[5].txt  = _("the century number (year/100)");
	time_format[6].txt  = _("the day of the month as a decimal number");
	time_format[7].txt  = _("the hour as a decimal number using a 24-hour clock");
	time_format[8].txt  = _("the hour as a decimal number using a 12-hour clock");
	time_format[9].txt  = _("the day of the year as a decimal number");
	time_format[10].txt = _("the month as a decimal number");
	time_format[11].txt = _("the minute as a decimal number");
	time_format[12].txt = _("either AM or PM");
	time_format[13].txt = _("the second as a decimal number");
	time_format[14].txt = _("the day of the week as a decimal number");
	time_format[15].txt = _("the preferred date for the current locale");
	time_format[16].txt = _("the last two digits of a year");
	time_format[17].txt = _("the year as a decimal number");
	time_format[18].txt = _("the time zone or name or abbreviation");

	if (datefmt_win) return datefmt_win;

	datefmt_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(datefmt_win), 8);
	gtk_window_set_title(GTK_WINDOW(datefmt_win), _("Date format"));
	gtk_window_set_position(GTK_WINDOW(datefmt_win), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request(datefmt_win, 440, 280);

	vbox1 = gtk_vbox_new(FALSE, 10);
	gtk_widget_show(vbox1);
	gtk_container_add(GTK_CONTAINER(datefmt_win), vbox1);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW(scrolledwindow1),
		 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show(scrolledwindow1);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwindow1, TRUE, TRUE, 0);

	titles[0] = _("Specifier");
	titles[1] = _("Description");
	datefmt_clist = gtk_clist_new_with_titles(2, titles);
	gtk_widget_show(datefmt_clist);
	gtk_container_add(GTK_CONTAINER(scrolledwindow1), datefmt_clist);
	/* gtk_clist_set_column_width(GTK_CLIST(datefmt_clist), 0, 80); */
	gtk_clist_set_selection_mode(GTK_CLIST(datefmt_clist),
				     GTK_SELECTION_BROWSE);

	for (i = 0; i < TIME_FORMAT_ELEMS; i++) {
		gchar *text[2];
		/* phoney casting necessary because of gtk... */
		text[0] = (gchar *)time_format[i].fmt;
		text[1] = (gchar *)time_format[i].txt;
		gtk_clist_append(GTK_CLIST(datefmt_clist), text);
	}

	table = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(table);
	gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label1 = gtk_label_new(_("Date format"));
	gtk_widget_show(label1);
	gtk_table_attach(GTK_TABLE(table), label1, 0, 1, 0, 1,
			 GTK_FILL, 0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(label1), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label1), 0, 0.5);

	datefmt_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(datefmt_entry), 256);
	gtk_widget_show(datefmt_entry);
	gtk_table_attach(GTK_TABLE(table), datefmt_entry, 1, 2, 0, 1,
			 (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	/* we need the "sample" entry box; add it as data so callbacks can
	 * get the entry box */
	g_object_set_data(G_OBJECT(datefmt_win), "datefmt_sample",
			  datefmt_entry);

	label2 = gtk_label_new(_("Example"));
	gtk_widget_show(label2);
	gtk_table_attach(GTK_TABLE(table), label2, 0, 1, 1, 2,
			 GTK_FILL, 0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(label2), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label2), 0, 0.5);

	label3 = gtk_label_new("");
	gtk_widget_show(label3);
	gtk_table_attach(GTK_TABLE(table), label3, 1, 2, 1, 2,
			 (GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(label3), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0.5);

	gtkut_stock_button_set_create(&confirm_area, &ok_btn, GTK_STOCK_OK,
				&cancel_btn, GTK_STOCK_CANCEL, NULL, NULL);

	gtk_box_pack_start(GTK_BOX(vbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_show(confirm_area);
	gtk_widget_grab_default(ok_btn);

	/* set the current format */
	gtk_entry_set_text(GTK_ENTRY(datefmt_entry), prefs_common.date_format);
	date_format_entry_on_change(GTK_EDITABLE(datefmt_entry),
				    GTK_LABEL(label3));

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(date_format_ok_btn_clicked), &datefmt_win);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(date_format_cancel_btn_clicked),
			 &datefmt_win);
	g_signal_connect(G_OBJECT(datefmt_win), "key_press_event",
			 G_CALLBACK(date_format_key_pressed), &datefmt_win);
	g_signal_connect(G_OBJECT(datefmt_win), "delete_event",
			 G_CALLBACK(date_format_on_delete), &datefmt_win);
	g_signal_connect(G_OBJECT(datefmt_entry), "changed",
			 G_CALLBACK(date_format_entry_on_change), label3);
	g_signal_connect(G_OBJECT(datefmt_clist), "select_row",
			 G_CALLBACK(date_format_select_row), datefmt_win);

	gtk_window_set_position(GTK_WINDOW(datefmt_win), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(datefmt_win), TRUE);

	gtk_widget_show(datefmt_win);
	manage_window_set_transient(GTK_WINDOW(datefmt_win));

	gtk_widget_grab_focus(ok_btn);

	return datefmt_win;
}

void prefs_quote_colors_dialog(void)
{
	if (!quote_color_win)
		prefs_quote_colors_dialog_create();
	gtk_widget_show(quote_color_win);
	manage_window_set_transient(GTK_WINDOW(quote_color_win));

	gtk_main();
	gtk_widget_hide(quote_color_win);
	gtk_window_present(GTK_WINDOW(dialog.window));

	main_window_reflect_prefs_all();
}

static void prefs_quote_colors_dialog_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *quotelevel1_label;
	GtkWidget *quotelevel2_label;
	GtkWidget *quotelevel3_label;
	GtkWidget *uri_label;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *recycle_colors_btn;
	GtkWidget *frame_colors;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 2);
	gtk_window_set_title(GTK_WINDOW(window), _("Set message colors"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);

	vbox = gtk_vbox_new (FALSE, VSPACING);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
	PACK_FRAME(vbox, frame_colors, _("Colors"));

	table = gtk_table_new (4, 2, FALSE);
	gtk_container_add (GTK_CONTAINER (frame_colors), table);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_table_set_row_spacings (GTK_TABLE (table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 4);

	color_buttons.quote_level1_btn = gtk_button_new();
	gtk_table_attach (GTK_TABLE (table), color_buttons.quote_level1_btn,
			  0, 1, 0, 1, 0, 0, 0, 0);
	gtk_widget_set_size_request (color_buttons.quote_level1_btn, 40, 30);
	gtk_container_set_border_width
		(GTK_CONTAINER (color_buttons.quote_level1_btn), 5);

	color_buttons.quote_level2_btn = gtk_button_new();
	gtk_table_attach (GTK_TABLE (table), color_buttons.quote_level2_btn,
			  0, 1, 1, 2, 0, 0, 0, 0);
	gtk_widget_set_size_request (color_buttons.quote_level2_btn, 40, 30);
	gtk_container_set_border_width (GTK_CONTAINER (color_buttons.quote_level2_btn), 5);

	color_buttons.quote_level3_btn = gtk_button_new_with_label ("");
	gtk_table_attach (GTK_TABLE (table), color_buttons.quote_level3_btn,
			  0, 1, 2, 3, 0, 0, 0, 0);
	gtk_widget_set_size_request (color_buttons.quote_level3_btn, 40, 30);
	gtk_container_set_border_width
		(GTK_CONTAINER (color_buttons.quote_level3_btn), 5);

	color_buttons.uri_btn = gtk_button_new_with_label ("");
	gtk_table_attach (GTK_TABLE (table), color_buttons.uri_btn,
			  0, 1, 3, 4, 0, 0, 0, 0);
	gtk_widget_set_size_request (color_buttons.uri_btn, 40, 30);
	gtk_container_set_border_width (GTK_CONTAINER (color_buttons.uri_btn), 5);

	quotelevel1_label = gtk_label_new (_("Quoted Text - First Level"));
	gtk_table_attach (GTK_TABLE (table), quotelevel1_label, 1, 2, 0, 1,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (quotelevel1_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (quotelevel1_label), 0, 0.5);

	quotelevel2_label = gtk_label_new (_("Quoted Text - Second Level"));
	gtk_table_attach (GTK_TABLE (table), quotelevel2_label, 1, 2, 1, 2,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (quotelevel2_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (quotelevel2_label), 0, 0.5);

	quotelevel3_label = gtk_label_new (_("Quoted Text - Third Level"));
	gtk_table_attach (GTK_TABLE (table), quotelevel3_label, 1, 2, 2, 3,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (quotelevel3_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (quotelevel3_label), 0, 0.5);

	uri_label = gtk_label_new (_("URI link"));
	gtk_table_attach (GTK_TABLE (table), uri_label, 1, 2, 3, 4,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (uri_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (uri_label), 0, 0.5);

	PACK_CHECK_BUTTON (vbox, recycle_colors_btn,
			   _("Recycle quote colors"));

	gtkut_stock_button_set_create(&hbbox, &ok_btn, GTK_STOCK_OK,
				      NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

	gtk_widget_grab_default(ok_btn);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	g_signal_connect (G_OBJECT (window), "delete_event",
			  G_CALLBACK (prefs_quote_colors_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(prefs_quote_colors_key_pressed), NULL);

	g_signal_connect(G_OBJECT(color_buttons.quote_level1_btn), "clicked",
			 G_CALLBACK(quote_color_set_dialog), "LEVEL1");
	g_signal_connect(G_OBJECT(color_buttons.quote_level2_btn), "clicked",
			 G_CALLBACK(quote_color_set_dialog), "LEVEL2");
	g_signal_connect(G_OBJECT(color_buttons.quote_level3_btn), "clicked",
			 G_CALLBACK(quote_color_set_dialog), "LEVEL3");
	g_signal_connect(G_OBJECT(color_buttons.uri_btn), "clicked",
			 G_CALLBACK(quote_color_set_dialog), "URI");
	g_signal_connect(G_OBJECT(recycle_colors_btn), "toggled",
			 G_CALLBACK(prefs_recycle_colors_toggled), NULL);
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(gtk_main_quit), NULL);

	/* show message button colors and recycle options */
	set_button_bg_color(color_buttons.quote_level1_btn,
			    prefs_common.quote_level1_col);
	set_button_bg_color(color_buttons.quote_level2_btn,
			    prefs_common.quote_level2_col);
	set_button_bg_color(color_buttons.quote_level3_btn,
			    prefs_common.quote_level3_col);
	set_button_bg_color(color_buttons.uri_btn,
			    prefs_common.uri_col);
	gtk_toggle_button_set_active((GtkToggleButton *)recycle_colors_btn,
				     prefs_common.recycle_quote_colors);

	gtk_widget_show_all(vbox);
	quote_color_win = window;
}

static gint prefs_quote_colors_deleted(GtkWidget *widet, GdkEventAny *event,
				       gpointer data)
{
	gtk_main_quit();
	return TRUE;
}

static gboolean prefs_quote_colors_key_pressed(GtkWidget *widget,
					       GdkEventKey *event,
					       gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_main_quit();
	return FALSE;
}

static void quote_color_set_dialog(GtkWidget *widget, gpointer data)
{
	gchar *type = (gchar *)data;
	gchar *title = NULL;
	gdouble color[4] = {0.0, 0.0, 0.0, 0.0};
	gint rgbvalue = 0;
	GtkColorSelectionDialog *dialog;

	if(g_ascii_strcasecmp(type, "LEVEL1") == 0) {
		title = _("Pick color for quotation level 1");
		rgbvalue = prefs_common.quote_level1_col;
	} else if(g_ascii_strcasecmp(type, "LEVEL2") == 0) {
		title = _("Pick color for quotation level 2");
		rgbvalue = prefs_common.quote_level2_col;
	} else if(g_ascii_strcasecmp(type, "LEVEL3") == 0) {
		title = _("Pick color for quotation level 3");
		rgbvalue = prefs_common.quote_level3_col;
	} else if(g_ascii_strcasecmp(type, "URI") == 0) {
		title = _("Pick color for URI");
		rgbvalue = prefs_common.uri_col;
	} else {   /* Should never be called */
		g_warning("Unrecognized datatype '%s' in quote_color_set_dialog\n", type);
		return;
	}

	color_dialog = gtk_color_selection_dialog_new(title);
	gtk_window_set_position(GTK_WINDOW(color_dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(color_dialog), TRUE);
	gtk_window_set_policy(GTK_WINDOW(color_dialog), FALSE, FALSE, FALSE);
	manage_window_set_transient(GTK_WINDOW(color_dialog));

	g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(color_dialog)->ok_button),
			 "clicked", G_CALLBACK(quote_colors_set_dialog_ok), data);
	g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(color_dialog)->cancel_button),
			 "clicked", G_CALLBACK(quote_colors_set_dialog_cancel), data);
	g_signal_connect(G_OBJECT(color_dialog), "key_press_event",
			 G_CALLBACK(quote_colors_set_dialog_key_pressed), data);

	/* preselect the previous color in the color selection dialog */
	color[0] = (gdouble) ((rgbvalue & 0xff0000) >> 16) / 255.0;
	color[1] = (gdouble) ((rgbvalue & 0x00ff00) >>  8) / 255.0;
	color[2] = (gdouble)  (rgbvalue & 0x0000ff)        / 255.0;
	dialog = GTK_COLOR_SELECTION_DIALOG(color_dialog);
	gtk_color_selection_set_color
		(GTK_COLOR_SELECTION(dialog->colorsel), color);

	gtk_widget_show(color_dialog);
}

static void quote_colors_set_dialog_ok(GtkWidget *widget, gpointer data)
{
	GtkColorSelection *colorsel = (GtkColorSelection *)
						((GtkColorSelectionDialog *)color_dialog)->colorsel;
	gdouble color[4];
	gint red, green, blue, rgbvalue;
	gchar *type = (gchar *)data;

	gtk_color_selection_get_color(colorsel, color);

	red      = (gint) (color[0] * 255.0);
	green    = (gint) (color[1] * 255.0);
	blue     = (gint) (color[2] * 255.0);
	rgbvalue = (gint) ((red * 0x10000) | (green * 0x100) | blue);

#if 0
	fprintf(stderr, "redc = %f, greenc = %f, bluec = %f\n", color[0], color[1], color[2]);
	fprintf(stderr, "red = %d, green = %d, blue = %d\n", red, green, blue);
	fprintf(stderr, "Color is %x\n", rgbvalue);
#endif

	if (g_ascii_strcasecmp(type, "LEVEL1") == 0) {
		prefs_common.quote_level1_col = rgbvalue;
		set_button_bg_color(color_buttons.quote_level1_btn, rgbvalue);
	} else if (g_ascii_strcasecmp(type, "LEVEL2") == 0) {
		prefs_common.quote_level2_col = rgbvalue;
		set_button_bg_color(color_buttons.quote_level2_btn, rgbvalue);
	} else if (g_ascii_strcasecmp(type, "LEVEL3") == 0) {
		prefs_common.quote_level3_col = rgbvalue;
		set_button_bg_color(color_buttons.quote_level3_btn, rgbvalue);
	} else if (g_ascii_strcasecmp(type, "URI") == 0) {
		prefs_common.uri_col = rgbvalue;
		set_button_bg_color(color_buttons.uri_btn, rgbvalue);
	} else
		fprintf( stderr, "Unrecognized datatype '%s' in quote_color_set_dialog_ok\n", type );

	gtk_widget_destroy(color_dialog);
}

static void quote_colors_set_dialog_cancel(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(color_dialog);
}

static gboolean quote_colors_set_dialog_key_pressed(GtkWidget *widget,
						    GdkEventKey *event,
						    gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		gtk_widget_destroy(color_dialog);
		return TRUE;
	}
	return FALSE;
}

static void set_button_bg_color(GtkWidget *widget, gint rgbvalue)
{
	GtkStyle *newstyle;
	GdkColor color;

	gtkut_convert_int_to_gdk_color(rgbvalue, &color);
	newstyle = gtk_style_copy(gtk_widget_get_default_style());
	newstyle->bg[GTK_STATE_NORMAL]   = color;
	newstyle->bg[GTK_STATE_PRELIGHT] = color;
	newstyle->bg[GTK_STATE_ACTIVE]   = color;

	gtk_widget_set_style(GTK_WIDGET(widget), newstyle);
}

static void prefs_enable_message_color_toggled(void)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(message.chkbtn_enablecol));
	gtk_widget_set_sensitive(message.button_edit_col, is_active);
	prefs_common.enable_color = is_active;
}

static void prefs_recycle_colors_toggled(GtkWidget *widget)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	prefs_common.recycle_quote_colors = is_active;
}

void prefs_quote_description(void)
{
	if (!quote_desc_win)
		prefs_quote_description_create();

	manage_window_set_transient(GTK_WINDOW(quote_desc_win));
	gtk_widget_show(quote_desc_win);
	gtk_main();
	gtk_widget_hide(quote_desc_win);
}

static void prefs_quote_description_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *vbox2;
	GtkWidget *label;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;

	quote_desc_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(quote_desc_win),
			     _("Description of symbols"));
	gtk_container_set_border_width(GTK_CONTAINER(quote_desc_win), 8);
	gtk_window_set_position(GTK_WINDOW(quote_desc_win), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(quote_desc_win), TRUE);
	gtk_window_set_policy(GTK_WINDOW(quote_desc_win), FALSE, FALSE, FALSE);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(quote_desc_win), vbox);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	vbox2 = gtk_vbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);

#define PACK_LABEL() \
	gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0); \
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT); \
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	label = gtk_label_new
		("%d\n"		/* date */
		 "%f\n"		/* from */
		 "%N\n"		/* full name of sender */
		 "%F\n"		/* first name of sender */
		 "%I\n"		/* initial of sender */
		 "%s\n"		/* subject */
		 "%t\n"		/* to */
		 "%c\n"		/* cc */
		 "%n\n"		/* newsgroups */
		 "%i");		/* message id */
	PACK_LABEL();

	label = gtk_label_new
		("?x{expr}");	/* condition */
	PACK_LABEL();

	label = gtk_label_new
		("%M\n"		/* message body */
		 "%Q\n"		/* quoted message body */
		 "%m\n"		/* message body without signature */
		 "%q\n"		/* quoted message body without signature */
		 "%%");		/* literal percent */
	PACK_LABEL();

	label = gtk_label_new
		("\\\\\n"	/* literal backslash */
		 "\\?\n"	/* literal question mark */
		 "\\{\n"	/* literal opening curly brace */
		 "\\}");	/* literal closing curly brace */
	PACK_LABEL();

	vbox2 = gtk_vbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);

	label = gtk_label_new
		(_("Date\n"
		   "From\n"
		   "Full Name of Sender\n"
		   "First Name of Sender\n"
		   "Initial of Sender\n"
		   "Subject\n"
		   "To\n"
		   "Cc\n"
		   "Newsgroups\n"
		   "Message-ID"));
	PACK_LABEL();

	label = gtk_label_new
		(_("If x is set, displays expr"));
	PACK_LABEL();

	label = gtk_label_new
		(_("Message body\n"
		   "Quoted message body\n"
		   "Message body without signature\n"
		   "Quoted message body without signature\n"
		   "Literal %"));
	PACK_LABEL();

	label = gtk_label_new
		(_("Literal backslash\n"
		   "Literal question mark\n"
		   "Literal opening curly brace\n"
		   "Literal closing curly brace"));
	PACK_LABEL();

#undef PACK_LABEL

	gtkut_stock_button_set_create(&hbbox, &ok_btn, GTK_STOCK_OK,
				      NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

	gtk_widget_grab_default(ok_btn);
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect
		(G_OBJECT(quote_desc_win), "key_press_event",
		 G_CALLBACK(prefs_quote_description_key_pressed), NULL);
	g_signal_connect(G_OBJECT(quote_desc_win), "delete_event",
			 G_CALLBACK(prefs_quote_description_deleted), NULL);

	gtk_widget_show_all(vbox);
}

static gboolean prefs_quote_description_key_pressed(GtkWidget *widget,
						    GdkEventKey *event,
						    gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_main_quit();
	return FALSE;
}

static gboolean prefs_quote_description_deleted(GtkWidget *widget,
						GdkEventAny *event,
						gpointer data)
{
	gtk_main_quit();
	return TRUE;
}

static void prefs_keybind_select(void)
{
	GtkWidget *window;
	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_title (GTK_WINDOW (window), _("Key bindings"));
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, FALSE);
	manage_window_set_transient (GTK_WINDOW (window));

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_container_add (GTK_CONTAINER (window), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new(_("Select the preset of key bindings."));
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	combo = gtk_combo_new ();
	gtk_box_pack_start (GTK_BOX (hbox1), combo, TRUE, TRUE, 0);
	gtkut_combo_set_items (GTK_COMBO (combo),
			       _("Default"),
			       "Mew / Wanderlust",
			       "Mutt",
			       _("Old Sylpheed"),
			       NULL);
	gtk_editable_set_editable
		(GTK_EDITABLE (GTK_COMBO (combo)->entry), FALSE);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	gtkut_stock_button_set_create (&confirm_area, &ok_btn, GTK_STOCK_OK,
				       &cancel_btn, GTK_STOCK_CANCEL,
				       NULL, NULL);
	gtk_box_pack_end (GTK_BOX (hbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (ok_btn);

	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	g_signal_connect (G_OBJECT (window), "delete_event",
			  G_CALLBACK (prefs_keybind_deleted), NULL);
	g_signal_connect (G_OBJECT (window), "key_press_event",
			  G_CALLBACK (prefs_keybind_key_pressed), NULL);
	g_signal_connect (G_OBJECT (ok_btn), "clicked",
			  G_CALLBACK (prefs_keybind_apply_clicked), NULL);
	g_signal_connect (G_OBJECT (cancel_btn), "clicked",
			  G_CALLBACK (prefs_keybind_cancel), NULL);

	gtk_widget_show_all(window);

	keybind.window = window;
	keybind.combo = combo;
}

static gboolean prefs_keybind_key_pressed(GtkWidget *widget, GdkEventKey *event,
					  gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_keybind_cancel();
	return FALSE;
}

static gint prefs_keybind_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data)
{
	prefs_keybind_cancel();
	return TRUE;
}

static void prefs_keybind_cancel(void)
{
	gtk_widget_destroy(keybind.window);
	keybind.window = NULL;
	keybind.combo = NULL;
}

struct KeyBind {
	const gchar *accel_path;
	const gchar *accel_key;
};

static void prefs_keybind_apply(struct KeyBind keybind[], gint num)
{
	gint i;
	guint key;
	GdkModifierType mods;

	for (i = 0; i < num; i++) {
		const gchar *accel_key =
			keybind[i].accel_key ? keybind[i].accel_key : "";
		gtk_accelerator_parse(accel_key, &key, &mods);
		gtk_accel_map_change_entry(keybind[i].accel_path,
					   key, mods, TRUE);
	}
}

static void prefs_keybind_apply_clicked(GtkWidget *widget)
{
	GtkEntry *entry = GTK_ENTRY(GTK_COMBO(keybind.combo)->entry);
	const gchar *text;
	struct KeyBind *menurc;
	gint n_menurc;

	static struct KeyBind default_menurc[] = {
		{"<Main>/File/Empty all trash",			""},
		{"<Main>/File/Save as...",			"<control>S"},
		{"<Main>/File/Print...",			""},
		{"<Main>/File/Exit",				"<control>Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search messages...",		"<shift><control>F"},

		{"<Main>/View/Show or hide/Message view",	"V"},
		{"<Main>/View/Thread view",			"<control>T"},
		{"<Main>/View/Go to/Prev message",		"P"},
		{"<Main>/View/Go to/Next message",		"N"},
		{"<Main>/View/Go to/Prev unread message",	"<shift>P"},
		{"<Main>/View/Go to/Next unread message",	"<shift>N"},
		{"<Main>/View/Go to/Other folder...",		"G"},
		{"<Main>/View/Open in new window",		"<control><alt>N"},
		{"<Main>/View/View source",			"<control>U"},
		{"<Main>/View/Show all headers",		"<control>H"},
		{"<Main>/View/Update",				"<control><alt>U"},

		{"<Main>/Message/Receive/Get new mail",			"<control>I"},
		{"<Main>/Message/Receive/Get from all accounts",	"<shift><control>I"},
		{"<Main>/Message/Compose new message",		"<control>M"},
		{"<Main>/Message/Reply",			"<control>R"},
		{"<Main>/Message/Reply to/all",			"<shift><control>R"},
		{"<Main>/Message/Reply to/sender",		""},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			"<control><alt>F"},
		{"<Main>/Message/Forward as attachment",	""},
		{"<Main>/Message/Move...",			"<control>O"},
		{"<Main>/Message/Copy...",			"<shift><control>O"},
		{"<Main>/Message/Delete",			"<control>D"},
		{"<Main>/Message/Mark/Set flag",		"<shift>asterisk"},
		{"<Main>/Message/Mark/Unset flag",		"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>exclam"},
		{"<Main>/Message/Mark/Mark as read",		""},

		{"<Main>/Tools/Address book",			"<shift><control>A"},
		{"<Main>/Tools/Execute",			"X"},
		{"<Main>/Tools/Log window",			"<shift><control>L"},

		{"<Compose>/File/Close",			"<control>W"},
		{"<Compose>/Edit/Select all",			"<control>A"},
	};

	static struct KeyBind mew_wl_menurc[] = {
		{"<Main>/File/Empty all trash",			"<shift>D"},
		{"<Main>/File/Save as...",			"Y"},
		{"<Main>/File/Print...",			"<shift>numbersign"},
		{"<Main>/File/Exit",				"<shift>Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search messages...",		"<control>S"},

		{"<Main>/View/Show or hide/Message view",	"V"},
		{"<Main>/View/Thread view",			"<shift>T"},
		{"<Main>/View/Go to/Prev message",		"P"},
		{"<Main>/View/Go to/Next message",		"N"},
		{"<Main>/View/Go to/Prev unread message",	"<shift>P"},
		{"<Main>/View/Go to/Next unread message",	"<shift>N"},
		{"<Main>/View/Go to/Other folder...",		"G"},
		{"<Main>/View/Open in new window",		"<control><alt>N"},
		{"<Main>/View/View source",			"<control>U"},
		{"<Main>/View/Show all headers",		"<shift>H"},
		{"<Main>/View/Update",				"<shift>S"},

		{"<Main>/Message/Receive/Get new mail",			"<control>I"},
		{"<Main>/Message/Receive/Get from all accounts",	"<shift><control>I"},
		{"<Main>/Message/Compose new message",		"W"},
		{"<Main>/Message/Reply",			"<control>R"},
		{"<Main>/Message/Reply to/all",			"<shift>A"},
		{"<Main>/Message/Reply to/sender",		""},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			"F"},
		{"<Main>/Message/Forward as attachment",	"<shift>F"},
		{"<Main>/Message/Move...",			"O"},
		{"<Main>/Message/Copy...",			"<shift>O"},
		{"<Main>/Message/Delete",			"D"},
		{"<Main>/Message/Mark/Set flag",		"<shift>asterisk"},
		{"<Main>/Message/Mark/Unset flag",		"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>exclam"},
		{"<Main>/Message/Mark/Mark as read",		"<shift>R"},

		{"<Main>/Tools/Address book",			"<shift><control>A"},
		{"<Main>/Tools/Execute",			"X"},
		{"<Main>/Tools/Log window",			"<shift><control>L"},

		{"<Compose>/File/Close",			"<alt>W"},
		{"<Compose>/Edit/Select all",			""},
	};

	static struct KeyBind mutt_menurc[] = {
		{"<Main>/File/Empty all trash",			""},
		{"<Main>/File/Save as...",			"S"},
		{"<Main>/File/Print...",			"P"},
		{"<Main>/File/Exit",				"Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search messages...",		"slash"},

		{"<Main>/View/Show or hide/Message view",	"V"},
		{"<Main>/View/Thread view",			"<control>T"},
		{"<Main>/View/Go to/Prev message",		""},
		{"<Main>/View/Go to/Next message",		""},
		{"<Main>/View/Go to/Prev unread message",	""},
		{"<Main>/View/Go to/Next unread message",	""},
		{"<Main>/View/Go to/Other folder...",		"C"},
		{"<Main>/View/Open in new window",		"<control><alt>N"},
		{"<Main>/View/View source",			"<control>U"},
		{"<Main>/View/Show all headers",		"<control>H"},
		{"<Main>/View/Update",				"<control><alt>U"},

		{"<Main>/Message/Receive/Get new mail",			"<control>I"},
		{"<Main>/Message/Receive/Get from all accounts",	"<shift><control>I"},
		{"<Main>/Message/Compose new message",		"M"},
		{"<Main>/Message/Reply",			"R"},
		{"<Main>/Message/Reply to/all",			"G"},
		{"<Main>/Message/Reply to/sender",		""},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			"F"},
		{"<Main>/Message/Forward as attachment",	""},
		{"<Main>/Message/Move...",			"<control>O"},
		{"<Main>/Message/Copy...",			"<shift>C"},
		{"<Main>/Message/Delete",			"D"},
		{"<Main>/Message/Mark/Set flag",		"<shift>F"},
		{"<Main>/Message/Mark/Unset flag",		"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>N"},
		{"<Main>/Message/Mark/Mark as read",		""},

		{"<Main>/Tools/Address book",			"<shift><control>A"},
		{"<Main>/Tools/Execute",			"X"},
		{"<Main>/Tools/Log window",			"<shift><control>L"},

		{"<Compose>/File/Close",			"<alt>W"},
		{"<Compose>/Edit/Select all",			""},
	};

	static struct KeyBind old_sylpheed_menurc[] = {
		{"<Main>/File/Empty all trash",			""},
		{"<Main>/File/Save as...",			""},
		{"<Main>/File/Print...",			"<alt>P"},
		{"<Main>/File/Exit",				"<alt>Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search messages...",		"<control>S"},

		{"<Main>/View/Show or hide/Message view",	"V"},
		{"<Main>/View/Thread view",			"<control>T"},
		{"<Main>/View/Go to/Prev message",		"P"},
		{"<Main>/View/Go to/Next message",		"N"},
		{"<Main>/View/Go to/Prev unread message",	"<shift>P"},
		{"<Main>/View/Go to/Next unread message",	"<shift>N"},
		{"<Main>/View/Go to/Other folder...",		"<alt>G"},
		{"<Main>/View/Open in new window",		"<shift><control>N"},
		{"<Main>/View/View source",			"<control>U"},
		{"<Main>/View/Show all headers",		"<control>H"},
		{"<Main>/View/Update",				"<alt>U"},

		{"<Main>/Message/Receive/Get new mail",			"<alt>I"},
		{"<Main>/Message/Receive/Get from all accounts",	"<shift><alt>I"},
		{"<Main>/Message/Compose new message",		"<alt>N"},
		{"<Main>/Message/Reply",			"<alt>R"},
		{"<Main>/Message/Reply to/all",			"<shift><alt>R"},
		{"<Main>/Message/Reply to/sender",		"<control><alt>R"},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			"<shift><alt>F"},
		{"<Main>/Message/Forward as attachment",	"<shift><control>F"},
		{"<Main>/Message/Move...",			"<alt>O"},
		{"<Main>/Message/Copy...",			""},
		{"<Main>/Message/Delete",			"<alt>D"},
		{"<Main>/Message/Mark/Set flag",		"<shift>asterisk"},
		{"<Main>/Message/Mark/Unset flag",		"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>exclam"},
		{"<Main>/Message/Mark/Mark as read",		""},

		{"<Main>/Tools/Address book",			"<alt>A"},
		{"<Main>/Tools/Execute",			"<alt>X"},
		{"<Main>/Tools/Log window",			"<alt>L"},

		{"<Compose>/File/Close",			"<alt>W"},
		{"<Compose>/Edit/Select all",			""},
	};

	static struct KeyBind empty_menurc[] = {
		{"<Main>/File/Empty all trash",			""},
		{"<Main>/File/Save as...",			""},
		{"<Main>/File/Print...",			""},
		{"<Main>/File/Exit",				""},

		{"<Main>/Edit/Copy",				""},
		{"<Main>/Edit/Select all",			""},
		{"<Main>/Edit/Find in current message...",	""},
		{"<Main>/Edit/Search messages...",		""},

		{"<Main>/View/Show or hide/Message view",	""},
		{"<Main>/View/Thread view",			""},
		{"<Main>/View/Go to/Prev message",		""},
		{"<Main>/View/Go to/Next message",		""},
		{"<Main>/View/Go to/Prev unread message",	""},
		{"<Main>/View/Go to/Next unread message",	""},
		{"<Main>/View/Go to/Other folder...",		""},
		{"<Main>/View/Open in new window",		""},
		{"<Main>/View/View source",			""},
		{"<Main>/View/Show all headers",		""},
		{"<Main>/View/Update",				""},

		{"<Main>/Message/Receive/Get new mail",			""},
		{"<Main>/Message/Receive/Get from all accounts",	""},
		{"<Main>/Message/Compose new message",		""},
		{"<Main>/Message/Reply",			""},
		{"<Main>/Message/Reply to/all",			""},
		{"<Main>/Message/Reply to/sender",		""},
		{"<Main>/Message/Reply to/mailing list",	""},
		{"<Main>/Message/Forward",			""},
		{"<Main>/Message/Forward as attachment",	""},
		{"<Main>/Message/Move...",			""},
		{"<Main>/Message/Copy...",			""},
		{"<Main>/Message/Delete",			""},
		{"<Main>/Message/Mark/Set flag",		""},
		{"<Main>/Message/Mark/Unset flag",		""},
		{"<Main>/Message/Mark/Mark as unread",		""},
		{"<Main>/Message/Mark/Mark as read",		""},

		{"<Main>/Tools/Address book",			""},
		{"<Main>/Tools/Execute",			""},
		{"<Main>/Tools/Log window",			""},

		{"<Compose>/File/Close",			""},
		{"<Compose>/Edit/Select all",			""},
	};

	text = gtk_entry_get_text(entry);

	if (!strcmp(text, _("Default"))) {
		menurc = default_menurc;
		n_menurc = G_N_ELEMENTS(default_menurc);
	} else if (!strcmp(text, "Mew / Wanderlust")) {
		menurc = mew_wl_menurc;
		n_menurc = G_N_ELEMENTS(mew_wl_menurc);
	} else if (!strcmp(text, "Mutt")) {
		menurc = mutt_menurc;
		n_menurc = G_N_ELEMENTS(mutt_menurc);
	} else if (!strcmp(text, _("Old Sylpheed"))) {
		menurc = old_sylpheed_menurc;
		n_menurc = G_N_ELEMENTS(old_sylpheed_menurc);
	} else
		return;

	prefs_keybind_apply(empty_menurc, G_N_ELEMENTS(empty_menurc));
	prefs_keybind_apply(menurc, n_menurc);

	gtk_widget_destroy(keybind.window);
	keybind.window = NULL;
	keybind.combo = NULL;
}

static void prefs_common_charset_set_data_from_optmenu(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gchar *charset;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*ui_data->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	charset = g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID);
	g_free(*((gchar **)pparam->data));
	*((gchar **)pparam->data) = g_strdup(charset);
}

static void prefs_common_charset_set_optmenu(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	GtkOptionMenu *optmenu;
	gint index;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	optmenu = GTK_OPTION_MENU(*ui_data->widget);
	g_return_if_fail(optmenu != NULL);

	index = menu_find_option_menu_index(optmenu, *((gchar **)pparam->data),
					    (GCompareFunc)strcmp2);
	if (index >= 0)
		gtk_option_menu_set_history(optmenu, index);
	else {
		gtk_option_menu_set_history(optmenu, 0);
		prefs_common_charset_set_data_from_optmenu(pparam);
	}
}

static void prefs_common_recv_dialog_set_optmenu(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	RecvDialogMode mode = *((RecvDialogMode *)pparam->data);
	GtkOptionMenu *optmenu;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gint index;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	optmenu = GTK_OPTION_MENU(*ui_data->widget);
	g_return_if_fail(optmenu != NULL);

	index = menu_find_option_menu_index(optmenu, GINT_TO_POINTER(mode),
					    NULL);
	if (index >= 0)
		gtk_option_menu_set_history(optmenu, index);
	else {
		gtk_option_menu_set_history(optmenu, 0);
		prefs_set_data_from_optmenu(pparam);
	}

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
}

static void prefs_common_uri_set_data_from_entry(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	gchar **str;
	const gchar *entry_str;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	entry_str = gtk_entry_get_text(GTK_ENTRY(*ui_data->widget));

	if (pparam->type == P_STRING) {
		str = (gchar **)pparam->data;
		g_free(*str);

		if (entry_str[0] == '\0' ||
		    !strcmp(_("(Default browser)"), entry_str))
			*str = NULL;
		else
			*str = g_strdup(entry_str);
	} else {
		g_warning("Invalid type for URI setting\n");
	}
}

static void prefs_common_uri_set_entry(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	gchar **str;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	if (pparam->type == P_STRING) {
		str = (gchar **)pparam->data;
		gtk_entry_set_text(GTK_ENTRY(*ui_data->widget),
				   *str ? *str : _("(Default browser)"));
	} else {
		g_warning("Invalid type for URI setting\n");
	}
}

static void prefs_common_addr_compl_set_data_from_radiobtn(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	GtkRadioButton *radiobtn;
	GSList *group;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	radiobtn = GTK_RADIO_BUTTON(*ui_data->widget);
	group = gtk_radio_button_get_group(radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON(group->data);
		gint mode;

		if (gtk_toggle_button_get_active(btn)) {
			mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn),
								 MENU_VAL_ID));
			if (mode == 2) {
				prefs_common.enable_address_completion = FALSE;
				prefs_common.fullauto_completion_mode = FALSE;
			} else {
				prefs_common.enable_address_completion = TRUE;
				if (mode == 0)
					prefs_common.fullauto_completion_mode = TRUE;
				else
					prefs_common.fullauto_completion_mode = FALSE;
			}
			break;
		}
		group = group->next;
	}
}

static void prefs_common_addr_compl_set_radiobtn(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	GtkRadioButton *radiobtn;
	GSList *group;
	gint mode;

	if (prefs_common.enable_address_completion) {
		if (prefs_common.fullauto_completion_mode)
			mode = 0;
		else
			mode = 1;
	} else
		mode = 2;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	radiobtn = GTK_RADIO_BUTTON(*ui_data->widget);
	group = gtk_radio_button_get_group(radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON(group->data);
		gint data;

		data = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn),
							 MENU_VAL_ID));
		if (data == mode) {
			gtk_toggle_button_set_active(btn, TRUE);
			break;
		}
		group = group->next;
	}
}

static void prefs_common_attach_toolbtn_pos_set_data_from_radiobtn(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	GtkRadioButton *radiobtn;
	GSList *group;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	radiobtn = GTK_RADIO_BUTTON(*ui_data->widget);
	group = gtk_radio_button_get_group(radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON(group->data);

		if (gtk_toggle_button_get_active(btn)) {
			prefs_common.attach_toolbutton_pos =
				GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), MENU_VAL_ID));
			break;
		}
		group = group->next;
	}
}

static void prefs_common_attach_toolbtn_pos_set_radiobtn(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	GtkRadioButton *radiobtn;
	GSList *group;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	radiobtn = GTK_RADIO_BUTTON(*ui_data->widget);
	group = gtk_radio_button_get_group(radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON(group->data);
		gint data;

		data = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn),
							 MENU_VAL_ID));
		if (data == prefs_common.attach_toolbutton_pos) {
			gtk_toggle_button_set_active(btn, TRUE);
			break;
		}
		group = group->next;
	}
}

static void prefs_common_online_mode_set_data_from_radiobtn(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	GtkRadioButton *radiobtn;
	GSList *group;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	radiobtn = GTK_RADIO_BUTTON(*ui_data->widget);
	group = gtk_radio_button_get_group(radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON(group->data);

		if (gtk_toggle_button_get_active(btn)) {
			prefs_common.startup_online_mode =
				GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), MENU_VAL_ID));
			break;
		}
		group = group->next;
	}
}

static void prefs_common_online_mode_set_radiobtn(PrefParam *pparam)
{
	PrefsUIData *ui_data;
	GtkRadioButton *radiobtn;
	GSList *group;

	ui_data = (PrefsUIData *)pparam->ui_data;
	g_return_if_fail(ui_data != NULL);
	g_return_if_fail(*ui_data->widget != NULL);

	radiobtn = GTK_RADIO_BUTTON(*ui_data->widget);
	group = gtk_radio_button_get_group(radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON(group->data);
		gint data;

		data = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn),
							 MENU_VAL_ID));
		if (data == prefs_common.startup_online_mode) {
			gtk_toggle_button_set_active(btn, TRUE);
			break;
		}
		group = group->next;
	}
}

static void prefs_common_dispitem_clicked(void)
{
	prefs_summary_column_open(FOLDER_ITEM_IS_SENT_FOLDER
		(main_window_get()->summaryview->folder_item));
}

static void prefs_common_select_folder_cb(GtkWidget *widget, gpointer data)
{
	FolderItem *item;
	gchar *id;

	item = foldersel_folder_sel(NULL, FOLDER_SEL_COPY, NULL);
	if (item && item->path) {
		id = folder_item_get_identifier(item);
		if (id) {
			gtk_entry_set_text(GTK_ENTRY(data), id);
			g_free(id);
		}
	}
}

#ifdef G_OS_WIN32
static void prefs_common_file_selection_cb(GtkButton *button, gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(data);
	gchar *file;
	gchar *ufile;

	file = filesel_select_file(_("Select file"), NULL,
				   GTK_FILE_CHOOSER_ACTION_OPEN);
	if (file) {
		ufile = conv_filename_to_utf8(file);
		gtk_entry_set_text(entry, ufile);
		g_free(ufile);
	}
	g_free(file);
}
#endif

static gint prefs_common_deleted(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	prefs_common_cancel();
	return TRUE;
}

static gboolean prefs_common_key_pressed(GtkWidget *widget, GdkEventKey *event,
					 gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_common_cancel();
	return FALSE;
}

static void prefs_common_ok(void)
{
	prefs_common_apply();
	gtk_widget_hide(dialog.window);
	if (quote_desc_win && GTK_WIDGET_VISIBLE(quote_desc_win))
		gtk_widget_hide(quote_desc_win);

	main_window_popup(main_window_get());
	inc_unlock();
}

static void prefs_common_apply(void)
{
	prefs_set_data_from_dialog(prefs_common_get_params());
	gtkut_stock_button_set_set_reverse(!prefs_common.comply_gnome_hig);
	main_window_reflect_prefs_all();
	compose_reflect_prefs_all();
	prefs_common_colorlabel_update();
	colorlabel_write_config();
	sock_set_io_timeout(prefs_common.io_timeout_secs);
	prefs_common_write_config();

	inc_autocheck_timer_remove();
	inc_autocheck_timer_set();
}

static void prefs_common_cancel(void)
{
	gtk_widget_hide(dialog.window);
	main_window_popup(main_window_get());
	inc_unlock();
}
