/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2016 Hiroyuki Yamamoto
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

#include <glib.h>
#include <gmodule.h>
#include <gtk/gtk.h>

#include "plugin.h"
#include "utils.h"
#include "folder.h"
#include "plugin-marshal.h"

G_DEFINE_TYPE(SylPlugin, syl_plugin, G_TYPE_OBJECT);

enum {
	PLUGIN_LOAD,
	PLUGIN_UNLOAD,
	FOLDERVIEW_MENU_POPUP,
	SUMMARYVIEW_MENU_POPUP,
	COMPOSE_CREATED,
	COMPOSE_DESTROY,
	TEXTVIEW_MENU_POPUP,
	COMPOSE_SEND,
	MESSAGEVIEW_SHOW,
	INC_MAIL_START,
	INC_MAIL_FINISHED,
	PREFS_COMMON_OPEN,
	PREFS_ACCOUNT_OPEN,
	PREFS_FILTER_OPEN,
	PREFS_FILTER_EDIT_OPEN,
	PREFS_TEMPLATE_OPEN,
	PLUGIN_MANAGER_OPEN,
	MAIN_WINDOW_TOOLBAR_CHANGED,
	COMPOSE_TOOLBAR_CHANGED,
	COMPOSE_ATTACH_CHANGED,
	LAST_SIGNAL
};

#define GETFUNC(sym)	{ func = syl_plugin_lookup_symbol(sym); }

#define SAFE_CALL(func_ptr)		{ if (func_ptr) func_ptr(); }
#define SAFE_CALL_RET(func_ptr)		(func_ptr ? func_ptr() : NULL)
#define SAFE_CALL_RET_VAL(func_ptr, retval) \
					(func_ptr ? func_ptr() : retval)
#define SAFE_CALL_ARG1(func_ptr, arg1)	{ if (func_ptr) func_ptr(arg1); }
#define SAFE_CALL_ARG1_RET(func_ptr, arg1) \
				(func_ptr ? func_ptr(arg1) : NULL)
#define SAFE_CALL_ARG1_RET_VAL(func_ptr, arg1, retval) \
				(func_ptr ? func_ptr(arg1) : retval)
#define SAFE_CALL_ARG2(func_ptr, arg1, arg2) \
				{ if (func_ptr) func_ptr(arg1, arg2); }
#define SAFE_CALL_ARG2_RET(func_ptr, arg1, arg2) \
				(func_ptr ? func_ptr(arg1, arg2) : NULL)
#define SAFE_CALL_ARG2_RET_VAL(func_ptr, arg1, arg2, retval) \
				(func_ptr ? func_ptr(arg1, arg2) : retval)
#define SAFE_CALL_ARG3(func_ptr, arg1, arg2, arg3) \
				{ if (func_ptr) func_ptr(arg1, arg2, arg3); }
#define SAFE_CALL_ARG3_RET(func_ptr, arg1, arg2, arg3) \
				(func_ptr ? func_ptr(arg1, arg2, arg3) : NULL)
#define SAFE_CALL_ARG3_RET_VAL(func_ptr, arg1, arg2, arg3, retval) \
				(func_ptr ? func_ptr(arg1, arg2, arg3) : retval)
#define SAFE_CALL_ARG4(func_ptr, arg1, arg2, arg3, arg4) \
				{ if (func_ptr) func_ptr(arg1, arg2, arg3, arg4); }
#define SAFE_CALL_ARG4_RET(func_ptr, arg1, arg2, arg3, arg4) \
				(func_ptr ? func_ptr(arg1, arg2, arg3, arg4) : NULL)
#define SAFE_CALL_ARG4_RET_VAL(func_ptr, arg1, arg2, arg3, arg4, retval) \
				(func_ptr ? func_ptr(arg1, arg2, arg3, arg4) : retval)

#define CALL_VOID_POINTER(getobj, sym)		\
{						\
	void (*func)(gpointer);			\
	gpointer obj;				\
	obj = getobj();				\
	if (obj) {				\
		GETFUNC(sym);			\
		SAFE_CALL_ARG1(func, obj);	\
	}					\
}


static guint plugin_signals[LAST_SIGNAL] = { 0 };

static GHashTable *sym_table = NULL;
static GSList *module_list = NULL;
static GObject *plugin_obj = NULL;

static void syl_plugin_init(SylPlugin *self)
{
}

