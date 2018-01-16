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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef G_OS_WIN32
#  include <pango/pangowin32.h>
#endif

#include "gtkutils.h"
#include "utils.h"
#include "codeconv.h"
#include "menu.h"

gboolean gtkut_get_str_size(GtkWidget *widget, const gchar *str,
			    gint *width, gint *height)
{
	PangoLayout *layout;

	g_return_val_if_fail(GTK_IS_WIDGET(widget), FALSE);

	layout = gtk_widget_create_pango_layout(widget, str);
	g_return_val_if_fail(layout, FALSE);
	pango_layout_get_pixel_size(layout, width, height);
	g_object_unref(layout);

	return TRUE;
}

gboolean gtkut_get_font_size(GtkWidget *widget, gint *width, gint *height)
{
	const gchar *str = "Abcdef";
	gboolean ret;

	ret = gtkut_get_str_size(widget, str, width, height);
	if (ret && width)
		*width = *width / g_utf8_strlen(str, -1);

	return ret;
}

PangoFontDescription *gtkut_get_default_font_desc(void)
{
	static PangoFontDescription *font_desc = NULL;

	if (!font_desc) {
		GtkWidget *window;

		window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_widget_ensure_style(window);
		font_desc = pango_font_description_copy
			(window->style->font_desc);
		gtk_object_sink(GTK_OBJECT(window));
	}

	return pango_font_description_copy(font_desc);
}

void gtkut_widget_set_small_font_size(GtkWidget *widget)
{
	PangoFontDescription *font_desc;
	gint size;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(widget->style != NULL);

	font_desc = gtkut_get_default_font_desc();
	size = pango_font_description_get_size(font_desc);
	pango_font_description_set_size(font_desc, size * PANGO_SCALE_SMALL);
	gtk_widget_modify_font(widget, font_desc);
	pango_font_description_free(font_desc);
}

gboolean gtkut_font_can_load(const gchar *str)
{
#ifdef G_OS_WIN32
	PangoFontDescription *desc;
	PangoContext *context;
	PangoFont *font;
	gboolean can_load = FALSE;

	desc = pango_font_description_from_string(str);
	if (desc) {
		context = pango_win32_get_context();
		font = pango_context_load_font(context, desc);
		if (font) {
			can_load = TRUE;
			g_object_unref(font);
		}
		g_object_unref(context);
		pango_font_description_free(desc);
	}

	return can_load;
#elif defined(__APPLE__)
	PangoFontDescription *desc;
	PangoContext *context;
	PangoFont *font;
	gboolean can_load = FALSE;

	desc = pango_font_description_from_string(str);
	if (desc) {
		context = gdk_pango_context_get_for_screen
			(gdk_screen_get_default());
		font = pango_context_load_font(context, desc);
		if (font) {
			can_load = TRUE;
			g_object_unref(font);
		}
		g_object_unref(context);
		pango_font_description_free(desc);
	}

	return can_load;
#else
	return FALSE;
#endif
}

static gdouble system_dpi = 0.0;

gdouble gtkut_get_dpi(void)
{
	gdouble dp, di;

	if (system_dpi > 0.0)
		return system_dpi;

	//dpi = gdk_screen_get_resolution(gdk_screen_get_default());

	dp = gdk_screen_get_height(gdk_screen_get_default());
	di = gdk_screen_get_height_mm(gdk_screen_get_default()) / 25.4;
	system_dpi = dp / di;

	debug_print("gtkut_get_dpi: dpi: %f\n", system_dpi);
	return system_dpi;
}

gdouble gtkut_get_dpi_multiplier(void)
{
	gdouble dpi;
	gdouble mul;

	dpi = gtkut_get_dpi();

	/* 96 / 120 / 144 dpi */
	if (dpi > 142)
		mul = 1.5;
	else if (dpi > 118)
		mul = 1.25;
	else
		mul = 1.0;

	return mul;
}

void gtkut_set_dpi(gdouble dpi)
{
	system_dpi = dpi;
}

