/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 Hiroyuki Yamamoto
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

#include <glib.h>
#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#ifdef G_OS_WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#endif /* G_OS_WIN32 */

#include "utils.h"
#include "socket.h"
#include "base64.h"

#define TIMEOUT_MSEC	90000

typedef struct _APIInfo
{
	gchar *auth_uri;
	gchar *token_uri;
	gchar *redirect_uri;
	gchar *client_id;
	gchar *client_secret;
	gchar *scope;
	gint local_port;
} APIInfo;

typedef struct _TokenData
{
	gint expires_in;
	gint ext_expires_in;
	gchar *access_token;
	gchar *refresh_token;
} TokenData;

typedef struct _AcceptData
{
	gboolean done;
	const gchar *state;
	gchar *code;
} AcceptData;

static AcceptData accept_data = {
	FALSE,
	NULL,
	NULL
};

static gchar *http_get(const gchar *url, gchar **r_header);
static gchar *http_post(const gchar *url, const gchar *req_body, gchar **r_header);

static gboolean socket_input_cb(GIOChannel *source, GIOCondition condition,
				gpointer data)
{
	gint sock, peersock;
	gchar buf[8192];
	GString *req;
	GString *res;
	gchar *getl = NULL;
	gchar *p, *e;
	gchar *code = NULL;
	gchar *r_state;

	req = g_string_new(NULL);
	sock = g_io_channel_unix_get_fd(source);
	peersock = fd_accept(sock);
	while (fd_gets(peersock, buf, sizeof(buf)) > 0) {
		if (buf[0] == '\r') {
			break;
		}
		if (!getl) {
			getl = g_strdup(buf);
		}
		g_string_append(req, buf);
	}
	if (req->len == 0) {
		fd_close(peersock);
		accept_data.done = TRUE;
		return TRUE;
	}
	res = g_string_new("HTTP/1.1 200 OK\r\n");
	g_string_append(res, "Connection: close\r\n");
	g_string_append(res, "Content-Type: text/plain\r\n");
	g_string_append(res, "\r\n");
	g_string_append(res, "Authorization finished. You may close this page.\r\n");
	fd_write_all(peersock, res->str, res->len);
	g_string_free(res, TRUE);
	fd_close(peersock);

	debug_print("Redirected request:\n%s\n", req->str);
	g_string_free(req, TRUE);

	p = strstr(getl, "?state=");
	if (!p) {
		p = strstr(getl, "&state=");
		if (!p) {
			g_warning("state not found");
			goto end;
		}
	}
	p += 7;
	e = strchr(p, '&');
	if (!e) {
		e = strchr(p, ' ');
		if (!e) {
			goto end;
		}
	}

	r_state = g_strndup(p, e - p);
	if (strcmp(r_state, accept_data.state) != 0) {
		g_warning("state doesn't match: %s != %s", r_state, accept_data.state);
		g_free(r_state);
		goto end;
	}
	g_free(r_state);

	p = strstr(getl, "?code=");
	if (!p) {
		p = strstr(getl, "&code=");
		if (!p) {
			g_warning("code not found");
			goto end;
		}
	}
	p += 6;
	e = strchr(p, '&');
	if (!e) {
		e = strchr(p, ' ');
		if (!e) {
			goto end;
		}
	}

	code = g_strndup(p, e - p);

end:
	g_free(getl);

	accept_data.code = code;
	accept_data.done = TRUE;

	return TRUE;
}

static gboolean socket_timeout_cb(gpointer data)
{
	debug_print("socket_timeout_cb: accept timed out\n");
	accept_data.done = TRUE;
	return FALSE;
}

static gchar *http_redirect_accept(gushort port, const gchar *state)
{
	GIOChannel *ch = NULL;
	gint sock;
	gint tag, timeout_tag;
	gchar *code;

	accept_data.done = FALSE;
	accept_data.state = state;
	accept_data.code = NULL;

	sock = fd_open_inet(port);
	if (sock < 0) {
		g_warning("Cannot open port %u\n", port);
		return NULL;
	}

	debug_print("http_redirect_accept: waiting port %u\n", port);
	ch = g_io_channel_unix_new(sock);
	tag = g_io_add_watch(ch, G_IO_IN|G_IO_PRI|G_IO_ERR, socket_input_cb, NULL);
	timeout_tag = g_timeout_add(TIMEOUT_MSEC, socket_timeout_cb, NULL);

	while (!accept_data.done) {
		g_main_context_iteration(NULL, TRUE);
	}

	g_source_remove(tag);
	g_io_channel_shutdown(ch, FALSE, NULL);
	g_io_channel_unref(ch);

	code = accept_data.code;
	return code;
}

