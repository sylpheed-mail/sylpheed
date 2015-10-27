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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "main.h"
#include "prefs.h"
#include "prefs_filter.h"
#include "prefs_filter_edit.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "colorlabel.h"
#include "manage_window.h"
#include "procheader.h"
#include "menu.h"
#include "filter.h"
#include "utils.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "alertpanel.h"
#include "folder.h"
#include "plugin.h"

static struct FilterRuleEditWindow {
	GtkWidget *window;

	GtkWidget *name_entry;
	GtkWidget *bool_op_optmenu;

	GtkWidget *cond_scrolled_win;
	FilterCondEdit cond_edit;

	GtkWidget *action_scrolled_win;
	GtkWidget *action_vbox;
	GSList *action_hbox_list;

	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	FilterRule *new_rule;
	gboolean edit_finished;
} rule_edit_window;

static struct FilterEditHeaderListDialog {
	GtkWidget *window;
	GtkWidget *clist;
	GtkWidget *entry;

	gboolean finished;
	gboolean ok;
} edit_header_list_dialog;

static void prefs_filter_edit_create		(void);
static void prefs_filter_edit_clear		(void);
static void prefs_filter_edit_rule_to_dialog	(FilterRule	*rule,
						 const gchar	*default_name);
static void prefs_filter_edit_update_header_list(FilterCondEdit	*cond_list);

static void prefs_filter_edit_set_action_hbox_menu_sensitive
						(ActionHBox	*hbox,
						 ActionMenuType	 type,
						 gboolean	 sensitive);
static void prefs_filter_edit_set_action_hbox_menus_sensitive
						(void);

static void prefs_filter_edit_get_action_hbox_menus_selection
						(gboolean	*selection);
static ActionMenuType prefs_filter_edit_get_action_hbox_type
						(ActionHBox	*hbox);

static void prefs_filter_edit_insert_action_hbox(ActionHBox	*hbox,
						 gint		 pos);
static void prefs_filter_edit_remove_cond_hbox	(FilterCondEdit	*cond_edit,
						 CondHBox	*hbox);
static void prefs_filter_edit_remove_action_hbox(ActionHBox	*hbox);

static void prefs_filter_edit_add_rule_action	(FilterRule	*rule);

static void prefs_filter_edit_set_cond_header_menu
						(FilterCondEdit	*cond_edit,
						 CondHBox	*hbox);

static void prefs_filter_edit_activate_cond_header
						(FilterCondEdit	*cond_edit,
						 const gchar	*header);

static void prefs_filter_edit_edit_header_list	(FilterCondEdit	*cond_edit);

static FilterRule *prefs_filter_edit_dialog_to_rule	(void);

/* callback functions */
static gint prefs_filter_edit_deleted		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);
static gboolean prefs_filter_edit_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_filter_edit_ok		(void);
static void prefs_filter_edit_cancel		(void);

static void prefs_filter_cond_activated_cb	(GtkWidget	*widget,
						 gpointer	 data);
static void prefs_filter_match_activated_cb	(GtkWidget	*widget,
						 gpointer	 data);
static void prefs_filter_action_activated_cb	(GtkWidget	*widget,
						 gpointer	 data);

static void prefs_filter_action_select_dest_cb	(GtkWidget	*widget,
						 gpointer	 data);

static void prefs_filter_cond_del_cb		(GtkWidget	*widget,
						 gpointer	 data);
static void prefs_filter_cond_add_cb		(GtkWidget	*widget,
						 gpointer	 data);
static void prefs_filter_action_del_cb		(GtkWidget	*widget,
						 gpointer	 data);
static void prefs_filter_action_add_cb		(GtkWidget	*widget,
						 gpointer	 data);


FilterRule *prefs_filter_edit_open(FilterRule *rule, const gchar *header,
				   const gchar *key)
{
	static gboolean lock = FALSE;
	FilterRule *new_rule;

	if (lock)
		return NULL;

	lock = TRUE;

	if (!rule_edit_window.window)
		prefs_filter_edit_create();

	manage_window_set_transient(GTK_WINDOW(rule_edit_window.window));

	prefs_filter_edit_set_header_list(&rule_edit_window.cond_edit, rule);
	prefs_filter_edit_rule_to_dialog(rule, key);
	if (header)
		prefs_filter_edit_activate_cond_header
			(&rule_edit_window.cond_edit, header);
	gtk_widget_show(rule_edit_window.window);

	syl_plugin_signal_emit("prefs-filter-edit-open", rule, header, key,
			       rule_edit_window.window);

	rule_edit_window.new_rule = NULL;
	rule_edit_window.edit_finished = FALSE;
	while (rule_edit_window.edit_finished == FALSE)
		gtk_main_iteration();

	gtk_widget_hide(rule_edit_window.window);
	prefs_filter_edit_clear();
	prefs_filter_set_msg_header_list(NULL);

	new_rule = rule_edit_window.new_rule;
	rule_edit_window.new_rule = NULL;

	if (new_rule)
		debug_print("new rule created: %s\n", new_rule->name);

	lock = FALSE;

	return new_rule;
}

