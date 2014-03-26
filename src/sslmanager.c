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

#if USE_SSL

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>

#include "ssl.h"
#include "sslmanager.h"
#include "manage_window.h"
#include "prefs_common.h"

gint ssl_manager_verify_cert(SockInfo *sockinfo, const gchar *hostname,
			     X509 *server_cert, glong verify_result)
{
	static PangoFontDescription *font_desc;
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *vbox;
	GtkWidget *label;
	const gchar *title;
	GString *message;
	gchar *subject, *issuer;
	guchar keyid[EVP_MAX_MD_SIZE];
	gchar keyidstr[EVP_MAX_MD_SIZE * 3 + 1] = "";
	guint keyidlen = 0;
	gchar *sha1_keyidstr, *md5_keyidstr;
	BIO *bio;
	gchar not_before[64] = "", not_after[64] = "";
	gint i;
	gint result;
	gboolean disable_always = FALSE;

	if (verify_result == X509_V_OK)
		return 0;

	gdk_threads_enter();

	title = _("SSL certificate verify failed");

	subject = X509_NAME_oneline(X509_get_subject_name(server_cert),
				    NULL, 0);
	issuer = X509_NAME_oneline(X509_get_issuer_name(server_cert), NULL, 0);

	bio = BIO_new(BIO_s_mem());
	ASN1_TIME_print(bio, X509_get_notBefore(server_cert));
	BIO_gets(bio, not_before, sizeof(not_before));
	BIO_reset(bio);
	ASN1_TIME_print(bio, X509_get_notAfter(server_cert));
	BIO_gets(bio, not_after, sizeof(not_after));
	BIO_free(bio);

	if (X509_digest(server_cert, EVP_sha1(), keyid, &keyidlen)) {
		for (i = 0; i < keyidlen; i++)
			g_snprintf(keyidstr + i * 3, 4, "%02x:", keyid[i]);
		keyidstr[keyidlen * 3 - 1] = '\0';
		sha1_keyidstr = g_ascii_strup(keyidstr, -1);
	} else {
		sha1_keyidstr = g_strdup("(cannot calculate digest)");
	}
	if (X509_digest(server_cert, EVP_md5(), keyid, &keyidlen)) {
		for (i = 0; i < keyidlen; i++)
			g_snprintf(keyidstr + i * 3, 4, "%02x:", keyid[i]);
		keyidstr[keyidlen * 3 - 1] = '\0';
		md5_keyidstr = g_ascii_strup(keyidstr, -1);
	} else {
		md5_keyidstr = g_strdup("(cannot calculate digest)");
	}

	message = g_string_new("");
	g_string_append_printf(message, _("The SSL certificate of %s cannot be verified by the following reason:"), hostname);
	if (verify_result == X509_V_ERR_APPLICATION_VERIFICATION) {
		g_string_append_printf(message, "\n  certificate hostname does not match\n\n");
	} else {
		g_string_append_printf(message, "\n  %s\n\n", X509_verify_cert_error_string(verify_result));
	}
	g_string_append_printf(message, _("Subject: %s\n"), subject ? subject : "(unknown)");
	g_string_append_printf(message, _("Issuer: %s\n"), issuer ? issuer : "(unknown)");
	g_string_append_printf(message, _("Issued date: %s\n"), not_before);
	g_string_append_printf(message, _("Expire date: %s\n"), not_after);
	g_string_append(message, "\n");
	g_string_append_printf(message, _("SHA1 fingerprint: %s\n"), sha1_keyidstr);
	g_string_append_printf(message, _("MD5 fingerprint: %s\n"), md5_keyidstr);
	g_string_append(message, "\n");
	g_string_append(message, _("Do you accept this certificate?"));
	g_free(md5_keyidstr);
	g_free(sha1_keyidstr);
	if (issuer)
		OPENSSL_free(issuer);
	if (subject)
		OPENSSL_free(subject);

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog),
				GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_widget_realize(dialog);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
			   hbox, FALSE, FALSE, 0);

	image = gtk_image_new_from_stock
		(GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);

	gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.0);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

	label = gtk_label_new(title);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

	if (!font_desc) {
		gint size;

		size = pango_font_description_get_size
			(label->style->font_desc);
		font_desc = pango_font_description_new();
		pango_font_description_set_weight
			(font_desc, PANGO_WEIGHT_BOLD);
		pango_font_description_set_size
			(font_desc, size * PANGO_SCALE_LARGE);
	}
	if (font_desc)
		gtk_widget_modify_font(label, font_desc);

	label = gtk_label_new(message->str);
	g_string_free(message, TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	GTK_WIDGET_UNSET_FLAGS(label, GTK_CAN_FOCUS);
#ifdef G_OS_WIN32
	{
		GtkStyle *style;
		style = gtk_widget_get_style(dialog);
		gtk_widget_modify_base(label, GTK_STATE_ACTIVE,
				       &style->base[GTK_STATE_SELECTED]);
		gtk_widget_modify_text(label, GTK_STATE_ACTIVE,
				       &style->text[GTK_STATE_SELECTED]);
	}
#endif

	/* prohibit acception of expired certificates */
	if (verify_result == X509_V_ERR_CERT_HAS_EXPIRED)
		disable_always = TRUE;

	if (prefs_common.comply_gnome_hig)
		gtk_dialog_add_buttons(GTK_DIALOG(dialog),
				       _("_Reject"), GTK_RESPONSE_REJECT,
				       _("_Temporarily accept"), GTK_RESPONSE_OK,
				       _("Always _accept"), GTK_RESPONSE_ACCEPT,
				       NULL);
	else
		gtk_dialog_add_buttons(GTK_DIALOG(dialog),
				       _("Always _accept"), GTK_RESPONSE_ACCEPT,
				       _("_Temporarily accept"), GTK_RESPONSE_OK,
				       _("_Reject"), GTK_RESPONSE_REJECT,
				       NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	if (disable_always)
		gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
						  GTK_RESPONSE_ACCEPT, FALSE);

	gtk_widget_show_all(dialog);

	result = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	gdk_threads_leave();

	switch (result) {
	case GTK_RESPONSE_ACCEPT:
		return 0;
	case GTK_RESPONSE_OK:
		return 1;
	case GTK_RESPONSE_REJECT:
	default:
		break;
	}

	return -1;
}

#endif /* USE_SSL */
