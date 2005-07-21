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
#include <gtk/gtkwidget.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtkimage.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if HAVE_LIBCOMPFACE
#  include <compface.h>
#endif

#include "main.h"
#include "headerview.h"
#include "prefs_common.h"
#include "codeconv.h"
#include "gtkutils.h"
#include "utils.h"

#define TR(str)	(prefs_common.trans_hdr ? gettext(str) : str)

#if 0
	_("From:");
	_("To:");
	_("Newsgroups:");
	_("Subject:");
#endif

#if HAVE_LIBCOMPFACE
#define XPM_XFACE_HEIGHT	(HEIGHT + 3)  /* 3 = 1 header + 2 colors */

static gchar *xpm_xface[XPM_XFACE_HEIGHT];

static void headerview_show_xface	(HeaderView	*headerview,
					 MsgInfo	*msginfo);
static gint create_xpm_from_xface	(gchar		*xpm[],
					 const gchar	*xface);
#endif

HeaderView *headerview_create(void)
{
	HeaderView *headerview;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *hbox1;
	GtkWidget *hbox2;
	GtkWidget *from_header_label;
	GtkWidget *from_body_label;
	GtkWidget *to_header_label;
	GtkWidget *to_body_label;
	GtkWidget *ng_header_label;
	GtkWidget *ng_body_label;
	GtkWidget *subject_header_label;
	GtkWidget *subject_body_label;

	debug_print(_("Creating header view...\n"));
	headerview = g_new0(HeaderView, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

	hbox1 = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);
	hbox2 = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

	from_header_label    = gtk_label_new(TR("From:"));
	from_body_label      = gtk_label_new("");
	to_header_label      = gtk_label_new(TR("To:"));
	to_body_label        = gtk_label_new("");
	ng_header_label      = gtk_label_new(TR("Newsgroups:"));
	ng_body_label        = gtk_label_new("");
	subject_header_label = gtk_label_new(TR("Subject:"));
	subject_body_label   = gtk_label_new("");

	gtk_label_set_selectable(GTK_LABEL(from_body_label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(to_body_label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(ng_body_label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(subject_body_label), TRUE);

	GTK_WIDGET_UNSET_FLAGS(from_body_label, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(to_body_label, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(ng_body_label, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(subject_body_label, GTK_CAN_FOCUS);

	gtk_box_pack_start(GTK_BOX(hbox1), from_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), from_body_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), to_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), to_body_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), ng_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), ng_body_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), subject_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), subject_body_label, FALSE, FALSE, 0);

	headerview->hbox = hbox;
	headerview->from_header_label    = from_header_label;
	headerview->from_body_label      = from_body_label;
	headerview->to_header_label      = to_header_label;
	headerview->to_body_label        = to_body_label;
	headerview->ng_header_label      = ng_header_label;
	headerview->ng_body_label        = ng_body_label;
	headerview->subject_header_label = subject_header_label;
	headerview->subject_body_label   = subject_body_label;
	headerview->image = NULL;

	gtk_widget_show_all(hbox);

	return headerview;
}

void headerview_init(HeaderView *headerview)
{
	static PangoFontDescription *boldfont = NULL;

	if (!boldfont) {
		boldfont = pango_font_description_new();
		pango_font_description_set_weight(boldfont, PANGO_WEIGHT_BOLD);
	}

	if (boldfont) {
		gtk_widget_modify_font(headerview->from_header_label, boldfont);
		gtk_widget_modify_font(headerview->to_header_label, boldfont);
		gtk_widget_modify_font(headerview->ng_header_label, boldfont);
		gtk_widget_modify_font(headerview->subject_header_label, boldfont);
	}

	headerview_clear(headerview);
	headerview_set_visibility(headerview, prefs_common.display_header_pane);

#if HAVE_LIBCOMPFACE
	{
		gint i;

		for (i = 0; i < XPM_XFACE_HEIGHT; i++) {
			xpm_xface[i] = g_malloc(WIDTH + 1);
			*xpm_xface[i] = '\0';
		}
	}
#endif
}

void headerview_show(HeaderView *headerview, MsgInfo *msginfo)
{
	headerview_clear(headerview);

	gtk_label_set_text(GTK_LABEL(headerview->from_body_label),
			   msginfo->from ? msginfo->from : _("(No From)"));
	if (msginfo->to) {
		gtk_label_set_text(GTK_LABEL(headerview->to_body_label),
				   msginfo->to);
		gtk_widget_show(headerview->to_header_label);
		gtk_widget_show(headerview->to_body_label);
	}
	if (msginfo->newsgroups) {
		gtk_label_set_text(GTK_LABEL(headerview->ng_body_label),
				   msginfo->newsgroups);
		gtk_widget_show(headerview->ng_header_label);
		gtk_widget_show(headerview->ng_body_label);
	}
	gtk_label_set_text(GTK_LABEL(headerview->subject_body_label),
			   msginfo->subject ? msginfo->subject
			   : _("(No Subject)"));

#if HAVE_LIBCOMPFACE
	headerview_show_xface(headerview, msginfo);
#endif
}