void gtkut_convert_int_to_gdk_color(gint rgbvalue, GdkColor *color)
{
	g_return_if_fail(color != NULL);

	color->pixel = 0L;
	color->red   = (int) (((gdouble)((rgbvalue & 0xff0000) >> 16) / 255.0) * 65535.0);
	color->green = (int) (((gdouble)((rgbvalue & 0x00ff00) >>  8) / 255.0) * 65535.0);
	color->blue  = (int) (((gdouble) (rgbvalue & 0x0000ff)        / 255.0) * 65535.0);
}

static gboolean reverse_order = FALSE;

void gtkut_stock_button_set_set_reverse(gboolean reverse)
{
	reverse_order = reverse;
}

void gtkut_stock_button_set_create(GtkWidget **bbox,
				   GtkWidget **button1, const gchar *label1,
				   GtkWidget **button2, const gchar *label2,
				   GtkWidget **button3, const gchar *label3)
{
	g_return_if_fail(bbox != NULL);
	g_return_if_fail(button1 != NULL);

	*bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(*bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(*bbox), 6 * gtkut_get_dpi_multiplier());

	if (button3) {
		*button3 = gtk_button_new_from_stock(label3);
		GTK_WIDGET_SET_FLAGS(*button3, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(*bbox), *button3, FALSE, FALSE, 0);
		gtk_widget_show(*button3);
	}

	if (button2) {
		*button2 = gtk_button_new_from_stock(label2);
		GTK_WIDGET_SET_FLAGS(*button2, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(*bbox), *button2, FALSE, FALSE, 0);
		gtk_widget_show(*button2);
	}

	*button1 = gtk_button_new_from_stock(label1);
	GTK_WIDGET_SET_FLAGS(*button1, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(*bbox), *button1, FALSE, FALSE, 0);
	gtk_widget_show(*button1);

	if (reverse_order)
		gtkut_box_set_reverse_order(GTK_BOX(*bbox), TRUE);
}

void gtkut_box_set_reverse_order(GtkBox *box, gboolean reverse)
{
	GList *cur;
	GList *new_order = NULL;
	gint pos = 0;
	gboolean is_reversed;

	g_return_if_fail(box != NULL);

	is_reversed = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(box), "reverse-order"));
	if (is_reversed == reverse)
		return;
	g_object_set_data(G_OBJECT(box), "reverse-order",
			  GINT_TO_POINTER(reverse));

	for (cur = box->children; cur != NULL; cur = cur->next) {
		GtkBoxChild *cinfo = cur->data;
		new_order = g_list_prepend(new_order, cinfo->widget);
	}

	for (cur = new_order; cur != NULL; cur = cur->next) {
		GtkWidget *child = cur->data;
		gtk_box_reorder_child(box, child, pos++);
	}

	g_list_free(new_order);
}

static void combo_button_size_request(GtkWidget *widget,
				      GtkRequisition *requisition,
				      gpointer data)
{
	ComboButton *combo = (ComboButton *)data;

	if (combo->arrow->allocation.height != requisition->height)
		gtk_widget_set_size_request(combo->arrow,
					    -1, requisition->height);
}

static void combo_button_enter(GtkWidget *widget, gpointer data)
{
	ComboButton *combo = (ComboButton *)data;

	if (GTK_WIDGET_STATE(combo->arrow) != GTK_STATE_PRELIGHT) {
		gtk_widget_set_state(combo->arrow, GTK_STATE_PRELIGHT);
		gtk_widget_queue_draw(combo->arrow);
	}
	if (GTK_WIDGET_STATE(combo->button) != GTK_STATE_PRELIGHT) {
		gtk_widget_set_state(combo->button, GTK_STATE_PRELIGHT);
		gtk_widget_queue_draw(combo->button);
	}
}

static void combo_button_leave(GtkWidget *widget, gpointer data)
{
	ComboButton *combo = (ComboButton *)data;

	if (GTK_WIDGET_STATE(combo->arrow) != GTK_STATE_NORMAL) {
		gtk_widget_set_state(combo->arrow, GTK_STATE_NORMAL);
		gtk_widget_queue_draw(combo->arrow);
	}
	if (GTK_WIDGET_STATE(combo->button) != GTK_STATE_NORMAL) {
		gtk_widget_set_state(combo->button, GTK_STATE_NORMAL);
		gtk_widget_queue_draw(combo->button);
	}
}