static void prefs_filter_edit_create(void)
{
	GtkWidget *window;

	GtkWidget *vbox;

	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *name_entry;

	GtkWidget *bool_op_optmenu;
	GtkWidget *menu;
	GtkWidget *menuitem;

	GtkWidget *cond_scrolled_win;
	GtkWidget *cond_vbox;

	GtkWidget *action_scrolled_win;
	GtkWidget *action_vbox;

	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_widget_set_size_request(window, 632 * gtkut_get_dpi_multiplier(), 405 * gtkut_get_dpi_multiplier());
	gtk_window_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_stock_button_set_create(&confirm_area, &ok_btn, GTK_STOCK_OK,
				      &cancel_btn, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_window_set_title(GTK_WINDOW(window),
			     _("Filter rule"));
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(prefs_filter_edit_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(prefs_filter_edit_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(prefs_filter_edit_ok), NULL);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(prefs_filter_edit_cancel), NULL);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Name:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	name_entry = gtk_entry_new();
	gtk_widget_show(name_entry);
	gtk_box_pack_start(GTK_BOX(hbox), name_entry, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	bool_op_optmenu = gtk_option_menu_new();
	gtk_widget_show(bool_op_optmenu);
	gtk_box_pack_start(GTK_BOX(hbox), bool_op_optmenu, FALSE, FALSE, 0);

	menu = gtk_menu_new();
	gtk_widget_show(menu);
	MENUITEM_ADD(menu, menuitem,
		     _("If any of the following condition matches"), FLT_OR);
	MENUITEM_ADD(menu, menuitem,
		     _("If all of the following conditions match"), FLT_AND);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(bool_op_optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(bool_op_optmenu), FLT_AND);

	cond_scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(cond_scrolled_win);
	gtk_widget_set_size_request(cond_scrolled_win, -1, 125 * gtkut_get_dpi_multiplier());
	gtk_box_pack_start(GTK_BOX(vbox), cond_scrolled_win, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cond_scrolled_win),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	cond_vbox = gtk_vbox_new(FALSE, 2);
	gtk_widget_show(cond_vbox);
	gtk_container_set_border_width(GTK_CONTAINER(cond_vbox), 2);
	gtk_scrolled_window_add_with_viewport
		(GTK_SCROLLED_WINDOW(cond_scrolled_win), cond_vbox);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Perform the following actions:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	action_scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(action_scrolled_win);
	gtk_box_pack_start(GTK_BOX(vbox), action_scrolled_win, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(action_scrolled_win),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	action_vbox = gtk_vbox_new(FALSE, 2);
	gtk_widget_show(action_vbox);
	gtk_container_set_border_width(GTK_CONTAINER(action_vbox), 2);
	gtk_scrolled_window_add_with_viewport
		(GTK_SCROLLED_WINDOW(action_scrolled_win), action_vbox);

	rule_edit_window.window = window;
	rule_edit_window.name_entry = name_entry;

	rule_edit_window.bool_op_optmenu = bool_op_optmenu;
	rule_edit_window.cond_scrolled_win = cond_scrolled_win;
	rule_edit_window.cond_edit.cond_vbox = cond_vbox;
	rule_edit_window.action_scrolled_win = action_scrolled_win;
	rule_edit_window.action_vbox = action_vbox;

	rule_edit_window.ok_btn = ok_btn;
	rule_edit_window.cancel_btn = cancel_btn;
}

FilterCondEdit *prefs_filter_edit_cond_edit_create(void)
{
	FilterCondEdit *cond_edit;
	GtkWidget *cond_vbox;

	cond_edit = g_new(FilterCondEdit, 1);

	cond_vbox = gtk_vbox_new(FALSE, 2);
	gtk_widget_show(cond_vbox);
	gtk_container_set_border_width(GTK_CONTAINER(cond_vbox), 2);

	cond_edit->cond_vbox = cond_vbox;
	cond_edit->cond_hbox_list = NULL;
	cond_edit->hdr_list = NULL;
	cond_edit->rule_hdr_list = NULL;
	cond_edit->add_hbox = NULL;

	return cond_edit;
}

void prefs_filter_edit_clear_cond_edit(FilterCondEdit *cond_edit)
{
	while (cond_edit->cond_hbox_list) {
		CondHBox *hbox = (CondHBox *)cond_edit->cond_hbox_list->data;
		prefs_filter_edit_remove_cond_hbox(cond_edit, hbox);
	}

	procheader_header_list_destroy(cond_edit->hdr_list);
	cond_edit->hdr_list = NULL;
	procheader_header_list_destroy(cond_edit->rule_hdr_list);
	cond_edit->rule_hdr_list = NULL;
}

static void prefs_filter_edit_clear(void)
{
	prefs_filter_edit_clear_cond_edit(&rule_edit_window.cond_edit);

	while (rule_edit_window.action_hbox_list) {
		ActionHBox *hbox =
			(ActionHBox *)rule_edit_window.action_hbox_list->data;
		prefs_filter_edit_remove_action_hbox(hbox);
	}
}

static void prefs_filter_edit_rule_to_dialog(FilterRule *rule,
					     const gchar *default_name)
{
	gint index = 1;
	static gint count = 1;

	if (rule && rule->name)
		gtk_entry_set_text(GTK_ENTRY(rule_edit_window.name_entry),
				   rule->name);
	else if (default_name)
		gtk_entry_set_text(GTK_ENTRY(rule_edit_window.name_entry),
				   default_name);
	else {
		gchar rule_name[32];
		g_snprintf(rule_name, sizeof(rule_name), "Rule %d", count++);
		gtk_entry_set_text(GTK_ENTRY(rule_edit_window.name_entry),
				   rule_name);
	}

	if (rule) {
		index = menu_find_option_menu_index
			(GTK_OPTION_MENU(rule_edit_window.bool_op_optmenu),
			 GINT_TO_POINTER(rule->bool_op), NULL);
		if (index < 0)
			index = 1;
	}
	if (index >= 0) {
		gtk_option_menu_set_history
			(GTK_OPTION_MENU(rule_edit_window.bool_op_optmenu),
			 index);
	}

	gtkut_scrolled_window_reset_position
		(GTK_SCROLLED_WINDOW(rule_edit_window.cond_scrolled_win));
	gtkut_scrolled_window_reset_position
		(GTK_SCROLLED_WINDOW(rule_edit_window.action_scrolled_win));

	prefs_filter_edit_add_rule_cond(&rule_edit_window.cond_edit, rule);
	prefs_filter_edit_add_rule_action(rule);
}

void prefs_filter_edit_set_header_list(FilterCondEdit *cond_edit,
				       FilterRule *rule)
{
	GSList *list;
	GSList *rule_hdr_list = NULL;
	GSList *cur;
	FilterCond *cond;

	procheader_header_list_destroy(cond_edit->hdr_list);
	cond_edit->hdr_list = NULL;
	procheader_header_list_destroy(cond_edit->rule_hdr_list);
	cond_edit->rule_hdr_list = NULL;

	list = prefs_filter_get_header_list();
	cond_edit->hdr_list = list;

	if (!rule)
		return;

	for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
		cond = (FilterCond *)cur->data;

		if (cond->type == FLT_COND_HEADER &&
		    procheader_find_header_list
			(rule_hdr_list, cond->header_name) < 0)
			rule_hdr_list = procheader_add_header_list
				(rule_hdr_list, cond->header_name, NULL);
	}

	cond_edit->rule_hdr_list = rule_hdr_list;
	cond_edit->hdr_list = procheader_merge_header_list_dup(list,
							       rule_hdr_list);
	procheader_header_list_destroy(list);
}

static void prefs_filter_edit_update_header_list(FilterCondEdit *cond_edit)
{
	GSList *list;

	procheader_header_list_destroy(cond_edit->hdr_list);
	cond_edit->hdr_list = NULL;

	list = prefs_filter_get_header_list();
	cond_edit->hdr_list = procheader_merge_header_list_dup
		(list, cond_edit->rule_hdr_list);
	procheader_header_list_destroy(list);
}

CondHBox *prefs_filter_edit_cond_hbox_create(FilterCondEdit *cond_edit)
{
	CondHBox *cond_hbox;
	GtkWidget *hbox;
	GtkWidget *cond_type_optmenu;
	GtkWidget *match_type_optmenu;
	GtkWidget *size_match_optmenu;
	GtkWidget *age_match_optmenu;
	GtkWidget *status_match_optmenu;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkWidget *key_entry;
	GtkObject *spin_btn_adj;
	GtkWidget *spin_btn;
	GtkWidget *label;
	GtkWidget *del_btn;
	GtkWidget *add_btn;
	GtkWidget *del_img;
	GtkWidget *add_img;
	GtkWidget *match_menu_in_addr;
	GtkWidget *match_menu_not_in_addr;

	cond_hbox = g_new0(CondHBox, 1);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);

	cond_type_optmenu = gtk_option_menu_new();
	gtk_widget_show(cond_type_optmenu);
	gtk_box_pack_start(GTK_BOX(hbox), cond_type_optmenu, FALSE, FALSE, 0);

#define COND_MENUITEM_ADD(str, action)					    \
{									    \
	MENUITEM_ADD(menu, menuitem, str, action);			    \
	g_signal_connect(G_OBJECT(menuitem), "activate",		    \
			 G_CALLBACK(prefs_filter_cond_activated_cb),	    \
			 cond_hbox);					    \
}

	menu = gtk_menu_new();
	gtk_widget_show(menu);
	MENUITEM_ADD(menu, menuitem, NULL,        PF_COND_SEPARATOR);
	COND_MENUITEM_ADD(_("To or Cc"),          PF_COND_TO_OR_CC);
	COND_MENUITEM_ADD(_("Any header"),        PF_COND_ANY_HEADER);
	COND_MENUITEM_ADD(_("Edit header..."),    PF_COND_EDIT_HEADER);

	MENUITEM_ADD(menu, menuitem, NULL,        PF_COND_SEPARATOR);
	COND_MENUITEM_ADD(_("Message body"),      PF_COND_BODY);
	COND_MENUITEM_ADD(_("Result of command"), PF_COND_CMD_TEST);
	COND_MENUITEM_ADD(_("Size"),              PF_COND_SIZE);
	COND_MENUITEM_ADD(_("Age"),               PF_COND_AGE);

	MENUITEM_ADD(menu, menuitem, NULL,        PF_COND_SEPARATOR);
	COND_MENUITEM_ADD(_("Unread"),            PF_COND_UNREAD);
	COND_MENUITEM_ADD(_("Marked"),            PF_COND_MARK);
	COND_MENUITEM_ADD(_("Has color label"),   PF_COND_COLOR_LABEL);
	COND_MENUITEM_ADD(_("Has attachment"),    PF_COND_MIME);
	/* COND_MENUITEM_ADD(_("Account"),           PF_COND_ACCOUNT); */

	gtk_option_menu_set_menu(GTK_OPTION_MENU(cond_type_optmenu), menu);

#undef COND_MENUITEM_ADD

	match_type_optmenu = gtk_option_menu_new();
	gtk_widget_show(match_type_optmenu);
	gtk_box_pack_start(GTK_BOX(hbox), match_type_optmenu, FALSE, FALSE, 0);

#define MATCH_MENUITEM_ADD(str, action)					    \
{									    \
	MENUITEM_ADD(menu, menuitem, str, action);			    \
	g_signal_connect(G_OBJECT(menuitem), "activate",		    \
			 G_CALLBACK(prefs_filter_match_activated_cb),	    \
			 cond_hbox);					    \
}
	menu = gtk_menu_new();
	gtk_widget_show(menu);
	MATCH_MENUITEM_ADD(_("contains"),               PF_MATCH_CONTAIN);
	MATCH_MENUITEM_ADD(_("doesn't contain"),        PF_MATCH_NOT_CONTAIN);
	MATCH_MENUITEM_ADD(_("is"),                     PF_MATCH_EQUAL);
	MATCH_MENUITEM_ADD(_("is not"),                 PF_MATCH_NOT_EQUAL);
#if defined(USE_ONIGURUMA) || defined(HAVE_REGCOMP)
	MATCH_MENUITEM_ADD(_("match to regex"),         PF_MATCH_REGEX);
	MATCH_MENUITEM_ADD(_("doesn't match to regex"), PF_MATCH_NOT_REGEX);
#endif
	MATCH_MENUITEM_ADD(_("is in addressbook"),      PF_MATCH_IN_ADDRESSBOOK);
	match_menu_in_addr = menuitem;
	MATCH_MENUITEM_ADD(_("is not in addressbook"),  PF_MATCH_NOT_IN_ADDRESSBOOK);
	match_menu_not_in_addr = menuitem;
	gtk_option_menu_set_menu(GTK_OPTION_MENU(match_type_optmenu), menu);

#undef MATCH_MENUITEM_ADD

	size_match_optmenu = gtk_option_menu_new();
	gtk_widget_show(size_match_optmenu);
	gtk_box_pack_start(GTK_BOX(hbox), size_match_optmenu, FALSE, FALSE, 0);

	menu = gtk_menu_new();
	gtk_widget_show(menu);
	MENUITEM_ADD(menu, menuitem, _("is larger than"), PF_SIZE_LARGER);
	MENUITEM_ADD(menu, menuitem, _("is smaller than"), PF_SIZE_SMALLER);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(size_match_optmenu), menu);

	age_match_optmenu = gtk_option_menu_new();
	gtk_widget_show(age_match_optmenu);
	gtk_box_pack_start(GTK_BOX(hbox), age_match_optmenu, FALSE, FALSE, 0);

	menu = gtk_menu_new();
	gtk_widget_show(menu);
	MENUITEM_ADD(menu, menuitem, _("is shorter than"), PF_AGE_SHORTER);
	MENUITEM_ADD(menu, menuitem, _("is longer than"), PF_AGE_LONGER);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(age_match_optmenu), menu);

	status_match_optmenu = gtk_option_menu_new();
	gtk_widget_show(status_match_optmenu);
	gtk_box_pack_start(GTK_BOX(hbox), status_match_optmenu,
			   FALSE, FALSE, 0);

	menu = gtk_menu_new();
	gtk_widget_show(menu);
	MENUITEM_ADD(menu, menuitem, _("matches to status"), PF_STATUS_MATCH);
	MENUITEM_ADD(menu, menuitem, _("doesn't match to status"),
		     PF_STATUS_NOT_MATCH);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(status_match_optmenu), menu);

	key_entry = gtk_entry_new();
	gtk_widget_show(key_entry);
	gtk_box_pack_start(GTK_BOX(hbox), key_entry, TRUE, TRUE, 0);

	spin_btn_adj = gtk_adjustment_new(0, 0, 99999, 1, 10, 0);
	spin_btn = gtk_spin_button_new(GTK_ADJUSTMENT(spin_btn_adj), 1, 0);
	gtk_box_pack_start(GTK_BOX(hbox), spin_btn, FALSE, FALSE, 0);
	gtk_widget_set_size_request(spin_btn, 64, -1);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_btn), TRUE);

	label = gtk_label_new(_("KB"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	del_img = gtk_image_new_from_stock
		(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
	gtk_widget_show(del_img);

	del_btn = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(del_btn), del_img);
	gtk_widget_show(del_btn);
	gtk_box_pack_end(GTK_BOX(hbox), del_btn, FALSE, FALSE, 0);

	add_img = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
	gtk_widget_show(add_img);

	add_btn = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(add_btn), add_img);
	gtk_widget_show(add_btn);
	gtk_box_pack_end(GTK_BOX(hbox), add_btn, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(prefs_filter_cond_del_cb), cond_hbox);
	g_signal_connect(G_OBJECT(add_btn), "clicked",
			 G_CALLBACK(prefs_filter_cond_add_cb), cond_hbox);

	cond_hbox->hbox = hbox;
	cond_hbox->cond_type_optmenu = cond_type_optmenu;
	cond_hbox->match_type_optmenu = match_type_optmenu;
	cond_hbox->size_match_optmenu = size_match_optmenu;
	cond_hbox->age_match_optmenu = age_match_optmenu;
	cond_hbox->status_match_optmenu = status_match_optmenu;
	cond_hbox->key_entry = key_entry;
	cond_hbox->spin_btn = spin_btn;
	cond_hbox->label = label;
	cond_hbox->match_menu_in_addr = match_menu_in_addr;
	cond_hbox->match_menu_not_in_addr = match_menu_not_in_addr;
	cond_hbox->del_btn = del_btn;
	cond_hbox->add_btn = add_btn;
	cond_hbox->cur_type = PF_COND_HEADER;
	cond_hbox->cur_header_name = NULL;
	cond_hbox->cond_edit = cond_edit;

	prefs_filter_edit_set_cond_header_menu(cond_edit, cond_hbox);
	gtk_option_menu_set_history(GTK_OPTION_MENU(cond_type_optmenu), 0);

	return cond_hbox;
}

