/* passphrase.c - GTK+ based passphrase callback
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
#  include <config.h>
#endif

#if USE_GPGME

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkdisplay.h>
#ifdef GDK_WINDOWING_X11
#  include <gdk/gdkx.h>
#endif /* GDK_WINDOWING_X11 */
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <string.h>
#include <sys/types.h>
#if HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#endif

#ifdef G_OS_WIN32
#  include <windows.h>
#endif

#include "passphrase.h"
#include "prefs_common.h"
#include "manage_window.h"
#include "utils.h"
#include "gtkutils.h"

static gboolean grab_all = FALSE;

static gboolean pass_ack;
static gchar *last_pass = NULL;

static void passphrase_ok_cb(GtkWidget *widget, gpointer data);
static void passphrase_cancel_cb(GtkWidget *widget, gpointer data);
static gint passphrase_deleted(GtkWidget *widget, GdkEventAny *event,
			       gpointer data);
static gboolean passphrase_key_pressed(GtkWidget *widget, GdkEventKey *event,
				       gpointer data);
static gchar* passphrase_mbox(const gchar *uid_hint, const gchar *pass_hint,
			      gint prev_bad);

static GtkWidget *create_description(const gchar *uid_hint,
				     const gchar *pass_hint, gint prev_bad);

void
gpgmegtk_set_passphrase_grab(gint yes)
{
    grab_all = yes;
}

static gchar*
passphrase_mbox(const gchar *uid_hint, const gchar *pass_hint, gint prev_bad)
{
    gchar *the_passphrase = NULL;
    GtkWidget *vbox;
    GtkWidget *confirm_box;
    GtkWidget *window;
    GtkWidget *pass_entry;
    GtkWidget *ok_button;
    GtkWidget *cancel_button;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), _("Passphrase"));
    gtk_widget_set_size_request(window, 450, -1);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
    g_signal_connect(G_OBJECT(window), "delete_event",
                     G_CALLBACK(passphrase_deleted), NULL);
    g_signal_connect(G_OBJECT(window), "key_press_event",
                     G_CALLBACK(passphrase_key_pressed), NULL);
    MANAGE_WINDOW_SIGNALS_CONNECT(window);
    manage_window_set_transient(GTK_WINDOW(window));

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    if (uid_hint || pass_hint) {
        GtkWidget *label;
        label = create_description (uid_hint, pass_hint, prev_bad);
        gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
    }

    pass_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), pass_entry, FALSE, FALSE, 0);
    gtk_entry_set_visibility(GTK_ENTRY(pass_entry), FALSE);
    gtk_widget_grab_focus(pass_entry);

    gtkut_stock_button_set_create(&confirm_box, &ok_button, GTK_STOCK_OK,
				  &cancel_button, GTK_STOCK_CANCEL,
				  NULL, NULL);
    gtk_box_pack_end(GTK_BOX(vbox), confirm_box, FALSE, FALSE, 0);
    gtk_widget_grab_default(ok_button);

    g_signal_connect(G_OBJECT(ok_button), "clicked",
                     G_CALLBACK(passphrase_ok_cb), NULL);
    g_signal_connect(G_OBJECT(pass_entry), "activate",
                     G_CALLBACK(passphrase_ok_cb), NULL);
    g_signal_connect(G_OBJECT(cancel_button), "clicked",
                     G_CALLBACK(passphrase_cancel_cb), NULL);

    gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    if (grab_all)   
        gtk_window_set_policy (GTK_WINDOW(window), FALSE, FALSE, TRUE);
    
    gtk_widget_show_all(window);

    if (grab_all) {
        /* make sure that window is viewable
	 * FIXME: this is still not enough */
        gtk_widget_show_now(window);
	gdk_flush();
#ifdef GDK_WINDOWING_X11
	gdk_x11_display_grab(gdk_display_get_default());
#endif /* GDK_WINDOWING_X11 */
        if (gdk_pointer_grab(window->window, TRUE, 0,
                             window->window, NULL, GDK_CURRENT_TIME)) {
#ifdef GDK_WINDOWING_X11
            gdk_x11_display_ungrab(gdk_display_get_default());
#endif /* GDK_WINDOWING_X11 */
            g_warning("OOPS: Could not grab mouse\n");
            gtk_widget_destroy(window);
            return NULL;
        }
        if (gdk_keyboard_grab(window->window, FALSE, GDK_CURRENT_TIME)) {
            gdk_display_pointer_ungrab(gdk_display_get_default(),
			 	       GDK_CURRENT_TIME);
#ifdef GDK_WINDOWING_X11
            gdk_x11_display_ungrab(gdk_display_get_default());
#endif /* GDK_WINDOWING_X11 */
            g_warning("OOPS: Could not grab keyboard\n");
            gtk_widget_destroy(window);
            return NULL;
        }
    }

    gtk_main();

    if (grab_all) {
        gdk_display_keyboard_ungrab(gdk_display_get_default(),
				    GDK_CURRENT_TIME);
        gdk_display_pointer_ungrab(gdk_display_get_default(), GDK_CURRENT_TIME);
#ifdef GDK_WINDOWING_X11
        gdk_x11_display_ungrab(gdk_display_get_default());
#endif /* GDK_WINDOWING_X11 */
        gdk_flush();
    }

    manage_window_focus_out(window, NULL, NULL);

    if (pass_ack) {
        const gchar *entry_text;
        entry_text = gtk_entry_get_text(GTK_ENTRY(pass_entry));
        the_passphrase = g_locale_from_utf8(entry_text, -1, NULL, NULL, NULL);
        if (!the_passphrase)
            the_passphrase = g_strdup(entry_text);
    }
    gtk_widget_destroy(window);

    return the_passphrase;
}


