/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
#include <gtk/gtknotebook.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkselection.h>
#include <stdio.h>
#include <unistd.h>

#include "main.h"
#include "mimeview.h"
#include "textview.h"
#include "imageview.h"
#include "procmime.h"
#include "summaryview.h"
#include "menu.h"
#include "filesel.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "rfc2015.h"

typedef enum
{
	COL_MIMETYPE = 0,
	COL_SIZE     = 1,
	COL_NAME     = 2
} MimeViewColumnPos;

#define N_MIMEVIEW_COLS	3

static void mimeview_set_multipart_tree		(MimeView	*mimeview,
						 MimeInfo	*mimeinfo,
						 GtkCTreeNode	*parent);
static GtkCTreeNode *mimeview_append_part	(MimeView	*mimeview,
						 MimeInfo	*partinfo,
						 GtkCTreeNode	*parent);
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

static void mimeview_selected		(GtkCTree	*ctree,
					 GtkCTreeNode	*node,
					 gint		 column,
					 MimeView	*mimeview);
static void mimeview_start_drag 	(GtkWidget	*widget,
					 gint		 button,
					 GdkEvent	*event,
					 MimeView	*mimeview);
static gint mimeview_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 MimeView	*mimeview);
static gint mimeview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 MimeView	*mimeview);

static void mimeview_drag_data_get      (GtkWidget	  *widget,
					 GdkDragContext   *drag_context,
					 GtkSelectionData *selection_data,
					 guint		   info,
					 guint		   time,
					 MimeView	  *mimeview);

static void mimeview_display_as_text	(MimeView	*mimeview);
static void mimeview_save_as		(MimeView	*mimeview);
static void mimeview_launch		(MimeView	*mimeview);
static void mimeview_open_with		(MimeView	*mimeview);
static void mimeview_view_file		(const gchar	*filename,
					 MimeInfo	*partinfo,
					 const gchar	*cmdline);
#if USE_GPGME
static void mimeview_check_signature	(MimeView	*mimeview);
#endif

