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

#ifndef __ALERTPANEL_H__
#define __ALERTPANEL_H__

#include <glib.h>

typedef enum
{
	G_ALERTDEFAULT,
	G_ALERTALTERNATE,
	G_ALERTOTHER,
	G_ALERTCANCEL,
	G_ALERTWAIT,

	G_ALERTDISABLE	= 1 << 16
} AlertValue;

#define G_ALERT_VALUE_MASK	0x0000ffff

typedef enum
{
	ALERT_NOTICE,
	ALERT_QUESTION,
	ALERT_WARNING,
	ALERT_ERROR
} AlertType;

AlertValue alertpanel_full	(const gchar	*title,
				 const gchar	*message,
				 AlertType	 type,
				 AlertValue	 default_value,
				 gboolean	 can_disable,
				 const gchar	*button1_label,
				 const gchar	*button2_label,
				 const gchar	*button3_label);

AlertValue alertpanel		(const gchar	*title,
				 const gchar	*message,
				 const gchar	*button1_label,
				 const gchar	*button2_label,
				 const gchar	*button3_label);

void alertpanel_message	(const gchar	*title,
			 const gchar	*message,
			 AlertType	 type);

AlertValue alertpanel_message_with_disable	(const gchar	*title,
						 const gchar	*message,
						 AlertType	 type);

void alertpanel_notice	(const gchar	*format,
			 ...) G_GNUC_PRINTF(1, 2);
void alertpanel_warning	(const gchar	*format,
			 ...) G_GNUC_PRINTF(1, 2);
void alertpanel_error	(const gchar	*format,
			 ...) G_GNUC_PRINTF(1, 2);

#endif /* __ALERTPANEL_H__ */
