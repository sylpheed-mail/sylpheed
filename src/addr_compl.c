/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 *
 * Copyright (C) 2000-2005 by Alfons Hoogervorst & The Sylpheed Claws Team.
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <string.h>
#include <ctype.h>

#include "xml.h"
#include "addr_compl.h"
#include "utils.h"
#include "addressbook.h"
#include "main.h"
#include "prefs_common.h"

/* How it works:
 *
 * The address book is read into memory. We set up an address list
 * containing all address book entries. Next we make the completion
 * list, which contains all the completable strings, and store a
 * reference to the address entry it belongs to.
 * After calling the g_completion_complete(), we get a reference
 * to a valid email address.  
 *
 * Completion is very simplified. We never complete on another prefix,
 * i.e. we neglect the next smallest possible prefix for the current
 * completion cache. This is simply done so we might break up the
 * addresses a little more (e.g. break up alfons@proteus.demon.nl into
 * something like alfons, proteus, demon, nl; and then completing on
 * any of those words).
 */ 
	
/* address_entry - structure which refers to the original address entry in the
 * address book 
 */
typedef struct
{
	gchar *name;
	gchar *address;
} address_entry;

/* completion_entry - structure used to complete addresses, with a reference
 * the the real address information.
 */
typedef struct
{
	gchar		*string; /* string to complete */
	address_entry	*ref;	 /* address the string belongs to  */
} completion_entry;

/*******************************************************************************/

static gint	    ref_count;		/* list ref count */
static GList 	   *completion_list;	/* list of strings to be checked */
static GList 	   *address_list;	/* address storage */
static GCompletion *completion;		/* completion object */

/* To allow for continuing completion we have to keep track of the state
 * using the following variables. No need to create a context object. */

static gint	    completion_count;		/* nr of addresses incl. the prefix */
static gint	    completion_next;		/* next prev address */
static GSList	   *completion_addresses;	/* unique addresses found in the
						   completion cache. */
static gchar	   *completion_prefix;		/* last prefix. (this is cached here
						 * because the prefix passed to g_completion
						 * is g_strdown()'ed */

/*******************************************************************************/


static void address_completion_entry_changed		(GtkEditable *editable,
							 gpointer     data);


/* completion_func() - used by GTK to find the string data to be used for 
 * completion 
 */
static gchar *completion_func(gpointer data)
{
	g_return_val_if_fail(data != NULL, NULL);

	return ((completion_entry *)data)->string;
} 

static void init_all(void)
{
	completion = g_completion_new(completion_func);
	g_return_if_fail(completion != NULL);
}

static void free_all(void)
{
	GList *walk;

	walk = g_list_first(completion_list);
	for (; walk != NULL; walk = g_list_next(walk)) {
		completion_entry *ce = (completion_entry *) walk->data;
		g_free(ce->string);
		g_free(ce);
	}
	g_list_free(completion_list);
	completion_list = NULL;

	walk = address_list;
	for (; walk != NULL; walk = g_list_next(walk)) {
		address_entry *ae = (address_entry *) walk->data;
		g_free(ae->name);
		g_free(ae->address);
		g_free(ae);
	}
	g_list_free(address_list);
	address_list = NULL;

	g_completion_free(completion);
	completion = NULL;
}

static gint address_entry_find_func(gconstpointer a, gconstpointer b)
{
	const address_entry *ae1 = a;
	const address_entry *ae2 = b;
	gint val;

	if (!a || !b)
		return -1;

	val = strcmp(ae1->name, ae2->name);
	if (val != 0)
		return val;
	val = strcmp(ae1->address, ae2->address);
	if (val != 0)
		return val;

	return 0;
}

static void add_completion_entry(const gchar *str, address_entry *ae)
{
	completion_entry *ce;

	if (!str || *str == '\0')
		return;
	if (!ae)
		return;

	ce = g_new0(completion_entry, 1);
	/* GCompletion list is case sensitive */
	ce->string = g_utf8_strdown(str, -1);
	ce->ref = ae;
	completion_list = g_list_append(completion_list, ce);
}

/* add_address() - adds address to the completion list. this function looks
 * complicated, but it's only allocation checks.
 */
