/* select-keys.c - GTK+ based key selection
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

#ifdef USE_GPGME
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkclist.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>

#include "intl.h"
#include "select-keys.h"
#include "utils.h"
#include "gtkutils.h"
#include "inputdialog.h"
#include "manage_window.h"

#define DIM(v) (sizeof(v)/sizeof((v)[0]))
#define DIMof(type,member)   DIM(((type *)0)->member)


enum col_titles { 
    COL_ALGO,
    COL_KEYID,
    COL_NAME,
    COL_EMAIL,
    COL_VALIDITY,

    N_COL_TITLES
};

struct select_keys_s {
    int okay;
    GtkWidget *window;
    GtkLabel *toplabel;
    GtkCList *clist;
    const char *pattern;
    GpgmeRecipients rset;
    GpgmeCtx select_ctx;

    GtkSortType sort_type;
    enum col_titles sort_column;
    
};


static void set_row (GtkCList *clist, GpgmeKey key);
static void fill_clist (struct select_keys_s *sk, const char *pattern);
static void create_dialog (struct select_keys_s *sk);
static void open_dialog (struct select_keys_s *sk);
static void close_dialog (struct select_keys_s *sk);
static gint delete_event_cb (GtkWidget *widget,
                             GdkEventAny *event, gpointer data);
static gboolean key_pressed_cb (GtkWidget *widget,
                                GdkEventKey *event, gpointer data);
static void select_btn_cb (GtkWidget *widget, gpointer data);
static void cancel_btn_cb (GtkWidget *widget, gpointer data);
static void other_btn_cb (GtkWidget *widget, gpointer data);
static void sort_keys (struct select_keys_s *sk, enum col_titles column);
static void sort_keys_name (GtkWidget *widget, gpointer data);
static void sort_keys_email (GtkWidget *widget, gpointer data);


static void
update_progress (struct select_keys_s *sk, int running, const char *pattern)
{
    static int windmill[] = { '-', '\\', '|', '/' };
    char *buf;

    if (!running)
        buf = g_strdup_printf (_("Please select key for `%s'"), 
                               pattern);
    else 
        buf = g_strdup_printf (_("Collecting info for `%s' ... %c"), 
                               pattern,
                               windmill[running%DIM(windmill)]);
    gtk_label_set_text (sk->toplabel, buf);
    g_free (buf);
}


/**
 * select_keys_get_recipients:
 * @recp_names: A list of email addresses
 * 
 * Select a list of recipients from a given list of email addresses.
 * This may pop up a window to present the user a choice, it will also
 * check that the recipients key are all valid.
 * 
 * Return value: NULL on error or a list of list of recipients.
 **/
GpgmeRecipients
gpgmegtk_recipient_selection (GSList *recp_names)
{
    struct select_keys_s sk;
    GpgmeError err;

    memset (&sk, 0, sizeof sk);

    err = gpgme_recipients_new (&sk.rset);
    if (err) {
        g_warning ("failed to allocate recipients set: %s",
                   gpgme_strerror (err));
        return NULL;
    }

    open_dialog (&sk);

    do {
        sk.pattern = recp_names? recp_names->data:NULL;
        gtk_clist_clear (sk.clist);
        fill_clist (&sk, sk.pattern);
        update_progress (&sk, 0, sk.pattern);
        gtk_main ();
        if (recp_names)
            recp_names = recp_names->next;
    } while (sk.okay && recp_names);

    close_dialog (&sk);

    if (!sk.okay) {
        gpgme_recipients_release (sk.rset);
        sk.rset = NULL;
    }
    return sk.rset;
} 

static void
destroy_key (gpointer data)
{
    GpgmeKey key = data;
    gpgme_key_release (key);
}

static void
set_row (GtkCList *clist, GpgmeKey key)
{
    const char *s;
    const char *text[N_COL_TITLES];
    char *algo_buf;
    int row;

    /* first check whether the key is capable of encryption which is not
     * the case for revoked, expired or sign-only keys */
    if ( !gpgme_key_get_ulong_attr (key, GPGME_ATTR_CAN_ENCRYPT, NULL, 0 ) )
        return;
    
    algo_buf = g_strdup_printf ("%lu/%s", 
         gpgme_key_get_ulong_attr (key, GPGME_ATTR_LEN, NULL, 0 ),
         gpgme_key_get_string_attr (key, GPGME_ATTR_ALGO, NULL, 0 ) );
    text[COL_ALGO] = algo_buf;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_KEYID, NULL, 0);
    if (strlen (s) == 16)
        s += 8; /* show only the short keyID */
    text[COL_KEYID] = s;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_NAME, NULL, 0);
    text[COL_NAME] = s;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_EMAIL, NULL, 0);
    text[COL_EMAIL] = s;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_VALIDITY, NULL, 0);
    text[COL_VALIDITY] = s;

    row = gtk_clist_append (clist, (gchar**)text);
    g_free (algo_buf);

    gtk_clist_set_row_data_full (clist, row, key, destroy_key);
}


