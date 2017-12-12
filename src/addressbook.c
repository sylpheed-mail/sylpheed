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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>
#include <setjmp.h>

#include "main.h"
#include "addressbook.h"
#include "manage_window.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "xml.h"
#include "prefs.h"
#include "procmime.h"
#include "utils.h"
#include "gtkutils.h"
#include "codeconv.h"
#include "about.h"
#include "addr_compl.h"

#include "mgutils.h"
#include "addressitem.h"
#include "addritem.h"
#include "addrcache.h"
#include "addrbook.h"
#include "addrindex.h"
#include "addressadd.h"
#include "vcard.h"
#include "editvcard.h"
#include "editgroup.h"
#include "editaddress.h"
#include "editbook.h"
#include "ldif.h"
#include "importldif.h"
#include "importcsv.h"
#include "exportcsv.h"

#ifdef USE_JPILOT
#include "jpilot.h"
#include "editjpilot.h"
#endif

#ifdef USE_LDAP
#include <pthread.h>
#include "syldap.h"
#include "editldap.h"

#define ADDRESSBOOK_LDAP_BUSYMSG "Busy"
#endif

typedef enum
{
	COL_FOLDER_NAME,
	COL_OBJ,
	COL_PIXBUF,
	COL_PIXBUF_OPEN,

	N_TREE_COLS
} AddressBookTreeColumnPos;

typedef enum
{
	COL_NAME,
	COL_ADDRESS,
	COL_NICKNAME,
	COL_REMARKS,
	COL_L_OBJ,
	COL_L_PIXBUF,

	N_LIST_COLS
} AddressBookListColumnPos;

enum
{
	DRAG_TYPE_OBJ,

	N_DRAG_TYPES
};

#define COL_NAME_WIDTH		164
#define COL_ADDRESS_WIDTH	156

#define COL_FOLDER_WIDTH	170

#define ADDRESSBOOK_MSGBUF_SIZE 2048

static GdkPixbuf *folderpix;
static GdkPixbuf *folderopenpix;
static GdkPixbuf *grouppix;
static GdkPixbuf *interfacepix;
static GdkPixbuf *bookpix;
static GdkPixbuf *personpix;
static GdkPixbuf *addresspix;
static GdkPixbuf *vcardpix;
static GdkPixbuf *jpilotpix;
static GdkPixbuf *categorypix;
static GdkPixbuf *ldappix;

/* Address list selection */
static GList *_addressListSelection_ = NULL;

static gboolean can_toggle_list_selection = TRUE;
static gboolean list_on_drag = FALSE;

/* Address index file and interfaces */
static AddressIndex *_addressIndex_ = NULL;
static GList *_addressInterfaceList_ = NULL;
static GList *_addressIFaceSelection_ = NULL;
#define ADDRESSBOOK_IFACE_SELECTION "1/y,3/y,4/y,2/n"

/* Address clipboard */
static GList *_clipObjectList_ = NULL;

static AddressBook_win addrbook;

static GHashTable *_addressBookTypeHash_ = NULL;
static GList *_addressBookTypeList_ = NULL;

static void addressbook_refresh			(void);
static void addressbook_reopen			(void);

static void addressbook_create			(void);
static gint addressbook_close			(void);

static void addressbook_menuitem_set_sensitive	(void);

/* callback functions */
static void addressbook_del_clicked		(GtkButton	*button,
						 gpointer	 data);
static void addressbook_reg_clicked		(GtkButton	*button,
						 gpointer	 data);
static void addressbook_to_clicked		(GtkButton	*button,
						 gpointer	 data);
static void addressbook_lup_clicked		(GtkButton	*button,
						 gpointer	data);
static void addressbook_close_clicked		(GtkButton	*button,
						 gpointer	data);

static void addressbook_tree_selection_changed	(GtkTreeSelection *selection,
						 gpointer	 data);
static void addressbook_list_selection_changed	(GtkTreeSelection *selection,
						 gpointer	 data);
static void addressbook_person_expand_node	(GtkTreeView	*treeview,
						 GtkTreeIter	*iter,
						 GtkTreePath	*path,
						 gpointer	*data);
static void addressbook_person_collapse_node	(GtkTreeView	*treeview,
						 GtkTreeIter	*iter,
						 GtkTreePath	*path,
						 gpointer	*data);

static void addressbook_drag_begin		(GtkWidget	*widget,
						 GdkDragContext	*drag_context,
						 gpointer	 data);
static void addressbook_drag_end		(GtkWidget	*widget,
						 GdkDragContext	*drag_context,
						 gpointer	 data);
static void addressbook_drag_data_get		(GtkWidget	*widget,
						 GdkDragContext	*drag_context,
						 GtkSelectionData *selection_data,
						 guint		 info,
						 guint		 time,
						 gpointer	 data);

#if 0
static void addressbook_entry_changed		(GtkWidget	*widget);
#endif
static void addressbook_entry_activated		(GtkWidget	*widget,
						 gpointer	 data);

static gboolean addressbook_list_button_pressed	(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
static gboolean addressbook_list_button_released(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
static gboolean addressbook_tree_button_pressed	(GtkWidget	*ctree,
						 GdkEventButton	*event,
						 gpointer	 data);
static gboolean addressbook_tree_button_released(GtkWidget	*ctree,
						 GdkEventButton	*event,
						 gpointer	 data);

static gboolean addressbook_drag_motion		(GtkWidget	*widget,
						 GdkDragContext	*context,
						 gint		 x,
						 gint		 y,
						 guint		 time,
						 gpointer	 data);
static void addressbook_drag_leave		(GtkWidget	*widget,
						 GdkDragContext	*context,
						 guint		 time,
						 gpointer	 data);
static void addressbook_drag_received		(GtkWidget	*widget,
						 GdkDragContext	*context,
						 gint		 x,
						 gint		 y,
						 GtkSelectionData *data,
						 guint		 info,
						 guint		 time,
						 gpointer	 user_data);

static void addressbook_folder_resized		(GtkWidget	*widget,
						 GtkAllocation	*allocation,
						 gpointer	 data);
static void addressbook_col_resized		(GtkWidget	*widget,
						 GtkAllocation	*allocation,
						 gpointer	 data);

static void addressbook_popup_close		(GtkMenuShell	*menu_shell,
						 gpointer	 data);

static void addressbook_new_folder_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_new_group_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_treenode_edit_cb	(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_treenode_delete_cb	(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);

static void addressbook_change_node_name	(GtkTreeIter	*iter,
						 const gchar	*name);

static void addressbook_new_address_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_compose_to_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_edit_address_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_delete_address_cb	(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_copy_address_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_paste_address_cb	(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);

static void close_cb				(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_file_save_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);

/* Data source edit stuff */
static void addressbook_new_book_cb		(gpointer	 data,
	       					 guint		 action,
						 GtkWidget	*widget);
static void addressbook_new_vcard_cb		(gpointer	 data,
	       					 guint		 action,
						 GtkWidget	*widget);

#ifdef USE_JPILOT
static void addressbook_new_jpilot_cb		(gpointer	 data,
	       					 guint		 action,
						 GtkWidget	*widget);
#endif

#ifdef USE_LDAP
static void addressbook_new_ldap_cb		(gpointer	 data,
	       					 guint		 action,
						 GtkWidget	*widget);
#endif

static void addressbook_set_list		(AddressObject	*obj);

static void addressbook_load_tree		(void);
void addressbook_read_file			(void);

static gboolean addressbook_add_object		(GtkTreeIter	*iter,
						 GtkTreeIter	*new_iter,
						 AddressObject	*obj);
static AddressDataSource *addressbook_find_datasource
						(GtkTreeIter	*iter);

static AddressBookFile *addressbook_get_book_file();

static gboolean addressbook_node_add_folder	(GtkTreeIter	*iter,
						 AddressDataSource *ds,
						 ItemFolder	*itemFolder,
						 AddressObjectType otype,
						 GtkTreeIter	*new_iter);
static gboolean addressbook_node_add_group	(GtkTreeIter	*iter,
						 AddressDataSource *ds,
						 ItemGroup	*itemGroup,
						 GtkTreeIter	*new_iter);
static void addressbook_tree_remove_children	(GtkTreeModel	*model,
						 GtkTreeIter	*parent);
static void addressbook_move_nodes_up		(GtkTreeIter	*iter);
static gboolean addressbook_find_group_node	(GtkTreeIter	*parent,
						 GtkTreeIter	*iter,
						 ItemGroup	*group);

/* static void addressbook_delete_object	(AddressObject	*obj); */

static gboolean key_pressed			(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void size_allocated			(GtkWidget	*widget,
						 GtkAllocation	*allocation,
						 gpointer	 data);

static gint addressbook_tree_compare		(GtkTreeModel	*model,
						 GtkTreeIter	*a,
						 GtkTreeIter	*b,
						 gpointer	 data);
static gint addressbook_list_name_compare	(GtkTreeModel	*model,
						 GtkTreeIter	*a,
						 GtkTreeIter	*b,
						 gpointer	 data);
static gint addressbook_list_address_compare	(GtkTreeModel	*model,
						 GtkTreeIter	*a,
						 GtkTreeIter	*b,
						 gpointer	 data);
static gint addressbook_list_nickname_compare	(GtkTreeModel	*model,
						 GtkTreeIter	*a,
						 GtkTreeIter	*b,
						 gpointer	 data);
static gint addressbook_list_remarks_compare	(GtkTreeModel	*model,
						 GtkTreeIter	*a,
						 GtkTreeIter	*b,
						 gpointer	 data);
 
static gboolean addressbook_list_select_func	(GtkTreeSelection *selection,
						 GtkTreeModel *model,
						 GtkTreePath *path,
						 gboolean cur_selected,
						 gpointer data);

/* static void addressbook_book_show_message	(AddressBookFile *book); */
/* static void addressbook_vcard_show_message	(VCardFile *vcf); */
#ifdef USE_JPILOT
/* static void addressbook_jpilot_show_message	(JPilotFile *jpf); */
#endif
#ifdef USE_LDAP
static void addressbook_ldap_show_message	(SyldapServer *server);
#endif

/* LUT's and IF stuff */
static void addressbook_free_adapter		(GtkTreeIter	*iter);
static void addressbook_free_child_adapters	(GtkTreeIter	*iter);
AddressTypeControlItem *addrbookctl_lookup	(gint		 ot);
AddressTypeControlItem *addrbookctl_lookup_iface(AddressIfType	 ifType);

void addrbookctl_build_map			(GtkWidget	*window);
void addrbookctl_build_iflist			(void);
AdapterInterface *addrbookctl_find_interface	(AddressIfType	 ifType);
void addrbookctl_build_ifselect			(void);

static void addrbookctl_free_interface		(AdapterInterface *adapter);
static void addrbookctl_free_datasource		(AdapterDSource	  *adapter);
static void addrbookctl_free_folder		(AdapterFolder	  *adapter);
static void addrbookctl_free_group		(AdapterGroup	  *adapter);

static void addressbook_list_select_clear	(void);
static void addressbook_list_select_remove	(AddressObject	*obj);
static void addressbook_list_select_set		(GList		*row_list);

static void addressbook_import_ldif_cb		(void);
static void addressbook_import_csv_cb		(void);

static void addressbook_export_csv_cb		(void);

static void addressbook_modified		(void);


static GtkTargetEntry addressbook_drag_types[] =
{
	{"application/octet-stream", GTK_TARGET_SAME_APP, DRAG_TYPE_OBJ}
};

static GtkItemFactoryEntry addressbook_entries[] =
{
	{N_("/_File"),			NULL,	NULL, 0, "<Branch>"},
	{N_("/_File/New _Book"),	NULL,	addressbook_new_book_cb,        0, NULL},
	{N_("/_File/New _vCard"),	NULL,	addressbook_new_vcard_cb,       0, NULL},
#ifdef USE_JPILOT
	{N_("/_File/New _JPilot"),	NULL,	addressbook_new_jpilot_cb,      0, NULL},
#endif
#ifdef USE_LDAP
	{N_("/_File/New _LDAP Server"),	NULL,	addressbook_new_ldap_cb,        0, NULL},
#endif
	{N_("/_File/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_File/_Edit"),		NULL,		addressbook_treenode_edit_cb,   0, NULL},
	{N_("/_File/_Delete"),		NULL,		addressbook_treenode_delete_cb, 0, NULL},
	{N_("/_File/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_File/_Save"),		"<control>S",	addressbook_file_save_cb,       0, NULL},
	{N_("/_File/_Close"),		"<control>W",	close_cb, 0, NULL},

	{N_("/_Edit"),			NULL,		NULL, 0, "<Branch>"},
	{N_("/_Edit/_Copy"),		"<control>C",	addressbook_copy_address_cb,    0, NULL},
	{N_("/_Edit/_Paste"),		"<control>V",	addressbook_paste_address_cb,    0, NULL},

	{N_("/_Address"),		NULL,		NULL, 0, "<Branch>"},
	{N_("/_Address/New _Address"),	"<control>N",	addressbook_new_address_cb,     0, NULL},
	{N_("/_Address/New _Group"),	"<control>G",	addressbook_new_group_cb,       0, NULL},
	{N_("/_Address/New _Folder"),	"<control>F",	addressbook_new_folder_cb,      0, NULL},
	{N_("/_Address/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_Address/Add _to recipient"),
					"<control>M",	addressbook_compose_to_cb, COMPOSE_ENTRY_TO, NULL},
	{N_("/_Address/Add to _Cc"),
					NULL,		addressbook_compose_to_cb, COMPOSE_ENTRY_CC, NULL},
	{N_("/_Address/Add to _Bcc"),
					NULL,		addressbook_compose_to_cb, COMPOSE_ENTRY_BCC, NULL},
	{N_("/_Address/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_Address/_Edit"),		"<control>Return",	addressbook_edit_address_cb,    0, NULL},
	{N_("/_Address/_Delete"),	"Delete",	addressbook_delete_address_cb,  0, NULL},

	{N_("/_Tools"),			NULL,		NULL, 0, "<Branch>"},
	{N_("/_Tools/Import _LDIF file"), NULL,		addressbook_import_ldif_cb,	0, NULL},
	{N_("/_Tools/Import _CSV file"), NULL,		addressbook_import_csv_cb,	0, NULL},
	{N_("/_Tools/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_Tools/Export to C_SV file"), NULL,	addressbook_export_csv_cb,	0, NULL}, 
	{N_("/_Help"),			NULL,		NULL, 0, "<Branch>"},
	{N_("/_Help/_About"),		NULL,		about_show, 0, NULL}
};

/* New options to be added. */
/*
	{N_("/_Edit"),			NULL,		NULL, 0, "<Branch>"},
	{N_("/_Edit/C_ut"),		"<control>X",	NULL,				0, NULL},
	{N_("/_Tools"),			NULL,		NULL, 0, "<Branch>"},
	{N_("/_Tools/Import _Mozilla"),	NULL,           NULL,				0, NULL},
	{N_("/_Tools/Import _vCard"),	NULL,           NULL,				0, NULL},
	{N_("/_Tools/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_Tools/Export _LDIF file"), NULL,		NULL,				0, NULL},
	{N_("/_Tools/Export v_Card"),	NULL,           NULL,				0, NULL},
*/

static GtkItemFactoryEntry addressbook_tree_popup_entries[] =
{
	{N_("/New _Address"),	NULL, addressbook_new_address_cb, 0, NULL},
	{N_("/New _Group"),	NULL, addressbook_new_group_cb,   0, NULL},
	{N_("/New _Folder"),	NULL, addressbook_new_folder_cb,  0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit"),		NULL, addressbook_treenode_edit_cb,   0, NULL},
	{N_("/_Delete"),	NULL, addressbook_treenode_delete_cb, 0, NULL}
};

static GtkItemFactoryEntry addressbook_list_popup_entries[] =
{
	{N_("/New _Address"),	NULL, addressbook_new_address_cb,  0, NULL},
	{N_("/New _Group"),	NULL, addressbook_new_group_cb,    0, NULL},
	{N_("/New _Folder"),	NULL, addressbook_new_folder_cb,   0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/Add _to recipient"),
				NULL, addressbook_compose_to_cb, COMPOSE_ENTRY_TO, NULL},
	{N_("/Add t_o Cc"),
				NULL, addressbook_compose_to_cb, COMPOSE_ENTRY_CC, NULL},
	{N_("/Add to _Bcc"),
				NULL, addressbook_compose_to_cb, COMPOSE_ENTRY_BCC, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit"),		NULL, addressbook_edit_address_cb,   0, NULL},
	{N_("/_Delete"),	NULL, addressbook_delete_address_cb, 0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Copy"),		NULL, addressbook_copy_address_cb,  0, NULL},
	{N_("/_Paste"),		NULL, addressbook_paste_address_cb, 0, NULL}
};


void addressbook_open(Compose *target)
{
	if (!addrbook.window) {
		GtkTreeView *treeview;
		GtkTreeModel *model;
		GtkTreeIter iter;
		GtkTreePath *path;

		addressbook_read_file();
		addressbook_create();
		addressbook_load_tree();
		treeview = GTK_TREE_VIEW(addrbook.treeview);
		model = gtk_tree_view_get_model(treeview);
		if (gtk_tree_model_get_iter_first(model, &iter)) {
			path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
			gtk_tree_path_free(path);
		}
		addressbook_menuitem_set_sensitive();
		gtk_widget_show(addrbook.window);
	} 

	gtk_window_present(GTK_WINDOW(addrbook.window));

	addressbook_set_target_compose(target);
}

void addressbook_set_target_compose(Compose *target)
{
	addrbook.target_compose = target;
}

Compose *addressbook_get_target_compose(void)
{
	return addrbook.target_compose;
}

static void addressbook_refresh(void)
{
	if (addrbook.window) {
		if (addrbook.tree_opened) {
			addressbook_reopen();
		}
	}
	addressbook_export_to_file();
}

static void addressbook_reopen(void)
{
	GtkTreePath *path;

	if (addrbook.tree_selected) {
		gtk_tree_row_reference_free(addrbook.tree_selected);
		addrbook.tree_selected = NULL;
	}
	if (addrbook.tree_opened) {
		path = gtk_tree_row_reference_get_path(addrbook.tree_opened);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(addrbook.treeview), path, NULL, FALSE);
		gtk_tree_path_free(path);
	}
}

/*
* Create the address book widgets. The address book contains two CTree widgets: the
* address index tree on the left and the address list on the right.
*
* The address index tree displays a hierarchy of interfaces and groups. Each node in
* this tree is linked to an address Adapter. Adapters have been created for interfaces,
* data sources and folder objects.
*
* The address list displays group, person and email objects. These items are linked
* directly to ItemGroup, ItemPerson and ItemEMail objects inside the address book data
* sources.
*
* In the tradition of MVC architecture, the data stores have been separated from the
* GUI components. The addrindex.c file provides the interface to all data stores.
*/
static void addressbook_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *spc_hbox;
	GtkWidget *vbox2;
	GtkWidget *tree_swin;
	GtkWidget *treeview;
	GtkTreeStore *tree_store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *list_vbox;
	GtkWidget *list_swin;
	GtkWidget *listview;
	GtkTreeStore *list_store;
	GtkWidget *paned;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *statusbar;
	GtkWidget *hbbox;
	GtkWidget *hbbox1;
	GtkWidget *hbbox2;
	GtkWidget *hsbox;
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
	gint n_entries;
	GList *nodeIf;

	debug_print("Creating addressbook window...\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Address book"));
	gtk_window_set_wmclass(GTK_WINDOW(window), "addressbook", "Sylpheed");
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_widget_set_size_request(window, 620 * gtkut_get_dpi_multiplier(), 360 * gtkut_get_dpi_multiplier());
	gtk_window_set_default_size(GTK_WINDOW(window),
				    prefs_common.addressbook_width,
				    prefs_common.addressbook_height);
	gtkut_window_move(GTK_WINDOW(window), prefs_common.addressbook_x,
			  prefs_common.addressbook_y);
	gtk_widget_realize(window);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(addressbook_close), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(size_allocated), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	n_entries = sizeof(addressbook_entries) /
		sizeof(addressbook_entries[0]);
	menubar = menubar_create(window, addressbook_entries, n_entries,
				 "<AddressBook>", NULL);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);
	menu_factory = gtk_item_factory_from_widget(menubar);

	spc_hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_size_request(spc_hbox, -1, BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(vbox), spc_hbox, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);

	tree_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tree_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(tree_swin),
					    GTK_SHADOW_IN);
	gtk_widget_set_size_request(tree_swin, prefs_common.addressbook_folder_width, -1);

	/* Address index */
	tree_store = gtk_tree_store_new(N_TREE_COLS, G_TYPE_STRING,
					G_TYPE_POINTER, GDK_TYPE_PIXBUF,
					GDK_TYPE_PIXBUF);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(tree_store),
					COL_FOLDER_NAME,
					addressbook_tree_compare,
					NULL, NULL);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store));
	g_object_unref(G_OBJECT(tree_store));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview),
					COL_FOLDER_NAME);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), FALSE);

	gtk_container_add(GTK_CONTAINER(tree_swin), treeview);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_spacing(column, 1);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_resizable(column, TRUE);

	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(renderer, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_title(column, _("Folder"));
	gtk_tree_view_column_set_attributes
		(column, renderer, "pixbuf", COL_PIXBUF,
		 "pixbuf-expander-open", COL_PIXBUF_OPEN,
		 "pixbuf-expander-closed", COL_PIXBUF, NULL);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer,
#if GTK_CHECK_VERSION(2, 6, 0)
		     "ellipsize", PANGO_ELLIPSIZE_END,
#endif
		     "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", COL_FOLDER_NAME, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(treeview), column);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(tree_store),
					     COL_FOLDER_NAME,
					     GTK_SORT_ASCENDING);

	g_signal_connect(G_OBJECT(tree_swin), "size-allocate",
			 G_CALLBACK(addressbook_folder_resized), NULL);

	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(addressbook_tree_selection_changed), NULL);
	g_signal_connect(G_OBJECT(treeview), "button_press_event",
			 G_CALLBACK(addressbook_tree_button_pressed),
			 NULL);
	g_signal_connect(G_OBJECT(treeview), "button_release_event",
			 G_CALLBACK(addressbook_tree_button_released),
			 NULL);

	gtk_drag_dest_set(treeview, GTK_DEST_DEFAULT_ALL,
			  addressbook_drag_types, N_DRAG_TYPES,
			  GDK_ACTION_MOVE | GDK_ACTION_COPY);
	g_signal_connect(G_OBJECT(treeview), "drag-motion",
			 G_CALLBACK(addressbook_drag_motion), NULL);
	g_signal_connect(G_OBJECT(treeview), "drag-leave",
			 G_CALLBACK(addressbook_drag_leave), NULL);
	g_signal_connect(G_OBJECT(treeview), "drag-data-received",
			 G_CALLBACK(addressbook_drag_received), NULL);

	list_vbox = gtk_vbox_new(FALSE, 4);

	list_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(list_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(list_swin),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(list_vbox), list_swin, TRUE, TRUE, 0);

	/* Address list */
	list_store = gtk_tree_store_new(N_LIST_COLS, G_TYPE_STRING,
					G_TYPE_STRING, G_TYPE_STRING,
					G_TYPE_STRING, G_TYPE_POINTER,
					GDK_TYPE_PIXBUF);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(list_store),
					COL_NAME,
					addressbook_list_name_compare,
					NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(list_store),
					COL_ADDRESS,
					addressbook_list_address_compare,
					NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(list_store),
					COL_NICKNAME,
					addressbook_list_nickname_compare,
					NULL, NULL);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(list_store),
					COL_REMARKS,
					addressbook_list_remarks_compare,
					NULL, NULL);

	listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	g_object_unref(G_OBJECT(list_store));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function
		(selection, addressbook_list_select_func, NULL, NULL);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(listview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(listview), COL_NAME);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(listview), FALSE);
	gtk_container_add(GTK_CONTAINER(list_swin), listview);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_spacing(column, 1);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.addressbook_col_name);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_title(column, _("Name"));
