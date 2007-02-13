/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto
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
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkclist.h>
#include <gtk/gtktable.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkstock.h>
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
	COL_NAME	= 0,
	COL_ADDRESS	= 1,
	COL_REMARKS	= 2
} AddressBookColumnPos;

#define N_COLS	3
#define COL_NAME_WIDTH		164
#define COL_ADDRESS_WIDTH	156

#define COL_FOLDER_WIDTH	170

#define ADDRESSBOOK_MSGBUF_SIZE 2048

static GdkPixmap *folderxpm;
static GdkBitmap *folderxpmmask;
static GdkPixmap *folderopenxpm;
static GdkBitmap *folderopenxpmmask;
static GdkPixmap *groupxpm;
static GdkBitmap *groupxpmmask;
static GdkPixmap *interfacexpm;
static GdkBitmap *interfacexpmmask;
static GdkPixmap *bookxpm;
static GdkBitmap *bookxpmmask;
static GdkPixmap *addressxpm;
static GdkBitmap *addressxpmmask;
static GdkPixmap *vcardxpm;
static GdkBitmap *vcardxpmmask;
static GdkPixmap *jpilotxpm;
static GdkBitmap *jpilotxpmmask;
static GdkPixmap *categoryxpm;
static GdkBitmap *categoryxpmmask;
static GdkPixmap *ldapxpm;
static GdkBitmap *ldapxpmmask;

/* Message buffer */
static gchar addressbook_msgbuf[ ADDRESSBOOK_MSGBUF_SIZE ];

/* Address list selection */
static GList *_addressListSelection_ = NULL;

/* Address index file and interfaces */
static AddressIndex *_addressIndex_ = NULL;
static GList *_addressInterfaceList_ = NULL;
static GList *_addressIFaceSelection_ = NULL;
#define ADDRESSBOOK_IFACE_SELECTION "1/y,3/y,4/y,2/n"

/* Address clipboard */
static AddressObject *clipObj = NULL;

static AddressBook_win addrbook;

static GHashTable *_addressBookTypeHash_ = NULL;
static GList *_addressBookTypeList_ = NULL;

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

static void addressbook_tree_selected		(GtkCTree	*ctree,
						 GtkCTreeNode	*node,
						 gint		 column,
						 gpointer	 data);
static void addressbook_list_selected		(GtkCList	*clist,
						 gint		 row,
						 gint		 column,
						 GdkEvent	*event,
						 gpointer	 data);
static void addressbook_list_row_selected	(GtkCTree	*clist,
						 GtkCTreeNode	*node,
						 gint		 column,
						 gpointer	 data);
static void addressbook_list_row_unselected	(GtkCTree	*clist,
						 GtkCTreeNode	*node,
						 gint		 column,
						 gpointer	 data);
static void addressbook_person_expand_node	(GtkCTree	*ctree,
						 GList		*node,
						 gpointer	*data );
static void addressbook_person_collapse_node	(GtkCTree	*ctree,
						 GList		*node,
						 gpointer	*data );

#if 0
static void addressbook_entry_changed		(GtkWidget	*widget);
#endif

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

static void addressbook_change_node_name	(GtkCTreeNode	*node,
						 const gchar	*name);

