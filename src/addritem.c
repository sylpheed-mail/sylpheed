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
 * General primitive address item objects.
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "addritem.h"
#include "mgutils.h"

/*
* Create new email address item.
*/
ItemEMail *addritem_create_item_email( void ) {
	ItemEMail *item;
	item = g_new0( ItemEMail, 1 );
	ADDRITEM_TYPE(item) = ITEMTYPE_EMAIL;
	ADDRITEM_ID(item) = NULL;
	ADDRITEM_NAME(item) = NULL;
	ADDRITEM_PARENT(item) = NULL;
	ADDRITEM_SUBTYPE(item) = 0;
	item->address = NULL;
	item->remarks = NULL;
	return item;
}

/*
* Create copy of specified email address item.
*/
ItemEMail *addritem_copy_item_email( ItemEMail *item ) {
	ItemEMail *itemNew = NULL;
	if( item ) {
		itemNew = addritem_create_item_email();
		ADDRITEM_TYPE(itemNew) = ADDRITEM_TYPE(item);
		ADDRITEM_ID(itemNew) = g_strdup( ADDRITEM_ID(item) );
		ADDRITEM_NAME(itemNew) = g_strdup( ADDRITEM_NAME(item) );
		ADDRITEM_PARENT(itemNew) = ADDRITEM_PARENT(item);
		itemNew->address = g_strdup( item->address );
		itemNew->remarks = g_strdup( item->remarks );
	}
	return itemNew;
}

void addritem_email_set_id( ItemEMail *email, const gchar *value ) {
	ADDRITEM_ID(email) = mgu_replace_string( ADDRITEM_ID(email), value );
}
void addritem_email_set_alias( ItemEMail *email, const gchar *value ) {
	ADDRITEM_NAME(email) = mgu_replace_string( ADDRITEM_NAME(email), value );
}
void addritem_email_set_address( ItemEMail *email, const gchar *value ) {
	email->address = mgu_replace_string( email->address, value );
}
void addritem_email_set_remarks( ItemEMail *email, const gchar *value ) {
	email->remarks = mgu_replace_string( email->remarks, value );
}

/*
* Free address item email.
*/
void addritem_free_item_email( ItemEMail *item ) {
	g_return_if_fail( item != NULL );

	/* Free internal stuff */
	g_free( ADDRITEM_ID(item) );
	g_free( ADDRITEM_NAME(item) );
	g_free( item->address );
	g_free( item->remarks );

	ADDRITEM_OBJECT(item)->type = ITEMTYPE_NONE;
	ADDRITEM_ID(item) = NULL;
	ADDRITEM_NAME(item) = NULL;
	ADDRITEM_PARENT(item) = NULL;
	ADDRITEM_SUBTYPE(item) = 0;
	item->address = NULL;
	item->remarks = NULL;
	g_free( item );
}

/*
* Create new attribute.
*/
UserAttribute *addritem_create_attribute( void ) {
	UserAttribute *item;
	item = g_new0( UserAttribute, 1 );
	item->uid = NULL;
	item->name = NULL;
	item->value = NULL;
	return item;
}

/*
* Create copy of specified attribute.
*/
UserAttribute *addritem_copy_attribute( UserAttribute *item ) {
	UserAttribute *itemNew = NULL;
	if( item ) {
		itemNew = addritem_create_attribute();
		itemNew->uid = g_strdup( item->uid );
		itemNew->name = g_strdup( item->name );
		itemNew->value = g_strdup( item->value );
	}
	return itemNew;
}

void addritem_attrib_set_id( UserAttribute *item, const gchar *value ) {
	g_return_if_fail( item != NULL );
	item->uid = mgu_replace_string( item->uid, value );
}
void addritem_attrib_set_name( UserAttribute *item, const gchar *value ) {
	g_return_if_fail( item != NULL );
	item->name = mgu_replace_string( item->name, value );
}
void addritem_attrib_set_value( UserAttribute *item, const gchar *value ) {
	g_return_if_fail( item != NULL );
	item->value = mgu_replace_string( item->value, value );
}

