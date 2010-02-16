/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2010 Hiroyuki Yamamoto
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

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <gtk/gtk.h>

#include "procmsg.h"
#include "folder.h"

/* SylPlugin object */

#define SYL_TYPE_PLUGIN		(syl_plugin_get_type())
#define SYL_PLUGIN(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), SYL_TYPE_PLUGIN, SylPlugin))
#define SYL_IS_PLUGIN(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), SYL_TYPE_PLUGIN))
#define SYL_PLUGIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), SYL_TYPE_PLUGIN, SylPluginClass))
#define SYL_IS_PLUGIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), SYL_TYPE_PLUGIN))
#define SYL_PLUGIN_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), SYL_TYPE_PLUGIN, SylPluginClass))

typedef struct _SylPlugin	SylPlugin;
typedef struct _SylPluginClass	SylPluginClass;
typedef struct _SylPluginInfo	SylPluginInfo;

typedef void (*SylPluginLoadFunc)	(void);
typedef void (*SylPluginUnloadFunc)	(void);
typedef void (*SylPluginCallbackFunc)	(void);

#define SYL_PLUGIN_INTERFACE_VERSION	0x0104

struct _SylPlugin
{
	GObject parent_instance;
};

struct _SylPluginClass
{
	GObjectClass parent_class;

	void (* plugin_load)	(GObject *obj, GModule *module);
	void (* plugin_unload)	(GObject *obj, GModule *module);

	void (* folderview_menu_popup)	(GObject *obj, gpointer ifactory);

	void (* compose_created)	(GObject *obj, gpointer compose);
	void (* compose_destroy)	(GObject *obj, gpointer compose);
};

struct _SylPluginInfo
{
	gchar *name;
	gchar *version;
	gchar *author;
	gchar *description;

	gpointer pad1;
	gpointer pad2;
	gpointer pad3;
	gpointer pad4;
};

GType syl_plugin_get_type	(void);

void syl_plugin_signal_connect		(const gchar *name, GCallback callback,
					 gpointer data);
void syl_plugin_signal_disconnect	(gpointer func, gpointer data);
void syl_plugin_signal_emit		(const gchar *name, ...);

/* Used by Sylpheed */

gint syl_plugin_init_lib	(void);

gint syl_plugin_load		(const gchar *file);
gint syl_plugin_load_all	(const gchar *dir);
void syl_plugin_unload_all	(void);

GSList *syl_plugin_get_module_list	(void);
SylPluginInfo *syl_plugin_get_info	(GModule *module);
gboolean syl_plugin_check_version	(GModule *module);

gint syl_plugin_add_symbol		(const gchar *name, gpointer sym);
gpointer syl_plugin_lookup_symbol	(const gchar *name);

/* Interfaces which should be implemented by plug-ins
     void plugin_load(void);
     void plugin_unload(void);
     SylPluginInfo *plugin_info(void);
     gint plugin_interface_version(void);
 */

/* Plug-in API (used by plug-ins) */

const gchar *syl_plugin_get_prog_version	(void);

void syl_plugin_main_window_lock		(void);
void syl_plugin_main_window_unlock		(void);
gpointer syl_plugin_main_window_get		(void);
void syl_plugin_main_window_popup		(gpointer mainwin);
GtkWidget *syl_plugin_main_window_get_statusbar	(void);

void syl_plugin_app_will_exit			(gboolean force);

/* Menu */
gint syl_plugin_add_menuitem			(const gchar *parent,
						 const gchar *label,
						 SylPluginCallbackFunc func,
						 gpointer data);
gint syl_plugin_add_factory_item		(const gchar *menu,
						 const gchar *label,
						 SylPluginCallbackFunc func,
						 gpointer data);

void syl_plugin_menu_set_sensitive		(const gchar	*path,
						 gboolean	 sensitive);
void syl_plugin_menu_set_sensitive_all		(GtkMenuShell	*menu_shell,
						 gboolean	 sensitive);
void syl_plugin_menu_set_active			(const gchar	*path,
						 gboolean	 is_active);


/* FolderView */
gpointer syl_plugin_folderview_get		(void);
FolderItem *syl_plugin_folderview_get_selected_item
						(void);

/* SummaryView */
gpointer syl_plugin_summary_view_get		(void);
void syl_plugin_sumary_select_by_msgnum		(guint msgnum);
gboolean syl_plugin_summary_select_by_msginfo	(MsgInfo *msginfo);

void syl_plugin_open_message			(const gchar *folder_id,
						 guint msgnum);

void syl_plugin_summary_lock			(void);
void syl_plugin_summary_unlock			(void);
gboolean syl_plugin_summary_is_locked		(void);

/* MessageView */
gpointer syl_plugin_messageview_create_with_new_window
						(void);
void syl_plugin_open_message_by_new_window	(MsgInfo *msginfo);

/* Compose */
gpointer syl_plugin_compose_new			(PrefsAccount *account,
						 FolderItem *item,
						 const gchar *mailto,
						 GPtrArray *attach_files);

/* entry type:
   0: To 1: Cc 2: Bcc 3: Reply-To 4: Subject 5: Newsgroups 6: Followup-To */
void syl_plugin_compose_entry_set		(gpointer compose,
						 const gchar *text,
						 gint type);
void syl_plugin_compose_entry_append		(gpointer compose,
						 const gchar *text,
						 gint type);
gchar *syl_plugin_compose_entry_get_text	(gpointer compose,
						 gint type);
void syl_plugin_compose_lock			(gpointer compose);
void syl_plugin_compose_unlock			(gpointer compose);

/* Others */
FolderItem *syl_plugin_folder_sel		(Folder *cur_folder,
						 gint sel_type,
						 const gchar *default_folder);
FolderItem *syl_plugin_folder_sel_full		(Folder *cur_folder,
						 gint sel_type,
						 const gchar *default_folder,
						 const gchar *message);

gchar *syl_plugin_input_dialog			(const gchar *title,
						 const gchar *message,
						 const gchar *default_string);
gchar *syl_plugin_input_dialog_with_invisible	(const gchar *title,
						 const gchar *message,
						 const gchar *default_string);

void syl_plugin_manage_window_set_transient	(GtkWindow *window);
void syl_plugin_manage_window_signals_connect	(GtkWindow *window);
GtkWidget *syl_plugin_manage_window_get_focus_window
						(void);

void syl_plugin_inc_mail			(void);
gboolean syl_plugin_inc_is_active		(void);
void syl_plugin_inc_lock			(void);
void syl_plugin_inc_unlock			(void);

#endif /* __PLUGIN_H__ */