static gint add_address(const gchar *name, const gchar *firstname, const gchar *lastname, const gchar *nickname, const gchar *address)
{
	address_entry *ae;
	GList         *found;

	if (!address || *address == '\0')
		return -1;

	/* debugg_print("add_address: [%s] [%s] [%s] [%s] [%s]\n", name, firstname, lastname, nickname, address); */

	ae = g_new0(address_entry, 1);
	ae->name    = g_strdup(name ? name : "");
	ae->address = g_strdup(address);		
	if ((found = g_list_find_custom(address_list, ae,
					address_entry_find_func))) {
		g_free(ae->name);
		g_free(ae->address);
		g_free(ae);
		ae = (address_entry *)found->data;
	} else
		address_list = g_list_append(address_list, ae);
 
	if (name) {
		const gchar *p = name;

		while (*p != '\0') {
			add_completion_entry(p, ae);
			while (*p && *p != '-' && *p != '.' && !g_ascii_isspace(*p))
				p++;
			while (*p == '-' || *p == '.' || g_ascii_isspace(*p))
				p++;
		}
	}
	add_completion_entry(firstname, ae);
	add_completion_entry(lastname, ae);
	add_completion_entry(nickname, ae);
	add_completion_entry(address, ae);

	return 0;
}

/* read_address_book()
 */ 
static void read_address_book(void) {	
	addressbook_load_completion_full( add_address );
}

/* start_address_completion() - returns the number of addresses 
 * that should be matched for completion.
 */
gint start_address_completion(void)
{
	clear_completion_cache();
	if (!ref_count) {
		init_all();
		/* open the address book */
		read_address_book();
		/* merge the completion entry list into g_completion */
		if (completion_list)
			g_completion_add_items(completion, completion_list);
	}
	ref_count++;
	debug_print("start_address_completion ref count %d\n", ref_count);

	return g_list_length(completion_list);
}

/* get_address_from_edit() - returns a possible address (or a part)
 * from an entry box. To make life easier, we only look at the last valid address 
 * component; address completion only works at the last string component in
 * the entry box. 
 */ 
gchar *get_address_from_edit(GtkEntry *entry, gint *start_pos)
{
	const gchar *edit_text, *p;
	gint cur_pos;
	gboolean in_quote = FALSE;
	gboolean in_bracket = FALSE;
	gchar *str;

	edit_text = gtk_entry_get_text(entry);
	if (edit_text == NULL) return NULL;

	cur_pos = gtk_editable_get_position(GTK_EDITABLE(entry));

	/* scan for a separator. doesn't matter if walk points at null byte. */
	for (p = g_utf8_offset_to_pointer(edit_text, cur_pos);
	     p > edit_text;
	     p = g_utf8_prev_char(p)) {
		if (*p == '"')
			in_quote ^= TRUE;
		else if (!in_quote) {
			if (!in_bracket && *p == ',')
				break;
			else if (*p == '>')
				in_bracket = TRUE;
			else if (*p == '<')
				in_bracket = FALSE;
		}
	}

	/* have something valid */
	if (g_utf8_strlen(p, -1) == 0)
		return NULL;

	/* now scan back until we hit a valid character */
	for (; *p && (*p == ',' || g_ascii_isspace(*p));
	     p = g_utf8_next_char(p))
		;

	if (g_utf8_strlen(p, -1) == 0)
		return NULL;

	if (start_pos) *start_pos = g_utf8_pointer_to_offset(edit_text, p);

	str = g_strdup(p);

	return str;
} 

/* replace_address_in_edit() - replaces an incompleted address with a completed one.
 */
void replace_address_in_edit(GtkEntry *entry, const gchar *newtext,
			     gint start_pos)
{
	gchar *origtext;

	if (!newtext) return;

	debug_print("replace_address_in_edit: %s\n", newtext);

	origtext = gtk_editable_get_chars(GTK_EDITABLE(entry), start_pos, -1);
	if (!strcmp(origtext, newtext)) {
		g_free(origtext);
		return;
	}
	g_free(origtext);

	g_signal_handlers_block_by_func
		(entry, address_completion_entry_changed, NULL);
	gtk_editable_delete_text(GTK_EDITABLE(entry), start_pos, -1);
	gtk_editable_insert_text(GTK_EDITABLE(entry), newtext, strlen(newtext),
				 &start_pos);
	gtk_editable_set_position(GTK_EDITABLE(entry), -1);
	g_signal_handlers_unblock_by_func
		(entry, address_completion_entry_changed, NULL);
}

