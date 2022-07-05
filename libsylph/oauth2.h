/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2022 Hiroyuki Yamamoto
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

#ifndef __OAUTH2_H__
#define __OAUTH2_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

gint oauth2_get_token		(const gchar	 *user,
				 gchar		**token,
				 gchar		**r_token,
				 gint		 *expire);
gchar *oauth2_get_sasl_xoauth2	(const gchar	 *user,
				 const gchar	 *token);

#endif /* __OAUTH2_H__ */