static void syl_plugin_class_init(SylPluginClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	plugin_signals[PLUGIN_LOAD] =
		g_signal_new("plugin-load",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, plugin_load),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[PLUGIN_UNLOAD] =
		g_signal_new("plugin-unload",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, plugin_unload),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[FOLDERVIEW_MENU_POPUP] =
		g_signal_new("folderview-menu-popup",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass,
					     folderview_menu_popup),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[SUMMARYVIEW_MENU_POPUP] =
		g_signal_new("summaryview-menu-popup",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass,
					     summaryview_menu_popup),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[COMPOSE_CREATED] =
		g_signal_new("compose-created",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, compose_created),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[COMPOSE_DESTROY] =
		g_signal_new("compose-destroy",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, compose_destroy),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[TEXTVIEW_MENU_POPUP] =
		g_signal_new("textview-menu-popup",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass,
					     textview_menu_popup),
			     NULL, NULL,
			     syl_plugin_marshal_VOID__POINTER_POINTER_STRING_STRING_POINTER,
			     G_TYPE_NONE,
			     5,
			     G_TYPE_POINTER,
			     G_TYPE_POINTER,
			     G_TYPE_STRING,
			     G_TYPE_STRING,
			     G_TYPE_POINTER);
	plugin_signals[COMPOSE_SEND] =
		g_signal_new("compose-send",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(SylPluginClass, compose_send),
			     NULL, NULL,
			     syl_plugin_marshal_BOOLEAN__POINTER_INT_INT_STRING_POINTER,
			     G_TYPE_BOOLEAN,
			     5,
			     G_TYPE_POINTER,
			     G_TYPE_INT,
			     G_TYPE_INT,
			     G_TYPE_STRING,
			     G_TYPE_POINTER);
	plugin_signals[MESSAGEVIEW_SHOW] =
		g_signal_new("messageview-show",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, messageview_show),
			     NULL, NULL,
			     syl_plugin_marshal_VOID__POINTER_POINTER_BOOLEAN,
			     G_TYPE_NONE,
			     3,
			     G_TYPE_POINTER,
			     G_TYPE_POINTER,
			     G_TYPE_BOOLEAN);

	plugin_signals[INC_MAIL_START] =
		g_signal_new("inc-mail-start",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, inc_mail_start),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[INC_MAIL_FINISHED] =
		g_signal_new("inc-mail-finished",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, inc_mail_finished),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__INT,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_INT);

	plugin_signals[PREFS_COMMON_OPEN] =
		g_signal_new("prefs-common-open",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, prefs_common_open),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[PREFS_ACCOUNT_OPEN] =
		g_signal_new("prefs-account-open",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, prefs_account_open),
			     NULL, NULL,
			     syl_plugin_marshal_VOID__POINTER_POINTER,
			     G_TYPE_NONE,
			     2,
			     G_TYPE_POINTER,
			     G_TYPE_POINTER);
	plugin_signals[PREFS_FILTER_OPEN] =
		g_signal_new("prefs-filter-open",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, prefs_filter_open),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[PREFS_FILTER_EDIT_OPEN] =
		g_signal_new("prefs-filter-edit-open",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, prefs_filter_edit_open),
			     NULL, NULL,
			     syl_plugin_marshal_VOID__POINTER_STRING_STRING_POINTER,
			     G_TYPE_NONE,
			     4,
			     G_TYPE_POINTER,
			     G_TYPE_STRING,
			     G_TYPE_STRING,
			     G_TYPE_POINTER);
	plugin_signals[PREFS_TEMPLATE_OPEN] =
		g_signal_new("prefs-template-open",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, prefs_template_open),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[PLUGIN_MANAGER_OPEN] =
		g_signal_new("plugin-manager-open",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, plugin_manager_open),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[MAIN_WINDOW_TOOLBAR_CHANGED] =
		g_signal_new("main-window-toolbar-changed",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, main_window_toolbar_changed),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE,
			     0);
	plugin_signals[COMPOSE_TOOLBAR_CHANGED] =
		g_signal_new("compose-toolbar-changed",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, compose_toolbar_changed),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
	plugin_signals[COMPOSE_TOOLBAR_CHANGED] =
		g_signal_new("compose-attach-changed",
			     G_TYPE_FROM_CLASS(gobject_class),
			     G_SIGNAL_RUN_FIRST,
			     G_STRUCT_OFFSET(SylPluginClass, compose_attach_changed),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE,
			     1,
			     G_TYPE_POINTER);
}

void syl_plugin_signal_connect(const gchar *name, GCallback callback,
			       gpointer data)
{
	g_return_if_fail(plugin_obj != NULL);

	g_signal_connect(plugin_obj, name, callback, data);
}

void syl_plugin_signal_disconnect(gpointer func, gpointer data)
{
	g_return_if_fail(plugin_obj != NULL);

	g_signal_handlers_disconnect_by_func(plugin_obj, func, data);
}

void syl_plugin_signal_emit(const gchar *name, ...)
{
	guint signal_id;

	g_return_if_fail(plugin_obj != NULL);

	if (g_signal_parse_name(name, G_TYPE_FROM_INSTANCE(plugin_obj), &signal_id, NULL, FALSE)) {
		 \
				va_list var_args;
		va_start(var_args, name);
		g_signal_emit_valist(plugin_obj, signal_id, 0, var_args);
		va_end(var_args);
	} else
		g_warning("%s: signal '%s' not found", G_STRLOC, name);
}

gint syl_plugin_init_lib(void)
{
	if (!g_module_supported()) {
		g_warning("Plug-in is not supported.");
		return -1;
	}

	if (!sym_table) {
		sym_table = g_hash_table_new(g_str_hash, g_str_equal);
	}

	if (!plugin_obj) {
		plugin_obj = g_object_new(SYL_TYPE_PLUGIN, NULL);
	}

	return 0;
}

gint syl_plugin_load(const gchar *name)
{
	GModule *module;
	SylPluginLoadFunc load_func = NULL;
	gchar *file;

	g_return_val_if_fail(plugin_obj != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);

	debug_print("syl_plugin_load: loading %s\n", name);

	if (!g_path_is_absolute(name))
		file = g_strconcat(PLUGINDIR, G_DIR_SEPARATOR_S, name, NULL);
	else
		file = g_strdup(name);

	module = g_module_open(file, G_MODULE_BIND_LAZY);
	if (!module) {
		g_warning("Cannot open module: %s: %s", name, g_module_error());
		g_free(file);
		return -1;
	}
	if (g_slist_find(module_list, module)) {
		g_warning("Module %s is already loaded", name);
		g_free(file);
		return -1;
	}

	if (g_module_symbol(module, "plugin_load", (gpointer)&load_func)) {
		if (!syl_plugin_check_version(module)) {
			g_warning("Version check failed. Skipping: %s", name);
			g_module_close(module);
			g_free(file);
			return -1;
		}

		debug_print("calling plugin_load() in %s\n",
			    g_module_name(module));
		load_func();
		module_list = g_slist_prepend(module_list, module);
		g_signal_emit(plugin_obj, plugin_signals[PLUGIN_LOAD], 0, module);
	} else {
		g_warning("Cannot get symbol: %s: %s", name, g_module_error());
		g_module_close(module);
		g_free(file);
		return -1;
	}

	g_free(file);

	return 0;
}

