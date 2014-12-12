/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
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
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkselection.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkvbbox.h>
#include <stdio.h>
#include <unistd.h>

#ifdef G_OS_WIN32
#  include <windows.h>
#endif

#include "main.h"
#include "mimeview.h"
#include "textview.h"
#include "imageview.h"
#include "procmime.h"
#include "procheader.h"
#include "summaryview.h"
#include "menu.h"
#include "compose.h"
#include "printing.h"
#include "filesel.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "rfc2015.h"

enum
{
	COL_MIMETYPE,
	COL_SIZE,
	COL_NAME,
	COL_MIME_INFO,
	N_COLS
};

static void mimeview_set_multipart_tree		(MimeView	*mimeview,
						 MimeInfo	*mimeinfo,
						 GtkTreeIter	*parent);
static gboolean mimeview_append_part		(MimeView	*mimeview,
						 MimeInfo	*partinfo,
						 GtkTreeIter	*iter,
						 GtkTreeIter	*parent);
static void mimeview_show_message_part		(MimeView	*mimeview,
						 MimeInfo	*partinfo);
static void mimeview_show_image_part		(MimeView	*mimeview,
						 MimeInfo	*partinfo);
static void mimeview_show_mime_part		(MimeView	*mimeview,
						 MimeInfo	*partinfo);
#if USE_GPGME
static void mimeview_show_signature_part	(MimeView	*mimeview,
						 MimeInfo	*partinfo);
#endif
static void mimeview_change_view_type		(MimeView	*mimeview,
						 MimeViewType	 type);

static void mimeview_selection_changed	(GtkTreeSelection	*selection,
					 MimeView		*mimeview);

static gint mimeview_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 MimeView	*mimeview);
static gint mimeview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 MimeView	*mimeview);

static void mimeview_drag_begin 	(GtkWidget	  *widget,
					 GdkDragContext	  *drag_context,
					 MimeView	  *mimeview);
static void mimeview_drag_end	 	(GtkWidget	  *widget,
					 GdkDragContext	  *drag_context,
					 MimeView	  *mimeview);
static void mimeview_drag_data_get      (GtkWidget	  *widget,
					 GdkDragContext   *drag_context,
					 GtkSelectionData *selection_data,
					 guint		   info,
					 guint		   time,
					 MimeView	  *mimeview);

static void mimeview_display_as_text	(MimeView	*mimeview);
static void mimeview_launch		(MimeView	*mimeview);
static void mimeview_open_with		(MimeView	*mimeview);
static void mimeview_view_file		(const gchar	*filename,
					 MimeInfo	*partinfo,
					 const gchar	*cmdline);

static void mimeview_reply		(MimeView	*mimeview,
					 guint		 action);

#if USE_GPGME
static void mimeview_check_signature	(MimeView	*mimeview);
#endif

static GtkItemFactoryEntry mimeview_popup_entries[] =
{
	{N_("/_Open"),		  NULL, mimeview_launch,	  0, NULL},
	{N_("/Open _with..."),	  NULL, mimeview_open_with,	  0, NULL},
	{N_("/_Display as text"), NULL, mimeview_display_as_text, 0, NULL},
	{N_("/_Save as..."),	  NULL, mimeview_save_as,	  0, NULL},
	{N_("/Save _all..."),	  NULL, mimeview_save_all,	  0, NULL},
	{N_("/_Print..."),	  NULL, mimeview_print,		  0, NULL},
	{N_("/---"),		  NULL, NULL,			  0, "<Separator>"},
	{N_("/_Reply"),		  NULL, NULL,			  0, "<Branch>"},
	{N_("/_Reply/_Reply"),	  NULL, mimeview_reply,		  COMPOSE_REPLY, NULL},
	{N_("/_Reply/Reply to _all"),
				  NULL, mimeview_reply,		  COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/_Reply/Reply to _sender"),
				  NULL, mimeview_reply,		  COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/_Reply/Reply to mailing _list"),
				  NULL, mimeview_reply,		  COMPOSE_REPLY_TO_LIST, NULL},
#if USE_GPGME
	{N_("/---"),		  NULL, NULL,			  0, "<Separator>"},
        {N_("/_Check signature"), NULL, mimeview_check_signature, 0, NULL}
#endif
};

static GtkTargetEntry mimeview_mime_types[] =
{
	{"text/uri-list", 0, 0}
};

