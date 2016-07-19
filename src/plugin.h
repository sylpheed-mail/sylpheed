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

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <gtk/gtk.h>

#include "procmsg.h"
#include "folder.h"
#include "filter.h"
#include "plugin-types.h"

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

#define SYL_PLUGIN_INTERFACE_VERSION	0x010a

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
	void (* summaryview_menu_popup)	(GObject *obj, gpointer ifactory);

	void (* compose_created)	(GObject *obj, gpointer compose);
	void (* compose_destroy)	(GObject *obj, gpointer compose);

	void (* textview_menu_popup)	(GObject	*obj,
					 GtkMenu	*menu,
					 GtkTextView	*textview,
					 const gchar	*uri,
					 const gchar	*selected_text,
					 MsgInfo	*msginfo);

	gboolean (* compose_send)	(GObject	*obj,
					 gpointer	 compose,
					 gint		 compose_mode,
					 gint		 send_mode,
					 const gchar	*msg_file,
					 GSList		*to_list);

	void (* messageview_show)	(GObject	*obj,
					 gpointer	 msgview,
					 MsgInfo	*msginfo,
					 gboolean	 all_headers);

	void (* inc_mail_start)		(GObject	*obj,
					 PrefsAccount	*account);
	void (* inc_mail_finished)	(GObject	*obj,
					 gint		 new_messages);

	/* Prefs dialogs */
	void (* prefs_common_open)	(GObject	*obj,
					 GtkWidget	*window);
	void (* prefs_account_open)	(GObject	*obj,
					 PrefsAccount	*account,
					 GtkWidget	*window);
	void (* prefs_filter_open)	(GObject	*obj,
					 GtkWidget	*window);
	void (* prefs_filter_edit_open)	(GObject	*obj,
					 FilterRule	*rule,
					 const gchar	*header,
					 const gchar	*key,
					 GtkWidget	*window);
	void (* prefs_template_open)	(GObject	*obj,
					 GtkWidget	*window);
	void (* plugin_manager_open)	(GObject	*obj,
					 GtkWidget	*window);

	void (* main_window_toolbar_changed) (GObject *obj);
	void (* compose_toolbar_changed)     (GObject *obj, gpointer compose);
	void (* compose_attach_changed)     (GObject *obj, gpointer compose);
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
GtkWidget *syl_plugin_main_window_get_toolbar	(void);
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

void syl_plugin_folderview_add_sub_widget	(GtkWidget	*widget);

void syl_plugin_folderview_select		(FolderItem	*item);
void syl_plugin_folderview_unselect		(void);
void syl_plugin_folderview_select_next_unread	(void);
FolderItem *syl_plugin_folderview_get_selected_item
						(void);

gint syl_plugin_folderview_check_new		(Folder         *folder);
gint syl_plugin_folderview_check_new_item	(FolderItem     *item);
gint syl_plugin_folderview_check_new_all	(void);

void syl_plugin_folderview_update_item		(FolderItem	*item,
						 gboolean	 update_summary);
void syl_plugin_folderview_update_item_foreach	(GHashTable	*table,
						 gboolean	 update_summary);
void syl_plugin_folderview_update_all_updated	(gboolean	 update_summary);
void syl_plugin_folderview_check_new_selected	(void);

/* SummaryView */
gpointer syl_plugin_summary_view_get		(void);
void syl_plugin_summary_select_by_msgnum	(guint msgnum);
gboolean syl_plugin_summary_select_by_msginfo	(MsgInfo *msginfo);

void syl_plugin_open_message			(const gchar *folder_id,
						 guint msgnum);

void syl_plugin_summary_show_queued_msgs	(void);

void syl_plugin_summary_lock			(void);
void syl_plugin_summary_unlock			(void);
gboolean syl_plugin_summary_is_locked		(void);
gboolean syl_plugin_summary_is_read_locked	(void);
void syl_plugin_summary_write_lock		(void);
void syl_plugin_summary_write_unlock		(void);
gboolean syl_plugin_summary_is_write_locked	(void);

FolderItem *syl_plugin_summary_get_current_folder
						(void);

gint syl_plugin_summary_get_selection_type	(void);
GSList *syl_plugin_summary_get_selected_msg_list(void);
GSList *syl_plugin_summary_get_msg_list		(void);