gint syl_plugin_load_all(const gchar *dir)
{
	GDir *d;
	const gchar *dir_name;
	gchar *path;
	gint count = 0;

	g_return_val_if_fail(dir != NULL, -1);

	debug_print("loading plugins from directory: %s\n", dir);

	if ((d = g_dir_open(dir, 0, NULL)) == NULL) {
		debug_print("failed to open directory: %s\n", dir);
		return -1;
	}

	while ((dir_name = g_dir_read_name(d)) != NULL) {
		if (!g_str_has_suffix(dir_name, "." G_MODULE_SUFFIX))
			continue;
		path = g_strconcat(dir, G_DIR_SEPARATOR_S, dir_name, NULL);
		if (syl_plugin_load(path) == 0)
			count++;
		g_free(path);
	}

	g_dir_close(d);

	return count;
}

void syl_plugin_unload_all(void)
{
	GSList *cur;

	g_return_if_fail(plugin_obj != NULL);

	for (cur = module_list; cur != NULL; cur = cur->next) {
		GModule *module = (GModule *)cur->data;
		SylPluginUnloadFunc unload_func = NULL;

		if (g_module_symbol(module, "plugin_unload",
				    (gpointer)&unload_func)) {
			g_signal_emit(plugin_obj, plugin_signals[PLUGIN_UNLOAD],
				      0, module);
			debug_print("calling plugin_unload() in %s\n",
				    g_module_name(module));
			unload_func();
		} else {
			g_warning("Cannot get symbol: %s", g_module_error());
		}
		if (!g_module_close(module)) {
			g_warning("Module unload failed: %s", g_module_error());
		}
	}

	g_slist_free(module_list);
	module_list = NULL;
}

GSList *syl_plugin_get_module_list(void)
{
	return module_list;
}

SylPluginInfo *syl_plugin_get_info(GModule *module)
{
	SylPluginInfo * (*plugin_info_func)(void);

	g_return_val_if_fail(module != NULL, NULL);

	debug_print("getting plugin information of %s\n",
		    g_module_name(module));

	if (g_module_symbol(module, "plugin_info",
			    (gpointer)&plugin_info_func)) {
		debug_print("calling plugin_info() in %s\n",
			    g_module_name(module));
		return plugin_info_func();
	} else {
		g_warning("Cannot get symbol: %s: %s", g_module_name(module),
			  g_module_error());
		return NULL;
	}
}

gboolean syl_plugin_check_version(GModule *module)
{
	gint (*version_func)(void);
	gint ver;
	gint a_major;
	gint a_minor;
	gint p_major;
	gint p_minor;

	g_return_val_if_fail(module != NULL, FALSE);

	if (g_module_symbol(module, "plugin_interface_version",
			    (gpointer)&version_func)) {
		debug_print("calling plugin_interface_version() in %s\n",
			    g_module_name(module));
		ver = version_func();
	} else {
		g_warning("Cannot get symbol: %s: %s", g_module_name(module),
			  g_module_error());
		return FALSE;
	}

	a_major = SYL_PLUGIN_INTERFACE_VERSION & 0xff00;
	a_minor = SYL_PLUGIN_INTERFACE_VERSION & 0x00ff;
	p_major = ver & 0xff00;
	p_minor = ver & 0x00ff;
	if (a_major == p_major && a_minor >= p_minor) {
		debug_print("Version OK: plugin: %d, app: %d\n",
			    ver, SYL_PLUGIN_INTERFACE_VERSION);
		return TRUE;
	} else {
		g_warning("Plugin interface version mismatch: plugin: %d, app: %d", ver, SYL_PLUGIN_INTERFACE_VERSION);
		return FALSE;
	}
}

gint syl_plugin_add_symbol(const gchar *name, gpointer sym)
{
	g_hash_table_insert(sym_table, (gpointer)name, sym);
	return 0;
}

gpointer syl_plugin_lookup_symbol(const gchar *name)
{
	return g_hash_table_lookup(sym_table, name);
}

const gchar *syl_plugin_get_prog_version(void)
{
	gpointer sym;

	sym = syl_plugin_lookup_symbol("prog_version");
	return (gchar *)sym;
}

void syl_plugin_main_window_lock(void)
{
	CALL_VOID_POINTER(syl_plugin_main_window_get,
			  "main_window_lock");
}

void syl_plugin_main_window_unlock(void)
{
	CALL_VOID_POINTER(syl_plugin_main_window_get,
			  "main_window_unlock");
}

gpointer syl_plugin_main_window_get(void)
{
	gpointer (*func)(void);

	func = syl_plugin_lookup_symbol("main_window_get");
	return SAFE_CALL_RET(func);
}

void syl_plugin_main_window_popup(gpointer mainwin)
{
	void (*func)(gpointer);

	func = syl_plugin_lookup_symbol("main_window_popup");
	SAFE_CALL_ARG1(func, mainwin);
}

GtkWidget *syl_plugin_main_window_get_toolbar(void)
{
	gpointer widget;

	widget = syl_plugin_lookup_symbol("main_window_toolbar");
	return GTK_WIDGET(widget);
}

