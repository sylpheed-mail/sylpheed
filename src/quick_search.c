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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkentry.h>

#include "summaryview.h"
#include "quick_search.h"
#include "filter.h"
#include "procheader.h"
#include "menu.h"
#include "addressbook.h"

static const struct {
	QSearchCondType type;
	FilterCondType ftype;
} qsearch_cond_types[] = {
	{QS_ALL,	-1},
	{QS_UNREAD,	FLT_COND_UNREAD},
	{QS_MARK,	FLT_COND_MARK},
	{QS_CLABEL,	FLT_COND_COLOR_LABEL},
	{QS_MIME,	FLT_COND_MIME},
	{QS_W1DAY,	-1},
	{QS_LAST5,	-1},
	{QS_LAST7,	-1},
	{QS_IN_ADDRESSBOOK,	-1},
	{QS_LAST30,	-1}
};

static GdkColor dim_color = {0, COLOR_DIM, COLOR_DIM, COLOR_DIM};

static void menu_activated		(GtkWidget	*menuitem,
					 QuickSearch	*qsearch);
static gboolean entry_focus_in		(GtkWidget	*entry,
					 GdkEventFocus	*event,
					 QuickSearch	*qsearch);
static gboolean entry_focus_out		(GtkWidget	*entry,
					 GdkEventFocus	*event,
					 QuickSearch	*qsearch);
static void entry_changed		(GtkWidget	*entry,
					 QuickSearch	*qsearch);
static void entry_activated		(GtkWidget	*entry,
					 QuickSearch	*qsearch);
static gboolean entry_key_pressed	(GtkWidget	*treeview,
					 GdkEventKey	*event,
					 QuickSearch	*qsearch);
static void clear_clicked		(GtkWidget	*button,
					 QuickSearch	*qsearch);


QuickSearch *quick_search_create(SummaryView *summaryview)
{
	QuickSearch *qsearch;
	GtkWidget *hbox;
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkWidget *hbox2;
	GtkWidget *label;
	GtkWidget *entry;
	GtkTooltips *tip;
	GtkWidget *vbox;
	GtkWidget *clear_btn;
	GtkWidget *image;
	GtkWidget *status_label;

	qsearch = g_new0(QuickSearch, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, FALSE, FALSE, 0);

#define COND_MENUITEM_ADD(str, action)					\
{									\
	MENUITEM_ADD(menu, menuitem, str, action);			\
	g_signal_connect(G_OBJECT(menuitem), "activate",		\
			 G_CALLBACK(menu_activated), qsearch);		\
}

	menu = gtk_menu_new();
	COND_MENUITEM_ADD(_("All"), QS_ALL);
	COND_MENUITEM_ADD(_("Unread"), QS_UNREAD);
	COND_MENUITEM_ADD(_("Marked"), QS_MARK);
	COND_MENUITEM_ADD(_("Have color label"), QS_CLABEL);
	COND_MENUITEM_ADD(_("Have attachment"), QS_MIME);
	MENUITEM_ADD(menu, menuitem, NULL, 0);
	COND_MENUITEM_ADD(_("Within 1 day"), QS_W1DAY);
	COND_MENUITEM_ADD(_("Last 5 days"), QS_LAST5);
	COND_MENUITEM_ADD(_("Last 7 days"), QS_LAST7);
	COND_MENUITEM_ADD(_("Last 30 days"), QS_LAST30);
	MENUITEM_ADD(menu, menuitem, NULL, 0);
	COND_MENUITEM_ADD(_("In addressbook"), QS_IN_ADDRESSBOOK);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);

#undef COND_MENUITEM_ADD

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_size_request(hbox2, 8, -1);
	gtk_box_pack_start(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);

	label = gtk_label_new(_("Search:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_size_request(hbox2, 4, -1);
	gtk_box_pack_start(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_widget_set_size_request(entry, 200 * gtkut_get_dpi_multiplier(), -1);
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(entry), "focus-in-event",
			 G_CALLBACK(entry_focus_in), qsearch);
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
			 G_CALLBACK(entry_focus_out), qsearch);
	g_signal_connect(G_OBJECT(entry), "changed",
			 G_CALLBACK(entry_changed), qsearch);
        g_signal_connect(G_OBJECT(entry), "activate",
                         G_CALLBACK(entry_activated), qsearch);
        g_signal_connect(G_OBJECT(entry), "key_press_event",
                         G_CALLBACK(entry_key_pressed), qsearch);

	tip = gtk_tooltips_new();
	gtk_tooltips_set_tip(tip, entry, _("Search for Subject or From"), NULL);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_size_request(hbox2, 2, -1);
	gtk_box_pack_start(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	clear_btn = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(clear_btn), GTK_RELIEF_NONE);
        gtk_widget_set_size_request(clear_btn, 20, 20);
        image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
        gtk_container_add(GTK_CONTAINER(clear_btn), image);
        GTK_WIDGET_UNSET_FLAGS(clear_btn, GTK_CAN_FOCUS);
        gtk_box_pack_start(GTK_BOX(vbox), clear_btn, TRUE, FALSE, 0);
        g_signal_connect(G_OBJECT(clear_btn), "clicked",
                         G_CALLBACK(clear_clicked), qsearch);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_size_request(hbox2, 4, -1);
	gtk_box_pack_start(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);

	status_label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), status_label, FALSE, FALSE, 0);

	qsearch->hbox = hbox;
	qsearch->optmenu = optmenu;
	qsearch->menu = menu;
	qsearch->label = label;
	qsearch->entry = entry;
	qsearch->clear_btn = clear_btn;
	qsearch->status_label = status_label;
	qsearch->summaryview = summaryview;
	summaryview->qsearch = qsearch;
	qsearch->entry_entered = FALSE;

	gtk_widget_show_all(hbox);
	gtk_widget_hide(clear_btn);

	entry_focus_out(entry, NULL, qsearch);

	return qsearch;
}

