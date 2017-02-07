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

#ifndef __PREFS_UI_H__
#define __PREFS_UI_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtksignal.h>
#include <gtk/gtklabel.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkvbox.h>
#include <stdio.h>

typedef struct _PrefsDialog	PrefsDialog;
typedef struct _PrefsUIData	PrefsUIData;

#include "prefs.h"
#include "gtkutils.h"

#define VSPACING		10
#define VSPACING_NARROW		4
#define VSPACING_NARROW_2	2
#define VBOX_BORDER		12
#define DEFAULT_ENTRY_WIDTH	80

struct _PrefsUIData
{
	gchar *name;
	GtkWidget **widget;
	DataSetFunc data_set_func;
	WidgetSetFunc widget_set_func;
};

struct _PrefsDialog 
{
	GtkWidget *window;
	GtkWidget *notebook;

	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *apply_btn;
};

#define SET_NOTEBOOK_LABEL(notebook, str, page_num) \
{ \
	GtkWidget *label; \
 \
	label = gtk_label_new (str); \
	gtk_widget_show (label); \
	gtk_notebook_set_tab_label \
		(GTK_NOTEBOOK (notebook), \
		 gtk_notebook_get_nth_page \
			(GTK_NOTEBOOK (notebook), page_num), \
		 label); \
}

#define APPEND_SUB_NOTEBOOK(notebook, vbox, str) \
{ \
	GtkWidget *label; \
 \
	label = gtk_label_new(str); \
	gtk_widget_show(label); \
	vbox = gtk_vbox_new(FALSE, VSPACING); \
	gtk_widget_show(vbox); \
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label); \
	gtk_container_set_border_width(GTK_CONTAINER(vbox), VBOX_BORDER); \
}

#define PACK_CHECK_BUTTON(box, chkbtn, label) \
{ \
	chkbtn = gtk_check_button_new_with_label(label); \
	gtk_widget_show(chkbtn); \
	gtk_box_pack_start(GTK_BOX(box), chkbtn, FALSE, TRUE, 0); \
}

#define PACK_END_CHECK_BUTTON(box, chkbtn, label) \
{ \
	chkbtn = gtk_check_button_new_with_label(label); \
	gtk_widget_show(chkbtn); \
	gtk_box_pack_end(GTK_BOX(box), chkbtn, FALSE, TRUE, 0); \
}

#define PACK_FRAME(box, frame, label) \
{ \
	frame = gtk_frame_new(label); \
	gtk_widget_show(frame); \
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, TRUE, 0); \
	gtk_frame_set_label_align(GTK_FRAME(frame), 0.01, 0.5); \
}

#define PACK_FRAME_WITH_CHECK_BUTTON(box, frame, chkbtn, label) \
{ \
	chkbtn = gtk_check_button_new_with_label(label); \
	gtk_widget_show(chkbtn); \
	PACK_FRAME(box, frame, NULL); \
	gtk_frame_set_label_widget(GTK_FRAME(frame), chkbtn); \
}

#define PACK_SMALL_LABEL(box, label, str) \
{ \
	label = gtk_label_new(str); \
	gtk_widget_show(label); \
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, TRUE, 0); \
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5); \
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT); \
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE); \
	gtkut_widget_set_small_font_size(label); \
}

#define PACK_VSPACER(box, vbox, spacing) \
{ \
	vbox = gtk_vbox_new(FALSE, 0); \
	gtk_widget_show(vbox); \
	gtk_box_pack_start(GTK_BOX(box), vbox, FALSE, TRUE, spacing); \
}

#define SET_TOGGLE_SENSITIVITY(togglewid, targetwid) \
{ \
	gtk_widget_set_sensitive(targetwid, FALSE); \
	g_signal_connect(G_OBJECT(togglewid), "toggled", \
			 G_CALLBACK(prefs_button_toggled), targetwid); \
}

#define SET_TOGGLE_SENSITIVITY_REV(togglewid, targetwid) \
{ \
	gtk_widget_set_sensitive(targetwid, TRUE); \
	g_signal_connect(G_OBJECT(togglewid), "toggled", \
			 G_CALLBACK(prefs_button_toggled_rev), targetwid); \
}

void prefs_dialog_create	(PrefsDialog	*dialog);
void prefs_dialog_destroy	(PrefsDialog	*dialog);

void prefs_button_toggled	(GtkToggleButton	*toggle_btn,
				 GtkWidget		*widget);
void prefs_button_toggled_rev	(GtkToggleButton	*toggle_btn,
				 GtkWidget		*widget);

void prefs_register_ui		(PrefParam	*param,
				 PrefsUIData	*ui_data);

void prefs_set_dialog		(PrefParam	*param);
void prefs_set_data_from_dialog	(PrefParam	*param);
void prefs_set_dialog_to_default(PrefParam	*param);

void prefs_set_data_from_entry	(PrefParam	*pparam);
void prefs_set_entry		(PrefParam	*pparam);
void prefs_set_data_from_text	(PrefParam	*pparam);
void prefs_set_text		(PrefParam	*pparam);
void prefs_set_data_from_toggle	(PrefParam	*pparam);
void prefs_set_toggle		(PrefParam	*pparam);
void prefs_set_data_from_spinbtn(PrefParam	*pparam);
void prefs_set_spinbtn		(PrefParam	*pparam);
void prefs_set_data_from_fontbtn(PrefParam	*pparam);
void prefs_set_fontbtn		(PrefParam	*pparam);
void prefs_set_data_from_optmenu(PrefParam	*pparam);
void prefs_set_optmenu		(PrefParam	*pparam);

gchar *prefs_get_escaped_str_from_text	(GtkWidget	*widget);
void prefs_set_escaped_str_to_text	(GtkWidget	*widget,
					 const gchar	*str);

#endif /* __PREFS_UI_H__ */