GtkWidget *syl_plugin_main_window_get_statusbar(void)
{
	gpointer widget;

	widget = syl_plugin_lookup_symbol("main_window_statusbar");
	return GTK_WIDGET(widget);
}

void syl_plugin_app_will_exit(gboolean force)
{
	void (*func)(gboolean);

	func = syl_plugin_lookup_symbol("app_will_exit");
	SAFE_CALL_ARG1(func, force);
}

static GtkItemFactory *get_item_factory(const gchar *path)
{
	GtkItemFactory *ifactory;

	if (!path)
		return NULL;

	if (strncmp(path, "<Main>", 6) == 0)
		ifactory = syl_plugin_lookup_symbol("main_window_menu_factory");
	else if (strncmp(path, "<MailFolder>", 12) == 0)
		ifactory = syl_plugin_lookup_symbol("folderview_mail_popup_factory");
	else if (strncmp(path, "<IMAPFolder>", 12) == 0)
		ifactory = syl_plugin_lookup_symbol("folderview_imap_popup_factory");
	else if (strncmp(path, "<NewsFolder>", 12) == 0)
		ifactory = syl_plugin_lookup_symbol("folderview_news_popup_factory");
	else if (strncmp(path, "<SummaryView>", 13) == 0)
		ifactory = syl_plugin_lookup_symbol("summaryview_popup_factory");
	else
		ifactory = syl_plugin_lookup_symbol("main_window_menu_factory");

	return ifactory;
}

gint syl_plugin_add_menuitem(const gchar *parent, const gchar *label,
			     SylPluginCallbackFunc func, gpointer data)
{
	GtkItemFactory *ifactory;
	GtkWidget *menu;
	GtkWidget *menuitem;

	if (!parent)
		return -1;

	ifactory = get_item_factory(parent);
	if (!ifactory)
		return -1;

	menu = gtk_item_factory_get_widget(ifactory, parent);
	if (!menu)
		return -1;

	if (label)
		menuitem = gtk_menu_item_new_with_label(label);
	else {
		menuitem = gtk_menu_item_new();
		gtk_widget_set_sensitive(menuitem, FALSE);
	}
	gtk_widget_show(menuitem);
	gtk_menu_append(GTK_MENU(menu), menuitem);
	if (func)
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(func), data);

	return 0;
}

gint syl_plugin_add_factory_item(const gchar *parent, const gchar *label,
				 SylPluginCallbackFunc func, gpointer data)
{
	GtkItemFactory *ifactory;
	GtkItemFactoryEntry entry = {NULL, NULL, NULL, 0, NULL};

	if (!parent)
		return -1;

	ifactory = get_item_factory(parent);
	if (!ifactory)
		return -1;

	if (label) {
		entry.path = (gchar *)label;
		if (g_str_has_suffix(label, "/---"))
			entry.item_type = "<Separator>";
		else
			entry.item_type = NULL;
	} else {
		entry.path = "/---";
		entry.item_type = "<Separator>";
	}
	entry.callback = func;
	g_print("entry.path = %s\n", entry.path);

	gtk_item_factory_create_item(ifactory, &entry, data, 2);

	return 0;
}

void syl_plugin_menu_set_sensitive(const gchar *path, gboolean sensitive)
{
	GtkItemFactory *ifactory;
	GtkWidget *widget;

	g_return_if_fail(path != NULL);

	ifactory = get_item_factory(path);
	if (!ifactory)
		return;

	widget = gtk_item_factory_get_item(ifactory, path);
	gtk_widget_set_sensitive(widget, sensitive);
}

void syl_plugin_menu_set_sensitive_all(GtkMenuShell *menu_shell,
				       gboolean sensitive)
{
	GList *cur;

	for (cur = menu_shell->children; cur != NULL; cur = cur->next)
		gtk_widget_set_sensitive(GTK_WIDGET(cur->data), sensitive);
}

void syl_plugin_menu_set_active(const gchar *path, gboolean is_active)
{
	GtkItemFactory *ifactory;
	GtkWidget *widget;

	g_return_if_fail(path != NULL);

	ifactory = get_item_factory(path);
	if (!ifactory)
		return;

	widget = gtk_item_factory_get_item(ifactory, path);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), is_active);
}

gpointer syl_plugin_folderview_get(void)
{
	gpointer (*func)(void);
	GETFUNC("folderview_get");
	return SAFE_CALL_RET(func);
}

void syl_plugin_folderview_add_sub_widget(GtkWidget *widget)
{
	void (*func)(gpointer, GtkWidget *);
	gpointer folderview;

	folderview = syl_plugin_folderview_get();
	if (folderview) {
		GETFUNC("folderview_add_sub_widget");
		SAFE_CALL_ARG2(func, folderview, widget);
	}
}

void syl_plugin_folderview_select(FolderItem *item)
{
	void (*func)(gpointer, FolderItem *);
	gpointer folderview;

	folderview = syl_plugin_folderview_get();
	if (folderview) {
		GETFUNC("folderview_select");
		SAFE_CALL_ARG2(func, folderview, item);
	}
}

void syl_plugin_folderview_unselect(void)
{
	CALL_VOID_POINTER(syl_plugin_folderview_get,
			  "folderview_unselect");
}

void syl_plugin_folderview_select_next_unread(void)
{
	CALL_VOID_POINTER(syl_plugin_folderview_get,
			  "folderview_select_next_unread");
}

FolderItem *syl_plugin_folderview_get_selected_item(void)
{
	FolderItem * (*func)(gpointer);
	gpointer folderview;

	folderview = syl_plugin_folderview_get();
	if (folderview) {
		GETFUNC("folderview_get_selected_item");
		return SAFE_CALL_ARG1_RET(func, folderview);
	}

	return NULL;
}