static void addressbook_new_address_cb		(gpointer	 data,
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

static void addressbook_set_clist		(AddressObject	*obj);

static void addressbook_load_tree		(void);
void addressbook_read_file			(void);

static GtkCTreeNode *addressbook_add_object	(GtkCTreeNode	*node,
						 AddressObject	*obj);
static AddressDataSource *addressbook_find_datasource
						(GtkCTreeNode	*node );

static AddressBookFile *addressbook_get_book_file();

static GtkCTreeNode *addressbook_node_add_folder
						(GtkCTreeNode	*node,
						AddressDataSource *ds,
						ItemFolder	*itemFolder,
						AddressObjectType otype);
static GtkCTreeNode *addressbook_node_add_group (GtkCTreeNode	*node,
						AddressDataSource *ds,
						ItemGroup	*itemGroup);
/* static GtkCTreeNode *addressbook_node_add_category */
/*						(GtkCTreeNode	*node, */
/*						 AddressDataSource *ds, */
/*						 ItemFolder	*itemFolder); */
static void addressbook_tree_remove_children	(GtkCTree	*ctree,
						GtkCTreeNode	*parent);
static void addressbook_move_nodes_up		(GtkCTree	*ctree,
						GtkCTreeNode	*node);
static GtkCTreeNode *addressbook_find_group_node (GtkCTreeNode	*parent,
						ItemGroup	*group);

/* static void addressbook_delete_object	(AddressObject	*obj); */

static gboolean key_pressed			(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void size_allocated			(GtkWidget	*widget,
						 GtkAllocation	*allocation,
						 gpointer	 data);

static gint addressbook_list_compare_func	(GtkCList	*clist,
						 gconstpointer	 ptr1,
						 gconstpointer	 ptr2);
/* static gint addressbook_obj_name_compare	(gconstpointer	 a, */
/*						 gconstpointer	 b); */

/* static void addressbook_book_show_message	(AddressBookFile *book); */
/* static void addressbook_vcard_show_message	(VCardFile *vcf); */
#ifdef USE_JPILOT
/* static void addressbook_jpilot_show_message	(JPilotFile *jpf); */
#endif
#ifdef USE_LDAP
static void addressbook_ldap_show_message	(SyldapServer *server);
#endif

/* LUT's and IF stuff */
static void addressbook_free_adapter		(GtkCTreeNode	*node);
static void addressbook_free_child_adapters	(GtkCTreeNode	*node);
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
static void addressbook_list_select_add		(AddressObject *obj);
static void addressbook_list_select_remove	(AddressObject *obj);

static void addressbook_import_ldif_cb		(void);

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
	{N_("/_Address/_Edit"),		"<control>Return",	addressbook_edit_address_cb,    0, NULL},
	{N_("/_Address/_Delete"),	"Delete",	addressbook_delete_address_cb,  0, NULL},

	{N_("/_Tools"),			NULL,		NULL, 0, "<Branch>"},
	{N_("/_Tools/Import _LDIF file"), NULL,		addressbook_import_ldif_cb,	0, NULL},

	{N_("/_Help"),			NULL,		NULL, 0, "<Branch>"},
	{N_("/_Help/_About"),		NULL,		about_show, 0, NULL}
};

/* New options to be added. */
/*
	{N_("/_Edit"),			NULL,		NULL, 0, "<Branch>"},
	{N_("/_Edit/C_ut"),		"<control>X",	NULL,				0, NULL},
	{N_("/_Edit/_Copy"),		"<control>C",	NULL,                           0, NULL},
	{N_("/_Edit/_Paste"),		"<control>V",	NULL,                           0, NULL},
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
	{N_("/_Edit"),		NULL, addressbook_edit_address_cb,   0, NULL},
	{N_("/_Delete"),	NULL, addressbook_delete_address_cb, 0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Copy"),		NULL, addressbook_copy_address_cb,  0, NULL},
	{N_("/_Paste"),		NULL, addressbook_paste_address_cb, 0, NULL}
};

void addressbook_open(Compose *target)
{
	if (!addrbook.window) {
		addressbook_read_file();
		addressbook_create();
		addressbook_load_tree();
		gtk_ctree_select(GTK_CTREE(addrbook.ctree),
				 GTK_CTREE_NODE(GTK_CLIST(addrbook.ctree)->row_list));
		addressbook_menuitem_set_sensitive();
		gtk_widget_show_all(addrbook.window);
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

void addressbook_refresh(void)
{
	if (addrbook.window) {
		if (addrbook.treeSelected) {
			addrbook.treeSelected = NULL;
			gtk_ctree_select(GTK_CTREE(addrbook.ctree),
					 addrbook.treeSelected);
		}
	}
	addressbook_export_to_file();
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
	GtkWidget *ctree_swin;
	GtkWidget *ctree;
	GtkWidget *clist_vbox;
	GtkWidget *clist_swin;
	GtkWidget *clist;
	GtkWidget *paned;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *statusbar;
	GtkWidget *hbbox;
	GtkWidget *hsbox;
	GtkWidget *del_btn;
	GtkWidget *reg_btn;
	GtkWidget *lup_btn;
	GtkWidget *to_btn;
	GtkWidget *cc_btn;
	GtkWidget *bcc_btn;
	GtkWidget *tree_popup;
	GtkWidget *list_popup;
	GtkItemFactory *tree_factory;
	GtkItemFactory *list_factory;
	GtkItemFactory *menu_factory;
	gint n_entries;
	GList *nodeIf;

	gchar *titles[N_COLS];
	gchar *text;
	gint i;

	debug_print("Creating addressbook window...\n");

	titles[COL_NAME]    = _("Name");
	titles[COL_ADDRESS] = _("E-Mail address");
	titles[COL_REMARKS] = _("Remarks");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Address book"));
	gtk_window_set_wmclass(GTK_WINDOW(window), "addressbook", "Sylpheed");
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, TRUE);
	gtk_widget_set_size_request(window, prefs_common.addressbook_width,
				    prefs_common.addressbook_height);
	gtk_window_move(GTK_WINDOW(window), prefs_common.addressbook_x,
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

	ctree_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ctree_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_widget_set_size_request(ctree_swin, COL_FOLDER_WIDTH + 40, -1);

	/* Address index */
	ctree = gtk_ctree_new(1, 0);
	gtk_container_add(GTK_CONTAINER(ctree_swin), ctree);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_clist_set_column_width(GTK_CLIST(ctree), 0, COL_FOLDER_WIDTH);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
	gtk_ctree_set_indent(GTK_CTREE(ctree), CTREE_INDENT);
	gtk_clist_set_compare_func(GTK_CLIST(ctree),
				   addressbook_list_compare_func);

	g_signal_connect(G_OBJECT(ctree), "tree_select_row",
			 G_CALLBACK(addressbook_tree_selected), NULL);
	g_signal_connect(G_OBJECT(ctree), "button_press_event",
			 G_CALLBACK(addressbook_tree_button_pressed),
			 NULL);
	g_signal_connect(G_OBJECT(ctree), "button_release_event",
			 G_CALLBACK(addressbook_tree_button_released),
			 NULL);

	clist_vbox = gtk_vbox_new(FALSE, 4);

	clist_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(clist_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(clist_vbox), clist_swin, TRUE, TRUE, 0);

	/* Address list */
	clist = gtk_ctree_new_with_titles(N_COLS, 0, titles);
	gtk_container_add(GTK_CONTAINER(clist_swin), clist);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_EXTENDED);
	gtk_ctree_set_line_style(GTK_CTREE(clist), GTK_CTREE_LINES_NONE);
	gtk_ctree_set_expander_style(GTK_CTREE(clist), GTK_CTREE_EXPANDER_SQUARE);
	gtk_ctree_set_indent(GTK_CTREE(clist), CTREE_INDENT);
	gtk_clist_set_column_width(GTK_CLIST(clist), COL_NAME,
				   COL_NAME_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(clist), COL_ADDRESS,
				   COL_ADDRESS_WIDTH);
	gtk_clist_set_compare_func(GTK_CLIST(clist),
				   addressbook_list_compare_func);

	for (i = 0; i < N_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[i].button,
				       GTK_CAN_FOCUS);

	g_signal_connect(G_OBJECT(clist), "tree_select_row",
			 G_CALLBACK(addressbook_list_row_selected), NULL);
	g_signal_connect(G_OBJECT(clist), "tree_unselect_row",
			 G_CALLBACK(addressbook_list_row_unselected), NULL);
	g_signal_connect(G_OBJECT(clist), "button_press_event",
			 G_CALLBACK(addressbook_list_button_pressed),
			 NULL);
	g_signal_connect(G_OBJECT(clist), "button_release_event",
			 G_CALLBACK(addressbook_list_button_released),
			 NULL);
	g_signal_connect(G_OBJECT(clist), "select_row",
			 G_CALLBACK(addressbook_list_selected), NULL);
	g_signal_connect(G_OBJECT(clist), "tree_expand",
			 G_CALLBACK(addressbook_person_expand_node), NULL);
	g_signal_connect(G_OBJECT(clist), "tree_collapse",
			 G_CALLBACK(addressbook_person_collapse_node), NULL);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(clist_vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Name:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

	address_completion_register_entry(GTK_ENTRY(entry));

#if 0
	g_signal_connect(G_OBJECT(entry), "changed",
			 G_CALLBACK(addressbook_entry_changed), NULL);
#endif

	paned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox2), paned, TRUE, TRUE, 0);
	gtk_paned_add1(GTK_PANED(paned), ctree_swin);
	gtk_paned_add2(GTK_PANED(paned), clist_vbox);

	/* Status bar */
	hsbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, 0);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, 0);

	/* Button panel */
	hbbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbbox), 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbbox), 4);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

	del_btn = gtk_button_new_with_label(_("Delete"));
	GTK_WIDGET_SET_FLAGS(del_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), del_btn, TRUE, TRUE, 0);
	reg_btn = gtk_button_new_with_label(_("Add"));
	GTK_WIDGET_SET_FLAGS(reg_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), reg_btn, TRUE, TRUE, 0);
	lup_btn = gtk_button_new_with_label(_("Lookup"));
	GTK_WIDGET_SET_FLAGS(lup_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), lup_btn, TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(addressbook_del_clicked), NULL);
	g_signal_connect(G_OBJECT(reg_btn), "clicked",
			 G_CALLBACK(addressbook_reg_clicked), NULL);
	g_signal_connect(G_OBJECT(lup_btn), "clicked",
			 G_CALLBACK(addressbook_lup_clicked), NULL);

	to_btn = gtk_button_new_with_label
		(prefs_common.trans_hdr ? _("To:") : "To:");
	GTK_WIDGET_SET_FLAGS(to_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), to_btn, TRUE, TRUE, 0);
	cc_btn = gtk_button_new_with_label
		(prefs_common.trans_hdr ? _("Cc:") : "Cc:");
	GTK_WIDGET_SET_FLAGS(cc_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), cc_btn, TRUE, TRUE, 0);
	bcc_btn = gtk_button_new_with_label
		(prefs_common.trans_hdr ? _("Bcc:") : "Bcc:");
	GTK_WIDGET_SET_FLAGS(bcc_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), bcc_btn, TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(to_btn), "clicked",
			 G_CALLBACK(addressbook_to_clicked),
			 GINT_TO_POINTER(COMPOSE_ENTRY_TO));
	g_signal_connect(G_OBJECT(cc_btn), "clicked",
			 G_CALLBACK(addressbook_to_clicked),
			 GINT_TO_POINTER(COMPOSE_ENTRY_CC));
	g_signal_connect(G_OBJECT(bcc_btn), "clicked",
			 G_CALLBACK(addressbook_to_clicked),
			 GINT_TO_POINTER(COMPOSE_ENTRY_BCC));

	/* Build icons for interface */
	stock_pixmap_gdk( window, STOCK_PIXMAP_INTERFACE,
			  &interfacexpm, &interfacexpmmask );

	/* Build control tables */
	addrbookctl_build_map(window);
	addrbookctl_build_iflist();
	addrbookctl_build_ifselect();

	/* Add each interface into the tree as a root level folder */
	nodeIf = _addressInterfaceList_;
	while( nodeIf ) {
		AdapterInterface *adapter = nodeIf->data;
		AddressInterface *iface = adapter->interface;
		nodeIf = g_list_next(nodeIf);

		if(iface->useInterface) {
			AddressTypeControlItem *atci = adapter->atci;
			text = atci->displayName;
			adapter->treeNode =
				gtk_ctree_insert_node( GTK_CTREE(ctree),
					NULL, NULL, &text, FOLDER_SPACING,
					interfacexpm, interfacexpmmask,
					interfacexpm, interfacexpmmask,
					FALSE, FALSE );
			menu_set_sensitive( menu_factory, atci->menuCommand, adapter->haveLibrary );
			gtk_ctree_node_set_row_data( GTK_CTREE(ctree), adapter->treeNode, adapter );
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

	addrbook.window  = window;
	addrbook.menubar = menubar;
	addrbook.ctree   = ctree;
	addrbook.clist   = clist;
	addrbook.entry   = entry;
	addrbook.statusbar = statusbar;
	addrbook.status_cid = gtk_statusbar_get_context_id( GTK_STATUSBAR(statusbar), "Addressbook Window" );

	addrbook.del_btn = del_btn;
	addrbook.reg_btn = reg_btn;
	addrbook.lup_btn = lup_btn;
	addrbook.to_btn  = to_btn;
	addrbook.cc_btn  = cc_btn;
	addrbook.bcc_btn = bcc_btn;

	addrbook.tree_popup   = tree_popup;
	addrbook.list_popup   = list_popup;
	addrbook.tree_factory = tree_factory;
	addrbook.list_factory = list_factory;
	addrbook.menu_factory = menu_factory;

	addrbook.listSelected = NULL;
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

static void addressbook_status_show( gchar *msg ) {
	if( addrbook.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(addrbook.statusbar), addrbook.status_cid );
		if( msg ) {
			gtk_statusbar_push( GTK_STATUSBAR(addrbook.statusbar), addrbook.status_cid, msg );
		}
	}
}

static void addressbook_ds_show_message( AddressDataSource *ds ) {
	gint retVal;
	gchar *name;
	*addressbook_msgbuf = '\0';
	if( ds ) {
		name = addrindex_ds_get_name( ds );
		retVal = addrindex_ds_get_status_code( ds );
		if( retVal == MGU_SUCCESS ) {
			if( ds ) {
				sprintf( addressbook_msgbuf, "%s", name );
			}
		}
		else {
			if( ds == NULL ) {
				sprintf( addressbook_msgbuf, "%s", mgu_error2string( retVal ) );
			}
			else {
				sprintf( addressbook_msgbuf, "%s: %s", name, mgu_error2string( retVal ) );
			}
		}
	}
	addressbook_status_show( addressbook_msgbuf );
}

/*
* Delete one or more objects from address list.
*/
static void addressbook_del_clicked(GtkButton *button, gpointer data)
{
	GtkCTree *clist = GTK_CTREE(addrbook.clist);
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *pobj, *obj;
	AdapterDSource *ads = NULL;
	GtkCTreeNode *nodeList;
	gboolean procFlag;
	AlertValue aval;
	AddressBookFile *abf = NULL;
	AddressDataSource *ds = NULL;

	pobj = gtk_ctree_node_get_row_data(ctree, addrbook.opened );
	g_return_if_fail(pobj != NULL);

	nodeList = addrbook.listSelected;
	obj = gtk_ctree_node_get_row_data( clist, nodeList );
	if( obj == NULL) return;
	ds = addressbook_find_datasource( addrbook.treeSelected );
	if( ds == NULL ) return;

	procFlag = FALSE;
	if( pobj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(pobj);
		if( ads->subType == ADDR_BOOK ) procFlag = TRUE;
	}
	else if( pobj->type == ADDR_ITEM_FOLDER ) {
		procFlag = TRUE;
	}
	else if( pobj->type == ADDR_ITEM_GROUP ) {
		procFlag = TRUE;
	}
	if( ! procFlag ) return;
	abf = ds->rawDataSource;
	if( abf == NULL ) return;

	/* Confirm deletion */
	aval = alertpanel( _("Delete address(es)"),
			_("Really delete the address(es)?"),
			GTK_STOCK_YES, GTK_STOCK_NO, NULL );
	if( aval != G_ALERTDEFAULT ) return;

	/* Process deletions */
	if( pobj->type == ADDR_DATASOURCE || pobj->type == ADDR_ITEM_FOLDER ) {
		/* Items inside folders */
		GList *node;
		node = _addressListSelection_;
		while( node ) {
			AddrItemObject *aio = node->data;
			node = g_list_next( node );
			if( aio->type == ADDR_ITEM_GROUP ) {
				ItemGroup *item = ( ItemGroup * ) aio;
				GtkCTreeNode *nd = NULL;

				nd = addressbook_find_group_node( addrbook.opened, item );
				item = addrbook_remove_group( abf, item );
				if( item ) {
					addritem_free_item_group( item );
					item = NULL;
				}
				/* Remove group from parent node */
				gtk_ctree_remove_node( ctree, nd );
			}
			else if( aio->type == ADDR_ITEM_PERSON ) {
				ItemPerson *item = ( ItemPerson * ) aio;
				if( item == ( ItemPerson * )clipObj )
					clipObj = NULL;
				item = addrbook_remove_person( abf, item );
				if( item ) {
					addritem_free_item_person( item );
					item = NULL;
				}
			}
			else if( aio->type == ADDR_ITEM_EMAIL ) {
				ItemEMail *item = ( ItemEMail * ) aio;
				ItemPerson *person = ( ItemPerson * ) ADDRITEM_PARENT(item);
				item = addrbook_person_remove_email( abf, person, item );
				if( item ) {
					addritem_free_item_email( item );
					item = NULL;
				}
			}
		}
		addressbook_list_select_clear();
		addrbook.treeSelected = NULL;
		gtk_ctree_select( ctree, addrbook.opened );
		return;
	}
	else if( pobj->type == ADDR_ITEM_GROUP ) {
		/* Items inside groups */
		GList *node;
		ItemGroup *group = ADAPTER_GROUP(pobj)->itemGroup;
		node = _addressListSelection_;
		while( node ) {
			AddrItemObject *aio = node->data;
			node = g_list_next( node );
			if( aio->type == ADDR_ITEM_EMAIL ) {
				ItemEMail *item = ( ItemEMail * ) aio;
				item = addrbook_group_remove_email( abf, group, item );
			}
		}
		addressbook_list_select_clear();
		addrbook.treeSelected = NULL;
		gtk_ctree_select( ctree, addrbook.opened );
		return;
	}

	gtk_ctree_node_set_row_data( clist, nodeList, NULL );
	gtk_ctree_remove_node( clist, nodeList );
	addressbook_list_select_remove( obj );

}

static void addressbook_reg_clicked(GtkButton *button, gpointer data)
{
	addressbook_new_address_cb( NULL, 0, NULL );
}

gchar *addressbook_format_address( AddressObject * obj ) {
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
			else if( ADDRITEM_NAME(person) ) {
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
		if( name && name[0] != '\0' ) {
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

	if (!addrbook.target_compose) {
		addrbook.target_compose = compose_new(NULL, NULL, NULL, NULL);
		if (!addrbook.target_compose)
			return;
	}

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
}

static void addressbook_menubar_set_sensitive( gboolean sensitive ) {
	menu_set_sensitive( addrbook.menu_factory, "/File/New Book",    sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/File/New vCard",  sensitive );
#ifdef USE_JPILOT
	menu_set_sensitive( addrbook.menu_factory, "/File/New JPilot", sensitive );
#endif
#ifdef USE_LDAP
	menu_set_sensitive( addrbook.menu_factory, "/File/New LDAP Server",  sensitive );
#endif
	menu_set_sensitive( addrbook.menu_factory, "/File/Edit",        sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/File/Delete",      sensitive );

	menu_set_sensitive( addrbook.menu_factory, "/Address/New Address", sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/Address/New Group",   sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/Address/New Folder",  sensitive );
	gtk_widget_set_sensitive( addrbook.reg_btn, sensitive );
	gtk_widget_set_sensitive( addrbook.del_btn, sensitive );
}

static void addressbook_menuitem_set_sensitive(void) {
	gboolean canAdd = FALSE;
	gboolean canEditTree = TRUE;
	gboolean canEditAddress = FALSE;
	gboolean canDelete = FALSE;
	gboolean canCopy = FALSE;
	gboolean canPaste = FALSE;
	AddressTypeControlItem *atci = NULL;
	AddressDataSource *ds = NULL;
	AddressInterface *iface = NULL;
	AddressObject *pobj = NULL;
	AddressObject *obj = NULL;

	addressbook_menubar_set_sensitive( FALSE );
	menu_set_insensitive_all(GTK_MENU_SHELL(addrbook.tree_popup));
	menu_set_insensitive_all( GTK_MENU_SHELL(addrbook.list_popup) );

	pobj = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.ctree), addrbook.treeSelected );
	if( pobj == NULL ) return;

	if( addrbook.listSelected )
		obj = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.clist), addrbook.listSelected );

	if( pobj->type == ADDR_INTERFACE ) {
		AdapterInterface *adapter = ADAPTER_INTERFACE(pobj);
		iface = adapter->interface;
		if( iface ) {
			if( iface->haveLibrary ) {
				/* Enable appropriate File / New command */
				atci = adapter->atci;
				menu_set_sensitive( addrbook.menu_factory, atci->menuCommand, TRUE );
			}
		}
		canEditTree = FALSE;
	}
	else if( pobj->type == ADDR_DATASOURCE ) {
		AdapterDSource *ads = ADAPTER_DSOURCE(pobj);
		ds = ads->dataSource;
		iface = ds->interface;
		if( ! iface->readOnly ) {
			canAdd = canEditAddress = TRUE;
		}
		if( ! iface->haveLibrary ) {
			canAdd = canEditAddress = FALSE;
		}
	}
	else if( pobj->type == ADDR_ITEM_FOLDER ) {
		ds = addressbook_find_datasource( addrbook.treeSelected );
		if( ds ) {
			iface = ds->interface;
			if( ! iface->readOnly ) {
				canAdd = canEditAddress = TRUE;
			}
		}
	}
	else if( pobj->type == ADDR_ITEM_GROUP ) {
		ds = addressbook_find_datasource( addrbook.treeSelected );
		if( ds ) {
			iface = ds->interface;
			if( ! iface->readOnly ) {
				canEditAddress = TRUE;
			}
		}
	}

	if( obj == NULL ) canEditAddress = FALSE;
	canDelete = canEditAddress;
	if( GTK_CLIST(addrbook.clist)->selection &&
	    GTK_CLIST(addrbook.clist)->selection->next ) canEditAddress = FALSE;
	if( obj && obj->type == ADDR_ITEM_PERSON ) canCopy = TRUE;
	if( clipObj && canAdd ) canPaste = TRUE;

	/* Enable add */
	menu_set_sensitive( addrbook.menu_factory, "/Address/New Address", canAdd );
	menu_set_sensitive( addrbook.menu_factory, "/Address/New Folder",  canAdd );
	menu_set_sensitive( addrbook.menu_factory, "/Address/New Group",   canAdd );

	/* Enable edit */
	menu_set_sensitive( addrbook.menu_factory, "/Address/Edit",   canEditAddress );
	menu_set_sensitive( addrbook.menu_factory, "/Address/Delete", canDelete );

	menu_set_sensitive( addrbook.menu_factory, "/File/Edit",      canEditTree );
	menu_set_sensitive( addrbook.menu_factory, "/File/Delete",    canEditTree );

	menu_set_sensitive( addrbook.menu_factory, "/Edit/Copy",   canCopy );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Paste",  canPaste );

	/* Popup menu */
	menu_set_sensitive( addrbook.tree_factory, "/New Address", canAdd );
	menu_set_sensitive( addrbook.tree_factory, "/New Folder", canAdd );
	menu_set_sensitive( addrbook.tree_factory, "/New Group", canAdd );
	menu_set_sensitive( addrbook.tree_factory, "/Edit", canEditTree );
	menu_set_sensitive( addrbook.tree_factory, "/Delete", canEditTree );

	menu_set_sensitive( addrbook.list_factory, "/New Address", canAdd );
	menu_set_sensitive( addrbook.list_factory, "/New Folder", canAdd );
	menu_set_sensitive( addrbook.list_factory, "/New Group", canAdd );
	menu_set_sensitive( addrbook.list_factory, "/Edit", canEditAddress );
	menu_set_sensitive( addrbook.list_factory, "/Delete", canDelete );
	menu_set_sensitive( addrbook.list_factory, "/Copy", canCopy );
	menu_set_sensitive( addrbook.list_factory, "/Paste", canPaste );

	/* Buttons */
	gtk_widget_set_sensitive( addrbook.reg_btn, canAdd );
	gtk_widget_set_sensitive( addrbook.del_btn, canDelete );
}

static void addressbook_tree_selected(GtkCTree *ctree, GtkCTreeNode *node,
				      gint column, gpointer data)
{
	AddressObject *obj = NULL;
	AdapterDSource *ads = NULL;
	AddressDataSource *ds = NULL;
	ItemFolder *rootFolder = NULL;

	if( addrbook.treeSelected == node ) return;
	addrbook.treeSelected = node;
	addrbook.listSelected = NULL;
	addressbook_status_show( "" );
	if( addrbook.entry != NULL ) gtk_entry_set_text(GTK_ENTRY(addrbook.entry), "");

	if( addrbook.clist ) gtk_clist_clear( GTK_CLIST(addrbook.clist) );
	if( node ) obj = gtk_ctree_node_get_row_data( ctree, node );
	if( obj == NULL ) return;

	addrbook.opened = node;

	if( obj->type == ADDR_DATASOURCE ) {
		/* Read from file */
		static gboolean tVal = TRUE;

		ads = ADAPTER_DSOURCE(obj);
		if( ads == NULL ) return;
		ds = ads->dataSource;
		if( ds == NULL ) return;		

		if( addrindex_ds_get_modify_flag( ds ) ) {
			addrindex_ds_read_data( ds );
		}

		if( ! addrindex_ds_get_read_flag( ds ) ) {
			addrindex_ds_read_data( ds );
		}
		addressbook_ds_show_message( ds );

		if( ! addrindex_ds_get_access_flag( ds ) ) {
			/* Remove existing folders and groups */
			gtk_clist_freeze( GTK_CLIST(ctree) );
			addressbook_tree_remove_children( ctree, node );
			gtk_clist_thaw( GTK_CLIST(ctree) );

			/* Load folders into the tree */
			rootFolder = addrindex_ds_get_root_folder( ds );
			if( ds->type == ADDR_IF_JPILOT ) {
				addressbook_node_add_folder( node, ds, rootFolder, ADDR_CATEGORY );
			}
			else {
				addressbook_node_add_folder( node, ds, rootFolder, ADDR_ITEM_FOLDER );
			}
			addrindex_ds_set_access_flag( ds, &tVal );
			gtk_ctree_expand( ctree, node );
		}
	}

	/* Update address list */
	addressbook_set_clist( obj );

	addressbook_menuitem_set_sensitive();

	addressbook_list_select_clear();

}

static void addressbook_list_selected(GtkCList *clist, gint row, gint column,
				      GdkEvent *event, gpointer data)
{
	if (event && event->type == GDK_2BUTTON_PRESS) {
		/* Handle double click */
		if (prefs_common.add_address_by_click &&
		    addrbook.target_compose)
			addressbook_to_clicked(NULL, GINT_TO_POINTER(COMPOSE_ENTRY_TO));
		else
			addressbook_edit_address_cb(NULL, 0, NULL);
	}
}

#if 0
static void addressbook_list_select_show() {
	GList *node = _addressListSelection_;
	gchar *addr = NULL;
	printf( "show selection...>>>\n" );
	while( node != NULL ) {
		AddressObject *obj = ( AddressObject * ) node->data;
		if( obj ) {
			printf( "- %d : '%s'\n", obj->type, obj->name );
			if( obj->type == ADDR_ITEM_GROUP ) {
				ItemGroup *group = ( ItemGroup * ) obj;
				GList *node = group->listEMail;
				while( node ) {
					ItemEMail *email = node->data;
					addr = addressbook_format_address( ( AddressObject * ) email );
					if( addr ) {
						printf( "\tgrp >%s<\n", addr );
						g_free( addr );
					}
					node = g_list_next( node );
				}
			}
			else {
				addr = addressbook_format_address( obj );
				if( addr ) {
					printf( "\t>%s<\n", addr );
					g_free( addr );
				}
			}
		}
		else {
			printf( "- NULL" );
		}
		node = g_list_next( node );
	}
	printf( "show selection...<<<\n" );
}
#endif

static void addressbook_list_select_clear() {
	if( _addressListSelection_ ) {
		g_list_free( _addressListSelection_ );
	}
	_addressListSelection_ = NULL;
}

static void addressbook_list_select_add( AddressObject *obj ) {
	if( obj ) {
		if(     obj->type == ADDR_ITEM_PERSON ||
			obj->type == ADDR_ITEM_EMAIL ||
			obj->type == ADDR_ITEM_GROUP ) {
			if( ! g_list_find( _addressListSelection_, obj ) ) {
				_addressListSelection_ = g_list_append( _addressListSelection_, obj );
			}
		}
	}
	/* addressbook_list_select_show(); */
}

static void addressbook_list_select_remove( AddressObject *obj ) {
	if( obj == NULL ) return;
	if( _addressListSelection_ ) {
		_addressListSelection_ = g_list_remove(	_addressListSelection_, obj );
	}
	/* addressbook_list_select_show(); */
}

static void addressbook_list_row_selected( GtkCTree *clist, GtkCTreeNode *node, gint column, gpointer data ) {
	GtkEntry *entry = GTK_ENTRY(addrbook.entry);
	AddressObject *obj = NULL;

	gtk_entry_set_text( entry, "" );
	addrbook.listSelected = node;
	obj = gtk_ctree_node_get_row_data( clist, node );
	if( obj != NULL ) {
		g_print( "list select: %d : '%s'\n", obj->type, obj->name );
		addressbook_list_select_add( obj );
	}

	addressbook_menuitem_set_sensitive();
}

static void addressbook_list_row_unselected( GtkCTree *ctree, GtkCTreeNode *node, gint column, gpointer data ) {
	AddressObject *obj;

	obj = gtk_ctree_node_get_row_data( ctree, node );
	if( obj != NULL ) {
		g_print( "list unselect: %d : '%s'\n", obj->type, obj->name );
		addressbook_list_select_remove( obj );
	}

	addressbook_menuitem_set_sensitive();
}

static gboolean addressbook_list_button_pressed(GtkWidget *widget,
						GdkEventButton *event,
						gpointer data)
{
	if( ! event ) return FALSE;

	if( event->button == 3 ) {
		gtk_menu_popup( GTK_MENU(addrbook.list_popup), NULL, NULL, NULL, NULL,
		       event->button, event->time );
	}
	return FALSE;
}

static gboolean addressbook_list_button_released(GtkWidget *widget,
						 GdkEventButton *event,
						 gpointer data)
{
	return FALSE;
}

static gboolean addressbook_tree_button_pressed(GtkWidget *ctree,
						GdkEventButton *event,
						gpointer data)
{
	GtkCList *clist = GTK_CLIST(ctree);
	gint row, column;
	AddressObject *obj = NULL;
	/* GtkCTreeNode *node; */
	AdapterDSource *ads = NULL;
	AddressInterface *iface = NULL;
	AddressDataSource *ds = NULL;

	if( ! event ) return FALSE;
/* */
	if( gtk_clist_get_selection_info( clist, event->x, event->y, &row, &column ) ) {
		gtk_clist_select_row( clist, row, column );
		gtkut_clist_set_focus_row(clist, row);
	}
/* */

	addressbook_menuitem_set_sensitive();

	if( event->button == 3 ) {
		gtk_menu_popup(GTK_MENU(addrbook.tree_popup), NULL, NULL, NULL, NULL,
			       event->button, event->time);
	}

	return FALSE;
}

static gboolean addressbook_tree_button_released(GtkWidget *ctree,
						 GdkEventButton *event,
						 gpointer data)
{
#if 0
	gtk_ctree_select(GTK_CTREE(addrbook.ctree), addrbook.opened);
	gtkut_ctree_set_focus_row(GTK_CTREE(addrbook.ctree), addrbook.opened);
#endif
	return FALSE;
}

static void addressbook_popup_close(GtkMenuShell *menu_shell, gpointer data)
{
	if (!addrbook.opened) return;

	gtk_ctree_select(GTK_CTREE(addrbook.ctree), addrbook.opened);
	gtkut_ctree_set_focus_row(GTK_CTREE(addrbook.ctree),
				  addrbook.opened);
}

static void addressbook_new_folder_cb(gpointer data, guint action,
				      GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *parentFolder = NULL;
	ItemFolder *folder = NULL;

	if( ! addrbook.treeSelected ) return;
	obj = gtk_ctree_node_get_row_data( ctree, addrbook.treeSelected );
	if( obj == NULL ) return;
	ds = addressbook_find_datasource( addrbook.treeSelected );
	if( ds == NULL ) return;

	if( obj->type == ADDR_DATASOURCE ) {
		if( ADAPTER_DSOURCE(obj)->subType != ADDR_BOOK ) return;
	}
	else if( obj->type == ADDR_ITEM_FOLDER ) {
		parentFolder = ADAPTER_FOLDER(obj)->itemFolder;
	}
	else {
		return;
	}

	abf = ds->rawDataSource;
	if( abf == NULL ) return;
	folder = addressbook_edit_folder( abf, parentFolder, NULL );
	if( folder ) {
		GtkCTreeNode *nn;
		nn = addressbook_node_add_folder( addrbook.treeSelected, ds, folder, ADDR_ITEM_FOLDER );
		gtk_ctree_expand( ctree, addrbook.treeSelected );
		if( addrbook.treeSelected == addrbook.opened ) addressbook_set_clist(obj);
	}

}

static void addressbook_new_group_cb(gpointer data, guint action,
				     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *parentFolder = NULL;
	ItemGroup *group = NULL;

	if( ! addrbook.treeSelected ) return;
	obj = gtk_ctree_node_get_row_data(ctree, addrbook.treeSelected);
	if( obj == NULL ) return;
	ds = addressbook_find_datasource( addrbook.treeSelected );
	if( ds == NULL ) return;

	if( obj->type == ADDR_DATASOURCE ) {
		if( ADAPTER_DSOURCE(obj)->subType != ADDR_BOOK ) return;
	}
	else if( obj->type == ADDR_ITEM_FOLDER ) {
		parentFolder = ADAPTER_FOLDER(obj)->itemFolder;
	}
	else {
		return;
	}

	abf = ds->rawDataSource;
	if( abf == NULL ) return;
	group = addressbook_edit_group( abf, parentFolder, NULL );
	if( group ) {
		GtkCTreeNode *nn;
		nn = addressbook_node_add_group( addrbook.treeSelected, ds, group );
		gtk_ctree_expand( ctree, addrbook.treeSelected );
		if( addrbook.treeSelected == addrbook.opened ) addressbook_set_clist(obj);
	}

}

static void addressbook_change_node_name(GtkCTreeNode *node, const gchar *name)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	gchar *text[1];
	guint8 spacing;
	GdkPixmap *pix_cl, *pix_op;
	GdkBitmap *mask_cl, *mask_op;
	gboolean is_leaf, expanded;

	gtk_ctree_get_node_info(ctree, node, text, &spacing,
				&pix_cl, &mask_cl, &pix_op, &mask_op,
				&is_leaf, &expanded);
	gtk_ctree_set_node_info(ctree, node, name, spacing,
				pix_cl, mask_cl, pix_op, mask_op,
				is_leaf, expanded);
}

