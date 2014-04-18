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
 * General functions for accessing external address book files.
 */

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <setjmp.h>

#include "xml.h"
#include "mgutils.h"
#include "addritem.h"
#include "addrcache.h"
#include "addrbook.h"
#include "utils.h"

#ifndef DEV_STANDALONE
#include "prefs.h"
#include "codeconv.h"
#endif

#define ADDRBOOK_MAX_SEARCH_COUNT 1000
#define ADDRBOOK_PREFIX           "addrbook-"
#define ADDRBOOK_SUFFIX           ".xml"
#define FILE_NUMDIGITS            6

#define ID_TIME_OFFSET            998000000
/*
* Create new address book.
*/
AddressBookFile *addrbook_create_book() {
	AddressBookFile *book;

	book = g_new0( AddressBookFile, 1 );
	book->name = NULL;
	book->path = NULL;
	book->fileName = NULL;
	book->retVal = MGU_SUCCESS;
	book->addressCache = addrcache_create();

	book->tempList = NULL;
	book->readFlag = FALSE;
	book->dirtyFlag = FALSE;
	book->modifyFlag = TRUE;
	book->accessFlag = FALSE;
	book->tempHash = NULL;
	return book;
}

/*
* Specify name to be used.
*/
void addrbook_set_name( AddressBookFile *book, const gchar *value ) {
	g_return_if_fail( book != NULL );
	book->name = mgu_replace_string( book->name, value );
}
void addrbook_set_path( AddressBookFile *book, const gchar *value ) {
	g_return_if_fail( book != NULL );
	book->path = mgu_replace_string( book->path, value );
	book->dirtyFlag = TRUE;
}
void addrbook_set_file( AddressBookFile *book, const gchar *value ) {
	g_return_if_fail( book != NULL );
	book->fileName = mgu_replace_string( book->fileName, value );
	book->dirtyFlag = TRUE;
}
void addrbook_set_accessed( AddressBookFile *book, const gboolean value ) {
	g_return_if_fail( book != NULL );
	book->accessFlag = value;
}
gboolean addrbook_get_modified( AddressBookFile *book ) {
	g_return_val_if_fail( book != NULL, FALSE );
	return book->modifyFlag;
}
gboolean addrbook_get_accessed( AddressBookFile *book ) {
	g_return_val_if_fail( book != NULL, FALSE );
	return book->accessFlag;
}
gboolean addrbook_get_read_flag( AddressBookFile *book ) {
	g_return_val_if_fail( book != NULL, FALSE );
	return book->readFlag;
}
gint addrbook_get_status( AddressBookFile *book ) {
	g_return_val_if_fail( book != NULL, -1 );
	return book->retVal;
}
ItemFolder *addrbook_get_root_folder( AddressBookFile *book ) {
	g_return_val_if_fail( book != NULL, NULL );
	return addrcache_get_root_folder( book->addressCache );
}
GList *addrbook_get_list_folder( AddressBookFile *book ) {
	g_return_val_if_fail( book != NULL, NULL );
	return addrcache_get_list_folder( book->addressCache );
}
GList *addrbook_get_list_person( AddressBookFile *book ) {
	g_return_val_if_fail( book != NULL, NULL );
	return addrcache_get_list_person( book->addressCache );
}
gchar *addrbook_get_name( AddressBookFile *book ) {
	g_return_val_if_fail( book != NULL, NULL );
	return book->name;
}

/*
* Empty address book.
*/
void addrbook_empty_book( AddressBookFile *book ) {
	g_return_if_fail( book != NULL );

	/* Free up folders and hash table */
	addrcache_clear( book->addressCache );

	g_list_free( book->tempList );
	book->tempList = NULL;

	/* Reset to initial state */
	book->retVal = MGU_SUCCESS;
	book->tempHash = NULL;
	book->readFlag = FALSE;
	book->dirtyFlag = FALSE;
	book->modifyFlag = FALSE;
	book->accessFlag = FALSE;
}

/*
* Free address book.
*/
void addrbook_free_book( AddressBookFile *book ) {
	g_return_if_fail( book != NULL );

	g_free( book->name );
	g_free( book->path );
	g_free( book->fileName );
	book->name = NULL;
	book->path = NULL;
	book->fileName = NULL;

	/* Free up folders and hash table */
	addrcache_free( book->addressCache );
	book->addressCache = NULL;

	g_list_free( book->tempList );
	book->tempList = NULL;

	book->retVal = MGU_SUCCESS;
	book->tempHash = NULL;
	book->readFlag = FALSE;
	book->dirtyFlag = FALSE;
	book->modifyFlag = FALSE;
	book->accessFlag = FALSE;

	g_free( book );
}

/*
* Print list of items.
*/
void addrbook_print_item_list( GList *list, FILE *stream ) {
	GList *node = list;

	while( node ) {
		AddrItemObject *obj = node->data;
		if( ADDRITEM_TYPE(obj) == ITEMTYPE_PERSON ) {
			addritem_print_item_person( ( ItemPerson * ) obj, stream );
		}
		else if( ADDRITEM_TYPE(obj) == ITEMTYPE_GROUP ) {
			addritem_print_item_group( ( ItemGroup * ) obj, stream );
		}
		else if( ADDRITEM_TYPE(obj) == ITEMTYPE_FOLDER ) {
			addritem_print_item_folder( ( ItemFolder * ) obj, stream );
		}
		node = g_list_next( node );
	}
	fprintf( stream, "\t---\n" );
}

/*
* Print address book.
*/
void addrbook_print_book( AddressBookFile *book, FILE *stream ) {
	g_return_if_fail( book != NULL );

	fprintf( stream, "AddressBook:\n" );
	fprintf( stream, "\tname  : '%s'\n", book->name );
	fprintf( stream, "\tpath  : '%s'\n", book->path );
	fprintf( stream, "\tfile  : '%s'\n", book->fileName );
	fprintf( stream, "\tstatus: %d : '%s'\n", book->retVal, mgu_error2string( book->retVal ) );
	addrcache_print( book->addressCache, stream );
}

/*
* Dump entire address book traversing folders.
*/
void addrbook_dump_book( AddressBookFile *book, FILE *stream ) {
	ItemFolder *folder;

	g_return_if_fail( book != NULL );

	addrbook_print_book( book, stream );
	folder = book->addressCache->rootFolder;
	addritem_print_item_folder( folder, stream );
}

/*
* Remove group from address book.
* param: group	Group to remove.
* return: Group, or NULL if not found. Note that object should still be freed.
*/
ItemGroup *addrbook_remove_group( AddressBookFile *book, ItemGroup *group ) {
	ItemGroup *item;

	g_return_val_if_fail( book != NULL, NULL );

	item = addrcache_remove_group( book->addressCache, group );
	if( item ) book->dirtyFlag = TRUE;
	return item;
}

/*
* Remove specified person from address book.
* param: person	Person to remove.
* return: Person, or NULL if not found. Note that object should still be freed.
*/
ItemPerson *addrbook_remove_person( AddressBookFile *book, ItemPerson *person ) {
	ItemPerson *item;

	g_return_val_if_fail( book != NULL, NULL );

	item = addrcache_remove_person( book->addressCache, person );
	if( item ) book->dirtyFlag = TRUE;
	return item;
}

/*
* Remove email address in address book for specified person.
* param: person	Person.
*        email	EMail to remove.
* return: EMail object, or NULL if not found. Note that object should still be freed.
*/
ItemEMail *addrbook_person_remove_email( AddressBookFile *book, ItemPerson *person, ItemEMail *email ) {
	ItemEMail *item;

	g_return_val_if_fail( book != NULL, NULL );

	item = addrcache_person_remove_email( book->addressCache, person, email );
	if( item ) book->dirtyFlag = TRUE;
	return item;
}

ItemEMail *addrbook_group_remove_email( AddressBookFile *book, ItemGroup *group, ItemEMail *email ) {
	ItemEMail *item;

	g_return_val_if_fail( book != NULL, NULL );

	item = addritem_group_remove_email( group, email );
	if( item ) book->dirtyFlag = TRUE;
	return item;
}

