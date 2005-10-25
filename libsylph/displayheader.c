/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include "displayheader.h"

gchar *display_header_prop_get_str(DisplayHeaderProp *dp)
{
	return g_strconcat(dp->hidden ? "-" : "", dp->name, NULL);
}
 
DisplayHeaderProp *display_header_prop_read_str(gchar *buf)
{
	DisplayHeaderProp *dp;

	dp = g_new0(DisplayHeaderProp, 1);

	dp->hidden = FALSE;
	if (*buf == '-') {
		dp->hidden = TRUE;
		buf++;
	}
	if (*buf == '\0') {
		g_free(dp);
		return NULL;
	}
	dp->name = g_strdup(buf);

	return dp;
}
 
void display_header_prop_free(DisplayHeaderProp *dp)
{
	if (!dp) return;

	g_free(dp->name);
	g_free(dp);
}