/*
* Free user attribute.
*/
void addritem_free_attribute( UserAttribute *item ) {
	g_return_if_fail( item != NULL );
	g_free( item->uid );
	g_free( item->name );
	g_free( item->value );
	item->uid = NULL;
	item->name = NULL;
	item->value = NULL;
	g_free( item );
}

/*
* Create new address book person.
*/
ItemPerson *addritem_create_item_person( void ) {
	ItemPerson *person;
	person = g_new0( ItemPerson, 1 );
	ADDRITEM_TYPE(person) = ITEMTYPE_PERSON;
	ADDRITEM_ID(person) = NULL;
	ADDRITEM_NAME(person) = NULL;
	ADDRITEM_PARENT(person) = NULL;
	ADDRITEM_SUBTYPE(person) = 0;
	person->firstName = NULL;
	person->lastName = NULL;
	person->nickName = NULL;
	person->listEMail = NULL;
	person->listAttrib = NULL;
	person->externalID = NULL;
	person->isOpened = FALSE;
	return person;
}

void addritem_person_set_id( ItemPerson *person, const gchar *value ) {
	ADDRITEM_ID(person) = mgu_replace_string( ADDRITEM_ID(person), value );
}
void addritem_person_set_first_name( ItemPerson *person, const gchar *value ) {
	person->firstName = mgu_replace_string( person->firstName, value );
}
void addritem_person_set_last_name( ItemPerson *person, const gchar *value ) {
	person->lastName = mgu_replace_string( person->lastName, value );
}
void addritem_person_set_nick_name( ItemPerson *person, const gchar *value ) {
	person->nickName = mgu_replace_string( person->nickName, value );
}
void addritem_person_set_common_name( ItemPerson *person, const gchar *value ) {
	ADDRITEM_NAME(person) = mgu_replace_string( ADDRITEM_NAME(person), value );
}
void addritem_person_set_external_id( ItemPerson *person, const gchar *value ) {
	person->externalID = mgu_replace_string( person->externalID, value );
}
void addritem_person_set_opened( ItemPerson *person, const gboolean value ) {
	person->isOpened = value;
}