/*
* Edit data source.
* Enter: obj   Address object to edit.
*        node  Node in tree.
* Return: New name of data source.
*/
static gchar *addressbook_edit_datasource( AddressObject *obj, GtkCTreeNode *node ) {
	gchar *newName = NULL;
	AddressDataSource *ds = NULL;
	AddressInterface *iface = NULL;
	AdapterDSource *ads = NULL;

	ds = addressbook_find_datasource( node );
	if( ds == NULL ) return NULL;
	iface = ds->interface;
	if( ! iface->haveLibrary ) return NULL;

	/* Read data from data source */
	if( ! addrindex_ds_get_read_flag( ds ) ) {
		addrindex_ds_read_data( ds );
	}

	/* Handle edit */
	ads = ADAPTER_DSOURCE(obj);
	if( ads->subType == ADDR_BOOK ) {
                if( addressbook_edit_book( _addressIndex_, ads ) == NULL ) return NULL;
	}
	else if( ads->subType == ADDR_VCARD ) {
       	        if( addressbook_edit_vcard( _addressIndex_, ads ) == NULL ) return NULL;
	}
#ifdef USE_JPILOT
	else if( ads->subType == ADDR_JPILOT ) {
                if( addressbook_edit_jpilot( _addressIndex_, ads ) == NULL ) return NULL;
	}
#endif
#ifdef USE_LDAP
	else if( ads->subType == ADDR_LDAP ) {
		if( addressbook_edit_ldap( _addressIndex_, ads ) == NULL ) return NULL;
	}
#endif
	else {
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
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	GtkCTreeNode *node = NULL, *parentNode = NULL;
	gchar *name = NULL;

	if( ! addrbook.treeSelected ) return;
	node = addrbook.treeSelected;
	if( GTK_CTREE_ROW(node)->level == 1 ) return;
	obj = gtk_ctree_node_get_row_data( ctree, node );
	if( obj == NULL ) return;
	parentNode = GTK_CTREE_ROW(node)->parent;

	ds = addressbook_find_datasource( node );
	if( ds == NULL ) return;

	if( obj->type == ADDR_DATASOURCE ) {
		name = addressbook_edit_datasource( obj, node );
		if( name == NULL ) return;
	}
	else {
		abf = ds->rawDataSource;
		if( abf == NULL ) return;
		if( obj->type == ADDR_ITEM_FOLDER ) {
			AdapterFolder *adapter = ADAPTER_FOLDER(obj);
			ItemFolder *item = adapter->itemFolder;
			ItemFolder *parentFolder = NULL;
			parentFolder = ( ItemFolder * ) ADDRITEM_PARENT(item);
			if( addressbook_edit_folder( abf, parentFolder, item ) == NULL ) return;
			name = ADDRITEM_NAME(item);
		}
		else if( obj->type == ADDR_ITEM_GROUP ) {
			AdapterGroup *adapter = ADAPTER_GROUP(obj);
			ItemGroup *item = adapter->itemGroup;
			ItemFolder *parentFolder = NULL;
			parentFolder = ( ItemFolder * ) ADDRITEM_PARENT(item);
			if( addressbook_edit_group( abf, parentFolder, item ) == NULL ) return;
			name = ADDRITEM_NAME(item);
		}
	}
	if( name && parentNode ) {
		/* Update node in tree view */
		addressbook_change_node_name( node, name );
		gtk_ctree_sort_node(ctree, parentNode);
		gtk_ctree_expand( ctree, node );
		addrbook.treeSelected = NULL;
		gtk_ctree_select( ctree, node );
	}
}

/*
* Delete an item from the tree widget.
*/
static void addressbook_treenode_delete_cb(gpointer data, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkCTreeNode *node = NULL;
	AddressObject *obj;
	gchar *message;
	AlertValue aval;
	AddressBookFile *abf = NULL;
	AdapterDSource *ads = NULL;
	AddressInterface *iface = NULL;
	AddressDataSource *ds = NULL;
	gboolean remFlag = FALSE;

	if( ! addrbook.treeSelected ) return;
	node = addrbook.treeSelected;
	if( GTK_CTREE_ROW(node)->level == 1 ) return;

	obj = gtk_ctree_node_get_row_data( ctree, node );
	g_return_if_fail(obj != NULL);

	if( obj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(obj);
		if( ads == NULL ) return;
		ds = ads->dataSource;
		if( ds == NULL ) return;
	}
	else {
		/* Must be folder or something else */
		ds = addressbook_find_datasource( node );
		if( ds == NULL ) return;

		/* Only allow deletion from non-readOnly data sources */
		iface = ds->interface;
		if( iface->readOnly ) return;
	}

	/* Confirm deletion */
	if( obj->type == ADDR_ITEM_FOLDER ) {
		message = g_strdup_printf
			( _( "Do you want to delete the folder AND all addresses in `%s' ?\n"
			     "If deleting the folder only, addresses will be moved into parent folder." ),
			 obj->name );
		aval = alertpanel( _("Delete folder"), message, _("_Folder only"), _("Folder and _addresses"), GTK_STOCK_CANCEL );
		g_free(message);
		if( aval != G_ALERTDEFAULT && aval != G_ALERTALTERNATE ) return;
	}
	else {
		message = g_strdup_printf(_("Really delete `%s' ?"), obj->name);
		aval = alertpanel(_("Delete"), message, GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		g_free(message);
		if (aval != G_ALERTDEFAULT) return;
	}

	/* Proceed with deletion */
	if( obj->type == ADDR_DATASOURCE ) {
		/* Remove data source. */
		if( addrindex_index_remove_datasource( _addressIndex_, ds ) ) {
			addressbook_free_child_adapters( node );
			remFlag = TRUE;
		}
	}
	else {
		abf = addressbook_get_book_file();
		if( abf == NULL ) return;
	}

	if( obj->type == ADDR_ITEM_FOLDER ) {
		AdapterFolder *adapter = ADAPTER_FOLDER(obj);
		ItemFolder *item = adapter->itemFolder;
		if( aval == G_ALERTDEFAULT ) {
			/* Remove folder only */
			item = addrbook_remove_folder( abf, item );
			if( item ) {
				addritem_free_item_folder( item );
				addressbook_move_nodes_up( ctree, node );
				remFlag = TRUE;
			}
		}
		else if( aval == G_ALERTALTERNATE ) {
			/* Remove folder and addresses */
			item = addrbook_remove_folder_delete( abf, item );
			if( item ) {
				addritem_free_item_folder( item );
				addressbook_free_child_adapters( node );
				remFlag = TRUE;
			}
		}
	}
	else if( obj->type == ADDR_ITEM_GROUP ) {
		AdapterGroup *adapter = ADAPTER_GROUP(obj);
		ItemGroup *item = adapter->itemGroup;

		item = addrbook_remove_group( abf, item );
		if( item ) {
			addritem_free_item_group( item );
			remFlag = TRUE;
		}
	}

	if( remFlag ) {
		/* Free up adapter and remove node. */
		addressbook_free_adapter( node );
		gtk_ctree_remove_node(ctree, node );
	}
}

static void addressbook_new_address_cb( gpointer data, guint action, GtkWidget *widget ) {
	AddressObject *pobj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;

	pobj = gtk_ctree_node_get_row_data(GTK_CTREE(addrbook.ctree), addrbook.treeSelected);
	if( pobj == NULL ) return;
	ds = addressbook_find_datasource( GTK_CTREE_NODE(addrbook.treeSelected) );
	if( ds == NULL ) return;

	abf = ds->rawDataSource;
	if( abf == NULL ) return;

	if( pobj->type == ADDR_DATASOURCE ) {
		if( ADAPTER_DSOURCE(pobj)->subType == ADDR_BOOK ) {
			/* New address */
			ItemPerson *person = addressbook_edit_person( abf, NULL, NULL, FALSE );
			if( person ) {
				if( addrbook.treeSelected == addrbook.opened ) {
					addrbook.treeSelected = NULL;
					gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
				}
			}
		}
	}
	else if( pobj->type == ADDR_ITEM_FOLDER ) {
		/* New address */
		ItemFolder *folder = ADAPTER_FOLDER(pobj)->itemFolder;
		ItemPerson *person = addressbook_edit_person( abf, folder, NULL, FALSE );
		if( person ) {
			if (addrbook.treeSelected == addrbook.opened) {
				addrbook.treeSelected = NULL;
				gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
			}
		}
	}
	else if( pobj->type == ADDR_ITEM_GROUP ) {
		/* New address in group */
		ItemGroup *group = ADAPTER_GROUP(pobj)->itemGroup;
		if( addressbook_edit_group( abf, NULL, group ) == NULL ) return;
		if (addrbook.treeSelected == addrbook.opened) {
			/* Change node name in tree. */
			addressbook_change_node_name( addrbook.treeSelected, ADDRITEM_NAME(group) );
			addrbook.treeSelected = NULL;
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
		}
	}
}

/*
* Search for specified group in address index tree.
*/
static GtkCTreeNode *addressbook_find_group_node( GtkCTreeNode *parent, ItemGroup *group ) {
	GtkCTreeNode *node = NULL;
	GtkCTreeRow *currRow;

	currRow = GTK_CTREE_ROW( parent );
	if( currRow ) {
		node = currRow->children;
		while( node ) {
			AddressObject *obj = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.ctree), node );
			if( obj->type == ADDR_ITEM_GROUP ) {
				ItemGroup *g = ADAPTER_GROUP(obj)->itemGroup;
				if( g == group ) return node;
			}
			currRow = GTK_CTREE_ROW(node);
			node = currRow->sibling;
		}
	}
	return NULL;
}