/* **********************************************************************
* Read/Write XML data file...
* ===========================
* Notes:
* 1)	The address book is structured as follows:
*
*		address-book
*			person
*				address-list
*					address
*				attribute-list
*					attribute
*			group
*				member-list
*					member
*			folder
*				item-list
*					item
*
* 2)	This sequence of elements was chosen so that the most important
* 	elements (person and their email addresses) appear first.
*
* 3)	Groups then appear. When groups are loaded, person's email
*	addresses have already been loaded and can be found.
*
* 4)	Finally folders are loaded. Any forward and backward references
*	to folders, groups and persons in the folders are resolved after
*	loading.
*
* ***********************************************************************
*/

/* Element tag names */
#define AB_ELTAG_ADDRESS         "address"
#define AB_ELTAG_ATTRIBUTE       "attribute"
#define AB_ELTAG_ATTRIBUTE_LIST  "attribute-list"
#define AB_ELTAG_ADDRESS_LIST    "address-list"
#define AB_ELTAG_MEMBER          "member"
#define AB_ELTAG_MEMBER_LIST     "member-list"
#define AB_ELTAG_ITEM            "item"
#define AB_ELTAG_ITEM_LIST       "item-list"
#define AB_ELTAG_ADDRESS_BOOK    "address-book"
#define AB_ELTAG_PERSON          "person"
#define AB_ELTAG_GROUP           "group"
#define AB_ELTAG_FOLDER          "folder"

/* Attribute tag names */
#define AB_ATTAG_TYPE            "type"
#define AB_ATTAG_UID             "uid"
#define AB_ATTAG_NAME            "name"
#define AB_ATTAG_REMARKS         "remarks"
#define AB_ATTAG_FIRST_NAME      "first-name"
#define AB_ATTAG_LAST_NAME       "last-name"
#define AB_ATTAG_NICK_NAME       "nick-name"
#define AB_ATTAG_COMMON_NAME     "cn"
#define AB_ATTAG_ALIAS           "alias"
#define AB_ATTAG_EMAIL           "email"
#define AB_ATTAG_EID             "eid"
#define AB_ATTAG_PID             "pid"

/* Attribute values */
#define AB_ATTAG_VAL_PERSON      "person"
#define AB_ATTAG_VAL_GROUP       "group"
#define AB_ATTAG_VAL_FOLDER      "folder"

/*
* Parse address item for person.
*/
static void addrbook_parse_address( AddressBookFile *book, XMLFile *file, ItemPerson *person ) {
	GList *attr;
	gchar *name, *value;
	ItemEMail *email = NULL;

	attr = xml_get_current_tag_attr(file);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		if( ! email ) email = addritem_create_item_email();
		if( strcmp( name, AB_ATTAG_UID ) == 0 ) {
			ADDRITEM_ID(email) = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_ALIAS ) == 0 ) {
			ADDRITEM_NAME(email) = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_EMAIL ) == 0 ) {
			email->address = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_REMARKS ) == 0 ) {
			email->remarks = g_strdup( value );
		}
		attr = g_list_next( attr );
	}
	if( email ) {
		if( person ) {
			addrcache_person_add_email( book->addressCache, person, email );
		}
		else {
			addritem_free_item_email( email );
			email = NULL;
		}
	}
}

/*
* Parse email address list.
*/
static void addrbook_parse_addr_list( AddressBookFile *book, XMLFile *file, ItemPerson *person ){
	GList *attr;
	guint prev_level;

	for (;;) {
		prev_level = file->level;
		if( xml_parse_next_tag( file ) ) {
			longjmp( book->jumper, 1 );
		}
		if (file->level < prev_level) return;
		if( xml_compare_tag( file, AB_ELTAG_ADDRESS ) ) {
			attr = xml_get_current_tag_attr(file);
			addrbook_parse_address( book, file, person );
			addrbook_parse_addr_list( book, file, person );
		}
	}
}

/*
* Parse attibute for person.
*/
static void addrbook_parse_attribute( XMLFile *file, ItemPerson *person ) {
	GList *attr;
	gchar *name, *value;
	gchar *element;
	UserAttribute *uAttr = NULL;

	attr = xml_get_current_tag_attr(file);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		if( ! uAttr ) uAttr = addritem_create_attribute();
		if( strcmp( name, AB_ATTAG_UID ) == 0 ) {
			addritem_attrib_set_id( uAttr, value );
		}
		else if( strcmp( name, AB_ATTAG_NAME ) == 0 ) {
			addritem_attrib_set_name( uAttr, value );
		}
		attr = g_list_next( attr );
	}

	element = xml_get_element( file );
	addritem_attrib_set_value( uAttr, element );

	if( uAttr ) {
		if( person ) {
			addritem_person_add_attribute( person, uAttr );
		}
		else {
			addritem_free_attribute( uAttr );
			uAttr = NULL;
		}
	}
}

/*
* Parse attribute list.
*/
static void addrbook_parse_attr_list( AddressBookFile *book, XMLFile *file, ItemPerson *person ){
	GList *attr;
	guint prev_level;

	for (;;) {
		prev_level = file->level;
		if( xml_parse_next_tag( file ) ) {
			longjmp( book->jumper, 1 );
		}
		if (file->level < prev_level) return;
		if( xml_compare_tag( file, AB_ELTAG_ATTRIBUTE ) ) {
			attr = xml_get_current_tag_attr(file);
			addrbook_parse_attribute( file, person );
			addrbook_parse_attr_list( book, file, person );
		}
	}
}

/*
* Parse person.
*/
static void addrbook_parse_person( AddressBookFile *book, XMLFile *file ) {
	GList *attr;
	gchar *name, *value;
	ItemPerson *person = NULL;

	attr = xml_get_current_tag_attr(file);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		if( ! person ) person = addritem_create_item_person();
		if( strcmp( name, AB_ATTAG_UID ) == 0 ) {
			ADDRITEM_ID(person) = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_FIRST_NAME ) == 0 ) {
			person->firstName = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_LAST_NAME ) == 0 ) {
			person->lastName = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_NICK_NAME ) == 0 ) {
			person->nickName = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_COMMON_NAME ) == 0 ) {
			ADDRITEM_NAME(person) = g_strdup( value );
		}
		attr = g_list_next( attr );
	}
	if( xml_parse_next_tag( file ) ) {	/* Consume closing tag */
		longjmp( book->jumper, 1 );
	}
	if( xml_compare_tag( file, AB_ELTAG_ADDRESS_LIST ) ) {
		addrbook_parse_addr_list( book, file, person );
		if( person ) {
			addrcache_hash_add_person( book->addressCache, person );
		}
	}
	if( xml_parse_next_tag( file ) ) {	/* Consume closing tag */
		longjmp( book->jumper, 1 );
	}
	if( xml_compare_tag( file, AB_ELTAG_ATTRIBUTE_LIST ) ) {
		addrbook_parse_attr_list( book, file, person );
	}
}

/*
* Parse group member.
*/
static void addrbook_parse_member( AddressBookFile *book, XMLFile *file, ItemGroup *group ) {
	GList *attr;
	gchar *name, *value;
	gchar *pid = NULL, *eid = NULL;
	ItemEMail *email = NULL;

	attr = xml_get_current_tag_attr(file);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, AB_ATTAG_PID ) == 0 ) {
			pid = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_EID ) == 0 ) {
			eid = g_strdup( value );
		}
		attr = g_list_next( attr );
	}
	email = addrcache_get_email( book->addressCache, pid, eid );
	if( email ) {
		if( group ) {
			addrcache_group_add_email( book->addressCache, group, email );
		}
		else {
			addritem_free_item_email( email );
			email = NULL;
		}
	}
}

/*
* Parse group member list.
*/
static void addrbook_parse_member_list( AddressBookFile *book, XMLFile *file, ItemGroup *group ){
	GList *attr;
	guint prev_level;

	for (;;) {
		prev_level = file->level;
		if( xml_parse_next_tag( file ) ) {
			longjmp( book->jumper, 1 );
		}
		if (file->level < prev_level) return;
		if( xml_compare_tag( file, AB_ELTAG_MEMBER ) ) {
			attr = xml_get_current_tag_attr(file);
			addrbook_parse_member( book, file, group );
			addrbook_parse_member_list( book, file, group );
		}
		else {
			attr = xml_get_current_tag_attr( file );
		}
	}
}

