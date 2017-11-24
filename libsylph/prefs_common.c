/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <errno.h>

#include "prefs.h"
#include "prefs_common.h"
#include "filter.h"
#include "codeconv.h"
#include "utils.h"

PrefsCommon prefs_common;

static PrefParam param[] = {
	/* Receive */
	{"autochk_newmail", "FALSE", &prefs_common.autochk_newmail, P_BOOL},
	{"autochk_interval", "10", &prefs_common.autochk_itv, P_INT},
	{"check_on_startup", "FALSE", &prefs_common.chk_on_startup, P_BOOL},
	{"scan_all_after_inc", "FALSE", &prefs_common.scan_all_after_inc,
	 P_BOOL},
	{"enable_newmsg_notify", "FALSE", &prefs_common.enable_newmsg_notify,
	 P_BOOL},
	{"newmsg_notify_command", NULL, &prefs_common.newmsg_notify_cmd,
	 P_STRING},
	{"enable_newmsg_notify_sound", "FALSE",
	 &prefs_common.enable_newmsg_notify_sound, P_BOOL},
	{"newmsg_notify_sound", NULL, &prefs_common.newmsg_notify_sound,
	 P_STRING},
	{"enable_newmsg_notify_window", "TRUE",
	 &prefs_common.enable_newmsg_notify_window, P_BOOL},
	{"notify_window_period", "10",
	 &prefs_common.notify_window_period, P_INT},

	{"inc_local", "FALSE", &prefs_common.inc_local, P_BOOL},
	{"filter_on_inc_local", "TRUE", &prefs_common.filter_on_inc, P_BOOL},
	{"spool_path", DEFAULT_SPOOL_PATH, &prefs_common.spool_path, P_STRING},

	/* Send */
	{"save_message", "TRUE", &prefs_common.savemsg, P_BOOL},
	{"filter_sent_message", "FALSE", &prefs_common.filter_sent, P_BOOL},
	{"recipients_autoreg", "TRUE", &prefs_common.recipients_autoreg,
	 P_BOOL},
	{"show_send_dialog", "TRUE", &prefs_common.show_send_dialog, P_BOOL},

	{"encoding_method", "0", &prefs_common.encoding_method, P_ENUM},
	{"mime_filename_encoding_method", "0",
	 &prefs_common.mime_fencoding_method, P_ENUM},
	{"check_attach", "FALSE", &prefs_common.check_attach, P_BOOL},
	{"check_attach_str", NULL, &prefs_common.check_attach_str, P_STRING},
	{"check_recipients", "FALSE", &prefs_common.check_recipients, P_BOOL},
	{"check_recp_exclude", NULL, &prefs_common.check_recp_exclude,
	 P_STRING},

	{"allow_jisx0201_kana", "FALSE", &prefs_common.allow_jisx0201_kana,
	 P_BOOL},

	/* Compose */
	{"auto_signature", "TRUE", &prefs_common.auto_sig, P_BOOL},
	{"signature_separator", "-- ", &prefs_common.sig_sep, P_STRING},

	{"auto_ext_editor", "FALSE", &prefs_common.auto_exteditor, P_BOOL},

	{"undo_level", "50", &prefs_common.undolevels, P_INT},

	{"linewrap_length", "72", &prefs_common.linewrap_len, P_INT},
	{"linewrap_quotation", "FALSE", &prefs_common.linewrap_quote, P_BOOL},
	{"linewrap_auto", "FALSE", &prefs_common.autowrap, P_BOOL},

	{"enable_autosave", "FALSE", &prefs_common.enable_autosave, P_BOOL},
	{"autosave_interval", "5", &prefs_common.autosave_itv, P_INT},

	{"reply_with_quote", "TRUE", &prefs_common.reply_with_quote, P_BOOL},
	{"reply_account_autoselect", "TRUE",
	 &prefs_common.reply_account_autosel, P_BOOL},
	{"default_reply_list", "TRUE", &prefs_common.default_reply_list,
	 P_BOOL},
	{"inherit_recipient_on_self_reply", "FALSE",
	 &prefs_common.inherit_recipient_on_self_reply, P_BOOL},
	{"reply_address_only", "FALSE",
	 &prefs_common.reply_address_only, P_BOOL},

	{"show_ruler", "TRUE", &prefs_common.show_ruler, P_BOOL},

	/* Quote */
	{"reply_quote_mark", "> ", &prefs_common.quotemark, P_STRING},
	{"reply_quote_format", "On %d\\n%f wrote:\\n\\n%Q",
	 &prefs_common.quotefmt, P_STRING},

	{"forward_quote_mark", "> ", &prefs_common.fw_quotemark, P_STRING},
	{"forward_quote_format",
	 "\\n\\nBegin forwarded message:\\n\\n"
	 "?d{Date: %d\\n}?f{From: %f\\n}?t{To: %t\\n}?c{Cc: %c\\n}"
	 "?n{Newsgroups: %n\\n}?s{Subject: %s\\n}\\n\\n%M",
	 &prefs_common.fw_quotefmt, P_STRING},

	/* Spelling */
	{"check_spell", "FALSE", &prefs_common.check_spell, P_BOOL},
	{"spell_lang", "en", &prefs_common.spell_lang, P_STRING},

	/* Display */
	{"message_font_name", DEFAULT_MESSAGE_FONT, &prefs_common.textfont,
	 P_STRING},

	{"display_folder_unread_num", "TRUE",
	 &prefs_common.display_folder_unread, P_BOOL},
	{"display_folder_num_columns", "FALSE",
	 &prefs_common.display_folder_num_columns, P_BOOL},
	{"folder_col_show_new", "FALSE",
	 &prefs_common.folder_col_visible[1], P_BOOL},
	{"folder_col_show_unread", "FALSE",
	 &prefs_common.folder_col_visible[2], P_BOOL},
	{"folder_col_show_total", "FALSE",
	 &prefs_common.folder_col_visible[3], P_BOOL},

	{"newsgroup_abbrev_len", "16", &prefs_common.ng_abbrev_len, P_INT},

	{"translate_header", "TRUE", &prefs_common.trans_hdr, P_BOOL},

	/* Display: Summary View */
	{"enable_swap_from", "FALSE", &prefs_common.swap_from, P_BOOL},
	{"date_format", "%y/%m/%d(%a) %H:%M", &prefs_common.date_format,
	 P_STRING},
	{"expand_thread", "TRUE", &prefs_common.expand_thread, P_BOOL},

	{"enable_rules_hint", "TRUE", &prefs_common.enable_rules_hint, P_BOOL},
	{"bold_unread", "TRUE", &prefs_common.bold_unread, P_BOOL},

	{"persist_qsearch_filter", "TRUE", &prefs_common.persist_qsearch_filter,
	 P_BOOL},

	{"toolbar_style", "4", &prefs_common.toolbar_style, P_ENUM},
	{"show_searchbar", "TRUE", &prefs_common.show_searchbar, P_BOOL},
	{"show_statusbar", "TRUE", &prefs_common.show_statusbar, P_BOOL},

	{"main_toolbar_setting", NULL, &prefs_common.main_toolbar_setting,
	 P_STRING},
	{"compose_toolbar_setting", NULL,
	 &prefs_common.compose_toolbar_setting, P_STRING},

	{"layout_type", "0", &prefs_common.layout_type, P_ENUM},

	{"summary_col_show_mark", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_MARK], P_BOOL},
	{"summary_col_show_unread", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_UNREAD], P_BOOL},
	{"summary_col_show_mime", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_MIME], P_BOOL},
	{"summary_col_show_subject", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_SUBJECT], P_BOOL},
	{"summary_col_show_from", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_FROM], P_BOOL},
	{"summary_col_show_date", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_DATE], P_BOOL},
	{"summary_col_show_size", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_SIZE], P_BOOL},
	{"summary_col_show_number", "FALSE",
	 &prefs_common.summary_col_visible[S_COL_NUMBER], P_BOOL},
	{"summary_col_show_to", "FALSE",
	 &prefs_common.summary_col_visible[S_COL_TO], P_BOOL},

	{"summary_col_pos_mark", "0",
	  &prefs_common.summary_col_pos[S_COL_MARK], P_INT},
	{"summary_col_pos_unread", "1",
	  &prefs_common.summary_col_pos[S_COL_UNREAD], P_INT},
	{"summary_col_pos_mime", "2",
	  &prefs_common.summary_col_pos[S_COL_MIME], P_INT},
	{"summary_col_pos_subject", "3",
	  &prefs_common.summary_col_pos[S_COL_SUBJECT], P_INT},
	{"summary_col_pos_from", "4",
	  &prefs_common.summary_col_pos[S_COL_FROM], P_INT},
	{"summary_col_pos_date", "5",
	  &prefs_common.summary_col_pos[S_COL_DATE], P_INT},
	{"summary_col_pos_size", "6",
	  &prefs_common.summary_col_pos[S_COL_SIZE], P_INT},
	{"summary_col_pos_number", "7",
	  &prefs_common.summary_col_pos[S_COL_NUMBER], P_INT},
	{"summary_col_pos_to", "8",
	  &prefs_common.summary_col_pos[S_COL_TO], P_INT},

	{"summary_sent_col_show_mark", "TRUE",
	 &prefs_common.summary_sent_col_visible[S_COL_MARK], P_BOOL},
	{"summary_sent_col_show_unread", "TRUE",
	 &prefs_common.summary_sent_col_visible[S_COL_UNREAD], P_BOOL},
	{"summary_sent_col_show_mime", "TRUE",
	 &prefs_common.summary_sent_col_visible[S_COL_MIME], P_BOOL},
	{"summary_sent_col_show_subject", "TRUE",
	 &prefs_common.summary_sent_col_visible[S_COL_SUBJECT], P_BOOL},
	{"summary_sent_col_show_from", "FALSE",
	 &prefs_common.summary_sent_col_visible[S_COL_FROM], P_BOOL},
	{"summary_sent_col_show_date", "TRUE",
	 &prefs_common.summary_sent_col_visible[S_COL_DATE], P_BOOL},
	{"summary_sent_col_show_size", "TRUE",
	 &prefs_common.summary_sent_col_visible[S_COL_SIZE], P_BOOL},
	{"summary_sent_col_show_number", "FALSE",
	 &prefs_common.summary_sent_col_visible[S_COL_NUMBER], P_BOOL},
	{"summary_sent_col_show_to", "TRUE",
	 &prefs_common.summary_sent_col_visible[S_COL_TO], P_BOOL},

	{"summary_sent_col_pos_mark", "0",
	  &prefs_common.summary_sent_col_pos[S_COL_MARK], P_INT},
	{"summary_sent_col_pos_unread", "1",
	  &prefs_common.summary_sent_col_pos[S_COL_UNREAD], P_INT},
	{"summary_sent_col_pos_mime", "2",
	  &prefs_common.summary_sent_col_pos[S_COL_MIME], P_INT},
	{"summary_sent_col_pos_subject", "3",
	  &prefs_common.summary_sent_col_pos[S_COL_SUBJECT], P_INT},
	{"summary_sent_col_pos_from", "8",
	  &prefs_common.summary_sent_col_pos[S_COL_FROM], P_INT},
	{"summary_sent_col_pos_date", "5",
	  &prefs_common.summary_sent_col_pos[S_COL_DATE], P_INT},
	{"summary_sent_col_pos_size", "6",
	  &prefs_common.summary_sent_col_pos[S_COL_SIZE], P_INT},
	{"summary_sent_col_pos_number", "7",
	  &prefs_common.summary_sent_col_pos[S_COL_NUMBER], P_INT},
	{"summary_sent_col_pos_to", "4",
	  &prefs_common.summary_sent_col_pos[S_COL_TO], P_INT},

	{"summary_col_size_mark", "10",
	 &prefs_common.summary_col_size[S_COL_MARK], P_INT},
	{"summary_col_size_unread", "13",
	 &prefs_common.summary_col_size[S_COL_UNREAD], P_INT},
	{"summary_col_size_mime", "10",
	 &prefs_common.summary_col_size[S_COL_MIME], P_INT},
	{"summary_col_size_subject", "200",
	 &prefs_common.summary_col_size[S_COL_SUBJECT], P_INT},
	{"summary_col_size_from", "120",
	 &prefs_common.summary_col_size[S_COL_FROM], P_INT},
	{"summary_col_size_date", "118",
	 &prefs_common.summary_col_size[S_COL_DATE], P_INT},
	{"summary_col_size_size", "45",
	 &prefs_common.summary_col_size[S_COL_SIZE], P_INT},
	{"summary_col_size_number", "40",
	 &prefs_common.summary_col_size[S_COL_NUMBER], P_INT},
	{"summary_col_size_to", "120",
	 &prefs_common.summary_col_size[S_COL_TO], P_INT},

	/* Widget size */
	{"folderwin_x", "16", &prefs_common.folderwin_x, P_INT},
	{"folderwin_y", "16", &prefs_common.folderwin_y, P_INT},
	{"folderview_width", "179", &prefs_common.folderview_width, P_INT},
	{"folderview_height", "450", &prefs_common.folderview_height, P_INT},
	{"folderview_visible", "TRUE", &prefs_common.folderview_visible,
	 P_BOOL},

	{"folder_col_folder", "150", &prefs_common.folder_col_folder, P_INT},
	{"folder_col_new", "32", &prefs_common.folder_col_new, P_INT},
	{"folder_col_unread", "32", &prefs_common.folder_col_unread, P_INT},
	{"folder_col_total", "32", &prefs_common.folder_col_total, P_INT},

	{"summaryview_width", "600", &prefs_common.summaryview_width, P_INT},
	{"summaryview_height", "180", &prefs_common.summaryview_height, P_INT},
	{"summaryview_vwidth", "300", &prefs_common.summaryview_vwidth, P_INT},
	{"summaryview_vheight", "600", &prefs_common.summaryview_vheight,
	 P_INT},

	{"main_messagewin_x", "256", &prefs_common.main_msgwin_x, P_INT},
	{"main_messagewin_y", "210", &prefs_common.main_msgwin_y, P_INT},
	{"messageview_width", "600", &prefs_common.msgview_width, P_INT},
	{"messageview_height", "300", &prefs_common.msgview_height, P_INT},
	{"messageview_vwidth", "300", &prefs_common.msgview_vwidth, P_INT},
	{"messageview_vheight", "600", &prefs_common.msgview_vheight, P_INT},
	{"messageview_visible", "TRUE", &prefs_common.msgview_visible, P_BOOL},

	{"mainview_x", "64", &prefs_common.mainview_x, P_INT},
	{"mainview_y", "64", &prefs_common.mainview_y, P_INT},
	{"mainview_width", "600", &prefs_common.mainview_width, P_INT},
	{"mainview_height", "600", &prefs_common.mainview_height, P_INT},
	{"mainwin_x", "64", &prefs_common.mainwin_x, P_INT},
	{"mainwin_y", "64", &prefs_common.mainwin_y, P_INT},
	{"mainwin_width", "800", &prefs_common.mainwin_width, P_INT},
	{"mainwin_height", "600", &prefs_common.mainwin_height, P_INT},
	{"messagewin_width", "600", &prefs_common.msgwin_width, P_INT},
	{"messagewin_height", "540", &prefs_common.msgwin_height, P_INT},

	{"mainwin_maximized", "FALSE", &prefs_common.mainwin_maximized, P_BOOL},

	{"sourcewin_width", "600", &prefs_common.sourcewin_width, P_INT},
	{"sourcewin_height", "500", &prefs_common.sourcewin_height, P_INT},

	{"compose_x", "32", &prefs_common.compose_x, P_INT},
	{"compose_y", "32", &prefs_common.compose_y, P_INT},
	{"compose_width", "600", &prefs_common.compose_width, P_INT},
	{"compose_height", "560", &prefs_common.compose_height, P_INT},

	{"compose_maximized", "FALSE", &prefs_common.compose_maximized, P_BOOL},

	{"addressbook_x", "32", &prefs_common.addressbook_x, P_INT},
	{"addressbook_y", "32", &prefs_common.addressbook_y, P_INT},
	{"addressbook_width", "620", &prefs_common.addressbook_width, P_INT},
	{"addressbook_height", "360", &prefs_common.addressbook_height, P_INT},
	{"addressbook_folder_width", "190", &prefs_common.addressbook_folder_width, P_INT},
	{"addressbook_col_name", "164", &prefs_common.addressbook_col_name, P_INT},
	{"addressbook_col_addr", "156", &prefs_common.addressbook_col_addr, P_INT},
	{"addressbook_col_nickname", "120", &prefs_common.addressbook_col_nickname, P_INT},
	{"addressbook_col_rem", "100", &prefs_common.addressbook_col_rem, P_INT},

	/* Message */
	{"enable_color", "TRUE", &prefs_common.enable_color, P_BOOL},

	{"quote_level1_color", "179", &prefs_common.quote_level1_col, P_INT},
	{"quote_level2_color", "179", &prefs_common.quote_level2_col, P_INT},
	{"quote_level3_color", "179", &prefs_common.quote_level3_col, P_INT},
	{"uri_color", "32512", &prefs_common.uri_col, P_INT},
	{"signature_color", "0", &prefs_common.sig_col, P_USHORT},
	{"recycle_quote_colors", "FALSE", &prefs_common.recycle_quote_colors,
	 P_BOOL},

	{"convert_mb_alnum", "FALSE", &prefs_common.conv_mb_alnum, P_BOOL},
	{"display_header_pane", "TRUE", &prefs_common.display_header_pane,
	 P_BOOL},
	{"show_attach_tab", "FALSE", &prefs_common.show_attach_tab, P_BOOL},
	{"show_attached_files_first", "TRUE",
	 &prefs_common.show_attached_files_first, P_BOOL},
	{"attach_toolbutton_pos", "0", &prefs_common.attach_toolbutton_pos,
	 P_INT},
	{"display_header", "TRUE", &prefs_common.display_header, P_BOOL},
	{"render_html", "TRUE", &prefs_common.render_html, P_BOOL},
	{"alt_prefer_html", "FALSE", &prefs_common.alt_prefer_html, P_BOOL},
	{"html_only_as_attach", "FALSE", &prefs_common.html_only_as_attach,
	 P_BOOL},
	{"line_space", "2", &prefs_common.line_space, P_INT},

	{"textview_cursor_visible", "FALSE",
	 &prefs_common.textview_cursor_visible, P_BOOL},

	{"enable_smooth_scroll", "FALSE", &prefs_common.enable_smooth_scroll,
	 P_BOOL},
	{"scroll_step", "1", &prefs_common.scroll_step, P_INT},
	{"scroll_half_page", "FALSE", &prefs_common.scroll_halfpage, P_BOOL},

	{"resize_image", "TRUE", &prefs_common.resize_image, P_BOOL},
	{"inline_image", "TRUE", &prefs_common.inline_image, P_BOOL},

	/* Encoding */
	{"default_encoding", NULL, &prefs_common.default_encoding, P_STRING},
	{"outgoing_charset", NULL, &prefs_common.outgoing_charset, P_STRING},

	{"show_other_header", "FALSE", &prefs_common.show_other_header, P_BOOL},

	/* MIME viewer */
	{"mime_image_viewer", NULL, &prefs_common.mime_image_viewer, P_STRING},
	{"mime_audio_player", NULL, &prefs_common.mime_audio_player, P_STRING},
