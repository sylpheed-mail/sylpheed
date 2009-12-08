/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
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
	{QS_LAST7,	-1}
};

static void menu_activated		(GtkWidget	*menuitem,
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
	gtk_widget_set_size_request(entry, 200, -1);
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
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

	qsearch->hbox = hbox;
	qsearch->optmenu = optmenu;
	qsearch->menu = menu;
	qsearch->label = label;
	qsearch->entry = entry;
	qsearch->clear_btn = clear_btn;
	qsearch->summaryview = summaryview;
	summaryview->qsearch = qsearch;

	gtk_widget_show_all(hbox);
	gtk_widget_hide(clear_btn);

	return qsearch;
}

void quick_search_clear_entry(QuickSearch *qsearch)
{
	gtk_entry_set_text(GTK_ENTRY(qsearch->entry), "");
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
	GSList *flt_mlist = NULL;
	GSList *cur;

	if (!summaryview->all_mlist)
		return NULL;

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
	case QS_ALL:
	default:
		break;
	}

	cond_list = NULL;

	if (key && *key != '\0') {
		cond = filter_cond_new(FLT_COND_HEADER, FLT_CONTAIN, 0,
				       "Subject", key);
		cond_list = g_slist_append(cond_list, cond);
		cond = filter_cond_new(FLT_COND_HEADER, FLT_CONTAIN, 0,
				       "From", key);
		cond_list = g_slist_append(cond_list, cond);
		if (FOLDER_ITEM_IS_SENT_FOLDER(summaryview->folder_item)) {
			cond = filter_cond_new(FLT_COND_TO_OR_CC, FLT_CONTAIN,
					       0, NULL, key);
			cond_list = g_slist_append(cond_list, cond);
		}
		rule = filter_rule_new("Quick search rule", FLT_OR, cond_list,
				       NULL);
	}

	memset(&fltinfo, 0, sizeof(FilterInfo));

	for (cur = summaryview->all_mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		GSList *hlist;

		if (status_rule) {
			if (!filter_match_rule(status_rule, msginfo, NULL,
					       &fltinfo))
				continue;
		}

		if (rule) {
			hlist = procheader_get_header_list_from_msginfo
				(msginfo);
			if (filter_match_rule(rule, msginfo, hlist, &fltinfo))
				flt_mlist = g_slist_prepend(flt_mlist, msginfo);

			procheader_header_list_destroy(hlist);
		} else
			flt_mlist = g_slist_prepend(flt_mlist, msginfo);
	}
	flt_mlist = g_slist_reverse(flt_mlist);

	filter_rule_free(rule);
	filter_rule_free(status_rule);

	return flt_mlist;
}

static void menu_activated(GtkWidget *menuitem, QuickSearch *qsearch)
{
	summary_qsearch(qsearch->summaryview);
}

static void entry_changed(GtkWidget *entry, QuickSearch *qsearch)
{
	const gchar *text;

	text = gtk_entry_get_text(GTK_ENTRY(entry));
	if (text && *text != '\0')
		gtk_widget_show(qsearch->clear_btn);
	else
		gtk_widget_hide(qsearch->clear_btn);
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