static AddressBookFile *addressbook_get_book_file() {
	AddressBookFile *abf = NULL;
	AddressDataSource *ds = NULL;

	ds = addressbook_find_datasource( addrbook.treeSelected );
	if( ds == NULL ) return NULL;
	if( ds->type == ADDR_IF_BOOK ) abf = ds->rawDataSource;
	return abf;
}

static void addressbook_tree_remove_children( GtkCTree *ctree, GtkCTreeNode *parent ) {
	GtkCTreeNode *node;
	GtkCTreeRow *row;

	/* Remove existing folders and groups */
	row = GTK_CTREE_ROW( parent );
	if( row ) {
		while( (node = row->children) ) {
			gtk_ctree_remove_node( ctree, node );
		}
	}
}

static void addressbook_move_nodes_up( GtkCTree *ctree, GtkCTreeNode *node ) {
	GtkCTreeNode *parent, *child;
	GtkCTreeRow *currRow;
	currRow = GTK_CTREE_ROW( node );
	if( currRow ) {
		parent = currRow->parent;
		while( (child = currRow->children) ) {
			gtk_ctree_move( ctree, child, parent, node );
		}
		gtk_ctree_sort_node( ctree, parent );
	}
}

static void addressbook_edit_address_cb( gpointer data, guint action, GtkWidget *widget ) {
	GtkCTree *clist = GTK_CTREE(addrbook.clist);
	GtkCTree *ctree;
	AddressObject *obj = NULL, *pobj = NULL;
	AddressDataSource *ds = NULL;
	GtkCTreeNode *node = NULL, *parentNode = NULL;
	gchar *name = NULL;
	AddressBookFile *abf = NULL;

	if( addrbook.listSelected == NULL ) return;
	obj = gtk_ctree_node_get_row_data( clist, addrbook.listSelected );
	g_return_if_fail(obj != NULL);

       	ctree = GTK_CTREE( addrbook.ctree );
	pobj = gtk_ctree_node_get_row_data( ctree, addrbook.treeSelected );
	node = gtk_ctree_find_by_row_data( ctree, addrbook.treeSelected, obj );

	ds = addressbook_find_datasource( GTK_CTREE_NODE(addrbook.treeSelected) );
	if( ds == NULL ) return;

	abf = addressbook_get_book_file();
	if( abf == NULL ) return;
	if( obj->type == ADDR_ITEM_EMAIL ) {
		ItemEMail *email = ( ItemEMail * ) obj;
		ItemPerson *person;
		if( email == NULL ) return;
		if( pobj && pobj->type == ADDR_ITEM_GROUP ) {
			/* Edit parent group */
			AdapterGroup *adapter = ADAPTER_GROUP(pobj);
			ItemGroup *itemGrp = adapter->itemGroup;
			if( addressbook_edit_group( abf, NULL, itemGrp ) == NULL ) return;
			name = ADDRITEM_NAME(itemGrp);
			node = addrbook.treeSelected;
			parentNode = GTK_CTREE_ROW(node)->parent;
		}
		else {
			/* Edit person - email page */
			person = ( ItemPerson * ) ADDRITEM_PARENT(email);
			if( addressbook_edit_person( abf, NULL, person, TRUE ) == NULL ) return;
			addrbook.treeSelected = NULL;
			gtk_ctree_select( ctree, addrbook.opened );
			invalidate_address_completion();
			return;
		}
	}
	else if( obj->type == ADDR_ITEM_PERSON ) {
		/* Edit person - basic page */
		ItemPerson *person = ( ItemPerson * ) obj;
		if( addressbook_edit_person( abf, NULL, person, FALSE ) == NULL ) return;
		addrbook.treeSelected = NULL;
		gtk_ctree_select( ctree, addrbook.opened );
		invalidate_address_completion();
		return;
	}
	else if( obj->type == ADDR_ITEM_GROUP ) {
		ItemGroup *itemGrp = ( ItemGroup * ) obj;
		if( addressbook_edit_group( abf, NULL, itemGrp ) == NULL ) return;
		parentNode = addrbook.treeSelected;
		node = addressbook_find_group_node( parentNode, itemGrp );
		name = ADDRITEM_NAME(itemGrp);
	}
	else {
		return;
	}

	/* Update tree node with node name */
	if( node == NULL ) return;
	addressbook_change_node_name( node, name );
	gtk_ctree_sort_node( ctree, parentNode );
	addrbook.treeSelected = NULL;
	gtk_ctree_select( ctree, addrbook.opened );
}

