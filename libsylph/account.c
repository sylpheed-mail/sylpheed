/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
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

#include "folder.h"
#include "account.h"
#include "prefs.h"
#include "prefs_account.h"
#include "procmsg.h"
#include "procheader.h"
#include "utils.h"
#include "sylmain.h"

PrefsAccount *cur_account;

static GList *account_list = NULL;

GHashTable *address_table;


void account_read_config_all(void)
{
	GSList *ac_label_list = NULL, *cur;
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	PrefsAccount *ac_prefs;

	debug_print(_("Reading all config for each account...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACCOUNT_RC, NULL);
	if ((fp = g_fopen(rcpath, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		return;
	}
	g_free(rcpath);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (!strncmp(buf, "[Account: ", 10)) {
			strretchomp(buf);
			memmove(buf, buf + 1, strlen(buf));
			buf[strlen(buf) - 1] = '\0';
			debug_print("Found label: %s\n", buf);
			ac_label_list = g_slist_append(ac_label_list,
						       g_strdup(buf));
		}
	}
	fclose(fp);

	/* read config data from file */
	cur_account = NULL;
	for (cur = ac_label_list; cur != NULL; cur = cur->next) {
		ac_prefs = prefs_account_new();
		prefs_account_read_config(ac_prefs, (gchar *)cur->data);
		account_list = g_list_append(account_list, ac_prefs);
		if (ac_prefs->is_default)
			cur_account = ac_prefs;
	}
	/* if default is not set, assume first account as default */
	if (!cur_account && account_list) {
		ac_prefs = (PrefsAccount *)account_list->data;
		account_set_as_default(ac_prefs);
		cur_account = ac_prefs;
	}

	while (ac_label_list) {
		g_free(ac_label_list->data);
		ac_label_list = g_slist_remove(ac_label_list,
					       ac_label_list->data);
	}
}

void account_write_config_all(void)
{
	prefs_account_write_config_all(account_list);
}

PrefsAccount *account_get_current_account(void)
{
	return cur_account;
}

PrefsAccount *account_find_from_smtp_server(const gchar *address,
					    const gchar *smtp_server)
{
	GList *cur;
	PrefsAccount *ac;

	g_return_val_if_fail(address != NULL, NULL);
	g_return_val_if_fail(smtp_server != NULL, NULL);

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ac = (PrefsAccount *)cur->data;
		if (!strcmp2(address, ac->address) &&
		    !strcmp2(smtp_server, ac->smtp_server))
			return ac;
	}

	return NULL;
}

/*
 * account_find_from_address:
 * @address: Email address string.
 *
 * Find a mail (not news) account with the specified email address.
 *
 * Return value: The found account, or NULL if not found.
 */
PrefsAccount *account_find_from_address(const gchar *address)
{
	GList *cur;
	PrefsAccount *ac;

	g_return_val_if_fail(address != NULL, NULL);

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ac = (PrefsAccount *)cur->data;
		if (ac->protocol != A_NNTP && ac->address &&
		    strcasestr(address, ac->address) != NULL)
			return ac;
	}

	return NULL;
}

PrefsAccount *account_find_from_id(gint id)
{
	GList *cur;
	PrefsAccount *ac;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ac = (PrefsAccount *)cur->data;
		if (id == ac->account_id)
			return ac;
	}

	return NULL;
}

PrefsAccount *account_find_from_item(FolderItem *item)
{
	PrefsAccount *ac;

	g_return_val_if_fail(item != NULL, NULL);

	ac = account_find_from_item_property(item);
	if (!ac)
		ac = item->folder->account;

	return ac;
}

PrefsAccount *account_find_from_item_property(FolderItem *item)
{
	PrefsAccount *ac;

	g_return_val_if_fail(item != NULL, NULL);

	ac = item->account;
	if (!ac) {
		FolderItem *cur_item = item->parent;
		while (cur_item != NULL) {
			if (cur_item->account && cur_item->ac_apply_sub) {
				ac = cur_item->account;
				break;
			}
			cur_item = cur_item->parent;
		}
	}

	return ac;
}

PrefsAccount *account_find_from_message_file(const gchar *file)
{
	static HeaderEntry hentry[] = {{"From:",		  NULL, FALSE},
				       {"X-Sylpheed-Account-Id:", NULL, FALSE},
				       {"AID:",			  NULL, FALSE},
				       {NULL,			  NULL, FALSE}};

	enum
	{
		H_FROM			= 0,
		H_X_SYLPHEED_ACCOUNT_ID = 1,
		H_AID			= 2
	};

	PrefsAccount *ac = NULL;
	FILE *fp;
	gchar *str;
	gchar buf[BUFFSIZE];
	gint hnum;

	g_return_val_if_fail(file != NULL, NULL);

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return NULL;
	}

	while ((hnum = procheader_get_one_field(buf, sizeof(buf), fp, hentry))
	       != -1) {
		str = buf + strlen(hentry[hnum].name);
		if (hnum == H_FROM)
			ac = account_find_from_address(str);
		else if (hnum == H_X_SYLPHEED_ACCOUNT_ID || hnum == H_AID) {
			PrefsAccount *tmp_ac;

			tmp_ac = account_find_from_id(atoi(str));
			if (tmp_ac) {
				ac = tmp_ac;
				break;
			}
		}
	}

	fclose(fp);
	return ac;
}

PrefsAccount *account_find_from_msginfo(MsgInfo *msginfo)
{
	gchar *file;
	PrefsAccount *ac;

	file = procmsg_get_message_file(msginfo);
	ac = account_find_from_message_file(file);
	g_free(file);

	if (!ac && msginfo->folder)
		ac = account_find_from_item(msginfo->folder);

	return ac;
}