#if GTK_CHECK_VERSION(2, 14, 0)
	/* gtk_tree_view_column_set_expand(column, TRUE); */
#endif

	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(renderer, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "pixbuf", COL_L_PIXBUF, NULL);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer,
#if GTK_CHECK_VERSION(2, 6, 0)
		     "ellipsize", PANGO_ELLIPSIZE_END,
#endif
		     "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", COL_NAME, NULL);

	gtk_tree_view_column_set_sort_column_id(column, COL_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(listview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(addressbook_col_resized),
			 GINT_TO_POINTER(COL_NAME));

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer,
#if GTK_CHECK_VERSION(2, 6, 0)
		     "ellipsize", PANGO_ELLIPSIZE_END,
#endif
		     "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("E-Mail address"), renderer, "text", COL_ADDRESS, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.addressbook_col_addr);
	gtk_tree_view_column_set_resizable(column, TRUE);
#if GTK_CHECK_VERSION(2, 14, 0)
	/* gtk_tree_view_column_set_expand(column, TRUE); */
#endif
	gtk_tree_view_column_set_sort_column_id(column, COL_ADDRESS);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(addressbook_col_resized),
			 GINT_TO_POINTER(COL_ADDRESS));

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer,
#if GTK_CHECK_VERSION(2, 6, 0)
		     "ellipsize", PANGO_ELLIPSIZE_END,
#endif
		     "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Nick Name"), renderer, "text", COL_NICKNAME, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width
		(column, prefs_common.addressbook_col_nickname);
	gtk_tree_view_column_set_resizable(column, TRUE);
#if GTK_CHECK_VERSION(2, 14, 0)
	/* gtk_tree_view_column_set_expand(column, TRUE); */
#endif
	gtk_tree_view_column_set_sort_column_id(column, COL_NICKNAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(addressbook_col_resized),
			 GINT_TO_POINTER(COL_NICKNAME));

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer,
#if GTK_CHECK_VERSION(2, 6, 0)
		     "ellipsize", PANGO_ELLIPSIZE_END,
#endif
		     "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Remarks"), renderer, "text", COL_REMARKS, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_resizable(column, TRUE);
#if GTK_CHECK_VERSION(2, 14, 0)
	/* gtk_tree_view_column_set_expand(column, TRUE); */
#endif
	gtk_tree_view_column_set_sort_column_id(column, COL_REMARKS);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
	g_signal_connect(G_OBJECT(column->button), "size-allocate",
			 G_CALLBACK(addressbook_col_resized),
			 GINT_TO_POINTER(COL_REMARKS));

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list_store),
					     COL_NAME, GTK_SORT_ASCENDING);

	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(addressbook_list_selection_changed), NULL);
	g_signal_connect(G_OBJECT(listview), "button_press_event",
			 G_CALLBACK(addressbook_list_button_pressed),
			 NULL);
	g_signal_connect(G_OBJECT(listview), "button_release_event",
			 G_CALLBACK(addressbook_list_button_released),
			 NULL);
	g_signal_connect(G_OBJECT(listview), "row_expanded",
			 G_CALLBACK(addressbook_person_expand_node), NULL);
	g_signal_connect(G_OBJECT(listview), "row_collapsed",
			 G_CALLBACK(addressbook_person_collapse_node), NULL);

	gtk_tree_view_enable_model_drag_source
		(GTK_TREE_VIEW(listview), GDK_BUTTON1_MASK,
		 addressbook_drag_types, N_DRAG_TYPES,
		 GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect_after(G_OBJECT(listview), "drag-begin",
			       G_CALLBACK(addressbook_drag_begin), NULL);
	g_signal_connect_after(G_OBJECT(listview), "drag-end",
			       G_CALLBACK(addressbook_drag_end), NULL);
	g_signal_connect(G_OBJECT(listview), "drag-data-get",
			 G_CALLBACK(addressbook_drag_data_get), NULL);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(list_vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Search:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

	address_completion_register_entry(GTK_ENTRY(entry));

#if 0
	g_signal_connect(G_OBJECT(entry), "changed",
			 G_CALLBACK(addressbook_entry_changed), NULL);
#endif
	g_signal_connect(G_OBJECT(entry), "activate",
			 G_CALLBACK(addressbook_entry_activated), NULL);

	paned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox2), paned, TRUE, TRUE, 0);
	gtk_paned_add1(GTK_PANED(paned), tree_swin);
	gtk_paned_add2(GTK_PANED(paned), list_vbox);

	/* Status bar */
	hsbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, 0);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, 0);

	hbbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

	/* Button panel */
	hbbox1 = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox1), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbbox1), 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbbox1), 4);
	gtk_button_box_set_child_size(GTK_BUTTON_BOX(hbbox1), 64, -1);
	gtk_box_pack_end(GTK_BOX(hbbox), hbbox1, FALSE, FALSE, 0);

	to_btn = gtk_button_new_with_label
		(prefs_common.trans_hdr ? _("To:") : "To:");
	GTK_WIDGET_SET_FLAGS(to_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox1), to_btn, TRUE, TRUE, 0);
	cc_btn = gtk_button_new_with_label
		(prefs_common.trans_hdr ? _("Cc:") : "Cc:");
	GTK_WIDGET_SET_FLAGS(cc_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox1), cc_btn, TRUE, TRUE, 0);
	bcc_btn = gtk_button_new_with_label
		(prefs_common.trans_hdr ? _("Bcc:") : "Bcc:");
	GTK_WIDGET_SET_FLAGS(bcc_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox1), bcc_btn, TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(to_btn), "clicked",
			 G_CALLBACK(addressbook_to_clicked),
			 GINT_TO_POINTER(COMPOSE_ENTRY_TO));
	g_signal_connect(G_OBJECT(cc_btn), "clicked",
			 G_CALLBACK(addressbook_to_clicked),
			 GINT_TO_POINTER(COMPOSE_ENTRY_CC));
	g_signal_connect(G_OBJECT(bcc_btn), "clicked",
			 G_CALLBACK(addressbook_to_clicked),
			 GINT_TO_POINTER(COMPOSE_ENTRY_BCC));

	hbbox2 = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox2), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbbox2), 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbbox2), 4);
	gtk_button_box_set_child_size(GTK_BUTTON_BOX(hbbox2), 64, -1);
	gtk_box_pack_end(GTK_BOX(hbbox), hbbox2, TRUE, TRUE, 0);

	del_btn = gtk_button_new_with_label(_("Delete"));
	GTK_WIDGET_SET_FLAGS(del_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox2), del_btn, TRUE, TRUE, 0);
	reg_btn = gtk_button_new_with_label(_("Add"));
	GTK_WIDGET_SET_FLAGS(reg_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox2), reg_btn, TRUE, TRUE, 0);
	lup_btn = gtk_button_new_with_label(_("Search"));
	GTK_WIDGET_SET_FLAGS(lup_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox2), lup_btn, TRUE, TRUE, 0);
	close_btn = gtk_button_new_with_mnemonic(_("_Close"));
	GTK_WIDGET_SET_FLAGS(close_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox2), close_btn, TRUE, TRUE, 0);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(hbbox2), close_btn,
					   TRUE);

	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(addressbook_del_clicked), NULL);
	g_signal_connect(G_OBJECT(reg_btn), "clicked",
			 G_CALLBACK(addressbook_reg_clicked), NULL);
	g_signal_connect(G_OBJECT(lup_btn), "clicked",
			 G_CALLBACK(addressbook_lup_clicked), NULL);
	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(addressbook_close_clicked), NULL);

	/* Build icons for interface */
	stock_pixbuf_gdk(window, STOCK_PIXMAP_INTERFACE, &interfacepix);
	/* Build control tables */
	addrbookctl_build_map(window);
	addrbookctl_build_iflist();
	addrbookctl_build_ifselect();

	/* Add each interface into the tree as a root level folder */
	nodeIf = _addressInterfaceList_;
	while( nodeIf ) {
		AdapterInterface *adapter = nodeIf->data;
		AddressInterface *iface = adapter->iface;

		nodeIf = g_list_next(nodeIf);

		if(iface->useInterface) {
			AddressTypeControlItem *atci = adapter->atci;
			gchar *text = atci->displayName;
			GtkTreeIter iter;
			GtkTreePath *path;

			debug_print("addressbook_create: adapter: %s\n", text);
			gtk_tree_store_append(tree_store, &iter, NULL);
			gtk_tree_store_set(tree_store, &iter,
					   COL_FOLDER_NAME, text,
					   COL_OBJ, adapter,
					   COL_PIXBUF, interfacepix,
					   COL_PIXBUF_OPEN, interfacepix, -1);
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(tree_store), &iter);
			adapter->tree_row = gtk_tree_row_reference_new(GTK_TREE_MODEL(tree_store), path);
			gtk_tree_path_free(path);
		}
	}

	/* Popup menu */
	n_entries = sizeof(addressbook_tree_popup_entries) /
		sizeof(addressbook_tree_popup_entries[0]);
	tree_popup = menu_create_items(addressbook_tree_popup_entries,
				       n_entries,
				       "<AddressBookTree>", &tree_factory,
				       NULL);
	g_signal_connect(G_OBJECT(tree_popup), "selection_done",
			 G_CALLBACK(addressbook_popup_close), NULL);
	n_entries = sizeof(addressbook_list_popup_entries) /
		sizeof(addressbook_list_popup_entries[0]);
	list_popup = menu_create_items(addressbook_list_popup_entries,
				       n_entries,
				       "<AddressBookList>", &list_factory,
				       NULL);

	addrbook.window     = window;
	addrbook.menubar    = menubar;
	addrbook.treeview   = treeview;
	addrbook.listview   = listview;
	addrbook.entry      = entry;
	addrbook.statusbar  = statusbar;
	addrbook.status_cid = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "Address Book Window");

	addrbook.to_btn  = to_btn;
	addrbook.cc_btn  = cc_btn;
	addrbook.bcc_btn = bcc_btn;
	addrbook.del_btn = del_btn;
	addrbook.reg_btn = reg_btn;
	addrbook.lup_btn = lup_btn;
	addrbook.close_btn = close_btn;

	addrbook.tree_popup   = tree_popup;
	addrbook.list_popup   = list_popup;
	addrbook.tree_factory = tree_factory;
	addrbook.list_factory = list_factory;
	addrbook.menu_factory = menu_factory;

	addrbook.tree_selected = NULL;
	addrbook.list_selected = NULL;
	address_completion_start(window);
	gtk_widget_show_all(window);
}

static gint addressbook_close(void)
{
	gtkut_widget_get_uposition(addrbook.window,
				   &prefs_common.addressbook_x,
				   &prefs_common.addressbook_y);
	gtk_widget_hide(addrbook.window);
	addressbook_export_to_file();
	return TRUE;
}

static void addressbook_status_show(const gchar *msg)
{
	if (addrbook.statusbar != NULL) {
		gtk_statusbar_pop(GTK_STATUSBAR(addrbook.statusbar), addrbook.status_cid);
		if (msg) {
			gtk_statusbar_push(GTK_STATUSBAR(addrbook.statusbar), addrbook.status_cid, msg);
		}
	}
}

static void addressbook_ds_show_message(AddressDataSource *ds)
{
	gint retVal;
	gchar *name;
	gchar msgbuf[ADDRESSBOOK_MSGBUF_SIZE] = "";

	if (ds) {
		name = addrindex_ds_get_name(ds);
		if (ds->type == ADDR_IF_BOOK && name &&
		    !strcmp(name, ADDR_DS_AUTOREG))
			name = _("Auto-registered address");
		retVal = addrindex_ds_get_status_code(ds);
		if (retVal == MGU_SUCCESS) {
			g_snprintf(msgbuf, sizeof(msgbuf), "%s", name);
		} else {
			g_snprintf(msgbuf, sizeof(msgbuf), "%s: %s",
				   name, mgu_error2string(retVal));
		}
	}

	addressbook_status_show(msgbuf);
}

/*
* Delete one or more objects from address list.
*/
static void addressbook_del_clicked(GtkButton *button, gpointer data)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeView *listview = GTK_TREE_VIEW(addrbook.listview);
	GtkTreeModel *lmodel;
	GtkTreeIter list_sel;
	AddressObject *pobj = NULL;
	AddressObject *obj = NULL;
	AdapterDSource *ads = NULL;
	gboolean procFlag;
	AlertValue aval;
	AddressBookFile *abf = NULL;
	AddressDataSource *ds = NULL;

	if (!addrbook.tree_opened)
		return;

	model = gtk_tree_view_get_model(treeview);
	lmodel = gtk_tree_view_get_model(listview);
	selection = gtk_tree_view_get_selection(treeview);

	gtkut_tree_row_reference_get_iter(model, addrbook.tree_opened, &iter);
	gtk_tree_model_get(model, &iter, COL_OBJ, &pobj, -1);
	g_return_if_fail(pobj != NULL);

	gtkut_tree_row_reference_get_iter(lmodel, addrbook.list_selected,
					  &list_sel);
	gtk_tree_model_get(lmodel, &list_sel, COL_L_OBJ, &obj, -1);
	if (obj == NULL)
		return;
	if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
		return;
	ds = addressbook_find_datasource(&iter);
	if (ds == NULL)
		return;

	procFlag = FALSE;
	if (pobj->type == ADDR_DATASOURCE) {
		ads = ADAPTER_DSOURCE(pobj);
		if(ads->subType == ADDR_BOOK)
			procFlag = TRUE;
	}
	else if (pobj->type == ADDR_ITEM_FOLDER) {
		procFlag = TRUE;
	} else if (pobj->type == ADDR_ITEM_GROUP) {
		procFlag = TRUE;
	}
	if (!procFlag)
		return;
	abf = ds->rawDataSource;
	if (abf == NULL)
		return;

	/* Confirm deletion */
	aval = alertpanel(_("Delete address(es)"),
			  _("Really delete the address(es)?"),
			  GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	if (aval != G_ALERTDEFAULT)
		return;

	/* Process deletions */
	if (pobj->type == ADDR_DATASOURCE || pobj->type == ADDR_ITEM_FOLDER) {
		/* Items inside folders */
		GList *node;

		node = _addressListSelection_;
		while (node) {
			AddrItemObject *aio = node->data;
			node = g_list_next(node);
			if (aio->type == ADDR_ITEM_GROUP) {
				ItemGroup *item = (ItemGroup *)aio;
				GtkTreeIter giter;

				addressbook_find_group_node(&iter, &giter, item);
				item = addrbook_remove_group(abf, item);
				if (item) {
					addritem_free_item_group(item);
					item = NULL;
				}
				/* Remove group from parent node */
				gtk_tree_store_remove(GTK_TREE_STORE(model), &giter);
			} else if (aio->type == ADDR_ITEM_PERSON) {
				ItemPerson *item = (ItemPerson *)aio;

				if (_clipObjectList_) {
					_clipObjectList_ = g_list_remove(_clipObjectList_, item);
				}
				item = addrbook_remove_person(abf, item);
				if (item) {
					addritem_free_item_person(item);
					item = NULL;
				}
			} else if (aio->type == ADDR_ITEM_EMAIL) {
				ItemEMail *item = (ItemEMail *)aio;
				ItemPerson *person = (ItemPerson *)ADDRITEM_PARENT(item);

				item = addrbook_person_remove_email(abf, person, item);
				if (item) {
					addritem_free_item_email(item);
					item = NULL;
				}
			}
		}
		addressbook_list_select_clear();
		addressbook_reopen();
		addressbook_modified();
		return;
	} else if (pobj->type == ADDR_ITEM_GROUP) {
		/* Items inside groups */
		GList *node;
		ItemGroup *group = ADAPTER_GROUP(pobj)->itemGroup;

		node = _addressListSelection_;
		while (node) {
			AddrItemObject *aio = node->data;
			node = g_list_next(node);
			if (aio->type == ADDR_ITEM_EMAIL) {
				ItemEMail *item = (ItemEMail *)aio;
				item = addrbook_group_remove_email(abf, group, item);
			}
		}
		addressbook_list_select_clear();
		addressbook_reopen();
		return;
	}

	gtk_tree_store_remove(GTK_TREE_STORE(lmodel), &list_sel);
	addressbook_list_select_remove(obj);
}