static gint combo_button_arrow_pressed(GtkWidget *widget, GdkEventButton *event,
				       gpointer data)
{
	ComboButton *combo = (ComboButton *)data;

	if (!event) return FALSE;

	gtk_menu_popup(GTK_MENU(combo->menu), NULL, NULL,
		       menu_button_position, combo->button,
		       event->button, event->time);

	return TRUE;
}

static void combo_button_destroy(GtkWidget *widget, gpointer data)
{
	ComboButton *combo = (ComboButton *)data;

	gtk_object_destroy(GTK_OBJECT(combo->factory));
	g_free(combo);
}

ComboButton *gtkut_combo_button_create(GtkWidget *button,
				       GtkItemFactoryEntry *entries,
				       gint n_entries, const gchar *path,
				       gpointer data)
{
	ComboButton *combo;
	GtkWidget *arrow;

	combo = g_new0(ComboButton, 1);

	combo->arrow = gtk_button_new();
	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_set_size_request(arrow, 7, -1);
	gtk_container_add(GTK_CONTAINER(combo->arrow), arrow);
	GTK_WIDGET_UNSET_FLAGS(combo->arrow, GTK_CAN_FOCUS);
	gtk_widget_show_all(combo->arrow);

	combo->button = button;
	combo->menu = menu_create_items(entries, n_entries, path,
					&combo->factory, data);
	combo->data = data;

	g_signal_connect(G_OBJECT(combo->button), "size_request",
			 G_CALLBACK(combo_button_size_request), combo);
#if 0
	g_signal_connect(G_OBJECT(combo->button), "enter",
			 G_CALLBACK(combo_button_enter), combo);
	g_signal_connect(G_OBJECT(combo->button), "leave",
			 G_CALLBACK(combo_button_leave), combo);
#endif
	g_signal_connect(G_OBJECT(combo->arrow), "enter",
			 G_CALLBACK(combo_button_enter), combo);
	g_signal_connect(G_OBJECT(combo->arrow), "leave",
			 G_CALLBACK(combo_button_leave), combo);
	g_signal_connect(G_OBJECT(combo->arrow), "button_press_event",
			 G_CALLBACK(combo_button_arrow_pressed), combo);
	g_signal_connect(G_OBJECT(combo->arrow), "destroy",
			 G_CALLBACK(combo_button_destroy), combo);

	return combo;
}

gint gtkut_ctree_get_nth_from_node(GtkCTree *ctree, GtkCTreeNode *node)
{
	g_return_val_if_fail(ctree != NULL, -1);
	g_return_val_if_fail(node != NULL, -1);

	return g_list_position(GTK_CLIST(ctree)->row_list, (GList *)node);
}

void gtkut_ctree_set_focus_row(GtkCTree *ctree, GtkCTreeNode *node)
{
	gtkut_clist_set_focus_row(GTK_CLIST(ctree),
				  gtkut_ctree_get_nth_from_node(ctree, node));
}

void gtkut_clist_set_focus_row(GtkCList *clist, gint row)
{
	clist->focus_row = row;
	GTKUT_CTREE_REFRESH(clist);
}

#ifdef G_OS_WIN32
static void vadjustment_changed(GtkAdjustment *adj, gpointer data)
{
	GtkWidget *widget = GTK_WIDGET(data);

	gtk_widget_queue_draw(widget);
}
#elif defined(__APPLE__)
static void clist_select_row(GtkCList *clist, gint row, gint column, GdkEventButton *event, gpointer data)
{
	gtk_widget_queue_draw(GTK_WIDGET(clist));
}
#endif

void gtkut_clist_set_redraw(GtkCList *clist)
{
#ifdef G_OS_WIN32
	if (clist->vadjustment) {
		g_signal_connect(G_OBJECT(clist->vadjustment), "changed",
				 G_CALLBACK(vadjustment_changed), clist);
	}
#elif defined(__APPLE__)
	g_signal_connect(G_OBJECT(clist), "select-row",
			 G_CALLBACK(clist_select_row), NULL);
#endif
}