static void 
passphrase_ok_cb(GtkWidget *widget, gpointer data)
{
    pass_ack = TRUE;
    gtk_main_quit();
}

static void 
passphrase_cancel_cb(GtkWidget *widget, gpointer data)
{
    pass_ack = FALSE;
    gtk_main_quit();
}


static gint
passphrase_deleted(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
    passphrase_cancel_cb(NULL, NULL);
    return TRUE;
}


static gboolean
passphrase_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event && event->keyval == GDK_Escape)
        passphrase_cancel_cb(NULL, NULL);
    return FALSE;
}

static gint 
linelen (const gchar *s)
{
    gint i;

    for (i = 0; *s && *s != '\n'; s++, i++)
        ;

    return i;
}

static GtkWidget *
create_description(const gchar *uid_hint, const gchar *pass_hint, gint prev_bad)
{
    const gchar *uid = NULL, *info = NULL;
    gchar *buf;
    GtkWidget *label;

    if (!uid_hint)
        uid = _("[no user id]");
    else
        uid = uid_hint;
    if (!pass_hint)
        info = "";
    else
        info = pass_hint;

    buf = g_strdup_printf (_("%sPlease enter the passphrase for:\n\n"
                           "  %.*s  \n"
                           "(%.*s)\n"),
                           prev_bad ?
                           _("Bad passphrase! Try again...\n\n") : "",
                           linelen (uid), uid, linelen (info), info);

    label = gtk_label_new (buf);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    g_free (buf);

    return label;
}

static int free_passphrase(gpointer _unused)
{
    if (last_pass != NULL) {
#if HAVE_MLOCK
        munlock(last_pass, strlen(last_pass));
#endif
        g_free(last_pass);
        last_pass = NULL;
        debug_print("%% passphrase removed\n");
    }
    
    return FALSE;
}

gpgme_error_t
gpgmegtk_passphrase_cb(void *opaque, const char *uid_hint,
        const char *passphrase_hint, int prev_bad, int fd)
{
    const char *pass;
#if defined(G_OS_WIN32) && !defined(HAVE_GPGME_IO_WRITEN)
    HANDLE hd = (HANDLE)fd;
    DWORD n;
#endif

    if (prefs_common.store_passphrase && last_pass != NULL && !prev_bad) {
#ifdef HAVE_GPGME_IO_WRITEN
        gpgme_io_writen(fd, last_pass, strlen(last_pass));
        gpgme_io_writen(fd, "\n", 1);
#elif defined(G_OS_WIN32)
        WriteFile(hd, last_pass, strlen(last_pass), &n, NULL);
        WriteFile(hd, "\n", 1, &n, NULL);
#else
        write(fd, last_pass, strlen(last_pass));
        write(fd, "\n", 1);
#endif
        return GPG_ERR_NO_ERROR;
    }
    gpgmegtk_set_passphrase_grab (prefs_common.passphrase_grab);
    debug_print ("%% requesting passphrase for `%s':\n", uid_hint);
    pass = passphrase_mbox (uid_hint, passphrase_hint, prev_bad);
    gpgmegtk_free_passphrase();
    if (!pass) {
        debug_print ("%% cancel passphrase entry\n");
#ifdef HAVE_GPGME_IO_WRITEN
        gpgme_io_writen(fd, "\n", 1);
#elif defined(G_OS_WIN32)
        WriteFile(hd, "\n", 1, &n, NULL);
        CloseHandle(hd); /* somehow it will block without this */
#else
        write(fd, "\n", 1);
#endif
        return GPG_ERR_CANCELED;
    }
    else {
        if (prefs_common.store_passphrase) {
            last_pass = g_strdup(pass);
#if HAVE_MLOCK
            if (mlock(last_pass, strlen(last_pass)) == -1)
                debug_print("%% locking passphrase failed\n");
#endif

            if (prefs_common.store_passphrase_timeout > 0) {
                g_timeout_add_full(G_PRIORITY_LOW, prefs_common.store_passphrase_timeout * 60 * 1000, free_passphrase, NULL, NULL);
            }
        }
        debug_print ("%% sending passphrase\n");
    }
#ifdef HAVE_GPGME_IO_WRITEN
    gpgme_io_writen(fd, pass, strlen(pass));
    gpgme_io_writen(fd, "\n", 1);
#elif defined(G_OS_WIN32)
    WriteFile(hd, pass, strlen(pass), &n, NULL);
    WriteFile(hd, "\n", 1, &n, NULL);
#else
    write(fd, pass, strlen(pass));
    write(fd, "\n", 1);
#endif
    return GPG_ERR_NO_ERROR;
}

void gpgmegtk_free_passphrase()
{
    (void)free_passphrase(NULL); // could be inline
}

#endif /* USE_GPGME */