ActionHBox *prefs_filter_edit_action_hbox_create(void)
{
	ActionHBox *action_hbox;
	GtkWidget *hbox;
	GtkWidget *action_type_optmenu;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkWidget *label;
	GtkWidget *folder_entry;
	GtkWidget *cmd_entry;
	GtkWidget *address_entry;
	GtkWidget *folder_img;
	GtkWidget *folder_sel_btn;
	GtkWidget *clabel_optmenu;
	GtkWidget *del_btn;
	GtkWidget *add_btn;
	GtkWidget *del_img;
	GtkWidget *add_img;

	action_hbox = g_new0(ActionHBox, 1);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);

	action_type_optmenu = gtk_option_menu_new();
	gtk_widget_show(action_type_optmenu);
	gtk_box_pack_start(GTK_BOX(hbox), action_type_optmenu, FALSE, FALSE, 0);

	memset(action_hbox->action_type_menu_items, 0,
	       sizeof(action_hbox->action_type_menu_items));

#define ACTION_MENUITEM_ADD(str, action)				      \
{									      \
	MENUITEM_ADD(menu, menuitem, str, action);			      \
	action_hbox->action_type_menu_items[action] = menuitem;		      \
	g_signal_connect(G_OBJECT(menuitem), "activate",		      \
			 G_CALLBACK(prefs_filter_action_activated_cb),	      \
			 action_hbox);					      \
}

	menu = gtk_menu_new();
	gtk_widget_show(menu);
	ACTION_MENUITEM_ADD(_("Move to"),               PF_ACTION_MOVE);
	ACTION_MENUITEM_ADD(_("Copy to"),               PF_ACTION_COPY);
	ACTION_MENUITEM_ADD(_("Don't receive"),         PF_ACTION_NOT_RECEIVE);
	ACTION_MENUITEM_ADD(_("Delete from server"),    PF_ACTION_DELETE);

	MENUITEM_ADD(menu, menuitem, NULL,              PF_ACTION_SEPARATOR);
	ACTION_MENUITEM_ADD(_("Set mark"),              PF_ACTION_MARK);
	ACTION_MENUITEM_ADD(_("Set color"),             PF_ACTION_COLOR_LABEL);
	ACTION_MENUITEM_ADD(_("Mark as read"),          PF_ACTION_MARK_READ);

#if 0
	MENUITEM_ADD(menu, menuitem, NULL,              PF_ACTION_SEPARATOR);
	ACTION_MENUITEM_ADD(_("Forward"),               PF_ACTION_FORWARD);
	ACTION_MENUITEM_ADD(_("Forward as attachment"), PF_ACTION_FORWARD_AS_ATTACHMENT);
	ACTION_MENUITEM_ADD(_("Redirect"),              PF_ACTION_REDIRECT);
#endif

	MENUITEM_ADD(menu, menuitem, NULL,              PF_ACTION_SEPARATOR);
	ACTION_MENUITEM_ADD(_("Execute command"),       PF_ACTION_EXEC);

	MENUITEM_ADD(menu, menuitem, NULL,              PF_ACTION_SEPARATOR);
	ACTION_MENUITEM_ADD(_("Stop rule evaluation"),  PF_ACTION_STOP_EVAL);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(action_type_optmenu), menu);

#undef ACTION_MENUITEM_ADD

	label = gtk_label_new(_("folder:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	folder_entry = gtk_entry_new();
	gtk_widget_show(folder_entry);
	gtk_box_pack_start(GTK_BOX(hbox), folder_entry, TRUE, TRUE, 0);

	folder_img = stock_pixbuf_widget(rule_edit_window.window,
					 STOCK_PIXMAP_FOLDER_OPEN);
	gtk_widget_show(folder_img);

	folder_sel_btn = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(folder_sel_btn), folder_img);
	gtk_widget_show(folder_sel_btn);
	gtk_box_pack_start(GTK_BOX(hbox), folder_sel_btn, FALSE, FALSE, 0);

	cmd_entry = gtk_entry_new();
	gtk_widget_show(cmd_entry);
	gtk_box_pack_start(GTK_BOX(hbox), cmd_entry, TRUE, TRUE, 0);

	address_entry = gtk_entry_new();
	gtk_widget_show(address_entry);
	gtk_box_pack_start(GTK_BOX(hbox), address_entry, TRUE, TRUE, 0);

	clabel_optmenu = gtk_option_menu_new();
	gtk_widget_show(clabel_optmenu);
	gtk_box_pack_start(GTK_BOX(hbox), clabel_optmenu, FALSE, FALSE, 0);

	menu = colorlabel_create_color_menu();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(clabel_optmenu), menu);

	del_img = gtk_image_new_from_stock
		(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
	gtk_widget_show(del_img);

	del_btn = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(del_btn), del_img);
	gtk_widget_show(del_btn);
	gtk_box_pack_end(GTK_BOX(hbox), del_btn, FALSE, FALSE, 0);

	add_img = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
	gtk_widget_show(add_img);

	add_btn = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(add_btn), add_img);
	gtk_widget_show(add_btn);
	gtk_box_pack_end(GTK_BOX(hbox), add_btn, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(folder_sel_btn), "clicked",
			 G_CALLBACK(prefs_filter_action_select_dest_cb),
			 action_hbox);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(prefs_filter_action_del_cb), action_hbox);
	g_signal_connect(G_OBJECT(add_btn), "clicked",
			 G_CALLBACK(prefs_filter_action_add_cb), action_hbox);

	action_hbox->hbox = hbox;
	action_hbox->action_type_optmenu = action_type_optmenu;
	action_hbox->label = label;
	action_hbox->folder_entry = folder_entry;
	action_hbox->cmd_entry = cmd_entry;
	action_hbox->address_entry = address_entry;
	action_hbox->folder_sel_btn = folder_sel_btn;
	action_hbox->clabel_optmenu = clabel_optmenu;
	action_hbox->del_btn = del_btn;
	action_hbox->add_btn = add_btn;

	return action_hbox;
}