/*
* Parse group.
*/
static void addrbook_parse_group( AddressBookFile *book, XMLFile *file ) {
	GList *attr;
	gchar *name, *value;
	ItemGroup *group = NULL;

	attr = xml_get_current_tag_attr(file);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		if( ! group ) group = addritem_create_item_group();
		if( strcmp( name, AB_ATTAG_UID ) == 0 ) {
			ADDRITEM_ID(group) = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_NAME ) == 0 ) {
			ADDRITEM_NAME(group) = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_REMARKS ) == 0 ) {
			group->remarks = g_strdup( value );
		}
		attr = g_list_next( attr );
	}
	if( xml_parse_next_tag( file ) ) {	/* Consume closing tag */
		longjmp( book->jumper, 1 );
	}
	if( xml_compare_tag( file, AB_ELTAG_MEMBER_LIST ) ) {
		if( group ) {
			addrcache_hash_add_group( book->addressCache, group );
		}
		addrbook_parse_member_list( book, file, group );
	}
}

/*
* Parse folder item.
*/
static void addrbook_parse_folder_item( AddressBookFile *book, XMLFile *file, ItemFolder *folder ) {
	GList *attr;
	gchar *name, *value;
	gchar *uid = NULL;

	attr = xml_get_current_tag_attr(file);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, AB_ATTAG_UID ) == 0 ) {
			uid = g_strdup( value );
		}
		attr = g_list_next( attr );
	}
	if( folder ) {
		if( uid ) {
			folder->listItems = g_list_append( folder->listItems, uid );
		}
	}
}

/*
* Parse folder item list.
*/
static void addrbook_parse_folder_list( AddressBookFile *book, XMLFile *file, ItemFolder *folder ){
	GList *attr;
	guint prev_level;

	for (;;) {
		prev_level = file->level;
		if( xml_parse_next_tag( file ) ) {
			longjmp( book->jumper, 1 );
		}
		if (file->level < prev_level) return;
		if( xml_compare_tag( file, AB_ELTAG_ITEM ) ) {
			attr = xml_get_current_tag_attr(file);
			addrbook_parse_folder_item( book, file, folder );
			addrbook_parse_folder_list( book, file, folder );
		}
		else {
			attr = xml_get_current_tag_attr( file );
		}
	}
}

/*
* Parse folder.
*/
static void addrbook_parse_folder( AddressBookFile *book, XMLFile *file ) {
	GList *attr;
	gchar *name, *value;
	ItemFolder *folder = NULL;

	attr = xml_get_current_tag_attr(file);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		if( ! folder ) {
			folder = addritem_create_item_folder();
		}
		if( strcmp( name, AB_ATTAG_UID ) == 0 ) {
			ADDRITEM_ID(folder) = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_NAME ) == 0 ) {
			ADDRITEM_NAME(folder) = g_strdup( value );
		}
		else if( strcmp( name, AB_ATTAG_REMARKS ) == 0 ) {
			folder->remarks = g_strdup( value );
		}
		attr = g_list_next( attr );
	}
	if( xml_parse_next_tag( file ) ) {	/* Consume closing tag */
		longjmp( book->jumper, 1 );
	}
	if( xml_compare_tag( file, AB_ELTAG_ITEM_LIST ) ) {
		if( folder ) {
			if( addrcache_hash_add_folder( book->addressCache, folder ) ) {
				book->tempList = g_list_append( book->tempList, folder );
				ADDRITEM_PARENT(folder) = NULL;	/* We will resolve folder later */
			}
		}
		addrbook_parse_folder_list( book, file, folder );
	}
}

/*
* Parse address book.
* Return: TRUE if data read successfully, FALSE if error reading data.
*/
static gboolean addrbook_read_tree( AddressBookFile *book, XMLFile *file ) {
	gboolean retVal;
	GList *attr;
	gchar *name, *value;

	book->retVal = MGU_BAD_FORMAT;
	if( xml_get_dtd( file ) ) {
		return FALSE;
	}
	if( xml_parse_next_tag( file ) ) {
		longjmp( book->jumper, 1 );
	}
	if( ! xml_compare_tag( file, AB_ELTAG_ADDRESS_BOOK ) ) {
		return FALSE;
	}

	attr = xml_get_current_tag_attr(file);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, AB_ATTAG_NAME ) == 0 ) {
			addrbook_set_name( book, value );
		}
		attr = g_list_next( attr );
	}

	retVal = TRUE;
	for (;;) {
		if (! file->level ) break;
		/* Get next item tag (person, group or folder) */
		if( xml_parse_next_tag( file ) ) {
			longjmp( book->jumper, 1 );
		}
		if( xml_compare_tag( file, AB_ELTAG_PERSON ) ) {
			addrbook_parse_person( book, file );
		}
		else if( xml_compare_tag( file, AB_ELTAG_GROUP ) ) {
			addrbook_parse_group( book, file );
		}
		else if( xml_compare_tag( file, AB_ELTAG_FOLDER ) ) {
			addrbook_parse_folder( book, file );
		}
	}
	if( retVal ) book->retVal = MGU_SUCCESS;
	return retVal;
}

/*
* Resolve folder items visitor function.
*/
static void addrbook_res_items_vis( gpointer key, gpointer value, gpointer data ) {
	AddressBookFile *book = data;
	AddrItemObject *obj = ( AddrItemObject * ) value;
	ItemFolder *rootFolder = book->addressCache->rootFolder;
	if( obj->parent == NULL ) {
		if( ADDRITEM_TYPE(obj) == ITEMTYPE_PERSON ) {
			rootFolder->listPerson = g_list_append( rootFolder->listPerson, obj );
			ADDRITEM_PARENT(obj) = ADDRITEM_OBJECT(rootFolder);
		}
		else if( ADDRITEM_TYPE(obj) == ITEMTYPE_GROUP ) {
			rootFolder->listGroup = g_list_append( rootFolder->listGroup, obj );
			ADDRITEM_PARENT(obj) = ADDRITEM_OBJECT(rootFolder);
		}
	}
}

/*
* Resolve folder items. Lists of UID's are replaced with pointers to data items.
*/
static void addrbook_resolve_folder_items( AddressBookFile *book ) {
	GList *nodeFolder = NULL;
	GList *listRemove = NULL;
	GList *node = NULL;
	ItemFolder *rootFolder = book->addressCache->rootFolder;
	nodeFolder = book->tempList;
	while( nodeFolder ) {
		ItemFolder *folder = nodeFolder->data;
		listRemove = NULL;
		node = folder->listItems;
		while( node ) {
			gchar *uid = node->data;
			AddrItemObject *aio = addrcache_get_object( book->addressCache, uid );
			if( aio ) {
				if( aio->type == ITEMTYPE_FOLDER ) {
					ItemFolder *item = ( ItemFolder * ) aio;
					folder->listFolder = g_list_append( folder->listFolder, item );
					ADDRITEM_PARENT(item) = ADDRITEM_OBJECT(folder);
					addrcache_hash_add_folder( book->addressCache, folder );
				}
				else if( aio->type == ITEMTYPE_PERSON ) {
					ItemPerson *item = ( ItemPerson * ) aio;
					folder->listPerson = g_list_append( folder->listPerson, item );
					ADDRITEM_PARENT(item) = ADDRITEM_OBJECT(folder);
				}
				else if( aio->type == ITEMTYPE_GROUP ) {
					ItemGroup *item = ( ItemGroup * ) aio;
					folder->listGroup = g_list_append( folder->listGroup, item );
					ADDRITEM_PARENT(item) = ADDRITEM_OBJECT(folder);
				}
				/* Replace data with pointer to item */
				g_free( uid );
				node->data = aio;
			}
			else {
				/* Not found, append to remove list. */
				listRemove = g_list_append( listRemove, uid );
			}
			node = g_list_next( node );
		}
		rootFolder->listFolder = g_list_append( rootFolder->listFolder, folder );

		/* Process remove list */
		node = listRemove;
		while( node ) {
			gchar *uid = node->data;
			folder->listItems = g_list_remove( folder->listItems, uid );
			g_free( uid );
			node = g_list_next( node );
		}
		g_list_free( listRemove );
		nodeFolder = g_list_next( nodeFolder );
	}

	/* Remove folders with parents. */
	listRemove = NULL;
	node = rootFolder->listFolder;
	while( node ) {
		ItemFolder *folder = ( ItemFolder * ) node->data;
		if( ADDRITEM_PARENT(folder) ) {
			/* Remove folders with parents */
			listRemove = g_list_append( listRemove, folder );
		}
		else {
			/* Add to root folder */
			ADDRITEM_PARENT(folder) = ADDRITEM_OBJECT(book->addressCache->rootFolder);
		}
		node = g_list_next( node );
	}

	/* Process remove list */
	node = listRemove;
	while( node ) {
		rootFolder->listFolder = g_list_remove( rootFolder->listFolder, node->data );
		node = g_list_next( node );
	}
	g_list_free( listRemove );

	/* Move all unparented persons and groups into root folder */
	g_hash_table_foreach( book->addressCache->itemHash, addrbook_res_items_vis, book );

	/* Free up some more */
	nodeFolder = book->tempList;
	while( nodeFolder ) {
		ItemFolder *folder = nodeFolder->data;
		g_list_free( folder->listItems );
		folder->listItems = NULL;
		nodeFolder = g_list_next( nodeFolder );
	}
	g_list_free( book->tempList );
	book->tempList = NULL;

}

