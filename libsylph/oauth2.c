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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "oauth2.h"
#include "base64.h"
#include "utils.h"


gint oauth2_get_token   (const gchar     *user,
                         gchar          **token,
                         gchar          **r_token,
                         gint            *expire)
{
	gchar *argv[] = {"syl-auth-helper", NULL, NULL, NULL, NULL};
	gchar *out = NULL;
	gint status = 0;
	GError *error = NULL;

	g_return_val_if_fail(user != NULL, -1);

	argv[1] = "--configdir";
	argv[2] = get_rc_dir();
	argv[3] = (gchar *)user;
	if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
			 NULL, NULL, &out, NULL, &status, &error)) {
		debug_print("syl-auth-helper out: %s\n", out);
		gchar **lines = g_strsplit(out, "\n", 4);
		if (lines && lines[0] && token) {
			*token = g_strdup(g_strchomp(lines[0]));
			if (lines[1] && r_token)
				*r_token = g_strdup(g_strchomp(lines[1]));
		}
		g_strfreev(lines);
		return 0;
	} else {
		g_warning("OAuth2 helper execution failed.\n");
		g_error_free(error);
		return -1;
	}
}

gchar *oauth2_get_sasl_xoauth2	(const gchar	 *user,
				 const gchar	 *token)
{
	gchar *response;
	gchar *response64;

	g_return_val_if_fail(user != NULL, NULL);
	g_return_val_if_fail(token != NULL, NULL);

	// SASL XOAUTH2:
	// base64("user=" + userName + "^Aauth=Bearer " + accessToken + "^A^A")

	response = g_strdup_printf("user=%s" "\x01" "auth=Bearer %s" "\x01\x01", user, token);
	response64 = g_malloc(strlen(response) * 2 + 1);
	base64_encode(response64, (guchar *)response, strlen(response));
	g_free(response);

	return response64;
}
