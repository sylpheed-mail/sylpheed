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
                         time_t          *expires_at)
{
	gchar *argv[] = {"syl-auth-helper", NULL, NULL};
	gchar *out = NULL;
	gchar *err = NULL;
	gint status = 0;
	GError *error = NULL;

	g_return_val_if_fail(user != NULL, -1);

	debug_print("Current time = %ld, oauth2 token expires_at = %ld\n", time(NULL), *expires_at);
	if (*token) {
		if (expires_at && *expires_at && time(NULL) < *expires_at-60) {
			debug_print("OAuth2 token present and not yet expired.\n");
			return 0;
		} else {
			debug_print("OAuth2 token present and expired, or about to expire within 60s, going to refresh it by running syl-auth-helper.\n");
		}
	} else {
		debug_print("OAuth2 token not present, going to obtain it by running syl-auth-helper.\n");
	}

	argv[1] = (gchar *)user;
	if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
			 NULL, NULL, &out, &err, &status, &error)) {
		//debug_print("syl-auth-helper out: %s\n", out); //contains access token, do not uncomment and show this output to anyone else.
		gchar **lines = g_strsplit(out, "\n", 4);
		if (lines && lines[0] && token) {
			*token = g_strdup(g_strchomp(lines[0]));
			if (lines[1] && expires_at) {
				*expires_at = atol(g_strchomp(lines[1]));
				debug_print("got token from OAuth2 helper, current unix time %ld, expires_at unix time %ld\n", time(NULL), *expires_at);
			} else {
				g_warning("got token from OAuth2 helper but no expiry time on 2nd line\n");
			}
			if (strlen(err) > 0) {
				g_warning("syl-auth-helper returned a token but also gave these errors:\n%s",err);
			}
		} else {
			if (strlen(err) > 0) {
				g_warning("syl-auth-helper did not return a token and gave these errors:\n%s",err);
			} else {
				g_warning("syl-auth-helper returned nothing, try running it manually with --debug for more info.\n");
			}
		}
		g_strfreev(lines);
	} else {
		g_warning("OAuth2 helper execution failed, error: %s\n", (error && error->message) ? error->message : "null");
	}
	if (error) {
		g_error_free(error);
	}
	g_free(out);
	g_free(err);
	return 0;
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