#if HAVE_LIBCOMPFACE
static void headerview_show_xface(HeaderView *headerview, MsgInfo *msginfo)
{
	gchar xface[2048];
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkWidget *hbox = headerview->hbox;

	if (!msginfo->xface || strlen(msginfo->xface) < 5) {
		if (headerview->image &&
		    GTK_WIDGET_VISIBLE(headerview->image)) {
			gtk_widget_hide(headerview->image);
			gtk_widget_queue_resize(hbox);
		}
		return;
	}
	if (!GTK_WIDGET_VISIBLE(headerview->hbox)) return;

	strncpy(xface, msginfo->xface, sizeof(xface));

	if (uncompface(xface) < 0) {
		g_warning("uncompface failed\n");
		if (headerview->image)
			gtk_widget_hide(headerview->image);
		return;
	}

	create_xpm_from_xface(xpm_xface, xface);

	pixmap = gdk_pixmap_create_from_xpm_d
		(hbox->window, &mask, &hbox->style->white, xpm_xface);

	if (!headerview->image) {
		GtkWidget *image;

		image = gtk_image_new_from_pixmap(pixmap, mask);
		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);
		headerview->image = image;
	} else {
		gtk_image_set_from_pixmap(GTK_IMAGE(headerview->image),
					  pixmap, mask);
		gtk_widget_show(headerview->image);
	}

	gdk_pixmap_unref(pixmap);
}
#endif

void headerview_clear(HeaderView *headerview)
{
	gtk_label_set_text(GTK_LABEL(headerview->from_body_label), "");
	gtk_label_set_text(GTK_LABEL(headerview->to_body_label), "");
	gtk_label_set_text(GTK_LABEL(headerview->ng_body_label), "");
	gtk_label_set_text(GTK_LABEL(headerview->subject_body_label), "");
	gtk_widget_hide(headerview->to_header_label);
	gtk_widget_hide(headerview->to_body_label);
	gtk_widget_hide(headerview->ng_header_label);
	gtk_widget_hide(headerview->ng_body_label);

	if (headerview->image && GTK_WIDGET_VISIBLE(headerview->image)) {
		gtk_widget_hide(headerview->image);
		gtk_widget_queue_resize(headerview->hbox);
	}
}

void headerview_set_visibility(HeaderView *headerview, gboolean visibility)
{
	if (visibility)
		gtk_widget_show(headerview->hbox);
	else
		gtk_widget_hide(headerview->hbox);
}

void headerview_destroy(HeaderView *headerview)
{
	g_free(headerview);
}

#if HAVE_LIBCOMPFACE
static gint create_xpm_from_xface(gchar *xpm[], const gchar *xface)
{
	static gchar *bit_pattern[] = {
		"....",
		"...#",
		"..#.",
		"..##",
		".#..",
		".#.#",
		".##.",
		".###",
		"#...",
		"#..#",
		"#.#.",
		"#.##",
		"##..",
		"##.#",
		"###.",
		"####"
	};

	static gchar *xface_header = "48 48 2 1";
	static gchar *xface_black  = "# c #000000";
	static gchar *xface_white  = ". c #ffffff";

	gint i, line = 0;
	const guchar *p;
	gchar buf[WIDTH * 4 + 1];  /* 4 = strlen("0x0000") */

	p = xface;

	strcpy(xpm[line++], xface_header);
	strcpy(xpm[line++], xface_black);
	strcpy(xpm[line++], xface_white);

	for (i = 0; i < HEIGHT; i++) {
		gint col;

		buf[0] = '\0';
     
		for (col = 0; col < 3; col++) {
			gint figure;

			p += 2;  /* skip '0x' */

			for (figure = 0; figure < 4; figure++) {
				gint n = 0;

				if ('0' <= *p && *p <= '9') {
					n = *p - '0';
				} else if ('a' <= *p && *p <= 'f') {
					n = *p - 'a' + 10;
				} else if ('A' <= *p && *p <= 'F') {
					n = *p - 'A' + 10;
				}

				strcat(buf, bit_pattern[n]);
				p++;  /* skip ',' */
			}

			p++;  /* skip '\n' */
		}

		strcpy(xpm[line++], buf);
		p++;
	}

	return 0;
}
#endif