static void addressbook_reg_clicked(GtkButton *button, gpointer data)
{
	addressbook_new_address_cb(NULL, 0, NULL);
}

gchar *addressbook_format_address(AddressObject *obj)
{
	gchar *buf = NULL;
	gchar *name = NULL;
	gchar *address = NULL;

	if( obj->type == ADDR_ITEM_EMAIL ) {
		ItemPerson *person = NULL;
		ItemEMail *email = ( ItemEMail * ) obj;

		person = ( ItemPerson * ) ADDRITEM_PARENT(email);
		if( email->address ) {
			if( ADDRITEM_NAME(email) ) {
				name = ADDRITEM_NAME(email);
				if( *name == '\0' ) {
					name = ADDRITEM_NAME(person);
				}
			}
			else if( person && ADDRITEM_NAME(person) ) {
				name = ADDRITEM_NAME(person);
			}
			else {
				buf = g_strdup( email->address );
			}
			address = email->address;
		}
	}
	else if( obj->type == ADDR_ITEM_PERSON ) {
		ItemPerson *person = ( ItemPerson * ) obj;
		GList *node = person->listEMail;

		name = ADDRITEM_NAME(person);
		if( node ) {
			ItemEMail *email = ( ItemEMail * ) node->data;
			address = email->address;
		}
	}
	if( address ) {
		if( !prefs_common.always_add_address_only &&
		    name && name[0] != '\0' ) {
			if( name[0] != '"' && strpbrk( name, ",.[]<>" ) != NULL )
				buf = g_strdup_printf( "\"%s\" <%s>", name, address );
			else
				buf = g_strdup_printf( "%s <%s>", name, address );
		}
		else {
			buf = g_strdup( address );
		}
	}

	return buf;
}

static void addressbook_to_clicked(GtkButton *button, gpointer data)
{
	GList *node = _addressListSelection_;
	gboolean new_compose = FALSE;

	if (!addrbook.target_compose) {
		new_compose = TRUE;
		addrbook.target_compose = compose_new(NULL, NULL, NULL, NULL);
		if (!addrbook.target_compose)
			return;
	}

	if (new_compose)
		compose_block_modified(addrbook.target_compose);

	while( node ) {
		AddressObject *obj = node->data;
		Compose *compose = addrbook.target_compose;

		node = g_list_next( node );
		if( obj->type == ADDR_ITEM_PERSON || obj->type == ADDR_ITEM_EMAIL ) {
			gchar *addr = addressbook_format_address( obj );
			compose_entry_append( compose, addr, (ComposeEntryType) data );
			g_free( addr );
			addr = NULL;
		}
		else if( obj->type == ADDR_ITEM_GROUP ) {
			ItemGroup *group = ( ItemGroup * ) obj;
			GList *nodeMail = group->listEMail;
			while( nodeMail ) {
				ItemEMail *email = nodeMail->data;
				gchar *addr = addressbook_format_address( ( AddressObject * ) email );
				compose_entry_append( compose, addr, (ComposeEntryType) data );
				g_free( addr );
				nodeMail = g_list_next( nodeMail );
			}
		}
	}

	if (new_compose)
		compose_unblock_modified(addrbook.target_compose);
}

static void addressbook_menuitem_set_sensitive(void)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeView *listview = GTK_TREE_VIEW(addrbook.listview);
	GtkTreeSelection *selection;
	GtkTreeSelection *lselection;
	GtkTreeModel *model;
	GtkTreeModel *lmodel;
	GtkTreeIter iter;
	GtkTreeIter liter;
	gboolean canAdd = FALSE;
	gboolean canEditTree = TRUE;
	gboolean canEditAddress = FALSE;
	gboolean canDelete = FALSE;
	gboolean canLookup = FALSE;
	gboolean canCopy = FALSE;
	gboolean canPaste = FALSE;
	gboolean hasListSelection = _addressListSelection_ != NULL;
	AddressTypeControlItem *atci = NULL;
	AddressDataSource *ds = NULL;
	AddressInterface *iface = NULL;
	AddressObject *pobj = NULL;
	AddressObject *obj = NULL;

	menu_set_sensitive(addrbook.menu_factory, "/File/New Book", FALSE);
	menu_set_sensitive(addrbook.menu_factory, "/File/New vCard", FALSE);
#ifdef USE_JPILOT
	menu_set_sensitive(addrbook.menu_factory, "/File/New JPilot", FALSE);
#endif
#ifdef USE_LDAP
	menu_set_sensitive(addrbook.menu_factory, "/File/New LDAP Server", FALSE);
#endif
	menu_set_insensitive_all(GTK_MENU_SHELL(addrbook.tree_popup));
	menu_set_insensitive_all(GTK_MENU_SHELL(addrbook.list_popup));

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, COL_OBJ, &pobj, -1);
	if (pobj == NULL)
		return;

	lmodel = gtk_tree_view_get_model(listview);
	lselection = gtk_tree_view_get_selection(listview);

	if (addrbook.list_selected) {
		if (gtkut_tree_row_reference_get_iter(lmodel, addrbook.list_selected, &liter))
			gtk_tree_model_get(lmodel, &liter, COL_L_OBJ, &obj, -1);
	}

	if (pobj->type == ADDR_INTERFACE) {
		AdapterInterface *adapter = ADAPTER_INTERFACE(pobj);

		iface = adapter->iface;
		if (iface && iface->haveLibrary) {
			/* Enable appropriate File / New command */
			atci = adapter->atci;
			menu_set_sensitive(addrbook.menu_factory, atci->menuCommand, TRUE);
		}
		canEditTree = FALSE;
	} else if (pobj->type == ADDR_DATASOURCE) {
		AdapterDSource *ads = ADAPTER_DSOURCE(pobj);

		ds = ads->dataSource;
		iface = ds->iface;
		if (!iface->readOnly) {
			canAdd = canEditAddress = TRUE;
		}
		if (!iface->haveLibrary) {
			canAdd = canEditAddress = FALSE;
		}
#ifdef USE_LDAP
		if (ads->subType == ADDR_LDAP) {
			if (iface->haveLibrary && ds->rawDataSource)
				canLookup = TRUE;
		} else
#endif
			canLookup = TRUE;
		if (ads->subType == ADDR_BOOK && pobj->name &&
		    !strcmp(pobj->name, ADDR_DS_AUTOREG))
			canEditTree = FALSE;
	} else if (pobj->type == ADDR_ITEM_FOLDER) {
		ds = addressbook_find_datasource(&iter);
		if (ds) {
			iface = ds->iface;
			if (!iface->readOnly) {
				canAdd = canEditAddress = TRUE;
			}
			canLookup = TRUE;
		}
	} else if (pobj->type == ADDR_ITEM_GROUP) {
		ds = addressbook_find_datasource(&iter);
		if (ds) {
			iface = ds->iface;
			if (!iface->readOnly) {
				canEditAddress = TRUE;
			}
			canLookup = TRUE;
		}
	}

	if (obj == NULL)
		canEditAddress = FALSE;
	canDelete = canEditAddress;
	if (gtk_tree_selection_count_selected_rows(lselection) > 1)
		canEditAddress = FALSE;
	if (_addressListSelection_) {
		GList *cur;
		AddressObject *item;

		for (cur = _addressListSelection_; cur != NULL; cur = cur->next) {
			item = cur->data;
			if (item->type == ADDR_ITEM_PERSON) {
				canCopy = TRUE;
				break;
			}
		}
	}
	if (_clipObjectList_ && canAdd)
		canPaste = TRUE;

	/* Enable add */
	menu_set_sensitive(addrbook.menu_factory, "/Address/New Address", canAdd);
	menu_set_sensitive(addrbook.menu_factory, "/Address/New Folder", canAdd);
	menu_set_sensitive(addrbook.menu_factory, "/Address/New Group", canAdd);

	menu_set_sensitive(addrbook.menu_factory, "/Address/Add to recipient", hasListSelection);
	menu_set_sensitive(addrbook.menu_factory, "/Address/Add to Cc", hasListSelection);
	menu_set_sensitive(addrbook.menu_factory, "/Address/Add to Bcc", hasListSelection);

	/* Enable edit */
	menu_set_sensitive(addrbook.menu_factory, "/Address/Edit", canEditAddress);
	menu_set_sensitive(addrbook.menu_factory, "/Address/Delete", canDelete);

	menu_set_sensitive(addrbook.menu_factory, "/File/Edit", canEditTree);
	menu_set_sensitive(addrbook.menu_factory, "/File/Delete", canEditTree);

	menu_set_sensitive(addrbook.menu_factory, "/Edit/Copy",   canCopy);
	menu_set_sensitive(addrbook.menu_factory, "/Edit/Paste",  canPaste);

	menu_set_sensitive(addrbook.menu_factory, "/Tools/Export to CSV file", canEditTree);

	/* Popup menu */
	menu_set_sensitive(addrbook.tree_factory, "/New Address", canAdd);
	menu_set_sensitive(addrbook.tree_factory, "/New Folder", canAdd);
	menu_set_sensitive(addrbook.tree_factory, "/New Group", canAdd);
	menu_set_sensitive(addrbook.tree_factory, "/Edit", canEditTree);
	menu_set_sensitive(addrbook.tree_factory, "/Delete", canEditTree);

	menu_set_sensitive(addrbook.list_factory, "/New Address", canAdd);
	menu_set_sensitive(addrbook.list_factory, "/New Folder", canAdd);
	menu_set_sensitive(addrbook.list_factory, "/New Group", canAdd);
	menu_set_sensitive(addrbook.list_factory, "/Add to recipient", hasListSelection);
	menu_set_sensitive(addrbook.list_factory, "/Add to Cc", hasListSelection);
	menu_set_sensitive(addrbook.list_factory, "/Add to Bcc", hasListSelection);
	menu_set_sensitive(addrbook.list_factory, "/Edit", canEditAddress);
	menu_set_sensitive(addrbook.list_factory, "/Delete", canDelete);
	menu_set_sensitive(addrbook.list_factory, "/Copy", canCopy);
	menu_set_sensitive(addrbook.list_factory, "/Paste", canPaste);

	/* Buttons */
	gtk_widget_set_sensitive(addrbook.to_btn, hasListSelection);
	gtk_widget_set_sensitive(addrbook.cc_btn, hasListSelection);
	gtk_widget_set_sensitive(addrbook.bcc_btn, hasListSelection);
	gtk_widget_set_sensitive(addrbook.reg_btn, canAdd);
	gtk_widget_set_sensitive(addrbook.del_btn, canDelete);
	gtk_widget_set_sensitive(addrbook.lup_btn, canLookup);
}

static void addressbook_tree_selection_changed(GtkTreeSelection *selection,
					       gpointer data)
{
	GtkTreeModel *model;
	GtkTreeModel *lmodel;
	GtkTreeIter iter;
	GtkTreePath *path;
	AddressObject *obj = NULL;
	AdapterDSource *ads = NULL;
	AddressDataSource *ds = NULL;
	ItemFolder *rootFolder = NULL;

	debug_print("addressbook_tree_selection_changed\n");

	if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
		debug_print("addressbook_tree_selection_changed: no selection\n");
		gtk_tree_row_reference_free(addrbook.tree_selected);
		addrbook.tree_selected = NULL;
		gtk_tree_row_reference_free(addrbook.tree_opened);
		addrbook.tree_opened = NULL;
		addressbook_status_show("");
		if (addrbook.entry)
			gtk_entry_set_text(GTK_ENTRY(addrbook.entry), "");
		if (addrbook.listview) {
			lmodel = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.listview));
			gtk_tree_store_clear(GTK_TREE_STORE(lmodel));
		}
		return;
	}

	path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_row_reference_free(addrbook.tree_selected);
	addrbook.tree_selected = gtk_tree_row_reference_new(model, path);
	gtk_tree_row_reference_free(addrbook.list_selected);
	addrbook.list_selected = NULL;
	addressbook_status_show("");
	if (addrbook.entry)
		gtk_entry_set_text(GTK_ENTRY(addrbook.entry), "");
	if (addrbook.listview) {
		lmodel = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.listview));
		gtk_tree_store_clear(GTK_TREE_STORE(lmodel));
	}
	gtk_tree_model_get(model, &iter, COL_OBJ, &obj, -1);
	if (obj == NULL) {
		gtk_tree_path_free(path);
		return;
	}

	gtk_tree_row_reference_free(addrbook.tree_opened);
	addrbook.tree_opened = gtk_tree_row_reference_new(model, path);

	if (obj->type == ADDR_DATASOURCE) {
		/* Read from file */
		static gboolean tVal = TRUE;

		ads = ADAPTER_DSOURCE(obj);
		if (ads == NULL) return;
		ds = ads->dataSource;
		if (ds == NULL) return;		

		if (addrindex_ds_get_modify_flag(ds)) {
			addrindex_ds_read_data(ds);
		}

		if (!addrindex_ds_get_read_flag(ds)) {
			addrindex_ds_read_data(ds);
		}
		addressbook_ds_show_message(ds);

		if (!addrindex_ds_get_access_flag(ds)) {
			/* Remove existing folders and groups */
			addressbook_tree_remove_children(model, &iter);

			/* Load folders into the tree */
			rootFolder = addrindex_ds_get_root_folder(ds);
			if (ds->type == ADDR_IF_JPILOT) {
				addressbook_node_add_folder(&iter, ds, rootFolder, ADDR_CATEGORY, NULL);
			} else {
				addressbook_node_add_folder(&iter, ds, rootFolder, ADDR_ITEM_FOLDER, NULL);
			}
			addrindex_ds_set_access_flag(ds, &tVal);
			gtk_tree_view_expand_row(GTK_TREE_VIEW(addrbook.treeview), path, TRUE);
		}
	}

	gtk_tree_path_free(path);

	/* Update address list */
	addressbook_set_list(obj);
	addressbook_menuitem_set_sensitive();
	addressbook_list_select_clear();
}

static void addressbook_list_selection_changed(GtkTreeSelection *selection,
					       gpointer data)
{
	GtkTreeModel *lmodel;
	GtkTreePath *path;
	GList *selected;

	debug_print("addressbook_list_selection_changed\n");

	selected = gtk_tree_selection_get_selected_rows(selection, &lmodel);
	addressbook_list_select_set(selected);
	gtk_tree_row_reference_free(addrbook.list_selected);
	if (selected) {
		path = (GtkTreePath *)selected->data;
		addrbook.list_selected = gtk_tree_row_reference_new(lmodel, path);
		g_list_foreach(selected, (GFunc)gtk_tree_path_free, NULL);
		g_list_free(selected);
	} else
		addrbook.list_selected = NULL;

	addressbook_menuitem_set_sensitive();
}

static void addressbook_list_select_clear(void)
{
	if (_addressListSelection_) {
		g_list_free(_addressListSelection_);
	}
	_addressListSelection_ = NULL;
}

static void addressbook_list_select_remove(AddressObject *obj)
{
	if (!obj)
		return;
	if (_addressListSelection_) {
		_addressListSelection_ = g_list_remove(_addressListSelection_, obj);
	}
}

static void addressbook_list_select_set(GList *row_list)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *cur;
	AddressObject *obj;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.listview));

	addressbook_list_select_clear();
	for (cur = row_list; cur != NULL; cur = cur->next) {
		obj = NULL;
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)cur->data);
		gtk_tree_model_get(model, &iter, COL_L_OBJ, &obj, -1);
		if (obj && (obj->type == ADDR_ITEM_PERSON ||
			    obj->type == ADDR_ITEM_EMAIL ||
			    obj->type == ADDR_ITEM_GROUP)) {
			_addressListSelection_ = g_list_prepend(_addressListSelection_, obj);
		}
	}
	_addressListSelection_ = g_list_reverse(_addressListSelection_);
}

static void addressbook_entry_activated(GtkWidget *widget, gpointer data)
{
	addressbook_lup_clicked(NULL, NULL);
}

static gboolean addressbook_list_button_pressed(GtkWidget *widget,
						GdkEventButton *event,
						gpointer data)
{
	GtkTreePath *path = NULL;
	GtkTreeSelection *selection;
	gboolean is_selected = FALSE;
	gboolean mod_pressed = FALSE;

	if (!event)
		return FALSE;

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y,
				      &path, NULL, NULL, NULL);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	if (path)
		is_selected = gtk_tree_selection_path_is_selected
			(selection, path);
	mod_pressed = ((event->state &
			(GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0);

	if (event->button == 1) {
		if (is_selected && !mod_pressed) {
			can_toggle_list_selection = FALSE;
		}

		if (is_selected && event->type == GDK_2BUTTON_PRESS) {
			debug_print("addressbook_list_button_pressed: GDK_2BUTTON_PRESS\n");
			/* Handle double click */
			if (prefs_common.add_address_by_click &&
			    addrbook.target_compose)
				addressbook_to_clicked
					(NULL, GINT_TO_POINTER(COMPOSE_ENTRY_TO));
			else
				addressbook_edit_address_cb(NULL, 0, NULL);
			return TRUE;
		}
	} else if (event->button == 3) {
		gtk_menu_popup(GTK_MENU(addrbook.list_popup), NULL, NULL,
			       NULL, NULL, event->button, event->time);
		gtk_tree_path_free(path);
		if (is_selected)
			return TRUE;
	}

	return FALSE;
}

static gboolean addressbook_list_button_released(GtkWidget *widget,
						 GdkEventButton *event,
						 gpointer data)
{
	if (!can_toggle_list_selection && !list_on_drag) {
		GtkTreePath *path;

		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &path, NULL, NULL, NULL)) {
			can_toggle_list_selection = TRUE;
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(widget), path, NULL, FALSE);
			gtk_tree_path_free(path);
		}
	}
	can_toggle_list_selection = TRUE;
	list_on_drag = FALSE;
	return FALSE;
}

