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

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtkitemfactory.h>
#include <stdlib.h>
#include <stdarg.h>

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif

#include "gtkutils.h"
#include "utils.h"
#include "gtksctree.h"
#include "codeconv.h"
#include "menu.h"

#warning FIXME_GTK2
gboolean gtkut_get_font_size(GtkWidget *widget, gint *width, gint *height)
{
	PangoLayout *layout;
	const gchar *str = "Abcdef";

	g_return_val_if_fail(GTK_IS_WIDGET(widget), FALSE);

	layout = gtk_widget_create_pango_layout(widget, str);
	g_return_val_if_fail(layout, FALSE);
	pango_layout_get_pixel_size(layout, width, height);
	if (width)
		*width = *width / g_utf8_strlen(str, -1);
	g_object_unref(layout);

	return TRUE;
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

void gtkut_convert_int_to_gdk_color(gint rgbvalue, GdkColor *color)
{
	g_return_if_fail(color != NULL);

	color->pixel = 0L;
	color->red   = (int) (((gdouble)((rgbvalue & 0xff0000) >> 16) / 255.0) * 65535.0);
	color->green = (int) (((gdouble)((rgbvalue & 0x00ff00) >>  8) / 255.0) * 65535.0);
	color->blue  = (int) (((gdouble) (rgbvalue & 0x0000ff)        / 255.0) * 65535.0);
}

void gtkut_button_set_create(GtkWidget **bbox,
			     GtkWidget **button1, const gchar *label1,
			     GtkWidget **button2, const gchar *label2,
			     GtkWidget **button3, const gchar *label3)
{
	g_return_if_fail(bbox != NULL);
	g_return_if_fail(button1 != NULL);

	*bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(*bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(*bbox), 5);

	*button1 = gtk_button_new_with_label(label1);
	GTK_WIDGET_SET_FLAGS(*button1, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(*bbox), *button1, TRUE, TRUE, 0);
	gtk_widget_show(*button1);

	if (button2) {
		*button2 = gtk_button_new_with_label(label2);
		GTK_WIDGET_SET_FLAGS(*button2, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(*bbox), *button2, TRUE, TRUE, 0);
		gtk_widget_show(*button2);
	}

	if (button3) {
		*button3 = gtk_button_new_with_label(label3);
		GTK_WIDGET_SET_FLAGS(*button3, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(*bbox), *button3, TRUE, TRUE, 0);
		gtk_widget_show(*button3);
	}
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
	gtk_box_set_spacing(GTK_BOX(*bbox), 5);

	*button1 = gtk_button_new_from_stock(label1);
	GTK_WIDGET_SET_FLAGS(*button1, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(*bbox), *button1, TRUE, TRUE, 0);
	gtk_widget_show(*button1);

	if (button2) {
		*button2 = gtk_button_new_from_stock(label2);
		GTK_WIDGET_SET_FLAGS(*button2, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(*bbox), *button2, TRUE, TRUE, 0);
		gtk_widget_show(*button2);
	}

	if (button3) {
		*button3 = gtk_button_new_from_stock(label3);
		GTK_WIDGET_SET_FLAGS(*button3, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(*bbox), *button3, TRUE, TRUE, 0);
		gtk_widget_show(*button3);
	}
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
	g_signal_connect(G_OBJECT(combo->button), "enter",
			 G_CALLBACK(combo_button_enter), combo);
	g_signal_connect(G_OBJECT(combo->button), "leave",
			 G_CALLBACK(combo_button_leave), combo);
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

#define CELL_SPACING 1
#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
				    (((row) + 1) * CELL_SPACING) + \
				    (clist)->voffset)
#define ROW_FROM_YPIXEL(clist, y) (((y) - (clist)->voffset) / \
				   ((clist)->row_height + CELL_SPACING))

void gtkut_ctree_node_move_if_on_the_edge(GtkCTree *ctree, GtkCTreeNode *node)
{
	GtkCList *clist = GTK_CLIST(ctree);
	gint row;
	GtkVisibility row_visibility, prev_row_visibility, next_row_visibility;

	g_return_if_fail(ctree != NULL);
	g_return_if_fail(node != NULL);

	row = g_list_position(clist->row_list, (GList *)node);
	if (row < 0 || row >= clist->rows || clist->row_height == 0) return;
	row_visibility = gtk_clist_row_is_visible(clist, row);
	prev_row_visibility = gtk_clist_row_is_visible(clist, row - 1);
	next_row_visibility = gtk_clist_row_is_visible(clist, row + 1);

	if (row_visibility == GTK_VISIBILITY_NONE) {
		gtk_clist_moveto(clist, row, -1, 0.5, 0);
		return;
	}
	if (row_visibility == GTK_VISIBILITY_FULL &&
	    prev_row_visibility == GTK_VISIBILITY_FULL &&
	    next_row_visibility == GTK_VISIBILITY_FULL)
		return;
	if (prev_row_visibility != GTK_VISIBILITY_FULL &&
	    next_row_visibility != GTK_VISIBILITY_FULL)
		return;

	if (prev_row_visibility != GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(clist, row, -1, 0.2, 0);
		return;
	}
	if (next_row_visibility != GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(clist, row, -1, 0.8, 0);
		return;
	}
}

#undef CELL_SPACING
#undef ROW_TOP_YPIXEL
#undef ROW_FROM_YPIXEL