static GtkItemFactoryEntry mimeview_popup_entries[] =
{
	{N_("/_Open"),		  NULL, mimeview_launch,	  0, NULL},
	{N_("/Open _with..."),	  NULL, mimeview_open_with,	  0, NULL},
	{N_("/_Display as text"), NULL, mimeview_display_as_text, 0, NULL},
	{N_("/_Save as..."),	  NULL, mimeview_save_as,	  0, NULL}
#if USE_GPGME
        ,
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

	GtkWidget *notebook;
	GtkWidget *vbox;
	GtkWidget *paned;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *mime_vbox;
	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	gchar *titles[N_MIMEVIEW_COLS];
	gint n_entries;
	gint i;

	debug_print(_("Creating MIME view...\n"));
	mimeview = g_new0(MimeView, 1);

	titles[COL_MIMETYPE] = _("MIME Type");
	titles[COL_SIZE]     = _("Size");
	titles[COL_NAME]     = _("Name");

	notebook = gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(notebook), vbox);
	gtk_notebook_set_tab_label_text(GTK_NOTEBOOK(notebook), vbox,
					_("Text"));

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_widget_set_size_request(scrolledwin, -1, 80);

	ctree = gtk_sctree_new_with_titles(N_MIMEVIEW_COLS, 0, titles);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_NONE);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), COL_SIZE,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_MIMETYPE, 240);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_SIZE, 64);
	for (i = 0; i < N_MIMEVIEW_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[i].button,
				       GTK_CAN_FOCUS);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);

	g_signal_connect(G_OBJECT(ctree), "tree_select_row",
			 G_CALLBACK(mimeview_selected), mimeview);
	g_signal_connect(G_OBJECT(ctree), "button_press_event",
			 G_CALLBACK(mimeview_button_pressed), mimeview);
	g_signal_connect(G_OBJECT(ctree), "key_press_event",
			 G_CALLBACK(mimeview_key_pressed), mimeview);
	g_signal_connect(G_OBJECT (ctree),"start_drag",
			 G_CALLBACK (mimeview_start_drag), mimeview);
	g_signal_connect(G_OBJECT(ctree), "drag_data_get",
			 G_CALLBACK(mimeview_drag_data_get), mimeview);
    
	mime_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_reallocate_redraws(GTK_CONTAINER(mime_vbox), TRUE);

	paned = gtk_vpaned_new();
	gtk_paned_add1(GTK_PANED(paned), scrolledwin);
	gtk_paned_add2(GTK_PANED(paned), mime_vbox);
	gtk_container_add(GTK_CONTAINER(notebook), paned);
	gtk_notebook_set_tab_label_text(GTK_NOTEBOOK(notebook), paned,
					_("Attachments"));

	gtk_widget_show_all(notebook);

	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);

	n_entries = sizeof(mimeview_popup_entries) /
		sizeof(mimeview_popup_entries[0]);
	popupmenu = menu_create_items(mimeview_popup_entries, n_entries,
				      "<MimeView>", &popupfactory, mimeview);

	mimeview->notebook     = notebook;
	mimeview->vbox         = vbox;
	mimeview->paned        = paned;
	mimeview->scrolledwin  = scrolledwin;
	mimeview->ctree        = ctree;
	mimeview->mime_vbox    = mime_vbox;
	mimeview->popupmenu    = popupmenu;
	mimeview->popupfactory = popupfactory;
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

        debug_print("mimeview_is_signed: open\n" );

	if (!mimeview->file) return FALSE;

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
	GtkCTree *ctree = GTK_CTREE(mimeview->ctree);
	GtkCTreeNode *node;

	mimeview_clear(mimeview);
	textview_clear(mimeview->messageview->textview);

	g_return_if_fail(file != NULL);
	g_return_if_fail(mimeinfo != NULL);

	mimeview->mimeinfo = mimeinfo;

	mimeview->file = g_strdup(file);

#if USE_GPGME
	if (prefs_common.auto_check_signatures) {
		FILE *fp;

		if ((fp = fopen(file, "rb")) == NULL) {
			FILE_OP_ERROR(file, "fopen");
			return;
		}
		rfc2015_check_signature(mimeinfo, fp);
		fclose(fp);
	} else
		set_unchecked_signature(mimeinfo);
#endif

	g_signal_handlers_block_by_func
		(G_OBJECT(ctree), G_CALLBACK(mimeview_selected), mimeview);

	mimeview_set_multipart_tree(mimeview, mimeinfo, NULL);

	g_signal_handlers_unblock_by_func
		(G_OBJECT(ctree), G_CALLBACK(mimeview_selected), mimeview);

	/* search first text part */
	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = GTK_CTREE_NODE_NEXT(node)) {
		MimeInfo *partinfo;

		partinfo = gtk_ctree_node_get_row_data(ctree, node);
		if (partinfo &&
		    (partinfo->mime_type == MIME_TEXT ||
		     partinfo->mime_type == MIME_TEXT_HTML))
			break;
	}
	textview_show_message(mimeview->messageview->textview, mimeinfo, file);

	if (!node)
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	if (node) {
		gtk_ctree_select(ctree, node);
		gtkut_ctree_set_focus_row(ctree, node);
		gtk_widget_grab_focus(mimeview->ctree);
	}
}

void mimeview_clear(MimeView *mimeview)
{
	GtkCList *clist = GTK_CLIST(mimeview->ctree);

	procmime_mimeinfo_free_all(mimeview->mimeinfo);
	mimeview->mimeinfo = NULL;

	gtk_clist_clear(clist);
	textview_clear(mimeview->textview);
	imageview_clear(mimeview->imageview);

	mimeview->opened = NULL;

	g_free(mimeview->file);
	mimeview->file = NULL;

	/* gtk_notebook_set_page(GTK_NOTEBOOK(mimeview->notebook), 0); */
}