static void addressbook_delete_address_cb(gpointer data, guint action,
					  GtkWidget *widget)
{
	addressbook_del_clicked(NULL, NULL);
}

static void addressbook_copy_address_cb(gpointer data, guint action,
					GtkWidget *widget)
{
	GtkCTree *clist = GTK_CTREE( addrbook.clist );
	AddressObject *obj;

	if( addrbook.listSelected == NULL ) return;
	obj = gtk_ctree_node_get_row_data( clist, addrbook.listSelected );
	g_return_if_fail(obj != NULL);

	if( obj->type == ADDR_ITEM_PERSON ) {
		clipObj = obj;
	}
	else {
		return;
	}

	addressbook_menuitem_set_sensitive();
}

static void addressbook_paste_address_cb(gpointer data, guint action,
					 GtkWidget *widget)
{
	GtkCTree *clist = GTK_CTREE( addrbook.clist );
	GtkCTree *ctree = GTK_CTREE( addrbook.ctree );
	AddressObject *obj = NULL, *pobj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *folder = NULL;
	ItemPerson *clipPerson, *person;
	GList *cur;

	if (!clipObj) return;
	if (clipObj->type != ADDR_ITEM_PERSON) return;

	pobj = gtk_ctree_node_get_row_data( ctree, addrbook.treeSelected );
	if( pobj->type == ADDR_ITEM_FOLDER )
		folder = ADAPTER_FOLDER(pobj)->itemFolder;
	else if( pobj->type == ADDR_DATASOURCE )
		folder = NULL;
	else
		return;

	ds = addressbook_find_datasource( GTK_CTREE_NODE(addrbook.treeSelected) );
	if( ds == NULL ) return;

	abf = ds->rawDataSource;
	if( abf == NULL ) return;

	clipPerson = (ItemPerson *)clipObj;
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

	if (addrbook.treeSelected == addrbook.opened) {
		addrbook.treeSelected = NULL;
		gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
	}
}

static void close_cb(gpointer data, guint action, GtkWidget *widget)
{
	addressbook_close();
}

static void addressbook_file_save_cb( gpointer data, guint action, GtkWidget *widget ) {
	addressbook_export_to_file();
}

static void addressbook_person_expand_node( GtkCTree *ctree, GList *node, gpointer *data ) {
	if( node ) {
		ItemPerson *person = gtk_ctree_node_get_row_data( ctree, GTK_CTREE_NODE(node) );
		if( person ) addritem_person_set_opened( person, TRUE );
	}
}

static void addressbook_person_collapse_node( GtkCTree *ctree, GList *node, gpointer *data ) {
	if( node ) {
		ItemPerson *person = gtk_ctree_node_get_row_data( ctree, GTK_CTREE_NODE(node) );
		if( person ) addritem_person_set_opened( person, FALSE );
	}
}

static gchar *addressbook_format_item_clist( ItemPerson *person, ItemEMail *email ) {
	gchar *str = NULL;
	gchar *eMailAlias = ADDRITEM_NAME(email);
	if( eMailAlias && *eMailAlias != '\0' ) {
		if( person ) {
			str = g_strdup_printf( "%s - %s", ADDRITEM_NAME(person), eMailAlias );
		}
		else {
			str = g_strdup( eMailAlias );
		}
	}
	return str;
}

static void addressbook_load_group( GtkCTree *clist, ItemGroup *itemGroup ) {
	GList *items = itemGroup->listEMail;
	AddressTypeControlItem *atci = addrbookctl_lookup( ADDR_ITEM_EMAIL );
	for( ; items != NULL; items = g_list_next( items ) ) {
		GtkCTreeNode *nodeEMail = NULL;
		gchar *text[N_COLS];
		ItemEMail *email = items->data;
		ItemPerson *person;
		gchar *str = NULL;

		if( ! email ) continue;

		person = ( ItemPerson * ) ADDRITEM_PARENT(email);
		str = addressbook_format_item_clist( person, email );
		if( str ) {
			text[COL_NAME] = str;
		}
		else {
			text[COL_NAME] = ADDRITEM_NAME(person);
		}
		text[COL_ADDRESS] = email->address;
		text[COL_REMARKS] = email->remarks;
		nodeEMail = gtk_ctree_insert_node(
				clist, NULL, NULL,
				text, FOLDER_SPACING,
				atci->iconXpm, atci->maskXpm,
				atci->iconXpmOpen, atci->maskXpmOpen,
				FALSE, FALSE );
		gtk_ctree_node_set_row_data( clist, nodeEMail, email );
		g_free( str );
		str = NULL;
	}
}