MimeView *mimeview_create(void)
{
	MimeView *mimeview;

	GtkWidget *paned;
	GtkWidget *scrolledwin;
	GtkWidget *treeview;
	GtkTreeStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *mime_vbox;
	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	gint n_entries;
	GtkWidget *reply_separator;
	GtkWidget *reply_menuitem;
	GList *child;

	debug_print(_("Creating MIME view...\n"));
	mimeview = g_new0(MimeView, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_widget_set_size_request(scrolledwin, -1, 80);

	store = gtk_tree_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_POINTER);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), COL_NAME);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), FALSE);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Data type"), renderer, "text", COL_MIMETYPE, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "xalign", 1.0, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Size"), renderer, "text", COL_SIZE, NULL);
	gtk_tree_view_column_set_alignment(column, 1.0);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ypad", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Name"), renderer, "text", COL_NAME, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_tree_view_enable_model_drag_source
		(GTK_TREE_VIEW(treeview), GDK_BUTTON1_MASK,
		 mimeview_mime_types, 1, GDK_ACTION_COPY);

	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(mimeview_selection_changed), mimeview);
	g_signal_connect(G_OBJECT(treeview), "button_press_event",
			 G_CALLBACK(mimeview_button_pressed), mimeview);
	g_signal_connect(G_OBJECT(treeview), "key_press_event",
			 G_CALLBACK(mimeview_key_pressed), mimeview);

	g_signal_connect_after(G_OBJECT (treeview),"drag-begin",
			       G_CALLBACK (mimeview_drag_begin), mimeview);
	g_signal_connect(G_OBJECT (treeview),"drag-end",
			 G_CALLBACK (mimeview_drag_end), mimeview);
	g_signal_connect(G_OBJECT(treeview), "drag-data-get",
			 G_CALLBACK(mimeview_drag_data_get), mimeview);
    
	mime_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_reallocate_redraws(GTK_CONTAINER(mime_vbox), TRUE);

	paned = gtk_vpaned_new();
	gtk_paned_add1(GTK_PANED(paned), scrolledwin);
	gtk_paned_add2(GTK_PANED(paned), mime_vbox);

	n_entries = sizeof(mimeview_popup_entries) /
		sizeof(mimeview_popup_entries[0]);
	popupmenu = menu_create_items(mimeview_popup_entries, n_entries,
				      "<MimeView>", &popupfactory, mimeview);

	reply_menuitem = gtk_item_factory_get_item(popupfactory, "/Reply");
	child = g_list_find(GTK_MENU_SHELL(popupmenu)->children,
			    reply_menuitem);
	reply_separator = GTK_WIDGET(child->prev->data);

	mimeview->paned        = paned;
	mimeview->scrolledwin  = scrolledwin;
	mimeview->treeview     = treeview;
	mimeview->store        = store;
	mimeview->selection    = selection;
	mimeview->mime_vbox    = mime_vbox;
	mimeview->popupmenu    = popupmenu;
	mimeview->popupfactory = popupfactory;
	mimeview->reply_separator = reply_separator;
	mimeview->reply_menuitem  = reply_menuitem;
	mimeview->type         = -1;

	return mimeview;
}

void mimeview_init(MimeView *mimeview)
{
	textview_init(mimeview->textview);
	imageview_init(mimeview->imageview);
}

/* 
 * Check whether the message is OpenPGP signed
 */
#if USE_GPGME
static gboolean mimeview_is_signed(MimeView *mimeview)
{
	MimeInfo *partinfo;

        debug_print("mimeview_is signed of %p\n", mimeview);

        if (!mimeview) return FALSE;
	if (!mimeview->opened) return FALSE;
	if (!rfc2015_is_available()) return FALSE;

        debug_print("mimeview_is_signed: open\n" );

	if (!mimeview->messageview->file) return FALSE;

        debug_print("mimeview_is_signed: file\n" );

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_val_if_fail(partinfo != NULL, FALSE);

	/* walk the tree and see whether there is a signature somewhere */
	do {
		if (rfc2015_has_signature(partinfo))
			return TRUE;
        } while ((partinfo = partinfo->parent) != NULL);

	debug_print("mimeview_is_signed: FALSE\n" );

	return FALSE;
}

static void set_unchecked_signature(MimeInfo *mimeinfo)
{
	MimeInfo **signedinfo;

	if (!rfc2015_is_available()) return;

	signedinfo = rfc2015_find_signature(mimeinfo);
	if (signedinfo == NULL) return;

	g_free(signedinfo[1]->sigstatus);
	signedinfo[1]->sigstatus =
		g_strdup(_("Select \"Check signature\" to check"));

	g_free(signedinfo[1]->sigstatus_full);
	signedinfo[1]->sigstatus_full = NULL;

	g_free(signedinfo);
}
#endif /* USE_GPGME */

void mimeview_show_message(MimeView *mimeview, MimeInfo *mimeinfo,
			   const gchar *file)
{
	GtkTreeModel *model = GTK_TREE_MODEL(mimeview->store);
	GtkTreeIter iter;
	gboolean valid;

	mimeview_clear(mimeview);
	textview_clear(mimeview->messageview->textview);

	g_return_if_fail(file != NULL);
	g_return_if_fail(mimeinfo != NULL);

#if USE_GPGME
	if (rfc2015_is_available() && prefs_common.auto_check_signatures) {
		FILE *fp;

		if ((fp = g_fopen(file, "rb")) == NULL) {
			FILE_OP_ERROR(file, "fopen");
			return;
		}
		rfc2015_check_signature(mimeinfo, fp);
		fclose(fp);
	} else
		set_unchecked_signature(mimeinfo);
#endif

	g_signal_handlers_block_by_func
		(G_OBJECT(mimeview->selection),
		 G_CALLBACK(mimeview_selection_changed), mimeview);

	mimeview_set_multipart_tree(mimeview, mimeinfo, NULL);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(mimeview->treeview));

	g_signal_handlers_unblock_by_func
		(G_OBJECT(mimeview->selection),
		 G_CALLBACK(mimeview_selection_changed), mimeview);

	/* search first text part */
	for (valid = gtk_tree_model_get_iter_first(model, &iter); valid;
	     valid = gtkut_tree_model_next(model, &iter)) {
		MimeInfo *partinfo;

		gtk_tree_model_get(model, &iter, COL_MIME_INFO, &partinfo, -1);
		if (partinfo &&
		    (partinfo->mime_type == MIME_TEXT ||
		     partinfo->mime_type == MIME_TEXT_HTML))
			break;
	}
	textview_show_message(mimeview->messageview->textview, mimeinfo, file);

	if (!valid)
		valid = gtk_tree_model_get_iter_first(model, &iter);

	if (valid) {
		GtkTreePath *path;

		path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(mimeview->treeview),
					 path, NULL, FALSE);
		gtk_tree_path_free(path);
		if (mimeview_get_selected_part(mimeview))
			gtk_widget_grab_focus(mimeview->treeview);
	}
}