void mimeview_destroy(MimeView *mimeview)
{
	textview_destroy(mimeview->textview);
	imageview_destroy(mimeview->imageview);
	procmime_mimeinfo_free_all(mimeview->mimeinfo);
	g_free(mimeview->file);
	g_free(mimeview);
}

MimeInfo *mimeview_get_selected_part(MimeView *mimeview)
{
	if (gtk_notebook_get_current_page
		(GTK_NOTEBOOK(mimeview->notebook)) == 0)
		return NULL;

	return gtk_ctree_node_get_row_data
		(GTK_CTREE(mimeview->ctree), mimeview->opened);
}

gboolean mimeview_step(MimeView *mimeview, GtkScrollType type)
{
	GtkCTree *ctree = GTK_CTREE(mimeview->ctree);
	GtkCTreeNode *node;

	if (type == GTK_SCROLL_STEP_FORWARD) {
		node = gtkut_ctree_node_next(ctree, mimeview->opened);
		if (node)
			gtkut_ctree_expand_parent_all(ctree, node);
		else
			return FALSE;
	} else {
		if (mimeview->opened) {
			node = GTK_CTREE_NODE_PREV(mimeview->opened);
			if (!node) return FALSE;
		} else
			return FALSE;
	}

	g_signal_emit_by_name(G_OBJECT(ctree), "scroll_vertical", type, 0.0);

	return TRUE;
}