static void addressbook_folder_load_person( GtkCTree *clist, ItemFolder *itemFolder ) {
	GList *items;
	AddressTypeControlItem *atci = addrbookctl_lookup( ADDR_ITEM_PERSON );
	AddressTypeControlItem *atciMail = addrbookctl_lookup( ADDR_ITEM_EMAIL );

	if( atci == NULL ) return;
	if( atciMail == NULL ) return;

	/* Load email addresses */
	items = addritem_folder_get_person_list( itemFolder );
	for( ; items != NULL; items = g_list_next( items ) ) {
		GtkCTreeNode *nodePerson = NULL;
		GtkCTreeNode *nodeEMail = NULL;
		gchar *text[N_COLS];
		gboolean flgFirst = TRUE, haveAddr = FALSE;
		/* gint row; */
		ItemPerson *person;
		GList *node;

		person = ( ItemPerson * ) items->data;
		if( person == NULL ) continue;

		text[COL_NAME] = NULL;
		node = person->listEMail;
		while( node ) {
			ItemEMail *email = node->data;
			gchar *eMailAddr = NULL;
			node = g_list_next( node );

			text[COL_ADDRESS] = email->address;
			text[COL_REMARKS] = email->remarks;
			eMailAddr = ADDRITEM_NAME(email);
			if( eMailAddr && *eMailAddr == '\0' ) eMailAddr = NULL;
			if( flgFirst ) {
				/* First email belongs with person */
				gchar *str = addressbook_format_item_clist( person, email );
				if( str ) {
					text[COL_NAME] = str;
				}
				else {
					text[COL_NAME] = ADDRITEM_NAME(person);
				}
				nodePerson = gtk_ctree_insert_node(
						clist, NULL, NULL,
						text, FOLDER_SPACING,
						atci->iconXpm, atci->maskXpm,
						atci->iconXpmOpen, atci->maskXpmOpen,
						FALSE, person->isOpened );
				g_free( str );
				str = NULL;
				gtk_ctree_node_set_row_data(clist, nodePerson, person );
			}
			else {
				/* Subsequent email is a child node of person */
				text[COL_NAME] = ADDRITEM_NAME(email);
				nodeEMail = gtk_ctree_insert_node(
						clist, nodePerson, NULL,
						text, FOLDER_SPACING,
						atciMail->iconXpm, atciMail->maskXpm,
						atciMail->iconXpmOpen, atciMail->maskXpmOpen,
						FALSE, TRUE );
				gtk_ctree_node_set_row_data(clist, nodeEMail, email );
			}
			flgFirst = FALSE;
			haveAddr = TRUE;
		}
		if( ! haveAddr ) {
			/* Have name without EMail */
			text[COL_NAME] = ADDRITEM_NAME(person);
			text[COL_ADDRESS] = NULL;
			text[COL_REMARKS] = NULL;
			nodePerson = gtk_ctree_insert_node(
					clist, NULL, NULL,
					text, FOLDER_SPACING,
					atci->iconXpm, atci->maskXpm,
					atci->iconXpmOpen, atci->maskXpmOpen,
					FALSE, person->isOpened );
			gtk_ctree_node_set_row_data(clist, nodePerson, person );
		}
	}
	gtk_ctree_sort_node(GTK_CTREE(clist), NULL);

	/* Free up the list */
	mgu_clear_list( items );
	g_list_free( items );
}

static void addressbook_folder_load_group( GtkCTree *clist, ItemFolder *itemFolder ) {
	GList *items;
	AddressTypeControlItem *atci =  addrbookctl_lookup( ADDR_ITEM_GROUP );

	/* Load any groups */
	if( ! atci ) return;
	items = addritem_folder_get_group_list( itemFolder );
	for( ; items != NULL; items = g_list_next( items ) ) {
		GtkCTreeNode *nodeGroup = NULL;
		gchar *text[N_COLS];
		ItemGroup *group = items->data;
		if( group == NULL ) continue;
		text[COL_NAME] = ADDRITEM_NAME(group);
		text[COL_ADDRESS] = NULL;
		text[COL_REMARKS] = NULL;
		nodeGroup = gtk_ctree_insert_node(clist, NULL, NULL,
				      text, FOLDER_SPACING,
				      atci->iconXpm, atci->maskXpm,
				      atci->iconXpmOpen, atci->maskXpmOpen,
				      FALSE, FALSE);
		gtk_ctree_node_set_row_data(clist, nodeGroup, group );
	}
	gtk_ctree_sort_node(clist, NULL);

	/* Free up the list */
	mgu_clear_list( items );
	g_list_free( items );
}

#if 0
/*
 * Load data sources into list.
 */
static void addressbook_node_load_datasource( GtkCTree *clist, AddressObject *obj ) {
	AdapterInterface *adapter;
	AddressInterface *iface;
	AddressTypeControlItem *atci = NULL;
	/* AddressDataSource *ds; */
	GtkCTreeNode *newNode, *node;
	GtkCTreeRow *row;
	GtkCell *cell = NULL;
	gchar *text[N_COLS];

	adapter = ADAPTER_INTERFACE(obj);
	if( adapter == NULL ) return;
	iface = adapter->interface;
	atci = adapter->atci;
	if( atci == NULL ) return;

	/* Create nodes in list copying values for data sources in tree */
	row = GTK_CTREE_ROW( adapter->treeNode );
	if( row ) {
		node = row->children;
		while( node ) {
			gpointer data = gtk_ctree_node_get_row_data( clist, node );
			row = GTK_CTREE_ROW( node );
			cell = ( ( GtkCListRow * )row )->cell;
			text[COL_NAME] = cell->u.text;
			text[COL_ADDRESS] = NULL;
			text[COL_REMARKS] = NULL;
			newNode = gtk_ctree_insert_node( clist, NULL, NULL,
				      text, FOLDER_SPACING,
				      atci->iconXpm, atci->maskXpm,
				      atci->iconXpmOpen, atci->maskXpmOpen,
				      FALSE, FALSE);
			gtk_ctree_node_set_row_data( clist, newNode, data );
			node = row->sibling;

		}
	}
	gtk_ctree_sort_node( clist, NULL );
}
#endif

static AddressDataSource *addressbook_find_datasource( GtkCTreeNode *node ) {
	AddressDataSource *ds = NULL;
	AddressObject *ao;

	g_return_val_if_fail(addrbook.ctree != NULL, NULL);

	while( node ) {
		if( GTK_CTREE_ROW(node)->level < 2 ) return NULL;
		ao = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.ctree), node );
		if( ao ) {
/*			printf( "ao->type = %d\n", ao->type ); */
			if( ao->type == ADDR_DATASOURCE ) {
				AdapterDSource *ads = ADAPTER_DSOURCE(ao);
/*				printf( "found it\n" ); */
				ds = ads->dataSource;
				break;
			}
		}
		node = GTK_CTREE_ROW(node)->parent;
	}
	return ds;
}

/*
* Load address list widget with children of specified object.
* Enter: obj	Parent object to be loaded.
*/
static void addressbook_set_clist( AddressObject *obj ) {
	GtkCTree *ctreelist = GTK_CTREE(addrbook.clist);
	GtkCList *clist = GTK_CLIST(addrbook.clist);
	AddressDataSource *ds = NULL;
	AdapterDSource *ads = NULL;

	if( obj == NULL ) {
		gtk_clist_clear(clist);
		return;
	}

	if( obj->type == ADDR_INTERFACE ) {
		/* printf( "set_clist: loading datasource...\n" ); */
		/* addressbook_node_load_datasource( clist, obj ); */
		return;
	}

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	if( obj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(obj);
		ds = ADAPTER_DSOURCE(obj)->dataSource;
		if( ds ) {
			/* Load root folder */
			ItemFolder *rootFolder = NULL;
			rootFolder = addrindex_ds_get_root_folder( ds );
			addressbook_folder_load_person( ctreelist, addrindex_ds_get_root_folder( ds ) );
			addressbook_folder_load_group( ctreelist, addrindex_ds_get_root_folder( ds ) );
		}
	}
	else {
		if( obj->type == ADDR_ITEM_GROUP ) {
			/* Load groups */
			ItemGroup *itemGroup = ADAPTER_GROUP(obj)->itemGroup;
			addressbook_load_group( ctreelist, itemGroup );
		}
		else if( obj->type == ADDR_ITEM_FOLDER ) {
			/* Load folders */
			ItemFolder *itemFolder = ADAPTER_FOLDER(obj)->itemFolder;
			addressbook_folder_load_person( ctreelist, itemFolder );
			addressbook_folder_load_group( ctreelist, itemFolder );
		}
	}

	gtk_clist_sort(clist);
	gtk_clist_thaw(clist);
}

/*
* Free adaptor for specified node.
*/
static void addressbook_free_adapter( GtkCTreeNode *node ) {
	AddressObject *ao;

	g_return_if_fail(addrbook.ctree != NULL);

	if( node ) {
		if( GTK_CTREE_ROW(node)->level < 2 ) return;
		ao = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.ctree), node );
		if( ao == NULL ) return;
		if( ao->type == ADDR_INTERFACE ) {
			AdapterInterface *ai = ADAPTER_INTERFACE(ao);
			addrbookctl_free_interface( ai );
		}
		else if( ao->type == ADDR_DATASOURCE ) {
			AdapterDSource *ads = ADAPTER_DSOURCE(ao);
			addrbookctl_free_datasource( ads );
		}
		else if( ao->type == ADDR_ITEM_FOLDER ) {
			AdapterFolder *af = ADAPTER_FOLDER(ao);
			addrbookctl_free_folder( af );
		}
		else if( ao->type == ADDR_ITEM_GROUP ) {
			AdapterGroup *ag = ADAPTER_GROUP(ao);
			addrbookctl_free_group( ag );
		}
		gtk_ctree_node_set_row_data( GTK_CTREE(addrbook.ctree), node, NULL );
	}
}

/*
* Free all children adapters.
*/
static void addressbook_free_child_adapters( GtkCTreeNode *node ) {
	GtkCTreeNode *parent, *child;
	GtkCTreeRow *currRow;

	if( node == NULL ) return;
	currRow = GTK_CTREE_ROW( node );
	if( currRow ) {
		parent = currRow->parent;
		child = currRow->children;
		while( child ) {
			addressbook_free_child_adapters( child );
			addressbook_free_adapter( child );
			currRow = GTK_CTREE_ROW( child );
			child = currRow->sibling;
		}
	}
}

AdapterDSource *addressbook_create_ds_adapter( AddressDataSource *ds,
				AddressObjectType otype, gchar *name )
{
	AdapterDSource *adapter = g_new0( AdapterDSource, 1 );
	ADDRESS_OBJECT(adapter)->type = ADDR_DATASOURCE;
	ADDRESS_OBJECT_NAME(adapter) = g_strdup( name );
	adapter->dataSource = ds;
	adapter->subType = otype;
	return adapter;
}

void addressbook_ads_set_name( AdapterDSource *adapter, gchar *value ) {
	ADDRESS_OBJECT_NAME(adapter) = mgu_replace_string( ADDRESS_OBJECT_NAME(adapter), value );
}

/*
 * Load tree from address index with the initial data.
 */
static void addressbook_load_tree( void ) {
	GtkCTree *ctree = GTK_CTREE( addrbook.ctree );
	GList *nodeIf, *nodeDS;
	AdapterInterface *adapter;
	AddressInterface *iface;
	AddressTypeControlItem *atci;
	AddressDataSource *ds;
	AdapterDSource *ads;
	GtkCTreeNode *node, *newNode;
	gchar *name;

	nodeIf = _addressInterfaceList_;
	while( nodeIf ) {
		adapter = nodeIf->data;
		node = adapter->treeNode;
		iface = adapter->interface;
		atci = adapter->atci;
		if( iface ) {
			if( iface->useInterface ) {
				/* Load data sources below interface node */
				nodeDS = iface->listSource;
				while( nodeDS ) {
					ds = nodeDS->data;
					newNode = NULL;
					name = addrindex_ds_get_name( ds );
					ads = addressbook_create_ds_adapter( ds, atci->objectType, name );
					newNode = addressbook_add_object( node, ADDRESS_OBJECT(ads) );
					nodeDS = g_list_next( nodeDS );
				}
				gtk_ctree_expand( ctree, node );
			}
		}
		nodeIf = g_list_next( nodeIf );
	}
}

/*
 * Convert the old address book to new format.
 */
static gboolean addressbook_convert( AddressIndex *addrIndex ) {
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
		alertpanel( _( "Addressbook conversion error" ), msg, GTK_STOCK_CLOSE, NULL, NULL );
	}
	else if( msg ) {
		debug_print( "Warning\n%s\n", msg );
		alertpanel( _( "Addressbook conversion" ), msg, GTK_STOCK_CLOSE, NULL, NULL );
	}

	return retVal;
}