gint syl_plugin_folderview_check_new(Folder *folder)
{
	gint (*func)(Folder *);
	GETFUNC("folderview_check_new");
	return SAFE_CALL_ARG1_RET_VAL(func, folder, FALSE);
}

gint syl_plugin_folderview_check_new_item(FolderItem *item)
{
	gint (*func)(FolderItem *);
	GETFUNC("folderview_check_new_item");
	return SAFE_CALL_ARG1_RET_VAL(func, item, FALSE);
}

gint syl_plugin_folderview_check_new_all(void)
{
	gint (*func)(void);
	GETFUNC("folderview_check_new_all");
	return SAFE_CALL_RET_VAL(func, FALSE);
}

void syl_plugin_folderview_update_item(FolderItem *item,
				       gboolean update_summary)
{
	void (*func)(FolderItem *, gboolean);
	GETFUNC("folderview_update_item");
	SAFE_CALL_ARG2(func, item, update_summary);
}

void syl_plugin_folderview_update_item_foreach(GHashTable *table,
					       gboolean update_summary)
{
	void (*func)(GHashTable *, gboolean);
	GETFUNC("folderview_update_item_foreach");
	SAFE_CALL_ARG2(func, table, update_summary);
}

void syl_plugin_folderview_update_all_updated(gboolean update_summary)
{
	void (*func)(gboolean);
	GETFUNC("folderview_update_all_updated");
	SAFE_CALL_ARG1(func, update_summary);
}

void syl_plugin_folderview_check_new_selected(void)
{
	CALL_VOID_POINTER(syl_plugin_folderview_get,
			  "folderview_check_new_selected");
}

gpointer syl_plugin_summary_view_get(void)
{
	gpointer sym;

	sym = syl_plugin_lookup_symbol("summaryview");
	return sym;
}

void syl_plugin_summary_select_by_msgnum(guint msgnum)
{
	void (*func)(gpointer, guint);
	gpointer summary;

	summary = syl_plugin_summary_view_get();
	if (summary) {
		func = syl_plugin_lookup_symbol("summary_select_by_msgnum");
		SAFE_CALL_ARG2(func, summary, msgnum);
	}
}

gboolean syl_plugin_summary_select_by_msginfo(MsgInfo *msginfo)
{
	gboolean (*func)(gpointer, MsgInfo *);
	gpointer summary;

	summary = syl_plugin_summary_view_get();
	if (summary) {
		func = syl_plugin_lookup_symbol("summary_select_by_msginfo");
		return SAFE_CALL_ARG2_RET_VAL(func, summary, msginfo, FALSE);
	}

	return FALSE;
}

void syl_plugin_open_message(const gchar *folder_id, guint msgnum)
{
	FolderItem *item;
	MsgInfo *msginfo;

	item = folder_find_item_from_identifier(folder_id);
	msginfo = folder_item_get_msginfo(item, msgnum);

	if (msginfo) {
		if (!syl_plugin_summary_select_by_msginfo(msginfo)) {
			syl_plugin_open_message_by_new_window(msginfo);
		}
		procmsg_msginfo_free(msginfo);
	}
}

void syl_plugin_summary_show_queued_msgs(void)
{
	CALL_VOID_POINTER(syl_plugin_summary_view_get,
			  "summary_show_queued_msgs");
}

void syl_plugin_summary_lock(void)
{
	CALL_VOID_POINTER(syl_plugin_summary_view_get,
			  "summary_lock");
}

void syl_plugin_summary_unlock(void)
{
	CALL_VOID_POINTER(syl_plugin_summary_view_get,
			  "summary_unlock");
}

gboolean syl_plugin_summary_is_locked(void)
{
	gboolean (*func)(gpointer);
	gpointer summary;

	summary = syl_plugin_summary_view_get();
	if (summary) {
		GETFUNC("summary_is_locked");
		return SAFE_CALL_ARG1_RET_VAL(func, summary, FALSE);
	}

	return FALSE;
}

gboolean syl_plugin_summary_is_read_locked(void)
{
	gboolean (*func)(gpointer);
	gpointer summary;

	summary = syl_plugin_summary_view_get();
	if (summary) {
		GETFUNC("summary_is_read_locked");
		return SAFE_CALL_ARG1_RET_VAL(func, summary, FALSE);
	}

	return FALSE;
}

void syl_plugin_summary_write_lock(void)
{
	CALL_VOID_POINTER(syl_plugin_summary_view_get,
			  "summary_write_lock");
}

void syl_plugin_summary_write_unlock(void)
{
	CALL_VOID_POINTER(syl_plugin_summary_view_get,
			  "summary_write_unlock");
}

gboolean syl_plugin_summary_is_write_locked(void)
{
	gboolean (*func)(gpointer);
	gpointer summary;

	summary = syl_plugin_summary_view_get();
	if (summary) {
		GETFUNC("summary_is_write_locked");
		return SAFE_CALL_ARG1_RET_VAL(func, summary, FALSE);
	}

	return FALSE;
}

FolderItem *syl_plugin_summary_get_current_folder(void)
{
	FolderItem * (*func)(gpointer);
	gpointer summary;

	summary = syl_plugin_summary_view_get();
	if (summary) {
		GETFUNC("summary_get_current_folder");
		return SAFE_CALL_ARG1_RET(func, summary);
	}

	return NULL;
}