static gboolean addressbook_tree_button_pressed(GtkWidget *widget,
						GdkEventButton *event,
						gpointer data)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(widget);
	GtkTreeSelection *selection;
	GtkTreePath *path;

	if (!event)
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos(treeview, event->x, event->y,
					   &path, NULL, NULL, NULL))
		return TRUE;
	selection = gtk_tree_view_get_selection(treeview);

	addressbook_menuitem_set_sensitive();

	if (event->button == 3) {
		gtk_tree_selection_select_path(selection, path);
		gtk_menu_popup(GTK_MENU(addrbook.tree_popup),
			       NULL, NULL, NULL, NULL,
			       event->button, event->time);
	} else if (event->type == GDK_2BUTTON_PRESS) {
		gtk_tree_view_expand_row(treeview, path, FALSE);
	}

	gtk_tree_path_free(path);
	return FALSE;
}

static gboolean addressbook_tree_button_released(GtkWidget *widget,
						 GdkEventButton *event,
						 gpointer data)
{
	return FALSE;
}

static gboolean addressbook_obj_is_droppable(GtkTreeModel *model,
					     AddressObject *obj)
{
	gboolean acceptable = FALSE;

	if (!model || !obj)
		return FALSE;
	if (!_addressListSelection_)
		return FALSE;

	if (obj->type == ADDR_DATASOURCE) {
		AdapterDSource *ads;
		AddressDataSource *ds;
		AddressInterface *iface;

		ads = ADAPTER_DSOURCE(obj);
		ds = ads ? ads->dataSource : NULL;
		iface = ds ? ds->iface : NULL;
		if (ads && ads->subType == ADDR_BOOK &&
		    iface && !iface->readOnly)
			acceptable = TRUE;
	} else if (obj->type == ADDR_ITEM_FOLDER) {
		acceptable = TRUE;
	}

	if (acceptable && addrbook.tree_opened) {
		AddressObject *open_obj;
		GtkTreeIter opened;

		gtkut_tree_row_reference_get_iter(model, addrbook.tree_opened,
						  &opened);
		gtk_tree_model_get(model, &opened, COL_OBJ, &open_obj, -1);
		if (open_obj == obj)
			acceptable = FALSE;
	}

	if (acceptable) {
		GList *node;
		for (node = _addressListSelection_; node != NULL; node = node->next) {
			AddressObject *obj = node->data;
			if (obj && obj->type == ADDR_ITEM_PERSON)
				break;
		}
		if (!node)
			acceptable = FALSE;
	}

	return acceptable;
}

static gboolean addressbook_drag_motion(GtkWidget *widget,
					GdkDragContext *context,
					gint x, gint y,
					guint time, gpointer data)
{
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	AddressObject *obj, *src_obj;
	AddressDataSource *src_ds;
	AddressBookFile *src_abf;
	gboolean acceptable;

	if (!gtk_tree_view_get_dest_row_at_pos
		(GTK_TREE_VIEW(widget), x, y, &path, NULL)) {
		gtk_tree_view_set_drag_dest_row
			(GTK_TREE_VIEW(widget), NULL,
			 GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
		gdk_drag_status(context, 0, time);
		return FALSE;
	}

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, COL_OBJ, &obj, -1);
	g_return_val_if_fail(obj != NULL, FALSE);

	/* source */
	gtkut_tree_row_reference_get_iter(model, addrbook.tree_opened, &iter);
	gtk_tree_model_get(model, &iter, COL_OBJ, &src_obj, -1);
	g_return_val_if_fail(src_obj != NULL, FALSE);
	src_ds = addressbook_find_datasource(&iter);
	g_return_val_if_fail(src_ds != NULL, FALSE);
	src_abf = src_ds->rawDataSource;
	g_return_val_if_fail(src_abf != NULL, FALSE);

#ifdef G_OS_WIN32
	{
		GdkModifierType state = 0;

		gdk_window_get_pointer(widget->window, NULL, NULL, &state);
		if ((state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) == 0)
			context->actions = GDK_ACTION_MOVE | GDK_ACTION_COPY;
	}
#endif

	if (!src_ds->iface ||
	    (src_ds->iface->readOnly || !src_ds->iface->haveLibrary))
		context->actions &= ~GDK_ACTION_MOVE;

	acceptable = addressbook_obj_is_droppable(model, obj);
	if ((context->actions & (GDK_ACTION_MOVE | GDK_ACTION_COPY)) == 0)
		acceptable = FALSE;

	if (!acceptable) {
		gtk_tree_view_set_drag_dest_row
			(GTK_TREE_VIEW(widget), NULL,
			 GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
		gdk_drag_status(context, 0, time);
	} else {
		GdkDragAction action = 0;

		if ((context->actions & GDK_ACTION_MOVE) != 0)
			action = GDK_ACTION_MOVE;
		else if ((context->actions & GDK_ACTION_COPY) != 0)
			action = GDK_ACTION_COPY;
		gtk_tree_view_set_drag_dest_row
			(GTK_TREE_VIEW(widget), path,
			 GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
		gdk_drag_status(context, action, time);
	}

	gtk_tree_path_free(path);

	return TRUE;
}

static void addressbook_drag_leave(GtkWidget *widget, GdkDragContext *context,
				   guint time, gpointer data)
{
	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget), NULL,
					GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
}

static void addressbook_drag_received(GtkWidget	*widget,
				      GdkDragContext *context,
				      gint x, gint y,
				      GtkSelectionData *data,
				      guint info, guint time,
				      gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	AddressObject *obj, *pobj, *src_obj;
	AddressDataSource *ds, *src_ds;
	AddressBookFile *abf, *src_abf;
	gboolean acceptable;
	gboolean is_move;
	GList *node;
	ItemFolder *folder = NULL;
	ItemPerson *person, *new_person;

	if (!gtk_tree_view_get_dest_row_at_pos
		(GTK_TREE_VIEW(widget), x, y, &path, NULL))
		return;

	debug_print("addressbook_drag_received_cb: received data\n");

	/* dest */
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, COL_OBJ, &pobj, -1);
	g_return_if_fail(pobj != NULL);
	ds = addressbook_find_datasource(&iter);
	g_return_if_fail(ds != NULL);

	/* source */
	gtkut_tree_row_reference_get_iter(model, addrbook.tree_opened, &iter);
	gtk_tree_model_get(model, &iter, COL_OBJ, &src_obj, -1);
	g_return_if_fail(src_obj != NULL);
	src_ds = addressbook_find_datasource(&iter);
	g_return_if_fail(src_ds != NULL);
	src_abf = src_ds->rawDataSource;
	g_return_if_fail(src_abf != NULL);

	acceptable = addressbook_obj_is_droppable(model, pobj);
	abf = ds->rawDataSource;
	if (!abf)
		acceptable = FALSE;

	if ((context->actions & (GDK_ACTION_MOVE | GDK_ACTION_COPY)) == 0)
		acceptable = FALSE;
	is_move = (context->actions & GDK_ACTION_MOVE) != 0;

	if (!src_ds->iface ||
	    (src_ds->iface->readOnly || !src_ds->iface->haveLibrary))
		is_move = FALSE;

	if (!_addressListSelection_ || !acceptable) {
		context->action = 0;
		gtk_drag_finish(context, FALSE, FALSE, time);
		gtk_tree_path_free(path);
		return;
	}

	if (pobj->type == ADDR_ITEM_FOLDER)
		folder = ADAPTER_FOLDER(pobj)->itemFolder;

	node = _addressListSelection_;
	while (node) {
		GList *cur;

		obj = node->data;
		node = g_list_next(node);
		if (obj->type != ADDR_ITEM_PERSON)
			continue;

		person = (ItemPerson *)obj;
		new_person = addrbook_add_address_list(abf, folder, NULL);
		ADDRITEM_NAME(new_person) = g_strdup(ADDRITEM_NAME(person));
		new_person->firstName = g_strdup(person->firstName);
                new_person->lastName = g_strdup(person->lastName);
                new_person->nickName = g_strdup(person->nickName);
		for (cur = person->listEMail; cur != NULL; cur = cur->next) {
			ItemEMail *email = cur->data;
			addritem_person_add_email(new_person, addritem_copy_item_email(email));
		}
		for (cur = person->listAttrib; cur != NULL; cur = cur->next) {
			UserAttribute *attr = cur->data;
			addritem_person_add_attribute(new_person, addritem_copy_attribute(attr));
		}

		if (is_move) {
			if (_clipObjectList_)
				_clipObjectList_ = g_list_remove(_clipObjectList_, person);
			person = addrbook_remove_person(src_abf, person);
			if (person)
				addritem_free_item_person(person);
		}
	}

	if (is_move) {
		addressbook_list_select_clear();
		addressbook_reopen();
	}

	context->action = 0;
	gtk_drag_finish(context, TRUE, FALSE, time);

	gtk_tree_path_free(path);

	addressbook_modified();
}

static void addressbook_folder_resized(GtkWidget *widget,
				       GtkAllocation *allocation,
				       gpointer data)
{
	gint width = allocation->width;

	if (width < 8)
		return;

	prefs_common.addressbook_folder_width = width;
}

static void addressbook_col_resized(GtkWidget *widget,
				    GtkAllocation *allocation, gpointer data)
{
	AddressBookListColumnPos type = (gint)data;
	gint width = allocation->width;

	if (width < 8)
		return;

	switch (type) {
	case COL_NAME:
		prefs_common.addressbook_col_name = width;
		break;
	case COL_ADDRESS:
		prefs_common.addressbook_col_addr = width;
		break;
	case COL_NICKNAME:
		prefs_common.addressbook_col_nickname = width;
		break;
	case COL_REMARKS:
		prefs_common.addressbook_col_rem = width;
		break;
	default:
		break;
	}
}

static void addressbook_popup_close(GtkMenuShell *menu_shell, gpointer data)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreePath *path;

	if (!addrbook.tree_opened)
		return;
 
	selection = gtk_tree_view_get_selection(treeview);
	path = gtk_tree_row_reference_get_path(addrbook.tree_opened);
	if (path) {
		gtk_tree_selection_select_path(selection, path);
		gtk_tree_path_free(path);
	}
}

static void addressbook_new_folder_cb(gpointer data, guint action,
				      GtkWidget *widget)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	AddressObject *obj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *parentFolder = NULL;
	ItemFolder *folder = NULL;

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, COL_OBJ, &obj, -1);
	if (obj == NULL)
		return;
	ds = addressbook_find_datasource(&iter);
	if (ds == NULL)
		return;

	if (obj->type == ADDR_DATASOURCE) {
		if (ADAPTER_DSOURCE(obj)->subType != ADDR_BOOK) return;
	} else if (obj->type == ADDR_ITEM_FOLDER) {
		parentFolder = ADAPTER_FOLDER(obj)->itemFolder;
	} else {
		return;
	}

	abf = ds->rawDataSource;
	if (abf == NULL) return;
	folder = addressbook_edit_folder(abf, parentFolder, NULL);
	if (folder) {
		GtkTreeIter new_iter;
		GtkTreePath *path;
		addressbook_node_add_folder(&iter, ds, folder, ADDR_ITEM_FOLDER, &new_iter);
		path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_expand_row(treeview, path, TRUE);
		gtk_tree_path_free(path);
		if (gtkut_tree_row_reference_equal(addrbook.tree_selected, addrbook.tree_opened))
			addressbook_set_list(obj);
	}

}

static void addressbook_new_group_cb(gpointer data, guint action,
				     GtkWidget *widget)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	AddressObject *obj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *parentFolder = NULL;
	ItemGroup *group = NULL;

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, COL_OBJ, &obj, -1);
	if (obj == NULL)
		return;
	ds = addressbook_find_datasource(&iter);
	if (ds == NULL)
		return;

	if (obj->type == ADDR_DATASOURCE) {
		if (ADAPTER_DSOURCE(obj)->subType != ADDR_BOOK) return;
	} else if (obj->type == ADDR_ITEM_FOLDER) {
		parentFolder = ADAPTER_FOLDER(obj)->itemFolder;
	} else {
		return;
	}

	abf = ds->rawDataSource;
	if (abf == NULL) return;
	group = addressbook_edit_group(abf, parentFolder, NULL);
	if (group) {
		GtkTreeIter new_iter;
		GtkTreePath *path;
		addressbook_node_add_group(&iter, ds, group, &new_iter);
		path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_expand_row(treeview, path, TRUE);
		gtk_tree_path_free(path);
		if (gtkut_tree_row_reference_equal(addrbook.tree_selected, addrbook.tree_opened))
			addressbook_set_list(obj);
	}

}

static void addressbook_change_node_name(GtkTreeIter *iter, const gchar *name)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeStore *store;

	store = GTK_TREE_STORE(gtk_tree_view_get_model(treeview));
	gtk_tree_store_set(store, iter, COL_FOLDER_NAME, name, -1);
}

/*
* Edit data source.
* Enter: obj   Address object to edit.
*        node  Node in tree.
* Return: New name of data source.
*/
static gchar *addressbook_edit_datasource(AddressObject *obj, GtkTreeIter *iter)
{
	gchar *newName = NULL;
	AddressDataSource *ds = NULL;
	AddressInterface *iface = NULL;
	AdapterDSource *ads = NULL;

	ds = addressbook_find_datasource(iter);
	if (ds == NULL)
		return NULL;
	iface = ds->iface;
	if (!iface->haveLibrary)
		return NULL;

	/* Read data from data source */
	if (!addrindex_ds_get_read_flag(ds)) {
		addrindex_ds_read_data(ds);
	}

	/* Handle edit */
	ads = ADAPTER_DSOURCE(obj);
	if (ads->subType == ADDR_BOOK) {
                if (addressbook_edit_book(_addressIndex_, ads) == NULL)
			return NULL;
	} else if (ads->subType == ADDR_VCARD) {
       	        if (addressbook_edit_vcard(_addressIndex_, ads) == NULL)
			return NULL;
#ifdef USE_JPILOT
	} else if (ads->subType == ADDR_JPILOT) {
                if (addressbook_edit_jpilot(_addressIndex_, ads) == NULL)
			return NULL;
#endif
#ifdef USE_LDAP
	} else if (ads->subType == ADDR_LDAP) {
		if (addressbook_edit_ldap(_addressIndex_, ads) == NULL)
			return NULL;
#endif
	} else {
		return NULL;
	}
	newName = obj->name;
	return newName;
}

/*
* Edit an object that is in the address tree area.
*/
static void addressbook_treenode_edit_cb(gpointer data, guint action,
					 GtkWidget *widget)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, parent;
	AddressObject *obj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	gchar *name = NULL;

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	if (gtk_tree_store_iter_depth(GTK_TREE_STORE(model), &iter) == 0)
		return;
	gtk_tree_model_get(model, &iter, COL_OBJ, &obj, -1);
	if (obj == NULL)
		return;
	if (!gtk_tree_model_iter_parent(model, &parent, &iter))
		return;

	ds = addressbook_find_datasource(&iter);
	if (ds == NULL)
		return;

	if (obj->type == ADDR_DATASOURCE) {
		name = addressbook_edit_datasource(obj, &iter);
		if (name == NULL)
			return;
	} else {
		abf = ds->rawDataSource;
		if (abf == NULL)
			return;
		if (obj->type == ADDR_ITEM_FOLDER) {
			AdapterFolder *adapter = ADAPTER_FOLDER(obj);
			ItemFolder *item = adapter->itemFolder;
			ItemFolder *parentFolder = NULL;
			parentFolder = (ItemFolder *)ADDRITEM_PARENT(item);
			if (addressbook_edit_folder(abf, parentFolder, item) == NULL)
				return;
			name = ADDRITEM_NAME(item);
		} else if (obj->type == ADDR_ITEM_GROUP) {
			AdapterGroup *adapter = ADAPTER_GROUP(obj);
			ItemGroup *item = adapter->itemGroup;
			ItemFolder *parentFolder = NULL;
			parentFolder = (ItemFolder *)ADDRITEM_PARENT(item);
			if (addressbook_edit_group(abf, parentFolder, item) == NULL)
				return;
			name = ADDRITEM_NAME(item);
		}
	}

	if (name) {
		GtkTreePath *path;

		/* Update node in tree view */
		addressbook_change_node_name(&iter, name);
		path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_expand_row(treeview, path, TRUE);
		gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
		gtk_tree_path_free(path);
	}
}

/*
* Delete an item from the tree widget.
*/
static void addressbook_treenode_delete_cb(gpointer data, guint action,
					 GtkWidget *widget)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, parent;
	AddressObject *obj;
	gchar *message;
	AlertValue aval;
	AddressBookFile *abf = NULL;
	AdapterDSource *ads = NULL;
	AddressInterface *iface = NULL;
	AddressDataSource *ds = NULL;
	gboolean remFlag = FALSE;

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	if (gtk_tree_store_iter_depth(GTK_TREE_STORE(model), &iter) == 0)
		return;
	gtk_tree_model_get(model, &iter, COL_OBJ, &obj, -1);
	g_return_if_fail(obj != NULL);

	if (obj->type == ADDR_DATASOURCE) {
		ads = ADAPTER_DSOURCE(obj);
		if (ads == NULL)
			return;
		ds = ads->dataSource;
		if (ds == NULL)
			return;
	} else {
		/* Must be folder or something else */
		ds = addressbook_find_datasource(&iter);
		if (ds == NULL)
			return;

		/* Only allow deletion from non-readOnly data sources */
		iface = ds->iface;
		if (iface->readOnly)
			return;
	}

	/* Confirm deletion */
	if (obj->type == ADDR_ITEM_FOLDER) {
		message = g_strdup_printf
			(_("Do you want to delete the folder AND all addresses in `%s' ?\n"
			   "If deleting the folder only, addresses will be moved into parent folder."),
			 obj->name);
		aval = alertpanel(_("Delete folder"), message, _("_Folder only"), _("Folder and _addresses"), GTK_STOCK_CANCEL);
		g_free(message);
		if (aval != G_ALERTDEFAULT && aval != G_ALERTALTERNATE)
			return;
	} else {
		message = g_strdup_printf(_("Really delete `%s' ?"), obj->name);
		aval = alertpanel(_("Delete"), message, GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		g_free(message);
		if (aval != G_ALERTDEFAULT)
			return;
	}

	/* Proceed with deletion */
	if (obj->type == ADDR_DATASOURCE) {
		/* Remove data source. */
		if (addrindex_index_remove_datasource(_addressIndex_, ds)) {
			addressbook_free_child_adapters(&iter);
			abf = addressbook_get_book_file();
			if (abf) {
				gchar *bookFile;
				bookFile = g_strconcat(abf->path, G_DIR_SEPARATOR_S, abf->fileName, NULL);
				debug_print("removing %s\n", bookFile);
				g_unlink(bookFile);
				g_free(bookFile);
			}
			remFlag = TRUE;
		}
	} else {
		abf = addressbook_get_book_file();
		if (abf == NULL)
			return;
	}

	if (obj->type == ADDR_ITEM_FOLDER) {
		AdapterFolder *adapter = ADAPTER_FOLDER(obj);
		ItemFolder *item = adapter->itemFolder;

		if (aval == G_ALERTDEFAULT) {
			/* Remove folder only */
			item = addrbook_remove_folder(abf, item);
			if (item) {
				addritem_free_item_folder(item);
				addressbook_move_nodes_up(&iter);
				remFlag = TRUE;
			}
		} else if (aval == G_ALERTALTERNATE) {
			/* Remove folder and addresses */
			item = addrbook_remove_folder_delete(abf, item);
			if (item) {
				addritem_free_item_folder(item);
				addressbook_free_child_adapters(&iter);
				remFlag = TRUE;
			}
		}
	} else if (obj->type == ADDR_ITEM_GROUP) {
		AdapterGroup *adapter = ADAPTER_GROUP(obj);
		ItemGroup *item = adapter->itemGroup;

		item = addrbook_remove_group(abf, item);
		if (item) {
			addritem_free_item_group(item);
			remFlag = TRUE;
		}
	}

	if (remFlag) {
		/* Free up adapter and remove node. */
		addressbook_free_adapter(&iter);
		gtk_tree_model_iter_parent(model, &parent, &iter);
		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
		gtk_tree_selection_select_iter(selection, &parent);
	}
}

