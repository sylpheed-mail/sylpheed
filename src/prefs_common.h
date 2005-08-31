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

#ifndef __PREFS_COMMON_H__
#define __PREFS_COMMON_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

typedef struct _PrefsCommon	PrefsCommon;

#include "prefs.h"
#include "mainwindow.h"
#include "summaryview.h"
//#include "codeconv.h"
//#include "textview.h"

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

struct _PrefsCommon
{
	/* Receive */
	gboolean use_extinc;
	gchar *extinc_cmd;
	gboolean inc_local;
	gboolean filter_on_inc;
	gchar *spool_path;
	gboolean scan_all_after_inc;
	gboolean autochk_newmail;
	gint autochk_itv;
	gboolean chk_on_startup;
	gboolean enable_newmsg_notify;
	gchar *newmsg_notify_cmd;

	/* Send */
	gboolean use_extsend;
	gchar *extsend_cmd;
	gboolean savemsg;
	gboolean filter_sent;
	gchar *outgoing_charset;
	TransferEncodingMethod encoding_method;

	gboolean allow_jisx0201_kana;

	/* Compose */
	gboolean auto_sig;
	gchar *sig_sep;
	gint undolevels;
	gint linewrap_len;
	gboolean linewrap_quote;
	gboolean autowrap;
	gboolean linewrap_at_send;
	gboolean auto_exteditor;
	gboolean reply_account_autosel;
	gboolean default_reply_list;
	gboolean show_ruler;

	/* Quote */
	gboolean reply_with_quote;
	gchar *quotemark;
	gchar *quotefmt;
	gchar *fw_quotemark;
	gchar *fw_quotefmt;

	/* Display */
	gchar *textfont;

	gboolean trans_hdr;
	gboolean display_folder_unread;
	gint ng_abbrev_len;

	gboolean swap_from;
	gboolean expand_thread;
	gchar *date_format;

	gboolean enable_rules_hint;
	gboolean bold_unread;

	ToolbarStyle toolbar_style;
	gboolean show_statusbar;

	gint folderview_vscrollbar_policy;

	/* Summary columns visibility, position and size */
	gboolean summary_col_visible[N_SUMMARY_COLS];
	gint summary_col_pos[N_SUMMARY_COLS];
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

	gint main_msgwin_x;
	gint main_msgwin_y;
	gint msgview_width;
	gint msgview_height;
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

	gint sourcewin_width;
	gint sourcewin_height;

	gint compose_width;
	gint compose_height;

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
	gboolean textview_cursor_visible;
	gboolean enable_smooth_scroll;
	gint scroll_step;
	gboolean scroll_halfpage;

	gboolean resize_image;
	gboolean inline_image;

	gchar *force_charset;

	gboolean show_other_header;
	GSList *disphdr_list;

	/* MIME viewer */
	gchar *mime_image_viewer;
	gchar *mime_audio_player;
	gchar *mime_open_cmd;

	GList *mime_open_cmd_history;

	/* Junk Mail */
	gboolean enable_junk;
	gchar *junk_learncmd;
	gchar *nojunk_learncmd;
	gchar *junk_classify_cmd;
	gchar *junk_folder;
	gboolean filter_junk_on_recv;

#if USE_GPGME
	/* Privacy */
	gboolean auto_check_signatures;
	gboolean gpg_signature_popup;
	gboolean store_passphrase;
	gint store_passphrase_timeout;
	gboolean passphrase_grab;
	gboolean gpg_warning;
#endif /* USE_GPGME */

	/* Interface */
	gboolean sep_folder;
	gboolean sep_msg;
	gboolean always_show_msg;
	gboolean open_unread_on_enter;
	gboolean mark_as_read_on_new_window;
	gboolean open_inbox_on_inc;
	gboolean immediate_exec;
	RecvDialogMode recv_dialog_mode;
	gboolean no_recv_err_panel;
	gboolean close_recv_dialog;
	gboolean comply_gnome_hig;

	/* Other */
	gchar *uri_cmd;
	gchar *print_cmd;
	gchar *ext_editor_cmd;

	gboolean add_address_by_click;

	gboolean confirm_on_exit;
	gboolean clean_on_exit;
	gboolean ask_on_clean;
	gboolean warn_queued_on_exit;

	gint logwin_line_limit;

	/* Advanced */
	gboolean strict_cache_check;
	gint io_timeout_secs;

	/* Filtering */
	GSList *fltlist;
	GSList *junk_fltlist;

	/* Actions */
	GSList *actions_list;

	/* Online / Offline */
	gboolean online_mode;
};

extern PrefsCommon prefs_common;

PrefParam *prefs_common_get_params	(void);

void prefs_common_read_config		(void);
void prefs_common_write_config		(void);

void prefs_common_junk_filter_list_set	(void);

#endif /* __PREFS_COMMON_H__ */
