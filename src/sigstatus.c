/* sigstatus.h - GTK+ based signature status display
 *      Copyright (C) 2001 Werner Koch (dd9jn)
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

#if USE_GPGME

#include <glib.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gdk/gdkkeysyms.h>
#include <gpgme.h>

#include "intl.h"
#include "gtkutils.h"
#include "utils.h"
#include "sigstatus.h"

/* remove the window after 30 seconds to avoid cluttering the deskop 
 * with too many of them */
#define MY_TIMEOUT (30*1000)

struct gpgmegtk_sig_status_s {
	GtkWidget *mainwindow;
	GtkWidget *label;
	gint running;
	gint destroy_pending;
	guint timeout_id;
	gint timeout_id_valid;
};


static void do_destroy(GpgmegtkSigStatus hd)
{
	if (!hd->running) {
		if (hd->mainwindow) {
			gtk_widget_destroy ( hd->mainwindow );
			hd->mainwindow = NULL;
		}
		if (hd->timeout_id_valid) {
			gtk_timeout_remove(hd->timeout_id);
			hd->timeout_id_valid = 0;
		}
		if (hd->destroy_pending) 
		g_free(hd);
	}
}

static void okay_cb(GtkWidget *widget, gpointer data)
{
	GpgmegtkSigStatus hd = data;

	hd->running = 0;
	do_destroy(hd);
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	GpgmegtkSigStatus hd = data;

	hd->running = 0;
	do_destroy(hd);

	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	GpgmegtkSigStatus hd = data;

	if (event && event->keyval == GDK_Escape) {
		hd->running = 0;
		do_destroy(hd);
	}
	return FALSE;
}

GpgmegtkSigStatus gpgmegtk_sig_status_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *okay_btn;
	GtkWidget *okay_area;
	GpgmegtkSigStatus hd;

	hd = g_malloc0(sizeof *hd);
	hd->running = 1;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	hd->mainwindow = window;
	gtk_widget_set_size_request(window, 400, -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), hd);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), hd);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 8);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Checking signature"));
	hd->label = label;
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);
	gtk_widget_show(label);

	gtkut_stock_button_set_create(&okay_area, &okay_btn, GTK_STOCK_OK,
				      NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), okay_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(okay_btn);
	g_signal_connect(G_OBJECT(okay_btn), "clicked",
			 G_CALLBACK(okay_cb), hd);

	gtk_widget_show_all(window);

	while (gtk_events_pending())
		gtk_main_iteration();

	return hd;
}

static gint timeout_cb(gpointer data)
{
	GpgmegtkSigStatus hd = data;

	hd->running = 0;
	hd->timeout_id_valid = 0;
	do_destroy(hd);

	return FALSE;
}

void gpgmegtk_sig_status_destroy(GpgmegtkSigStatus hd)
{
	if (hd) {
		hd->destroy_pending = 1;
		if (hd->running && !hd->timeout_id_valid) {
			hd->timeout_id = gtk_timeout_add(MY_TIMEOUT,
							 timeout_cb, hd);
			hd->timeout_id_valid = 1;
		}
		do_destroy(hd);
	}
}

/* Fixme: remove status and get it from the context */
void gpgmegtk_sig_status_update(GpgmegtkSigStatus hd, GpgmeCtx ctx)
{
	gint idx;
	time_t created;
	GpgmeSigStat status;
	gchar *text = NULL;

	if (!hd || !hd->running || !ctx)
		return;

	for (idx = 0; gpgme_get_sig_status(ctx, idx, &status, &created);
	     idx++) {
		gchar *tmp;
		const gchar *userid;
		GpgmeKey key = NULL;

		if (!gpgme_get_sig_key (ctx, idx, &key)) {
			userid = gpgme_key_get_string_attr
				(key, GPGME_ATTR_USERID, NULL, 0);
		} else
			userid = "[?]";

		tmp = g_strdup_printf(_("%s%s%s from \"%s\""),
				      text ? text : "",
				      text ? "\n" : "",
				      gpgmegtk_sig_status_to_string(status),
				      userid);
		g_free (text);
		text = tmp;
		gpgme_key_unref (key);
	}

	gtk_label_set_text(GTK_LABEL(hd->label), text); 
	g_free(text);

	while (gtk_events_pending())
		gtk_main_iteration();
}

const char *gpgmegtk_sig_status_to_string(GpgmeSigStat status)
{
	const char *result = "?";

	switch (status) {
	case GPGME_SIG_STAT_NONE:
		result = _("Oops: Signature not verified");
		break;
	case GPGME_SIG_STAT_NOSIG:
		result = _("No signature found");
		break;
	case GPGME_SIG_STAT_GOOD:
		result = _("Good signature");
		break;
	case GPGME_SIG_STAT_BAD:
		result = _("BAD signature");
		break;
	case GPGME_SIG_STAT_NOKEY:
		result = _("No public key to verify the signature");
		break;
	case GPGME_SIG_STAT_ERROR:
		result = _("Error verifying the signature");
		break;
	default:
		break;
	}

	return result;
}

#endif /* USE_GPGME */