gint gtkut_ctree_get_nth_from_node(GtkCTree *ctree, GtkCTreeNode *node)
{
	g_return_val_if_fail(ctree != NULL, -1);
	g_return_val_if_fail(node != NULL, -1);

	return g_list_position(GTK_CLIST(ctree)->row_list, (GList *)node);
}

/* get the next node, including the invisible one */
GtkCTreeNode *gtkut_ctree_node_next(GtkCTree *ctree, GtkCTreeNode *node)
{
	GtkCTreeNode *parent;

	if (!node) return NULL;

	if (GTK_CTREE_ROW(node)->children)
		return GTK_CTREE_ROW(node)->children;

	if (GTK_CTREE_ROW(node)->sibling)
		return GTK_CTREE_ROW(node)->sibling;

	for (parent = GTK_CTREE_ROW(node)->parent; parent != NULL;
	     parent = GTK_CTREE_ROW(parent)->parent) {
		if (GTK_CTREE_ROW(parent)->sibling)
			return GTK_CTREE_ROW(parent)->sibling;
	}

	return NULL;
}

/* get the previous node, including the invisible one */
GtkCTreeNode *gtkut_ctree_node_prev(GtkCTree *ctree, GtkCTreeNode *node)
{
	GtkCTreeNode *prev;
	GtkCTreeNode *child;

	if (!node) return NULL;

	prev = GTK_CTREE_NODE_PREV(node);
	if (prev == GTK_CTREE_ROW(node)->parent)
		return prev;

	child = prev;
	while (GTK_CTREE_ROW(child)->children != NULL) {
		child = GTK_CTREE_ROW(child)->children;
		while (GTK_CTREE_ROW(child)->sibling != NULL)
			child = GTK_CTREE_ROW(child)->sibling;
	}

	return child;
}

gboolean gtkut_ctree_node_is_selected(GtkCTree *ctree, GtkCTreeNode *node)
{
	GtkCList *clist = GTK_CLIST(ctree);
	GList *cur;

	for (cur = clist->selection; cur != NULL; cur = cur->next) {
		if (node == GTK_CTREE_NODE(cur->data))
			return TRUE;
	}

	return FALSE;
}

GtkCTreeNode *gtkut_ctree_find_collapsed_parent(GtkCTree *ctree,
						GtkCTreeNode *node)
{
	if (!node) return NULL;

	while ((node = GTK_CTREE_ROW(node)->parent) != NULL) {
		if (!GTK_CTREE_ROW(node)->expanded)
			return node;
	}

	return NULL;
}

void gtkut_ctree_expand_parent_all(GtkCTree *ctree, GtkCTreeNode *node)
{
	while ((node = gtkut_ctree_find_collapsed_parent(ctree, node)) != NULL)
		gtk_ctree_expand(ctree, node);
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
	guint start_pos, end_pos;
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

/*
 * Walk through the widget tree and disclaim the selection from all currently
 * realized GtkEditable widgets.
 */
static void gtkut_check_before_remove(GtkWidget *widget, gpointer unused)
{
	g_return_if_fail(widget != NULL);

	if (!GTK_WIDGET_REALIZED(widget))
		return; /* all nested widgets must be unrealized too */
	if (GTK_IS_CONTAINER(widget))
		gtk_container_forall(GTK_CONTAINER(widget),
				     gtkut_check_before_remove, NULL);
#if 0
	if (GTK_IS_EDITABLE(widget))
		gtk_editable_claim_selection(GTK_EDITABLE(widget),
					     FALSE, GDK_CURRENT_TIME);
#endif
}

/*
 * Wrapper around gtk_container_remove to work around a bug in GtkText and
 * GtkEntry (in all GTK+ versions up to and including at least 1.2.10).
 *
 * The problem is that unrealizing a GtkText or GtkEntry widget which has the
 * active selection completely messes up selection handling, leading to
 * non-working selections and crashes.  Removing a widget from its container
 * implies unrealizing it and all its child widgets; this triggers the bug if
 * the removed widget or any of its children is GtkText or GtkEntry.  As a
 * workaround, this function walks through the widget subtree before removing
 * and disclaims the selection from all GtkEditable widgets found.
 *
 * A similar workaround may be needed for gtk_widget_reparent(); currently it
 * is not necessary because Sylpheed does not use gtk_widget_reparent() for
 * GtkEditable widgets or containers holding such widgets.
 */
void gtkut_container_remove(GtkContainer *container, GtkWidget *widget)
{
	gtkut_check_before_remove(widget, NULL);
	gtk_container_remove(container, widget);
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
		g_warning("An error occured while converting a string from UTF-8 to UCS-4: %s\n", error->message);
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
		g_warning("An error occured while converting a string from UTF-8 to UCS-4: %s\n", error->message);
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

	gtk_window_present(GTK_WINDOW(window));
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

#warning FIXME_GTK2
void gtkut_widget_wait_for_draw(GtkWidget *widget)
{
	if (!GTK_WIDGET_VISIBLE(widget) || !GTK_WIDGET_MAPPED(widget)) return;

	while (gtk_events_pending())
		gtk_main_iteration();
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

	clist = gtk_sctree_new_with_titles(1, 0, NULL);
	g_object_ref(G_OBJECT(clist));
	gtk_object_sink(GTK_OBJECT(clist));
	gtkut_clist_bindings_add(clist);
	g_object_unref(G_OBJECT(clist));
}