static size_t write_func(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t real_size = size * nmemb;
	GString *body = (GString *)userp;

	g_string_append_len(body, contents, real_size);
	return real_size;
}

static gchar *http_get(const gchar *url, gchar **r_header)
{
	CURL *curl;
	CURLcode res;
	GString *header;
	GString *body;

	curl = curl_easy_init();
	if (!curl) {
		return NULL;
	}

	header = g_string_new(NULL);
	body = g_string_new(NULL);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, header);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		g_warning("curl: %s\n", curl_easy_strerror(res));
		g_string_free(body, TRUE);
		curl_easy_cleanup(curl);
		return NULL;
	}
	curl_easy_cleanup(curl);

	if (r_header) {
		*r_header = g_string_free(header, FALSE);
	} else {
		g_string_free(header, TRUE);
	}
	return g_string_free(body, FALSE);
}

static gchar *http_post(const gchar *url, const gchar *req_body, gchar **r_header)
{
	CURL *curl;
	CURLcode res;
	GString *header;
	GString *body;
#ifdef G_OS_WIN32
	gchar *cafile;
#endif

	debug_print("http_post: url: %s\n", url);
	debug_print("http_post: body: %s\n", req_body);

	curl = curl_easy_init();
	if (!curl) {
		return NULL;
	}

	header = g_string_new(NULL);
	body = g_string_new(NULL);

#ifdef G_OS_WIN32
	cafile = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S,
			     "etc/ssl/certs/certs.crt", NULL);
	if (is_file_exist(cafile)) {
		curl_easy_setopt(curl, CURLOPT_CAINFO, cafile);
	} else {
		g_warning("CA bundle file not found.");
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);
	}
	g_free(cafile);
#endif
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, header);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		g_warning("curl: %s\n", curl_easy_strerror(res));
		g_string_free(body, TRUE);
		g_string_free(header, TRUE);
		curl_easy_cleanup(curl);
		return NULL;
	}
	curl_easy_cleanup(curl);

	if (r_header) {
		*r_header = g_string_free(header, FALSE);
	} else {
		g_string_free(header, TRUE);
	}
	return g_string_free(body, FALSE);
}

static APIInfo *get_api_info(GKeyFile *key_file, const gchar *address)
{
	gchar **groups;
	gint i;
	const gchar *group = NULL;
	APIInfo *api = NULL;

	if (!address) return NULL;

	groups = g_key_file_get_groups(key_file, NULL);
	if (!groups) return NULL;

	for (i = 0; groups[i] != NULL; i++) {
		debug_print("group: %s\n", groups[i]);
		if (g_pattern_match_simple(groups[i], address)) {
			debug_print("group: %s matches: %s\n", groups[i], address);
			group = groups[i];
			break;
		}
	}

	if (group) {
		api = g_new0(APIInfo, 1);
		api->auth_uri = g_key_file_get_string(key_file, group, "auth_uri", NULL);
		api->token_uri = g_key_file_get_string(key_file, group, "token_uri", NULL);
		api->redirect_uri = g_key_file_get_string(key_file, group, "redirect_uri", NULL);
		api->client_id = g_key_file_get_string(key_file, group, "client_id", NULL);
		api->client_secret = g_key_file_get_string(key_file, group, "client_secret", NULL);
		api->scope = g_key_file_get_string(key_file, group, "scope", NULL);
		api->local_port = g_key_file_get_integer(key_file, group, "local_port", NULL);
		if (api->local_port == 0)
			api->local_port = 8089;
	}

	g_strfreev(groups);

	return api;
}

static gint parse_token_response(const gchar *body, TokenData *data)
{
	const gchar *p, *e;

	if (!body || !data) return -1;

	p = strstr(body, "\"access_token\"");
	if (!p) return -1;
	p += 14;
	p = strchr(p, ':');
	if (!p) return -1;
	p++;
	p = strchr(p, '\"');
	if (!p) return -1;
	p++;
	e = strchr(p, '\"');
	if (!e) return -1;
	data->access_token = g_strndup(p, e - p);

	p = strstr(body, "\"refresh_token\"");
	if (!p) return 0;
	p += 15;
	p = strchr(p, ':');
	if (!p) return 0;
	p++;
	p = strchr(p, '\"');
	if (!p) return 0;
	p++;
	e = strchr(p, '\"');
	if (!e) return 0;
	data->refresh_token = g_strndup(p, e - p);

	return 0;
}

static gchar *generate_state()
{
	guint32 data[8];
	gint i;
	gchar *str;

	for (i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
		data[i] = g_random_int();
	}

	str = g_malloc(sizeof(data) * 2 + 1);
	base64_encode(str, (guchar *)data, sizeof(data));
	/* convert to Base64 URL encoding */
	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] == '+')
			str[i] = '-';
		else if (str[i] == '/')
			str[i] = '_';
		else if (str[i] == '=') {
			str[i] = '\0';
			break;
		}
	}
	return str;
}