void syl_plugin_summary_redisplay_msg		(void);
void syl_plugin_summary_open_msg		(void);
void syl_plugin_summary_view_source		(void);
void syl_plugin_summary_reedit			(void);

void syl_plugin_summary_update_selected_rows	(void);
void syl_plugin_summary_update_by_msgnum	(guint msgnum);

/* MessageView */
gpointer syl_plugin_messageview_create_with_new_window
						(void);
void syl_plugin_open_message_by_new_window	(MsgInfo *msginfo);

/* Compose */
gpointer syl_plugin_compose_new			(PrefsAccount *account,
						 FolderItem *item,
						 const gchar *mailto,
						 GPtrArray *attach_files);
/* compose mode:
   1: reply 2: reply to sender 3: reply to all 4: reply to list
   | 1 << 16: with quote */
gpointer syl_plugin_compose_reply		(MsgInfo *msginfo,
						 FolderItem *item,
						 gint mode,
						 const gchar *body);
gpointer syl_plugin_compose_forward		(GSList *mlist,
						 FolderItem *item,
						 gboolean as_attach,
						 const gchar *body);
gpointer syl_plugin_compose_redirect		(MsgInfo *msginfo,
						 FolderItem *item);
gpointer syl_plugin_compose_reedit		(MsgInfo *msginfo);

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

GtkWidget *syl_plugin_compose_get_toolbar	(gpointer compose);
GtkWidget *syl_plugin_compose_get_misc_hbox	(gpointer compose);
GtkWidget *syl_plugin_compose_get_textview	(gpointer compose);

gint syl_plugin_compose_send			(gpointer compose,
						 gboolean close_on_success);
void syl_plugin_compose_attach_append		(gpointer compose,
						 const gchar *file,
						 const gchar *filename,
						 const gchar *content_type);
void syl_plugin_compose_attach_remove_all	(gpointer compose);
/* returns list of SylPluginAttachInfo (GSList must be freed by caller) */
GSList *syl_plugin_get_attach_list		(gpointer compose);

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

void syl_plugin_update_check			(gboolean show_dialog_always);
void syl_plugin_update_check_set_check_url		(const gchar *url);
const gchar *syl_plugin_update_check_get_check_url	(void);
void syl_plugin_update_check_set_download_url		(const gchar *url);
const gchar *syl_plugin_update_check_get_download_url	(void);
void syl_plugin_update_check_set_jump_url		(const gchar *url);
const gchar *syl_plugin_update_check_get_jump_url	(void);
void syl_plugin_update_check_set_check_plugin_url	(const gchar *url);
const gchar *syl_plugin_update_check_get_check_plugin_url(void);
void syl_plugin_update_check_set_jump_plugin_url	(const gchar *url);
const gchar *syl_plugin_update_check_get_jump_plugin_url(void);

/* type corresponds to AlertType
 * default_value and return value corresponds to AlertValue */
gint syl_plugin_alertpanel_full			(const gchar *title,
						 const gchar *message,
						 gint type,
						 gint default_value,
						 gboolean can_disable,
						 const gchar *btn1_label,
						 const gchar *btn2_label,
						 const gchar *btn3_label);
gint syl_plugin_alertpanel			(const gchar *title,
						 const gchar *message,
						 const gchar *btn1_label,
						 const gchar *btn2_label,
						 const gchar *btn3_label);
void syl_plugin_alertpanel_message		(const gchar *title,
						 const gchar *message,
						 gint type);
gint syl_plugin_alertpanel_message_with_disable	(const gchar *title,
						 const gchar *message,
						 gint type);

/* Send message */
gint syl_plugin_send_message			(const gchar	*file,
						 PrefsAccount	*ac,
						 GSList		*to_list);
gint syl_plugin_send_message_queue_all		(FolderItem	*queue,
						 gboolean	 save_msgs,
						 gboolean	 filter_msgs);
gint syl_plugin_send_message_set_reply_flag	(const gchar	*reply_target,
						 const gchar	*msgid);
gint syl_plugin_send_message_set_forward_flags	(const gchar	*forward_targets);

/* Notification window */
gint syl_plugin_notification_window_open	(const gchar	*message,
						 const gchar	*submessage,
						 guint		 timeout);
void syl_plugin_notification_window_set_message	(const gchar	*message,
						 const gchar	*submessage);
void syl_plugin_notification_window_close	(void);

#endif /* __PLUGIN_H__ */