gint syl_plugin_summary_get_selection_type(void)
{
	gint (*func)(gpointer);
	gpointer summary;

	summary = syl_plugin_summary_view_get();
	if (summary) {
		GETFUNC("summary_get_selection_type");
		return SAFE_CALL_ARG1_RET_VAL(func, summary, 0);
	}

	return 0;
}

GSList *syl_plugin_summary_get_selected_msg_list(void)
{
	GSList * (*func)(gpointer);
	gpointer summary;

	summary = syl_plugin_summary_view_get();
	if (summary) {
		GETFUNC("summary_get_selected_msg_list");
		return SAFE_CALL_ARG1_RET(func, summary);
	}

	return NULL;
}

GSList *syl_plugin_summary_get_msg_list(void)
{
	GSList * (*func)(gpointer);
	gpointer summary;

	summary = syl_plugin_summary_view_get();
	if (summary) {
		GETFUNC("summary_get_msg_list");
		return SAFE_CALL_ARG1_RET(func, summary);
	}

	return NULL;
}

void syl_plugin_summary_redisplay_msg(void)
{
	CALL_VOID_POINTER(syl_plugin_summary_view_get,
			  "summary_redisplay_msg");
}

void syl_plugin_summary_open_msg(void)
{
	CALL_VOID_POINTER(syl_plugin_summary_view_get,
			  "summary_open_msg");
}

void syl_plugin_summary_view_source(void)
{
	CALL_VOID_POINTER(syl_plugin_summary_view_get,
			  "summary_view_source");
}

void syl_plugin_summary_reedit(void)
{
	CALL_VOID_POINTER(syl_plugin_summary_view_get,
			  "summary_reedit");
}

void syl_plugin_summary_update_selected_rows(void)
{
	CALL_VOID_POINTER(syl_plugin_summary_view_get,
			  "summary_update_selected_rows");
}

void syl_plugin_summary_update_by_msgnum(guint msgnum)
{
	void (*func)(gpointer, guint);
	gpointer summary;

	summary = syl_plugin_summary_view_get();
	if (summary) {
		func = syl_plugin_lookup_symbol("summary_update_by_msgnum");
		SAFE_CALL_ARG2(func, summary, msgnum);
	}
}

gpointer syl_plugin_messageview_create_with_new_window(void)
{
	gpointer (*func)(void);

	func = syl_plugin_lookup_symbol("messageview_create_with_new_window");
	return SAFE_CALL_RET(func);
}

void syl_plugin_open_message_by_new_window(MsgInfo *msginfo)
{
	gpointer msgview;
	gpointer (*func)(gpointer, MsgInfo *, gboolean);

	msgview = syl_plugin_messageview_create_with_new_window();
	if (msgview) {
		func = syl_plugin_lookup_symbol("messageview_show");
		SAFE_CALL_ARG3(func, msgview, msginfo, FALSE);
	}
}


gpointer syl_plugin_compose_new(PrefsAccount *account, FolderItem *item,
				const gchar *mailto, GPtrArray *attach_files)
{
	gpointer (*func)(PrefsAccount *, FolderItem *, const gchar *,
			 GPtrArray *);

	func = syl_plugin_lookup_symbol("compose_new");
	return SAFE_CALL_ARG4_RET(func, account, item, mailto, attach_files);
}

gpointer syl_plugin_compose_reply(MsgInfo *msginfo, FolderItem *item,
				  gint mode, const gchar *body)
{
	gpointer (*func)(MsgInfo *, FolderItem *, gint, const gchar *);

	GETFUNC("compose_reply");
	return SAFE_CALL_ARG4_RET(func, msginfo, item, mode, body);
}

gpointer syl_plugin_compose_forward(GSList *mlist, FolderItem *item,
				    gboolean as_attach, const gchar *body)
{
	gpointer (*func)(GSList *, FolderItem *, gboolean, const gchar *);

	GETFUNC("compose_forward");
	return SAFE_CALL_ARG4_RET(func, mlist, item, as_attach, body);
}

gpointer syl_plugin_compose_redirect(MsgInfo *msginfo, FolderItem *item)
{
	gpointer (*func)(MsgInfo *, FolderItem *);

	GETFUNC("compose_redirect");
	return SAFE_CALL_ARG2_RET(func, msginfo, item);
}

gpointer syl_plugin_compose_reedit(MsgInfo *msginfo)
{
	gpointer (*func)(MsgInfo *);

	GETFUNC("compose_reedit");
	return SAFE_CALL_ARG1_RET(func, msginfo);
}

void syl_plugin_compose_entry_set(gpointer compose, const gchar *text,
				  gint type)
{
	void (*func)(gpointer, const gchar *, gint);

	func = syl_plugin_lookup_symbol("compose_entry_set");
	SAFE_CALL_ARG3(func, compose, text, type);
}

void syl_plugin_compose_entry_append(gpointer compose, const gchar *text,
				     gint type)
{
	void (*func)(gpointer, const gchar *, gint);

	func = syl_plugin_lookup_symbol("compose_entry_append");
	SAFE_CALL_ARG3(func, compose, text, type);
}

gchar *syl_plugin_compose_entry_get_text(gpointer compose, gint type)
{
	gchar * (*func)(gpointer, gint);

	func = syl_plugin_lookup_symbol("compose_entry_get_text");
	return SAFE_CALL_ARG2_RET(func, compose, type);
}

void syl_plugin_compose_lock(gpointer compose)
{
	void (*func)(gpointer);

	func = syl_plugin_lookup_symbol("compose_lock");
	SAFE_CALL_ARG1(func, compose);
}

void syl_plugin_compose_unlock(gpointer compose)
{
	void (*func)(gpointer);

	func = syl_plugin_lookup_symbol("compose_unlock");
	SAFE_CALL_ARG1(func, compose);
}