gboolean gtkut_tree_model_next(GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreeIter iter_, parent;
	gboolean valid;

	if (gtk_tree_model_iter_children(model, &iter_, iter)) {
		*iter = iter_;
		return TRUE;
	}

	iter_ = *iter;
	if (gtk_tree_model_iter_next(model, &iter_)) {
		*iter = iter_;
		return TRUE;
	}

	iter_ = *iter;
	valid = gtk_tree_model_iter_parent(model, &parent, &iter_);
	while (valid) {
		iter_ = parent;
		if (gtk_tree_model_iter_next(model, &iter_)) {
			*iter = iter_;
			return TRUE;
		}

		iter_ = parent;
		valid = gtk_tree_model_iter_parent(model, &parent, &iter_);
	}

	return FALSE;
}

gboolean gtkut_tree_model_prev(GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreeIter iter_, child, next, parent;
	GtkTreePath *path;
	gboolean found = FALSE;

	iter_ = *iter;

	path = gtk_tree_model_get_path(model, &iter_);

	if (gtk_tree_path_prev(path)) {
		gtk_tree_model_get_iter(model, &child, path);

		while (gtk_tree_model_iter_has_child(model, &child)) {
			iter_ = child;
			gtk_tree_model_iter_children(model, &child, &iter_);
			next = child;
			while (gtk_tree_model_iter_next(model, &next))
				child = next;
		}

		*iter = child;
		found = TRUE;
	} else if (gtk_tree_model_iter_parent(model, &parent, &iter_)) {
		*iter = parent;
		found = TRUE;
	}

	gtk_tree_path_free(path);

	return found;
}

gboolean gtkut_tree_model_get_iter_last(GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreeIter iter_, child, next;

	if (!gtk_tree_model_get_iter_first(model, &iter_))
		return FALSE;

	for (;;) {
		next = iter_;
		while (gtk_tree_model_iter_next(model, &next))
			iter_ = next;
		if (gtk_tree_model_iter_children(model, &child, &iter_))
			iter_ = child;
		else
			break;
	}

	*iter = iter_;
	return TRUE;
}