void addressbook_read_file( void ) {
	AddressIndex *addrIndex = NULL;

	debug_print( "Reading address index...\n" );
	if( _addressIndex_ ) {
		debug_print( "address book already read!!!\n" );
		return;
	}

	addrIndex = addrindex_create_index();

	/* Use new address book index. */
	addrindex_set_file_path( addrIndex, get_rc_dir() );
	addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
	addrindex_read_data( addrIndex );
	if( addrIndex->retVal == MGU_NO_FILE ) {
		/* Conversion required */
		debug_print( "Converting...\n" );
		if( addressbook_convert( addrIndex ) ) {
			_addressIndex_ = addrIndex;
		}
	}
	else if( addrIndex->retVal == MGU_SUCCESS ) {
		_addressIndex_ = addrIndex;
	}
	else {
		/* Error reading address book */
		debug_print( "Could not read address index.\n" );
		addrindex_print_index( addrIndex, stdout );
		alertpanel( _( "Addressbook Error" ),
			    _( "Could not read address index" ),
			    GTK_STOCK_CLOSE, NULL, NULL );
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
		alertpanel( _( "Addressbook Conversion Error" ), msg,
			    GTK_STOCK_CLOSE, NULL, NULL );
	}
	else {
		if( msg ) {
			debug_print( "Warning\n%s\n", msg );
			alertpanel( _( "Addressbook Conversion" ), msg,
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
static GtkCTreeNode *addressbook_add_object(GtkCTreeNode *node,
					    AddressObject *obj)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkCTreeNode *added;
	AddressObject *pobj;
	AddressObjectType otype;
	AddressTypeControlItem *atci = NULL;

	g_return_val_if_fail(node != NULL, NULL);
	g_return_val_if_fail(obj  != NULL, NULL);

	pobj = gtk_ctree_node_get_row_data(ctree, node);
	g_return_val_if_fail(pobj != NULL, NULL);

	/* Determine object type to be displayed */
	if( obj->type == ADDR_DATASOURCE ) {
		otype = ADAPTER_DSOURCE(obj)->subType;
	}
	else {
		otype = obj->type;
	}

	/* Handle any special conditions. */
	added = node;
	atci = addrbookctl_lookup( otype );
	if( atci ) {
		if( atci->showInTree ) {
			/* Add object to tree */
			gchar **name;
			name = &obj->name;
			added = gtk_ctree_insert_node( ctree, node, NULL, name, FOLDER_SPACING,
				atci->iconXpm, atci->maskXpm, atci->iconXpmOpen, atci->maskXpmOpen,
				atci->treeLeaf, atci->treeExpand );
			gtk_ctree_node_set_row_data(ctree, added, obj);
		}
	}

	gtk_ctree_sort_node(ctree, node);

	return added;
}

/*
* Add group into the address index tree.
* Enter: node	   Parent node.
*        ds        Data source.
*        itemGroup Group to add.
* Return: Inserted node.
*/
static GtkCTreeNode *addressbook_node_add_group( GtkCTreeNode *node, AddressDataSource *ds, ItemGroup *itemGroup ) {
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkCTreeNode *newNode;
	AdapterGroup *adapter;
	AddressTypeControlItem *atci = NULL;
	gchar **name;

	if( ds == NULL ) return NULL;
	if( node == NULL || itemGroup == NULL ) return NULL;

	name = &itemGroup->obj.name;

	atci = addrbookctl_lookup( ADDR_ITEM_GROUP );

	adapter = g_new0( AdapterGroup, 1 );
	ADDRESS_OBJECT_TYPE(adapter) = ADDR_ITEM_GROUP;
	ADDRESS_OBJECT_NAME(adapter) = g_strdup( ADDRITEM_NAME(itemGroup) );
	adapter->itemGroup = itemGroup;

	newNode = gtk_ctree_insert_node( ctree, node, NULL, name, FOLDER_SPACING,
			atci->iconXpm, atci->maskXpm, atci->iconXpm, atci->maskXpm,
			atci->treeLeaf, atci->treeExpand );
	gtk_ctree_node_set_row_data( ctree, newNode, adapter );
	gtk_ctree_sort_node( ctree, node );
	return newNode;
}

/*
* Add folder into the address index tree.
* Enter: node	    Parent node.
*        ds         Data source.
*        itemFolder Folder to add.
*        otype      Object type to display.
* Return: Inserted node.
*/
static GtkCTreeNode *addressbook_node_add_folder(
		GtkCTreeNode *node, AddressDataSource *ds, ItemFolder *itemFolder, AddressObjectType otype )
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkCTreeNode *newNode = NULL;
	AdapterFolder *adapter;
	AddressTypeControlItem *atci = NULL;
	GList *listItems = NULL;
	gchar **name;
	ItemFolder *rootFolder;

	if( ds == NULL ) return NULL;
	if( node == NULL || itemFolder == NULL ) return NULL;

	/* Determine object type */
	atci = addrbookctl_lookup( otype );
	if( atci == NULL ) return NULL;

	rootFolder = addrindex_ds_get_root_folder( ds );
	if( itemFolder == rootFolder ) {
		newNode = node;
	}
	else {
		name = &itemFolder->obj.name;

		adapter = g_new0( AdapterFolder, 1 );
		ADDRESS_OBJECT_TYPE(adapter) = ADDR_ITEM_FOLDER;
		ADDRESS_OBJECT_NAME(adapter) = g_strdup( ADDRITEM_NAME(itemFolder) );
		adapter->itemFolder = itemFolder;

		newNode = gtk_ctree_insert_node( ctree, node, NULL, name, FOLDER_SPACING,
				atci->iconXpm, atci->maskXpm, atci->iconXpm, atci->maskXpm,
				atci->treeLeaf, atci->treeExpand );
		gtk_ctree_node_set_row_data( ctree, newNode, adapter );
	}

	listItems = itemFolder->listFolder;
	while( listItems ) {
		ItemFolder *item = listItems->data;
		addressbook_node_add_folder( newNode, ds, item, otype );
		listItems = g_list_next( listItems );
	}
	listItems = itemFolder->listGroup;
	while( listItems ) {
		ItemGroup *item = listItems->data;
		addressbook_node_add_group( newNode, ds, item );
		listItems = g_list_next( listItems );
	}
	gtk_ctree_sort_node( ctree, node );
	return newNode;
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
		invalidate_address_completion();
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
* Comparsion using names of AddressItem objects.
*/
/*
static gint addressbook_list_compare_func(GtkCList *clist,
					  gconstpointer ptr1,
					  gconstpointer ptr2)
{
	AddressObject *obj1 = ((GtkCListRow *)ptr1)->data;
	AddressObject *obj2 = ((GtkCListRow *)ptr2)->data;
	gchar *name1 = NULL, *name2 = NULL;
	if( obj1 ) name1 = obj1->name;
	if( obj2 ) name2 = obj2->name;
	if( ! name1 ) return ( name2 != NULL );
	if( ! name2 ) return -1;
	return g_ascii_strcasecmp(name1, name2);
}
*/

/*
* Comparison using cell contents (text in first column).
*/
static gint addressbook_list_compare_func( GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2 ) {
	GtkCell *cell1 = ((GtkCListRow *)ptr1)->cell;
	GtkCell *cell2 = ((GtkCListRow *)ptr2)->cell;
	gchar *name1 = NULL, *name2 = NULL;
	if( cell1 ) name1 = cell1->u.text;
	if( cell2 ) name2 = cell2->u.text;
	if( ! name1 ) return ( name2 != NULL );
	if( ! name2 ) return -1;
	return g_ascii_strcasecmp( name1, name2 );
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
	*addressbook_msgbuf = '\0';
	if( abf ) {
		if( abf->retVal == MGU_SUCCESS ) {
			sprintf( addressbook_msgbuf, "%s", abf->name );
		}
		else {
			sprintf( addressbook_msgbuf, "%s: %s", abf->name, mgu_error2string( abf->retVal ) );
		}
	}
	addressbook_status_show( addressbook_msgbuf );
}
#endif

static void addressbook_new_book_cb( gpointer data, guint action, GtkWidget *widget ) {
	AdapterDSource *ads;
	AdapterInterface *adapter;

	adapter = addrbookctl_find_interface( ADDR_IF_BOOK );
	if( adapter == NULL ) return;
	if( addrbook.treeSelected == NULL ) return;
	if( addrbook.treeSelected != adapter->treeNode ) return;
	ads = addressbook_edit_book( _addressIndex_, NULL );
	if( ads ) {
		addressbook_add_object( addrbook.treeSelected, ADDRESS_OBJECT(ads) );
		if( addrbook.treeSelected == addrbook.opened ) {
			addrbook.treeSelected = NULL;
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
		}
	}
}

static void addressbook_new_vcard_cb( gpointer data, guint action, GtkWidget *widget ) {
	AdapterDSource *ads;
	AdapterInterface *adapter;

	adapter = addrbookctl_find_interface( ADDR_IF_VCARD );
	if( adapter == NULL ) return;
	if( addrbook.treeSelected != adapter->treeNode ) return;
	ads = addressbook_edit_vcard( _addressIndex_, NULL );
	if( ads ) {
		addressbook_add_object( addrbook.treeSelected, ADDRESS_OBJECT(ads) );
		if( addrbook.treeSelected == addrbook.opened ) {
			addrbook.treeSelected = NULL;
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
		}
	}
}

#if 0
static void addressbook_vcard_show_message( VCardFile *vcf ) {
	*addressbook_msgbuf = '\0';
	if( vcf ) {
		if( vcf->retVal == MGU_SUCCESS ) {
			sprintf( addressbook_msgbuf, "%s", vcf->name );
		}
		else {
			sprintf( addressbook_msgbuf, "%s: %s", vcf->name, mgu_error2string( vcf->retVal ) );
		}
	}
	addressbook_status_show( addressbook_msgbuf );
}
#endif

#ifdef USE_JPILOT
static void addressbook_new_jpilot_cb( gpointer data, guint action, GtkWidget *widget ) {
	AdapterDSource *ads;
	AdapterInterface *adapter;
	AddressInterface *iface;

	adapter = addrbookctl_find_interface( ADDR_IF_JPILOT );
	if( adapter == NULL ) return;
	if( addrbook.treeSelected != adapter->treeNode ) return;
	iface = adapter->interface;
	if( ! iface->haveLibrary ) return;
	ads = addressbook_edit_jpilot( _addressIndex_, NULL );
	if( ads ) {
		addressbook_add_object( addrbook.treeSelected, ADDRESS_OBJECT(ads) );
		if( addrbook.treeSelected == addrbook.opened ) {
			addrbook.treeSelected = NULL;
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
		}
	}
}

#if 0
static void addressbook_jpilot_show_message( JPilotFile *jpf ) {
	*addressbook_msgbuf = '\0';
	if( jpf ) {
		if( jpf->retVal == MGU_SUCCESS ) {
			sprintf( addressbook_msgbuf, "%s", jpf->name );
		}
		else {
			sprintf( addressbook_msgbuf, "%s: %s", jpf->name, mgu_error2string( jpf->retVal ) );
		}
	}
	addressbook_status_show( addressbook_msgbuf );
}
#endif
#endif /* USE_JPILOT */

#ifdef USE_LDAP
static void addressbook_new_ldap_cb( gpointer data, guint action, GtkWidget *widget ) {
	AdapterDSource *ads;
	AdapterInterface *adapter;
	AddressInterface *iface;

	adapter = addrbookctl_find_interface( ADDR_IF_LDAP );
	if( adapter == NULL ) return;
	if( addrbook.treeSelected != adapter->treeNode ) return;
	iface = adapter->interface;
	if( ! iface->haveLibrary ) return;
	ads = addressbook_edit_ldap( _addressIndex_, NULL );
	if( ads ) {
		addressbook_add_object( addrbook.treeSelected, ADDRESS_OBJECT(ads) );
		if( addrbook.treeSelected == addrbook.opened ) {
			addrbook.treeSelected = NULL;
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
		}
	}
}

static void addressbook_ldap_show_message( SyldapServer *svr ) {
	*addressbook_msgbuf = '\0';
	if( svr ) {
		if( svr->busyFlag ) {
			sprintf( addressbook_msgbuf, "%s: %s", svr->name, ADDRESSBOOK_LDAP_BUSYMSG );
		}
		else {
			if( svr->retVal == MGU_SUCCESS ) {
				sprintf( addressbook_msgbuf, "%s", svr->name );
			}
			else {
				sprintf( addressbook_msgbuf, "%s: %s", svr->name, mgu_error2string( svr->retVal ) );
			}
		}
	}
	addressbook_status_show( addressbook_msgbuf );
}

static void ldapsearch_callback( SyldapServer *sls ) {
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	AdapterDSource *ads = NULL;
	AddressDataSource *ds = NULL;
	AddressInterface *iface = NULL;

	if( sls == NULL ) return;
	if( ! addrbook.treeSelected ) return;
	if( GTK_CTREE_ROW( addrbook.treeSelected )->level == 1 ) return;

	obj = gtk_ctree_node_get_row_data( ctree, addrbook.treeSelected );
	if( obj == NULL ) return;
	if( obj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(obj);
		if( ads->subType == ADDR_LDAP ) {
			SyldapServer *server;

			ds = ads->dataSource;
			if( ds == NULL ) return;
			iface = ds->interface;
			if( ! iface->haveLibrary ) return;
			server = ds->rawDataSource;
			if( server == sls ) {
				/* Read from cache */
				gtk_widget_show_all(addrbook.window);
				addressbook_set_clist( obj );
				addressbook_ldap_show_message( sls );
				gtk_widget_show_all(addrbook.window);
				gtk_entry_set_text( GTK_ENTRY(addrbook.entry), "" );
			}
		}
	}
}
#endif

/*
 * Lookup button handler.
 */
static void addressbook_lup_clicked( GtkButton *button, gpointer data ) {
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	AdapterDSource *ads = NULL;
#ifdef USE_LDAP
	AddressDataSource *ds = NULL;
	AddressInterface *iface = NULL;
#endif /* USE_LDAP */
	gchar *sLookup;

	if( ! addrbook.treeSelected ) return;
	if( GTK_CTREE_ROW( addrbook.treeSelected )->level == 1 ) return;

	obj = gtk_ctree_node_get_row_data( ctree, addrbook.treeSelected );
	if( obj == NULL ) return;

	sLookup = gtk_editable_get_chars( GTK_EDITABLE(addrbook.entry), 0, -1 );
	g_strchomp( sLookup );

	if( obj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(obj);
#ifdef USE_LDAP
		if( ads->subType == ADDR_LDAP ) {
			SyldapServer *server;

			ds = ads->dataSource;
			if( ds == NULL ) return;
			iface = ds->interface;
			if( ! iface->haveLibrary ) return;
			server = ds->rawDataSource;
			if( server ) {
				syldap_cancel_read( server );
				if( *sLookup == '\0' || strlen( sLookup ) < 1 ) return;
				syldap_set_search_value( server, sLookup );
				syldap_set_callback( server, ldapsearch_callback );
				syldap_read_data_th( server );
				addressbook_ldap_show_message( server );
			}
		}
#endif /* USE_LDAP */
	}

	g_free( sLookup );
}

/* **********************************************************************
* Build lookup tables.
* ***********************************************************************
*/

/*
* Build table that controls the rendering of object types.
*/
void addrbookctl_build_map( GtkWidget *window ) {
	AddressTypeControlItem *atci;

	/* Build icons */
	stock_pixmap_gdk(window, STOCK_PIXMAP_DIR_CLOSE, &folderxpm, &folderxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_DIR_OPEN, &folderopenxpm, &folderopenxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_GROUP, &groupxpm, &groupxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_VCARD, &vcardxpm, &vcardxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_BOOK, &bookxpm, &bookxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_ADDRESS, &addressxpm, &addressxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_JPILOT, &jpilotxpm, &jpilotxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_CATEGORY, &categoryxpm, &categoryxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_LDAP, &ldapxpm, &ldapxpmmask);

	_addressBookTypeHash_ = g_hash_table_new( g_int_hash, g_int_equal );
	_addressBookTypeList_ = NULL;

	/* Interface */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_INTERFACE;
	atci->interfaceType = ADDR_IF_NONE;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "Interface" );
	atci->iconXpm = folderxpm;
	atci->maskXpm = folderxpmmask;
	atci->iconXpmOpen = folderopenxpm;
	atci->maskXpmOpen = folderopenxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Address book */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_BOOK;
	atci->interfaceType = ADDR_IF_BOOK;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "Address Book" );
	atci->iconXpm = bookxpm;
	atci->maskXpm = bookxpmmask;
	atci->iconXpmOpen = bookxpm;
	atci->maskXpmOpen = bookxpmmask;
	atci->menuCommand = "/File/New Book";
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Item person */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_ITEM_PERSON;
	atci->interfaceType = ADDR_IF_NONE;
	atci->showInTree = FALSE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "Person" );
	atci->iconXpm = NULL;
	atci->maskXpm = NULL;
	atci->iconXpmOpen = NULL;
	atci->maskXpmOpen = NULL;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Item email */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_ITEM_EMAIL;
	atci->interfaceType = ADDR_IF_NONE;
	atci->showInTree = FALSE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = TRUE;
	atci->displayName = _( "EMail Address" );
	atci->iconXpm = addressxpm;
	atci->maskXpm = addressxpmmask;
	atci->iconXpmOpen = addressxpm;
	atci->maskXpmOpen = addressxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Item group */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_ITEM_GROUP;
	atci->interfaceType = ADDR_IF_BOOK;
	atci->showInTree = TRUE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "Group" );
	atci->iconXpm = groupxpm;
	atci->maskXpm = groupxpmmask;
	atci->iconXpmOpen = groupxpm;
	atci->maskXpmOpen = groupxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Item folder */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_ITEM_FOLDER;
	atci->interfaceType = ADDR_IF_BOOK;
	atci->showInTree = TRUE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "Folder" );
	atci->iconXpm = folderxpm;
	atci->maskXpm = folderxpmmask;
	atci->iconXpmOpen = folderopenxpm;
	atci->maskXpmOpen = folderopenxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* vCard */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_VCARD;
	atci->interfaceType = ADDR_IF_VCARD;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = TRUE;
	atci->displayName = _( "vCard" );
	atci->iconXpm = vcardxpm;
	atci->maskXpm = vcardxpmmask;
	atci->iconXpmOpen = vcardxpm;
	atci->maskXpmOpen = vcardxpmmask;
	atci->menuCommand = "/File/New vCard";
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* JPilot */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_JPILOT;
	atci->interfaceType = ADDR_IF_JPILOT;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "JPilot" );
	atci->iconXpm = jpilotxpm;
	atci->maskXpm = jpilotxpmmask;
	atci->iconXpmOpen = jpilotxpm;
	atci->maskXpmOpen = jpilotxpmmask;
	atci->menuCommand = "/File/New JPilot";
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Category */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_CATEGORY;
	atci->interfaceType = ADDR_IF_JPILOT;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = TRUE;
	atci->displayName = _( "JPilot" );
	atci->iconXpm = categoryxpm;
	atci->maskXpm = categoryxpmmask;
	atci->iconXpmOpen = categoryxpm;
	atci->maskXpmOpen = categoryxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* LDAP Server */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_LDAP;
	atci->interfaceType = ADDR_IF_LDAP;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = TRUE;
	atci->displayName = _( "LDAP Server" );
	atci->iconXpm = ldapxpm;
	atci->maskXpm = ldapxpmmask;
	atci->iconXpmOpen = ldapxpm;
	atci->maskXpmOpen = ldapxpmmask;
	atci->menuCommand = "/File/New LDAP Server";
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

}

