/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright 		(C) 2001 Match Grun
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * 		(at your option) any later version.
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

/*
 * Definitions necessary to access external address book files.
 */

#ifndef __ADDRBOOK_H__
#define __ADDRBOOK_H__

#include <stdio.h>
#include <glib.h>
#include <setjmp.h>

#include "addritem.h"
#include "addrcache.h"

/* Address book file */
typedef struct _AddressBookFile AddressBookFile;

struct _AddressBookFile {
	gchar *name;
	gchar *path;
	gchar *fileName;
	AddressCache *addressCache;
	gint  retVal;
	gint  maxValue;
	GList *tempList;
	GHashTable *tempHash;
	gboolean readFlag;
	gboolean dirtyFlag;
	gboolean modifyFlag;
	gboolean accessFlag;
	jmp_buf jumper;
};

/* Function prototypes */

AddressBookFile *addrbook_create_book	( void );
void addrbook_empty_book		( AddressBookFile *book );
void addrbook_free_book			( AddressBookFile *book );
void addrbook_print_book		( AddressBookFile *book, FILE *stream );
void addrbook_dump_hash			( AddressBookFile *book, FILE *stream );
void addrbook_dump_book			( AddressBookFile *book, FILE *stream );
void addrbook_set_name			( AddressBookFile *book, const gchar *value );
void addrbook_set_path			( AddressBookFile *book, const gchar *value );
void addrbook_set_file			( AddressBookFile *book, const gchar *value );
void addrbook_set_accessed		( AddressBookFile *book, const gboolean value );
gboolean addrbook_get_modified		( AddressBookFile *book );
gboolean addrbook_get_accessed		( AddressBookFile *book );
gboolean addrbook_get_read_flag		( AddressBookFile *book );
gint addrbook_get_status		( AddressBookFile *book );
ItemFolder *addrbook_get_root_folder	( AddressBookFile *book );
GList *addrbook_get_list_folder		( AddressBookFile *book );
GList *addrbook_get_list_person		( AddressBookFile *book );
gchar *addrbook_get_name		( AddressBookFile *book );

ItemPerson *addrbook_remove_person	( AddressBookFile *book, ItemPerson *person );
ItemGroup *addrbook_remove_group	( AddressBookFile *book, ItemGroup *group );
ItemEMail *addrbook_person_remove_email	( AddressBookFile *book, ItemPerson *person,
					  ItemEMail *email );
ItemEMail *addrbook_group_remove_email	( AddressBookFile *book, ItemGroup *group,
					  ItemEMail *email );

gint addrbook_read_data			( AddressBookFile *book );
gint addrbook_save_data			( AddressBookFile *book );

ItemEMail *addrbook_move_email_before	( AddressBookFile *book, ItemPerson *person,
					  ItemEMail *itemMove, ItemEMail *itemTarget );
ItemEMail *addrbook_move_email_after	( AddressBookFile *book, ItemPerson *person,
					  ItemEMail *itemMove, ItemEMail *itemTarget );

void addrbook_update_address_list	( AddressBookFile *book, ItemPerson *person,
					  GList *listEMail );
ItemPerson *addrbook_add_address_list	( AddressBookFile *book, ItemFolder *folder,
					  GList *listEMail );
GList *addrbook_get_available_email_list( AddressBookFile *book, ItemGroup *group );
void addrbook_update_group_list		( AddressBookFile *book, ItemGroup *group,
					  GList *listEMail );
ItemGroup *addrbook_add_group_list	( AddressBookFile *book, ItemFolder *folder,
					  GList *listEMail );
ItemFolder *addrbook_add_new_folder	( AddressBookFile *book, ItemFolder *parent );

void addrbook_update_attrib_list	( AddressBookFile *book, ItemPerson *person, GList *listAttrib );
void addrbook_add_attrib_list		( AddressBookFile *book, ItemPerson *person, GList *listAttrib );

ItemFolder *addrbook_remove_folder	( AddressBookFile *book, ItemFolder *folder );
ItemFolder *addrbook_remove_folder_delete( AddressBookFile *book, ItemFolder *folder );

GList *addrbook_get_bookfile_list	( AddressBookFile *book );
gchar *addrbook_gen_new_file_name	( gint fileNum );
gint addrbook_test_read_file		( AddressBookFile *book, gchar *fileName );

GList *addrbook_get_all_persons		( AddressBookFile *book );

ItemPerson *addrbook_add_contact	( AddressBookFile *book, ItemFolder *folder,
					  const gchar *name, const gchar *address,
					  const gchar *remarks );

#endif /* __ADDRBOOK_H__ */