void prefs_filter_edit_cond_hbox_set(CondHBox *hbox, FilterCond *cond)
{
	GtkOptionMenu *cond_type_optmenu =
		GTK_OPTION_MENU(hbox->cond_type_optmenu);
	GtkOptionMenu *match_type_optmenu =
		GTK_OPTION_MENU(hbox->match_type_optmenu);
	gint cond_index = -1;
	gint match_index = -1;
	CondMenuType cond_type = PF_COND_NONE;
	MatchMenuType match_type = PF_MATCH_NONE;
	SizeMatchType size_type = PF_SIZE_LARGER;
	AgeMatchType age_type = PF_AGE_SHORTER;
	StatusMatchType status_type = PF_STATUS_MATCH;

	switch (cond->type) {
	case FLT_COND_HEADER:
		cond_type = PF_COND_HEADER; break;
	case FLT_COND_TO_OR_CC:
		cond_type = PF_COND_TO_OR_CC; break;
	case FLT_COND_ANY_HEADER:
		cond_type = PF_COND_ANY_HEADER; break;
	case FLT_COND_BODY:
		cond_type = PF_COND_BODY; break;
	case FLT_COND_CMD_TEST:
		cond_type = PF_COND_CMD_TEST; break;
	case FLT_COND_SIZE_GREATER:
		cond_type = PF_COND_SIZE;
		if (FLT_IS_NOT_MATCH(cond->match_flag))
			size_type = PF_SIZE_SMALLER;
		else
			size_type = PF_SIZE_LARGER;
		break;
	case FLT_COND_AGE_GREATER:
		cond_type = PF_COND_AGE;
		if (FLT_IS_NOT_MATCH(cond->match_flag))
			age_type = PF_AGE_SHORTER;
		else
			age_type = PF_AGE_LONGER;
		break;
	case FLT_COND_UNREAD:
		cond_type = PF_COND_UNREAD; break;
	case FLT_COND_MARK:
		cond_type = PF_COND_MARK; break;
	case FLT_COND_COLOR_LABEL:
		cond_type = PF_COND_COLOR_LABEL; break;
	case FLT_COND_MIME:
		cond_type = PF_COND_MIME; break;
	case FLT_COND_ACCOUNT:
		cond_type = PF_COND_ACCOUNT; break;
	default:
		break;
	}

	switch (cond->type) {
	case FLT_COND_HEADER:
	case FLT_COND_TO_OR_CC:
	case FLT_COND_ANY_HEADER:
	case FLT_COND_BODY:
		switch (cond->match_type) {
		case FLT_CONTAIN:
			if (FLT_IS_NOT_MATCH(cond->match_flag))
				match_type = PF_MATCH_NOT_CONTAIN;
			else
				match_type = PF_MATCH_CONTAIN;
			break;
		case FLT_EQUAL:
			if (FLT_IS_NOT_MATCH(cond->match_flag))
				match_type = PF_MATCH_NOT_EQUAL;
			else
				match_type = PF_MATCH_EQUAL;
			break;
		case FLT_REGEX:
			if (FLT_IS_NOT_MATCH(cond->match_flag))
				match_type = PF_MATCH_NOT_REGEX;
			else
				match_type = PF_MATCH_REGEX;
			break;
		case FLT_IN_ADDRESSBOOK:
			if (FLT_IS_NOT_MATCH(cond->match_flag))
				match_type = PF_MATCH_NOT_IN_ADDRESSBOOK;
			else
				match_type = PF_MATCH_IN_ADDRESSBOOK;
			break;
		}
		break;
	case FLT_COND_UNREAD:
	case FLT_COND_MARK:
	case FLT_COND_COLOR_LABEL:
	case FLT_COND_MIME:
		if (FLT_IS_NOT_MATCH(cond->match_flag))
			status_type = PF_STATUS_NOT_MATCH;
		else
			status_type = PF_STATUS_MATCH;
		break;
	default:
		break;
	}

	if (cond_type == PF_COND_HEADER)
		cond_index = procheader_find_header_list
			(hbox->cond_edit->hdr_list, cond->header_name);
	else
		cond_index = menu_find_option_menu_index
			(cond_type_optmenu, GINT_TO_POINTER(cond_type), NULL);
	if (cond_index >= 0) {
		GtkWidget *menuitem;

		if (cond_type == PF_COND_SIZE || cond_type == PF_COND_AGE) {
			gtk_spin_button_set_value
				(GTK_SPIN_BUTTON(hbox->spin_btn),
				 (gfloat)cond->int_value);
		} else {
			gtk_entry_set_text(GTK_ENTRY(hbox->key_entry),
					   cond->str_value ?
					   cond->str_value : "");
		}
		gtk_option_menu_set_history(cond_type_optmenu, cond_index);
		menuitem = gtk_menu_get_active
			(GTK_MENU(gtk_option_menu_get_menu(cond_type_optmenu)));
		gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
	}

	match_index = menu_find_option_menu_index
		(match_type_optmenu, GINT_TO_POINTER(match_type), NULL);
	if (match_index >= 0)
		gtk_option_menu_set_history(match_type_optmenu, match_index);
	if (cond_type == PF_COND_SIZE)
		gtk_option_menu_set_history
			(GTK_OPTION_MENU(hbox->size_match_optmenu), size_type);
	else if (cond_type == PF_COND_AGE)
		gtk_option_menu_set_history
			(GTK_OPTION_MENU(hbox->age_match_optmenu), age_type);
	else if (cond_type == PF_COND_UNREAD || cond_type == PF_COND_MARK ||
		 cond_type == PF_COND_COLOR_LABEL || cond_type == PF_COND_MIME)
		gtk_option_menu_set_history
			(GTK_OPTION_MENU(hbox->status_match_optmenu),
			 status_type);

	if (match_type == PF_MATCH_IN_ADDRESSBOOK ||
	    match_type == PF_MATCH_NOT_IN_ADDRESSBOOK)
		gtk_widget_hide(hbox->key_entry);
}

void prefs_filter_edit_action_hbox_set(ActionHBox *hbox, FilterAction *action)
{
	GtkOptionMenu *type_optmenu = GTK_OPTION_MENU(hbox->action_type_optmenu);
	GtkWidget *menu;
	ActionMenuType type = PF_ACTION_NONE;

	menu = gtk_option_menu_get_menu(type_optmenu);

	switch (action->type) {
	case FLT_ACTION_MOVE:
		type = PF_ACTION_MOVE; break;
	case FLT_ACTION_COPY:
		type = PF_ACTION_COPY; break;
	case FLT_ACTION_NOT_RECEIVE:
		type = PF_ACTION_NOT_RECEIVE; break;
	case FLT_ACTION_DELETE:
		type = PF_ACTION_DELETE; break;
	case FLT_ACTION_EXEC:
		type = PF_ACTION_EXEC; break;
	case FLT_ACTION_MARK:
		type = PF_ACTION_MARK; break;
	case FLT_ACTION_COLOR_LABEL:
		type = PF_ACTION_COLOR_LABEL; break;
	case FLT_ACTION_MARK_READ:
		type = PF_ACTION_MARK_READ; break;
	case FLT_ACTION_STOP_EVAL:
		type = PF_ACTION_STOP_EVAL; break;
	default:
		break;
	}

	switch (type) {
	case PF_ACTION_MOVE:
	case PF_ACTION_COPY:
		gtk_entry_set_text(GTK_ENTRY(hbox->folder_entry),
				   action->str_value ? action->str_value : "");
		break;
	case PF_ACTION_EXEC:
		gtk_entry_set_text(GTK_ENTRY(hbox->cmd_entry),
				   action->str_value ? action->str_value : "");
		break;
	case PF_ACTION_COLOR_LABEL:
		gtk_option_menu_set_history
			(GTK_OPTION_MENU(hbox->clabel_optmenu),
			 action->int_value - 1);
		break;
	default:
		break;
	}

	prefs_filter_edit_set_action_hbox_widgets(hbox, type);
}

void prefs_filter_edit_cond_hbox_select(CondHBox *hbox, CondMenuType type,
					const gchar *header_name)
{
	gint index;
	GtkOptionMenu *cond_type_optmenu =
		GTK_OPTION_MENU(hbox->cond_type_optmenu);
	GtkWidget *menu;

	if (type == PF_COND_HEADER) {
		if (header_name)
			index = procheader_find_header_list
				(hbox->cond_edit->hdr_list, header_name);
		else
			index = 0;
	} else
		index = menu_find_option_menu_index
			(cond_type_optmenu, GINT_TO_POINTER(type), NULL);

	if (index < 0)
		index = 0;

	menu = gtk_option_menu_get_menu(cond_type_optmenu);
	gtk_option_menu_set_history(cond_type_optmenu, index);
}

void prefs_filter_edit_set_cond_hbox_widgets(CondHBox *hbox, CondMenuType type)
{
	MatchMenuType match_type;

	switch (type) {
	case PF_COND_HEADER:
	case PF_COND_TO_OR_CC:
	case PF_COND_ANY_HEADER:
	case PF_COND_BODY:
		gtk_widget_show(hbox->match_type_optmenu);
		gtk_widget_hide(hbox->size_match_optmenu);
		gtk_widget_hide(hbox->age_match_optmenu);
		gtk_widget_hide(hbox->status_match_optmenu);
		match_type = menu_get_option_menu_active_index
			(GTK_OPTION_MENU(hbox->match_type_optmenu));
		if (match_type == PF_MATCH_IN_ADDRESSBOOK ||
		    match_type == PF_MATCH_NOT_IN_ADDRESSBOOK)
			gtk_widget_hide(hbox->key_entry);
		else
			gtk_widget_show(hbox->key_entry);
		gtk_widget_hide(hbox->spin_btn);
		gtk_widget_hide(hbox->label);
		if (type == PF_COND_HEADER || type == PF_COND_TO_OR_CC) {
			gtk_widget_show(hbox->match_menu_in_addr);
			gtk_widget_show(hbox->match_menu_not_in_addr);
		} else {
			gtk_widget_hide(hbox->match_menu_in_addr);
			gtk_widget_hide(hbox->match_menu_not_in_addr);
			if (match_type == PF_MATCH_IN_ADDRESSBOOK ||
			    match_type == PF_MATCH_NOT_IN_ADDRESSBOOK) {
				gtk_option_menu_set_history(GTK_OPTION_MENU(hbox->match_type_optmenu), 0);
				gtk_widget_show(hbox->key_entry);
			}
		}
		break;
	case PF_COND_CMD_TEST:
		gtk_widget_hide(hbox->match_type_optmenu);
		gtk_widget_hide(hbox->size_match_optmenu);
		gtk_widget_hide(hbox->age_match_optmenu);
		gtk_widget_hide(hbox->status_match_optmenu);
		gtk_widget_show(hbox->key_entry);
		gtk_widget_hide(hbox->spin_btn);
		gtk_widget_hide(hbox->label);
		break;
	case PF_COND_SIZE:
		gtk_widget_hide(hbox->match_type_optmenu);
		gtk_widget_show(hbox->size_match_optmenu);
		gtk_widget_hide(hbox->age_match_optmenu);
		gtk_widget_hide(hbox->status_match_optmenu);
		gtk_widget_hide(hbox->key_entry);
		gtk_widget_show(hbox->spin_btn);
		gtk_widget_show(hbox->label);
		gtk_label_set_text(GTK_LABEL(hbox->label), _("KB"));
		break;
	case PF_COND_AGE:
		gtk_widget_hide(hbox->match_type_optmenu);
		gtk_widget_hide(hbox->size_match_optmenu);
		gtk_widget_show(hbox->age_match_optmenu);
		gtk_widget_hide(hbox->status_match_optmenu);
		gtk_widget_hide(hbox->key_entry);
		gtk_widget_show(hbox->spin_btn);
		gtk_widget_show(hbox->label);
		gtk_label_set_text(GTK_LABEL(hbox->label), _("day(s)"));
		break;
	case PF_COND_UNREAD:
	case PF_COND_MARK:
	case PF_COND_COLOR_LABEL:
	case PF_COND_MIME:
		gtk_widget_hide(hbox->match_type_optmenu);
		gtk_widget_hide(hbox->size_match_optmenu);
		gtk_widget_hide(hbox->age_match_optmenu);
		gtk_widget_show(hbox->status_match_optmenu);
		gtk_widget_hide(hbox->key_entry);
		gtk_widget_hide(hbox->spin_btn);
		gtk_widget_hide(hbox->label);
		break;
	case PF_COND_ACCOUNT:
		gtk_widget_hide(hbox->match_type_optmenu);
		gtk_widget_hide(hbox->size_match_optmenu);
		gtk_widget_hide(hbox->age_match_optmenu);
		gtk_widget_hide(hbox->status_match_optmenu);
		gtk_widget_hide(hbox->key_entry);
		/* gtk_widget_show(hbox->account_optmenu); */
		gtk_widget_hide(hbox->spin_btn);
		gtk_widget_hide(hbox->label);
		break;
	default:
		break;
	}
}