static void addressbook_new_address_cb(gpointer data, guint action, GtkWidget *widget)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	AddressObject *pobj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, COL_OBJ, &pobj, -1);
	if (pobj == NULL)
		return;
	ds = addressbook_find_datasource(&iter);
	if (ds == NULL)
		return;

	abf = ds->rawDataSource;
	if (abf == NULL)
		return;

	if (pobj->type == ADDR_DATASOURCE) {
		if (ADAPTER_DSOURCE(pobj)->subType == ADDR_BOOK) {
			/* New address */
			ItemPerson *person = addressbook_edit_person(abf, NULL, NULL, FALSE);
			if (person) {
				if (gtkut_tree_row_reference_equal(addrbook.tree_selected, addrbook.tree_opened)) {
					addressbook_reopen();
				}
				addressbook_modified();
			}
		}
	}
	else if( pobj->type == ADDR_ITEM_FOLDER ) {
		/* New address */
		ItemFolder *folder = ADAPTER_FOLDER(pobj)->itemFolder;
		ItemPerson *person = addressbook_edit_person(abf, folder, NULL, FALSE);
		if (person) {
			if (gtkut_tree_row_reference_equal(addrbook.tree_selected, addrbook.tree_opened)) {
				addressbook_reopen();
			}
			addressbook_modified();
		}
	}
	else if( pobj->type == ADDR_ITEM_GROUP ) {
		/* New address in group */
		ItemGroup *group = ADAPTER_GROUP(pobj)->itemGroup;

		if (addressbook_edit_group(abf, NULL, group) == NULL)
			return;
		if (gtkut_tree_row_reference_equal
			(addrbook.tree_selected, addrbook.tree_opened)) {
			/* Change node name in tree. */
			addressbook_change_node_name(&iter, ADDRITEM_NAME(group));
			addressbook_reopen();
		}
	}
}

static void addressbook_compose_to_cb(gpointer data, guint action, GtkWidget *widget)
{
	addressbook_to_clicked(NULL, GINT_TO_POINTER(action));
}

/*
* Search for specified group in address index tree.
*/
static gboolean addressbook_find_group_node(GtkTreeIter *parent, GtkTreeIter *iter, ItemGroup *group)
{
	GtkTreeModel *model;
	GtkTreeIter iter_;
	gboolean valid = TRUE;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.treeview));
	valid = gtk_tree_model_iter_children(model, &iter_, parent);
	while (valid) {
		AddressObject *obj = NULL;

		gtk_tree_model_get(model, &iter_, COL_OBJ, &obj, -1);
		if (obj && obj->type == ADDR_ITEM_GROUP) {
			ItemGroup *g = ADAPTER_GROUP(obj)->itemGroup;
			if (g == group) {
				*iter = iter_;
				return TRUE;
			}
		}
		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static AddressBookFile *addressbook_get_book_file(void)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	AddressBookFile *abf = NULL;
	AddressDataSource *ds = NULL;

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
		return NULL;
	ds = addressbook_find_datasource(&iter);
	if (ds == NULL)
		return NULL;
	if (ds->type == ADDR_IF_BOOK)
		abf = ds->rawDataSource;
	return abf;
}

static void addressbook_tree_remove_children(GtkTreeModel *model,
					     GtkTreeIter *parent)
{
	GtkTreeIter iter;

	/* Remove existing folders and groups */
	while (gtk_tree_model_iter_children(model, &iter, parent)) {
		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
	}
}

static void addressbook_move_nodes_recursive(GtkTreeIter *iter,
					     GtkTreeIter *parent)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeModel *model;
	GtkTreeIter child, new_iter;
	gboolean valid;

	model = gtk_tree_view_get_model(treeview);

	valid = gtk_tree_model_iter_children(model, &child, iter);
	while (valid) {
		gchar *name = NULL;
		AddressObject *obj = NULL;
		GdkPixbuf *pixbuf = NULL;
		GdkPixbuf *pixbuf_open = NULL;

		gtk_tree_model_get(model, &child, COL_FOLDER_NAME, &name,
				   COL_OBJ, &obj, COL_PIXBUF, &pixbuf,
				   COL_PIXBUF_OPEN, &pixbuf_open, -1);
		gtk_tree_store_append(GTK_TREE_STORE(model), &new_iter, parent);
		gtk_tree_store_set(GTK_TREE_STORE(model), &new_iter,
				   COL_FOLDER_NAME, name,
				   COL_OBJ, obj,
				   COL_PIXBUF, pixbuf,
				   COL_PIXBUF_OPEN, pixbuf_open, -1);
		g_free(name);
		addressbook_move_nodes_recursive(&child, &new_iter);
		valid = gtk_tree_model_iter_next(model, &child);
	}
}

static void addressbook_move_nodes_up(GtkTreeIter *iter)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeModel *model;
	GtkTreeIter parent;

	model = gtk_tree_view_get_model(treeview);
	if (!gtk_tree_model_iter_parent(model, &parent, iter))
		return;

	addressbook_move_nodes_recursive(iter, &parent);
}

static void addressbook_edit_address_cb(gpointer data, guint action, GtkWidget *widget)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter liter, iter, node, parent;
	gboolean node_found;
	gboolean parent_found = FALSE;
	GtkTreeView *listview = GTK_TREE_VIEW(addrbook.listview);
	GtkTreeModel *lmodel;
	AddressObject *obj = NULL, *pobj = NULL;
	AddressDataSource *ds = NULL;
	gchar *name = NULL;
	AddressBookFile *abf = NULL;

	if (addrbook.list_selected == NULL)
		return;

	lmodel = gtk_tree_view_get_model(listview);
	if (!gtkut_tree_row_reference_get_iter(lmodel, addrbook.list_selected,
					       &liter))
		return;
	gtk_tree_model_get(lmodel, &liter, COL_L_OBJ, &obj, -1);
	g_return_if_fail(obj != NULL);

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;
	gtk_tree_model_get(model, &iter, COL_OBJ, &pobj, -1);
	node_found = gtkut_tree_model_find_by_column_data
		(model, &node, &iter, COL_OBJ, obj);

	ds = addressbook_find_datasource(&iter);
	if (ds == NULL)
		return;

	abf = addressbook_get_book_file();
	if (abf == NULL)
		return;

	if (obj->type == ADDR_ITEM_EMAIL) {
		ItemEMail *email = (ItemEMail *)obj;
		ItemPerson *person;

		if (email == NULL)
			return;
		if (pobj && pobj->type == ADDR_ITEM_GROUP) {
			/* Edit parent group */
			AdapterGroup *adapter = ADAPTER_GROUP(pobj);
			ItemGroup *itemGrp = adapter->itemGroup;

			if (addressbook_edit_group(abf, NULL, itemGrp) == NULL)
				return;
			name = ADDRITEM_NAME(itemGrp);
			node = iter;
			node_found = TRUE;
			parent_found = gtk_tree_model_iter_parent(model, &parent, &node);
		} else {
			/* Edit person - email page */
			person = (ItemPerson *)ADDRITEM_PARENT(email);
			if (addressbook_edit_person(abf, NULL, person, TRUE) == NULL)
				return;
			addressbook_reopen();
			addressbook_modified();
			return;
		}
	} else if (obj->type == ADDR_ITEM_PERSON) {
		/* Edit person - basic page */
		ItemPerson *person = (ItemPerson *)obj;

		if (addressbook_edit_person(abf, NULL, person, FALSE) == NULL)
			return;
		addressbook_reopen();
		addressbook_modified();
		return;
	} else if (obj->type == ADDR_ITEM_GROUP) {
		ItemGroup *itemGrp = (ItemGroup *)obj;

		if (addressbook_edit_group(abf, NULL, itemGrp) == NULL)
			return;
		parent = iter;
		parent_found = TRUE;
		node_found = addressbook_find_group_node(&parent, &node, itemGrp);
		name = ADDRITEM_NAME(itemGrp);
	} else {
		return;
	}

	/* Update tree node with node name */
	if (node_found) {
		addressbook_change_node_name(&node, name);
		addressbook_reopen();
	}
}

static void addressbook_delete_address_cb(gpointer data, guint action,
					  GtkWidget *widget)
{
	addressbook_del_clicked(NULL, NULL);
}

static void addressbook_copy_address_cb(gpointer data, guint action,
					GtkWidget *widget)
{
	AddressObject *obj;
	GList *node;

	if (_addressListSelection_ == NULL)
		return;

	g_list_free(_clipObjectList_);
	_clipObjectList_ = NULL;

	node = _addressListSelection_;
	while (node) {
		obj = node->data;
		if (obj->type == ADDR_ITEM_PERSON) {
			_clipObjectList_ = g_list_append(_clipObjectList_, obj);
		}
		node = g_list_next(node);
	}

	addressbook_menuitem_set_sensitive();
}

static void addressbook_paste_address_cb(gpointer data, guint action,
					 GtkWidget *widget)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	AddressObject *obj = NULL, *pobj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *folder = NULL;
	ItemPerson *clipPerson, *person;
	GList *cur;
	GList *node;

	if (!_clipObjectList_)
		return;

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;
	gtk_tree_model_get(model, &iter, COL_OBJ, &pobj, -1);
	if (pobj->type == ADDR_ITEM_FOLDER)
		folder = ADAPTER_FOLDER(pobj)->itemFolder;
	else if (pobj->type == ADDR_DATASOURCE)
		folder = NULL;
	else
		return;

	ds = addressbook_find_datasource(&iter);
	if (ds == NULL)
		return;

	abf = ds->rawDataSource;
	if (abf == NULL)
		return;

	node = _clipObjectList_;
	while (node) {
		obj = node->data;
		node = g_list_next(node);
		if (obj->type != ADDR_ITEM_PERSON)
			continue;
		clipPerson = (ItemPerson *)obj;

		person = addrbook_add_address_list(abf, folder, NULL);
		ADDRITEM_NAME(person) = g_strdup(ADDRITEM_NAME(clipPerson));
		person->firstName = g_strdup(clipPerson->firstName);
		person->lastName = g_strdup(clipPerson->lastName);
		person->nickName = g_strdup(clipPerson->nickName);
		for (cur = clipPerson->listEMail; cur != NULL; cur = cur->next) {
			ItemEMail *email = cur->data;
			addritem_person_add_email(person, addritem_copy_item_email(email));
		}
		for (cur = clipPerson->listAttrib; cur != NULL; cur = cur->next) {
			UserAttribute *attr = cur->data;
			addritem_person_add_attribute(person, addritem_copy_attribute(attr));
		}
	}

	if (gtkut_tree_row_reference_equal(addrbook.tree_selected, addrbook.tree_opened))
		addressbook_reopen();
	addressbook_modified();
}

static void close_cb(gpointer data, guint action, GtkWidget *widget)
{
	addressbook_close();
}

static void addressbook_file_save_cb(gpointer data, guint action, GtkWidget *widget)
{
	addressbook_export_to_file();
}

static void addressbook_person_expand_node(GtkTreeView *treeview,
					   GtkTreeIter *iter, GtkTreePath *path,
					   gpointer *data)
{
	GtkTreeModel *model;
	ItemPerson *person = NULL;

	model = gtk_tree_view_get_model(treeview);
	gtk_tree_model_get(model, iter, COL_L_OBJ, &person, -1);
	if (person)
		addritem_person_set_opened(person, TRUE);
}

static void addressbook_person_collapse_node(GtkTreeView *treeview,
					     GtkTreeIter *iter,
					     GtkTreePath *path, gpointer *data)
{
	GtkTreeModel *model;
	ItemPerson *person = NULL;

	model = gtk_tree_view_get_model(treeview);
	gtk_tree_model_get(model, iter, COL_L_OBJ, &person, -1);
	if (person)
		addritem_person_set_opened(person, FALSE);
}

static void addressbook_drag_begin(GtkWidget *widget,
				   GdkDragContext *drag_context, gpointer data)
{
	list_on_drag = TRUE;
	/* gtk_drag_set_icon_default(drag_context); */
}

static void addressbook_drag_end(GtkWidget *widget,
				 GdkDragContext *drag_context, gpointer data)
{
}

static void addressbook_drag_data_get(GtkWidget *widget,
				      GdkDragContext *drag_context,
				      GtkSelectionData *selection_data,
				      guint info, guint time, gpointer data)
{
	if (info == DRAG_TYPE_OBJ) {
		gtk_selection_data_set(selection_data, selection_data->target,
				       8, (guchar *)"drag-from-list", 14);
	}
}

static gchar *addressbook_format_item_list(ItemPerson *person, ItemEMail *email)
{
	gchar *str = NULL;
	gchar *eMailAlias = ADDRITEM_NAME(email);

	if (eMailAlias && *eMailAlias != '\0') {
		if (person) {
			str = g_strdup_printf("%s - %s", ADDRITEM_NAME(person), eMailAlias);
		} else {
			str = g_strdup(eMailAlias);
		}
	}
	return str;
}

static gboolean addressbook_match_item(const gchar *name,
				       const gchar *email_alias,
				       const gchar *addr,
				       const gchar *remarks,
				       const gchar *str)
{
	if (!name)
		return FALSE;
	if (!str || str[0] == '\0')
		return TRUE;
	if (strcasestr(name, str))
		return TRUE;
	else if (email_alias && strcasestr(email_alias, str))
		return TRUE;
	else if (addr && strcasestr(addr, str))
		return TRUE;
	else if (remarks && strcasestr(remarks, str))
		return TRUE;

	return FALSE;
}

static void addressbook_load_group(ItemGroup *itemGroup)
{
	GtkTreeView *listview = GTK_TREE_VIEW(addrbook.listview);
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *items = itemGroup->listEMail;
	AddressTypeControlItem *atci = addrbookctl_lookup(ADDR_ITEM_EMAIL);
	const gchar *search_str;

	model = gtk_tree_view_get_model(listview);
	search_str = gtk_entry_get_text(GTK_ENTRY(addrbook.entry));

	for (; items != NULL; items = g_list_next(items)) {
		ItemEMail *email = items->data;
		ItemPerson *person;
		const gchar *name;
		gchar *str;

		if (!email) continue;

		person = (ItemPerson *)ADDRITEM_PARENT(email);
		if (!person) {
			g_warning("email %p (%s) don't have parent\n", email, email->address);
			continue;
		}
		if (!addressbook_match_item(ADDRITEM_NAME(person),
					    ADDRITEM_NAME(email),
					    email->address, email->remarks,
					    search_str))
			continue;

		str = addressbook_format_item_list(person, email);
		if (str) {
			name = str;
		} else {
			name = ADDRITEM_NAME(person);
		}
		gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
				   COL_NAME, name,
				   COL_ADDRESS, email->address ? email->address : "",
				   COL_NICKNAME, person->nickName ? person->nickName : "",
				   COL_REMARKS, email->remarks ? email->remarks : "",
				   COL_L_OBJ, email,
				   COL_L_PIXBUF, atci->icon_pixbuf,
				   -1);
		g_free(str);
	}
}

static void addressbook_folder_load_person(ItemFolder *itemFolder)
{
	GtkTreeView *listview = GTK_TREE_VIEW(addrbook.listview);
	GtkTreeModel *model;
	GList *items;
	AddressTypeControlItem *atci = addrbookctl_lookup(ADDR_ITEM_PERSON);
	AddressTypeControlItem *atciMail = addrbookctl_lookup(ADDR_ITEM_EMAIL);
	const gchar *search_str;

	if (atci == NULL)
		return;
	if (atciMail == NULL)
		return;

	model = gtk_tree_view_get_model(listview);
	search_str = gtk_entry_get_text(GTK_ENTRY(addrbook.entry));

	/* Load email addresses */
	items = addritem_folder_get_person_list(itemFolder);
	for (; items != NULL; items = g_list_next(items)) {
		GtkTreeIter iperson, iemail;
		gboolean flgFirst = TRUE, haveAddr = FALSE;
		ItemPerson *person;
		ItemEMail *email;
		GList *node;

		person = (ItemPerson *)items->data;
		if (!person)
			continue;

		node = person->listEMail;
		if (node && node->data) {
			email = node->data;
			if (!addressbook_match_item(ADDRITEM_NAME(person), ADDRITEM_NAME(email), email->address, email->remarks, search_str))
				continue;
		} else {
			if (!addressbook_match_item(ADDRITEM_NAME(person), NULL, NULL, NULL, search_str))
				continue;
		}

		while (node) {
			const gchar *name;

			email = node->data;
			node = g_list_next(node);

			if (flgFirst) {
				/* First email belongs with person */
				gchar *str = addressbook_format_item_list(person, email);
				if (str) {
					name = str;
				} else {
					name = ADDRITEM_NAME(person);
				}

				gtk_tree_store_append(GTK_TREE_STORE(model),
						      &iperson, NULL);
				gtk_tree_store_set(GTK_TREE_STORE(model),
						   &iperson,
						   COL_NAME, name,
						   COL_ADDRESS, email->address ? email->address : "",
						   COL_NICKNAME, person->nickName ? person->nickName : "",
						   COL_REMARKS, email->remarks ? email->remarks : "",
						   COL_L_OBJ, person,
						   COL_L_PIXBUF, atci->icon_pixbuf,
						   -1);
				g_free(str);
			} else {
				/* Subsequent email is a child node of person */
				name = ADDRITEM_NAME(email);
				gtk_tree_store_append(GTK_TREE_STORE(model),
						      &iemail, &iperson);
				gtk_tree_store_set(GTK_TREE_STORE(model),
						   &iemail,
						   COL_NAME, name,
						   COL_ADDRESS, email->address ? email->address : "",
						   COL_NICKNAME, "",
						   COL_REMARKS, email->remarks ? email->remarks : "",
						   COL_L_OBJ, email,
						   COL_L_PIXBUF, atciMail->icon_pixbuf,
						   -1);
			}
			flgFirst = FALSE;
			haveAddr = TRUE;
		}
		if (!haveAddr) {
			/* Have name without EMail */
			gtk_tree_store_append(GTK_TREE_STORE(model),
					      &iperson, NULL);
			gtk_tree_store_set(GTK_TREE_STORE(model),
					   &iperson,
					   COL_NAME, ADDRITEM_NAME(person),
					   COL_ADDRESS, "",
					   COL_NICKNAME, person->nickName ? person->nickName : "",
					   COL_REMARKS, "",
					   COL_L_OBJ, person,
					   COL_L_PIXBUF, atci->icon_pixbuf,
					   -1);
		}
		if (person->isOpened) {
			GtkTreePath *path;
			path = gtk_tree_model_get_path(model, &iperson);
			gtk_tree_view_expand_row(listview, path, TRUE);
			gtk_tree_path_free(path);
		}
	}

	/* Free up the list */
	mgu_clear_list(items);
	g_list_free(items);
}

