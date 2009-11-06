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

#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

#include <glib.h>

#include "prefs.h"
#include "prefs_account.h"
#include "folder.h"
#include "procmsg.h"

typedef gint	(*AccountFunc)	(PrefsAccount	*ac_prefs,
				 gpointer	 user_data);

extern PrefsAccount *cur_account;

void	      account_read_config_all	(void);
void	      account_write_config_all	(void);

PrefsAccount *account_get_current_account	(void);

PrefsAccount *account_find_from_smtp_server	(const gchar	*address,
						 const gchar	*smtp_server);
PrefsAccount *account_find_from_address		(const gchar	*address);
PrefsAccount *account_find_from_id		(gint		 id);
PrefsAccount *account_find_from_item		(FolderItem	*item);
PrefsAccount *account_find_from_item_property	(FolderItem	*item);
PrefsAccount *account_find_from_message_file	(const gchar	*file);
PrefsAccount *account_find_from_msginfo		(MsgInfo	*msginfo);

gboolean      account_address_exist	(const gchar	*address);

void	      account_foreach		(AccountFunc	 func,
					 gpointer	 user_data);
GList	     *account_get_list		(void);
void	      account_list_free		(void);
void	      account_append		(PrefsAccount	*ac_prefs);

void	      account_set_as_default	(PrefsAccount	*ac_prefs);
PrefsAccount *account_get_default	(void);

//void	      account_set_missing_folder(void);
FolderItem   *account_get_special_folder(PrefsAccount		*ac_prefs,
					 SpecialFolderItemType	 type);

void	      account_destroy		(PrefsAccount	*ac_prefs);

void	      account_update_lock	(void);
void	      account_update_unlock	(void);
void	      account_updated		(void);

#endif /* __ACCOUNT_H__ */