gboolean gtkut_tree_model_find_by_column_data(GtkTreeModel *model,
					      GtkTreeIter *iter,
					      GtkTreeIter *start,
					      gint col, gpointer data)
{
	gboolean valid;
	GtkTreeIter iter_;
	gpointer store_data;

	if (start) {
		gtk_tree_model_get(model, start, col, &store_data, -1);
		if (store_data == data) {
			*iter = *start;
			return TRUE;
		}
		valid = gtk_tree_model_iter_children(model, &iter_, start);
	} else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	while (valid) {
		if (gtkut_tree_model_find_by_column_data
			(model, iter, &iter_, col, data)) {
			return TRUE;
		}

		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

void gtkut_tree_model_foreach(GtkTreeModel *model, GtkTreeIter *start,
			      GtkTreeModelForeachFunc func, gpointer user_data)
{
	gboolean valid = TRUE;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_return_if_fail(func != NULL);

	if (!start) {
		gtk_tree_model_foreach(model, func, user_data);
		return;
	}

	path = gtk_tree_model_get_path(model, start);
	func(model, path, start, user_data);
	gtk_tree_path_free(path);

	valid = gtk_tree_model_iter_children(model, &iter, start);
	while (valid) {
		gtkut_tree_model_foreach(model, &iter, func, user_data);
		valid = gtk_tree_model_iter_next(model, &iter);
	}
}

gboolean gtkut_tree_row_reference_get_iter(GtkTreeModel *model,
					   GtkTreeRowReference *ref,
					   GtkTreeIter *iter)
{
	GtkTreePath *path;
	gboolean valid = FALSE;

	if (ref) {
		path = gtk_tree_row_reference_get_path(ref);
		if (path) {
			valid = gtk_tree_model_get_iter(model, iter, path);
			gtk_tree_path_free(path);
		}
	}

	return valid;
}

gboolean gtkut_tree_row_reference_equal(GtkTreeRowReference *ref1,
					GtkTreeRowReference *ref2)
{
	GtkTreePath *path1, *path2;
	gint result;

	if (ref1 == NULL || ref2 == NULL)
		return FALSE;

	path1 = gtk_tree_row_reference_get_path(ref1);
	if (!path1)
		return FALSE;
	path2 = gtk_tree_row_reference_get_path(ref2);
	if (!path2) {
		gtk_tree_path_free(path1);
		return FALSE;
	}

	result = gtk_tree_path_compare(path1, path2);

	gtk_tree_path_free(path2);
	gtk_tree_path_free(path1);

	return (result == 0);
}

void gtkut_tree_sortable_unset_sort_column_id(GtkTreeSortable *sortable)
{
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_tree_sortable_set_sort_column_id
		(sortable, GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
		 GTK_SORT_ASCENDING);
#else
	GtkTreeStore *store = GTK_TREE_STORE(sortable);

	g_return_if_fail(GTK_IS_TREE_STORE(sortable));

	if (store->sort_column_id == -2 && store->order == GTK_SORT_ASCENDING)
		return;

	store->sort_column_id = -2;
	store->order = GTK_SORT_ASCENDING;

	gtk_tree_sortable_sort_column_changed(sortable);
#endif
}

gboolean gtkut_tree_view_find_collapsed_parent(GtkTreeView *treeview,
					       GtkTreeIter *parent,
					       GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkTreeIter iter_, parent_;
	GtkTreePath *path;
	gboolean valid;

	if (!iter) return FALSE;

	model = gtk_tree_view_get_model(treeview);
	valid = gtk_tree_model_iter_parent(model, &parent_, iter);

	while (valid) {
		path = gtk_tree_model_get_path(model, &parent_);
		if (!gtk_tree_view_row_expanded(treeview, path)) {
			*parent = parent_;
			gtk_tree_path_free(path);
			return TRUE;
		}
		gtk_tree_path_free(path);
		iter_ = parent_;
		valid = gtk_tree_model_iter_parent(model, &parent_, &iter_);
	}

	return FALSE;
}

void gtkut_tree_view_expand_parent_all(GtkTreeView *treeview, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkTreeIter parent;
	GtkTreePath *path;

	model = gtk_tree_view_get_model(treeview);

	if (gtk_tree_model_iter_parent(model, &parent, iter)) {
		path = gtk_tree_model_get_path(model, &parent);
		gtk_tree_view_expand_to_path(treeview, path);
		gtk_tree_path_free(path);
	}
}

#define SCROLL_EDGE_SIZE 15

/* borrowed from gtktreeview.c */
void gtkut_tree_view_vertical_autoscroll(GtkTreeView *treeview)
{
	GdkRectangle visible_rect;
	gint y, wy;
	gint offset;
	GtkAdjustment *vadj;
	gfloat value;

	gdk_window_get_pointer(gtk_tree_view_get_bin_window(treeview),
			       NULL, &wy, NULL);
	gtk_tree_view_widget_to_tree_coords(treeview, 0, wy, NULL, &y);

	gtk_tree_view_get_visible_rect(treeview, &visible_rect);

	/* see if we are near the edge. */
	offset = y - (visible_rect.y + 2 * SCROLL_EDGE_SIZE);
	if (offset > 0) {
		offset = y - (visible_rect.y + visible_rect.height - 2 * SCROLL_EDGE_SIZE);
		if (offset < 0)
			return;
	}

	vadj = gtk_tree_view_get_vadjustment(treeview);
	value = CLAMP(vadj->value + offset, 0.0, vadj->upper - vadj->page_size);
	gtk_adjustment_set_value(vadj, value);
}

/* modified version of gtk_tree_view_scroll_to_cell */
void gtkut_tree_view_scroll_to_cell(GtkTreeView *treeview, GtkTreePath *path,
				    gboolean align_center)
{
	GdkRectangle cell_rect;
	GdkRectangle vis_rect;
	gint dest_x, dest_y;
	gint margin = 0;

	if (!path)
		return;

	gtk_tree_view_get_cell_area(treeview, path, NULL, &cell_rect);
	gtk_tree_view_widget_to_tree_coords(treeview, cell_rect.x, cell_rect.y,
					    NULL, &(cell_rect.y));
	gtk_tree_view_get_visible_rect(treeview, &vis_rect);

	dest_x = vis_rect.x;
	dest_y = vis_rect.y;

	/* add margin */
	if (cell_rect.height * 2 < vis_rect.height)
		margin = cell_rect.height + (align_center ? 0 : 2);

	if (cell_rect.y < vis_rect.y + margin) {
		if (align_center)
			dest_y = cell_rect.y -
				(vis_rect.height - cell_rect.height) / 2;
		else
			dest_y = cell_rect.y - margin;
	}
	if (cell_rect.y + cell_rect.height >
		vis_rect.y + vis_rect.height - margin) {
		if (align_center)
			dest_y = cell_rect.y -
				(vis_rect.height - cell_rect.height) / 2;
		else
			dest_y = cell_rect.y + cell_rect.height -
				vis_rect.height + margin;
	}

	gtk_tree_view_scroll_to_point(treeview, dest_x, dest_y);
}

void gtkut_tree_view_fast_clear(GtkTreeView *treeview, GtkTreeStore *store)
{
#if GTK_CHECK_VERSION(2, 8, 0) && !GTK_CHECK_VERSION(2, 10, 0)
	gtk_tree_store_clear(store);
#else
	/* this is faster than above, but it seems to trigger crashes in
	   GTK+ 2.8.x */
	gtk_tree_view_set_model(treeview, NULL);
	gtk_tree_store_clear(store);
	gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store));
#endif
}