/*
* Free linked list of item addresses.
*/
void addritem_free_list_email( GList *list ) {
	GList *node = list;
	while( node ) {
		addritem_free_item_email( node->data );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( list );
}

/*
* Free linked list of attributes.
*/
void addritem_free_list_attribute( GList *list ) {
	GList *node = list;
	while( node ) {
		addritem_free_attribute( node->data );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( list );
}

/*
* Free address person.
*/
void addritem_free_item_person( ItemPerson *person ) {
	g_return_if_fail( person != NULL );

	/* Free internal stuff */
	g_free( ADDRITEM_ID(person) );
	g_free( ADDRITEM_NAME(person) );
	g_free( person->firstName );
	g_free( person->lastName );
	g_free( person->nickName );
	g_free( person->externalID );
	addritem_free_list_email( person->listEMail );
	addritem_free_list_attribute( person->listAttrib );

	ADDRITEM_OBJECT(person)->type = ITEMTYPE_NONE;
	ADDRITEM_ID(person) = NULL;
	ADDRITEM_NAME(person) = NULL;
	ADDRITEM_PARENT(person) = NULL;
	ADDRITEM_SUBTYPE(person) = 0;
	person->firstName = NULL;
	person->lastName = NULL;
	person->nickName = NULL;
	person->externalID = NULL;
	person->listEMail = NULL;
	person->listAttrib = NULL;

	g_free( person );
}

/*
* Print address item.
*/
void addritem_print_item_email( ItemEMail *item, FILE *stream ) {
	g_return_if_fail( item != NULL );
	fprintf( stream, "\t\tt/id: %d : '%s'\n", ADDRITEM_TYPE(item), ADDRITEM_ID(item) );
	fprintf( stream, "\t\tsubty: %d\n", ADDRITEM_SUBTYPE(item) );
	fprintf( stream, "\t\talis: '%s'\n", ADDRITEM_NAME(item) );
	fprintf( stream, "\t\taddr: '%s'\n", item->address );
	fprintf( stream, "\t\trems: '%s'\n", item->remarks );
	fprintf( stream, "\t\t---\n" );
}

/*
* Print user attribute.
*/
void addritem_print_attribute( UserAttribute *item, FILE *stream ) {
	g_return_if_fail( item != NULL );
	fprintf( stream, "\t\tuid  : '%s'\n", item->uid );
	fprintf( stream, "\t\tname : '%s'\n", item->name );
	fprintf( stream, "\t\tvalue: '%s'\n", item->value );
	fprintf( stream, "\t\t---\n" );
}

/*
* Print person item.
*/
void addritem_print_item_person( ItemPerson *person, FILE *stream ) {
	GList *node;
	g_return_if_fail( person != NULL );
	fprintf( stream, "Person:\n" );
	fprintf( stream, "\tt/uid: %d : '%s'\n", ADDRITEM_TYPE(person), ADDRITEM_ID(person) );
	fprintf( stream, "\tsubty: %d\n", ADDRITEM_SUBTYPE(person) );
	fprintf( stream, "\tcommn: '%s'\n", ADDRITEM_NAME(person) );
	fprintf( stream, "\tfirst: '%s'\n", person->firstName );
	fprintf( stream, "\tlast : '%s'\n", person->lastName );
	fprintf( stream, "\tnick : '%s'\n", person->nickName );
	fprintf( stream, "\textID: '%s'\n", person->externalID );
	fprintf( stream, "\teMail:\n" );
	fprintf( stream, "\t---\n" );
	node = person->listEMail;
	while( node ) {
		addritem_print_item_email( node->data, stream );
		node = g_list_next( node );
	}
	fprintf( stream, "\tuAttr:\n" );
	fprintf( stream, "\t---\n" );
	node = person->listAttrib;
	while( node ) {
		addritem_print_attribute( node->data, stream );
		node = g_list_next( node );
	}
	fprintf( stream, "\t===\n" );
}

/*
* Add EMail address to person.
* return: TRUE if item added.
*/
gboolean addritem_person_add_email( ItemPerson *person, ItemEMail *email ) {
	GList *node;

	g_return_val_if_fail( person != NULL, FALSE );
	g_return_val_if_fail( email != NULL, FALSE );

	node = person->listEMail;
	while( node ) {
		if( node->data == email ) return FALSE;
		node = g_list_next( node );
	}
	person->listEMail = g_list_append( person->listEMail, email );
	ADDRITEM_PARENT(email) = ADDRITEM_OBJECT(person);
	return TRUE;
}

/*
* Return email object with specified ID.
* param: person Person object.
*        eid	EMail ID.
* return: EMail object, or NULL if not found.
*/
ItemEMail *addritem_person_get_email( ItemPerson *person, const gchar *eid ) {
	ItemEMail *email = NULL;
	GList *node;

	g_return_val_if_fail( person != NULL, NULL );
	if( eid == NULL || *eid == '\0' ) return NULL;

	/* Look for email */
	node = person->listEMail;
	while( node ) {
		AddrItemObject *objE = node->data;
		gchar *ide = ADDRITEM_ID(objE);
		if( ide ) {
			if( strcmp( ide, eid ) == 0 ) {
				email = ( ItemEMail * ) objE;
			}
		}
		node = g_list_next( node );
	}
	return email;
}

/*
* Remove email address for specified person.
* param: person Person object.
*        eid	EMail ID.
* return: EMail object, or NULL if not found. Note that object should still be freed.
*/
ItemEMail *addritem_person_remove_email_id( ItemPerson *person, const gchar *eid ) {
	ItemEMail *email = NULL;
	GList *node;

	g_return_val_if_fail( person != NULL, NULL );
	if( eid == NULL || *eid == '\0' ) return NULL;

	/* Look for email */
	node = person->listEMail;
	while( node ) {
		AddrItemObject *objE = node->data;
		gchar *ide = ADDRITEM_ID(objE);
		if( ide ) {
			if( strcmp( ide, eid ) == 0 ) {
				email = ( ItemEMail * ) objE;
			}
		}
		node = g_list_next( node );
	}

	if( email ) {
		/* Remove email from person's address list */
		if( person->listEMail ) {
			person->listEMail = g_list_remove( person->listEMail, email );
		}
		/* Unlink reference to person. */
		ADDRITEM_PARENT(email) = NULL;
	}
	return email;
}

/*
* Remove email address for specified.
* param: person	Person.
*        email	EMail to remove.
* return: EMail object, or NULL if not found. Note that object should still be freed.
*/
ItemEMail *addritem_person_remove_email( ItemPerson *person, ItemEMail *email ) {
	gboolean found = FALSE;
	GList *node;

	g_return_val_if_fail( person != NULL, NULL );
	if( email == NULL ) return NULL;

	/* Look for email */
	node = person->listEMail;
	while( node ) {
		if( node-> data == email ) {
			found = TRUE;
			break;
		}
		node = g_list_next( node );
	}

	if( found ) {
		/* Remove email from person's address list */
		if( person->listEMail ) {
			person->listEMail = g_list_remove( person->listEMail, email );
		}
		/* Unlink reference to person. */
		ADDRITEM_PARENT(email) = NULL;
		return email;
	}
	return NULL;
}

/*
* Add user attribute to person.
* return: TRUE if item added.
*/
void addritem_person_add_attribute( ItemPerson *person, UserAttribute *attrib ) {
	g_return_if_fail( person != NULL );
	person->listAttrib = g_list_append( person->listAttrib, attrib );
}

/*
* Return attribute with specified ID.
* param: person Person object.
*        aid    Attribute ID.
* return: UserAttribute object, or NULL if not found. Note that object should still be freed.
*/
UserAttribute *addritem_person_get_attribute( ItemPerson *person, const gchar *aid ) {
	UserAttribute *attrib = NULL;
	GList *node;

	g_return_val_if_fail( person != NULL, NULL );
	if( aid == NULL || *aid == '\0' ) return NULL;

	/* Look for attribute */
	node = person->listAttrib;
	while( node ) {
		UserAttribute *attr = node->data;
		gchar *ida = attr->uid;
		if( ida ) {
			if( strcmp( ida, aid ) == 0 ) {
				attrib = attr;
			}
		}
		node = g_list_next( node );
	}
	return attrib;
}

/*
* Remove attribute from person.
* param: person Person object.
*        aid    Attribute ID.
* return: UserAttribute object, or NULL if not found. Note that object should still be freed.
*/
UserAttribute *addritem_person_remove_attrib_id( ItemPerson *person, const gchar *aid ) {
	UserAttribute *attrib = NULL;
	GList *node;

	g_return_val_if_fail( person != NULL, NULL );
	if( aid == NULL || *aid == '\0' ) return NULL;

	/* Look for attribute */
	node = person->listAttrib;
	while( node ) {
		UserAttribute *attr = node->data;
		gchar *ida = attr->uid;
		if( ida ) {
			if( strcmp( ida, aid ) == 0 ) {
				attrib = attr;
			}
		}
		node = g_list_next( node );
	}

	/* Remove email from person's address list */
	if( person->listAttrib ) {
		person->listAttrib = g_list_remove( person->listAttrib, attrib );
	}
	return attrib;
}

/*
* Remove attribute from person.
* param: person	Person.
*        attrib	Attribute to remove.
* return: UserAttribute object. Note that object should still be freed.
*/
UserAttribute *addritem_person_remove_attribute( ItemPerson *person, UserAttribute *attrib ) {
	gboolean found = FALSE;
	GList *node;

	g_return_val_if_fail( person != NULL, NULL );
	if( attrib == NULL ) return NULL;

	/* Look for attribute */
	node = person->listAttrib;
	while( node ) {
		if( node-> data == attrib ) {
			found = TRUE;
			break;
		}
		node = g_list_next( node );
	}

	if( found ) {
		/* Remove attribute */
		if( person->listAttrib ) {
			person->listAttrib = g_list_remove( person->listAttrib, attrib );
		}
	}
	return attrib;
}

/*
* Create new address book group.
*/
ItemGroup *addritem_create_item_group( void ) {
	ItemGroup *group;

	group = g_new0( ItemGroup, 1 );
	ADDRITEM_TYPE(group) = ITEMTYPE_GROUP;
	ADDRITEM_ID(group) = NULL;
	ADDRITEM_NAME(group) = NULL;
	ADDRITEM_PARENT(group) = NULL;
	ADDRITEM_SUBTYPE(group) = 0;
	group->remarks = NULL;
	group->listEMail = NULL;
	return group;
}

/*
* Specify name to be used.
*/
void addritem_group_set_id( ItemGroup *group, const gchar *value ) {
	ADDRITEM_ID(group) = mgu_replace_string( ADDRITEM_ID(group), value );
}
void addritem_group_set_name( ItemGroup *group, const gchar *value ) {
	ADDRITEM_NAME(group) = mgu_replace_string( ADDRITEM_NAME(group), value );
}
void addritem_group_set_remarks( ItemGroup *group, const gchar *value ) {
	group->remarks = mgu_replace_string( group->remarks, value );
}

/*
* Free address group.
*/
void addritem_free_item_group( ItemGroup *group ) {
	g_return_if_fail( group != NULL );

	/* Free internal stuff */
	g_free( ADDRITEM_ID(group) );
	g_free( ADDRITEM_NAME(group) );
	g_free( group->remarks );
	mgu_clear_list( group->listEMail );
	g_list_free( group->listEMail );

	ADDRITEM_TYPE(group) = ITEMTYPE_NONE;
	ADDRITEM_ID(group) = NULL;
	ADDRITEM_NAME(group) = NULL;
	ADDRITEM_PARENT(group) = NULL;
	ADDRITEM_SUBTYPE(group) = 0;
	group->remarks = NULL;
	group->listEMail = NULL;

	g_free( group );
}

/*
* Add EMail address to group.
* return: TRUE if item added.
*/
gboolean addritem_group_add_email( ItemGroup *group, ItemEMail *email ) {
	GList *node;

	g_return_val_if_fail( group != NULL, FALSE );
	g_return_val_if_fail( email != NULL, FALSE );

	node = group->listEMail;
	while( node ) {
		if( node->data == email ) return FALSE;
		node = g_list_next( node );
	}
	group->listEMail = g_list_append( group->listEMail, email );
	return TRUE;
}

/*
* Remove email address for specified group.
* param: group	Group from which to remove address.
*        email	EMail to remove
* return: EMail object, or NULL if email not found in group. Note that this object is
* referenced (linked) to a group and should *NOT* be freed. This object should only be
* freed after removing from a person.
*/
ItemEMail *addritem_group_remove_email( ItemGroup *group, ItemEMail *email ) {
	if( group && email ) {
		GList *node = group->listEMail;
		while( node ) {
			if( node->data == email ) {
				group->listEMail = g_list_remove( group->listEMail, email );
				return email;
			}
			node = g_list_next( node );
		}
	}
	return NULL;
}

/*
* Remove email address for specified group and ID.
* param: group	Group from which to remove address.
*        eid	EMail ID.
* return: EMail object, or NULL if email not found in group. Note that this object is
* referenced (linked) to a group and should *NOT* be freed. This object should only be
* freed after removing from a person.
*/
ItemEMail *addritem_group_remove_email_id( ItemGroup *group, const gchar *eid ) {
	if( group ) {
		GList *node = group->listEMail;
		while( node ) {
			ItemEMail *email = ( ItemEMail * ) node->data;
			if( strcmp( ADDRITEM_ID( email ), eid ) == 0 ) {
				group->listEMail = g_list_remove( group->listEMail, email );
				return email;
			}
			node = g_list_next( node );
		}
	}
	return NULL;
}

/*
* Print address group item.
*/
void addritem_print_item_group( ItemGroup *group, FILE *stream ) {
	GList *node;
	ItemPerson *person;
	ItemEMail *item;
	g_return_if_fail( group != NULL );
	fprintf( stream, "Group:\n" );
	fprintf( stream, "\tt/u: %d : '%s'\n", ADDRITEM_TYPE(group), ADDRITEM_ID(group) );
	fprintf( stream, "\tsub: %d\n", ADDRITEM_SUBTYPE(group) );
	fprintf( stream, "\tgrp: '%s'\n", ADDRITEM_NAME(group) );
	fprintf( stream, "\trem: '%s'\n", group->remarks );
	fprintf( stream, "\t---\n" );
	node = group->listEMail;
	while( node ) {
		item = node->data;
		person = ( ItemPerson * ) ADDRITEM_PARENT(item);
		if( person ) {
			fprintf( stream, "\t\tpid : '%s'\n", ADDRITEM_ID(person) );
			fprintf( stream, "\t\tcomn: '%s'\n", ADDRITEM_NAME(person) );
		}
		else {
			fprintf( stream, "\t\tpid : ???\n" );
			fprintf( stream, "\t\tcomn: ???\n" );
		}
		addritem_print_item_email( item, stream );
		node = g_list_next( node );
	}
	fprintf( stream, "\t***\n" );
}

/*
* Create new address folder.
*/
ItemFolder *addritem_create_item_folder( void ) {
	ItemFolder *folder;
	folder = g_new0( ItemFolder, 1 );
	ADDRITEM_TYPE(folder) = ITEMTYPE_FOLDER;
	ADDRITEM_ID(folder) = NULL;
	ADDRITEM_NAME(folder) = NULL;
	ADDRITEM_PARENT(folder) = NULL;
	ADDRITEM_SUBTYPE(folder) = 0;
	folder->remarks = NULL;
	folder->isRoot = FALSE;
	folder->listItems = NULL;
	folder->listFolder = NULL;
	folder->listPerson = NULL;
	folder->listGroup = NULL;
	folder->userData = NULL;
	return folder;
}

/*
* Specify name to be used.
*/
void addritem_folder_set_id( ItemFolder *folder, const gchar *value ) {
	ADDRITEM_ID(folder) = mgu_replace_string( ADDRITEM_ID(folder), value );
}
void addritem_folder_set_name( ItemFolder *folder, const gchar *value ) {
	ADDRITEM_NAME(folder) = mgu_replace_string( ADDRITEM_NAME(folder), value );
}
void addritem_folder_set_remarks( ItemFolder *folder, const gchar *value ) {
	folder->remarks = mgu_replace_string( folder->remarks, value );
}

/*
* Free address folder. Note: this does not free up the lists of children
* (folders, groups and person). This should be done prior to calling this
* function.
*/
void addritem_free_item_folder( ItemFolder *folder ) {
	g_return_if_fail( folder != NULL );

	/* Free internal stuff */
	g_free( ADDRITEM_ID(folder) );
	g_free( ADDRITEM_NAME(folder) );
	g_free( folder->remarks );
	mgu_clear_list( folder->listItems );
	g_list_free( folder->listItems );

	ADDRITEM_TYPE(folder) = ITEMTYPE_NONE;
	ADDRITEM_ID(folder) = NULL;
	ADDRITEM_NAME(folder) = NULL;
	ADDRITEM_PARENT(folder) = NULL;
	ADDRITEM_SUBTYPE(folder) = 0;
	folder->isRoot = FALSE;
	folder->remarks = NULL;
	folder->listItems = NULL;
	folder->listFolder = NULL;
	folder->listGroup = NULL;
	folder->listPerson = NULL;

	g_free( folder->userData );
	folder->userData = NULL;

	g_free( folder );
}

/*
* Free up folders recursively. Note: this does not free up the lists of children
* (folders, groups and person). This should be done prior to calling this
* function.
*/
void addritem_free_item_folder_recurse( ItemFolder *parent ) {
	GList *node = parent->listFolder;

	while( node ) {
		ItemFolder *folder = node->data;
		addritem_free_item_folder_recurse( folder );
		node = g_list_next( node );
	}
	g_list_free( parent->listPerson );
	g_list_free( parent->listGroup );
	g_list_free( parent->listFolder );
	parent->listPerson = NULL;
	parent->listGroup = NULL;
	parent->listFolder = NULL;
}

/*
* Free up list of person in specified folder.
*/
void addritem_folder_free_person( ItemFolder *folder ) {
	GList *node;

	g_return_if_fail( folder != NULL );
	
	/* Free up folder of persons. */
	node = folder->listPerson;
	while( node ) {
		ItemPerson *person = node->data;
		addritem_free_item_person( person );
		person = NULL;
		node = g_list_next( node );
	}
}

/*
* Add person into folder.
* return: TRUE if person added.
*/
gboolean addritem_folder_add_person( ItemFolder *folder, ItemPerson *item ) {
	g_return_val_if_fail( folder != NULL, FALSE );
	g_return_val_if_fail( item != NULL, FALSE );

	folder->listPerson = g_list_append( folder->listPerson, item );
	ADDRITEM_PARENT(item) = ADDRITEM_OBJECT(folder);
	return TRUE;
}

/*
* Add folder into folder.
* return: TRUE if folder added.
*/
gboolean addritem_folder_add_folder( ItemFolder *folder, ItemFolder *item ) {
	g_return_val_if_fail( folder != NULL, FALSE );
	g_return_val_if_fail( item != NULL, FALSE );

	folder->listFolder = g_list_append( folder->listFolder, item );
	ADDRITEM_PARENT(item) = ADDRITEM_OBJECT(folder);
	return TRUE;
}

/*
* Add group into folder.
* return: TRUE if folder added.
*/
gboolean addritem_folder_add_group( ItemFolder *folder, ItemGroup *item ) {
	g_return_val_if_fail( folder != NULL, FALSE );
	g_return_val_if_fail( item != NULL, FALSE );

	folder->listGroup = g_list_append( folder->listGroup, item );
	ADDRITEM_PARENT(item) = ADDRITEM_OBJECT(folder);
	return TRUE;
}

/*
* Print address folder item.
*/
void addritem_print_item_folder( ItemFolder *folder, FILE *stream ) {
	GList *node;
	/* ItemPerson *person; */
	ItemFolder *parent;

	g_return_if_fail( folder != NULL );

	fprintf( stream, "Folder:\n" );
	fprintf( stream, "\tt/u: %d : '%s'\n", ADDRITEM_TYPE(folder), ADDRITEM_ID(folder) );
	fprintf( stream, "\tsub: %d\n", ADDRITEM_SUBTYPE(folder) );
	fprintf( stream, "\tnam: '%s'\n", ADDRITEM_NAME(folder) );
	fprintf( stream, "\trem: '%s'\n", folder->remarks );
	fprintf( stream, "\t---\n" );
	parent = ( ItemFolder * ) ADDRITEM_PARENT(folder);
	if( parent ) {
		fprintf( stream, "\tpar: %s : %s\n", ADDRITEM_ID(parent), ADDRITEM_NAME(parent) );
	}
	else {
		fprintf( stream, "\tpar: NULL\n" );
	}
	node = folder->listFolder;
	while( node ) {
		AddrItemObject *aio = node->data;
		if( aio ) {
			if( aio->type == ITEMTYPE_FOLDER ) {
				ItemFolder *item = ( ItemFolder * ) aio;
				addritem_print_item_folder( item, stream );
			}
		}
		else {
			fprintf( stream, "\t\tpid : ???\n" );
		}

		node = g_list_next( node );
	}

	node = folder->listPerson;
	while( node ) {
		AddrItemObject *aio = node->data;
		if( aio ) {
			if( aio->type == ITEMTYPE_PERSON ) {
				ItemPerson *item = ( ItemPerson * ) aio;
				addritem_print_item_person( item, stream );
			}
		}
		else {
			fprintf( stream, "\t\tpid : ???\n" );
		}

		node = g_list_next( node );
	}

	node = folder->listGroup;
	while( node ) {
		AddrItemObject *aio = node->data;
		if( aio ) {
			if( aio->type == ITEMTYPE_GROUP ) {
				ItemGroup *item = ( ItemGroup * ) aio;
				addritem_print_item_group( item, stream );
			}
		}
		else {
			fprintf( stream, "\t\tpid : ???\n" );
		}
		node = g_list_next( node );
	}
	fprintf( stream, "\t###\n" );
}

/*
* Return link list of persons for specified folder. Note that the list contains
* references to items and should be g_free() when done. Do *NOT* attempt to use the
* addritem_free_xxx() functions... this will destroy the addressbook data!
* Return: List of items, or NULL if none.
*/
GList *addritem_folder_get_person_list( ItemFolder *folder ) {
	GList *list = NULL;
	GList *node = NULL;

	g_return_val_if_fail( folder != NULL, NULL );

	node = folder->listPerson;
	while( node ) {
		ItemPerson *person = node->data;
		list = g_list_append( list, person );
		node = g_list_next( node );
	}
	return list;
}

/*
* Return link list of groups for specified folder. Note that the list contains
* references to items and should be g_free() when done. Do *NOT* attempt to use the
* addritem_free_xxx() functions... this will destroy the addressbook data!
* Return: List of items, or NULL if none.
*/
GList *addritem_folder_get_group_list( ItemFolder *folder ) {
	GList *list = NULL;
	GList *node = NULL;

	g_return_val_if_fail( folder != NULL, NULL );

	node = folder->listGroup;
	while( node ) {
		ItemGroup *group = node->data;
		list = g_list_append( list, group );
		node = g_list_next( node );
	}
	return list;
}

/*
* Move person's email item.
* param: person     Person.
*        itemMove   Item to move.
*        itemTarget Target item before which to move item.
*/

ItemEMail *addritem_move_email_before( ItemPerson *person, ItemEMail *itemMove, ItemEMail *itemTarget ) {
	gint posT, posM;

	g_return_val_if_fail( person != NULL, NULL );

	if( itemTarget == NULL ) return NULL;
	if( itemMove == NULL ) return NULL;
	if( itemMove == itemTarget ) return itemMove;

	posT = g_list_index( person->listEMail, itemTarget );
	if( posT < 0 ) return NULL;
	posM = g_list_index( person->listEMail, itemMove );
	if( posM < 0 ) return NULL;
	person->listEMail = g_list_remove( person->listEMail, itemMove );
	person->listEMail = g_list_insert( person->listEMail, itemMove, posT );
	return itemMove;
}

/*
* Move person's email item.
* param: person     Person.
*        itemMove   Item to move.
*        itemTarget Target item after which to move item.
*/
ItemEMail *addritem_move_email_after( ItemPerson *person, ItemEMail *itemMove, ItemEMail *itemTarget ) {
	gint posT, posM;

	g_return_val_if_fail( person != NULL, NULL );

	if( itemTarget == NULL ) return NULL;
	if( itemMove == NULL ) return NULL;
	if( itemMove == itemTarget ) return itemMove;

	posT = g_list_index( person->listEMail, itemTarget );
	if( posT < 0 ) return NULL;
	posM = g_list_index( person->listEMail, itemMove );
	if( posM < 0 ) return NULL;
	person->listEMail = g_list_remove( person->listEMail, itemMove );
	person->listEMail = g_list_insert( person->listEMail, itemMove, 1+posT );
	return itemMove;
}

/*
* End of Source.
*/