static void 
fill_clist (struct select_keys_s *sk, const char *pattern)
{
    GtkCList *clist;
    GpgmeCtx ctx;
    GpgmeError err;
    GpgmeKey key;
    int running=0;

    g_return_if_fail (sk);
    clist = sk->clist;
    g_return_if_fail (clist);

    debug_print ("select_keys:fill_clist:  pattern `%s'\n", pattern);

    /*gtk_clist_freeze (select_keys.clist);*/
    err = gpgme_new (&ctx);
    g_assert (!err);

    sk->select_ctx = ctx;

    update_progress (sk, ++running, pattern);
    while (gtk_events_pending ())
        gtk_main_iteration ();

    err = gpgme_op_keylist_start (ctx, pattern, 0);
    if (err) {
        debug_print ("** gpgme_op_keylist_start(%s) failed: %s",
                     pattern, gpgme_strerror (err));
        sk->select_ctx = NULL;
        return;
    }
    update_progress (sk, ++running, pattern);
    while ( !(err = gpgme_op_keylist_next ( ctx, &key )) ) {
        debug_print ("%% %s:%d:  insert\n", __FILE__ ,__LINE__ );
        set_row (clist, key ); key = NULL;
        update_progress (sk, ++running, pattern);
        while (gtk_events_pending ())
            gtk_main_iteration ();
    }
    debug_print ("%% %s:%d:  ready\n", __FILE__ ,__LINE__ );
    if (err != GPGME_EOF)
        debug_print ("** gpgme_op_keylist_next failed: %s",
                     gpgme_strerror (err));
    sk->select_ctx = NULL;
    gpgme_release (ctx);
    /*gtk_clist_thaw (select_keys.clist);*/
}


static void 
create_dialog (struct select_keys_s *sk)
{
    GtkWidget *window;
    GtkWidget *vbox, *vbox2, *hbox;
    GtkWidget *bbox;
    GtkWidget *scrolledwin;
    GtkWidget *clist;
    GtkWidget *label;
    GtkWidget *select_btn, *cancel_btn, *other_btn;
    const char *titles[N_COL_TITLES];

    g_assert (!sk->window);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request (window, 520, 280);
    gtk_container_set_border_width (GTK_CONTAINER (window), 8);
    gtk_window_set_title (GTK_WINDOW (window), _("Select Keys"));
    gtk_window_set_modal (GTK_WINDOW (window), TRUE);
    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK (delete_event_cb), sk);
    g_signal_connect (G_OBJECT (window), "key_press_event",
                      G_CALLBACK (key_pressed_cb), sk);
    MANAGE_WINDOW_SIGNALS_CONNECT (window);

    vbox = gtk_vbox_new (FALSE, 8);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    hbox  = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    label = gtk_label_new ( "" );
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);

    scrolledwin = gtk_scrolled_window_new (NULL, NULL);
    gtk_box_pack_start (GTK_BOX (hbox), scrolledwin, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    titles[COL_ALGO]     = _("Size");
    titles[COL_KEYID]    = _("Key ID");
    titles[COL_NAME]     = _("Name");
    titles[COL_EMAIL]    = _("Address");
    titles[COL_VALIDITY] = _("Val");

    clist = gtk_clist_new_with_titles (N_COL_TITLES, (char**)titles);
    gtk_container_add (GTK_CONTAINER (scrolledwin), clist);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_ALGO,      72);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_KEYID,     76);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_NAME,     130);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_EMAIL,    130);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_VALIDITY,  20);
    gtk_clist_set_selection_mode (GTK_CLIST(clist), GTK_SELECTION_BROWSE);
    g_signal_connect (G_OBJECT(GTK_CLIST(clist)->column[COL_NAME].button),
		      "clicked",
                      G_CALLBACK(sort_keys_name), sk);
    g_signal_connect (G_OBJECT(GTK_CLIST(clist)->column[COL_EMAIL].button),
		      "clicked",
                      G_CALLBACK(sort_keys_email), sk);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    gtkut_stock_button_set_create (&bbox, 
                                   &select_btn, _("Select"),
                                   &cancel_btn, GTK_STOCK_CANCEL,
                                   &other_btn,  _("Other"));
    gtk_box_pack_end (GTK_BOX (hbox), bbox, FALSE, FALSE, 0);
    gtk_widget_grab_default (select_btn);

    g_signal_connect (G_OBJECT (select_btn), "clicked",
                      G_CALLBACK (select_btn_cb), sk);
    g_signal_connect (G_OBJECT(cancel_btn), "clicked",
                      G_CALLBACK (cancel_btn_cb), sk);
    g_signal_connect (G_OBJECT (other_btn), "clicked",
                      G_CALLBACK (other_btn_cb), sk);

    vbox2 = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);

    gtk_widget_show_all (window);

    sk->window = window;
    sk->toplabel = GTK_LABEL (label);
    sk->clist  = GTK_CLIST (clist);
}


