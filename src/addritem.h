/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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
 * Address item data.
 */

#ifndef __ADDRITEM_H__
#define __ADDRITEM_H__

#include <stdio.h>
#include <glib.h>

#define ADDRITEM_OBJECT(obj)	((AddrItemObject *)obj)
#define ADDRITEM_TYPE(obj)	(ADDRITEM_OBJECT(obj)->type)
#define ADDRITEM_NAME(obj)	(ADDRITEM_OBJECT(obj)->name)
#define ADDRITEM_ID(obj)	(ADDRITEM_OBJECT(obj)->uid)
#define ADDRITEM_PARENT(obj)	(ADDRITEM_OBJECT(obj)->parent)
#define ADDRITEM_SUBTYPE(obj)	(ADDRITEM_OBJECT(obj)->subType)

typedef enum {
	ITEMTYPE_NONE,
	ITEMTYPE_PERSON,
	ITEMTYPE_EMAIL,
	ITEMTYPE_FOLDER,
	ITEMTYPE_GROUP,
	ITEMTYPE_INDEX,
	ITEMTYPE_INTERFACE,
	ITEMTYPE_DATASOURCE
} ItemObjectType;

typedef struct _AddrItemObject AddrItemObject;
struct _AddrItemObject {
	ItemObjectType type;
	gchar          *name;
	gchar          *uid;
	AddrItemObject *parent;
	gint           subType;
};

typedef struct _ItemPerson ItemPerson;
struct _ItemPerson {
	AddrItemObject obj;
	gchar    *firstName;
	gchar    *lastName;
	gchar    *nickName;
	gchar    *externalID;
	GList    *listEMail;
	GList    *listAttrib;
	gboolean isOpened;
};

typedef struct _ItemEMail ItemEMail;
struct _ItemEMail {
	AddrItemObject obj;
	gchar *address;
	gchar *remarks;
};

typedef struct _UserAttribute UserAttribute;
struct _UserAttribute {
	gchar *uid;
	gchar *name;
	gchar *value;
};

typedef struct _ItemFolder ItemFolder;
struct _ItemFolder {
	AddrItemObject obj;
	gchar    *remarks;
	gboolean isRoot;
	GList    *listItems;
	GList    *listFolder;
	GList    *listPerson;
	GList    *listGroup;
	gpointer userData;
};

typedef struct _ItemGroup ItemGroup;
struct _ItemGroup {
	AddrItemObject obj;
	gchar *remarks;
	GList *listEMail;
};

/* Function prototypes */
ItemEMail *addritem_create_item_email	( void );
ItemEMail *addritem_copy_item_email	( ItemEMail *item );
void addritem_email_set_id		( ItemEMail *email, const gchar *value );
void addritem_email_set_alias		( ItemEMail *email, const gchar *value );
void addritem_email_set_address		( ItemEMail *email, const gchar *value );
void addritem_email_set_remarks		( ItemEMail *email, const gchar *value );
void addritem_free_item_email		( ItemEMail *item );

UserAttribute *addritem_create_attribute( void );
UserAttribute *addritem_copy_attribute	( UserAttribute *item );
void addritem_attrib_set_id		( UserAttribute *item, const gchar *value );
void addritem_attrib_set_name		( UserAttribute *item, const gchar *value );
void addritem_attrib_set_value		( UserAttribute *item, const gchar *value );
void addritem_free_attribute		( UserAttribute *item );

ItemPerson *addritem_create_item_person	( void );
void addritem_person_set_id		( ItemPerson *person, const gchar *value );
void addritem_person_set_first_name	( ItemPerson *person, const gchar *value );
void addritem_person_set_last_name	( ItemPerson *person, const gchar *value );
void addritem_person_set_nick_name	( ItemPerson *person, const gchar *value );
void addritem_person_set_common_name	( ItemPerson *person, const gchar *value );
void addritem_person_set_external_id	( ItemPerson *person, const gchar *value );
void addritem_person_set_opened		( ItemPerson *person, const gboolean value );
void addritem_free_item_person		( ItemPerson *person );
void addritem_free_list_email		( GList *list );
void addritem_free_list_attribute	( GList *list );

ItemGroup *addritem_create_item_group	( void );
void addritem_free_item_group		( ItemGroup *group );
void addritem_print			( ItemGroup *group, FILE *stream );
void addritem_group_set_id		( ItemGroup *group, const gchar *value );
void addritem_group_set_name		( ItemGroup *group, const gchar *value );
void addritem_group_set_remarks		( ItemGroup *group, const gchar *value );

void addritem_print_item_email		( ItemEMail *item, FILE *stream );
void addritem_print_attribute		( UserAttribute *item, FILE *stream );
void addritem_print_item_person		( ItemPerson *person, FILE *stream );
void addritem_print_item_group		( ItemGroup *group, FILE *stream );
void addritem_print_item_folder		( ItemFolder *folder, FILE *stream );

gboolean addritem_person_add_email		( ItemPerson *person, ItemEMail *email );
ItemEMail *addritem_person_get_email		( ItemPerson *person, const gchar *eid );
ItemEMail *addritem_person_remove_email_id	( ItemPerson *person, const gchar *eid );
ItemEMail *addritem_person_remove_email		( ItemPerson *person, ItemEMail *email );

void addritem_person_add_attribute		( ItemPerson *person, UserAttribute *attrib );
UserAttribute *addritem_person_get_attribute	( ItemPerson *person, const gchar *aid );
UserAttribute *addritem_person_remove_attrib_id	( ItemPerson *person, const gchar *aid );
UserAttribute *addritem_person_remove_attribute	( ItemPerson *person, UserAttribute *attrib );

ItemFolder *addritem_create_item_folder	( void );
void addritem_folder_set_id		( ItemFolder *folder, const gchar *value );
void addritem_folder_set_name		( ItemFolder *folder, const gchar *value );
void addritem_folder_set_remarks	( ItemFolder *folder, const gchar *value );
void addritem_free_item_folder		( ItemFolder *folder );
void addritem_free_item_folder_recurse	( ItemFolder *parent );

gboolean addritem_group_add_email		( ItemGroup *group, ItemEMail *email );
ItemEMail *addritem_group_remove_email		( ItemGroup *group, ItemEMail *email );
ItemEMail *addritem_group_remove_email_id	( ItemGroup *group, const gchar *eid );

gboolean addritem_folder_add_person	( ItemFolder *folder, ItemPerson *item );
gboolean addritem_folder_add_folder	( ItemFolder *folder, ItemFolder *item );
gboolean addritem_folder_add_group	( ItemFolder *folder, ItemGroup *item );
void addritem_folder_free_person	( ItemFolder *folder );
GList *addritem_folder_get_person_list	( ItemFolder *folder );
GList *addritem_folder_get_group_list	( ItemFolder *folder );

ItemEMail *addritem_move_email_before	( ItemPerson *person, ItemEMail *itemMove, ItemEMail *itemTarget );
ItemEMail *addritem_move_email_after	( ItemPerson *person, ItemEMail *itemMove, ItemEMail *itemTarget );

#endif /* __ADDRITEM_H__ */