void quick_search_clear_entry(QuickSearch *qsearch)
{
	qsearch->entry_entered = FALSE;
	if (GTK_WIDGET_HAS_FOCUS(qsearch->entry))
		entry_focus_in(qsearch->entry, NULL, qsearch);
	else
		entry_focus_out(qsearch->entry, NULL, qsearch);

	gtk_label_set_text(GTK_LABEL(qsearch->status_label), "");
	gtk_widget_hide(qsearch->clear_btn);
}

GSList *quick_search_filter(QuickSearch *qsearch, QSearchCondType type,
			   const gchar *key)
{
	SummaryView *summaryview = qsearch->summaryview;
	FilterCondType ftype;
	FilterRule *status_rule = NULL;
	FilterRule *rule = NULL;
	FilterCond *cond;
	FilterInfo fltinfo;
	GSList *cond_list = NULL;
	GSList *rule_list = NULL;
	GSList *flt_mlist = NULL;
	GSList *cur;
	gint count = 0, total = 0;
	gchar status_text[1024];
	gboolean dmode;

	if (!summaryview->all_mlist)
		return NULL;

	debug_print("quick_search_filter: filtering summary (type: %d)\n",
		    type);

	switch (type) {
	case QS_UNREAD:
	case QS_MARK:
	case QS_CLABEL:
	case QS_MIME:
		ftype = qsearch_cond_types[type].ftype;
		cond = filter_cond_new(ftype, 0, 0, NULL, NULL);
		cond_list = g_slist_append(cond_list, cond);
		status_rule = filter_rule_new("Status filter rule", FLT_OR,
					      cond_list, NULL);
		break;
	case QS_W1DAY:
		cond = filter_cond_new(FLT_COND_AGE_GREATER, 0, FLT_NOT_MATCH,
				       NULL, "1");
		cond_list = g_slist_append(cond_list, cond);
		status_rule = filter_rule_new("Status filter rule", FLT_OR,
					      cond_list, NULL);
		break;
	case QS_LAST5:
		cond = filter_cond_new(FLT_COND_AGE_GREATER, 0, FLT_NOT_MATCH,
				       NULL, "5");
		cond_list = g_slist_append(cond_list, cond);
		status_rule = filter_rule_new("Status filter rule", FLT_OR,
					      cond_list, NULL);
		break;
	case QS_LAST7:
		cond = filter_cond_new(FLT_COND_AGE_GREATER, 0, FLT_NOT_MATCH,
				       NULL, "7");
		cond_list = g_slist_append(cond_list, cond);
		status_rule = filter_rule_new("Status filter rule", FLT_OR,
					      cond_list, NULL);
		break;
	case QS_LAST30:
		cond = filter_cond_new(FLT_COND_AGE_GREATER, 0, FLT_NOT_MATCH,
				       NULL, "30");
		cond_list = g_slist_append(cond_list, cond);
		status_rule = filter_rule_new("Status filter rule", FLT_OR,
					      cond_list, NULL);
		break;
	case QS_IN_ADDRESSBOOK:
		cond = filter_cond_new(FLT_COND_HEADER, FLT_IN_ADDRESSBOOK, 0,
				       "From", NULL);
		cond_list = g_slist_append(cond_list, cond);
		status_rule = filter_rule_new("Status filter rule", FLT_OR,
					      cond_list, NULL);
		break;
	case QS_ALL:
	default:
		break;
	}

	if (key) {
		gchar **keys;
		gint i;

		keys = g_strsplit(key, " ", -1);
		for (i = 0; keys[i] != NULL; i++) {
			cond_list = NULL;

			if (keys[i] == '\0')
				continue;

			cond = filter_cond_new(FLT_COND_HEADER, FLT_CONTAIN, 0,
					       "Subject", keys[i]);
			cond_list = g_slist_append(cond_list, cond);
			cond = filter_cond_new(FLT_COND_HEADER, FLT_CONTAIN, 0,
					       "From", keys[i]);
			cond_list = g_slist_append(cond_list, cond);
			if (FOLDER_ITEM_IS_SENT_FOLDER(summaryview->folder_item)) {
				cond = filter_cond_new(FLT_COND_TO_OR_CC, FLT_CONTAIN,
						       0, NULL, keys[i]);
				cond_list = g_slist_append(cond_list, cond);
			}

			if (cond_list) {
				rule = filter_rule_new("Quick search rule",
						       FLT_OR, cond_list, NULL);
				rule_list = g_slist_append(rule_list, rule);
			}
		}
		g_strfreev(keys);
	}

	memset(&fltinfo, 0, sizeof(FilterInfo));
	dmode = get_debug_mode();
	set_debug_mode(FALSE);

	for (cur = summaryview->all_mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		GSList *hlist = NULL;
		gboolean matched = TRUE;

		total++;

		if (status_rule) {
			if (type == QS_IN_ADDRESSBOOK)
				hlist = procheader_get_header_list_from_msginfo
					(msginfo);
			if (!filter_match_rule(status_rule, msginfo, hlist,
					       &fltinfo)) {
				if (hlist)
					procheader_header_list_destroy(hlist);
				continue;
			}
		}

		if (rule_list) {
			GSList *rcur;

			if (!hlist)
				hlist = procheader_get_header_list_from_msginfo
					(msginfo);

			/* AND keyword match */
			for (rcur = rule_list; rcur != NULL; rcur = rcur->next) {
				rule = (FilterRule *)rcur->data;
				if (!filter_match_rule(rule, msginfo, hlist, &fltinfo)) {
					matched = FALSE;
					break;
				}
			}
		}

		if (matched) {
			flt_mlist = g_slist_prepend(flt_mlist, msginfo);
			count++;
		}

		if (hlist)
			procheader_header_list_destroy(hlist);
	}
	flt_mlist = g_slist_reverse(flt_mlist);

	set_debug_mode(dmode);

	if (status_rule || rule) {
		if (count > 0)
			g_snprintf(status_text, sizeof(status_text),
				   _("%1$d in %2$d matched"), count, total);
		else
			g_snprintf(status_text, sizeof(status_text),
				   _("No messages matched"));
		gtk_label_set_text(GTK_LABEL(qsearch->status_label),
				   status_text);
	} else
		gtk_label_set_text(GTK_LABEL(qsearch->status_label), "");

	filter_rule_list_free(rule_list);
	filter_rule_free(status_rule);

	return flt_mlist;
}

