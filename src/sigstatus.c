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
#include <glib/gi18n.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gdk/gdkkeysyms.h>
#include <gpgme.h>

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
		if (hd->timeout_id_valid) {
			g_source_remove(hd->timeout_id);
			hd->timeout_id_valid = 0;
		}
		if (hd->mainwindow) {
			gtk_widget_destroy ( hd->mainwindow );
			hd->mainwindow = NULL;
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

static gboolean delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
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
		return TRUE;
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

	window = gtk_dialog_new();
	hd->mainwindow = window;
	gtk_widget_set_size_request(window, 400, -1);
	gtk_window_set_title(GTK_WINDOW(window), _("Signature check result"));
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_dialog_set_has_separator(GTK_DIALOG(window), FALSE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), hd);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), hd);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox),
			   vbox, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Checking signature"));
	hd->label = label;
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_widget_show(label);

	gtkut_stock_button_set_create(&okay_area, &okay_btn, GTK_STOCK_OK,
				      NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(window)->action_area),
			 okay_area, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(okay_area), 5);
	gtk_widget_grab_default(okay_btn);
	gtk_widget_grab_focus(okay_btn);
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

	gdk_threads_enter();

	hd->running = 0;
	hd->timeout_id_valid = 0;
	do_destroy(hd);

	gdk_threads_leave();

	return FALSE;
}

void gpgmegtk_sig_status_destroy(GpgmegtkSigStatus hd)
{
	if (hd) {
		hd->destroy_pending = 1;
		if (hd->running && !hd->timeout_id_valid) {
			hd->timeout_id = g_timeout_add(MY_TIMEOUT,
						       timeout_cb, hd);
			hd->timeout_id_valid = 1;
		}
		do_destroy(hd);
	}
}

void gpgmegtk_sig_status_update(GpgmegtkSigStatus hd, gpgme_ctx_t ctx)
{
	gpgme_verify_result_t result;
	gpgme_signature_t sig;
	gchar *text = NULL;

	if (!hd || !hd->running || !ctx)
		return;
	result = gpgme_op_verify_result(ctx);
	if (!result)
		return;

	sig = result->signatures;
	while (sig) {
		gchar *tmp;
		const gchar *userid;
		gpgme_key_t key = NULL;

		if (!gpgme_get_key(ctx, sig->fpr, &key, 0)) {
			userid = key->uids->uid;
		} else {
			userid = "[?]";
		}
		tmp = g_strdup_printf
			(_("%s%s%s from \"%s\""),
			 text ? text : "", text ? "\n" : "",
			 gpgmegtk_sig_status_to_string(sig, FALSE), userid);
		g_free (text);
		text = tmp;
		gpgme_key_unref (key);
		sig = sig->next;
	}
	gtk_label_set_text(GTK_LABEL(hd->label), text); 
	g_free(text);

	while (gtk_events_pending())
		gtk_main_iteration();
}

const gchar *gpgmegtk_sig_status_to_string(gpgme_signature_t signature,
					   gboolean use_name)
{
	const gchar *result = "?";

	g_return_val_if_fail(signature != NULL, result);

	switch (gpg_err_code(signature->status)) {
	case GPG_ERR_NO_DATA:
		result = _("No signature found");
		break;
	case GPG_ERR_NO_ERROR:
		switch (signature->validity) {
		case GPGME_VALIDITY_ULTIMATE:
		case GPGME_VALIDITY_FULL:
		case GPGME_VALIDITY_MARGINAL:
			result = use_name ? _("Good signature from \"%s\"") :
				_("Good signature");
			break;
		default:
			result = use_name ?
				_("Valid signature but the key for \"%s\" is not trusted") :
				_("Valid signature (untrusted key)");
			break;
		}
		break;
	case GPG_ERR_SIG_EXPIRED:
		result = use_name ? _("Signature valid but expired for \"%s\"") :
			_("Signature valid but expired");
		break;
	case GPG_ERR_KEY_EXPIRED:
		result = use_name ? _("Signature valid but the signing key for \"%s\" has expired") :
			_("Signature valid but the signing key has expired");
		break;
	case GPG_ERR_CERT_REVOKED:
		result = use_name ? _("Signature valid but the signing key for \"%s\" has been revoked") :
			_("Signature valid but the signing key has been revoked");
		break;
	case GPG_ERR_BAD_SIGNATURE:
		result = use_name ? _("BAD signature from \"%s\"") :
			_("BAD signature");
		break;
	case GPG_ERR_NO_PUBKEY:
		result = _("No public key to verify the signature");
		break;
	default:
		result = _("Error verifying the signature");
		break;
	}

	return result;
}

#endif /* USE_GPGME */