void prefs_filter_edit_set_action_hbox_widgets(ActionHBox *hbox,
					       ActionMenuType type)
{
	GtkOptionMenu *type_optmenu = GTK_OPTION_MENU(hbox->action_type_optmenu);
	gint index;

	switch (type) {
	case PF_ACTION_MOVE:
	case PF_ACTION_COPY:
		gtk_widget_show(hbox->label);
		gtk_label_set_text(GTK_LABEL(hbox->label), _("folder:"));
		gtk_widget_show(hbox->folder_entry);
		gtk_widget_show(hbox->folder_sel_btn);
		gtk_widget_hide(hbox->cmd_entry);
		gtk_widget_hide(hbox->address_entry);
		gtk_widget_hide(hbox->clabel_optmenu);
		break;
	case PF_ACTION_NOT_RECEIVE:
	case PF_ACTION_DELETE:
	case PF_ACTION_MARK:
	case PF_ACTION_MARK_READ:
	case PF_ACTION_STOP_EVAL:
		gtk_widget_hide(hbox->label);
		gtk_widget_hide(hbox->folder_entry);
		gtk_widget_hide(hbox->folder_sel_btn);
		gtk_widget_hide(hbox->cmd_entry);
		gtk_widget_hide(hbox->address_entry);
		gtk_widget_hide(hbox->clabel_optmenu);
		break;
	case PF_ACTION_EXEC:
	case PF_ACTION_EXEC_ASYNC:
		gtk_widget_hide(hbox->label);
		gtk_widget_hide(hbox->folder_entry);
		gtk_widget_hide(hbox->folder_sel_btn);
		gtk_widget_show(hbox->cmd_entry);
		gtk_widget_hide(hbox->address_entry);
		gtk_widget_hide(hbox->clabel_optmenu);
		break;
	case PF_ACTION_COLOR_LABEL:
		gtk_widget_hide(hbox->label);
		gtk_widget_hide(hbox->folder_entry);
		gtk_widget_hide(hbox->folder_sel_btn);
		gtk_widget_hide(hbox->cmd_entry);
		gtk_widget_hide(hbox->address_entry);
		gtk_widget_show(hbox->clabel_optmenu);
		break;
	case PF_ACTION_FORWARD:
	case PF_ACTION_FORWARD_AS_ATTACHMENT:
	case PF_ACTION_REDIRECT:
		gtk_widget_show(hbox->label);
		gtk_label_set_text(GTK_LABEL(hbox->label), _("address:"));
		gtk_widget_hide(hbox->folder_entry);
		gtk_widget_hide(hbox->folder_sel_btn);
		gtk_widget_hide(hbox->cmd_entry);
		gtk_widget_show(hbox->address_entry);
		gtk_widget_hide(hbox->clabel_optmenu);
		break;
	default:
		break;
	}

	index = menu_find_option_menu_index(type_optmenu, GINT_TO_POINTER(type),
					    NULL);
	gtk_option_menu_set_history(type_optmenu, index);
	prefs_filter_edit_set_action_hbox_menus_sensitive();
}

static void prefs_filter_edit_set_action_hbox_menu_sensitive
	(ActionHBox *hbox, ActionMenuType type, gboolean sensitive)
{
	GtkWidget *menuitem;

	menuitem = hbox->action_type_menu_items[type];
	if (menuitem)
		gtk_widget_set_sensitive(menuitem, sensitive);
}

static void prefs_filter_edit_set_action_hbox_menus_sensitive(void)
{
	GSList *cur;
	ActionHBox *cur_hbox;
	ActionMenuType menu_type;
	ActionMenuType cur_type;
	gboolean action_menu_selection[PF_ACTION_NONE];
	gboolean action_menu_sensitive[PF_ACTION_NONE];

	prefs_filter_edit_get_action_hbox_menus_selection
		(action_menu_selection);

	for (cur = rule_edit_window.action_hbox_list; cur != NULL;
	     cur = cur->next) {
		cur_hbox = (ActionHBox *)cur->data;
		menu_type = prefs_filter_edit_get_action_hbox_type(cur_hbox);
		for (cur_type = PF_ACTION_MOVE; cur_type < PF_ACTION_NONE;
		     cur_type++)
			action_menu_sensitive[cur_type] = TRUE;

		for (cur_type = PF_ACTION_MOVE; cur_type < PF_ACTION_NONE;
		     cur_type++) {
			switch (cur_type) {
			case PF_ACTION_MOVE:
			case PF_ACTION_NOT_RECEIVE:
			case PF_ACTION_DELETE:
				if (action_menu_selection[cur_type] == TRUE &&
				    menu_type != cur_type) {
					action_menu_sensitive[PF_ACTION_MOVE] = FALSE;
					action_menu_sensitive[PF_ACTION_NOT_RECEIVE] = FALSE;
					action_menu_sensitive[PF_ACTION_DELETE] = FALSE;
				}
				break;
			case PF_ACTION_MARK:
			case PF_ACTION_COLOR_LABEL:
			case PF_ACTION_MARK_READ:
			case PF_ACTION_STOP_EVAL:
				if (action_menu_selection[cur_type] == TRUE &&
				    menu_type != cur_type)
					action_menu_sensitive[cur_type] = FALSE;
				break;
			default:
				break;
			}
		}

		for (cur_type = PF_ACTION_MOVE; cur_type < PF_ACTION_NONE;
		     cur_type++) {
			prefs_filter_edit_set_action_hbox_menu_sensitive
				(cur_hbox, cur_type,
				 action_menu_sensitive[cur_type]);
		}
	}
}

static void prefs_filter_edit_get_action_hbox_menus_selection(gboolean *selection)
{
	GSList *cur;
	ActionHBox *cur_hbox;
	ActionMenuType menu_type;
	ActionMenuType cur_type;

	for (cur_type = PF_ACTION_MOVE; cur_type < PF_ACTION_NONE; cur_type++)
		selection[cur_type] = FALSE;

	for (cur = rule_edit_window.action_hbox_list; cur != NULL;
	     cur = cur->next) {
		cur_hbox = (ActionHBox *)cur->data;
		menu_type = prefs_filter_edit_get_action_hbox_type(cur_hbox);
		if (menu_type >= PF_ACTION_MOVE && menu_type < PF_ACTION_NONE)
			selection[menu_type] = TRUE;
	}
}

static ActionMenuType prefs_filter_edit_get_action_hbox_type(ActionHBox *hbox)
{
	GtkWidget *menuitem;
	ActionMenuType type;

	g_return_val_if_fail(hbox != NULL, PF_ACTION_NONE);

	menuitem = gtk_menu_get_active
		(GTK_MENU(gtk_option_menu_get_menu
			(GTK_OPTION_MENU(hbox->action_type_optmenu))));
	type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem),
						 MENU_VAL_ID));

	return type;
}

void prefs_filter_edit_insert_cond_hbox(FilterCondEdit *cond_edit,
					CondHBox *hbox, gint pos)
{
	g_return_if_fail(cond_edit != NULL);
	g_return_if_fail(hbox != NULL);

	if (!cond_edit->cond_hbox_list) {
		gtk_widget_set_sensitive(hbox->del_btn, FALSE);
	} else if (cond_edit->cond_hbox_list &&
		   !cond_edit->cond_hbox_list->next) {
		CondHBox *top_hbox =
			(CondHBox *)cond_edit->cond_hbox_list->data;
		gtk_widget_set_sensitive(top_hbox->del_btn, TRUE);
	}

	gtk_box_pack_start(GTK_BOX(cond_edit->cond_vbox),
			   hbox->hbox, FALSE, FALSE, 0);
	if (pos >= 0) {
		gtk_box_reorder_child(GTK_BOX(cond_edit->cond_vbox),
				      hbox->hbox, pos);
	}

	cond_edit->cond_hbox_list =
		g_slist_insert(cond_edit->cond_hbox_list, hbox, pos);
}

static void prefs_filter_edit_insert_action_hbox(ActionHBox *hbox, gint pos)
{
	g_return_if_fail(hbox != NULL);

	if (!rule_edit_window.action_hbox_list) {
		gtk_widget_set_sensitive(hbox->del_btn, FALSE);
	} else if (rule_edit_window.action_hbox_list &&
		   !rule_edit_window.action_hbox_list->next) {
		ActionHBox *top_hbox =
			(ActionHBox *)rule_edit_window.action_hbox_list->data;
		gtk_widget_set_sensitive(top_hbox->del_btn, TRUE);
	}

	gtk_box_pack_start(GTK_BOX(rule_edit_window.action_vbox),
			   hbox->hbox, FALSE, FALSE, 0);
	if (pos >= 0) {
		gtk_box_reorder_child(GTK_BOX(rule_edit_window.action_vbox),
				      hbox->hbox, pos);
	}

	rule_edit_window.action_hbox_list =
		g_slist_insert(rule_edit_window.action_hbox_list, hbox, pos);
}