#if 0
static gint insert_address_func(gconstpointer a, gconstpointer b)
{
	const address_entry *ae1 = a;
	const address_entry *ae2 = b;
	gchar *s1, *s2;
	gint val;

	if (!a || !b)
		return -1;

	s1 = g_utf8_casefold(ae1->address, -1);
	s2 = g_utf8_casefold(ae2->address, -1);
	val = g_utf8_collate(s1, s2);
	g_free(s2);
	g_free(s1);
	if (val != 0)
		return val;
	s1 = g_utf8_casefold(ae1->name, -1);
	s2 = g_utf8_casefold(ae2->name, -1);
	val = g_utf8_collate(s1, s2);
	g_free(s2);
	g_free(s1);
	if (val != 0)
		return val;

	return 0;
}
#endif

/* complete_address() - tries to complete an addres, and returns the
 * number of addresses found. use get_complete_address() to get one.
 * returns zero if no match was found, otherwise the number of addresses,
 * with the original prefix at index 0. 
 */
guint complete_address(const gchar *str)
{
	GList *result;
	gchar *d;
	guint  count, cpl;
	completion_entry *ce;

	g_return_val_if_fail(str != NULL, 0);

	clear_completion_cache();
	completion_prefix = g_strdup(str);

	/* g_completion is case sensitive */
	d = g_utf8_strdown(str, -1);
	result = g_completion_complete(completion, d, NULL);

	count = g_list_length(result);
	if (count) {
		/* create list with unique addresses  */
		for (cpl = 0, result = g_list_first(result);
		     result != NULL;
		     result = g_list_next(result)) {
			ce = (completion_entry *)(result->data);
			if (NULL == g_slist_find(completion_addresses,
						 ce->ref)) {
				cpl++;
				completion_addresses =
					g_slist_append(completion_addresses,
						       ce->ref);
#if 0
					g_slist_insert_sorted
						(completion_addresses, ce->ref,
						 insert_address_func);
#endif
			}
		}
		count = cpl + 1;	/* index 0 is the original prefix */
		completion_next = 1;	/* we start at the first completed one */
	} else {
		g_free(completion_prefix);
		completion_prefix = NULL;
	}

	completion_count = count;

	g_free(d);

	return count;
}

/* get_complete_address() - returns a complete address. the returned
 * string should be freed 
 */
gchar *get_complete_address(gint index)
{
	const address_entry *p;
	gchar *address = NULL;

	if (index < completion_count) {
		if (index == 0)
			address = g_strdup(completion_prefix);
		else {
			/* get something from the unique addresses */
			p = (address_entry *)g_slist_nth_data
				(completion_addresses, index - 1);
			if (p != NULL) {
				if (!p->name || p->name[0] == '\0')
					address = g_strdup(p->address);
				else if (p->name[0] != '"' &&
					 strpbrk(p->name, "(),.:;<>@[]") != NULL)
					address = g_strdup_printf
						("\"%s\" <%s>", p->name, p->address);
				else
					address = g_strdup_printf
						("%s <%s>", p->name, p->address);
			}
		}
	}

	return address;
}

gchar *get_next_complete_address(void)
{
	if (is_completion_pending()) {
		gchar *res;

		res = get_complete_address(completion_next);
		completion_next += 1;
		if (completion_next >= completion_count)
			completion_next = 0;

		return res;
	} else
		return NULL;
}

gchar *get_prev_complete_address(void)
{
	if (is_completion_pending()) {
		int n = completion_next - 2;

		/* real previous */
		n = (n + (completion_count * 5)) % completion_count;

		/* real next */
		completion_next = n + 1;
		if (completion_next >=  completion_count)
			completion_next = 0;
		return get_complete_address(n);
	} else
		return NULL;
}

guint get_completion_count(void)
{
	if (is_completion_pending())
		return completion_count;
	else
		return 0;
}

/* should clear up anything after complete_address() */
void clear_completion_cache(void)
{
	if (is_completion_pending()) {
		if (completion_prefix)
			g_free(completion_prefix);

		if (completion_addresses) {
			g_slist_free(completion_addresses);
			completion_addresses = NULL;
		}

		completion_count = completion_next = 0;
	}
}