static void addressbook_folder_load_group(ItemFolder *itemFolder)
{
	GtkTreeView *listview = GTK_TREE_VIEW(addrbook.listview);
	GtkTreeModel *model;
	GList *items;
	AddressTypeControlItem *atci = addrbookctl_lookup(ADDR_ITEM_GROUP);
	const gchar *search_str;

	if (!atci)
		return;

	model = gtk_tree_view_get_model(listview);
	search_str = gtk_entry_get_text(GTK_ENTRY(addrbook.entry));

	/* Load any groups */
	items = addritem_folder_get_group_list(itemFolder);
	for (; items != NULL; items = g_list_next(items)) {
		GtkTreeIter iter;
		ItemGroup *group = items->data;

		if (!group)
			continue;
		if (!addressbook_match_item(ADDRITEM_NAME(group),
					    NULL, NULL, NULL, search_str))
			continue;

		gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
				   COL_NAME, ADDRITEM_NAME(group),
				   COL_ADDRESS, "",
				   COL_NICKNAME, "",
				   COL_REMARKS, "",
				   COL_L_OBJ, group,
				   COL_L_PIXBUF, atci->icon_pixbuf,
				   -1);
	}

	/* Free up the list */
	mgu_clear_list(items);
	g_list_free(items);
}

static AddressDataSource *addressbook_find_datasource(GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkTreeIter iter_, parent;
	AddressDataSource *ds = NULL;
	AddressObject *ao;

	g_return_val_if_fail(addrbook.treeview != NULL, NULL);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.treeview));
	parent = *iter;

	do {
		iter_ = parent;
		if (gtk_tree_store_iter_depth(GTK_TREE_STORE(model), &iter_) < 1)
			return NULL;
		ao = NULL;
		gtk_tree_model_get(model, &iter_, COL_OBJ, &ao, -1);
		if (ao) {
			/* g_print("ao->type = %d\n", ao->type); */
			if (ao->type == ADDR_DATASOURCE) {
				AdapterDSource *ads = ADAPTER_DSOURCE(ao);
				/* g_print("found it\n"); */
				ds = ads->dataSource;
				break;
			}
		}
	} while (gtk_tree_model_iter_parent(model, &parent, &iter_));

	return ds;
}

/*
* Load address list widget with children of specified object.
* Enter: obj	Parent object to be loaded.
*/
static void addressbook_set_list(AddressObject *obj)
{
	GtkTreeView *listview = GTK_TREE_VIEW(addrbook.listview);
	GtkTreeModel *model;
	AddressDataSource *ds = NULL;
	AdapterDSource *ads = NULL;
	gboolean sorted;
	gint address_list_sort_id = COL_NAME;
	GtkSortType order = GTK_SORT_ASCENDING;

	debug_print("addressbook_set_list\n");

	model = gtk_tree_view_get_model(listview);

	if (obj == NULL) {
		gtk_tree_store_clear(GTK_TREE_STORE(model));
		return;
	}

	if (obj->type == ADDR_INTERFACE) {
		return;
	}

	gtk_tree_store_clear(GTK_TREE_STORE(model));
	sorted = gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(model),
						      &address_list_sort_id,
						      &order);
	if (!sorted) {
		address_list_sort_id = COL_NAME;
		order = GTK_SORT_ASCENDING;
	}
	gtkut_tree_sortable_unset_sort_column_id(GTK_TREE_SORTABLE(model));

	if (obj->type == ADDR_DATASOURCE) {
		ads = ADAPTER_DSOURCE(obj);
		ds = ADAPTER_DSOURCE(obj)->dataSource;
		if (ds) {
			/* Load root folder */
			ItemFolder *rootFolder = NULL;
			rootFolder = addrindex_ds_get_root_folder(ds);
			addressbook_folder_load_person(addrindex_ds_get_root_folder(ds));
			addressbook_folder_load_group(addrindex_ds_get_root_folder(ds));
		}
	} else if (obj->type == ADDR_ITEM_GROUP) {
		/* Load groups */
		ItemGroup *itemGroup = ADAPTER_GROUP(obj)->itemGroup;
		addressbook_load_group(itemGroup);
	} else if (obj->type == ADDR_ITEM_FOLDER) {
		/* Load folders */
		ItemFolder *itemFolder = ADAPTER_FOLDER(obj)->itemFolder;
		addressbook_folder_load_person(itemFolder);
		addressbook_folder_load_group(itemFolder);
	}

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
					     address_list_sort_id, order);
}

/*
* Free adaptor for specified node.
*/
static void addressbook_free_adapter(GtkTreeIter *iter)
{
	GtkTreeModel *model;
	AddressObject *ao = NULL;

	g_return_if_fail(addrbook.treeview != NULL);
	g_return_if_fail(iter != NULL);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.treeview));
	if (gtk_tree_store_iter_depth(GTK_TREE_STORE(model), iter) < 1)
		return;
	gtk_tree_model_get(model, iter, COL_OBJ, &ao, -1);
	if (ao == NULL)
		return;

	if (ao->type == ADDR_INTERFACE) {
		AdapterInterface *ai = ADAPTER_INTERFACE(ao);
		addrbookctl_free_interface(ai);
	} else if (ao->type == ADDR_DATASOURCE) {
		AdapterDSource *ads = ADAPTER_DSOURCE(ao);
		addrbookctl_free_datasource(ads);
	} else if (ao->type == ADDR_ITEM_FOLDER) {
		AdapterFolder *af = ADAPTER_FOLDER(ao);
		addrbookctl_free_folder(af);
	} else if (ao->type == ADDR_ITEM_GROUP) {
		AdapterGroup *ag = ADAPTER_GROUP(ao);
		addrbookctl_free_group(ag);
	}

	gtk_tree_store_set(GTK_TREE_STORE(model), iter, COL_OBJ, NULL, -1);
}

/*
* Free all children adapters.
*/
static void addressbook_free_child_adapters(GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkTreeIter child;
	gboolean valid;

	g_return_if_fail(iter != NULL);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.treeview));
	valid = gtk_tree_model_iter_children(model, &child, iter);
	while (valid) {
		addressbook_free_child_adapters(&child);
		addressbook_free_adapter(&child);
		valid = gtk_tree_model_iter_next(model, &child);
	}
}

AdapterDSource *addressbook_create_ds_adapter(AddressDataSource *ds,
					      AddressObjectType otype,
					      gchar *name)
{
	AdapterDSource *adapter = g_new0(AdapterDSource, 1);
	ADDRESS_OBJECT(adapter)->type = ADDR_DATASOURCE;
	ADDRESS_OBJECT_NAME(adapter) = g_strdup(name);
	adapter->dataSource = ds;
	adapter->subType = otype;
	return adapter;
}

void addressbook_ads_set_name(AdapterDSource *adapter, gchar *value)
{
	ADDRESS_OBJECT_NAME(adapter) =
		mgu_replace_string(ADDRESS_OBJECT_NAME(adapter), value);
}

/*
 * Load tree from address index with the initial data.
 */
static void addressbook_load_tree(void)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeModel *model;
	GtkTreeIter iter, new_iter;
	GtkTreePath *path;
	GList *nodeIf, *nodeDS;
	AdapterInterface *adapter;
	AddressInterface *iface;
	AddressTypeControlItem *atci;
	AddressDataSource *ds;
	AdapterDSource *ads;
	gchar *name;

	model = gtk_tree_view_get_model(treeview);

	nodeIf = _addressInterfaceList_;
	while (nodeIf) {
		adapter = nodeIf->data;
		gtkut_tree_row_reference_get_iter(model, adapter->tree_row, &iter);
		iface = adapter->iface;
		atci = adapter->atci;
		if (iface) {
			if (iface->useInterface) {
				/* Load data sources below interface node */
				nodeDS = iface->listSource;
				while (nodeDS) {
					ds = nodeDS->data;
					name = addrindex_ds_get_name(ds);
					ads = addressbook_create_ds_adapter(ds, atci->objectType, name);
					addressbook_add_object(&iter, &new_iter, ADDRESS_OBJECT(ads));
					nodeDS = g_list_next( nodeDS );
				}
				path = gtk_tree_model_get_path(model, &iter);
				gtk_tree_view_expand_row(treeview, path, TRUE);
				gtk_tree_path_free(path);
			}
		}
		nodeIf = g_list_next(nodeIf);
	}
}

/*
 * Convert the old address book to new format.
 */
static gboolean addressbook_convert(AddressIndex *addrIndex)
{
	gboolean retVal = FALSE;
	gboolean errFlag = TRUE;
	gchar *msg = NULL;

	/* Read old address book, performing conversion */
	debug_print( "Reading and converting old address book...\n" );
	addrindex_set_file_name( addrIndex, ADDRESSBOOK_OLD_FILE );
	addrindex_read_data( addrIndex );
	if( addrIndex->retVal == MGU_NO_FILE ) {
		/* We do not have a file - new user */
		debug_print( "New user... create new books...\n" );
		addrindex_create_new_books( addrIndex );
		if( addrIndex->retVal == MGU_SUCCESS ) {
			/* Save index file */
			addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
			addrindex_save_data( addrIndex );
			if( addrIndex->retVal == MGU_SUCCESS ) {
				retVal = TRUE;
				errFlag = FALSE;
			}
			else {
				msg = _( "New user, could not save index file." );
			}
		}
		else {
			msg = _( "New user, could not save address book files." );
		}
	}
	else {
		/* We have an old file */
		if( addrIndex->wasConverted ) {
			/* Converted successfully - save address index */
			addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
			addrindex_save_data( addrIndex );
			if( addrIndex->retVal == MGU_SUCCESS ) {
				msg = _( "Old address book converted successfully." );
				retVal = TRUE;
				errFlag = FALSE;
			}
			else {
				msg = _("Old address book converted,\n"
					"could not save new address index file" );
			}
		}
		else {
			/* File conversion failed - just create new books */
			debug_print( "File conversion failed... just create new books...\n" );
			addrindex_create_new_books( addrIndex );
			if( addrIndex->retVal == MGU_SUCCESS ) {
				/* Save index */
				addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
				addrindex_save_data( addrIndex );
				if( addrIndex->retVal == MGU_SUCCESS ) {
					msg = _("Could not convert address book,\n"
						"but created empty new address book files." );
					retVal = TRUE;
					errFlag = FALSE;
				}
				else {
					msg = _("Could not convert address book,\n"
						"could not create new address book files." );
				}
			}
			else {
				msg = _("Could not convert address book\n"
					"and could not create new address book files." );
			}
		}
	}
	if( errFlag ) {
		debug_print( "Error\n%s\n", msg );
		alertpanel( _( "Address book conversion error" ), msg, GTK_STOCK_CLOSE, NULL, NULL );
	}
	else if( msg ) {
		debug_print( "Warning\n%s\n", msg );
		alertpanel( _( "Address book conversion" ), msg, GTK_STOCK_CLOSE, NULL, NULL );
	}

	return retVal;
}

void addressbook_read_file(void)
{
	AddressIndex *addrIndex = NULL;

	debug_print( "Reading address index...\n" );
	if (_addressIndex_) {
		debug_print("address book already read!\n");
		return;
	}

	addrIndex = addrindex_create_index();

	/* Use new address book index. */
	addrindex_set_file_path(addrIndex, get_rc_dir());
	addrindex_set_file_name(addrIndex, ADDRESSBOOK_INDEX_FILE);
	addrindex_read_data(addrIndex);
	if (addrIndex->retVal == MGU_NO_FILE) {
		/* Conversion required */
		debug_print("Converting...\n");
		if (addressbook_convert(addrIndex)) {
			addrindex_create_extra_books(addrIndex);
			_addressIndex_ = addrIndex;
		}
	} else if (addrIndex->retVal == MGU_SUCCESS) {
		addrindex_create_extra_books(addrIndex);
		_addressIndex_ = addrIndex;
	} else {
		gchar msg[1024];

		/* Error reading address book */
		debug_print("Could not read address index.\n");
		addrindex_print_index(addrIndex, stdout);
		g_snprintf(msg, sizeof(msg),
			   _("Could not read address index:\n\n%s%c%s"),
			   addrIndex->filePath, G_DIR_SEPARATOR,
			   addrIndex->fileName);
		alertpanel_message(_("Address Book Error"), msg, ALERT_ERROR);
	}
	debug_print( "done.\n" );
}

#if 0
void addressbook_read_file_old( void ) {
	AddressIndex *addrIndex = NULL;
	gboolean errFlag = TRUE;
	gchar *msg = NULL;

	if( _addressIndex_ ) {
		debug_print( "address book already read!!!\n" );
		return;
	}

	addrIndex = addrindex_create_index();

	/* Use use new address book. */
	/* addrindex_set_file_path( addrIndex, "/home/match/tmp/empty-dir" ); */
	addrindex_set_file_path( addrIndex, get_rc_dir() );
	addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );

	debug_print( "Reading address index...\n" );
	addrindex_read_data( addrIndex );
	if( addrIndex->retVal == MGU_NO_FILE ) {
		/* Read old address book, performing conversion */
		debug_print( "Reading and converting old address book...\n" );
		addrindex_set_file_name( addrIndex, ADDRESSBOOK_OLD_FILE );
		addrindex_read_data( addrIndex );
		if( addrIndex->retVal == MGU_NO_FILE ) {
			/* We do not have a file - new user */
			debug_print( "New user... create new books...\n" );
			addrindex_create_new_books( addrIndex );
			if( addrIndex->retVal == MGU_SUCCESS ) {
				/* Save index file */
				addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
				addrindex_save_data( addrIndex );
				if( addrIndex->retVal == MGU_SUCCESS ) {
					errFlag = FALSE;
				}
				else {
					msg = g_strdup( _( "New user, could not save index file." ) );
				}
			}
			else {
				msg = g_strdup( _( "New user, could not save address book files." ) );
			}
		}
		else {
			/* We have an old file */
			if( addrIndex->wasConverted ) {
				/* Converted successfully - save address index */
				addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
				addrindex_save_data( addrIndex );
				if( addrIndex->retVal == MGU_SUCCESS ) {
					msg = g_strdup( _( "Old address book converted successfully." ) );
					errFlag = FALSE;
				}
				else {
					msg = g_strdup( _(
						"Old address book converted, " \
						"could not save new address index file" ) );
				}
			}
			else {
				/* File conversion failed - just create new books */
				debug_print( "File conversion failed... just create new books...\n" );
				addrindex_create_new_books( addrIndex );
				if( addrIndex->retVal == MGU_SUCCESS ) {
					/* Save index */
					addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
					addrindex_save_data( addrIndex );
					if( addrIndex->retVal == MGU_SUCCESS ) {
						msg = g_strdup( _(
							"Could not convert address book, " \
							"but created empty new address book files." ) );
						errFlag = FALSE;
					}
					else {
						msg = g_strdup( _(
							"Could not convert address book, " \
							"could not create new address book files." ) );
					}
				}
				else {
					msg = g_strdup( _(
						"Could not convert address book " \
						"and could not create new address book files." ) );
				}
			}
		}
	}
	else if( addrIndex->retVal == MGU_SUCCESS ) {
		errFlag = FALSE;
	}
	else {
		debug_print( "Could not read address index.\n" );
		addrindex_print_index( addrIndex, stdout );
		msg = g_strdup( _( "Could not read address index" ) );
	}
	_addressIndex_ = addrIndex;

	if( errFlag ) {
		debug_print( "Error\n%s\n", msg );
		alertpanel( _( "Address Book Conversion Error" ), msg,
			    GTK_STOCK_CLOSE, NULL, NULL );
	}
	else {
		if( msg ) {
			debug_print( "Warning\n%s\n", msg );
			alertpanel( _( "Address Book Conversion" ), msg,
				    GTK_STOCK_CLOSE, NULL, NULL );
		}
	}
	if( msg ) g_free( msg );
	debug_print( "done.\n" );
}
#endif

/*
* Add object into the address index tree widget.
* Enter: node	Parent node.
*        obj	Object to add.
* Return: Node that was added, or NULL if object not added.
*/
static gboolean addressbook_add_object(GtkTreeIter *iter, GtkTreeIter *new_iter,
				       AddressObject *obj)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeModel *model;
	GtkTreeIter added;
	AddressObject *pobj;
	AddressObjectType otype;
	AddressTypeControlItem *atci = NULL;
	const gchar *name;

	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(obj  != NULL, FALSE);

	model = gtk_tree_view_get_model(treeview);
	gtk_tree_model_get(model, iter, COL_OBJ, &pobj, -1);
	g_return_val_if_fail(pobj != NULL, FALSE);

	/* Determine object type to be displayed */
	if (obj->type == ADDR_DATASOURCE) {
		otype = ADAPTER_DSOURCE(obj)->subType;
	} else {
		otype = obj->type;
	}

	/* Handle any special conditions. */
	atci = addrbookctl_lookup(otype);
	if (atci && atci->showInTree) {
		/* Add object to tree */
		debug_print("addressbook_add_object: obj: %s\n", obj->name);
		if (otype == ADDR_BOOK && obj->name &&
		    !strcmp(obj->name, ADDR_DS_AUTOREG))
			name = _("Auto-registered address");
		else
			name = obj->name;
		gtk_tree_store_append(GTK_TREE_STORE(model), &added, iter);
		gtk_tree_store_set(GTK_TREE_STORE(model), &added,
				   COL_FOLDER_NAME, name,
				   COL_OBJ, obj,
				   COL_PIXBUF, atci->icon_pixbuf,
				   COL_PIXBUF_OPEN, atci->icon_open_pixbuf,
				   -1);
		if (new_iter)
			*new_iter = added;
		return TRUE;
	}

	return FALSE;
}

/*
* Add group into the address index tree.
* Enter: node	   Parent node.
*        ds        Data source.
*        itemGroup Group to add.
* Return: Inserted node.
*/
static gboolean addressbook_node_add_group(GtkTreeIter *iter, AddressDataSource *ds, ItemGroup *itemGroup, GtkTreeIter *new_iter)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeStore *store;
	GtkTreeIter new_iter_;
	AdapterGroup *adapter;
	AddressTypeControlItem *atci = NULL;
	gchar *name;

	if(ds == NULL) return FALSE;
	if(iter == NULL || itemGroup == NULL) return FALSE;

	name = itemGroup->obj.name;

	atci = addrbookctl_lookup(ADDR_ITEM_GROUP);

	adapter = g_new0(AdapterGroup, 1);
	ADDRESS_OBJECT_TYPE(adapter) = ADDR_ITEM_GROUP;
	ADDRESS_OBJECT_NAME(adapter) = g_strdup(ADDRITEM_NAME(itemGroup));
	adapter->itemGroup = itemGroup;

	debug_print("addressbook_node_add_group: name: %s\n", name);
	store = GTK_TREE_STORE(gtk_tree_view_get_model(treeview));
	gtk_tree_store_append(store, &new_iter_, iter);
	gtk_tree_store_set(store, &new_iter_,
			   COL_FOLDER_NAME, name,
			   COL_OBJ, adapter,
			   COL_PIXBUF, atci->icon_pixbuf,
			   COL_PIXBUF_OPEN, atci->icon_open_pixbuf,
			   -1);
	if (new_iter)
		*new_iter = new_iter_;
	return TRUE;
}

