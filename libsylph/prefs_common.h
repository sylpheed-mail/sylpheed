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

#ifndef __PREFS_COMMON_H__
#define __PREFS_COMMON_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

typedef struct _PrefsCommon	PrefsCommon;

#include "enums.h"
#include "prefs.h"

typedef enum {
	RECV_DIALOG_ALWAYS,
	RECV_DIALOG_MANUAL,
	RECV_DIALOG_NEVER
} RecvDialogMode;

typedef enum {
	CTE_AUTO,
	CTE_BASE64,
	CTE_QUOTED_PRINTABLE,
	CTE_8BIT
} TransferEncodingMethod;

typedef enum {
	FENC_MIME,
	FENC_RFC2231,
	FENC_NONE
} MIMEFilenameEncodingMethod;

struct _PrefsCommon
{
	/* Receive */
	gboolean scan_all_after_inc;
	gboolean autochk_newmail;
	gint autochk_itv;
	gboolean chk_on_startup;
	gboolean enable_newmsg_notify;
	gchar *newmsg_notify_cmd;

	gboolean inc_local;
	gboolean filter_on_inc;
	gchar *spool_path;

	/* Send */
	gboolean savemsg;
	gboolean filter_sent;
	TransferEncodingMethod encoding_method;
	MIMEFilenameEncodingMethod mime_fencoding_method;

	gboolean check_attach;
	gchar *check_attach_str;
	gboolean check_recipients;
	gchar *check_recp_exclude;

	gboolean allow_jisx0201_kana;

	/* Compose */
	gboolean auto_sig;
	gchar *sig_sep;
	gint undolevels;
	gint linewrap_len;
	gboolean linewrap_quote;
	gboolean autowrap;
	gboolean auto_exteditor;
	gboolean enable_autosave;
	gint autosave_itv;
	gboolean reply_account_autosel;
	gboolean default_reply_list;
	gboolean inherit_recipient_on_self_reply;

	gboolean show_ruler;

	/* Quote */
	gboolean reply_with_quote;
	gchar *quotemark;
	gchar *quotefmt;
	gchar *fw_quotemark;
	gchar *fw_quotefmt;

	/* Spelling */
	gboolean check_spell;
	gchar *spell_lang;

	/* Display */
	gchar *textfont;

	gboolean trans_hdr;
	gboolean display_folder_unread;
	gboolean display_folder_num_columns;
	gint ng_abbrev_len;

	gboolean swap_from;
	gboolean expand_thread;
	gchar *date_format;

	gboolean enable_rules_hint;
	gboolean bold_unread;

	gboolean persist_qsearch_filter;

	ToolbarStyle toolbar_style;
	gboolean show_searchbar;
	gboolean show_statusbar;

	gchar *main_toolbar_setting;
	gchar *compose_toolbar_setting;

	LayoutType layout_type;

	/* Summary columns visibility, position and size */
	gboolean summary_col_visible[N_SUMMARY_COLS];
	gint summary_col_pos[N_SUMMARY_COLS];
	gboolean summary_sent_col_visible[N_SUMMARY_COLS];
	gint summary_sent_col_pos[N_SUMMARY_COLS];
	gint summary_col_size[N_SUMMARY_COLS];

	/* Widget visibility, position and size */
	gint folderwin_x;
	gint folderwin_y;
	gint folderview_width;
	gint folderview_height;
	gboolean folderview_visible;

	gint folder_col_folder;
	gint folder_col_new;
	gint folder_col_unread;
	gint folder_col_total;

	gint summaryview_width;
	gint summaryview_height;
	gint summaryview_vwidth;
	gint summaryview_vheight;

	gint main_msgwin_x;
	gint main_msgwin_y;
	gint msgview_width;
	gint msgview_height;
	gint msgview_vwidth;
	gint msgview_vheight;
	gboolean msgview_visible;

	gint mainview_x;
	gint mainview_y;
	gint mainview_width;
	gint mainview_height;
	gint mainwin_x;
	gint mainwin_y;
	gint mainwin_width;
	gint mainwin_height;

	gint msgwin_width;
	gint msgwin_height;

	gboolean mainwin_maximized;

	gint sourcewin_width;
	gint sourcewin_height;

	gint compose_x;
	gint compose_y;
	gint compose_width;
	gint compose_height;

	gboolean compose_maximized;

	gint addressbook_x;
	gint addressbook_y;
	gint addressbook_width;
	gint addressbook_height;

	/* Message */
	gboolean enable_color;
	gint quote_level1_col;
	gint quote_level2_col;
	gint quote_level3_col;
	gint uri_col;
	gushort sig_col;
	gboolean recycle_quote_colors;
	gboolean conv_mb_alnum;
	gboolean display_header_pane;
	gboolean display_header;
	gint line_space;
	gboolean render_html;
	gboolean html_only_as_attach;
	gboolean textview_cursor_visible;
	gboolean enable_smooth_scroll;
	gint scroll_step;
	gboolean scroll_halfpage;