/*
* Search for specified object type.
*/
AddressTypeControlItem *addrbookctl_lookup( gint ot ) {
	gint objType = ot;
	return ( AddressTypeControlItem * ) g_hash_table_lookup( _addressBookTypeHash_, &objType );
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

static void addrbookctl_free_interface( AdapterInterface *adapter ) {
	addrbookctl_free_address( ADDRESS_OBJECT(adapter) );
	adapter->interface = NULL;
	adapter->interfaceType = ADDR_IF_NONE;
	adapter->atci = NULL;
	adapter->enabled = FALSE;
	adapter->haveLibrary = FALSE;
	adapter->treeNode = NULL;
	g_free( adapter );
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
void addrbookctl_build_iflist() {
	AddressTypeControlItem *atci;
	AdapterInterface *adapter;
	GList *list = NULL;

	if( _addressIndex_ == NULL ) {
		_addressIndex_ = addrindex_create_index();
	}
	_addressInterfaceList_ = NULL;
	list = addrindex_get_interface_list( _addressIndex_ );
	while( list ) {
		AddressInterface *interface = list->data;
		atci = addrbookctl_lookup_iface( interface->type );
		if( atci ) {
			adapter = g_new0( AdapterInterface, 1 );
			adapter->interfaceType = interface->type;
			adapter->atci = atci;
			adapter->interface = interface;
			adapter->treeNode = NULL;
			adapter->enabled = TRUE;
			adapter->haveLibrary = interface->haveLibrary;
			ADDRESS_OBJECT(adapter)->type = ADDR_INTERFACE;
			ADDRESS_OBJECT_NAME(adapter) = g_strdup( atci->displayName );
			_addressInterfaceList_ = g_list_append( _addressInterfaceList_, adapter );
		}
		list = g_list_next( list );
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
void addrbookctl_build_ifselect() {
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
gboolean addressbook_add_contact( const gchar *name, const gchar *address, const gchar *remarks ) {
	debug_print( "addressbook_add_contact: name/address: %s - %s\n", name, address );
	if( addressadd_selection( _addressIndex_, name, address, remarks ) ) {
		debug_print( "addressbook_add_contact - added\n" );
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
gboolean addressbook_load_completion( gint (*callBackFunc) ( const gchar *, const gchar * ) ) {
	/* AddressInterface *interface; */
	AddressDataSource *ds;
	GList *nodeIf, *nodeDS;
	GList *listP, *nodeP;
	GList *nodeM;
	gchar *sName, *sAddress, *sAlias, *sFriendly;

	debug_print( "addressbook_load_completion\n" );

	if( _addressIndex_ == NULL ) return FALSE;

	nodeIf = addrindex_get_interface_list( _addressIndex_ );
	while( nodeIf ) {
		AddressInterface *interface = nodeIf->data;
		nodeDS = interface->listSource;
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
				sName = person->nickName;
				if( sName == NULL || *sName == '\0' ) {
					sName = ADDRITEM_NAME(person);
				}

				/* Process each E-Mail address */
				while( nodeM ) {
					ItemEMail *email = nodeM->data;
					/* Have mail */
					sFriendly = sName;
					sAddress = email->address;
					if( sAddress || *sAddress != '\0' ) {
						sAlias = ADDRITEM_NAME(email);
						if( sAlias && *sAlias != '\0' ) {
							sFriendly = sAlias;
						}
						( callBackFunc ) ( sFriendly, sAddress );
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

/* **********************************************************************
* Address Import.
* ***********************************************************************
*/

/*
* Import LDIF file.
*/
static void addressbook_import_ldif_cb() {
	AddressDataSource *ds = NULL;
	AdapterDSource *ads = NULL;
	AddressBookFile *abf = NULL;
	AdapterInterface *adapter;
	GtkCTreeNode *newNode;

	adapter = addrbookctl_find_interface( ADDR_IF_BOOK );
	if ( !adapter || !adapter->treeNode ) return;

	abf = addressbook_imp_ldif( _addressIndex_ );
	if ( !abf ) return;

	ds = addrindex_index_add_datasource( _addressIndex_, ADDR_IF_BOOK, abf );
	ads = addressbook_create_ds_adapter( ds, ADDR_BOOK, NULL );
	addressbook_ads_set_name( ads, abf->name );
	newNode = addressbook_add_object( adapter->treeNode, ADDRESS_OBJECT(ads) );
	if ( newNode ) {
		addrbook.treeSelected = NULL;
		gtk_ctree_select( GTK_CTREE(addrbook.ctree), newNode );
		addrbook.treeSelected = newNode;
	}

	/* Notify address completion */
	invalidate_address_completion();
}

/*
* End of Source.
*/