/*
* Read address book file.
*/
gint addrbook_read_data( AddressBookFile *book ) {
	XMLFile *file = NULL;
	gchar *fileSpec = NULL;

	g_return_val_if_fail( book != NULL, -1 );

	fileSpec = g_strconcat( book->path, G_DIR_SEPARATOR_S, book->fileName, NULL );
	book->retVal = MGU_OPEN_FILE;
	book->accessFlag = FALSE;
	book->modifyFlag = FALSE;
	file = xml_open_file( fileSpec );
	g_free( fileSpec );
	if( file ) {
		book->tempList = NULL;

		/* Trap for parsing errors. */
		if( setjmp( book->jumper ) ) {
			xml_close_file( file );
			return book->retVal;
		}
		addrbook_read_tree( book, file );
		xml_close_file( file );

		/* Resolve folder items */
		addrbook_resolve_folder_items( book );
		book->tempList = NULL;
		book->readFlag = TRUE;
		book->dirtyFlag = FALSE;
	}
	return book->retVal;
}

static void addrbook_write_elem_s( FILE *fp, gint lvl, gchar *name ) {
	gint i;
	for( i = 0; i < lvl; i++ ) fputs( "  ", fp );
	fputs( "<", fp );
	fputs( name, fp );
}

static void addrbook_write_elem_e( FILE *fp, gint lvl, gchar *name ) {
	gint i;
	for( i = 0; i < lvl; i++ ) fputs( "  ", fp );
	fputs( "</", fp );
	fputs( name, fp );
	fputs( ">\n", fp );
}

static void addrbook_write_attr( FILE *fp, gchar *name, gchar *value ) {
	fputs( " ", fp );
	fputs( name, fp );
	fputs( "=\"", fp );
	xml_file_put_escape_str( fp, value );
	fputs( "\"", fp );
}

/*
* Write file hash table visitor function.
*/
static void addrbook_write_item_person_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;
	FILE *fp = ( FILE * ) data;
	GList *node;

	if( ! obj ) return;
	if( ADDRITEM_TYPE(obj) == ITEMTYPE_PERSON ) {
		ItemPerson *person = ( ItemPerson * ) value;
		if( person ) {
			addrbook_write_elem_s( fp, 1, AB_ELTAG_PERSON );
			addrbook_write_attr( fp, AB_ATTAG_UID, ADDRITEM_ID(person) );
			addrbook_write_attr( fp, AB_ATTAG_FIRST_NAME, person->firstName );
			addrbook_write_attr( fp, AB_ATTAG_LAST_NAME, person->lastName );
			addrbook_write_attr( fp, AB_ATTAG_NICK_NAME, person->nickName );
			addrbook_write_attr( fp, AB_ATTAG_COMMON_NAME, ADDRITEM_NAME(person) );
			fputs( " >\n", fp);

			/* Output email addresses */
			addrbook_write_elem_s( fp, 2, AB_ELTAG_ADDRESS_LIST );
			fputs( ">\n", fp );
			node = person->listEMail;
			while ( node ) {
				ItemEMail *email = node->data;
				addrbook_write_elem_s( fp, 3, AB_ELTAG_ADDRESS );
				addrbook_write_attr( fp, AB_ATTAG_UID, ADDRITEM_ID(email) );
				addrbook_write_attr( fp, AB_ATTAG_ALIAS, ADDRITEM_NAME(email) );
				addrbook_write_attr( fp, AB_ATTAG_EMAIL, email->address );
				addrbook_write_attr( fp, AB_ATTAG_REMARKS, email->remarks );
				fputs( " />\n", fp);
				node = g_list_next( node );
			}
			addrbook_write_elem_e( fp, 2, AB_ELTAG_ADDRESS_LIST );

			/* Output user attributes */
			addrbook_write_elem_s( fp, 2, AB_ELTAG_ATTRIBUTE_LIST );
			fputs( ">\n", fp );
			node = person->listAttrib;
			while ( node ) {
				UserAttribute *attrib = node->data;
				addrbook_write_elem_s( fp, 3, AB_ELTAG_ATTRIBUTE );
				addrbook_write_attr( fp, AB_ATTAG_UID, attrib->uid );
				addrbook_write_attr( fp, AB_ATTAG_NAME, attrib->name );
				fputs( " >", fp);
				xml_file_put_escape_str( fp, attrib->value );
				addrbook_write_elem_e( fp, 0, AB_ELTAG_ATTRIBUTE );
				node = g_list_next( node );
			}
			addrbook_write_elem_e( fp, 2, AB_ELTAG_ATTRIBUTE_LIST );
			addrbook_write_elem_e( fp, 1, AB_ELTAG_PERSON );
		}
	}
}

/*
* Write file hash table visitor function.
*/
static void addrbook_write_item_group_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;
	FILE *fp = ( FILE * ) data;
	GList *node;

	if( ! obj ) return;
	if( ADDRITEM_TYPE(obj) == ITEMTYPE_GROUP ) {
		ItemGroup *group = ( ItemGroup * ) value;
		if( group ) {
			addrbook_write_elem_s( fp, 1, AB_ELTAG_GROUP );
			addrbook_write_attr( fp, AB_ATTAG_UID, ADDRITEM_ID(group) );
			addrbook_write_attr( fp, AB_ATTAG_NAME, ADDRITEM_NAME(group) );
			addrbook_write_attr( fp, AB_ATTAG_REMARKS, group->remarks );
			fputs( " >\n", fp );

			/* Output email address links */
			addrbook_write_elem_s( fp, 2, AB_ELTAG_MEMBER_LIST );
			fputs( ">\n", fp );
			node = group->listEMail;
			while ( node ) {
				ItemEMail *email = node->data;
				ItemPerson *person = ( ItemPerson * ) ADDRITEM_PARENT(email);
				addrbook_write_elem_s( fp, 3, AB_ELTAG_MEMBER );
				addrbook_write_attr( fp, AB_ATTAG_PID, ADDRITEM_ID(person) );
				addrbook_write_attr( fp, AB_ATTAG_EID, ADDRITEM_ID(email) );
				fputs( " />\n", fp );
				node = g_list_next( node );
			}
			addrbook_write_elem_e( fp, 2, AB_ELTAG_MEMBER_LIST );
			addrbook_write_elem_e( fp, 1, AB_ELTAG_GROUP );
		}
	}
}