/*
* Add folder into the address index tree.
* Enter: iter	    Parent node.
*        ds         Data source.
*        itemFolder Folder to add.
*        otype      Object type to display.
*        new_iter   Inserted node.
* Return: TRUE if inserted.
*/
static gboolean addressbook_node_add_folder(GtkTreeIter *iter, AddressDataSource *ds, ItemFolder *itemFolder, AddressObjectType otype, GtkTreeIter *new_iter)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeStore *store;
	GtkTreeIter new_iter_;
	AdapterFolder *adapter;
	AddressTypeControlItem *atci = NULL;
	GList *listItems = NULL;
	gchar *name;
	ItemFolder *rootFolder;

	if (ds == NULL)
		return FALSE;
	if (iter == NULL || itemFolder == NULL)
		return FALSE;

	/* Determine object type */
	atci = addrbookctl_lookup(otype);
	if (atci == NULL)
		return FALSE;

	store = GTK_TREE_STORE(gtk_tree_view_get_model(treeview));

	rootFolder = addrindex_ds_get_root_folder(ds);
	if (itemFolder == rootFolder) {
		new_iter_ = *iter;
	} else {
		name = itemFolder->obj.name;

		adapter = g_new0(AdapterFolder, 1);
		ADDRESS_OBJECT_TYPE(adapter) = ADDR_ITEM_FOLDER;
		ADDRESS_OBJECT_NAME(adapter) = g_strdup(ADDRITEM_NAME(itemFolder));
		adapter->itemFolder = itemFolder;

		debug_print("addressbook_node_add_folder: name: %s\n", name);
		gtk_tree_store_append(store, &new_iter_, iter);
		gtk_tree_store_set(store, &new_iter_,
				   COL_FOLDER_NAME, name,
				   COL_OBJ, adapter,
				   COL_PIXBUF, atci->icon_pixbuf,
				   COL_PIXBUF_OPEN, atci->icon_open_pixbuf,
				   -1);
	}

	listItems = itemFolder->listFolder;
	while (listItems) {
		ItemFolder *item = listItems->data;
		addressbook_node_add_folder(&new_iter_, ds, item, otype, NULL);
		listItems = g_list_next(listItems);
	}
	listItems = itemFolder->listGroup;
	while (listItems) {
		ItemGroup *item = listItems->data;
		addressbook_node_add_group(&new_iter_, ds, item, NULL);
		listItems = g_list_next(listItems);
	}
	if (new_iter)
		*new_iter = new_iter_;
	return TRUE;
}

#if 0
static void addressbook_delete_object(AddressObject *obj) {
	AdapterDSource *ads = NULL;
	AddressDataSource *ds = NULL;
	if (!obj) return;

	/* Remove data source. */
	/* printf( "Delete obj type : %d\n", obj->type ); */

	ads = ADAPTER_DSOURCE(obj);
	if( ads == NULL ) return;
	ds = ads->dataSource;
	if( ds == NULL ) return;

	/* Remove data source */
	if( addrindex_index_remove_datasource( _addressIndex_, ds ) ) {
		addrindex_free_datasource( _addressIndex_, ds );
	}
	/* Free up Adapter object */
	g_free( ADAPTER_DSOURCE(obj) );
}
#endif

void addressbook_export_to_file( void ) {
	if( _addressIndex_ ) {
		/* Save all new address book data */
		debug_print( "Saving address books...\n" );
		addrindex_save_all_books( _addressIndex_ );

		debug_print( "Exporting addressbook to file...\n" );
		addrindex_save_data( _addressIndex_ );
		if( _addressIndex_->retVal != MGU_SUCCESS ) {
			addrindex_print_index( _addressIndex_, stdout );
		}

		/* Notify address completion of new data */
		addressbook_modified();
	}
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		addressbook_close();
	return FALSE;
}

static void size_allocated(GtkWidget *widget, GtkAllocation *allocation,
			   gpointer data)
{
	if (allocation->width <= 1 || allocation->height <= 1)
		return;

	prefs_common.addressbook_width = allocation->width;
	prefs_common.addressbook_height = allocation->height;
}

/*
* Comparison using cell contents (text in first column).
*/
static gint addressbook_tree_compare(GtkTreeModel *model, GtkTreeIter *a,
				     GtkTreeIter *b, gpointer data)
{
	gchar *name1 = NULL, *name2 = NULL;
	AddressObject *obj1 = NULL, *obj2 = NULL;
	gint ret;

	gtk_tree_model_get(model, a, COL_FOLDER_NAME, &name1, COL_OBJ, &obj1,
			   -1);
	gtk_tree_model_get(model, b, COL_FOLDER_NAME, &name2, COL_OBJ, &obj2,
			   -1);

	/* Do not sort toplevel row */
	if (obj1 && obj1->type == ADDR_INTERFACE)
		return 0;

	if (!name1 || !name2) {
		if (!name1)
			ret = (name2 != NULL);
		else
			ret = -1;
		g_free(name2);
		g_free(name1);
		return ret;
	}
	ret = g_ascii_strcasecmp(name1, name2);
	g_free(name2);
	g_free(name1);
	return ret;
}

static gint addressbook_list_col_compare(GtkTreeModel *model, GtkTreeIter *a,
					 GtkTreeIter *b, gint col)
{
	gchar *name1 = NULL, *name2 = NULL;
	gint ret;

	gtk_tree_model_get(model, a, col, &name1, -1);
	gtk_tree_model_get(model, b, col, &name2, -1);

	if (!name1)
		name1 = g_strdup("");
	if (!name2)
		name2 = g_strdup("");
	ret = g_ascii_strcasecmp(name1, name2);
	g_free(name2);
	g_free(name1);
	return ret;
}

static gint addressbook_list_name_compare(GtkTreeModel *model, GtkTreeIter *a,
					  GtkTreeIter *b, gpointer data)
{
	gint ret;

	ret = addressbook_list_col_compare(model, a, b, COL_NAME);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_NICKNAME);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_ADDRESS);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_REMARKS);

	return ret;
}

static gint addressbook_list_address_compare(GtkTreeModel *model,
					     GtkTreeIter *a, GtkTreeIter *b,
					     gpointer data)
{
	gint ret;

	ret = addressbook_list_col_compare(model, a, b, COL_ADDRESS);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_NAME);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_NICKNAME);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_REMARKS);

	return ret;
}

static gint addressbook_list_nickname_compare(GtkTreeModel *model,
					      GtkTreeIter *a, GtkTreeIter *b,
					      gpointer data)
{
	gint ret;

	ret = addressbook_list_col_compare(model, a, b, COL_NICKNAME);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_NAME);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_ADDRESS);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_REMARKS);

	return ret;
}

static gint addressbook_list_remarks_compare(GtkTreeModel *model,
					     GtkTreeIter *a, GtkTreeIter *b,
					     gpointer data)
{
	gint ret;

	ret = addressbook_list_col_compare(model, a, b, COL_REMARKS);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_NAME);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_NICKNAME);
	if (ret == 0)
		ret = addressbook_list_col_compare(model, a, b, COL_ADDRESS);

	return ret;
}

static gboolean addressbook_list_select_func(GtkTreeSelection *selection,
					     GtkTreeModel *model,
					     GtkTreePath *path,
					     gboolean cur_selected,
					     gpointer data)
{
	return can_toggle_list_selection;
}

/* static */ 
gint addressbook_obj_name_compare(gconstpointer a, gconstpointer b)
{
	const AddressObject *obj = a;
	const gchar *name = b;
	AddressTypeControlItem *atci = NULL;

	if (!obj || !name) return -1;

	atci = addrbookctl_lookup( obj->type );
	if( ! atci ) return -1;
	if( ! obj->name ) return -1;
	return g_ascii_strcasecmp(obj->name, name);
}

#if 0
static void addressbook_book_show_message( AddressBookFile *abf ) {
	gchar msgbuf[ADDRESSBOOK_MSGBUF_SIZE] = "";

	if (abf) {
		if (abf->retVal == MGU_SUCCESS) {
			g_snprintf(msgbuf, sizeof(msgbuf), "%s", abf->name);
		} else {
			g_snprintf(msgbuf, sizeof(msgbuf), "%s: %s", abf->name, mgu_error2string(abf->retVal));
		}
	}
	addressbook_status_show(msgbuf);
}
#endif

static void addressbook_new_book_cb(gpointer data, guint action, GtkWidget *widget)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	AdapterDSource *ads;
	AdapterInterface *adapter;

	adapter = addrbookctl_find_interface(ADDR_IF_BOOK);
	if (adapter == NULL)
		return;
	if (!gtkut_tree_row_reference_equal(addrbook.tree_selected, adapter->tree_row))
		return;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.treeview));
	gtkut_tree_row_reference_get_iter(model, addrbook.tree_selected, &iter);

	ads = addressbook_edit_book(_addressIndex_, NULL);
	if (ads) {
		addressbook_add_object(&iter, NULL, ADDRESS_OBJECT(ads));
		if (gtkut_tree_row_reference_equal(addrbook.tree_selected, addrbook.tree_opened)) {
			addressbook_reopen();
		}
	}
}

static void addressbook_new_vcard_cb(gpointer data, guint action, GtkWidget *widget)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	AdapterDSource *ads;
	AdapterInterface *adapter;

	adapter = addrbookctl_find_interface(ADDR_IF_VCARD);
	if (adapter == NULL)
		return;
	if (!gtkut_tree_row_reference_equal(addrbook.tree_selected, adapter->tree_row))
		return;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.treeview));
	gtkut_tree_row_reference_get_iter(model, addrbook.tree_selected, &iter);

	ads = addressbook_edit_vcard(_addressIndex_, NULL);
	if (ads) {
		addressbook_add_object(&iter, NULL, ADDRESS_OBJECT(ads));
		if (gtkut_tree_row_reference_equal(addrbook.tree_selected, addrbook.tree_opened)) {
			addressbook_reopen();
		}
		addressbook_modified();
	}
}

#if 0
static void addressbook_vcard_show_message( VCardFile *vcf ) {
	gchar msgbuf[ADDRESSBOOK_MSGBUF_SIZE] = "";

	if (vcf) {
		if (vcf->retVal == MGU_SUCCESS) {
			g_snprintf(msgbuf, sizeof(msgbuf), "%s", vcf->name);
		} else {
			g_snprintf(msgbuf, sizeof(msgbuf), "%s: %s", vcf->name, mgu_error2string(vcf->retVal));
		}
	}
	addressbook_status_show(msgbuf);
}
#endif

#ifdef USE_JPILOT
static void addressbook_new_jpilot_cb(gpointer data, guint action, GtkWidget *widget)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	AdapterDSource *ads;
	AdapterInterface *adapter;
	AddressInterface *iface;

	adapter = addrbookctl_find_interface(ADDR_IF_JPILOT);
	if (adapter == NULL)
		return;
	if (!gtkut_tree_row_reference_equal(addrbook.tree_selected, adapter->tree_row))
		return;
	iface = adapter->iface;
	if (!iface->haveLibrary)
		return;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.treeview));
	gtkut_tree_row_reference_get_iter(model, addrbook.tree_selected, &iter);

	ads = addressbook_edit_jpilot(_addressIndex_, NULL);
	if (ads) {
		addressbook_add_object(&iter, NULL, ADDRESS_OBJECT(ads));
		if (gtkut_tree_row_reference_equal(addrbook.tree_selected, addrbook.tree_opened)) {
			addressbook_reopen();
		}
	}
}

#if 0
static void addressbook_jpilot_show_message( JPilotFile *jpf ) {
	gchar msgbuf[ADDRESSBOOK_MSGBUF_SIZE] = "";

	if (jpf) {
		if (jpf->retVal == MGU_SUCCESS) {
			g_snprintf(msgbuf, sizeof(msgbuf), "%s", jpf->name);
		} else {
			g_snprintf(msgbuf, sizeof(msgbuf), "%s: %s", jpf->name, mgu_error2string(jpf->retVal));
		}
	}
	addressbook_status_show(msgbuf);
}
#endif
#endif /* USE_JPILOT */

#ifdef USE_LDAP
static void addressbook_new_ldap_cb(gpointer data, guint action, GtkWidget *widget)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	AdapterDSource *ads;
	AdapterInterface *adapter;
	AddressInterface *iface;

	adapter = addrbookctl_find_interface(ADDR_IF_LDAP);
	if (adapter == NULL)
		return;
	if (!gtkut_tree_row_reference_equal(addrbook.tree_selected, adapter->tree_row))
		return;
	iface = adapter->iface;
	if (!iface->haveLibrary)
		return;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(addrbook.treeview));
	gtkut_tree_row_reference_get_iter(model, addrbook.tree_selected, &iter);

	ads = addressbook_edit_ldap(_addressIndex_, NULL);
	if( ads ) {
		addressbook_add_object(&iter, NULL, ADDRESS_OBJECT(ads));
		if (gtkut_tree_row_reference_equal(addrbook.tree_selected, addrbook.tree_opened)) {
			addressbook_reopen();
		}
	}
}

static void addressbook_ldap_show_message(SyldapServer *svr)
{
	gchar msgbuf[ADDRESSBOOK_MSGBUF_SIZE] = "";

	if (svr) {
		if (svr->busyFlag) {
			g_snprintf(msgbuf, sizeof(msgbuf), "%s: %s", svr->name, ADDRESSBOOK_LDAP_BUSYMSG);
		} else {
			if (svr->retVal == MGU_SUCCESS) {
				g_snprintf(msgbuf, sizeof(msgbuf), "%s", svr->name);
			} else {
				g_snprintf(msgbuf, sizeof(msgbuf), "%s: %s", svr->name, mgu_error2string(svr->retVal));
			}
		}
	}
	addressbook_status_show(msgbuf);
}

static void ldapsearch_callback(SyldapServer *sls)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	AddressObject *obj;
	AdapterDSource *ads = NULL;
	AddressDataSource *ds = NULL;
	AddressInterface *iface = NULL;

	if (sls == NULL)
		return;

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;
	if (gtk_tree_store_iter_depth(GTK_TREE_STORE(model), &iter) == 0)
		return;

	gtk_tree_model_get(model, &iter, COL_OBJ, &obj, -1);
	if (obj == NULL)
		return;
	if (obj->type == ADDR_DATASOURCE) {
		ads = ADAPTER_DSOURCE(obj);
		if (ads->subType == ADDR_LDAP) {
			SyldapServer *server;

			ds = ads->dataSource;
			if (ds == NULL)
				return;
			iface = ds->iface;
			if (!iface->haveLibrary)
				return;
			server = ds->rawDataSource;
			if (server == sls) {
				/* Read from cache */
				gtk_widget_show(addrbook.window);
				addressbook_set_list(obj);
				addressbook_ldap_show_message(sls);
				gtk_widget_show(addrbook.window);
				gtk_entry_set_text(GTK_ENTRY(addrbook.entry), "");
			}
		}
	}
}
#endif

/*
 * Lookup button handler.
 */
static void addressbook_lup_clicked(GtkButton *button, gpointer data)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	AddressObject *obj;
	AdapterDSource *ads = NULL;
#ifdef USE_LDAP
	AddressDataSource *ds = NULL;
	AddressInterface *iface = NULL;
#endif /* USE_LDAP */
	gchar *sLookup;

	selection = gtk_tree_view_get_selection(treeview);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;
	if (gtk_tree_store_iter_depth(GTK_TREE_STORE(model), &iter) == 0)
		return;

	gtk_tree_model_get(model, &iter, COL_OBJ, &obj, -1);
	if (obj == NULL)
		return;

	sLookup = gtk_editable_get_chars(GTK_EDITABLE(addrbook.entry), 0, -1);
	g_strstrip(sLookup);

	if (obj->type == ADDR_DATASOURCE) {
		ads = ADAPTER_DSOURCE(obj);
#ifdef USE_LDAP
		if (ads->subType == ADDR_LDAP) {
			SyldapServer *server;

			ds = ads->dataSource;
			if (ds == NULL)
				return;
			iface = ds->iface;
			if (!iface->haveLibrary)
				return;
			server = ds->rawDataSource;
			if (server) {
				syldap_cancel_read(server);
				if (*sLookup == '\0' || strlen(sLookup) < 1 )
					return;
				syldap_set_search_value(server, sLookup);
				syldap_set_callback(server, ldapsearch_callback);
				syldap_read_data_th(server);
				addressbook_ldap_show_message(server);
			}
		} else
#endif /* USE_LDAP */
			addressbook_set_list(obj);
	} else {
		addressbook_set_list(obj);
	}

	g_free(sLookup);
}

static void addressbook_close_clicked(GtkButton	*button, gpointer data)
{
	addressbook_close();
}

/* **********************************************************************
* Build lookup tables.
* ***********************************************************************
*/