	gboolean resize_image;
	gboolean inline_image;

	/* Encoding */
	gchar *force_charset;
	gchar *default_encoding;
	gchar *outgoing_charset;

	gboolean show_other_header;
	GSList *disphdr_list;

	/* MIME viewer */
	gchar *mime_image_viewer;
	gchar *mime_audio_player;
	gchar *mime_open_cmd;
	gchar *mime_cmd;

	GList *mime_open_cmd_history;

	/* Junk Mail */
	gboolean enable_junk;
	gchar *junk_learncmd;
	gchar *nojunk_learncmd;
	gchar *junk_classify_cmd;
	gchar *junk_folder;
	gboolean filter_junk_on_recv;
	gboolean filter_junk_before;
	gboolean delete_junk_on_recv;
	gboolean mark_junk_as_read;

	/* Privacy */
	gboolean auto_check_signatures;
	gboolean gpg_signature_popup;
	gboolean store_passphrase;
	gint store_passphrase_timeout;
	gboolean passphrase_grab;
	gboolean gpg_warning;

	/* Interface */
	gboolean sep_folder;
	gboolean sep_msg;
	gboolean always_show_msg;
	gboolean open_unread_on_enter;
	gboolean remember_last_selected;
	gboolean mark_as_read_on_new_window;
	gboolean open_inbox_on_inc;
	gboolean open_inbox_on_startup;
	gboolean immediate_exec;
	gboolean comply_gnome_hig;
	gboolean show_trayicon;
	gboolean minimize_to_tray;
	gboolean toggle_window_on_trayicon_click;

	/* Other */
	RecvDialogMode recv_dialog_mode;
	gboolean no_recv_err_panel;
	gboolean close_recv_dialog;

	gboolean add_address_by_click;

	gboolean confirm_on_exit;
	gboolean clean_on_exit;
	gboolean ask_on_clean;
	gboolean warn_queued_on_exit;

	gint logwin_line_limit;

	/* External commands */
	gchar *uri_cmd;
	gchar *ext_editor_cmd;

	gboolean use_print_cmd;
	gchar *print_cmd;

	gboolean use_extinc;
	gchar *extinc_cmd;
	gboolean use_extsend;
	gchar *extsend_cmd;

	/* Update check */
	gboolean auto_update_check;
	gboolean use_http_proxy;
	gchar *http_proxy_host;

	/* Advanced */
	gboolean strict_cache_check;
	gint io_timeout_secs;

	/* Filtering */
	GSList *fltlist;

	/* deprecated: do not use */
	GSList *junk_fltlist;
	GSList *manual_junk_fltlist;

	/* Actions */
	GSList *actions_list;

	/* Online / Offline */
	gboolean online_mode;

	/* Append new members here */
	gboolean folder_col_visible[4];      /* Display */
	gboolean reply_address_only;         /* Compose */
	gboolean recipients_autoreg;         /* Send */
	gboolean enable_address_completion;  /* Compose */
	gboolean fullauto_completion_mode;   /* Compose */

	gchar *user_agent_str;

	gboolean change_account_on_folder_sel; /* Interface */
	gboolean always_mark_read_on_show_msg; /* Interface */

	gboolean always_add_address_only;    /* Compose */
	gboolean show_send_dialog;           /* Send */

	gint addressbook_folder_width;
	gint addressbook_col_name;
	gint addressbook_col_addr;
	gint addressbook_col_rem;

	gchar *prev_open_dir;
	gchar *prev_save_dir;
	gchar *prev_folder_dir;

	gboolean enable_newmsg_notify_sound; /* Receive */
	gchar *newmsg_notify_sound;          /* Receive */

	gboolean show_attach_tab;            /* Message - Attachment */
	gboolean show_attached_files_first;  /* Message - Attachment */
	gint attach_toolbutton_pos;          /* Message - Attachment */

	gboolean enable_newmsg_notify_window; /* Receive */

	gboolean nofilter_junk_sender_in_book; /* Junk Mail */

	gboolean alt_prefer_html;            /* Message */

	gint save_file_type;

	gint notify_window_period;           /* Receive */

	gint startup_online_mode;            /* Online */

	gint addressbook_col_nickname;
};

extern PrefsCommon prefs_common;

PrefsCommon *prefs_common_get		(void);

PrefParam *prefs_common_get_params	(void);

void prefs_common_read_config		(void);
void prefs_common_write_config		(void);

/* deprecated */
void prefs_common_junk_filter_list_set		(void);

void prefs_common_junk_folder_rename_path	(const gchar	*old_path,
						 const gchar	*new_path);

#endif /* __PREFS_COMMON_H__ */