gboolean is_completion_pending(void)
{
	/* check if completion pending, i.e. we might satisfy a request for the next
	 * or previous address */
	 return completion_count;
}

/* invalidate_address_completion() - should be called if address book
 * changed; 
 */
gint invalidate_address_completion(void)
{
	if (ref_count) {
		/* simply the same as start_address_completion() */
		debug_print("Invalidation request for address completion\n");
		free_all();
		init_all();
		read_address_book();
		if (completion_list)
			g_completion_add_items(completion, completion_list);
		clear_completion_cache();
	}

	return g_list_length(completion_list);
}

gint end_address_completion(void)
{
	clear_completion_cache();

	if (0 == --ref_count)
		free_all();

	debug_print("end_address_completion ref count %d\n", ref_count);

	return ref_count; 
}


/* address completion entry ui. the ui (completion list was inspired by galeon's
 * auto completion list). remaining things powered by sylpheed's completion engine.
 */

#define ENTRY_DATA_TAB_HOOK	"tab_hook"			/* used to lookup entry */
#define WINDOW_DATA_COMPL_ENTRY	"compl_entry"	/* used to store entry for compl. window */
#define WINDOW_DATA_COMPL_CLIST "compl_clist"	/* used to store clist for compl. window */

static void address_completion_mainwindow_set_focus	(GtkWindow   *window,
							 GtkWidget   *widget,
							 gpointer     data);
static gboolean address_completion_entry_key_pressed	(GtkEntry    *entry,
							 GdkEventKey *ev,
							 gpointer     data);
static gboolean address_completion_complete_address_in_entry
							(GtkEntry    *entry,
							 gboolean     next);
static void address_completion_create_completion_window	(GtkEntry    *entry,
							 gboolean     select_next);

static void completion_window_select_row(GtkCList	 *clist,
					 gint		  row,
					 gint		  col,
					 GdkEvent	 *event,
					 GtkWidget	**window);
static gboolean completion_window_button_press
					(GtkWidget	 *widget,
					 GdkEventButton  *event,
					 GtkWidget	**window);
static gboolean completion_window_key_press
					(GtkWidget	 *widget,
					 GdkEventKey	 *event,
					 GtkWidget	**window);


static void completion_window_advance_to_row(GtkCList *clist, gint row)
{
	g_return_if_fail(row < completion_count);
	gtk_clist_select_row(clist, row, 0);
}

static void completion_window_advance_selection(GtkCList *clist, gboolean forward)
{
	int row;

	g_return_if_fail(clist != NULL);
	g_return_if_fail(clist->selection != NULL);

	row = GPOINTER_TO_INT(clist->selection->data);

	row = forward ? (row + 1) % completion_count :
			(row - 1) < 0 ? completion_count - 1 : row - 1;

	gtk_clist_freeze(clist);
	completion_window_advance_to_row(clist, row);
	gtk_clist_thaw(clist);

#ifdef __APPLE__
	/* workaround for a draw bug in OS X */
	gtk_widget_queue_draw(GTK_WIDGET(clist));
#endif
}

#if 0
/* completion_window_accept_selection() - accepts the current selection in the
 * clist, and destroys the window */
static void completion_window_accept_selection(GtkWidget **window,
					       GtkCList *clist,
					       GtkEntry *entry)
{
	gchar *address = NULL, *text = NULL;
	gint   cursor_pos, row;

	g_return_if_fail(window != NULL);
	g_return_if_fail(*window != NULL);
	g_return_if_fail(clist != NULL);
	g_return_if_fail(entry != NULL);
	g_return_if_fail(clist->selection != NULL);

	/* FIXME: I believe it's acceptable to access the selection member directly  */
	row = GPOINTER_TO_INT(clist->selection->data);

	/* we just need the cursor position */
	address = get_address_from_edit(entry, &cursor_pos);
	g_free(address);
	gtk_clist_get_text(clist, row, 0, &text);
	replace_address_in_edit(entry, text, cursor_pos);

	clear_completion_cache();
	gtk_widget_destroy(*window);
	*window = NULL;
}
#endif

/* completion_window_apply_selection() - apply the current selection in the
 * clist */