/*
* Write file hash table visitor function.
*/
static void addrbook_write_item_folder_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;
	FILE *fp = ( FILE * ) data;
	GList *node;

	if( ! obj ) return;
	if( ADDRITEM_TYPE(obj) == ITEMTYPE_FOLDER ) {
		ItemFolder *folder = ( ItemFolder * ) value;
		if( folder ) {
			addrbook_write_elem_s( fp, 1, AB_ELTAG_FOLDER );
			addrbook_write_attr( fp, AB_ATTAG_UID, ADDRITEM_ID(folder) );
			addrbook_write_attr( fp, AB_ATTAG_NAME, ADDRITEM_NAME(folder) );
			addrbook_write_attr( fp, AB_ATTAG_REMARKS, folder->remarks );
			fputs( " >\n", fp );
			addrbook_write_elem_s( fp, 2, AB_ELTAG_ITEM_LIST );
			fputs( ">\n", fp );

			/* Output persons */
			node = folder->listPerson;
			while ( node ) {
				ItemPerson *item = node->data;
				addrbook_write_elem_s( fp, 3, AB_ELTAG_ITEM );
				addrbook_write_attr( fp, AB_ATTAG_TYPE,  AB_ATTAG_VAL_PERSON );
				addrbook_write_attr( fp, AB_ATTAG_UID, ADDRITEM_ID(item ) );
				fputs( " />\n", fp );
				node = g_list_next( node );
			}

			/* Output groups */
			node = folder->listGroup;
			while ( node ) {
				ItemGroup *item = node->data;
				addrbook_write_elem_s( fp, 3, AB_ELTAG_ITEM );
				addrbook_write_attr( fp, AB_ATTAG_TYPE, AB_ATTAG_VAL_GROUP );
				addrbook_write_attr( fp, AB_ATTAG_UID, ADDRITEM_ID(item ) );
				fputs( " />\n", fp );
				node = g_list_next( node );
			}

			/* Output folders */
			node = folder->listFolder;
			while ( node ) {
				ItemFolder *item = node->data;
				addrbook_write_elem_s( fp, 3, AB_ELTAG_ITEM );
				addrbook_write_attr( fp, AB_ATTAG_TYPE, AB_ATTAG_VAL_FOLDER );
				addrbook_write_attr( fp, AB_ATTAG_UID, ADDRITEM_ID(item ) );
				fputs( " />\n", fp );
				node = g_list_next( node );
			}
			addrbook_write_elem_e( fp, 2, AB_ELTAG_ITEM_LIST );
			addrbook_write_elem_e( fp, 1, AB_ELTAG_FOLDER );
		}
	}
}

/*
* Output address book data to specified file.
* return: Status code.
*/
gint addrbook_write_to( AddressBookFile *book, gchar *newFile ) {
	FILE *fp;
	gchar *fileSpec;
#ifndef DEV_STANDALONE
	PrefFile *pfile;
#endif

	g_return_val_if_fail( book != NULL, -1 );
	g_return_val_if_fail( newFile != NULL, -1 );

	fileSpec = g_strconcat( book->path, G_DIR_SEPARATOR_S, newFile, NULL );

	book->retVal = MGU_OPEN_FILE;
#ifdef DEV_STANDALONE
	fp = g_fopen( fileSpec, "wb" );
	g_free( fileSpec );
	if( fp ) {
		fputs( "<?xml version=\"1.0\" ?>\n", fp );
#else
	pfile = prefs_file_open( fileSpec );
	g_free( fileSpec );
	if( pfile ) {
		fp = pfile->fp;
		fprintf( fp, "<?xml version=\"1.0\" encoding=\"%s\" ?>\n", CS_INTERNAL );
#endif
		addrbook_write_elem_s( fp, 0, AB_ELTAG_ADDRESS_BOOK );
		addrbook_write_attr( fp, AB_ATTAG_NAME, book->name );
		fputs( " >\n", fp );

		/* Output all persons */
		g_hash_table_foreach( book->addressCache->itemHash, addrbook_write_item_person_vis, fp );

		/* Output all groups */
		g_hash_table_foreach( book->addressCache->itemHash, addrbook_write_item_group_vis, fp );

		/* Output all folders */
		g_hash_table_foreach( book->addressCache->itemHash, addrbook_write_item_folder_vis, fp );

		addrbook_write_elem_e( fp, 0, AB_ELTAG_ADDRESS_BOOK );
		book->retVal = MGU_SUCCESS;
#ifdef DEV_STANDALONE
		fclose( fp );
#else
		if( prefs_file_close( pfile ) < 0 ) {
			book->retVal = MGU_ERROR_WRITE;
		}
#endif
	}

	fileSpec = NULL;
	return book->retVal;
}

/*
* Output address book data to original file.
* return: Status code.
*/
gint addrbook_save_data( AddressBookFile *book ) {
	g_return_val_if_fail( book != NULL, -1 );

	book->retVal = MGU_NO_FILE;
	if( book->fileName == NULL || *book->fileName == '\0' ) return book->retVal;
	if( book->path == NULL || *book->path == '\0' ) return book->retVal;

	addrbook_write_to( book, book->fileName );
	if( book->retVal == MGU_SUCCESS ) {
		book->dirtyFlag = FALSE;
	}
	return book->retVal;
}

/* **********************************************************************
* Address book edit interface functions...
* ***********************************************************************
*/

/*
* Move person's email item.
* param: book       Address book.
*        person     Person.
*        itemMove   Item to move.
*        itemTarget Target item before which to move item.
*/
ItemEMail *addrbook_move_email_before( AddressBookFile *book, ItemPerson *person,
			ItemEMail *itemMove, ItemEMail *itemTarget )
{
	ItemEMail *email = NULL;

	g_return_val_if_fail( book != NULL, NULL );

	email = addritem_move_email_before( person, itemMove, itemTarget );
	if( email ) {
		book->dirtyFlag = TRUE;
	}
	return email;
}

/*
* Move person's email item.
* param: book       Address book.
*        person     Person.
*        itemMove   Item to move.
*        itemTarget Target item after which to move item.
*/
ItemEMail *addrbook_move_email_after( AddressBookFile *book, ItemPerson *person,
			ItemEMail *itemMove, ItemEMail *itemTarget )
{
	ItemEMail *email = NULL;

	g_return_val_if_fail( book != NULL, NULL );

	email = addritem_move_email_after( person, itemMove, itemTarget );
	if( email ) {
		book->dirtyFlag = TRUE;
	}
	return email;
}

/*
* Hash table visitor function.
*/
static gboolean addrbook_free_simple_hash_vis( gpointer *key, gpointer *value, gpointer *data ) {
	g_free( key );
	key = NULL;
	value = NULL;
	return TRUE;
}

/*
* Update address book email list for specified person.
* Enter: book      Address book.
*        person    Person to update.
*        listEMail New list of email addresses.
* Note: The existing email addresses are replaced with the new addresses. Any references
* to old addresses in the groups are re-linked to the new addresses. All old addresses
* linked to the person are removed.
*/
void addrbook_update_address_list( AddressBookFile *book, ItemPerson *person, GList *listEMail ) {
	GList *node;
	GList *oldData;
	GList *listGroup;

	g_return_if_fail( book != NULL );
	g_return_if_fail( person != NULL );

	/* Remember old list */
	oldData = person->listEMail;

	/* Attach new address list to person. */
   	node = listEMail;
	while( node ) {
		ItemEMail *email = node->data;
		if( ADDRITEM_ID(email) == NULL ) {
			/* Allocate an ID */
			addrcache_id_email( book->addressCache, email );
		}
		ADDRITEM_PARENT(email) = ADDRITEM_OBJECT(person);
		node = g_list_next( node );
	}
	person->listEMail = listEMail;

	/* Get groups where person's email is listed */
	listGroup = addrcache_get_group_for_person( book->addressCache, person );
	if( listGroup ) {
		GHashTable *hashEMail;
		GList *nodeGrp;

		/* Load hash table with new address entries */
		hashEMail = g_hash_table_new( g_str_hash, g_str_equal );
	   	node = listEMail;
		while( node ) {
			ItemEMail *email = node->data;
			gchar *addr = g_strdup( email->address );
			g_strdown( addr );
			if( ! g_hash_table_lookup( hashEMail, addr ) ) {
				g_hash_table_insert( hashEMail, addr, email );
			}
			node = g_list_next( node );
		}

		/* Re-parent new addresses to existing groups, where email address match. */
		nodeGrp = listGroup;
		while( nodeGrp ) {
			ItemGroup *group = ( ItemGroup * ) nodeGrp->data;
			GList *groupEMail = group->listEMail;
			GList *nodeGrpEM;
			GList *listRemove = NULL;

			/* Process each email item linked to group */
			nodeGrpEM = groupEMail;
			while( nodeGrpEM ) {
				ItemEMail *emailGrp = ( ItemEMail * ) nodeGrpEM->data;
				if( ADDRITEM_PARENT(emailGrp) == ADDRITEM_OBJECT(person) ) {
					/* Found an email address for this person */
					ItemEMail *emailNew = NULL;
					gchar *addr = g_strdup( emailGrp->address );
					g_strdown( addr );
					emailNew = ( ItemEMail * ) g_hash_table_lookup( hashEMail, addr );
					g_free( addr );
					if( emailNew ) {
						/* Point to this entry */
						nodeGrpEM->data = emailNew;
					}
					else {
						/* Mark for removal */
						listRemove = g_list_append( listRemove, emailGrp );
					}
				}
				/* Move on to next email link */
				nodeGrpEM = g_list_next( nodeGrpEM );
			}

			/* Process all removed links in current group */
			nodeGrpEM = listRemove;
			while( nodeGrpEM ) {
				ItemEMail *emailGrp = nodeGrpEM->data;
				groupEMail = g_list_remove( groupEMail, emailGrp );
				nodeGrpEM = g_list_next( nodeGrpEM );
			}
			group->listEMail = groupEMail;

			/* Move on to next group */
			nodeGrp = g_list_next( nodeGrp );

		}

		/* Clear hash table */
		g_hash_table_foreach_remove( hashEMail, ( GHRFunc ) addrbook_free_simple_hash_vis, NULL );
		g_hash_table_destroy( hashEMail );
		hashEMail = NULL;
		g_list_free( listGroup );
		listGroup = NULL;
	}

	/* Free up old data */
	addritem_free_list_email( oldData );
	oldData = NULL;
	book->dirtyFlag = TRUE;

}

/*
* Add person and address data to address book.
* Enter: book      Address book.
*        folder    Folder where to add person, or NULL for root folder.
*        listEMail New list of email addresses.
* Return: Person added.
* Note: A new person is created with specified list of email addresses. All objects inserted
* into address book.
*/
ItemPerson *addrbook_add_address_list( AddressBookFile *book, ItemFolder *folder, GList *listEMail ) {
	ItemPerson *person;
	ItemFolder *f = folder;
	GList *node;

	g_return_val_if_fail( book != NULL, NULL );

	if( ! f ) f = book->addressCache->rootFolder;
	person = addritem_create_item_person();
	addrcache_id_person( book->addressCache, person );
	addrcache_folder_add_person( book->addressCache, f, person );

   	node = listEMail;
	while( node ) {
		ItemEMail *email = node->data;
		if( ADDRITEM_ID(email) == NULL ) {
			addrcache_id_email( book->addressCache, email );
		}
		addrcache_person_add_email( book->addressCache, person, email );
		node = g_list_next( node );
	}
	book->dirtyFlag = TRUE;
	return person;
}

#if 0
/*
* Load hash table visitor function.
*/
static void addrbook_load_hash_table_email_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;

	if( ADDRITEM_TYPE(obj) == ITEMTYPE_EMAIL ) {
		GHashTable *table = ( GHashTable * ) data;
		gchar *newKey = g_strdup( key );
		ItemEMail *email = ( ItemEMail * ) obj;
		if( ! g_hash_table_lookup( table, newKey ) ) {
			g_hash_table_insert( table, newKey, email );
		}
	}
}