static void prefs_filter_edit_remove_cond_hbox(FilterCondEdit *cond_edit,
					       CondHBox *hbox)
{
	g_return_if_fail(cond_edit != NULL);
	g_return_if_fail(hbox != NULL);
	g_return_if_fail(cond_edit->cond_hbox_list != NULL);

	cond_edit->cond_hbox_list =
		g_slist_remove(cond_edit->cond_hbox_list, hbox);
	gtk_widget_destroy(hbox->hbox);
	g_free(hbox);

	if (cond_edit->cond_hbox_list && !cond_edit->cond_hbox_list->next) {
		hbox = (CondHBox *)cond_edit->cond_hbox_list->data;
		gtk_widget_set_sensitive(hbox->del_btn, FALSE);
	}
}

static void prefs_filter_edit_remove_action_hbox(ActionHBox *hbox)
{
	g_return_if_fail(hbox != NULL);
	g_return_if_fail(rule_edit_window.action_hbox_list != NULL);

	rule_edit_window.action_hbox_list =
		g_slist_remove(rule_edit_window.action_hbox_list, hbox);
	gtk_widget_destroy(hbox->hbox);
	g_free(hbox);

	prefs_filter_edit_set_action_hbox_menus_sensitive();

	if (rule_edit_window.action_hbox_list &&
	    !rule_edit_window.action_hbox_list->next) {
		hbox = (ActionHBox *)rule_edit_window.action_hbox_list->data;
		gtk_widget_set_sensitive(hbox->del_btn, FALSE);
	}
}

void prefs_filter_edit_add_rule_cond(FilterCondEdit *cond_edit,
				     FilterRule *rule)
{
	CondHBox *hbox;
	GSList *cur;
	FilterCond *cond;

	if (!rule || !rule->cond_list) {
		hbox = prefs_filter_edit_cond_hbox_create(cond_edit);
		prefs_filter_edit_set_cond_hbox_widgets(hbox, PF_COND_HEADER);
		prefs_filter_edit_insert_cond_hbox(cond_edit, hbox, -1);
		if (cond_edit->add_hbox)
			cond_edit->add_hbox(hbox);
		return;
	}

	for (cur = rule->cond_list; cur != NULL; cur = cur->next) {
		cond = (FilterCond *)cur->data;

		hbox = prefs_filter_edit_cond_hbox_create(cond_edit);
		prefs_filter_edit_cond_hbox_set(hbox, cond);
		prefs_filter_edit_insert_cond_hbox(cond_edit, hbox, -1);
		if (cond_edit->add_hbox)
			cond_edit->add_hbox(hbox);
	}
}

static void prefs_filter_edit_add_rule_action(FilterRule *rule)
{
	ActionHBox *hbox;
	GSList *cur;

	if (!rule || !rule->action_list) {
		hbox = prefs_filter_edit_action_hbox_create();
		prefs_filter_edit_insert_action_hbox(hbox, -1);
		prefs_filter_edit_set_action_hbox_widgets(hbox, PF_ACTION_MOVE);
		return;
	}

	for (cur = rule->action_list; cur != NULL; cur = cur->next) {
		FilterAction *action = (FilterAction *)cur->data;

		hbox = prefs_filter_edit_action_hbox_create();
		prefs_filter_edit_insert_action_hbox(hbox, -1);
		prefs_filter_edit_action_hbox_set(hbox, action);
	}
}

static void prefs_filter_edit_set_cond_header_menu(FilterCondEdit *cond_edit,
						   CondHBox *hbox)
{
	GSList *cur;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gint pos = 0;
	GList *child;

	menu = gtk_option_menu_get_menu
		(GTK_OPTION_MENU(hbox->cond_type_optmenu));

	/* destroy header items */
	child = GTK_MENU_SHELL(menu)->children;
	while (child != NULL) {
		GList *next = child->next;
		menuitem = GTK_WIDGET(child->data);
		if (!g_object_get_data(G_OBJECT(menuitem), "header_str"))
			break;
		gtk_widget_destroy(menuitem);
		child = next;
	}

	for (cur = cond_edit->hdr_list; cur != NULL; cur = cur->next, pos++) {
		Header *header = (Header *)cur->data;

		menuitem = gtk_menu_item_new_with_label(header->name);
		gtk_widget_show(menuitem);
		gtk_menu_insert(GTK_MENU(menu), menuitem, pos);
		g_object_set_data(G_OBJECT(menuitem), MENU_VAL_ID,
				  GINT_TO_POINTER(PF_COND_HEADER));
		g_object_set_data(G_OBJECT(menuitem), "header_str",
				  header->name);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(prefs_filter_cond_activated_cb),
				 hbox);
	}

	if (hbox->cur_type == PF_COND_HEADER)
		prefs_filter_edit_cond_hbox_select
			(hbox, hbox->cur_type, hbox->cur_header_name);
}

static void prefs_filter_edit_activate_cond_header(FilterCondEdit *cond_edit,
						   const gchar *header)
{
	gint index;
	CondHBox *hbox;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GList *cur;
	gchar *menu_header;

	g_return_if_fail(header != NULL);
	g_return_if_fail(cond_edit != NULL);
	g_return_if_fail(cond_edit->cond_hbox_list != NULL);

	hbox = (CondHBox *)cond_edit->cond_hbox_list->data;
	menu = gtk_option_menu_get_menu
		(GTK_OPTION_MENU(hbox->cond_type_optmenu));

	for (cur = GTK_MENU_SHELL(menu)->children, index = 0;
	     cur != NULL; cur = cur->next, index++) {
		menuitem = GTK_WIDGET(cur->data);
		menu_header = g_object_get_data(G_OBJECT(menuitem),
						"header_str");
		if (!menu_header)
			break;
		if (!g_ascii_strcasecmp(menu_header, header)) {
			gtk_option_menu_set_history
				(GTK_OPTION_MENU(hbox->cond_type_optmenu),
				 index);
			gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
			break;
		}
	}
}

static gint edit_header_list_dialog_deleted(GtkWidget *widget,
					    GdkEventAny *event, gpointer data)
{
	edit_header_list_dialog.finished = TRUE;
	return TRUE;
}

static gboolean edit_header_list_dialog_key_pressed(GtkWidget *widget,
						    GdkEventKey *event,
						    gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		edit_header_list_dialog.finished = TRUE;
	return FALSE;
}

static void edit_header_list_dialog_add(void)
{
	GtkCList *clist = GTK_CLIST(edit_header_list_dialog.clist);
	const gchar *text;
	gchar *ctext[1];
	gint row;
	gchar *row_text;

	text = gtk_entry_get_text(GTK_ENTRY(edit_header_list_dialog.entry));
	if (text[0] == '\0') return;

	for (row = 0; gtk_clist_get_text(clist, row, 0, &row_text) != 0;
	     row++) {
		if (g_ascii_strcasecmp(row_text, text) == 0) return;
	}

	ctext[0] = (gchar *)text;
	gtk_clist_append(clist, ctext);
}

static void edit_header_list_dialog_delete(void)
{
	GtkCList *clist = GTK_CLIST(edit_header_list_dialog.clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	gtk_clist_remove(clist, row);
}

static void edit_header_list_dialog_ok(void)
{
	edit_header_list_dialog.finished = TRUE;
	edit_header_list_dialog.ok = TRUE;
}

static void edit_header_list_dialog_cancel(void)
{
	edit_header_list_dialog.finished = TRUE;
}

static void prefs_filter_edit_edit_header_list_dialog_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;

	GtkWidget *vbox2;
	GtkWidget *scrwin;
	GtkWidget *clist;

	GtkWidget *entry_hbox;
	GtkWidget *label;
	GtkWidget *entry;

	GtkWidget *btn_vbox;
	GtkWidget *add_btn;
	GtkWidget *del_btn;

	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	gchar *title[1];

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_position(GTK_WINDOW(window),GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	gtk_window_set_title(GTK_WINDOW(window), _("Edit header list"));

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(edit_header_list_dialog_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(edit_header_list_dialog_key_pressed), NULL);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	vbox2 = gtk_vbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);

	scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrwin, 140 * gtkut_get_dpi_multiplier(), 180 * gtkut_get_dpi_multiplier());
	gtk_box_pack_start(GTK_BOX(vbox2), scrwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	title[0] = _("Headers");
	clist = gtk_clist_new_with_titles(1, title);
	gtk_container_add(GTK_CONTAINER(scrwin), clist);
	gtk_clist_set_column_width(GTK_CLIST(clist), 0, 80);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
	gtkut_clist_set_redraw(GTK_CLIST(clist));
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[0].button,
			       GTK_CAN_FOCUS);

	entry_hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(vbox), entry_hbox, FALSE, TRUE, 0);

	label = gtk_label_new(_("Header:"));
	gtk_box_pack_start(GTK_BOX(entry_hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(entry_hbox), entry, TRUE, TRUE, 0);

	btn_vbox = gtk_vbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(hbox), btn_vbox, FALSE, FALSE, 0);

	add_btn = gtk_button_new_with_label(_("Add"));
	gtk_box_pack_start(GTK_BOX(btn_vbox), add_btn, FALSE, FALSE, 0);

	del_btn = gtk_button_new_with_label(_(" Delete "));
	gtk_box_pack_start(GTK_BOX(btn_vbox), del_btn, FALSE, FALSE, 0);

	gtkut_stock_button_set_create(&confirm_area, &ok_btn, GTK_STOCK_OK,
				      &cancel_btn, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	g_signal_connect(G_OBJECT(add_btn), "clicked",
			 G_CALLBACK(edit_header_list_dialog_add), NULL);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(edit_header_list_dialog_delete), NULL);
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(edit_header_list_dialog_ok), NULL);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(edit_header_list_dialog_cancel), NULL);

	manage_window_set_transient(GTK_WINDOW(window));

	gtk_widget_show_all(window);

	edit_header_list_dialog.window = window;
	edit_header_list_dialog.clist = clist;
	edit_header_list_dialog.entry = entry;
	edit_header_list_dialog.finished = FALSE;
	edit_header_list_dialog.ok = FALSE;
}