static void completion_window_apply_selection(GtkCList *clist, GtkEntry *entry)
{
	gchar *address = NULL, *text = NULL;
	gint   cursor_pos, row;

	g_return_if_fail(clist != NULL);
	g_return_if_fail(entry != NULL);
	g_return_if_fail(clist->selection != NULL);

	row = GPOINTER_TO_INT(clist->selection->data);

	address = get_address_from_edit(entry, &cursor_pos);
	g_free(address);
	gtk_clist_get_text(clist, row, 0, &text);
	replace_address_in_edit(entry, text, cursor_pos);
}

static void completion_window_apply_selection_address_only(GtkCList *clist, GtkEntry *entry)
{
	gchar *address = NULL;
	address_entry *ae;
	gint   cursor_pos, row;

	g_return_if_fail(clist != NULL);
	g_return_if_fail(entry != NULL);
	g_return_if_fail(clist->selection != NULL);

	row = GPOINTER_TO_INT(clist->selection->data);

	ae = (address_entry *)g_slist_nth_data(completion_addresses, row - 1);
	if (ae && ae->address) {
		address = get_address_from_edit(entry, &cursor_pos);
		g_free(address);
		replace_address_in_edit(entry, ae->address, cursor_pos);
	}
}

/* should be called when creating the main window containing address
 * completion entries */
void address_completion_start(GtkWidget *mainwindow)
{
	start_address_completion();

	/* register focus change hook */
	g_signal_connect(G_OBJECT(mainwindow), "set_focus",
			 G_CALLBACK(address_completion_mainwindow_set_focus),
			 mainwindow);
}

/* Need unique data to make unregistering signal handler possible for the auto
 * completed entry */
#define COMPLETION_UNIQUE_DATA (GINT_TO_POINTER(0xfeefaa))

void address_completion_register_entry(GtkEntry *entry)
{
	g_return_if_fail(entry != NULL);
	g_return_if_fail(GTK_IS_ENTRY(entry));

	/* add hooked property */
	g_object_set_data(G_OBJECT(entry), ENTRY_DATA_TAB_HOOK, entry);

	/* add keypress event */
	g_signal_connect_closure
		(G_OBJECT(entry), "key_press_event",
		 g_cclosure_new
			(G_CALLBACK(address_completion_entry_key_pressed),
			 COMPLETION_UNIQUE_DATA, NULL),
		 FALSE);
	g_signal_connect(G_OBJECT(entry), "changed",
			 G_CALLBACK(address_completion_entry_changed),
			 NULL);
}

void address_completion_unregister_entry(GtkEntry *entry)
{
	GObject *entry_obj;

	g_return_if_fail(entry != NULL);
	g_return_if_fail(GTK_IS_ENTRY(entry));

	entry_obj = g_object_get_data(G_OBJECT(entry), ENTRY_DATA_TAB_HOOK);
	g_return_if_fail(entry_obj);
	g_return_if_fail(entry_obj == G_OBJECT(entry));

	/* has the hooked property? */
	g_object_set_data(G_OBJECT(entry), ENTRY_DATA_TAB_HOOK, NULL);

	/* remove the hook */
	g_signal_handlers_disconnect_by_func
		(G_OBJECT(entry), 
		 G_CALLBACK(address_completion_entry_key_pressed),
		 COMPLETION_UNIQUE_DATA);
}

/* should be called when main window with address completion entries
 * terminates.
 * NOTE: this function assumes that it is called upon destruction of
 * the window */
void address_completion_end(GtkWidget *mainwindow)
{
	/* if address_completion_end() is really called on closing the window,
	 * we don't need to unregister the set_focus_cb */
	end_address_completion();
}

/* if focus changes to another entry, then clear completion cache */
static void address_completion_mainwindow_set_focus(GtkWindow *window,
						    GtkWidget *widget,
						    gpointer   data)
{
	if (widget && GTK_IS_ENTRY(widget) &&
	    g_object_get_data(G_OBJECT(widget), ENTRY_DATA_TAB_HOOK)) {
		clear_completion_cache();
	}
}

static GtkWidget *completion_window;

/* watch for tabs in one of the address entries. if no tab then clear the
 * completion cache */