static void mimeview_set_multipart_tree(MimeView *mimeview,
					MimeInfo *mimeinfo,
					GtkCTreeNode *parent)
{
	GtkCTreeNode *node;

	g_return_if_fail(mimeinfo != NULL);

	if (mimeinfo->children)
		mimeinfo = mimeinfo->children;

	while (mimeinfo != NULL) {
		node = mimeview_append_part(mimeview, mimeinfo, parent);

		if (mimeinfo->children)
			mimeview_set_multipart_tree(mimeview, mimeinfo, node);
		else if (mimeinfo->sub &&
			 mimeinfo->sub->mime_type != MIME_TEXT &&
			 mimeinfo->sub->mime_type != MIME_TEXT_HTML)
			mimeview_set_multipart_tree(mimeview, mimeinfo->sub,
						    node);
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

static GtkCTreeNode *mimeview_append_part(MimeView *mimeview,
					  MimeInfo *partinfo,
					  GtkCTreeNode *parent)
{
	GtkCTree *ctree = GTK_CTREE(mimeview->ctree);
	GtkCTreeNode *node;
	gchar *str[N_MIMEVIEW_COLS];

	str[COL_MIMETYPE] =
		partinfo->content_type ? partinfo->content_type : "";
	str[COL_SIZE] = to_human_readable(partinfo->size);
	str[COL_NAME] = get_part_name(partinfo);

	node = gtk_ctree_insert_node(ctree, parent, NULL, str, 0,
				     NULL, NULL, NULL, NULL,
				     FALSE, TRUE);
	gtk_ctree_node_set_row_data(ctree, node, partinfo);

	return node;
}

static void mimeview_show_message_part(MimeView *mimeview, MimeInfo *partinfo)
{
	FILE *fp;
	const gchar *fname;
#if USE_GPGME
	MimeInfo *pi;
#endif

	if (!partinfo) return;

#if USE_GPGME
	for (pi = partinfo; pi && !pi->plaintextfile ; pi = pi->parent)
		;
	fname = pi ? pi->plaintextfile : mimeview->file;
#else
	fname = mimeview->file;
#endif /* USE_GPGME */
	if (!fname) return;

	if ((fp = fopen(fname, "rb")) == NULL) {
		FILE_OP_ERROR(fname, "fopen");
		return;
	}

	if (fseek(fp, partinfo->fpos, SEEK_SET) < 0) {
		FILE_OP_ERROR(mimeview->file, "fseek");
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

	if (procmime_get_part(filename, mimeview->file, partinfo) < 0)
		alertpanel_error
			(_("Can't get the part of multipart message."));
	else {
		mimeview_change_view_type(mimeview, MIMEVIEW_IMAGE);
		imageview_show_image(mimeview->imageview, partinfo, filename,
				     prefs_common.resize_image);
		unlink(filename);
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
		g_snprintf(buf, sizeof(buf), "[%s  %s (%d bytes)]\n\n",
			   partinfo->filename ? partinfo->filename :
			   partinfo->name,
			   partinfo->content_type, partinfo->size);
	else
		g_snprintf(buf, sizeof(buf), "[%s (%d bytes)]\n\n",
			   partinfo->content_type, partinfo->size);
	gtk_text_buffer_insert(buffer, &iter, buf, -1);

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

	button = gtk_button_new_with_mnemonic(_("_Check signature"));
	gtk_container_add(GTK_CONTAINER(vbbox), button);
	g_signal_connect(button, "clicked",
			 G_CALLBACK(check_signature_button_clicked), mimeview);

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

static void mimeview_selected(GtkCTree *ctree, GtkCTreeNode *node, gint column,
			      MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (mimeview->opened == node) return;
	mimeview->opened = node;
	gtk_ctree_node_moveto(ctree, node, -1, 0.5, 0);

	partinfo = gtk_ctree_node_get_row_data(ctree, node);
	if (!partinfo) return;

	/* ungrab the mouse event */
	if (GTK_WIDGET_HAS_GRAB(ctree)) {
		gtk_grab_remove(GTK_WIDGET(ctree));
		if (gdk_pointer_is_grabbed())
			gdk_pointer_ungrab(GDK_CURRENT_TIME);
	}

	switch (partinfo->mime_type) {
	case MIME_TEXT:
	case MIME_TEXT_HTML:
	case MIME_MESSAGE_RFC822:
	case MIME_MULTIPART:
		mimeview_show_message_part(mimeview, partinfo);
		break;
#if (HAVE_GDK_PIXBUF || HAVE_GDK_IMLIB)
	case MIME_IMAGE:
		mimeview_show_image_part(mimeview, partinfo);
		break;
#endif
	default:
		mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
#if USE_GPGME
		if (g_strcasecmp(partinfo->content_type,
				 "application/pgp-signature") == 0)
			mimeview_show_signature_part(mimeview, partinfo);
		else
#endif
			mimeview_show_mime_part(mimeview, partinfo);
		break;
	}
}

static void mimeview_start_drag(GtkWidget *widget, gint button,
				GdkEvent *event, MimeView *mimeview)
{
	GtkTargetList *list;
	GdkDragContext *context;
	MimeInfo *partinfo;

	g_return_if_fail(mimeview != NULL);

	partinfo = mimeview_get_selected_part(mimeview);
	if (partinfo->filename == NULL && partinfo->name == NULL) return;

	list = gtk_target_list_new(mimeview_mime_types, 1);
	context = gtk_drag_begin(widget, list,
				 GDK_ACTION_COPY, button, event);
	gtk_drag_set_icon_default(context);
}

static gint mimeview_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    MimeView *mimeview)
{
	GtkCList *clist = GTK_CLIST(widget);
	MimeInfo *partinfo;
	gint row, column;

	if (!event) return FALSE;

	if (event->button == 2 || event->button == 3) {
		if (!gtk_clist_get_selection_info(clist, event->x, event->y,
						  &row, &column))
			return FALSE;
		gtk_clist_unselect_all(clist);
		gtk_clist_select_row(clist, row, column);
		gtkut_clist_set_focus_row(clist, row);
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
	mimeview_key_pressed(mimeview->ctree, event, mimeview);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

#define KEY_PRESS_EVENT_STOP() \
	g_signal_stop_emission_by_name(G_OBJECT(ctree), "key_press_event");

static gint mimeview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				 MimeView *mimeview)
{
	SummaryView *summaryview = NULL;
	GtkCTree *ctree = GTK_CTREE(widget);
	GtkCTreeNode *node;
	gboolean mod_pressed;

	if (!event) return FALSE;
	if (!mimeview->opened) return FALSE;

	if (mimeview->messageview->mainwin)
		summaryview = mimeview->messageview->mainwin->summaryview;
	mod_pressed =
		((event->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK)) != 0);

	switch (event->keyval) {
	case GDK_space:
		if (textview_scroll_page(mimeview->textview, mod_pressed))
			return TRUE;

		node = GTK_CTREE_NODE_NEXT(mimeview->opened);
		if (node) {
			gtk_sctree_unselect_all(GTK_SCTREE(ctree));
			gtk_sctree_select(GTK_SCTREE(ctree), node);
			return TRUE;
		}
		if (summaryview)
			summary_pass_key_press_event(summaryview, event);
		break;
	case GDK_BackSpace:
		textview_scroll_page(mimeview->textview, TRUE);
		return TRUE;
	case GDK_Return:
		textview_scroll_one_line(mimeview->textview, mod_pressed);
		return TRUE;
	case GDK_t:
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		mimeview_display_as_text(mimeview);
		return TRUE;
	case GDK_Left:
	case GDK_Escape:
	case GDK_Delete:
		if (summaryview)
			summary_pass_key_press_event(summaryview, event);
		break;
	default:
		break;
	}

	return FALSE;
}

static void mimeview_drag_data_get(GtkWidget	    *widget,
				   GdkDragContext   *drag_context,
				   GtkSelectionData *selection_data,
				   guint	     info,
				   guint	     time,
				   MimeView	    *mimeview)
{
	gchar *filename, *uriname;
	const gchar *bname;
	MimeInfo *partinfo;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	if (!partinfo) return;
	if (!partinfo->filename && !partinfo->name) return;

	filename = partinfo->filename ? partinfo->filename : partinfo->name;
	bname = g_basename(filename);
	if (*bname == '\0') return;

	filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
			       bname, NULL);

	if (procmime_get_part(filename, mimeview->file, partinfo) < 0)
		alertpanel_error
			(_("Can't save the part of multipart message."));

	uriname = g_strconcat("file://", filename, NULL);
	gtk_selection_data_set(selection_data, selection_data->target, 8,
			       uriname, strlen(uriname));

	g_free(uriname);
	g_free(filename);
}

static void mimeview_display_as_text(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview->opened) return;

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(partinfo != NULL);
	mimeview_show_message_part(mimeview, partinfo);
}

static void mimeview_save_as(MimeView *mimeview)
{
	gchar *filename;
	gchar *defname = NULL;
	MimeInfo *partinfo;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(partinfo != NULL);

	if (partinfo->filename)
		defname = partinfo->filename;
	else if (partinfo->name) {
		Xstrdup_a(defname, partinfo->name, return);
		subst_for_filename(defname);
	}

	filename = filesel_save_as(defname);
	if (!filename) return;

	if (procmime_get_part(filename, mimeview->file, partinfo) < 0)
		alertpanel_error
			(_("Can't save the part of multipart message."));

	g_free(filename);
}

static void mimeview_launch(MimeView *mimeview)
{
	MimeInfo *partinfo;
	gchar *filename;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(partinfo != NULL);

	filename = procmime_get_tmp_file_name(partinfo);

	if (procmime_get_part(filename, mimeview->file, partinfo) < 0)
		alertpanel_error
			(_("Can't save the part of multipart message."));
	else
		mimeview_view_file(filename, partinfo, NULL);

	g_free(filename);
}

static void mimeview_open_with(MimeView *mimeview)
{
	MimeInfo *partinfo;
	gchar *filename;
	gchar *cmd;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(partinfo != NULL);

	filename = procmime_get_tmp_file_name(partinfo);

	if (procmime_get_part(filename, mimeview->file, partinfo) < 0) {
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

static void mimeview_view_file(const gchar *filename, MimeInfo *partinfo,
			       const gchar *cmdline)
{
	static gchar *default_image_cmdline = "display '%s'";
	static gchar *default_audio_cmdline = "play '%s'";
	static gchar *default_html_cmdline = DEFAULT_BROWSER_CMD;
	static gchar *mime_cmdline = "metamail -d -b -x -c %s '%s'";
	gchar buf[1024];
	gchar m_buf[1024];
	const gchar *cmd;
	const gchar *def_cmd;
	const gchar *p;

	if (cmdline) {
		cmd = cmdline;
		def_cmd = NULL;
	} else if (MIME_APPLICATION_OCTET_STREAM == partinfo->mime_type) {
		return;
	} else if (MIME_IMAGE == partinfo->mime_type) {
		cmd = prefs_common.mime_image_viewer;
		def_cmd = default_image_cmdline;
	} else if (MIME_AUDIO == partinfo->mime_type) {
		cmd = prefs_common.mime_audio_player;
		def_cmd = default_audio_cmdline;
	} else if (MIME_TEXT_HTML == partinfo->mime_type) {
		cmd = prefs_common.uri_cmd;
		def_cmd = default_html_cmdline;
	} else {
		g_snprintf(m_buf, sizeof(m_buf), mime_cmdline,
			   partinfo->content_type, "%s");
		cmd = m_buf;
		def_cmd = NULL;
	}

	if (cmd && (p = strchr(cmd, '%')) && *(p + 1) == 's' &&
	    !strchr(p + 2, '%'))
		g_snprintf(buf, sizeof(buf), cmd, filename);
	else {
		if (cmd)
			g_warning(_("MIME viewer command line is invalid: `%s'"), cmd);
		if (def_cmd)
			g_snprintf(buf, sizeof(buf), def_cmd, filename);
		else
			return;
	}

	execute_command_line(buf, TRUE);
}

#if USE_GPGME
static void update_node_name(GtkCTree *ctree, GtkCTreeNode *node,
			     gpointer data)
{
	MimeInfo *partinfo;
	gchar *part_name;

	partinfo = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(partinfo != NULL);

	part_name = get_part_name(partinfo);
	gtk_ctree_node_set_text(ctree, node, COL_NAME, part_name);
}

static void mimeview_update_names(MimeView *mimeview)
{
	GtkCTree *ctree = GTK_CTREE(mimeview->ctree);

	gtk_ctree_pre_recursive(ctree, NULL, update_node_name, NULL);
}

static void mimeview_update_signature_info(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview) return;
	if (!mimeview->opened) return;

	partinfo = mimeview_get_selected_part(mimeview);
	if (!partinfo) return;

	if (g_strcasecmp(partinfo->content_type,
			 "application/pgp-signature") == 0) {
		mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
		mimeview_show_signature_part(mimeview, partinfo);
	}
}

static void mimeview_check_signature(MimeView *mimeview)
{
	MimeInfo *mimeinfo;
	FILE *fp;

	g_return_if_fail (mimeview_is_signed(mimeview));

	mimeinfo = mimeview_get_selected_part(mimeview);
	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(mimeview->file != NULL);

	while (mimeinfo->parent)
		mimeinfo = mimeinfo->parent;

	if ((fp = fopen(mimeview->file, "rb")) == NULL) {
		FILE_OP_ERROR(mimeview->file, "fopen");
		return;
	}

	rfc2015_check_signature(mimeinfo, fp);
	fclose(fp);

	mimeview_update_names(mimeview);
	mimeview_update_signature_info(mimeview);

	textview_show_message(mimeview->messageview->textview, mimeinfo,
			      mimeview->file);
}
#endif /* USE_GPGME */