static void prefs_filter_edit_edit_header_list_dialog_set(void)
{
	GtkCList *clist = GTK_CLIST(edit_header_list_dialog.clist);
	GSList *list;
	GSList *cur;
	gchar *text[1];

	gtk_clist_freeze(clist);

	list = prefs_filter_get_user_header_list();
	for (cur = list; cur != NULL; cur = cur->next) {
		Header *header = (Header *)cur->data;
		text[0] = header->name;
		gtk_clist_append(clist, text);
	}

	gtk_clist_thaw(clist);
}

static GSList *prefs_filter_edit_edit_header_list_dialog_get(void)
{
	GtkCList *clist = GTK_CLIST(edit_header_list_dialog.clist);
	gint row;
	gchar *text;
	GSList *list = NULL;

	for (row = 0; gtk_clist_get_text(clist, row, 0, &text) != 0; row++)
		list = procheader_add_header_list(list, text, NULL);

	return list;
}

static void prefs_filter_edit_edit_header_list(FilterCondEdit *cond_edit)
{
	GSList *list;
	GSList *cur;

	prefs_filter_edit_edit_header_list_dialog_create();
	prefs_filter_edit_edit_header_list_dialog_set();

	while (edit_header_list_dialog.finished == FALSE)
		gtk_main_iteration();

	if (edit_header_list_dialog.ok == TRUE) {
		list = prefs_filter_edit_edit_header_list_dialog_get();
		prefs_filter_set_user_header_list(list);
		prefs_filter_edit_update_header_list(cond_edit);
		for (cur = cond_edit->cond_hbox_list; cur != NULL;
		     cur = cur->next) {
			CondHBox *hbox = (CondHBox *)cur->data;
			prefs_filter_edit_set_cond_header_menu(cond_edit, hbox);
		}
		prefs_filter_write_user_header_list();
	}

	gtk_widget_destroy(edit_header_list_dialog.window);
	edit_header_list_dialog.window = NULL;
	edit_header_list_dialog.clist = NULL;
	edit_header_list_dialog.entry = NULL;
	edit_header_list_dialog.finished = FALSE;
	edit_header_list_dialog.ok = FALSE;
}

FilterCond *prefs_filter_edit_cond_hbox_to_cond(CondHBox *hbox,
						gboolean case_sens,
						gchar **error_msg)
{
	FilterCond *cond = NULL;
	GtkWidget *cond_type_menuitem;
	CondMenuType cond_menu_type;
	MatchMenuType match_menu_type;
	const gchar *header_name;
	const gchar *key_str;
	gint int_value;
	FilterMatchType match_type = FLT_CONTAIN;
	FilterMatchFlag match_flag = 0;
	SizeMatchType size_type;
	AgeMatchType age_type;
	StatusMatchType status_type;
	gchar *error_msg_ = NULL;

	cond_type_menuitem = gtk_menu_get_active
		(GTK_MENU(gtk_option_menu_get_menu
			(GTK_OPTION_MENU(hbox->cond_type_optmenu))));
	cond_menu_type = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(cond_type_menuitem), MENU_VAL_ID));

	match_menu_type = menu_get_option_menu_active_index
		(GTK_OPTION_MENU(hbox->match_type_optmenu));

	key_str = gtk_entry_get_text(GTK_ENTRY(hbox->key_entry));

	switch (match_menu_type) {
	case PF_MATCH_CONTAIN:
		match_type = FLT_CONTAIN;
		break;
	case PF_MATCH_NOT_CONTAIN:
		match_type = FLT_CONTAIN;
		match_flag |= FLT_NOT_MATCH;
		break;
	case PF_MATCH_EQUAL:
		match_type = FLT_EQUAL;
		break;
	case PF_MATCH_NOT_EQUAL:
		match_type = FLT_EQUAL;
		match_flag |= FLT_NOT_MATCH;
		break;
	case PF_MATCH_REGEX:
		match_type = FLT_REGEX;
		break;
	case PF_MATCH_NOT_REGEX:
		match_type = FLT_REGEX;
		match_flag |= FLT_NOT_MATCH;
		break;
	case PF_MATCH_IN_ADDRESSBOOK:
		match_type = FLT_IN_ADDRESSBOOK;
		break;
	case PF_MATCH_NOT_IN_ADDRESSBOOK:
		match_type = FLT_IN_ADDRESSBOOK;
		match_flag |= FLT_NOT_MATCH;
		break;
	default:
		break;
	}

	if (case_sens)
		match_flag |= FLT_CASE_SENS;

	switch (cond_menu_type) {
	case PF_COND_HEADER:
		header_name = g_object_get_data
			(G_OBJECT(cond_type_menuitem), "header_str");
		cond = filter_cond_new(FLT_COND_HEADER,
				       match_type, match_flag,
				       header_name, key_str);
		break;
	case PF_COND_TO_OR_CC:
		cond = filter_cond_new(FLT_COND_TO_OR_CC, match_type,
				       match_flag, NULL, key_str);
		break;
	case PF_COND_ANY_HEADER:
		cond = filter_cond_new(FLT_COND_ANY_HEADER, match_type,
				       match_flag, NULL, key_str);
		break;
	case PF_COND_BODY:
		cond = filter_cond_new(FLT_COND_BODY, match_type,
				       match_flag, NULL, key_str);
		break;
	case PF_COND_CMD_TEST:
		if (key_str && *key_str)
			cond = filter_cond_new(FLT_COND_CMD_TEST,
					       0, 0, NULL, key_str);
		else
			error_msg_ = _("Command is not specified.");
		break;
	case PF_COND_SIZE:
		size_type = menu_get_option_menu_active_index
			 (GTK_OPTION_MENU(hbox->size_match_optmenu));
		match_flag = size_type == PF_SIZE_LARGER ? 0 : FLT_NOT_MATCH;
		int_value = gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON(hbox->spin_btn));
		cond = filter_cond_new(FLT_COND_SIZE_GREATER,
				       0, match_flag, NULL, itos(int_value));
		break;
	case PF_COND_AGE:
		age_type = menu_get_option_menu_active_index
			 (GTK_OPTION_MENU(hbox->age_match_optmenu));
		match_flag = age_type == PF_AGE_LONGER ? 0 : FLT_NOT_MATCH;
		int_value = gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON(hbox->spin_btn));
		cond = filter_cond_new(FLT_COND_AGE_GREATER,
				       0, match_flag, NULL, itos(int_value));
		break;
	case PF_COND_UNREAD:
		status_type = menu_get_option_menu_active_index
			(GTK_OPTION_MENU(hbox->status_match_optmenu));
		match_flag = status_type == PF_STATUS_MATCH ? 0 : FLT_NOT_MATCH;
		cond = filter_cond_new(FLT_COND_UNREAD, 0, match_flag,
				       NULL, NULL);
		break;
	case PF_COND_MARK:
		status_type = menu_get_option_menu_active_index
			(GTK_OPTION_MENU(hbox->status_match_optmenu));
		match_flag = status_type == PF_STATUS_MATCH ? 0 : FLT_NOT_MATCH;
		cond = filter_cond_new(FLT_COND_MARK, 0, match_flag,
				       NULL, NULL);
		break;
	case PF_COND_COLOR_LABEL:
		status_type = menu_get_option_menu_active_index
			(GTK_OPTION_MENU(hbox->status_match_optmenu));
		match_flag = status_type == PF_STATUS_MATCH ? 0 : FLT_NOT_MATCH;
		cond = filter_cond_new(FLT_COND_COLOR_LABEL, 0, match_flag,
				       NULL, NULL);
		break;
	case PF_COND_MIME:
		status_type = menu_get_option_menu_active_index
			(GTK_OPTION_MENU(hbox->status_match_optmenu));
		match_flag = status_type == PF_STATUS_MATCH ? 0 : FLT_NOT_MATCH;
		cond = filter_cond_new(FLT_COND_MIME, 0, match_flag,
				       NULL, NULL);
		break;
	case PF_COND_ACCOUNT:
	case PF_COND_EDIT_HEADER:
	default:
		break;
	}

	if (error_msg)
		*error_msg = error_msg_;

	return cond;
}

static gboolean check_dest_folder(const gchar *dest, gchar **error_msg)
{
	FolderItem *item;

	if (!dest || *dest == '\0') {
		*error_msg = _("Destination folder is not specified.");
		return FALSE;
	}

	item = folder_find_item_from_identifier(dest);
	if (!item || !item->path || !item->parent) {
		*error_msg = _("The specified destination folder does not exist.");
		return FALSE;
	}

	return TRUE;
}

FilterAction *prefs_filter_edit_action_hbox_to_action(ActionHBox *hbox,
						      gchar **error_msg)
{
	FilterAction *action = NULL;
	ActionMenuType action_menu_type;
	const gchar *str;
	guint color;
	gchar *error_msg_ = NULL;

	action_menu_type = prefs_filter_edit_get_action_hbox_type(hbox);

	switch (action_menu_type) {
	case PF_ACTION_MOVE:
		str = gtk_entry_get_text(GTK_ENTRY(hbox->folder_entry));
		if (check_dest_folder(str, &error_msg_))
			action = filter_action_new(FLT_ACTION_MOVE, str);
		break;
	case PF_ACTION_COPY:
		str = gtk_entry_get_text(GTK_ENTRY(hbox->folder_entry));
		if (check_dest_folder(str, &error_msg_))
			action = filter_action_new(FLT_ACTION_COPY, str);
		break;
	case PF_ACTION_NOT_RECEIVE:
		action = filter_action_new(FLT_ACTION_NOT_RECEIVE, NULL);
		break;
	case PF_ACTION_DELETE:
		action = filter_action_new(FLT_ACTION_DELETE, NULL);
		break;
	case PF_ACTION_EXEC:
		str = gtk_entry_get_text(GTK_ENTRY(hbox->cmd_entry));
		if (str && *str)
			action = filter_action_new(FLT_ACTION_EXEC, str);
		else
			error_msg_ = _("Command is not specified.");
		break;
	case PF_ACTION_EXEC_ASYNC:
		str = gtk_entry_get_text(GTK_ENTRY(hbox->cmd_entry));
		if (str && *str)
			action = filter_action_new(FLT_ACTION_EXEC_ASYNC, str);
		else
			error_msg_ = _("Command is not specified.");
		break;
	case PF_ACTION_MARK:
		action = filter_action_new(FLT_ACTION_MARK, NULL);
		break;
	case PF_ACTION_COLOR_LABEL:
		color = colorlabel_get_color_menu_active_item
			(gtk_option_menu_get_menu
			 (GTK_OPTION_MENU(hbox->clabel_optmenu)));
		action = filter_action_new(FLT_ACTION_COLOR_LABEL,
					   itos(color));
		break;
	case PF_ACTION_MARK_READ:
		action = filter_action_new(FLT_ACTION_MARK_READ, NULL);
		break;
	case PF_ACTION_FORWARD:
	case PF_ACTION_FORWARD_AS_ATTACHMENT:
	case PF_ACTION_REDIRECT:
		break;
	case PF_ACTION_STOP_EVAL:
		action = filter_action_new(FLT_ACTION_STOP_EVAL, NULL);
		break;
	case PF_ACTION_SEPARATOR:
	default:
		break;
	}

	if (error_msg)
		*error_msg = error_msg_;

	return action;
}