static gboolean address_completion_entry_key_pressed(GtkEntry    *entry,
						     GdkEventKey *ev,
						     gpointer     data)
{
	if (!prefs_common.fullauto_completion_mode && ev->keyval == GDK_Tab &&
	    !completion_window) {
		if (address_completion_complete_address_in_entry(entry, TRUE)) {
			address_completion_create_completion_window(entry, TRUE);
			/* route a void character to the default handler */
			/* this is a dirty hack; we're actually changing a key
			 * reported by the system. */
			ev->keyval = GDK_AudibleBell_Enable;
			ev->state &= ~GDK_SHIFT_MASK;
			return TRUE;
		}
	}

	if (!completion_window)
		return FALSE;

	if (       ev->keyval == GDK_Up
		|| ev->keyval == GDK_Down
		|| ev->keyval == GDK_Page_Up
		|| ev->keyval == GDK_Page_Down
		|| ev->keyval == GDK_Return
		|| ev->keyval == GDK_Escape
		|| ev->keyval == GDK_Tab
		|| ev->keyval == GDK_ISO_Left_Tab) {
		completion_window_key_press(completion_window, ev, &completion_window);
		return TRUE;
	} else if (ev->keyval == GDK_Shift_L
		|| ev->keyval == GDK_Shift_R
		|| ev->keyval == GDK_Control_L
		|| ev->keyval == GDK_Control_R
		|| ev->keyval == GDK_Caps_Lock
		|| ev->keyval == GDK_Shift_Lock
		|| ev->keyval == GDK_Meta_L
		|| ev->keyval == GDK_Meta_R
		|| ev->keyval == GDK_Alt_L
		|| ev->keyval == GDK_Alt_R) {
		/* these buttons should not clear the cache... */
	} else {
		clear_completion_cache();
		gtk_widget_destroy(completion_window);
		completion_window = NULL;
	}

	return FALSE;
}

static void address_completion_entry_changed(GtkEditable *editable,
					     gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(editable);

	if (!prefs_common.fullauto_completion_mode)
		return;

	g_signal_handlers_block_by_func
		(editable, address_completion_entry_changed, data);
	if (address_completion_complete_address_in_entry(entry, TRUE)) {
		address_completion_create_completion_window(entry, FALSE);
	} else {
		clear_completion_cache();
		if (completion_window) {
			gtk_widget_destroy(completion_window);
			completion_window = NULL;
		}
	}
	g_signal_handlers_unblock_by_func
		(editable, address_completion_entry_changed, data);
}

/* initialize the completion cache and put first completed string
 * in entry. this function used to do back cycling but this is not
 * currently used. since the address completion behaviour has been
 * changed regularly, we keep the feature in case someone changes
 * his / her mind again. :) */
static gboolean address_completion_complete_address_in_entry(GtkEntry *entry,
							     gboolean  next)
{
	gint ncount = 0, cursor_pos;
	gchar *address, *new = NULL;
	gboolean completed = FALSE;

	g_return_val_if_fail(entry != NULL, FALSE);

	if (!GTK_WIDGET_HAS_FOCUS(entry)) return FALSE;

	/* get an address component from the cursor */
	address = get_address_from_edit(entry, &cursor_pos);
	if (!address) return FALSE;

	/* still something in the cache */
	if (is_completion_pending()) {
		new = next ? get_next_complete_address() :
			get_prev_complete_address();
	} else {
		if (0 < (ncount = complete_address(address)))
			new = get_next_complete_address();
	}

	if (new) {
		/* prevent "change" signal */
		/* replace_address_in_edit(entry, new, cursor_pos); */

		/* don't complete if entry equals to the completed address */
		if (ncount == 2 && !strcmp(address, new))
			completed = FALSE;
		else
			completed = TRUE;
		g_free(new);
	}

	g_free(address);

	return completed;
}