GtkWidget *syl_plugin_compose_get_toolbar(gpointer compose)
{
	GtkWidget * (*func)(gpointer);

	func = syl_plugin_lookup_symbol("compose_get_toolbar");
	return SAFE_CALL_ARG1_RET(func, compose);
}

GtkWidget *syl_plugin_compose_get_misc_hbox(gpointer compose)
{
	GtkWidget * (*func)(gpointer);

	func = syl_plugin_lookup_symbol("compose_get_misc_hbox");
	return SAFE_CALL_ARG1_RET(func, compose);
}

GtkWidget *syl_plugin_compose_get_textview(gpointer compose)
{
	GtkWidget * (*func)(gpointer);

	func = syl_plugin_lookup_symbol("compose_get_textview");
	return SAFE_CALL_ARG1_RET(func, compose);
}

gint syl_plugin_compose_send(gpointer compose, gboolean close_on_success)
{
	gint (*func)(gpointer, gboolean);

	GETFUNC("compose_send");
	return SAFE_CALL_ARG2_RET_VAL(func, compose, close_on_success, -1);
}

void syl_plugin_compose_attach_append(gpointer compose,
				      const gchar *file,
				      const gchar *filename,
				      const gchar *content_type)
{
	void (*func)(gpointer, const gchar *, const gchar *, const gchar *);

	GETFUNC("compose_attach_append");
	SAFE_CALL_ARG4(func, compose, file, filename, content_type);
}

void syl_plugin_compose_attach_remove_all(gpointer compose)
{
	void (*func)(gpointer);

	GETFUNC("compose_attach_remove_all");
	SAFE_CALL_ARG1(func, compose);
}

GSList *syl_plugin_get_attach_list(gpointer compose)
{
	GSList * (*func)(gpointer);

	GETFUNC("compose_get_attach_list");
	return SAFE_CALL_ARG1_RET(func, compose);
}


FolderItem *syl_plugin_folder_sel(Folder *cur_folder, gint sel_type,
				  const gchar *default_folder)
{
	FolderItem * (*func)(Folder *, gint, const gchar *);

	func = syl_plugin_lookup_symbol("foldersel_folder_sel");
	return SAFE_CALL_ARG3_RET(func, cur_folder, sel_type, default_folder);
}

FolderItem *syl_plugin_folder_sel_full(Folder *cur_folder, gint sel_type,
				       const gchar *default_folder,
				       const gchar *message)
{
	FolderItem * (*func)(Folder *, gint, const gchar *, const gchar *);

	func = syl_plugin_lookup_symbol("foldersel_folder_sel_full");
	return SAFE_CALL_ARG4_RET(func, cur_folder, sel_type, default_folder,
				  message);
}

gchar *syl_plugin_input_dialog(const gchar *title, const gchar *message,
			       const gchar *default_string)
{
	gchar * (*func)(const gchar *, const gchar *, const gchar *);

	func = syl_plugin_lookup_symbol("input_dialog");
	return SAFE_CALL_ARG3_RET(func, title, message, default_string);
}

gchar *syl_plugin_input_dialog_with_invisible(const gchar *title,
					      const gchar *message,
					      const gchar *default_string)
{
	gchar * (*func)(const gchar *, const gchar *, const gchar *);

	func = syl_plugin_lookup_symbol("input_dialog_with_invisible");
	return SAFE_CALL_ARG3_RET(func, title, message, default_string);
}

void syl_plugin_manage_window_set_transient(GtkWindow *window)
{
	void (*func)(GtkWindow *);

	func = syl_plugin_lookup_symbol("manage_window_set_transient");
	SAFE_CALL_ARG1(func, window);
}

void syl_plugin_manage_window_signals_connect(GtkWindow *window)
{
	void (*func)(GtkWindow *);

	func = syl_plugin_lookup_symbol("manage_window_signals_connect");
	SAFE_CALL_ARG1(func, window);
}

GtkWidget *syl_plugin_manage_window_get_focus_window(void)
{
	GtkWidget * (*func)(void);

	func = syl_plugin_lookup_symbol("manage_window_get_focus_window");
	return SAFE_CALL_RET(func);
}

void syl_plugin_inc_mail(void)
{
	void (*func)(gpointer);

	func = syl_plugin_lookup_symbol("inc_mail");
	SAFE_CALL_ARG1(func, syl_plugin_main_window_get());
}

gboolean syl_plugin_inc_is_active(void)
{
	gboolean (*func)(void);

	func = syl_plugin_lookup_symbol("inc_is_active");
	return SAFE_CALL_RET_VAL(func, FALSE);
}

void syl_plugin_inc_lock(void)
{
	void (*func)(void);

	func = syl_plugin_lookup_symbol("inc_lock");
	SAFE_CALL(func);
}

void syl_plugin_inc_unlock(void)
{
	void (*func)(void);

	func = syl_plugin_lookup_symbol("inc_unlock");
	SAFE_CALL(func);
}

void syl_plugin_update_check(gboolean show_dialog_always)
{
	void (*func)(gboolean);

	func = syl_plugin_lookup_symbol("update_check");
	SAFE_CALL_ARG1(func, show_dialog_always);
}

void syl_plugin_update_check_set_check_url(const gchar *url)
{
	void (*func)(const gchar *);

	func = syl_plugin_lookup_symbol("update_check_set_check_url");
	SAFE_CALL_ARG1(func, url);
}