void mimeview_clear(MimeView *mimeview)
{
	mimeview->has_attach_file = FALSE;

	gtk_tree_store_clear(mimeview->store);
	textview_clear(mimeview->textview);
	imageview_clear(mimeview->imageview);

	gtk_tree_path_free(mimeview->opened);
	mimeview->opened = NULL;

	g_free(mimeview->drag_file);
	mimeview->drag_file = NULL;
}

void mimeview_destroy(MimeView *mimeview)
{
	textview_destroy(mimeview->textview);
	imageview_destroy(mimeview->imageview);
	g_object_unref(mimeview->popupfactory);
	g_free(mimeview->drag_file);
	g_free(mimeview);
}

MimeInfo *mimeview_get_selected_part(MimeView *mimeview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(mimeview->store);
	GtkTreeIter iter;
	MimeInfo *partinfo = NULL;

	if (!mimeview->opened)
		return NULL;
	if (gtk_notebook_get_current_page
		(GTK_NOTEBOOK(mimeview->messageview->notebook)) == 0)
		return NULL;

	if (gtk_tree_model_get_iter(model, &iter, mimeview->opened))
		gtk_tree_model_get(model, &iter, COL_MIME_INFO, &partinfo, -1);

	return partinfo;
}

gboolean mimeview_step(MimeView *mimeview, GtkScrollType type)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(mimeview->treeview);
	GtkTreeModel *model = GTK_TREE_MODEL(mimeview->store);
	GtkTreeIter iter;
	gboolean moved;

	if (!mimeview->opened)
		return FALSE;
	if (!gtk_tree_model_get_iter(model, &iter, mimeview->opened))
		return FALSE;

	if (type == GTK_SCROLL_STEP_FORWARD) {
		if (gtkut_tree_model_next(model, &iter))
			gtkut_tree_view_expand_parent_all(treeview, &iter);
		else
			return FALSE;
	} else {
		if (!gtkut_tree_model_prev(model, &iter))
			return FALSE;
	}

	g_signal_emit_by_name(G_OBJECT(treeview), "move-cursor",
			      GTK_MOVEMENT_DISPLAY_LINES,
			      type == GTK_SCROLL_STEP_FORWARD ? 1 : -1, &moved);

	return TRUE;
}

static void mimeview_set_multipart_tree(MimeView *mimeview,
					MimeInfo *mimeinfo,
					GtkTreeIter *parent)
{
	GtkTreeIter iter;

	g_return_if_fail(mimeinfo != NULL);

	if (mimeinfo->children)
		mimeinfo = mimeinfo->children;

	while (mimeinfo != NULL) {
		mimeview_append_part(mimeview, mimeinfo, &iter, parent);

		if (mimeinfo->children)
			mimeview_set_multipart_tree(mimeview, mimeinfo, &iter);
		else if (mimeinfo->sub &&
			 mimeinfo->sub->mime_type != MIME_TEXT &&
			 mimeinfo->sub->mime_type != MIME_TEXT_HTML)
			mimeview_set_multipart_tree(mimeview, mimeinfo->sub,
						    &iter);
		mimeinfo = mimeinfo->next;
	}
}

static gchar *get_part_name(MimeInfo *partinfo)
{
#if USE_GPGME
	if (partinfo->sigstatus)
		return partinfo->sigstatus;
	else
#endif
	if (partinfo->name)
		return partinfo->name;
	else if (partinfo->filename)
		return partinfo->filename;
	else
		return "";
}

static gboolean mimeview_append_part(MimeView *mimeview, MimeInfo *partinfo,
				     GtkTreeIter *iter, GtkTreeIter *parent)
{
	gchar *mime_type;
	gchar *size;
	gchar *name;

	mime_type = partinfo->content_type ? partinfo->content_type : "";
	size = to_human_readable(partinfo->content_size);
	name = get_part_name(partinfo);
	if (name && *name != '\0')
		mimeview->has_attach_file = TRUE;

	gtk_tree_store_append(mimeview->store, iter, parent);
	gtk_tree_store_set(mimeview->store, iter,
			   COL_MIMETYPE, mime_type,
			   COL_SIZE, size,
			   COL_NAME, name,
			   COL_MIME_INFO, partinfo,
			   -1);

	return TRUE;
}