#ifdef G_OS_WIN32
	{"mime_open_command", "notepad '%s'", &prefs_common.mime_open_cmd,
#else
	{"mime_open_command", "gedit '%s'", &prefs_common.mime_open_cmd,
#endif
	 P_STRING},
	{"mime_command", NULL, &prefs_common.mime_cmd, P_STRING},

	/* Junk mail */
	{"enable_junk", "FALSE", &prefs_common.enable_junk, P_BOOL},
#ifdef G_OS_WIN32
	{"junk_learn_command", "sylfilter -j",
	 &prefs_common.junk_learncmd, P_STRING},
	{"nojunk_learn_command", "sylfilter -c",
	 &prefs_common.nojunk_learncmd, P_STRING},
	{"junk_classify_command", "sylfilter",
	 &prefs_common.junk_classify_cmd, P_STRING},
#else
	{"junk_learn_command", "bogofilter -N -s -I",
	 &prefs_common.junk_learncmd, P_STRING},
	{"nojunk_learn_command", "bogofilter -n -S -I",
	 &prefs_common.nojunk_learncmd, P_STRING},
	{"junk_classify_command", "bogofilter -I",
	 &prefs_common.junk_classify_cmd, P_STRING},
#endif
	{"junk_folder", NULL, &prefs_common.junk_folder, P_STRING},
	{"filter_junk_on_receive", "FALSE", &prefs_common.filter_junk_on_recv,
	 P_BOOL},
	{"filter_junk_before", "FALSE", &prefs_common.filter_junk_before,
	 P_BOOL},
	{"delete_junk_on_receive", "TRUE", &prefs_common.delete_junk_on_recv,
	 P_BOOL},
	{"nofilter_junk_sender_in_book", "TRUE",
	 &prefs_common.nofilter_junk_sender_in_book, P_BOOL},
	{"mark_junk_as_read", "FALSE", &prefs_common.mark_junk_as_read, P_BOOL},

	/* Privacy */
	{"auto_check_signatures", "TRUE", &prefs_common.auto_check_signatures,
	 P_BOOL},
	{"gpg_signature_popup", "FALSE", &prefs_common.gpg_signature_popup,
	 P_BOOL},
	{"store_passphrase", "FALSE", &prefs_common.store_passphrase, P_BOOL},
	{"store_passphrase_timeout", "0",
	 &prefs_common.store_passphrase_timeout, P_INT},
	{"passphrase_grab", "FALSE", &prefs_common.passphrase_grab, P_BOOL},
#ifdef G_OS_WIN32
	{"show_gpg_warning", "FALSE", &prefs_common.gpg_warning, P_BOOL},
#else
	{"show_gpg_warning", "TRUE", &prefs_common.gpg_warning, P_BOOL},
#endif

	/* Interface */
	{"separate_folder", "FALSE", &prefs_common.sep_folder, P_BOOL},
	{"separate_message", "FALSE", &prefs_common.sep_msg, P_BOOL},

	{"always_show_message_when_selected", "TRUE",
	 &prefs_common.always_show_msg, P_BOOL},
	{"open_unread_on_enter", "FALSE", &prefs_common.open_unread_on_enter,
	 P_BOOL},
	{"always_mark_read_on_show_msg", "FALSE",
	 &prefs_common.always_mark_read_on_show_msg, P_BOOL},
	{"remember_last_selected", "FALSE",
	 &prefs_common.remember_last_selected, P_BOOL},
	{"mark_as_read_on_new_window", "FALSE",
	 &prefs_common.mark_as_read_on_new_window, P_BOOL},
	{"open_inbox_on_inc", "FALSE", &prefs_common.open_inbox_on_inc, P_BOOL},
	{"open_inbox_on_startup", "FALSE", &prefs_common.open_inbox_on_startup,
	 P_BOOL},
	{"change_account_on_folder_selection", "TRUE",
	 &prefs_common.change_account_on_folder_sel, P_BOOL},
	{"immediate_execution", "TRUE", &prefs_common.immediate_exec, P_BOOL},

#ifdef G_OS_WIN32
	{"comply_gnome_hig", "FALSE", &prefs_common.comply_gnome_hig, P_BOOL},
#else
	{"comply_gnome_hig", "TRUE", &prefs_common.comply_gnome_hig, P_BOOL},
#endif

	{"show_trayicon", "TRUE", &prefs_common.show_trayicon, P_BOOL},
	{"minimize_to_tray", "FALSE", &prefs_common.minimize_to_tray, P_BOOL},
#ifdef G_OS_WIN32
	{"toggle_window_on_trayicon_click", "FALSE",
#else
	{"toggle_window_on_trayicon_click", "TRUE",
#endif
	 &prefs_common.toggle_window_on_trayicon_click, P_BOOL},

	/* Other */
	{"receive_dialog_mode", "1", &prefs_common.recv_dialog_mode, P_ENUM},
	{"no_receive_error_panel", "FALSE", &prefs_common.no_recv_err_panel,
	 P_BOOL},
	{"close_receive_dialog", "TRUE", &prefs_common.close_recv_dialog,
	 P_BOOL},

	{"add_address_by_click", "FALSE", &prefs_common.add_address_by_click,
	 P_BOOL},
	{"enable_address_completion", "TRUE",
	 &prefs_common.enable_address_completion, P_BOOL},
	{"fullauto_completion_mode", "TRUE",
	 &prefs_common.fullauto_completion_mode, P_BOOL},
	{"always_add_address_only", "FALSE",
	 &prefs_common.always_add_address_only, P_BOOL},

	{"confirm_on_exit", "FALSE", &prefs_common.confirm_on_exit, P_BOOL},
	{"clean_trash_on_exit", "FALSE", &prefs_common.clean_on_exit, P_BOOL},
	{"ask_on_cleaning", "TRUE", &prefs_common.ask_on_clean, P_BOOL},
	{"warn_queued_on_exit", "TRUE", &prefs_common.warn_queued_on_exit,
	 P_BOOL},

	{"logwindow_line_limit", "1000", &prefs_common.logwin_line_limit,
	 P_INT},

	{"online_mode", "TRUE", &prefs_common.online_mode, P_BOOL},
	{"startup_online_mode", "1", &prefs_common.startup_online_mode, P_INT},

	/* External commands */
#ifdef G_OS_WIN32
	{"uri_open_command", NULL, &prefs_common.uri_cmd,
#else
	{"uri_open_command", DEFAULT_BROWSER_CMD, &prefs_common.uri_cmd,
#endif
	 P_STRING},
#ifdef G_OS_WIN32
	{"ext_editor_command", "notepad '%s'", &prefs_common.ext_editor_cmd,
#else
	{"ext_editor_command", "gedit %s", &prefs_common.ext_editor_cmd,
#endif
	 P_STRING},

	{"use_print_command", "FALSE", &prefs_common.use_print_cmd, P_BOOL},
#ifdef G_OS_WIN32
	{"print_command", NULL, &prefs_common.print_cmd, P_STRING},
#else
	{"print_command", "lpr %s", &prefs_common.print_cmd, P_STRING},
#endif

	{"use_ext_inc", "FALSE", &prefs_common.use_extinc, P_BOOL},
	{"ext_inc_path", DEFAULT_INC_PATH, &prefs_common.extinc_cmd, P_STRING},
	{"use_ext_sendmail", "FALSE", &prefs_common.use_extsend, P_BOOL},
	{"ext_sendmail_cmd", DEFAULT_SENDMAIL_CMD, &prefs_common.extsend_cmd,
	 P_STRING},

	/* Update check */
	{"auto_update_check", "TRUE", &prefs_common.auto_update_check, P_BOOL},
	{"use_http_proxy", "FALSE", &prefs_common.use_http_proxy, P_BOOL},
	{"http_proxy_host", NULL, &prefs_common.http_proxy_host, P_STRING},

	/* Advanced */
	{"strict_cache_check", "FALSE", &prefs_common.strict_cache_check,
	 P_BOOL},
	{"io_timeout_secs", "60", &prefs_common.io_timeout_secs, P_INT},

	/* File selector */
	{"filesel_prev_open_dir", NULL, &prefs_common.prev_open_dir, P_STRING},
	{"filesel_prev_save_dir", NULL, &prefs_common.prev_save_dir, P_STRING},
	{"filesel_prev_folder_dir", NULL, &prefs_common.prev_folder_dir, P_STRING},
	{"filesel_save_file_type", "0", &prefs_common.save_file_type, P_INT},

	{NULL, NULL, NULL, P_OTHER}
};


PrefsCommon *prefs_common_get(void)
{
	return &prefs_common;
}

PrefParam *prefs_common_get_params(void)
{
	return param;
}

void prefs_common_read_config(void)
{
	FILE *fp;
	gchar *path;
	gchar buf[PREFSBUFSIZE];

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);

	prefs_read_config(param, "Common", path, NULL);

	g_free(path);

	//prefs_common.online_mode = TRUE;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMAND_HISTORY,
			   NULL);
	if ((fp = g_fopen(path, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
		g_free(path);
		return;
	}
	g_free(path);
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		g_strstrip(buf);
		if (buf[0] == '\0') continue;
		prefs_common.mime_open_cmd_history =
			add_history(prefs_common.mime_open_cmd_history, buf);
	}
	fclose(fp);

	prefs_common.mime_open_cmd_history =
		g_list_reverse(prefs_common.mime_open_cmd_history);
}

void prefs_common_write_config(void)
{
	GList *cur;
	FILE *fp;
	gchar *path;

	prefs_write_config(param, "Common", COMMON_RC);

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMAND_HISTORY,
			   NULL);
	if ((fp = g_fopen(path, "wb")) == NULL) {
		FILE_OP_ERROR(path, "fopen");
		g_free(path);
		return;
	}

	for (cur = prefs_common.mime_open_cmd_history;
	     cur != NULL; cur = cur->next) {
		fputs((gchar *)cur->data, fp);
		fputc('\n', fp);
	}

	fclose(fp);
	g_free(path);
}

void prefs_common_junk_filter_list_set(void)
{
}

void prefs_common_junk_folder_rename_path(const gchar *old_path,
					  const gchar *new_path)
{
	gint len;
	gchar *base;
	gchar *dest_path;

	g_return_if_fail(old_path != NULL);
	g_return_if_fail(new_path != NULL);

	if (!prefs_common.junk_folder)
		return;

	len = strlen(old_path);

	if (!strncmp(old_path, prefs_common.junk_folder, len)) {
		base = prefs_common.junk_folder + len;
		if (*base != '/' && *base != '\0')
			return;
		while (*base == '/') base++;
		if (*base == '\0')
			dest_path = g_strdup(new_path);
		else
			dest_path = g_strconcat(new_path, "/", base, NULL);
		debug_print("prefs_common_junk_folder_rename_path(): "
			    "renaming %s -> %s\n", prefs_common.junk_folder,
			    dest_path);
		g_free(prefs_common.junk_folder);
		prefs_common.junk_folder = dest_path;
	}
}