const gchar *syl_plugin_update_check_get_check_url(void)
{
	const gchar * (*func)(void);

	func = syl_plugin_lookup_symbol("update_check_get_check_url");
	return SAFE_CALL_RET(func);
}

void syl_plugin_update_check_set_download_url(const gchar *url)
{
	void (*func)(const gchar *);

	func = syl_plugin_lookup_symbol("update_check_set_download_url");
	SAFE_CALL_ARG1(func, url);
}

const gchar *syl_plugin_update_check_get_download_url(void)
{
	const gchar * (*func)(void);

	func = syl_plugin_lookup_symbol("update_check_get_download_url");
	return SAFE_CALL_RET(func);
}

void syl_plugin_update_check_set_jump_url(const gchar *url)
{
	void (*func)(const gchar *);

	func = syl_plugin_lookup_symbol("update_check_set_jump_url");
	SAFE_CALL_ARG1(func, url);
}

const gchar *syl_plugin_update_check_get_jump_url(void)
{
	const gchar * (*func)(void);

	func = syl_plugin_lookup_symbol("update_check_get_jump_url");
	return SAFE_CALL_RET(func);
}

void syl_plugin_update_check_set_check_plugin_url(const gchar *url)
{
	void (*func)(const gchar *);

	func = syl_plugin_lookup_symbol("update_check_set_check_plugin_url");
	SAFE_CALL_ARG1(func, url);
}

const gchar *syl_plugin_update_check_get_check_plugin_url(void)
{
	const gchar * (*func)(void);

	func = syl_plugin_lookup_symbol("update_check_get_check_plugin_url");
	return SAFE_CALL_RET(func);
}

void syl_plugin_update_check_set_jump_plugin_url(const gchar *url)
{
	void (*func)(const gchar *);

	func = syl_plugin_lookup_symbol("update_check_set_jump_plugin_url");
	SAFE_CALL_ARG1(func, url);
}

const gchar *syl_plugin_update_check_get_jump_plugin_url(void)
{
	const gchar * (*func)(void);

	func = syl_plugin_lookup_symbol("update_check_get_jump_plugin_url");
	return SAFE_CALL_RET(func);
}

gint syl_plugin_alertpanel_full(const gchar *title, const gchar *message,
				gint type, gint default_value,
				gboolean can_disable,
				const gchar *btn1_label,
				const gchar *btn2_label,
				const gchar *btn3_label)
{
	gint (*func)(const gchar *, const gchar *, gint, gint, gboolean,
			const gchar *, const gchar *, const gchar *);

	GETFUNC("alertpanel_full");
	return func ? func(title, message, type, default_value, can_disable,
			   btn1_label, btn2_label, btn3_label) : -1;
}

gint syl_plugin_alertpanel(const gchar *title, const gchar *message,
			   const gchar *btn1_label,
			   const gchar *btn2_label,
			   const gchar *btn3_label)
{
	gint (*func)(const gchar *, const gchar *,
			const gchar *, const gchar *, const gchar *);

	GETFUNC("alertpanel");
	return func ? func(title, message, btn1_label, btn2_label, btn3_label)
		: -1;
}

void syl_plugin_alertpanel_message(const gchar *title, const gchar *message,
				   gint type)
{
	void (*func)(const gchar *, const gchar *, gint);

	GETFUNC("alertpanel_message");
	SAFE_CALL_ARG3(func, title, message, type);
}

gint syl_plugin_alertpanel_message_with_disable(const gchar *title,
						const gchar *message,
						gint type)
{
	gint (*func)(const gchar *, const gchar *, gint);

	GETFUNC("alertpanel_message_with_disable");
	return SAFE_CALL_ARG3_RET_VAL(func, title, message, type, 0);
}

gint syl_plugin_send_message(const gchar *file, PrefsAccount *ac,
			     GSList *to_list)
{
	gint (*func)(const gchar *, PrefsAccount *, GSList *);

	GETFUNC("send_message");
	return SAFE_CALL_ARG3_RET_VAL(func, file, ac, to_list, -1);
}

gint syl_plugin_send_message_queue_all(FolderItem *queue, gboolean save_msgs,
				       gboolean filter_msgs)
{
	gint (*func)(FolderItem *, gboolean, gboolean);

	GETFUNC("send_message_queue_all");
	return SAFE_CALL_ARG3_RET_VAL(func, queue, save_msgs, filter_msgs, -1);
}

gint syl_plugin_send_message_set_reply_flag(const gchar *reply_target,
					    const gchar *msgid)
{
	gint (*func)(const gchar *, const gchar *);

	GETFUNC("send_message_set_reply_flag");
	return SAFE_CALL_ARG2_RET_VAL(func, reply_target, msgid, -1);
}

gint syl_plugin_send_message_set_forward_flags(const gchar *forward_targets)
{
	gint (*func)(const gchar *);

	GETFUNC("send_message_set_forward_flags");
	return SAFE_CALL_ARG1_RET_VAL(func, forward_targets, -1);
}

gint syl_plugin_notification_window_open(const gchar *message,
					 const gchar *submessage,
					 guint timeout)
{
	gint (*func)(const gchar *, const gchar *, guint);

	GETFUNC("notification_window_open");
	return SAFE_CALL_ARG3_RET_VAL(func, message, submessage, timeout, -1);
}

void syl_plugin_notification_window_set_message(const gchar *message,
						const gchar *submessage)
{
	void (*func)(const gchar *, const gchar *);

	GETFUNC("notification_window_set_message");
	SAFE_CALL_ARG2(func, message, submessage);
}

void syl_plugin_notification_window_close(void)
{
	void (*func)(void);

	GETFUNC("notification_window_close");
	SAFE_CALL(func);
}