int main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;
	gchar *header = NULL, *body;
	gchar *auth_req_uri;
	GString *auth_uri;
	CURL *curl;
	gchar *tmp;
	gchar *code;
	gchar *state;
	GString *req_body;
	GKeyFile *key_file;
	gchar *file;
	TokenData data = {0, 0, NULL, NULL};
	gint i;
	const gchar *address = NULL;
	APIInfo *api;

	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--debug", 7)) {
			set_debug_mode(TRUE);
		} else if (!strncmp(argv[i], "--configdir", 11)) {
			const gchar *p = argv[i + 1];

			if (p && *p != '\0' && *p != '-') {
				/* this must only be done at startup */
#ifdef G_OS_WIN32
				gchar *utf8dir;

				utf8dir = g_locale_to_utf8
					(p, -1, NULL, NULL, NULL);
				if (utf8dir) {
					set_rc_dir(utf8dir);
					g_free(utf8dir);
				} else
					set_rc_dir(p);
#else
				set_rc_dir(p);
#endif
				i++;
			}
		} else if (!address && argv[i] && argv[i][0] != '-') {
			address = argv[i];
		}
	}

	key_file = g_key_file_new();
	file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "oauth2.ini", NULL);
	if (!g_key_file_load_from_file(key_file, file, G_KEY_FILE_NONE, NULL)) {
		g_free(file);
		file = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S, "oauth2.ini", NULL);
		if (!g_key_file_load_from_file(key_file, file, G_KEY_FILE_NONE, NULL)) {
			g_free(file);
			g_warning("oauth2.ini not found.");
			return EXIT_FAILURE;
		}
	}

	debug_print("load %s\n", file);
	g_free(file);

	if (!address) {
		g_warning("You must specify user id (address).");
		return EXIT_FAILURE;
	}
	api = get_api_info(key_file, address);
	if (!api) {
		g_warning("could not get API info");
		return EXIT_FAILURE;
	}

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();
	if (!curl) {
		curl_global_cleanup();
		return EXIT_FAILURE;
	}
	auth_uri = g_string_new(api->auth_uri);
	g_string_append(auth_uri, "?response_type=code");
	g_string_append_printf(auth_uri, "&client_id=%s", api->client_id);
	tmp = curl_easy_escape(curl, api->redirect_uri, 0);
	g_string_append_printf(auth_uri, "&redirect_uri=%s", tmp);
	curl_free(tmp);
	tmp = curl_easy_escape(curl, api->scope, 0);
	g_string_append_printf(auth_uri, "&scope=%s", tmp);
	curl_free(tmp);
	state = generate_state();
	g_string_append_printf(auth_uri, "&state=%s", state);
	g_string_append(auth_uri, "&access_type=offline");

	debug_print("url: %s\n", auth_uri->str);
	open_uri(auth_uri->str, NULL);
	g_string_free(auth_uri, TRUE);

	code = http_redirect_accept(api->local_port, state);
	if (!code) {
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		return EXIT_FAILURE;
	}
	debug_print("code: %s\n",code);

	req_body = g_string_new(NULL);
	g_string_append_printf(req_body, "client_id=%s", api->client_id);
	tmp = curl_easy_escape(curl, api->scope, 0);
	g_string_append_printf(req_body, "&scope=%s", tmp);
	curl_free(tmp);
	g_string_append_printf(req_body, "&state=%s", state);
	g_string_append_printf(req_body, "&code=%s", code);
	tmp = curl_easy_escape(curl, api->redirect_uri, 0);
	g_string_append_printf(req_body, "&redirect_uri=%s", tmp);
	curl_free(tmp);
	g_string_append(req_body, "&grant_type=authorization_code");
	tmp = curl_easy_escape(curl, api->client_secret, 0);
	g_string_append_printf(req_body, "&client_secret=%s", tmp);
	curl_free(tmp);

	body = http_post(api->token_uri, req_body->str, &header);
	if (!header && !body) {
		g_warning("could not get token");
	}
	if (header) {
		debug_print("token response header:\n%s\n", header);
		g_free(header);
	}
	if (body) {
		debug_print("token response body:\n%s\n\n", body);
		parse_token_response(body, &data);
		if (data.access_token) {
			fputs(data.access_token, stdout);
			fputs("\n", stdout);
		}
		if (data.refresh_token) {
			fputs(data.refresh_token, stdout);
			fputs("\n", stdout);
		}
		g_free(body);
	}
	g_string_free(req_body, TRUE);
	g_free(code);
	g_free(state);

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	return ret;
}