static void address_completion_create_completion_window(GtkEntry *entry_,
							gboolean select_next)
{
	gint x, y, width;
	GtkWidget *scroll, *clist;
	GtkRequisition r;
	guint count = 0;
	GtkWidget *entry = GTK_WIDGET(entry_);

	debug_print("address_completion_create_completion_window\n");

	if (completion_window) {
		gtk_widget_destroy(completion_window);
		completion_window = NULL;
	}

	scroll = gtk_scrolled_window_new(NULL, NULL);
	clist  = gtk_clist_new(1);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
	
	completion_window = gtk_window_new(GTK_WINDOW_POPUP);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(completion_window), scroll);
	gtk_container_add(GTK_CONTAINER(scroll), clist);

	/* set the unique data so we can always get back the entry and
	 * clist window to which this completion window has been attached */
	g_object_set_data(G_OBJECT(completion_window),
			  WINDOW_DATA_COMPL_ENTRY, entry_);
	g_object_set_data(G_OBJECT(completion_window),
			  WINDOW_DATA_COMPL_CLIST, clist);

	g_signal_connect(G_OBJECT(clist), "select_row",
			 G_CALLBACK(completion_window_select_row),
			 &completion_window);

	for (count = 0; count < get_completion_count(); count++) {
		gchar *text[] = {NULL, NULL};

		text[0] = get_complete_address(count);
		gtk_clist_append(GTK_CLIST(clist), text);
		g_free(text[0]);
	}

	gdk_window_get_origin(entry->window, &x, &y);
	gtk_widget_size_request(entry, &r);
	width = entry->allocation.width;
	y += r.height;
	gtk_window_move(GTK_WINDOW(completion_window), x, y);

	gtk_widget_size_request(clist, &r);
	gtk_widget_set_size_request(completion_window, width, r.height);
	gtk_widget_show_all(completion_window);
	gtk_widget_size_request(clist, &r);

	if ((y + r.height) > gdk_screen_height()) {
		gtk_window_set_policy(GTK_WINDOW(completion_window),
				      TRUE, FALSE, FALSE);
		gtk_widget_set_size_request(completion_window, width,
					    gdk_screen_height () - y);
	}

	g_signal_connect(G_OBJECT(completion_window),
			 "button-press-event",
			 G_CALLBACK(completion_window_button_press),
			 &completion_window);
	g_signal_connect(G_OBJECT(completion_window),
			 "key-press-event",
			 G_CALLBACK(completion_window_key_press),
			 &completion_window);
	gdk_pointer_grab(completion_window->window, TRUE,
			 GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK,
			 NULL, NULL, GDK_CURRENT_TIME);
	gtk_grab_add(completion_window);

	/* this gets rid of the irritating focus rectangle that doesn't
	 * follow the selection */
	GTK_WIDGET_UNSET_FLAGS(clist, GTK_CAN_FOCUS);
	gtk_clist_select_row(GTK_CLIST(clist), select_next ? 1 : 0, 0);

	debug_print("address_completion_create_completion_window done\n");
}


/* row selection sends completed address to entry.
 * note: event is NULL if selected by anything else than a mouse button. */
static void completion_window_select_row(GtkCList *clist, gint row, gint col,
					 GdkEvent *event,
					 GtkWidget **window)
{
	GtkEntry *entry;

	g_return_if_fail(window != NULL);
	g_return_if_fail(*window != NULL);

	entry = GTK_ENTRY(g_object_get_data(G_OBJECT(*window),
					    WINDOW_DATA_COMPL_ENTRY));
	g_return_if_fail(entry != NULL);

	completion_window_apply_selection(clist, entry);

	if (!event || event->type != GDK_BUTTON_RELEASE)
		return;

	clear_completion_cache();
	gtk_widget_destroy(*window);
	*window = NULL;
}

/* completion_window_button_press() - check is mouse click is anywhere
 * else (not in the completion window). in that case the completion
 * window is destroyed, and the original prefix is restored */
static gboolean completion_window_button_press(GtkWidget *widget,
					       GdkEventButton *event,
					       GtkWidget **window)
{
	GtkWidget *event_widget, *entry;
	gchar *prefix;
	gint cursor_pos;
	gboolean restore = TRUE;

	g_return_val_if_fail(window != NULL, FALSE);
	g_return_val_if_fail(*window != NULL, FALSE);

	entry = GTK_WIDGET(g_object_get_data(G_OBJECT(*window),
					     WINDOW_DATA_COMPL_ENTRY));
	g_return_val_if_fail(entry != NULL, FALSE);

	event_widget = gtk_get_event_widget((GdkEvent *)event);
	if (event_widget != widget) {
		while (event_widget) {
			if (event_widget == widget)
				return FALSE;
			else if (event_widget == entry) {
				restore = FALSE;
				break;
			}
		    event_widget = event_widget->parent;
		}
	}

	if (restore) {
		prefix = get_complete_address(0);
		g_free(get_address_from_edit(GTK_ENTRY(entry), &cursor_pos));
		replace_address_in_edit(GTK_ENTRY(entry), prefix, cursor_pos);
		g_free(prefix);
	}

	clear_completion_cache();
	gtk_widget_destroy(*window);
	*window = NULL;

	return TRUE;
}