static void mimeview_show_message_part(MimeView *mimeview, MimeInfo *partinfo)
{
	FILE *fp;
	const gchar *fname;

	if (!partinfo) return;

	fname = mimeview->messageview->file;
	if (!fname) return;

	if ((fp = g_fopen(fname, "rb")) == NULL) {
		FILE_OP_ERROR(fname, "fopen");
		return;
	}

	if (fseek(fp, partinfo->fpos, SEEK_SET) < 0) {
		FILE_OP_ERROR(fname, "fseek");
		fclose(fp);
		return;
	}

	mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
	textview_show_part(mimeview->textview, partinfo, fp);

	fclose(fp);
}

static void mimeview_show_image_part(MimeView *mimeview, MimeInfo *partinfo)
{
	gchar *filename;

	if (!partinfo) return;

	filename = procmime_get_tmp_file_name(partinfo);

	if (procmime_get_part(filename, mimeview->messageview->file, partinfo) < 0)
		alertpanel_error
			(_("Can't get the part of multipart message."));
	else {
		mimeview_change_view_type(mimeview, MIMEVIEW_IMAGE);
		imageview_show_image(mimeview->imageview, partinfo, filename,
				     prefs_common.resize_image);
		g_unlink(filename);
	}

	g_free(filename);
}

static void save_as_button_clicked(GtkWidget *widget, gpointer data)
{
	MimeView *mimeview = (MimeView *)data;

	mimeview_save_as(mimeview);
}

static void display_as_text_button_clicked(GtkWidget *widget, gpointer data)
{
	MimeView *mimeview = (MimeView *)data;

	mimeview_display_as_text(mimeview);
}

static void open_button_clicked(GtkWidget *widget, gpointer data)
{
	MimeView *mimeview = (MimeView *)data;

	mimeview_launch(mimeview);
}

static void open_with_button_clicked(GtkWidget *widget, gpointer data)
{
	MimeView *mimeview = (MimeView *)data;

	mimeview_open_with(mimeview);
}

static void mimeview_show_mime_part(MimeView *mimeview, MimeInfo *partinfo)
{
	TextView *textview = mimeview->textview;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextChildAnchor *anchor;
	GtkWidget *vbbox;
	GtkWidget *button;
	gchar buf[BUFFSIZE];

	if (!partinfo) return;

	textview_set_font(textview, NULL);
	textview_clear(textview);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));
	gtk_text_buffer_get_start_iter(buffer, &iter);

	gtk_text_buffer_insert(buffer, &iter,
			       _("Select an action for the attached file:\n"),
			       -1);
	if (partinfo->filename || partinfo->name)
		g_snprintf(buf, sizeof(buf), "[%s  %s (%s)]\n\n",
			   partinfo->filename ? partinfo->filename :
			   partinfo->name,
			   partinfo->content_type,
			   to_human_readable(partinfo->content_size));
	else
		g_snprintf(buf, sizeof(buf), "[%s (%s)]\n\n",
			   partinfo->content_type,
			   to_human_readable(partinfo->content_size));
	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, buf, -1,
						 "mimepart", NULL);

	vbbox = gtk_vbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(vbbox), 5);

	button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_container_add(GTK_CONTAINER(vbbox), button);
	g_signal_connect(button, "clicked", G_CALLBACK(open_button_clicked),
			 mimeview);
	button = gtk_button_new_with_mnemonic(_("Open _with..."));
	gtk_container_add(GTK_CONTAINER(vbbox), button);
	g_signal_connect(button, "clicked",
			 G_CALLBACK(open_with_button_clicked), mimeview);
	button = gtk_button_new_with_mnemonic(_("_Display as text"));
	gtk_container_add(GTK_CONTAINER(vbbox), button);
	g_signal_connect(button, "clicked",
			 G_CALLBACK(display_as_text_button_clicked), mimeview);
	button = gtk_button_new_with_mnemonic(_("_Save as..."));
	gtk_container_add(GTK_CONTAINER(vbbox), button);
	g_signal_connect(button, "clicked", G_CALLBACK(save_as_button_clicked),
			 mimeview);

	gtk_widget_show_all(vbbox);

	anchor = gtk_text_buffer_create_child_anchor(buffer, &iter);
	gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(textview->text),
					  vbbox, anchor);
}

#if USE_GPGME
static void check_signature_button_clicked(GtkWidget *widget, gpointer data)
{
	MimeView *mimeview = (MimeView *)data;

	mimeview_check_signature(mimeview);
}

static void mimeview_show_signature_part(MimeView *mimeview,
					 MimeInfo *partinfo)
{
	TextView *textview = mimeview->textview;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextChildAnchor *anchor;
	GtkWidget *vbbox;
	GtkWidget *button;

	if (!partinfo) return;

	textview_set_font(textview, NULL);
	textview_clear(textview);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));
	gtk_text_buffer_get_start_iter(buffer, &iter);

	if (partinfo->sigstatus_full) {
		gtk_text_buffer_insert
			(buffer, &iter, partinfo->sigstatus_full, -1);
		return;
	}

	gtk_text_buffer_insert
		(buffer, &iter,
		 _("This signature has not been checked yet.\n\n"), -1);

	vbbox = gtk_vbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(vbbox), 5);

	if (rfc2015_is_available()) {
		button = gtk_button_new_with_mnemonic(_("_Check signature"));
		gtk_container_add(GTK_CONTAINER(vbbox), button);
		g_signal_connect(button, "clicked",
				 G_CALLBACK(check_signature_button_clicked),
				 mimeview);
	}

	gtk_widget_show_all(vbbox);

	anchor = gtk_text_buffer_create_child_anchor(buffer, &iter);
	gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(textview->text),
					  vbbox, anchor);
}
#endif /* USE_GPGME */