static void
open_dialog (struct select_keys_s *sk)
{
    if (!sk->window)
        create_dialog (sk);
    manage_window_set_transient (GTK_WINDOW (sk->window));
    sk->okay = 0;
    sk->sort_column = N_COL_TITLES; /* use an invalid value */
    sk->sort_type = GTK_SORT_ASCENDING;
    gtk_widget_show (sk->window);
}


static void
close_dialog (struct select_keys_s *sk)
{
    g_return_if_fail (sk);
    gtk_widget_destroy (sk->window);
    sk->window = NULL;
}


static gint
delete_event_cb (GtkWidget *widget, GdkEventAny *event, gpointer data)
{
    struct select_keys_s *sk = data;

    sk->okay = 0;
    gtk_main_quit ();

    return TRUE;
}


static gboolean
key_pressed_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    struct select_keys_s *sk = data;

    g_return_val_if_fail (sk, FALSE);
    if (event && event->keyval == GDK_Escape) {
        sk->okay = 0;
        gtk_main_quit ();
    }
    return FALSE;
}


static void 
select_btn_cb (GtkWidget *widget, gpointer data)
{
    struct select_keys_s *sk = data;
    int row;
    GpgmeKey key;

    g_return_if_fail (sk);
    if (!sk->clist->selection) {
        debug_print ("** nothing selected");
        return;
    }
    row = GPOINTER_TO_INT(sk->clist->selection->data);
    key = gtk_clist_get_row_data(sk->clist, row);
    if (key) {
        const char *s = gpgme_key_get_string_attr (key,
                                                   GPGME_ATTR_FPR,
                                                   NULL, 0 );
        if ( gpgme_key_get_ulong_attr (key, GPGME_ATTR_VALIDITY, NULL, 0 )
             < GPGME_VALIDITY_FULL ) {
            debug_print ("** FIXME: we are faking the trust calculation");
        }
        if (!gpgme_recipients_add_name_with_validity (sk->rset, s,
                                                      GPGME_VALIDITY_FULL) ) {
            sk->okay = 1;
            gtk_main_quit ();
        }
    }
}


static void 
cancel_btn_cb (GtkWidget *widget, gpointer data)
{
    struct select_keys_s *sk = data;

    g_return_if_fail (sk);
    sk->okay = 0;
    if (sk->select_ctx)
        gpgme_cancel (sk->select_ctx);
    gtk_main_quit ();
}


static void
other_btn_cb (GtkWidget *widget, gpointer data)
{
    struct select_keys_s *sk = data;
    char *uid;

    g_return_if_fail (sk);
    uid = input_dialog ( _("Add key"),
                         _("Enter another user or key ID:"),
                         NULL );
    if (!uid)
        return;
    fill_clist (sk, uid);
    update_progress (sk, 0, sk->pattern);
    g_free (uid);
}


static gint 
cmp_attr (gconstpointer pa, gconstpointer pb, GpgmeAttr attr)
{
    GpgmeKey a = ((GtkCListRow *)pa)->data;
    GpgmeKey b = ((GtkCListRow *)pb)->data;
    const char *sa, *sb;
    
    sa = a? gpgme_key_get_string_attr (a, attr, NULL, 0 ) : NULL;
    sb = b? gpgme_key_get_string_attr (b, attr, NULL, 0 ) : NULL;
    if (!sa)
        return !!sb;
    if (!sb)
        return -1;
    return strcasecmp(sa, sb);
}

static gint 
cmp_name (GtkCList *clist, gconstpointer pa, gconstpointer pb)
{
    return cmp_attr (pa, pb, GPGME_ATTR_NAME);
}

static gint 
cmp_email (GtkCList *clist, gconstpointer pa, gconstpointer pb)
{
    return cmp_attr (pa, pb, GPGME_ATTR_EMAIL);
}

static void
sort_keys ( struct select_keys_s *sk, enum col_titles column)
{
    GtkCList *clist = sk->clist;

    switch (column) {
      case COL_NAME:
        gtk_clist_set_compare_func (clist, cmp_name);
        break;
      case COL_EMAIL:
        gtk_clist_set_compare_func (clist, cmp_email);
        break;
      default:
        return;
    }

    /* column clicked again: toggle as-/decending */
    if ( sk->sort_column == column) {
        sk->sort_type = sk->sort_type == GTK_SORT_ASCENDING ?
                        GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
    }
    else
        sk->sort_type = GTK_SORT_ASCENDING;

    sk->sort_column = column;
    gtk_clist_set_sort_type (clist, sk->sort_type);
    gtk_clist_sort (clist);
}

static void
sort_keys_name (GtkWidget *widget, gpointer data)
{
    sort_keys ((struct select_keys_s*)data, COL_NAME);
}

static void
sort_keys_email (GtkWidget *widget, gpointer data)
{
    sort_keys ((struct select_keys_s*)data, COL_EMAIL);
}

#endif /*USE_GPGME*/
