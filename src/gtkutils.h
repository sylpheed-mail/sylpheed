/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto
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

#ifndef __GTKUTILS_H__
#define __GTKUTILS_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkctree.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkstock.h>

#include <stdlib.h>

typedef struct _ComboButton	ComboButton;

struct _ComboButton
{
	GtkWidget *arrow;
	GtkWidget *button;
	GtkWidget *menu;
	GtkItemFactory *factory;
	gpointer data;
};

#define GTK_EVENTS_FLUSH() \
{ \
	while (gtk_events_pending()) \
		gtk_main_iteration(); \
}

#define PIXMAP_CREATE(widget, pixmap, mask, xpm_d) \
{ \
	if (!pixmap) { \
		GtkStyle *style = gtk_widget_get_style(widget); \
		pixmap = gdk_pixmap_create_from_xpm_d \
			(widget->window, &mask, \
			 &style->bg[GTK_STATE_NORMAL], xpm_d); \
	} \
}

#define GTK_WIDGET_PTR(wid)	(*(GtkWidget **)wid)

#define GTKUT_CTREE_NODE_SET_ROW_DATA(node, d) \
{ \
	GTK_CTREE_ROW(node)->row.data = d; \
}

#define GTKUT_CTREE_NODE_GET_ROW_DATA(node) \
	(GTK_CTREE_ROW(node)->row.data)

#define GTKUT_CTREE_REFRESH(clist) \
	GTK_CLIST_GET_CLASS(clist)->refresh(clist)

gboolean gtkut_get_str_size		(GtkWidget	*widget,
					 const gchar	*str,
					 gint		*width,
					 gint		*height);
gboolean gtkut_get_font_size		(GtkWidget	*widget,
					 gint		*width,
					 gint		*height);
PangoFontDescription *gtkut_get_default_font_desc
					(void);
void gtkut_widget_set_small_font_size	(GtkWidget	*widget);

gboolean gtkut_font_can_load		(const gchar	*str);

gdouble gtkut_get_dpi			(void);
gdouble gtkut_get_dpi_multiplier	(void);
void gtkut_set_dpi			(gdouble	 dpi);

void gtkut_convert_int_to_gdk_color	(gint		 rgbvalue,
					 GdkColor	*color);

void gtkut_stock_button_set_set_reverse	(gboolean	 reverse);
void gtkut_stock_button_set_create	(GtkWidget	**bbox,
					 GtkWidget	**button1,
					 const gchar	 *label1,
					 GtkWidget	**button2,
					 const gchar	 *label2,
					 GtkWidget	**button3,
					 const gchar	 *label3);

void gtkut_box_set_reverse_order	(GtkBox		*box,
					 gboolean	 reverse);

ComboButton *gtkut_combo_button_create	(GtkWidget		*button,
					 GtkItemFactoryEntry	*entries,
					 gint			 n_entries,
					 const gchar		*path,
					 gpointer		 data);

/* CTree functions */

gint gtkut_ctree_get_nth_from_node	(GtkCTree	*ctree,
					 GtkCTreeNode	*node);
void gtkut_ctree_set_focus_row		(GtkCTree	*ctree,
					 GtkCTreeNode	*node);
void gtkut_clist_set_focus_row		(GtkCList	*clist,
					 gint		 row);
void gtkut_clist_set_redraw		(GtkCList	*clist);

/* TreeView functions */

gboolean gtkut_tree_model_next		(GtkTreeModel	*model,
					 GtkTreeIter	*iter);
gboolean gtkut_tree_model_prev		(GtkTreeModel	*model,
					 GtkTreeIter	*iter);

gboolean gtkut_tree_model_get_iter_last	(GtkTreeModel	*model,
					 GtkTreeIter	*iter);

gboolean gtkut_tree_model_find_by_column_data
					(GtkTreeModel	*model,
					 GtkTreeIter	*iter,
					 GtkTreeIter	*start,
					 gint		 col,
					 gpointer	 data);

void gtkut_tree_model_foreach		(GtkTreeModel		*model,
					 GtkTreeIter		*start,
					 GtkTreeModelForeachFunc func,
					 gpointer		 user_data);

gboolean gtkut_tree_row_reference_get_iter
					(GtkTreeModel		*model,
					 GtkTreeRowReference	*ref,
					 GtkTreeIter		*iter);
gboolean gtkut_tree_row_reference_equal	(GtkTreeRowReference	*ref1,
					 GtkTreeRowReference	*ref2);

void gtkut_tree_sortable_unset_sort_column_id
					(GtkTreeSortable	*sortable);

gboolean gtkut_tree_view_find_collapsed_parent
					(GtkTreeView	*treeview,
					 GtkTreeIter	*parent,
					 GtkTreeIter	*iter);
void gtkut_tree_view_expand_parent_all	(GtkTreeView	*treeview,
					 GtkTreeIter	*iter);

void gtkut_tree_view_vertical_autoscroll(GtkTreeView	*treeview);

void gtkut_tree_view_scroll_to_cell	(GtkTreeView	*treeview,
					 GtkTreePath	*path,
					 gboolean	 align_center);

void gtkut_tree_view_fast_clear		(GtkTreeView	*treeview,
					 GtkTreeStore	*store);

void gtkut_combo_set_items		(GtkCombo	*combo,
					 const gchar	*str1, ...);

gchar *gtkut_editable_get_selection	(GtkEditable	*editable);
void gtkut_editable_disable_im		(GtkEditable	*editable);

void gtkut_entry_strip_text		(GtkEntry	*entry);

void gtkut_container_remove		(GtkContainer	*container,
					 GtkWidget	*widget);

void gtkut_scrolled_window_reset_position
					(GtkScrolledWindow	*window);

/* TextView functions */

gboolean gtkut_text_buffer_match_string	(GtkTextBuffer		*buffer,
					 const GtkTextIter	*iter,
					 gunichar		*wcs,
					 gint			 len,
					 gboolean		 case_sens);
gboolean gtkut_text_buffer_find		(GtkTextBuffer		*buffer,
					 const GtkTextIter	*iter,
					 const gchar		*str,
					 gboolean		 case_sens,
					 GtkTextIter		*match_pos);
gboolean gtkut_text_buffer_find_backward(GtkTextBuffer		*buffer,
					 const GtkTextIter	*iter,
					 const gchar		*str,
					 gboolean		 case_sens,
					 GtkTextIter		*match_pos);

void gtkut_text_buffer_insert_with_tag_by_name
					(GtkTextBuffer		*buffer,
					 GtkTextIter		*iter,
					 const gchar		*text,
					 gint			 len,
					 const gchar		*tag);

gchar *gtkut_text_view_get_selection	(GtkTextView	*textview);

void gtkut_window_popup			(GtkWidget	*window);
gboolean gtkut_window_modal_exist	(void);

void gtkut_window_move			(GtkWindow	*window,
					 gint		 x,
					 gint		 y);

void gtkut_widget_get_uposition		(GtkWidget	*widget,
					 gint		*px,
					 gint		*py);
void gtkut_widget_draw_now		(GtkWidget	*widget);
void gtkut_widget_init			(void);

void gtkut_events_flush			(void);

#endif /* __GTKUTILS_H__ */