GSList *prefs_filter_edit_cond_edit_to_list(FilterCondEdit *cond_edit,
					    gboolean case_sens)
{
	GSList *cur;
	FilterCond *cond;
	GSList *cond_list = NULL;

	for (cur = cond_edit->cond_hbox_list; cur != NULL; cur = cur->next) {
		CondHBox *hbox = (CondHBox *)cur->data;
		gchar *error_msg;

		cond = prefs_filter_edit_cond_hbox_to_cond(hbox, case_sens,
							   &error_msg);
		if (cond) {
			cond_list = g_slist_append(cond_list, cond);
		} else {
			if (!error_msg)
				error_msg = _("Invalid condition exists.");
			alertpanel_error("%s", error_msg);
			filter_cond_list_free(cond_list);
			return NULL;
		}
	}

	return cond_list;
}

static FilterRule *prefs_filter_edit_dialog_to_rule(void)
{
	FilterRule *rule = NULL;
	GSList *cur;
	const gchar *rule_name;
	FilterBoolOp bool_op;
	GSList *cond_list = NULL;
	GSList *action_list = NULL;
	GtkWidget *bool_op_menuitem;
	gchar *error_msg = NULL;

	rule_name = gtk_entry_get_text(GTK_ENTRY(rule_edit_window.name_entry));
	if (!rule_name || *rule_name == '\0') {
		error_msg = _("Rule name is not specified.");
		goto error;
	}

	bool_op_menuitem = gtk_menu_get_active
		(GTK_MENU(gtk_option_menu_get_menu
			(GTK_OPTION_MENU(rule_edit_window.bool_op_optmenu))));
	bool_op = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(bool_op_menuitem), MENU_VAL_ID));

	cond_list = prefs_filter_edit_cond_edit_to_list
		(&rule_edit_window.cond_edit, FALSE);
	if (!cond_list)
		return NULL;

	for (cur = rule_edit_window.action_hbox_list; cur != NULL;
	     cur = cur->next) {
		ActionHBox *hbox = (ActionHBox *)cur->data;
		FilterAction *action;

		action = prefs_filter_edit_action_hbox_to_action(hbox,
								 &error_msg);
		if (action)
			action_list = g_slist_append(action_list, action);
		else {
			if (!error_msg)
				error_msg = _("Invalid action exists.");
			goto error;
		}
	}

error:
	if (error_msg || !cond_list || !action_list) {
		if (!error_msg) {
			if (!cond_list)
				error_msg = _("Condition not exist.");
			else
				error_msg = _("Action not exist.");
		}
		alertpanel_error("%s", error_msg);
		if (cond_list)
			filter_cond_list_free(cond_list);
		if (action_list)
			filter_action_list_free(action_list);
		return NULL;
	}

	rule = filter_rule_new(rule_name, bool_op, cond_list, action_list);

	return rule;
}

/* callback functions */

static gint prefs_filter_edit_deleted(GtkWidget *widget, GdkEventAny *event,
				      gpointer data)
{
	prefs_filter_edit_cancel();
	return TRUE;
}

static gboolean prefs_filter_edit_key_pressed(GtkWidget *widget,
					      GdkEventKey *event,
					      gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_filter_edit_cancel();
	return FALSE;
}

static void prefs_filter_edit_ok(void)
{
	FilterRule *rule;

	rule = prefs_filter_edit_dialog_to_rule();
	if (rule) {
		rule_edit_window.new_rule = rule;
		rule_edit_window.edit_finished = TRUE;
	}
}

static void prefs_filter_edit_cancel(void)
{
	rule_edit_window.new_rule = NULL;
	rule_edit_window.edit_finished = TRUE;
}

static void prefs_filter_cond_activated_cb(GtkWidget *widget, gpointer data)
{
	CondHBox *hbox = (CondHBox *)data;
	FilterCondEdit *cond_edit = hbox->cond_edit;
	CondMenuType type;

	type = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(widget), MENU_VAL_ID));

	if (type == PF_COND_EDIT_HEADER) {
		prefs_filter_edit_edit_header_list(cond_edit);
		prefs_filter_edit_cond_hbox_select
			(hbox, hbox->cur_type, hbox->cur_header_name);
	} else {
		hbox->cur_type = type;
		g_free(hbox->cur_header_name);
		hbox->cur_header_name = NULL;

		prefs_filter_edit_set_cond_hbox_widgets(hbox, type);
		if (type == PF_COND_HEADER) {
			gchar *header_name;
			gchar *header_field;

			header_name = (gchar *)g_object_get_data
				(G_OBJECT(widget), "header_str");
			header_field = prefs_filter_get_msg_header_field
				(header_name);
			if (header_field)
				gtk_entry_set_text(GTK_ENTRY(hbox->key_entry),
						   header_field);
			hbox->cur_header_name = g_strdup(header_name);
		}
	}
}

static void prefs_filter_match_activated_cb(GtkWidget *widget, gpointer data)
{
	CondHBox *hbox = (CondHBox *)data;
	GtkWidget *cond_type_menuitem;
	CondMenuType cond_menu_type;

	cond_type_menuitem = gtk_menu_get_active
		(GTK_MENU(gtk_option_menu_get_menu
			(GTK_OPTION_MENU(hbox->cond_type_optmenu))));
	cond_menu_type = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(cond_type_menuitem), MENU_VAL_ID));

	prefs_filter_edit_set_cond_hbox_widgets(hbox, cond_menu_type);
}

static void prefs_filter_action_activated_cb(GtkWidget *widget, gpointer data)
{
	ActionHBox *hbox = (ActionHBox *)data;
	ActionMenuType type;

	type = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(widget), MENU_VAL_ID));
	prefs_filter_edit_set_action_hbox_widgets(hbox, type);
}

static void prefs_filter_action_select_dest_cb(GtkWidget *widget, gpointer data)
{
	ActionHBox *hbox = (ActionHBox *)data;

	FolderItem *dest;
	gchar *id;

	dest = foldersel_folder_sel(NULL, FOLDER_SEL_COPY, NULL);
	if (!dest || !dest->path) return;

	id = folder_item_get_identifier(dest);
	if (id) {
		gtk_entry_set_text(GTK_ENTRY(hbox->folder_entry), id);
		g_free(id);
	}
}

static void prefs_filter_cond_del_cb(GtkWidget *widget, gpointer data)
{
	CondHBox *hbox = (CondHBox *)data;
	FilterCondEdit *cond_edit = hbox->cond_edit;

	if (cond_edit->cond_hbox_list && cond_edit->cond_hbox_list->next)
		prefs_filter_edit_remove_cond_hbox(cond_edit, hbox);
}

static void prefs_filter_cond_add_cb(GtkWidget *widget, gpointer data)
{
	CondHBox *hbox = (CondHBox *)data;
	CondHBox *new_hbox;
	FilterCondEdit *cond_edit = hbox->cond_edit;
	gint index;

	index = g_slist_index(cond_edit->cond_hbox_list, hbox);
	g_return_if_fail(index >= 0);
	new_hbox = prefs_filter_edit_cond_hbox_create(cond_edit);
	prefs_filter_edit_set_cond_hbox_widgets(new_hbox, PF_COND_HEADER);
	prefs_filter_edit_insert_cond_hbox(cond_edit, new_hbox, index + 1);
	if (cond_edit->add_hbox)
		cond_edit->add_hbox(new_hbox);
}

static void prefs_filter_action_del_cb(GtkWidget *widget, gpointer data)
{
	ActionHBox *hbox = (ActionHBox *)data;

	if (rule_edit_window.action_hbox_list &&
	    rule_edit_window.action_hbox_list->next)
		prefs_filter_edit_remove_action_hbox(hbox);
}

static void prefs_filter_action_add_cb(GtkWidget *widget, gpointer data)
{
	ActionHBox *hbox = (ActionHBox *)data;
	ActionHBox *new_hbox;
	gboolean action_menu_selection[PF_ACTION_NONE];
	gint index;

	prefs_filter_edit_get_action_hbox_menus_selection(action_menu_selection);

	index = g_slist_index(rule_edit_window.action_hbox_list, hbox);
	g_return_if_fail(index >= 0);
	new_hbox = prefs_filter_edit_action_hbox_create();
	prefs_filter_edit_insert_action_hbox(new_hbox, index + 1);
	if (action_menu_selection[PF_ACTION_MOVE] == TRUE ||
	    action_menu_selection[PF_ACTION_NOT_RECEIVE] == TRUE ||
	    action_menu_selection[PF_ACTION_DELETE] == TRUE)
		prefs_filter_edit_set_action_hbox_widgets(new_hbox,
							  PF_ACTION_COPY);
	else
		prefs_filter_edit_set_action_hbox_widgets(new_hbox,
							  PF_ACTION_MOVE);
}