gboolean account_address_exist(const gchar *address)
{
	if (!address_table) {
		GList *cur;

		address_table = g_hash_table_new(g_str_hash, g_str_equal);
		for (cur = account_list; cur != NULL; cur = cur->next) {
			PrefsAccount *ac = (PrefsAccount *)cur->data;

			if (ac->address)
				g_hash_table_insert(address_table, ac->address,
						    GINT_TO_POINTER(1));
		}
	}

	return (gboolean)g_hash_table_lookup(address_table, address);
}

void account_foreach(AccountFunc func, gpointer user_data)
{
	GList *cur;

	for (cur = account_list; cur != NULL; cur = cur->next)
		if (func((PrefsAccount *)cur->data, user_data) != 0)
			return;
}

GList *account_get_list(void)
{
	return account_list;
}

void account_list_free(void)
{
	g_list_free(account_list);
	account_list = NULL;
}

void account_append(PrefsAccount *ac_prefs)
{
	account_list = g_list_append(account_list, ac_prefs);
	account_updated();
}

void account_set_as_default(PrefsAccount *ac_prefs)
{
	PrefsAccount *ap;
	GList *cur;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ap = (PrefsAccount *)cur->data;
		if (ap->is_default)
			ap->is_default = FALSE;
	}

	ac_prefs->is_default = TRUE;
}

PrefsAccount *account_get_default(void)
{
	PrefsAccount *ap;
	GList *cur;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ap = (PrefsAccount *)cur->data;
		if (ap->is_default)
			return ap;
	}

	return NULL;
}

#if 0
void account_set_missing_folder(void)
{
	PrefsAccount *ap;
	GList *cur;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ap = (PrefsAccount *)cur->data;
		if ((ap->protocol == A_IMAP4 || ap->protocol == A_NNTP) &&
		    !ap->folder) {
			Folder *folder;

			if (ap->protocol == A_IMAP4) {
				folder = folder_new(F_IMAP, ap->account_name,
						    ap->recv_server);
			} else {
				folder = folder_new(F_NEWS, ap->account_name,
						    ap->nntp_server);
			}

			folder->account = ap;
			ap->folder = REMOTE_FOLDER(folder);
			folder_add(folder);
			if (ap->protocol == A_IMAP4) {
				if (main_window_toggle_online_if_offline
					(main_window_get())) {
					folder->klass->create_tree(folder);
					statusbar_pop_all();
				}
			}
		}
	}
}
#endif

FolderItem *account_get_special_folder(PrefsAccount *ac_prefs,
				       SpecialFolderItemType type)
{
	FolderItem *item = NULL;

	g_return_val_if_fail(ac_prefs != NULL, NULL);

	switch (type) {
	case F_INBOX:
		if (ac_prefs->folder)
			item = FOLDER(ac_prefs->folder)->inbox;
		if (!item)
			item = folder_get_default_inbox();
		break;
	case F_OUTBOX:
		if (ac_prefs->set_sent_folder && ac_prefs->sent_folder) {
			item = folder_find_item_from_identifier
				(ac_prefs->sent_folder);
		}
		if (!item) {
			if (ac_prefs->folder)
				item = FOLDER(ac_prefs->folder)->outbox;
			if (!item)
				item = folder_get_default_outbox();
		}
		break;
	case F_DRAFT:
		if (ac_prefs->set_draft_folder && ac_prefs->draft_folder) {
			item = folder_find_item_from_identifier
				(ac_prefs->draft_folder);
		}
		if (!item) {
			if (ac_prefs->folder)
				item = FOLDER(ac_prefs->folder)->draft;
			if (!item)
				item = folder_get_default_draft();
		}
		break;
	case F_QUEUE:
		if (ac_prefs->set_queue_folder && ac_prefs->queue_folder) {
			item = folder_find_item_from_identifier
				(ac_prefs->queue_folder);
			/* only allow queue-type folder */
			if (item && item->stype != F_QUEUE)
				item = NULL;
		}
		if (!item) {
			if (ac_prefs->folder)
				item = FOLDER(ac_prefs->folder)->queue;
			if (!item)
				item = folder_get_default_queue();
		}
		break;
	case F_TRASH:
		if (ac_prefs->set_trash_folder && ac_prefs->trash_folder) {
			item = folder_find_item_from_identifier
				(ac_prefs->trash_folder);
		}
		if (!item) {
			if (ac_prefs->folder)
				item = FOLDER(ac_prefs->folder)->trash;
			if (!item)
				item = folder_get_default_trash();
		}
		break;
	default:
		break;
	}

	return item;
}

void account_destroy(PrefsAccount *ac_prefs)
{
	g_return_if_fail(ac_prefs != NULL);

	folder_unref_account_all(ac_prefs);

	account_list = g_list_remove(account_list, ac_prefs);
	if (cur_account == ac_prefs)
		cur_account = NULL;
	prefs_account_free(ac_prefs);

	if (!cur_account && account_list) {
		cur_account = account_get_default();
		if (!cur_account) {
			ac_prefs = (PrefsAccount *)account_list->data;
			account_set_as_default(ac_prefs);
			cur_account = ac_prefs;
		}
	}

	account_updated();
}

static guint account_update_lock_count = 0;

void account_update_lock(void)
{
	account_update_lock_count++;
}

void account_update_unlock(void)
{
	if (account_update_lock_count > 0)
		account_update_lock_count--;
}

void account_updated(void)
{
	if (account_update_lock_count)
		return;

	if (address_table) {
		g_hash_table_destroy(address_table);
		address_table = NULL;
	}

	if (syl_app_get())
		g_signal_emit_by_name(syl_app_get(), "account-updated");
}
