/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Match Grun
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

/*
 * General functions for accessing address index file.
 */

#ifndef __ADDRINDEX_H__
#define __ADDRINDEX_H__

#include <stdio.h>
#include <glib.h>
#include "addritem.h"

#define ADDRESSBOOK_MAX_IFACE  4
#define ADDRESSBOOK_INDEX_FILE "addrbook--index.xml"
#define ADDRESSBOOK_OLD_FILE   "addressbook.xml"

#define ADDR_DS_AUTOREG "@Auto-registered"

typedef enum {
	ADDR_IF_NONE,
	ADDR_IF_BOOK,
	ADDR_IF_VCARD,
	ADDR_IF_JPILOT,
	ADDR_IF_LDAP,
	ADDR_IF_COMMON,
	ADDR_IF_PERSONAL
} AddressIfType;

typedef struct _AddressIndex AddressIndex;
struct _AddressIndex {
	AddrItemObject obj;
	gchar *filePath;
	gchar *fileName;
	gint  retVal;
	gboolean needsConversion;
	gboolean wasConverted;
	gboolean conversionError;
	AddressIfType lastType;
	gboolean dirtyFlag;
	GList *interfaceList;
};

typedef struct _AddressInterface AddressInterface;
struct _AddressInterface {
	AddrItemObject obj;
	AddressIfType type;
	gchar *name;
	gchar *listTag;
	gchar *itemTag;
	gboolean legacyFlag;
	gboolean useInterface;
	gboolean haveLibrary;
	gboolean readOnly;
	GList *listSource;
	gboolean (*getModifyFlag)( void * );
	gboolean (*getAccessFlag)( void * );
	gboolean (*getReadFlag)( void * );
	gint (*getStatusCode)( void * );
	gint (*getReadData)( void * );
	ItemFolder *(*getRootFolder)( void * );
	GList *(*getListFolder)( void * );
	GList *(*getListPerson)( void * );
	GList *(*getAllPersons)( void * );
	GList *(*getAllGroups)( void * );
	gchar *(*getName)( void * );
	void (*setAccessFlag)( void *, void * );
};

typedef struct _AddressDataSource AddressDataSource;
struct _AddressDataSource {
	AddrItemObject obj;
	AddressIfType type;
	AddressInterface *iface;
	gpointer rawDataSource;
};

AddressIndex *addrindex_create_index	();
void addrindex_set_file_path		( AddressIndex *addrIndex, const gchar *value );
void addrindex_set_file_name		( AddressIndex *addrIndex, const gchar *value );
void addrindex_set_dirty		( AddressIndex *addrIndex, const gboolean value );
GList *addrindex_get_interface_list	( AddressIndex *addrIndex );
void addrindex_free_index		( AddressIndex *addrIndex );
void addrindex_print_index		( AddressIndex *addrIndex, FILE *stream );

AddressInterface *addrindex_get_interface		( AddressIndex *addrIndex, AddressIfType ifType );
AddressDataSource *addrindex_index_add_datasource	( AddressIndex *addrIndex, AddressIfType ifType, gpointer dataSource );
AddressDataSource *addrindex_index_remove_datasource	( AddressIndex *addrIndex, AddressDataSource *dataSource );
void addrindex_free_datasource				( AddressIndex *addrIndex, AddressDataSource *ds );

gint addrindex_read_data		( AddressIndex *addrIndex );
gint addrindex_write_to			( AddressIndex *addrIndex, const gchar *newFile );
gint addrindex_save_data		( AddressIndex *addrIndex );
gint addrindex_create_new_books		( AddressIndex *addrIndex );
gint addrindex_save_all_books		( AddressIndex *addrIndex );
gint addrindex_create_extra_books	( AddressIndex *addrIndex );

gboolean addrindex_ds_get_modify_flag	( AddressDataSource *ds );
gboolean addrindex_ds_get_access_flag	( AddressDataSource *ds );
gboolean addrindex_ds_get_read_flag	( AddressDataSource *ds );
gint addrindex_ds_get_status_code	( AddressDataSource *ds );
gint addrindex_ds_read_data		( AddressDataSource *ds );
ItemFolder *addrindex_ds_get_root_folder( AddressDataSource *ds );
GList *addrindex_ds_get_list_folder	( AddressDataSource *ds );
GList *addrindex_ds_get_list_person	( AddressDataSource *ds );
gchar *addrindex_ds_get_name		( AddressDataSource *ds );
void addrindex_ds_set_access_flag	( AddressDataSource *ds, gboolean *value );
gboolean addrindex_ds_get_readonly	( AddressDataSource *ds );
GList *addrindex_ds_get_all_persons	( AddressDataSource *ds );
GList *addrindex_ds_get_all_groups	( AddressDataSource *ds );

#endif /* __ADDRINDEX_H__ */

/*
* End of Source.
*/