/*
* Build table that controls the rendering of object types.
*/
void addrbookctl_build_map(GtkWidget *window)
{
	AddressTypeControlItem *atci;

	/* Build icons */
	stock_pixbuf_gdk(window, STOCK_PIXMAP_FOLDER_CLOSE, &folderpix);
	stock_pixbuf_gdk(window, STOCK_PIXMAP_FOLDER_OPEN, &folderopenpix);
	stock_pixbuf_gdk(window, STOCK_PIXMAP_GROUP, &grouppix);
	stock_pixbuf_gdk(window, STOCK_PIXMAP_VCARD, &vcardpix);
	stock_pixbuf_gdk(window, STOCK_PIXMAP_BOOK, &bookpix);
	stock_pixbuf_gdk(window, STOCK_PIXMAP_PERSON, &personpix);
	stock_pixbuf_gdk(window, STOCK_PIXMAP_ADDRESS, &addresspix);
	stock_pixbuf_gdk(window, STOCK_PIXMAP_JPILOT, &jpilotpix);
	stock_pixbuf_gdk(window, STOCK_PIXMAP_CATEGORY, &categorypix);
	stock_pixbuf_gdk(window, STOCK_PIXMAP_LDAP, &ldappix);

	_addressBookTypeHash_ = g_hash_table_new(g_int_hash, g_int_equal);
	_addressBookTypeList_ = NULL;

	/* Interface */
	atci = g_new0(AddressTypeControlItem, 1);
	atci->objectType = ADDR_INTERFACE;
	atci->interfaceType = ADDR_IF_NONE;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = FALSE;
	atci->displayName = _("Interface");
	atci->icon_pixbuf = folderpix;
	atci->icon_open_pixbuf = folderopenpix;
	atci->menuCommand = NULL;
	g_hash_table_insert(_addressBookTypeHash_, &atci->objectType, atci);
	_addressBookTypeList_ = g_list_append(_addressBookTypeList_, atci);

	/* Address book */
	atci = g_new0(AddressTypeControlItem, 1);
	atci->objectType = ADDR_BOOK;
	atci->interfaceType = ADDR_IF_BOOK;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = FALSE;
	atci->displayName = _("Address Book");
	atci->icon_pixbuf = bookpix;
	atci->icon_open_pixbuf = bookpix;
	atci->menuCommand = "/File/New Book";
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Item person */
	atci = g_new0(AddressTypeControlItem, 1);
	atci->objectType = ADDR_ITEM_PERSON;
	atci->interfaceType = ADDR_IF_NONE;
	atci->showInTree = FALSE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = FALSE;
	atci->displayName = _("Person");
	atci->icon_pixbuf = personpix;
	atci->icon_open_pixbuf = personpix;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Item email */
	atci = g_new0(AddressTypeControlItem, 1);
	atci->objectType = ADDR_ITEM_EMAIL;
	atci->interfaceType = ADDR_IF_NONE;
	atci->showInTree = FALSE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = TRUE;
	atci->displayName = _("EMail Address");
	atci->icon_pixbuf = addresspix;
	atci->icon_open_pixbuf = addresspix;
	atci->menuCommand = NULL;
	g_hash_table_insert(_addressBookTypeHash_, &atci->objectType, atci);
	_addressBookTypeList_ = g_list_append(_addressBookTypeList_, atci);

	/* Item group */
	atci = g_new0(AddressTypeControlItem, 1);
	atci->objectType = ADDR_ITEM_GROUP;
	atci->interfaceType = ADDR_IF_BOOK;
	atci->showInTree = TRUE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = FALSE;
	atci->displayName = _("Group");
	atci->icon_pixbuf = grouppix;
	atci->icon_open_pixbuf = grouppix;
	atci->menuCommand = NULL;
	g_hash_table_insert(_addressBookTypeHash_, &atci->objectType, atci);
	_addressBookTypeList_ = g_list_append(_addressBookTypeList_, atci);

	/* Item folder */
	atci = g_new0(AddressTypeControlItem, 1);
	atci->objectType = ADDR_ITEM_FOLDER;
	atci->interfaceType = ADDR_IF_BOOK;
	atci->showInTree = TRUE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = FALSE;
	atci->displayName = _("Folder");
	atci->icon_pixbuf = folderpix;
	atci->icon_open_pixbuf = folderopenpix;
	atci->menuCommand = NULL;
	g_hash_table_insert(_addressBookTypeHash_, &atci->objectType, atci);
	_addressBookTypeList_ = g_list_append(_addressBookTypeList_, atci);

	/* vCard */
	atci = g_new0(AddressTypeControlItem, 1);
	atci->objectType = ADDR_VCARD;
	atci->interfaceType = ADDR_IF_VCARD;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = TRUE;
	atci->displayName = _("vCard");
	atci->icon_pixbuf = vcardpix;
	atci->icon_open_pixbuf = vcardpix;
	atci->menuCommand = "/File/New vCard";
	g_hash_table_insert(_addressBookTypeHash_, &atci->objectType, atci);
	_addressBookTypeList_ = g_list_append(_addressBookTypeList_, atci);

	/* JPilot */
	atci = g_new0(AddressTypeControlItem, 1);
	atci->objectType = ADDR_JPILOT;
	atci->interfaceType = ADDR_IF_JPILOT;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = FALSE;
	atci->displayName = _("JPilot");
	atci->icon_pixbuf = jpilotpix;
	atci->icon_open_pixbuf = jpilotpix;
	atci->menuCommand = "/File/New JPilot";
	g_hash_table_insert(_addressBookTypeHash_, &atci->objectType, atci);
	_addressBookTypeList_ = g_list_append(_addressBookTypeList_, atci);

	/* Category */
	atci = g_new0(AddressTypeControlItem, 1);
	atci->objectType = ADDR_CATEGORY;
	atci->interfaceType = ADDR_IF_JPILOT;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = TRUE;
	atci->displayName = _("JPilot");
	atci->icon_pixbuf = categorypix;
	atci->icon_open_pixbuf = categorypix;
	atci->menuCommand = NULL;
	g_hash_table_insert(_addressBookTypeHash_, &atci->objectType, atci);
	_addressBookTypeList_ = g_list_append(_addressBookTypeList_, atci);

	/* LDAP Server */
	atci = g_new0(AddressTypeControlItem, 1);
	atci->objectType = ADDR_LDAP;
	atci->interfaceType = ADDR_IF_LDAP;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = TRUE;
	atci->displayName = _("LDAP Server");
	atci->icon_pixbuf = ldappix;
	atci->icon_open_pixbuf = ldappix;
	atci->menuCommand = "/File/New LDAP Server";
	g_hash_table_insert(_addressBookTypeHash_, &atci->objectType, atci);
	_addressBookTypeList_ = g_list_append(_addressBookTypeList_, atci);
}

/*
* Search for specified object type.
*/
AddressTypeControlItem *addrbookctl_lookup(gint ot)
{
	gint objType = ot;
	return (AddressTypeControlItem *)g_hash_table_lookup(_addressBookTypeHash_, &objType);
}

/*
* Search for specified interface type.
*/
AddressTypeControlItem *addrbookctl_lookup_iface( AddressIfType ifType ) {
	GList *node = _addressBookTypeList_;
	while( node ) {
		AddressTypeControlItem *atci = node->data;
		if( atci->interfaceType == ifType ) return atci;
		node = g_list_next( node );
	}
	return NULL;
}

static void addrbookctl_free_address( AddressObject *obj ) {
	g_free( obj->name );
	obj->type = ADDR_NONE;
	obj->name = NULL;
}

static void addrbookctl_free_interface(AdapterInterface *adapter)
{
	addrbookctl_free_address(ADDRESS_OBJECT(adapter));
	adapter->iface = NULL;
	adapter->interfaceType = ADDR_IF_NONE;
	adapter->atci = NULL;
	adapter->enabled = FALSE;
	adapter->haveLibrary = FALSE;
	if (adapter->tree_row) {
		gtk_tree_row_reference_free(adapter->tree_row);
		adapter->tree_row = NULL;
	}
	g_free(adapter);
}

static void addrbookctl_free_datasource( AdapterDSource *adapter ) {
	addrbookctl_free_address( ADDRESS_OBJECT(adapter) );
	adapter->dataSource = NULL;
	adapter->subType = ADDR_NONE;
	g_free( adapter );
}

static void addrbookctl_free_folder( AdapterFolder *adapter ) {
	addrbookctl_free_address( ADDRESS_OBJECT(adapter) );
	adapter->itemFolder = NULL;
	g_free( adapter );
}

static void addrbookctl_free_group( AdapterGroup *adapter ) {
	addrbookctl_free_address( ADDRESS_OBJECT(adapter) );
	adapter->itemGroup = NULL;
	g_free( adapter );
}

/*
 * Build GUI interface list.
 */
void addrbookctl_build_iflist(void)
{
	AddressTypeControlItem *atci;
	AdapterInterface *adapter;
	GList *list = NULL;

	if(_addressIndex_ == NULL) {
		_addressIndex_ = addrindex_create_index();
	}
	_addressInterfaceList_ = NULL;
	list = addrindex_get_interface_list(_addressIndex_);
	while (list) {
		AddressInterface *iface = list->data;
		atci = addrbookctl_lookup_iface(iface->type);
		if (atci) {
			adapter = g_new0(AdapterInterface, 1);
			adapter->interfaceType = iface->type;
			adapter->atci = atci;
			adapter->iface = iface;
			adapter->tree_row = NULL;
			adapter->enabled = TRUE;
			adapter->haveLibrary = iface->haveLibrary;
			ADDRESS_OBJECT(adapter)->type = ADDR_INTERFACE;
			ADDRESS_OBJECT_NAME(adapter) = g_strdup(atci->displayName);
			_addressInterfaceList_ = g_list_append(_addressInterfaceList_, adapter);
		}
		list = g_list_next(list);
	}
}

void addrbookctl_free_selection( GList *list ) {
	GList *node = list;
	while( node ) {
		AdapterInterface *adapter = node->data;
		adapter = NULL;
		node = g_list_next( node );
	}
	g_list_free( list );
}

/*
* Find GUI interface type specified interface type.
* Return: Interface item, or NULL if not found.
*/
AdapterInterface *addrbookctl_find_interface( AddressIfType ifType ) {
	GList *node = _addressInterfaceList_;
	while( node ) {
		AdapterInterface *adapter = node->data;
		if( adapter->interfaceType == ifType ) return adapter;
		node = g_list_next( node );
	}
	return NULL;
}

/*
* Build interface list selection.
*/
void addrbookctl_build_ifselect(void)
{
	GList *newList = NULL;
	gchar *selectStr;
	gchar **splitStr;
	gint ifType;
	gint i;
	gchar *endptr = NULL;
	gboolean enabled;
	AdapterInterface *adapter;
	/* GList *node; */

	selectStr = g_strdup( ADDRESSBOOK_IFACE_SELECTION );

	/* Parse string */
	splitStr = g_strsplit( selectStr, ",", -1 );
	for( i = 0; i < ADDRESSBOOK_MAX_IFACE; i++ ) {
		if( splitStr[i] ) {
			/* printf( "%d : %s\n", i, splitStr[i] ); */
			ifType = strtol( splitStr[i], &endptr, 10 );
			enabled = TRUE;
			if( *endptr ) {
				if( strcmp( endptr, "/n" ) == 0 ) {
					enabled = FALSE;
				}
			}
			/* printf( "\t%d : %s\n", ifType, enabled ? "yes" : "no" ); */
			adapter = addrbookctl_find_interface( ifType );
			if( adapter ) {
				newList = g_list_append( newList, adapter );
			}
		}
		else {
			break;
		}
	}
	/* printf( "i=%d\n", i ); */
	g_strfreev( splitStr );
	g_free( selectStr );

	/* Replace existing list */
	mgu_clear_list( _addressIFaceSelection_ );
	g_list_free( _addressIFaceSelection_ );
	_addressIFaceSelection_ = newList;
	newList = NULL;

}

/* **********************************************************************
* Add sender to address book.
* ***********************************************************************
*/

/*
 * This function is used by the Add sender to address book function.
 */
gboolean addressbook_add_contact(const gchar *name, const gchar *address, const gchar *remarks)
{
	debug_print("addressbook_add_contact: name/address: %s <%s>\n", name ? name : "", address);
	if (addressadd_selection(_addressIndex_, name, address, remarks)) {
		debug_print("addressbook_add_contact - added\n");
		addressbook_refresh();
	}
	return TRUE;
}

/*
 * This function is used by the automatic address registration.
 */
gboolean addressbook_add_contact_autoreg(const gchar *name, const gchar *address, const gchar *remarks)
{
	debug_print("addressbook_add_contact_autoreg: name/address: %s <%s>\n", name ? name : "", address);
	if (addressadd_autoreg(_addressIndex_, name, address, remarks)) {
		addressbook_refresh();
	}
	return TRUE;
}

/* **********************************************************************
* Address completion support.
* ***********************************************************************
*/

/*
* This function is used by the address completion function to load
* addresses.
* Enter: callBackFunc Function to be called when an address is
*                     to be loaded.
* Return: TRUE if data loaded, FALSE if address index not loaded.
*/
gboolean addressbook_load_completion_full(AddressBookCompletionFunc func)
{
	/* AddressInterface *interface; */
	AddressDataSource *ds;
	GList *nodeIf, *nodeDS;
	GList *listP, *nodeP;
	GList *nodeM;
	gchar *sName, *sAddress, *sAlias, *sNickName;
	gchar *sFirstName, *sLastName;

	debug_print( "addressbook_load_completion\n" );

	if( _addressIndex_ == NULL ) return FALSE;

	nodeIf = addrindex_get_interface_list( _addressIndex_ );
	while( nodeIf ) {
		AddressInterface *iface = nodeIf->data;
		nodeDS = iface->listSource;
		while( nodeDS ) {
			ds = nodeDS->data;

			/* Read address book */
			if( ! addrindex_ds_get_read_flag( ds ) ) {
				addrindex_ds_read_data( ds );
			}

			/* Get all persons */
			listP = addrindex_ds_get_all_persons( ds );
			nodeP = listP;
			while( nodeP ) {
				ItemPerson *person = nodeP->data;
				nodeM = person->listEMail;

				/* Figure out name to use */
				sName = ADDRITEM_NAME(person);
				sNickName = person->nickName;
				if( sName == NULL || *sName == '\0' ) {
					if (sNickName)
						sName = sNickName;
				}
				sFirstName = person->firstName;
				sLastName = person->lastName;

				/* Process each E-Mail address */
				while( nodeM ) {
					ItemEMail *email = nodeM->data;
					/* Have mail */
					sAddress = email->address;
					if( sAddress && *sAddress != '\0' ) {
						sAlias = ADDRITEM_NAME(email);
						if( sAlias && *sAlias != '\0' ) {
							func( sName, sFirstName, sLastName, sAlias, sAddress );
						} else {
							func( sName, sFirstName, sLastName, sNickName, sAddress );
						}
					}
					nodeM = g_list_next( nodeM );
				}
				nodeP = g_list_next( nodeP );
			}
			/* Free up the list */
			g_list_free( listP );

			nodeDS = g_list_next( nodeDS );
		}
		nodeIf = g_list_next( nodeIf );
	}
	debug_print( "addressbook_load_completion... done\n" );

	return TRUE;
}

static gint (*real_func)(const gchar *, const gchar *, const gchar *);

static gint wrapper_func(const gchar *name, const gchar *firstname, const gchar *lastname, const gchar *nickname, const gchar *address)
{
	return real_func(name, address, nickname);
}

gboolean addressbook_load_completion(gint (*callBackFunc)(const gchar *, const gchar *, const gchar *))
{
	gboolean ret;

	real_func = callBackFunc;
	ret = addressbook_load_completion_full(wrapper_func);
	real_func = NULL;

	return ret;
}

/* **********************************************************************
* Address Import.
* ***********************************************************************
*/

/*
* Import LDIF file.
*/
static void addressbook_import_ldif_cb(void)
{
	AddressDataSource *ds = NULL;
	AdapterDSource *ads = NULL;
	AddressBookFile *abf = NULL;
	AdapterInterface *adapter;
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeModel *model;
	GtkTreeIter iter, new_iter;
	GtkTreePath *path;

	adapter = addrbookctl_find_interface(ADDR_IF_BOOK);
	if (!adapter || !adapter->tree_row)
		return;

	abf = addressbook_imp_ldif(_addressIndex_);
	gtk_window_present(GTK_WINDOW(addrbook.window));
	if (!abf)
		return;

	ds = addrindex_index_add_datasource(_addressIndex_, ADDR_IF_BOOK, abf);
	ads = addressbook_create_ds_adapter(ds, ADDR_BOOK, NULL);
	addressbook_ads_set_name(ads, abf->name);

	model = gtk_tree_view_get_model(treeview);
	gtkut_tree_row_reference_get_iter(model, adapter->tree_row, &iter);

	if (addressbook_add_object(&iter, &new_iter, ADDRESS_OBJECT(ads))) {
		path = gtk_tree_model_get_path(model, &new_iter);
		gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
		gtk_tree_path_free(path);
	}

	/* Notify address completion */
	addressbook_modified();
}

gboolean addressbook_import_ldif_file(const gchar *file, const gchar *book_name)
{
	AddressBookFile *abf;

	g_return_val_if_fail(file != NULL, FALSE);
	g_return_val_if_fail(book_name != NULL, FALSE);

	abf = addressbook_imp_ldif_file(_addressIndex_, file, book_name);
	if (!abf)
		return FALSE;

	addrindex_index_add_datasource(_addressIndex_, ADDR_IF_BOOK, abf);
	addrindex_save_data(_addressIndex_);
	addressbook_modified();

	return TRUE;
}

/*
* Import CSV file.
*/
static void addressbook_import_csv_cb(void)
{
	AddressDataSource *ds = NULL;
	AdapterDSource *ads = NULL;
	AddressBookFile *abf = NULL;
	AdapterInterface *adapter;
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeModel *model;
	GtkTreeIter iter, new_iter;
	GtkTreePath *path;

	adapter = addrbookctl_find_interface(ADDR_IF_BOOK);
	if (!adapter || !adapter->tree_row)
		return;

	abf = addressbook_imp_csv(_addressIndex_);
	gtk_window_present(GTK_WINDOW(addrbook.window));
	if (!abf)
		return;

	ds = addrindex_index_add_datasource(_addressIndex_, ADDR_IF_BOOK, abf);
	ads = addressbook_create_ds_adapter(ds, ADDR_BOOK, NULL);
	addressbook_ads_set_name(ads, abf->name);

	model = gtk_tree_view_get_model(treeview);
	gtkut_tree_row_reference_get_iter(model, adapter->tree_row, &iter);

	if (addressbook_add_object(&iter, &new_iter, ADDRESS_OBJECT(ads))) {
		path = gtk_tree_model_get_path(model, &new_iter);
		gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
		gtk_tree_path_free(path);
	}

	/* Notify address completion */
	addressbook_modified();
}

/*
* Export CSV file.
*/
static void addressbook_export_csv_cb(void)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(addrbook.treeview);
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	AddressObject *obj;
	ItemFolder *itemFolder = NULL;
	ItemGroup *itemGroup = NULL;
	GList *items;
	GList *cur;
	gchar delim = ',';

	if (!addrbook.tree_opened) {
		return;
	}

	model = gtk_tree_view_get_model(treeview);
	gtkut_tree_row_reference_get_iter(model, addrbook.tree_opened, &iter);
	gtk_tree_model_get(model, &iter, COL_OBJ, &obj, -1);
	g_return_if_fail(obj != NULL);

	if (obj->type == ADDR_INTERFACE) {
		return;
	}

	addressbook_exp_csv(obj);
}

/* **********************************************************************
* Address Book Fast Search.
* ***********************************************************************
*/

static GHashTable *addr_table;

#if USE_THREADS
G_LOCK_DEFINE_STATIC(addr_table);
#define S_LOCK(name)	G_LOCK(name)
#define S_UNLOCK(name)	G_UNLOCK(name)
#else
#define S_LOCK(name)
#define S_UNLOCK(name)
#endif

static gint load_address(const gchar *name, const gchar *address,
			 const gchar *nickname)
{
	gchar *addr;

	if (!address)
		return -1;

	addr = g_ascii_strdown(address, -1);

	if (g_hash_table_lookup(addr_table, addr) == NULL)
		g_hash_table_insert(addr_table, addr, addr);
	else
		g_free(addr);

	return 0;
}

static void addressbook_modified(void)
{
	S_LOCK(addr_table);

	if (addr_table) {
		hash_free_strings(addr_table);
		g_hash_table_destroy(addr_table);
		addr_table = NULL;
	}

	S_UNLOCK(addr_table);

	invalidate_address_completion();
}

gboolean addressbook_has_address(const gchar *address)
{
	GSList *list, *cur;
	gchar *addr;
	gboolean found = FALSE;

	if (!address)
		return FALSE;

	/* debug_print("addressbook_has_address: check if addressbook has address: %s\n", address); */

	list = address_list_append(NULL, address);
	if (!list)
		return FALSE;

	S_LOCK(addr_table);

	if (!addr_table) {
		addr_table = g_hash_table_new(g_str_hash, g_str_equal);
		addressbook_load_completion(load_address);
	}

	for (cur = list; cur != NULL; cur = cur->next) {
		addr = g_ascii_strdown((gchar *)cur->data, -1);

		if (g_hash_table_lookup(addr_table, addr)) {
			found = TRUE;
			/* debug_print("<%s> is in addressbook\n", addr); */
		} else {
			found = FALSE;
			g_free(addr);
			break;
		}
		g_free(addr);
	}

	S_UNLOCK(addr_table);

	slist_free_strings(list);

	return found;
}

/*
* End of Source.
*/