static void mimeview_change_view_type(MimeView *mimeview, MimeViewType type)
{
	TextView  *textview  = mimeview->textview;
	ImageView *imageview = mimeview->imageview;
	GList *children;

	if (mimeview->type == type) return;

	children = gtk_container_get_children
		(GTK_CONTAINER(mimeview->mime_vbox));
	if (children) {
		gtkut_container_remove(GTK_CONTAINER(mimeview->mime_vbox),
				       GTK_WIDGET(children->data));
		g_list_free(children);
	}

	switch (mimeview->type) {
	case MIMEVIEW_IMAGE:
		imageview_clear(mimeview->imageview);
		break;
	case MIMEVIEW_TEXT:
		textview_clear(mimeview->textview);
		break;
	default:
		break;
	}

	switch (type) {
	case MIMEVIEW_IMAGE:
		gtk_container_add(GTK_CONTAINER(mimeview->mime_vbox),
				  GTK_WIDGET_PTR(imageview));
		break;
	case MIMEVIEW_TEXT:
		gtk_container_add(GTK_CONTAINER(mimeview->mime_vbox),
				  GTK_WIDGET_PTR(textview));
		break;
	default:
		return;
	}

	mimeview->type = type;
}

static void mimeview_selection_changed(GtkTreeSelection *selection,
				       MimeView *mimeview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(mimeview->store);
	GtkTreeIter iter;
	GtkTreePath *path;
	MimeInfo *partinfo;

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		if (mimeview->opened) {
			gtk_tree_path_free(mimeview->opened);
			mimeview->opened = NULL;
		}
		return;
	}

	path = gtk_tree_model_get_path(model, &iter);

	if (mimeview->opened &&
	    gtk_tree_path_compare(mimeview->opened, path) == 0) {
		gtk_tree_path_free(path);
		return;
	}

	gtk_tree_path_free(mimeview->opened);
	mimeview->opened = path;
	path = NULL;

	gtk_tree_model_get(model, &iter, COL_MIME_INFO, &partinfo, -1);
	if (!partinfo)
		return;

	switch (partinfo->mime_type) {
	case MIME_TEXT:
	case MIME_TEXT_HTML:
	case MIME_MESSAGE_RFC822:
	case MIME_MULTIPART:
		mimeview_show_message_part(mimeview, partinfo);
		break;
	case MIME_IMAGE:
		if (prefs_common.inline_image) {
			mimeview_show_image_part(mimeview, partinfo);
			break;
		}
		/* FALLTHROUGH */
	default:
		mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
#if USE_GPGME
		if (rfc2015_is_signature_part(partinfo))
			mimeview_show_signature_part(mimeview, partinfo);
		else
#endif
			mimeview_show_mime_part(mimeview, partinfo);
		break;
	}
}