void gtkut_combo_set_items(GtkCombo *combo, const gchar *str1, ...)
{
	va_list args;
	gchar *s;
	GList *combo_items = NULL;

	g_return_if_fail(str1 != NULL);

	combo_items = g_list_append(combo_items, (gpointer)str1);
	va_start(args, str1);
	s = va_arg(args, gchar*);
	while (s) {
		combo_items = g_list_append(combo_items, (gpointer)s);
		s = va_arg(args, gchar*);
	}
	va_end(args);

	gtk_combo_set_popdown_strings(combo, combo_items);

	g_list_free(combo_items);
}

gchar *gtkut_editable_get_selection(GtkEditable *editable)
{
	gint start_pos, end_pos;
	gboolean found;

	g_return_val_if_fail(GTK_IS_EDITABLE(editable), NULL);

	found = gtk_editable_get_selection_bounds(editable,
						  &start_pos, &end_pos);
	if (found)
		return gtk_editable_get_chars(editable, start_pos, end_pos);
	else
		return NULL;
}

void gtkut_editable_disable_im(GtkEditable *editable)
{
	g_return_if_fail(editable != NULL);

#if USE_XIM
	if (editable->ic) {
		gdk_ic_destroy(editable->ic);
		editable->ic = NULL;
	}
	if (editable->ic_attr) {
		gdk_ic_attr_destroy(editable->ic_attr);
		editable->ic_attr = NULL;
	}
#endif
}

void gtkut_entry_strip_text(GtkEntry *entry)
{
	gchar *text;
	gint len;

	text = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	len = strlen(text);
	g_strstrip(text);
	if (len > strlen(text))
		gtk_entry_set_text(entry, text);
	g_free(text);
}

void gtkut_container_remove(GtkContainer *container, GtkWidget *widget)
{
	gtk_container_remove(container, widget);
}

void gtkut_scrolled_window_reset_position(GtkScrolledWindow *window)
{
	GtkAdjustment *adj;

	adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(window));
	gtk_adjustment_set_value(adj, adj->lower);
	adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(window));
	gtk_adjustment_set_value(adj, adj->lower);
}