/*
* Load hash table with links to email addresses.
*/
static void addrbook_load_hash_table_email( AddressBookFile *book, GHashTable *table ) {
	g_return_if_fail( book != NULL );
	g_return_if_fail( table != NULL );
	g_hash_table_foreach( book->addressCache->itemHash, addrbook_load_hash_table_email_vis, table );
}
#endif

/*
* Build available email list visitor function.
*/
static void addrbook_build_avail_email_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;

	if( ADDRITEM_TYPE(obj) == ITEMTYPE_PERSON ) {
		AddressBookFile *book = data;
		ItemPerson *person = ( ItemPerson * ) obj;
		GList *node = person->listEMail;
		while( node ) {
			ItemEMail *email = node->data;
			/* gchar *newKey = g_strdup( ADDRITEM_ID(email) ); */

			if( ! g_hash_table_lookup( book->tempHash, ADDRITEM_ID(email) ) ) {
				book->tempList = g_list_append( book->tempList, email );
			}
			node = g_list_next( node );
		}
	}
}

/*
* Return link list of available email items (which have not already been linked to
* groups). Note that the list contains references to items and should be g_free()
* when done. Do *NOT* attempt to used the addrbook_free_xxx() functions... this will
* destroy the addressbook data!
* Return: List of items, or NULL if none.
*/
GList *addrbook_get_available_email_list( AddressBookFile *book, ItemGroup *group ) {
	GList *list = NULL;
	GHashTable *table;

	g_return_val_if_fail( book != NULL, NULL );

	/* Load hash table with group email entries */
	table = g_hash_table_new( g_str_hash, g_str_equal );
	if( group ) {
		list = group->listEMail;
		while( list ) {
			ItemEMail *email = list->data;
			g_hash_table_insert( table, ADDRITEM_ID(email), email );
			list = g_list_next( list );
		}
	}

	/* Build list of available email addresses which exclude those already in groups */
	book->tempList = NULL;
	book->tempHash = table;
	g_hash_table_foreach( book->addressCache->itemHash, addrbook_build_avail_email_vis, book );
	list = book->tempList;
	book->tempList = NULL;
	book->tempHash = NULL;

	/* Clear hash table */
	g_hash_table_destroy( table );
	table = NULL;

	return list;
}

/*
* Update address book email list for specified group.
* Enter: book      Address book.
*        group     group to update.
*        listEMail New list of email addresses. This should *NOT* be g_free() when done.
* Note: The existing email addresses are replaced with the new addresses. Any references
* to old addresses in the groups are re-linked to the new addresses. All old addresses
* linked to the person are removed.
*/
void addrbook_update_group_list( AddressBookFile *book, ItemGroup *group, GList *listEMail ) {
	GList *oldData;

	g_return_if_fail( book != NULL );
	g_return_if_fail( group != NULL );

	/* Remember old list */
	oldData = group->listEMail;
	group->listEMail = listEMail;
	mgu_clear_list( oldData );
	g_list_free ( oldData );
	book->dirtyFlag = TRUE;
}

/*
* Add group and email list to address book.
* Enter: book      Address book.
*        folder    Parent folder, or NULL for root folder.
*        listEMail New list of email addresses. This should *NOT* be g_free() when done.
* Return: Group object.
* Note: The existing email addresses are replaced with the new addresses. Any references
* to old addresses in the groups are re-linked to the new addresses. All old addresses
* linked to the person are removed.
*/
ItemGroup *addrbook_add_group_list( AddressBookFile *book, ItemFolder *folder, GList *listEMail ) {
	ItemGroup *group = NULL;
	ItemFolder *f = folder;

	g_return_val_if_fail( book != NULL, NULL );

	if( ! f ) f = book->addressCache->rootFolder;
	group = addritem_create_item_group();
	addrcache_id_group( book->addressCache, group );
	addrcache_folder_add_group( book->addressCache, f, group );
	group->listEMail = listEMail;
	book->dirtyFlag = TRUE;
	return group;
}

/*
* Add new folder to address book.
* Enter: book   Address book.
*        parent	Parent folder.
* Return: Folder that was added. This should *NOT* be g_free() when done.
*/
ItemFolder *addrbook_add_new_folder( AddressBookFile *book, ItemFolder *parent ) {
	ItemFolder *folder = NULL;
	ItemFolder *p = parent;

	g_return_val_if_fail( book != NULL, NULL );

	if( ! p ) p = book->addressCache->rootFolder;
	folder = addritem_create_item_folder();
	addrcache_id_folder( book->addressCache, folder );
	if( addrcache_hash_add_folder( book->addressCache, folder ) ) {
		p->listFolder = g_list_append( p->listFolder, folder );
		ADDRITEM_PARENT(folder) = ADDRITEM_OBJECT(p);
		book->dirtyFlag = TRUE;
	}
	else {
		addritem_free_item_folder( folder );
		folder = NULL;
	}
	return folder;
}