static gint mimeview_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    MimeView *mimeview)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(widget);
	MimeInfo *partinfo;

	if (!event) return FALSE;

	if (event->button == 2 || event->button == 3) {
		GtkTreePath *path;

		if (!gtk_tree_view_get_path_at_pos(treeview, event->x, event->y,
						   &path, NULL, NULL, NULL))
			return FALSE;
		gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
		gtk_tree_path_free(path);
	}

	if (event->button == 2 ||
	    (event->button == 1 && event->type == GDK_2BUTTON_PRESS)) {
		/* call external program for image, audio or html */
		mimeview_launch(mimeview);
	} else if (event->button == 3) {
		partinfo = mimeview_get_selected_part(mimeview);
		if (partinfo && (partinfo->mime_type == MIME_TEXT ||
				 partinfo->mime_type == MIME_TEXT_HTML ||
				 partinfo->mime_type == MIME_MESSAGE_RFC822 ||
				 partinfo->mime_type == MIME_IMAGE ||
				 partinfo->mime_type == MIME_MULTIPART))
			menu_set_sensitive(mimeview->popupfactory,
					   "/Display as text", FALSE);
		else
			menu_set_sensitive(mimeview->popupfactory,
					   "/Display as text", TRUE);
		if (partinfo &&
		    partinfo->mime_type == MIME_APPLICATION_OCTET_STREAM)
			menu_set_sensitive(mimeview->popupfactory,
					   "/Open", FALSE);
		else
			menu_set_sensitive(mimeview->popupfactory,
					   "/Open", TRUE);

		menu_set_sensitive(mimeview->popupfactory,
				   "/Save all...", mimeview->has_attach_file);

		if (partinfo && (partinfo->mime_type == MIME_TEXT ||
				 partinfo->mime_type == MIME_TEXT_HTML ||
				 partinfo->mime_type == MIME_MESSAGE_RFC822))
			menu_set_sensitive(mimeview->popupfactory,
					   "/Print...", TRUE);
		else
			menu_set_sensitive(mimeview->popupfactory,
					   "/Print...", FALSE);

		if (partinfo && partinfo->mime_type == MIME_MESSAGE_RFC822) {
			gtk_widget_show(mimeview->reply_separator);
			gtk_widget_show(mimeview->reply_menuitem);
		} else {
			gtk_widget_hide(mimeview->reply_separator);
			gtk_widget_hide(mimeview->reply_menuitem);
		}
#if USE_GPGME
		menu_set_sensitive(mimeview->popupfactory,
				   "/Check signature",
				   mimeview_is_signed(mimeview));
#endif

		gtk_menu_popup(GTK_MENU(mimeview->popupmenu),
			       NULL, NULL, NULL, NULL,
			       event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

void mimeview_pass_key_press_event(MimeView *mimeview, GdkEventKey *event)
{
	mimeview_key_pressed(mimeview->treeview, event, mimeview);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

#define KEY_PRESS_EVENT_STOP() \
	g_signal_stop_emission_by_name(G_OBJECT(treeview), "key_press_event");

static gint mimeview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				 MimeView *mimeview)
{
	SummaryView *summaryview = NULL;
	GtkTreeView *treeview = GTK_TREE_VIEW(widget);
	GtkTreeModel *model = GTK_TREE_MODEL(mimeview->store);
	GtkTreeIter iter;
	gboolean mod_pressed;

	if (!event) return FALSE;
	if (!mimeview->opened) return FALSE;
	if (!gtk_tree_model_get_iter(model, &iter, mimeview->opened))
		return FALSE;

	if (mimeview->messageview->mainwin)
		summaryview = mimeview->messageview->mainwin->summaryview;
	mod_pressed =
		((event->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK)) != 0);

	switch (event->keyval) {
	case GDK_space:
	case GDK_KP_Space:
		if (textview_scroll_page(mimeview->textview, mod_pressed))
			return TRUE;

		if (gtkut_tree_model_next(model, &iter)) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
			gtk_tree_path_free(path);
			return TRUE;
		}
		if (summaryview)
			summary_pass_key_press_event(summaryview, event);
		break;
	case GDK_BackSpace:
		textview_scroll_page(mimeview->textview, TRUE);
		return TRUE;
	case GDK_Return:
	case GDK_KP_Enter:
		textview_scroll_one_line(mimeview->textview, mod_pressed);
		return TRUE;
	case GDK_t:
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		mimeview_display_as_text(mimeview);
		return TRUE;
	case GDK_Escape:
		if (summaryview)
			gtk_widget_grab_focus(summaryview->treeview);
		break;
	case GDK_Left:
	case GDK_Delete:
	case GDK_KP_Left:
	case GDK_KP_Delete:
		if (summaryview)
			summary_pass_key_press_event(summaryview, event);
		break;
	default:
		break;
	}

	return FALSE;
}

static void mimeview_drag_begin(GtkWidget *widget, GdkDragContext *drag_context,
				MimeView *mimeview)
{
	gchar *filename;
	gchar *bname = NULL;
	MimeInfo *partinfo;

	if (!mimeview->opened) return;
	if (!mimeview->messageview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	if (!partinfo) return;

	filename = partinfo->filename ? partinfo->filename : partinfo->name;
	if (filename) {
		const gchar *bname_;

		bname_ = g_basename(filename);
		bname = conv_filename_from_utf8(bname_);
		subst_for_filename(bname);
	}
	if (!bname || *bname == '\0')
		filename = procmime_get_tmp_file_name_for_user(partinfo);
	else
		filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
				       bname, NULL);

	if (procmime_get_part(filename, mimeview->messageview->file, partinfo) < 0) {
		g_warning(_("Can't save the part of multipart message."));
	} else
		mimeview->drag_file = encode_uri(filename);

	g_free(filename);

	gtk_drag_set_icon_default(drag_context);
}

static void mimeview_drag_end(GtkWidget *widget, GdkDragContext *drag_context,
			      MimeView *mimeview)
{
	if (mimeview->drag_file) {
		g_free(mimeview->drag_file);
		mimeview->drag_file = NULL;
	}
}

static void mimeview_drag_data_get(GtkWidget	    *widget,
				   GdkDragContext   *drag_context,
				   GtkSelectionData *selection_data,
				   guint	     info,
				   guint	     time,
				   MimeView	    *mimeview)
{
	if (!mimeview->drag_file) return;

	gtk_selection_data_set(selection_data, selection_data->target, 8,
			       (guchar *)mimeview->drag_file,
			       strlen(mimeview->drag_file));
}

static void mimeview_display_as_text(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview->opened) return;

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(partinfo != NULL);
	mimeview_show_message_part(mimeview, partinfo);
}

void mimeview_save_as(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview->opened) return;
	if (!mimeview->messageview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(partinfo != NULL);

	mimeview_save_part_as(mimeview, partinfo);
}

void mimeview_save_all(MimeView *mimeview)
{
	gchar *dir;

	dir = filesel_select_dir(NULL);
	if (!dir) return;

	if (procmime_get_all_parts(dir, mimeview->messageview->file, mimeview->messageview->mimeinfo) < 0)
		alertpanel_error(_("Can't save the attachments."));

	g_free(dir);
}