static gboolean completion_window_key_press(GtkWidget *widget,
					    GdkEventKey *event,
					    GtkWidget **window)
{
	GtkWidget *entry;
	gchar *prefix;
	gint cursor_pos;
	GtkWidget *clist;

	g_return_val_if_fail(window != NULL, FALSE);
	g_return_val_if_fail(*window != NULL, FALSE);

	if (!is_completion_pending())
		g_warning("completion is not pending!\n");

	entry = GTK_WIDGET(g_object_get_data(G_OBJECT(*window),
					     WINDOW_DATA_COMPL_ENTRY));
	clist = GTK_WIDGET(g_object_get_data(G_OBJECT(*window),
					     WINDOW_DATA_COMPL_CLIST));
	g_return_val_if_fail(entry != NULL, FALSE);

	/* allow keyboard navigation in the alternatives clist */
	if (event->keyval == GDK_Up || event->keyval == GDK_Down ||
	    event->keyval == GDK_Page_Up || event->keyval == GDK_Page_Down) {
		completion_window_advance_selection
			(GTK_CLIST(clist),
			 event->keyval == GDK_Down ||
			 event->keyval == GDK_Page_Down ? TRUE : FALSE);
		return FALSE;
	}		

	/* also make tab / shift tab go to next previous completion entry. we're
	 * changing the key value */
	if (event->keyval == GDK_Tab || event->keyval == GDK_ISO_Left_Tab) {
		event->keyval = (event->state & GDK_SHIFT_MASK)
			? GDK_Up : GDK_Down;
		/* need to reset shift state if going up */
		if (event->state & GDK_SHIFT_MASK)
			event->state &= ~GDK_SHIFT_MASK;
		completion_window_advance_selection(GTK_CLIST(clist), 
			event->keyval == GDK_Down ? TRUE : FALSE);
		return FALSE;
	}

	/* look for presses that accept the selection */
	if (event->keyval == GDK_Return ||
	    (!prefs_common.fullauto_completion_mode &&
	     event->keyval == GDK_space)) {
		/* insert address only if shift or control is pressed */
		if (event->state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK) ||
		    prefs_common.always_add_address_only) {
			completion_window_apply_selection_address_only
				(GTK_CLIST(clist), GTK_ENTRY(entry));
		}
		clear_completion_cache();
		gtk_widget_destroy(*window);
		*window = NULL;
		return FALSE;
	}

	/* key state keys should never be handled */
	if (event->keyval == GDK_Shift_L
		 || event->keyval == GDK_Shift_R
		 || event->keyval == GDK_Control_L
		 || event->keyval == GDK_Control_R
		 || event->keyval == GDK_Caps_Lock
		 || event->keyval == GDK_Shift_Lock
		 || event->keyval == GDK_Meta_L
		 || event->keyval == GDK_Meta_R
		 || event->keyval == GDK_Alt_L
		 || event->keyval == GDK_Alt_R) {
		return FALSE;
	}

	/* other key, let's restore the prefix (orignal text) */
	if (!prefs_common.fullauto_completion_mode ||
	    event->keyval == GDK_Escape) {
		prefix = get_complete_address(0);
		g_free(get_address_from_edit(GTK_ENTRY(entry), &cursor_pos));
		replace_address_in_edit(GTK_ENTRY(entry), prefix, cursor_pos);
		g_free(prefix);
	}

	/* make sure anything we typed comes in the edit box */
	if ((!prefs_common.fullauto_completion_mode && event->length > 0 &&
	     event->keyval != GDK_Escape) ||
	    (prefs_common.fullauto_completion_mode &&
	     event->keyval != GDK_Escape)) {
		GtkWidget *pwin = entry;

		while ((pwin = gtk_widget_get_parent(pwin)) != NULL) {
			if (GTK_WIDGET_TOPLEVEL(pwin)) {
				gtk_window_propagate_key_event
					(GTK_WINDOW(pwin), event);
				if (prefs_common.fullauto_completion_mode)
					return TRUE;
			}
		}
	}

	/* and close the completion window */
	clear_completion_cache();
	gtk_widget_destroy(*window);
	*window = NULL;

	return TRUE;
}