/*
* Update address book attribute list for specified person.
* Enter: book       Address book.
*        person     Person to update.
*        listAttrib New list of attributes.
* Note: The existing email addresses are replaced with the new addresses. All old attributes
* linked to the person are removed.
*/
void addrbook_update_attrib_list( AddressBookFile *book, ItemPerson *person, GList *listAttrib ) {
	GList *node;
	GList *oldData;

	g_return_if_fail( book != NULL );
	g_return_if_fail( person != NULL );

	/* Remember old list */
	oldData = person->listAttrib;

	/* Attach new address list to person. */
   	node = listAttrib;
	while( node ) {
		UserAttribute *attrib = node->data;
		if( attrib->uid == NULL ) {
			/* Allocate an ID */
			addrcache_id_attribute( book->addressCache, attrib );
		}
		node = g_list_next( node );
	}
	person->listAttrib = listAttrib;

	/* Free up old data */
	addritem_free_list_attribute( oldData );
	oldData = NULL;
	book->dirtyFlag = TRUE;

}

/*
* Add attribute data for person to address book.
* Enter: book       Address book.
*        person     New person object.
*        listAttrib New list of attributes.
* Note: Only attributes are inserted into address book.
*/
void addrbook_add_attrib_list( AddressBookFile *book, ItemPerson *person, GList *listAttrib ) {
	GList *node;

	g_return_if_fail( book != NULL );
	g_return_if_fail( person != NULL );

   	node = listAttrib;
	while( node ) {
		UserAttribute *attrib = node->data;
		if( attrib->uid == NULL ) {
			addrcache_id_attribute( book->addressCache, attrib );
		}
		addritem_person_add_attribute( person, attrib );
		node = g_list_next( node );
	}
	book->dirtyFlag = TRUE;
}

/*
* Return address book file for specified object.
* Enter: aio Book item object.
* Return: Address book, or NULL if not found.
*/
AddressBookFile *addrbook_item_get_bookfile( AddrItemObject *aio ) {
	AddressBookFile *book = NULL;

	if( aio ) {
		ItemFolder *parent = NULL;
		ItemFolder *root = NULL;
		if( aio->type == ITEMTYPE_EMAIL ) {
			ItemPerson *person = ( ItemPerson * ) ADDRITEM_PARENT(aio);
			if( person ) {
				parent = ( ItemFolder * ) ADDRITEM_PARENT(person);
			}
		}
		else {
			parent = ( ItemFolder * ) ADDRITEM_PARENT(aio);
		}
		if( parent ) {
			root = addrcache_find_root_folder( parent );
		}
		if( root ) {
			book = ( AddressBookFile * ) ADDRITEM_PARENT(root);
		}
	}
	return book;
}

/*
* Remove folder from address book. Children are re-parented to parent folder.
* param: folder Folder to remove.
* return: Folder, or NULL if not found. Note that object should still be freed.
*/
ItemFolder *addrbook_remove_folder( AddressBookFile *book, ItemFolder *folder ) {
	ItemFolder *f;

	g_return_val_if_fail( book != NULL, NULL );

	f = addrcache_remove_folder( book->addressCache, folder );
	if( f ) book->dirtyFlag = TRUE;
	return f;
}

/*
* Remove folder from address book. Children are deleted.
* param: folder Folder to remove.
* return: Folder, or NULL if not found. Note that object should still be freed.
*/
ItemFolder *addrbook_remove_folder_delete( AddressBookFile *book, ItemFolder *folder ) {
	ItemFolder *f;

	g_return_val_if_fail( book != NULL, NULL );

	f = addrcache_remove_folder_delete( book->addressCache, folder );
	if( f ) book->dirtyFlag = TRUE;
	return f;
}

#define WORK_BUFLEN     1024
#define ADDRBOOK_DIGITS "0123456789"

/*
* Return list of existing address book files.
* Enter: book Address book file.
* Return: File list.
*/
GList *addrbook_get_bookfile_list( AddressBookFile *book ) {
	gchar *adbookdir;
	GDir *dir;
	const gchar *dir_name;
	GStatBuf statbuf;
	gchar buf[ WORK_BUFLEN ];
	gchar numbuf[ WORK_BUFLEN ];
	gint len, lenpre, lensuf, lennum;
	long int val, maxval;
	GList *fileList = NULL;

	g_return_val_if_fail( book != NULL, NULL );

	if( book->path == NULL || *book->path == '\0' ) {
		book->retVal = MGU_NO_PATH;
		return NULL;
	}

	strcpy( buf, book->path );
	len = strlen( buf );
	if( len > 0 ) {
		if( buf[ len-1 ] != G_DIR_SEPARATOR ) {
			buf[ len ] = G_DIR_SEPARATOR;
			buf[ ++len ] = '\0';
		}
	}

	adbookdir = g_strdup( buf );
	strcat( buf, ADDRBOOK_PREFIX );

	if( ( dir = g_dir_open( adbookdir, 0, NULL ) ) == NULL ) {
		book->retVal = MGU_OPEN_DIRECTORY;
		g_free( adbookdir );
		return NULL;
	}

	lenpre = strlen( ADDRBOOK_PREFIX );
	lensuf = strlen( ADDRBOOK_SUFFIX );
	lennum = FILE_NUMDIGITS + lenpre;
	maxval = -1;

	while( ( dir_name = g_dir_read_name( dir ) ) != NULL ) {
		gchar *endptr = NULL;
		gint i;
		gboolean flg;

		strcpy( buf, adbookdir );
		strcat( buf, dir_name );
		g_stat( buf, &statbuf );
		if( S_IFREG & statbuf.st_mode ) {
			if( strncmp( dir_name, ADDRBOOK_PREFIX, lenpre ) == 0 ) {
				if( strncmp( (dir_name) + lennum, ADDRBOOK_SUFFIX, lensuf ) == 0 ) {
					strncpy( numbuf, (dir_name) + lenpre, FILE_NUMDIGITS );
					numbuf[ FILE_NUMDIGITS ] = '\0';
					flg = TRUE;
					for( i = 0; i < FILE_NUMDIGITS; i++ ) {
						if( ! strchr( ADDRBOOK_DIGITS, numbuf[i] ) ) {
							flg = FALSE;
							break;
						}
					}
					if( flg ) {
						/* Get value */
						val = strtol( numbuf, &endptr, 10 );
						if( endptr  && val > -1 ) {
							if( val > maxval ) maxval = val;
							fileList = g_list_append( fileList, g_strdup( dir_name ) );
						}
					}
				}
			}
		}
	}
	g_dir_close( dir );
	g_free( adbookdir );

	book->maxValue = maxval; 
	book->retVal = MGU_SUCCESS;
	return fileList;
}

/*
* Return file name for specified file number.
* Enter:  fileNum File number.
* Return: File name, or NULL if file number too large. Should be g_free() when done.
*/
gchar *addrbook_gen_new_file_name( gint fileNum ) {
	gchar fmt[ 30 ];
	gchar buf[ WORK_BUFLEN ];
	gint n = fileNum;
	long int nmax;

	if( n < 1 ) n = 1;
	nmax = -1 + (long int) pow( 10, FILE_NUMDIGITS );
	if( fileNum > nmax ) return NULL;
	sprintf( fmt, "%%s%%0%dd%%s", FILE_NUMDIGITS );
	sprintf( buf, fmt, ADDRBOOK_PREFIX, n, ADDRBOOK_SUFFIX );
	return g_strdup( buf );
}

/* **********************************************************************
* Address book test functions...
* ***********************************************************************
*/

#if 0
static void addrbook_show_attribs( GList *attr ) {
	while( attr ) {
		gchar *name = ((XMLAttr *)attr->data)->name;
		gchar *value = ((XMLAttr *)attr->data)->value;
		printf( "\tn/v = %s : %s\n", name, value );
		attr = g_list_next( attr );
	}
	printf( "\t---\n" );
}
#endif