gboolean gtkut_text_buffer_match_string(GtkTextBuffer *textbuf,
					const GtkTextIter *iter,
					gunichar *wcs, gint len,
					gboolean case_sens)
{
	GtkTextIter start_iter, end_iter;
	gchar *utf8str, *p;
	gint match_count;

	start_iter = end_iter = *iter;
	gtk_text_iter_forward_chars(&end_iter, len);

	utf8str = gtk_text_buffer_get_text(textbuf, &start_iter, &end_iter,
					   FALSE);
	if (!utf8str) return FALSE;

	if ((gint)g_utf8_strlen(utf8str, -1) != len) {
		g_free(utf8str);
		return FALSE;
	}

	for (p = utf8str, match_count = 0;
	     *p != '\0' && match_count < len;
	     p = g_utf8_next_char(p), match_count++) {
		gunichar wc;

		wc = g_utf8_get_char(p);

		if (case_sens) {
			if (wc != wcs[match_count])
				break;
		} else {
			if (g_unichar_tolower(wc) !=
			    g_unichar_tolower(wcs[match_count]))
				break;
		}
	}

	g_free(utf8str);

	if (match_count == len)
		return TRUE;
	else
		return FALSE;
}

gboolean gtkut_text_buffer_find(GtkTextBuffer *buffer, const GtkTextIter *iter,
				const gchar *str, gboolean case_sens,
				GtkTextIter *match_pos)
{
	gunichar *wcs;
	gint len;
	glong items_read = 0, items_written = 0;
	GError *error = NULL;
	GtkTextIter iter_;
	gboolean found = FALSE;

	wcs = g_utf8_to_ucs4(str, -1, &items_read, &items_written, &error);
	if (error != NULL) {
		g_warning("An error occurred while converting a string from UTF-8 to UCS-4: %s\n", error->message);
		g_error_free(error);
	}
	if (!wcs || items_written <= 0) return FALSE;
	len = (gint)items_written;

	iter_ = *iter;
	do {
		found = gtkut_text_buffer_match_string
			(buffer, &iter_, wcs, len, case_sens);
		if (found) {
			*match_pos = iter_;
			break;
		}
	} while (gtk_text_iter_forward_char(&iter_));

	g_free(wcs);

	return found;
}

gboolean gtkut_text_buffer_find_backward(GtkTextBuffer *buffer,
					 const GtkTextIter *iter,
					 const gchar *str, gboolean case_sens,
					 GtkTextIter *match_pos)
{
	gunichar *wcs;
	gint len;
	glong items_read = 0, items_written = 0;
	GError *error = NULL;
	GtkTextIter iter_;
	gboolean found = FALSE;

	wcs = g_utf8_to_ucs4(str, -1, &items_read, &items_written, &error);
	if (error != NULL) {
		g_warning("An error occurred while converting a string from UTF-8 to UCS-4: %s\n", error->message);
		g_error_free(error);
	}
	if (!wcs || items_written <= 0) return FALSE;
	len = (gint)items_written;

	iter_ = *iter;
	while (gtk_text_iter_backward_char(&iter_)) {
		found = gtkut_text_buffer_match_string
			(buffer, &iter_, wcs, len, case_sens);
		if (found) {
			*match_pos = iter_;
			break;
		}
	}

	g_free(wcs);

	return found;
}

#define MAX_TEXT_LINE_LEN	8190

void gtkut_text_buffer_insert_with_tag_by_name(GtkTextBuffer *buffer,
					       GtkTextIter *iter,
					       const gchar *text,
					       gint len,
					       const gchar *tag)
{
	if (len < 0)
		len = strlen(text);

	gtk_text_buffer_insert_with_tags_by_name
		(buffer, iter, text, len, tag, NULL);

	if (len > 0 && text[len - 1] != '\n') {
		/* somehow returns invalid value first (bug?),
		   so call it twice */
		gtk_text_iter_get_chars_in_line(iter);
		if (gtk_text_iter_get_chars_in_line(iter) > MAX_TEXT_LINE_LEN) {
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, iter, "\n", 1, tag, NULL);
		}
	}
}

