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

#ifndef __COMPOSE_H__
#define __COMPOSE_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtktexttag.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktooltips.h>

typedef struct _Compose		Compose;
typedef struct _AttachInfo	AttachInfo;

#include "procmsg.h"
#include "procmime.h"
#include "folder.h"
#include "addressbook.h"
#include "prefs_account.h"
#include "undo.h"
#include "codeconv.h"

typedef enum
{
	COMPOSE_ENTRY_TO,
	COMPOSE_ENTRY_CC,
	COMPOSE_ENTRY_BCC,
	COMPOSE_ENTRY_REPLY_TO,
	COMPOSE_ENTRY_SUBJECT,
	COMPOSE_ENTRY_NEWSGROUPS,
	COMPOSE_ENTRY_FOLLOWUP_TO
} ComposeEntryType;

typedef enum
{
	COMPOSE_REPLY             = 1,
	COMPOSE_REPLY_TO_SENDER   = 2,
	COMPOSE_REPLY_TO_ALL      = 3,
	COMPOSE_REPLY_TO_LIST     = 4,
	COMPOSE_FORWARD           = 5,
	COMPOSE_FORWARD_AS_ATTACH = 6,
	COMPOSE_NEW               = 7,
	COMPOSE_REDIRECT          = 8,
	COMPOSE_REEDIT            = 9,

	COMPOSE_WITH_QUOTE        = 1 << 16,
	COMPOSE_WITHOUT_QUOTE     = 2 << 16,

	COMPOSE_MODE_MASK         = 0xffff,
	COMPOSE_QUOTE_MODE_MASK   = 0x30000
} ComposeMode;

#define COMPOSE_MODE(mode)		((mode) & COMPOSE_MODE_MASK)
#define COMPOSE_QUOTE_MODE(mode)	((mode) & COMPOSE_QUOTE_MODE_MASK)

struct _Compose
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;

	GtkWidget *toolbar;
	GtkWidget *send_btn;
	GtkWidget *sendl_btn;
	GtkWidget *draft_btn;
	GtkWidget *insert_btn;
	GtkWidget *attach_btn;
	GtkWidget *sig_btn;
	GtkWidget *exteditor_btn;
	GtkWidget *linewrap_btn;
	GtkWidget *addrbook_btn;
	GtkWidget *prefs_common_btn;
	GtkWidget *prefs_account_btn;

	GtkWidget *vbox2;

	GtkWidget *table_vbox;
	GtkWidget *table;
	GtkWidget *to_hbox;
	GtkWidget *to_entry;
	GtkWidget *newsgroups_hbox;
	GtkWidget *newsgroups_entry;
	GtkWidget *subject_entry;
	GtkWidget *cc_hbox;
	GtkWidget *cc_entry;
	GtkWidget *bcc_hbox;
	GtkWidget *bcc_entry;
	GtkWidget *reply_hbox;
	GtkWidget *reply_entry;
	GtkWidget *followup_hbox;
	GtkWidget *followup_entry;

	GtkWidget *misc_hbox;
	GtkWidget *attach_toggle;
	GtkWidget *signing_chkbtn;
	GtkWidget *encrypt_chkbtn;

	GtkWidget *paned;

	GtkWidget *attach_scrwin;
	GtkWidget *attach_treeview;
	GtkListStore *attach_store;

	GtkWidget *edit_vbox;
	GtkWidget *ruler_hbox;
	GtkWidget *ruler;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	GtkWidget *focused_editable;

	GtkWidget *popupmenu;

	GtkItemFactory *popupfactory;

	GtkWidget *tmpl_menu;

	/* GtkSpell */
	GtkWidget *spell_menu;
	gchar     *spell_lang;
	gboolean   check_spell;
	GSList    *dict_list;

	ComposeMode mode;

	MsgInfo *targetinfo;
	gchar *reply_target;
	gchar *forward_targets;

	gchar	*replyto;
	gchar	*cc;
	gchar	*bcc;
	gchar	*newsgroups;
	gchar	*followup_to;

	gchar	*ml_post;

	gchar	*inreplyto;
	gchar	*references;
	gchar	*msgid;
	gchar	*boundary;

	gboolean autowrap;

	gboolean use_to;
	gboolean use_cc;
	gboolean use_bcc;
	gboolean use_replyto;
	gboolean use_newsgroups;
	gboolean use_followupto;
	gboolean use_attach;

	CharSet out_encoding;

	gboolean use_mdn;

	/* privacy settings */
	gboolean use_signing;
	gboolean use_encryption;

	gboolean modified;

	GSList *to_list;
	GSList *newsgroup_list;

	PrefsAccount *account;

	UndoMain *undostruct;

	GtkTextTag *sig_tag;

	/* external editor */
	gchar *exteditor_file;
	GPid   exteditor_pid;
	guint  exteditor_tag;

	guint autosave_tag;

	guint lock_count;

	gboolean window_maximized;

	gboolean block_modified;

	GtkTooltips *toolbar_tip;

	GtkWidget *sig_combo;
};

struct _AttachInfo
{
	gchar *file;
	gchar *content_type;
	EncodingType encoding;
	gchar *name;
	gsize size;
};

Compose *compose_new		(PrefsAccount	*account,
				 FolderItem	*item,
				 const gchar	*mailto,
				 GPtrArray	*attach_files);

Compose *compose_reply		(MsgInfo	*msginfo,
				 FolderItem	*item,
				 ComposeMode	 mode,
				 const gchar	*body);
Compose *compose_forward	(GSList		*mlist,
				 FolderItem	*item,
				 gboolean	 as_attach,
				 const gchar	*body);
Compose *compose_redirect	(MsgInfo	*msginfo,
				 FolderItem	*item);
Compose *compose_reedit		(MsgInfo	*msginfo);

GList *compose_get_compose_list	(void);

void compose_entry_set		(Compose	  *compose,
				 const gchar	  *text,
				 ComposeEntryType  type);
void compose_entry_append	(Compose	  *compose,
				 const gchar	  *text,
				 ComposeEntryType  type);
gchar *compose_entry_get_text	(Compose	  *compose,
				 ComposeEntryType  type);

void compose_lock		(Compose	*compose);
void compose_unlock		(Compose	*compose);

void compose_block_modified	(Compose	*compose);
void compose_unblock_modified	(Compose	*compose);

void compose_reflect_prefs_all	(void);

GtkWidget *compose_get_toolbar	(Compose	*compose);
GtkWidget *compose_get_misc_hbox(Compose	*compose);
GtkWidget *compose_get_textview	(Compose	*compose);

void compose_attach_append	(Compose	*compose,
				 const gchar	*file,
				 const gchar	*filename,
				 const gchar	*content_type);
void compose_attach_remove_all	(Compose	*compose);
GSList *compose_get_attach_list	(Compose	*compose);

gint compose_send		(Compose	*compose,
				 gboolean	 close_on_success);

#endif /* __COMPOSE_H__ */