/*
* Test email address list.
*/
static void addrbook_chkparse_addr_list( AddressBookFile *book, XMLFile *file ){
	guint prev_level;
	GList *attr;

	for (;;) {
		prev_level = file->level;
		if( xml_parse_next_tag( file ) ) {
			longjmp( book->jumper, 1 );
		}
		if (file->level < prev_level) return;
		attr = xml_get_current_tag_attr(file);
		/* addrbook_show_attribs( attr ); */
		if( xml_compare_tag( file, AB_ELTAG_ADDRESS ) ) {
			addrbook_chkparse_addr_list( book, file );
		}
	}
}

/*
* Test user attributes for person.
*/
static void addrbook_chkparse_attribute( AddressBookFile *book, XMLFile *file ) {
	GList *attr;
	gchar *element;

	attr = xml_get_current_tag_attr(file);
	/* addrbook_show_attribs( attr ); */
	element = xml_get_element( file );
	/* printf( "\t\tattrib value : %s\n", element ); */
}

/*
* Test attribute list.
*/
static void addrbook_chkparse_attr_list( AddressBookFile *book, XMLFile *file ){
	guint prev_level;

	for (;;) {
		prev_level = file->level;
		if( xml_parse_next_tag( file ) ) {
			longjmp( book->jumper, 1 );
		}
		if (file->level < prev_level) return;
		if( xml_compare_tag( file, AB_ELTAG_ATTRIBUTE ) ) {
			addrbook_chkparse_attribute( book, file );
			addrbook_chkparse_attr_list( book, file );
		}
	}
}

/*
* Test person.
*/
static void addrbook_chkparse_person( AddressBookFile *book, XMLFile *file ) {
	GList *attr;

	attr = xml_get_current_tag_attr(file);
	/* addrbook_show_attribs( attr ); */
	if( xml_parse_next_tag( file ) ) {	/* Consume closing tag */
		longjmp( book->jumper, 1 );
	}
	if( xml_compare_tag( file, AB_ELTAG_ADDRESS_LIST ) ) {
		addrbook_chkparse_addr_list( book, file );
	}
	if( xml_parse_next_tag( file ) ) {	/* Consume closing tag */
		longjmp( book->jumper, 1 );
	}
	if( xml_compare_tag( file, AB_ELTAG_ATTRIBUTE_LIST ) ) {
		addrbook_chkparse_attr_list( book, file );
	}
}

/*
* Test group member list.
*/
static void addrbook_chkparse_member_list( AddressBookFile *book, XMLFile *file ){
	GList *attr;
	guint prev_level;

	for (;;) {
		prev_level = file->level;
		if( xml_parse_next_tag( file ) ) {
			longjmp( book->jumper, 1 );
		}
		if (file->level < prev_level) return;
		if( xml_compare_tag( file, AB_ELTAG_MEMBER ) ) {
			attr = xml_get_current_tag_attr(file);
			/* addrbook_show_attribs( attr ); */
			addrbook_chkparse_member_list( book, file );
		}
		else {
			attr = xml_get_current_tag_attr( file );
			/* addrbook_show_attribs( attr ); */
		}
	}
}

/*
* Test group.
*/
static void addrbook_chkparse_group( AddressBookFile *book, XMLFile *file ) {
	GList *attr;

	attr = xml_get_current_tag_attr(file);
	/* addrbook_show_attribs( attr ); */
	if( xml_parse_next_tag( file ) ) {	/* Consume closing tag */
		longjmp( book->jumper, 1 );
	}
	if( xml_compare_tag( file, AB_ELTAG_MEMBER_LIST ) ) {
		addrbook_chkparse_member_list( book, file );
	}
}

/*
* Test folder item list.
*/
static void addrbook_chkparse_folder_list( AddressBookFile *book, XMLFile *file ){
	GList *attr;
	guint prev_level;

	for (;;) {
		prev_level = file->level;
		if( xml_parse_next_tag( file ) ) {
			longjmp( book->jumper, 1 );
		}
		if (file->level < prev_level) return;
		if( xml_compare_tag( file, AB_ELTAG_ITEM ) ) {
			attr = xml_get_current_tag_attr(file);
			/* addrbook_show_attribs( attr ); */
			addrbook_chkparse_folder_list( book, file );
		}
		else {
			attr = xml_get_current_tag_attr( file );
			/* addrbook_show_attribs( attr ); */
		}
	}
}

/*
* Test folder.
*/
static void addrbook_chkparse_folder( AddressBookFile *book, XMLFile *file ) {
	GList *attr;

	attr = xml_get_current_tag_attr(file);
	/* addrbook_show_attribs( attr ); */
	if( xml_parse_next_tag( file ) ) {	/* Consume closing tag */
		longjmp( book->jumper, 1 );
	}
	if( xml_compare_tag( file, AB_ELTAG_ITEM_LIST ) ) {
		addrbook_chkparse_folder_list( book, file );
	}
}

/*
* Test address book.
*/
static gboolean addrbook_chkread_tree( AddressBookFile *book, XMLFile *file ) {
	GList *attr;
	gboolean retVal;

	if( xml_get_dtd( file ) ) {
		return FALSE;
	}
	if( xml_parse_next_tag( file ) ) {
		return FALSE;
	}

	if( ! xml_compare_tag( file, AB_ELTAG_ADDRESS_BOOK ) ) {
		return FALSE;
	}

	attr = xml_get_current_tag_attr(file);
	/* addrbook_show_attribs( attr ); */

	retVal = TRUE;
	for (;;) {
		if (! file->level ) break;
		/* Get item tag */
		if( xml_parse_next_tag( file ) ) {
			longjmp( book->jumper, 1 );
		}
		/* Get next tag (person, group or folder) */
		if( xml_compare_tag( file, AB_ELTAG_PERSON ) ) {
			addrbook_chkparse_person( book, file );
		}
		else if( xml_compare_tag( file, AB_ELTAG_GROUP ) ) {
			addrbook_chkparse_group( book, file );
		}
		else if( xml_compare_tag( file, AB_ELTAG_FOLDER ) ) {
			addrbook_chkparse_folder( book, file );
		}
	}
	return retVal;
}

/*
* Test address book file by parsing contents.
* Enter: book     Address book file to check.
*        fileName File name to check.
* Return: MGU_SUCCESS if file appears to be valid format.
*/
gint addrbook_test_read_file( AddressBookFile *book, gchar *fileName ) {
	XMLFile *file = NULL;
	gchar *fileSpec = NULL;

	g_return_val_if_fail( book != NULL, -1 );

	fileSpec = g_strconcat( book->path, G_DIR_SEPARATOR_S, fileName, NULL );
	book->retVal = MGU_OPEN_FILE;
	file = xml_open_file( fileSpec );
	g_free( fileSpec );
	if( file ) {
		book->retVal = MGU_BAD_FORMAT;
		if( setjmp( book->jumper ) ) {
			/* printf( "Caught Ya!!!\n" ); */
			xml_close_file( file );
			return book->retVal;
		}
		if( addrbook_chkread_tree( book, file ) ) {
			book->retVal = MGU_SUCCESS;
		}
		xml_close_file( file );
	}
	return book->retVal;
}

/*
* Return link list of all persons in address book.  Note that the list contains
* references to items. Do *NOT* attempt to use the addrbook_free_xxx() functions...
* this will destroy the addressbook data!
* Return: List of items, or NULL if none.
*/
GList *addrbook_get_all_persons( AddressBookFile *book ) {
	g_return_val_if_fail( book != NULL, NULL );
	return addrcache_get_all_persons( book->addressCache );
}

/*
* Add person and address data to address book.
* Enter: book      Address book.
*        folder    Folder where to add person, or NULL for root folder.
*        name      Common name.
*        address   EMail address.
*        remarks   Remarks.
* Return: Person added. Do not *NOT* to use the addrbook_free_xxx() functions...
* this will destroy the address book data.
*/
ItemPerson *addrbook_add_contact( AddressBookFile *book, ItemFolder *folder, const gchar *name,
		const gchar *address, const gchar *remarks )
{
	ItemPerson *person = NULL;

	g_return_val_if_fail( book != NULL, NULL );

	person = addrcache_add_contact( book->addressCache, folder, name, address, remarks );
	if( person ) book->dirtyFlag = TRUE;
	return person;
}

/*
* End of Source.
*/