gchar *gtkut_text_view_get_selection(GtkTextView *textview)
{
	GtkTextBuffer *buffer;
	GtkTextIter start_iter, end_iter;
	gboolean found;

	g_return_val_if_fail(GTK_IS_TEXT_VIEW(textview), NULL);

	buffer = gtk_text_view_get_buffer(textview);
	found = gtk_text_buffer_get_selection_bounds(buffer,
						     &start_iter, &end_iter);
	if (found)
		return gtk_text_buffer_get_text(buffer, &start_iter, &end_iter,
						FALSE);
	else
		return NULL;
}

void gtkut_window_popup(GtkWidget *window)
{
	gint x, y, sx, sy, new_x, new_y;

	g_return_if_fail(window != NULL);
	g_return_if_fail(window->window != NULL);

	sx = gdk_screen_width();
	sy = gdk_screen_height();

	gdk_window_get_origin(window->window, &x, &y);
	new_x = x % sx; if (new_x < 0) new_x = 0;
	new_y = y % sy; if (new_y < 0) new_y = 0;
	if (new_x != x || new_y != y)
		gdk_window_move(window->window, new_x, new_y);

	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), FALSE);
	gtk_widget_show(window);
	gtk_window_present(GTK_WINDOW(window));
#ifdef G_OS_WIN32
	/* ensure that the window is displayed at the top */
	gdk_window_show(window->window);
#endif
}

gboolean gtkut_window_modal_exist(void)
{
	GList *window_list, *cur;
	gboolean exist = FALSE;

	window_list = gtk_window_list_toplevels();
	for (cur = window_list; cur != NULL; cur = cur->next) {
		GtkWidget *window = GTK_WIDGET(cur->data);

		if (GTK_WIDGET_VISIBLE(window) &&
		    gtk_window_get_modal(GTK_WINDOW(window))) {
			exist = TRUE;
			break;
		}
	}
	g_list_free(window_list);

	return exist;
}

/* ensure that the window is displayed on screen */
void gtkut_window_move(GtkWindow *window, gint x, gint y)
{
	g_return_if_fail(window != NULL);

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	x %= gdk_screen_width();
	y %= gdk_screen_height();

	gtk_window_move(window, x, y);
}

void gtkut_widget_get_uposition(GtkWidget *widget, gint *px, gint *py)
{
	gint x, y;
	gint sx, sy;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(widget->window != NULL);

	sx = gdk_screen_width();
	sy = gdk_screen_height();

	/* gdk_window_get_root_origin ever return *rootwindow*'s position */
	gdk_window_get_root_origin(widget->window, &x, &y);

	x %= sx; if (x < 0) x = 0;
	y %= sy; if (y < 0) y = 0;
	*px = x;
	*py = y;
}

void gtkut_widget_draw_now(GtkWidget *widget)
{
	if (GTK_WIDGET_VISIBLE(widget) && GTK_WIDGET_DRAWABLE(widget))
		gdk_window_process_updates(widget->window, FALSE);
}

static void gtkut_clist_bindings_add(GtkWidget *clist)
{
	GtkBindingSet *binding_set;

	binding_set = gtk_binding_set_by_class(GTK_CLIST_GET_CLASS(clist));

	gtk_binding_entry_add_signal(binding_set, GDK_n, GDK_CONTROL_MASK,
				     "scroll_vertical", 2,
				     G_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				     G_TYPE_FLOAT, 0.0);
	gtk_binding_entry_add_signal(binding_set, GDK_p, GDK_CONTROL_MASK,
				     "scroll_vertical", 2,
				     G_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				     G_TYPE_FLOAT, 0.0);
}

void gtkut_widget_init(void)
{
	GtkWidget *clist;

	clist = gtk_clist_new(1);
	g_object_ref(G_OBJECT(clist));
	gtk_object_sink(GTK_OBJECT(clist));
	gtkut_clist_bindings_add(clist);
	g_object_unref(G_OBJECT(clist));

	clist = gtk_ctree_new(1, 0);
	g_object_ref(G_OBJECT(clist));
	gtk_object_sink(GTK_OBJECT(clist));
	gtkut_clist_bindings_add(clist);
	g_object_unref(G_OBJECT(clist));
}

void gtkut_events_flush(void)
{
	GTK_EVENTS_FLUSH();
}
