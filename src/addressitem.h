/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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
 * Address item data. Shared among GUI components only.
 */

#ifndef __ADDRESSITEM_H__
#define __ADDRESSITEM_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkctree.h>

#include "compose.h"
#include "addrindex.h"

#define ADDRESS_OBJECT(obj)		((AddressObject *)obj)
#define ADDRESS_OBJECT_TYPE(obj)	(ADDRESS_OBJECT(obj)->type)
#define ADDRESS_OBJECT_NAME(obj)	(ADDRESS_OBJECT(obj)->name)

#define ADAPTER_INTERFACE(obj)		((AdapterInterface *)obj)
#define ADAPTER_FOLDER(obj)		((AdapterFolder *)obj)
#define ADAPTER_GROUP(obj)		((AdapterGroup *)obj)
#define ADAPTER_DSOURCE(obj)		((AdapterDSource *)obj)

typedef enum {
	ADDR_NONE,
	ADDR_ITEM_PERSON,
	ADDR_ITEM_EMAIL,
	ADDR_ITEM_FOLDER,
	ADDR_ITEM_GROUP,
	ADDR_INTERFACE,
	ADDR_DATASOURCE,
	ADDR_BOOK,		/* Sub-type */
	ADDR_VCARD,		/* Sub-type */
	ADDR_JPILOT,		/* Sub-type */
	ADDR_CATEGORY,		/* Sub-type */
	ADDR_LDAP		/* Sub-type */
} AddressObjectType;

typedef struct _AddressBook_win	AddressBook_win;
struct _AddressBook_win
{
	GtkWidget *window;
	GtkWidget *menubar;
	GtkWidget *treeview;
	GtkWidget *listview;
	GtkWidget *entry;
	GtkWidget *statusbar;

	GtkWidget *to_btn;
	GtkWidget *cc_btn;
	GtkWidget *bcc_btn;
	GtkWidget *del_btn;
	GtkWidget *reg_btn;
	GtkWidget *lup_btn;
	GtkWidget *close_btn;

	GtkWidget *tree_popup;
	GtkWidget *list_popup;
	GtkItemFactory *tree_factory;
	GtkItemFactory *list_factory;
	GtkItemFactory *menu_factory;

	GtkTreeRowReference *tree_selected;
	GtkTreeRowReference *tree_opened;
	GtkTreeRowReference *list_selected;

	Compose *target_compose;
	gint status_cid;
};

typedef struct _AddressTypeControlItem	AddressTypeControlItem;
struct _AddressTypeControlItem {
	AddressObjectType objectType;
	AddressIfType interfaceType;
	gchar *displayName;
	gboolean showInTree;
	gboolean treeExpand;
	gboolean treeLeaf;
	gchar *menuCommand;
	GdkPixbuf *icon_pixbuf;
	GdkPixbuf *icon_open_pixbuf;
};

typedef struct _AddressObject	AddressObject;
struct _AddressObject {
	AddressObjectType type;
	gchar *name;
};

typedef struct _AdapterInterface AdapterInterface;
struct _AdapterInterface {
	AddressObject obj;
	AddressInterface *iface;
	AddressIfType interfaceType;
	AddressTypeControlItem *atci;
	gboolean enabled;
	gboolean haveLibrary;
	GtkTreeRowReference *tree_row;
};

typedef struct _AdapterDSource AdapterDSource;
struct _AdapterDSource {
	AddressObject obj;
	AddressDataSource *dataSource;
	AddressObjectType subType;
};

typedef struct _AdapterFolder AdapterFolder;
struct _AdapterFolder {
	AddressObject obj;
	ItemFolder *itemFolder;
};

typedef struct _AdapterGroup AdapterGroup;
struct _AdapterGroup {
	AddressObject obj;
	ItemGroup *itemGroup;
};

typedef struct _AddressFileSelection AddressFileSelection;
struct _AddressFileSelection {
	GtkWidget *fileSelector;
	gboolean cancelled;
};

AdapterDSource *addressbook_create_ds_adapter	( AddressDataSource	*ds,
						  AddressObjectType	otype,
						  gchar			*name );

void addressbook_ads_set_name			( AdapterDSource 	*adapter,
						  gchar *value );

#endif /* __ADDRESSITEM_H__ */

/*
* End of Source.
*/