void mimeview_print(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview->opened) return;
	if (!mimeview->messageview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(partinfo != NULL);

	mimeview_print_part(mimeview, partinfo);
}

void mimeview_print_part(MimeView *mimeview, MimeInfo *partinfo)
{
	g_return_if_fail(partinfo != NULL);

	if (!mimeview->messageview->file) return;

	if (partinfo->mime_type == MIME_MESSAGE_RFC822) {
		gchar *filename;
		MsgInfo *msginfo;
		MsgFlags flags = {0, 0};

		filename = procmime_get_tmp_file_name(partinfo);
		if (procmime_get_part(filename, mimeview->messageview->file, partinfo) < 0) {
			alertpanel_error
				(_("Can't save the part of multipart message."));
			g_free(filename);
			return;
		}

		msginfo = procheader_parse_file(filename, flags, TRUE);
		msginfo->file_path = filename;
		filename = NULL;
		printing_print_message
			(msginfo, mimeview->textview->show_all_headers);
		procmsg_msginfo_free(msginfo);
	} else if (partinfo->mime_type == MIME_TEXT ||
		   partinfo->mime_type == MIME_TEXT_HTML) {
		printing_print_message_part(mimeview->messageview->msginfo,
					    partinfo);
	}
}

void mimeview_launch_part(MimeView *mimeview, MimeInfo *partinfo)
{
	gchar *filename;

	g_return_if_fail(partinfo != NULL);

	if (!mimeview->messageview->file) return;

	filename = procmime_get_tmp_file_name_for_user(partinfo);

	if (procmime_get_part(filename, mimeview->messageview->file, partinfo) < 0)
		alertpanel_error
			(_("Can't save the part of multipart message."));
	else
		mimeview_view_file(filename, partinfo, NULL);

	g_free(filename);
}

void mimeview_open_part_with(MimeView *mimeview, MimeInfo *partinfo)
{
	gchar *filename;
	gchar *cmd;

	g_return_if_fail(partinfo != NULL);

	if (!mimeview->messageview->file) return;

	filename = procmime_get_tmp_file_name_for_user(partinfo);

	if (procmime_get_part(filename, mimeview->messageview->file, partinfo) < 0) {
		alertpanel_error
			(_("Can't save the part of multipart message."));
		g_free(filename);
		return;
	}

	if (!prefs_common.mime_open_cmd_history)
		prefs_common.mime_open_cmd_history =
			add_history(NULL, prefs_common.mime_open_cmd);

	cmd = input_dialog_combo
		(_("Open with"),
		 _("Enter the command line to open file:\n"
		   "(`%s' will be replaced with file name)"),
		 prefs_common.mime_open_cmd,
		 prefs_common.mime_open_cmd_history,
		 TRUE);
	if (cmd) {
		mimeview_view_file(filename, partinfo, cmd);
		g_free(prefs_common.mime_open_cmd);
		prefs_common.mime_open_cmd = cmd;
		prefs_common.mime_open_cmd_history =
			add_history(prefs_common.mime_open_cmd_history, cmd);
	}

	g_free(filename);
}

void mimeview_save_part_as(MimeView *mimeview, MimeInfo *partinfo)
{
	gchar *filename = NULL;

	g_return_if_fail(partinfo != NULL);

	if (!mimeview->messageview->file) return;

	if (partinfo->filename) {
		filename = filesel_save_as(partinfo->filename);
	} else if (partinfo->name) {
		gchar *defname;

		defname = g_strdup(partinfo->name);
		subst_for_filename(defname);
		filename = filesel_save_as(defname);
		g_free(defname);
	} else
		filename = filesel_save_as(NULL);

	if (!filename)
		return;

	if (procmime_get_part(filename, mimeview->messageview->file, partinfo) < 0)
		alertpanel_error
			(_("Can't save the part of multipart message."));

	g_free(filename);
}

static void mimeview_launch(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview->opened) return;
	if (!mimeview->messageview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(partinfo != NULL);

	mimeview_launch_part(mimeview, partinfo);
}

static void mimeview_open_with(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview->opened) return;
	if (!mimeview->messageview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(partinfo != NULL);

	mimeview_open_part_with(mimeview, partinfo);
}