static void menu_activated(GtkWidget *menuitem, QuickSearch *qsearch)
{
	summary_qsearch(qsearch->summaryview);
}

static gboolean entry_focus_in(GtkWidget *entry, GdkEventFocus *event,
			       QuickSearch *qsearch)
{
	if (!qsearch->entry_entered) {
		g_signal_handlers_block_by_func(entry, entry_changed, qsearch);
		gtk_entry_set_text(GTK_ENTRY(entry), "");
		gtk_widget_modify_text(entry, GTK_STATE_NORMAL, NULL);
		g_signal_handlers_unblock_by_func(entry, entry_changed, qsearch);
	}

	return FALSE;
}

static gboolean entry_focus_out(GtkWidget *entry, GdkEventFocus *event,
				QuickSearch *qsearch)
{
	if (!qsearch->entry_entered) {
		g_signal_handlers_block_by_func(entry, entry_changed, qsearch);
		gtk_widget_modify_text(entry, GTK_STATE_NORMAL, &dim_color);
		gtk_entry_set_text(GTK_ENTRY(entry), _("Search for Subject or From"));
		g_signal_handlers_unblock_by_func(entry, entry_changed, qsearch);
	}

	return FALSE;
}

static void entry_changed(GtkWidget *entry, QuickSearch *qsearch)
{
	const gchar *text;

	text = gtk_entry_get_text(GTK_ENTRY(entry));
	if (text && *text != '\0') {
		gtk_widget_show(qsearch->clear_btn);
		qsearch->entry_entered = TRUE;
	} else {
		gtk_widget_hide(qsearch->clear_btn);
		qsearch->entry_entered = FALSE;
	}
}

static void entry_activated(GtkWidget *entry, QuickSearch *qsearch)
{
	gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
	summary_qsearch(qsearch->summaryview);
}

static gboolean entry_key_pressed(GtkWidget *treeview, GdkEventKey *event,
				  QuickSearch *qsearch)
{
	if (event && event->keyval == GDK_Escape) {
		summary_qsearch_clear_entry(qsearch->summaryview);
		return TRUE;
	}
	return FALSE;
}

static void clear_clicked(GtkWidget *button, QuickSearch *qsearch)
{
	summary_qsearch_clear_entry(qsearch->summaryview);
}