static void mimeview_view_file(const gchar *filename, MimeInfo *partinfo,
			       const gchar *cmdline)
{
	const gchar *cmd = NULL;
	gchar buf[BUFFSIZE];

	if (!cmdline) {
#ifdef G_OS_WIN32
		DWORD dwtype;

		if (g_file_test(filename, G_FILE_TEST_IS_EXECUTABLE) ||
		    str_has_suffix_case(filename, ".scr") ||
		    str_has_suffix_case(filename, ".pif") ||
		    GetBinaryType(filename, &dwtype)) {
			alertpanel_full
				(_("Opening executable file"),
				 _("This is an executable file. Opening executable file is restricted for security.\n"
				   "If you want to launch it, save it to somewhere and make sure it is not an virus or something like a malicious program."),
				 ALERT_WARNING, G_ALERTDEFAULT, FALSE,
				 GTK_STOCK_OK, NULL, NULL);
			return;
		}
		execute_open_file(filename, partinfo->content_type);
		return;
#elif defined(__APPLE__)
		if (g_file_test(filename, G_FILE_TEST_IS_EXECUTABLE) ||
		    str_has_suffix_case(filename, ".py") ||
		    str_has_suffix_case(filename, ".rb") ||
		    str_has_suffix_case(filename, ".sh")) {
			alertpanel_full
				(_("Opening executable file"),
				 _("This is an executable file. Opening executable file is restricted for security.\n"
				   "If you want to launch it, save it to somewhere and make sure it is not an virus or something like a malicious program."),
				 ALERT_WARNING, G_ALERTDEFAULT, FALSE,
				 GTK_STOCK_OK, NULL, NULL);
			return;
		}
		execute_open_file(filename, partinfo->content_type);
		return;
#else
		if (MIME_IMAGE == partinfo->mime_type)
			cmd = prefs_common.mime_image_viewer;
		else if (MIME_AUDIO == partinfo->mime_type)
			cmd = prefs_common.mime_audio_player;
		else if (MIME_TEXT_HTML == partinfo->mime_type)
			cmd = prefs_common.uri_cmd;
		if (!cmd) {
			if (prefs_common.mime_cmd) {
				if (str_find_format_times
					(prefs_common.mime_cmd, 's') == 2) {
					g_snprintf(buf, sizeof(buf),
						   prefs_common.mime_cmd,
						   partinfo->content_type,
						   "%s");
					cmd = buf;
				} else
					cmd = prefs_common.mime_cmd;
			} else {
				procmime_execute_open_file
					(filename, partinfo->content_type);
				return;
			}
		}
#endif
	} else
		cmd = cmdline;

	if (cmd && str_find_format_times(cmd, 's') == 1) {
		gchar *cmdbuf;
		cmdbuf = g_strdup_printf(cmd, filename);
		execute_command_line(cmdbuf, TRUE);
		g_free(cmdbuf);
	} else if (cmd)
		g_warning("MIME viewer command line is invalid: '%s'", cmd);
}

static void mimeview_reply(MimeView *mimeview, guint action)
{
	MimeInfo *partinfo;
	gchar *filename;
	MsgInfo *msginfo;
	MsgFlags flags = {0, 0};
	ComposeMode mode = action;

	if (!mimeview->opened) return;
	if (!mimeview->messageview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(partinfo != NULL);

	if (partinfo->mime_type != MIME_MESSAGE_RFC822)
		return;

	filename = procmime_get_tmp_file_name(partinfo);
	if (procmime_get_part(filename, mimeview->messageview->file, partinfo) < 0) {
		alertpanel_error
			(_("Can't save the part of multipart message."));
			g_free(filename);
			return;
	}

	msginfo = procheader_parse_file(filename, flags, TRUE);
	msginfo->file_path = filename;
	filename = NULL;
	if (prefs_common.reply_with_quote)
		mode |= COMPOSE_WITH_QUOTE;

	if (mimeview->messageview->mainwin)
		compose_reply(msginfo, mimeview->messageview->mainwin->summaryview->folder_item,
			      mode, NULL);
	else
		compose_reply(msginfo, NULL, mode, NULL);

	g_unlink(msginfo->file_path);
	procmsg_msginfo_free(msginfo);
}

#if USE_GPGME
static gboolean update_node_name_func(GtkTreeModel *model, GtkTreePath *path,
				      GtkTreeIter *iter, gpointer data)
{
	MimeInfo *partinfo;
	gchar *part_name;

	gtk_tree_model_get(model, iter, COL_MIME_INFO, &partinfo, -1);
	g_return_val_if_fail(partinfo != NULL, FALSE);

	part_name = get_part_name(partinfo);
	gtk_tree_store_set(GTK_TREE_STORE(model), iter, COL_NAME, part_name,
			   -1);

	return FALSE;
}

static void mimeview_update_names(MimeView *mimeview)
{
	gtk_tree_model_foreach(GTK_TREE_MODEL(mimeview->store),
			       update_node_name_func, NULL);
}

static void mimeview_update_signature_info(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview) return;
	if (!mimeview->opened) return;

	partinfo = mimeview_get_selected_part(mimeview);
	if (!partinfo) return;

	if (rfc2015_is_signature_part(partinfo)) {
		mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
		mimeview_show_signature_part(mimeview, partinfo);
	}
}

static void mimeview_check_signature(MimeView *mimeview)
{
	MimeInfo *mimeinfo;
	FILE *fp;

	g_return_if_fail (mimeview_is_signed(mimeview));

	if (!rfc2015_is_available())
		return;

	mimeinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(mimeview->messageview->file != NULL);

	while (mimeinfo->parent)
		mimeinfo = mimeinfo->parent;

	if ((fp = g_fopen(mimeview->messageview->file, "rb")) == NULL) {
		FILE_OP_ERROR(mimeview->messageview->file, "fopen");
		return;
	}

	rfc2015_check_signature(mimeinfo, fp);
	fclose(fp);

	mimeview_update_names(mimeview);
	mimeview_update_signature_info(mimeview);

	textview_show_message(mimeview->messageview->textview, mimeinfo,
			      mimeview->messageview->file);
}
#endif /* USE_GPGME */
