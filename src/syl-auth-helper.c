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
#include <gpgme.h>
#ifdef G_OS_WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <sys/file.h>
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
	gint use_pkce;
	gchar *gpg_key_id;
} APIInfo;

typedef struct _TokenData
{
	gint expires_in;
	time_t expires_at;
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

typedef struct _GpgGlobals
{
	gpgme_ctx_t gpg_ctx;
	gpgme_key_t recipient_key[2];
	gchar *gpg_key_id;
	gchar *oauth_creds_file;
} GpgGlobals;

static GpgGlobals gpg_globals;

static char *lock_file_path;
static int lock_fd;

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
	//debug_print("http_post: body: %s\n", req_body); //contains refresh token which grants access to your email, do not uncomment and show this output to anyone else.

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
		api->use_pkce = g_key_file_get_integer(key_file, group, "use_pkce", NULL);
		api->gpg_key_id = g_key_file_get_string(key_file, group, "gpg_key_id", NULL);
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
	if (!p) return -1;
	p += 15;
	p = strchr(p, ':');
	if (!p) return -1;
	p++;
	p = strchr(p, '\"');
	if (!p) return -1;
	p++;
	e = strchr(p, '\"');
	if (!e) return -1;
	data->refresh_token = g_strndup(p, e - p);

	p = strstr(body, "\"expires_in\"");
	if (!p) return -1;
	p += 12;
	p = strchr(p, ':');
	if (!p) return -1;
	p++;
	e = strchr(p, ',');
	if (!e) {
		e = strchr(p, '}');
	}
	if (!e) return -1;
	char * tmp = g_strndup(p, e - p);
	data->expires_in = atol(tmp);
	free(tmp);
	data->expires_at = time(NULL) + data->expires_in;
	debug_print ("parsed oauth token from http response, expires in %d seconds from now, at unix time %lu\n", data->expires_in, data->expires_at);
	return 0;
}

static gint parse_token_file(const gchar *body, TokenData *data)
{
	//format is:
	//line1: access token
	//line2: refresh_token
	//line3: unix time expiry
	const gchar *p, *e;

	//could rewrite using g_strsplit.
	if (!body || !data) return -1;
	p=body;
	e = strstr(body, "\n");
	if (!e) return -1;
	data->access_token = g_strndup(p, e - p);
	p = e+1;

	e = strstr(p, "\n");
	if (!e) return -1;
	data->refresh_token = g_strndup(p, e - p);
	p = e+1;

	e = strstr(p, "\n");
	if (!e) return -1;
	char * tmp = g_strndup(p, e - p);
	data->expires_at = atol(tmp);
	free (tmp);
	debug_print ("got OAuth token from storage, expires at unix time %ld\n", data->expires_at);
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

static gchar *generate_challenge(char* verifier)
{
	GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
	gsize *checksum_len = malloc(sizeof(gsize));
	*checksum_len = 32; //sha256 is 32 bytes
	guint8 *checksum_dest = malloc(*checksum_len);
	gchar *str;
	g_checksum_update(checksum, (const guchar*)verifier, strlen(verifier));
	g_checksum_get_digest(checksum, checksum_dest, checksum_len);
	g_checksum_free(checksum);
	str = g_malloc(*checksum_len * 2 + 1);
	base64_encode(str, checksum_dest, *checksum_len);
	free(checksum_len);
	free(checksum_dest);
	/* convert to Base64 URL encoding */
	for (int i = 0; str[i] != '\0'; i++) {
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

#define fail_if_err(gpg_err)						\
	do {								\
		if (gpg_err) {						\
			fprintf(stderr, "%s:%d: %s: %s\n",		\
				__FILE__, __LINE__, gpgme_strsource(gpg_err), gpgme_strerror(gpg_err)); \
			exit(1);					\
		}							\
	} while (0)

//Returns:
//True if token is present and needs refresh,
//False if token isn't present and need to obtain a new one,
//Exits if present and doesn't need refresh.
gboolean get_creds_from_storage(TokenData *data)
{
	gpgme_error_t gpg_err;
	gpgme_data_t gpg_data_crypt;
	gpgme_data_t gpg_data_plain;
	const size_t read_size = 4096;
	gchar *oauth_creds_plain_buffer;
	size_t oauth_creds_plain_buffer_sz = read_size;
	size_t total_read;
	ssize_t bytes_read;
	gboolean ret;

	struct stat fileStat;
	if (stat(gpg_globals.oauth_creds_file, &fileStat) == 0) {
		// Check if others have read permission, they should not.
		if ((fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) != (S_IRUSR|S_IWUSR)) {
			fprintf(stderr, "Permissions for %s are not -rw-------.\n", gpg_globals.oauth_creds_file);
			exit(-1);
		}
		gpg_err = gpgme_data_new_from_file(&gpg_data_crypt, gpg_globals.oauth_creds_file, 1);
		fail_if_err(gpg_err);
		gpg_err = gpgme_data_new(&gpg_data_plain);
		fail_if_err(gpg_err);
		gpg_err = gpgme_op_decrypt(gpg_globals.gpg_ctx, gpg_data_crypt, gpg_data_plain);
		fail_if_err(gpg_err);
		gpgme_data_seek(gpg_data_plain, 0, SEEK_SET);
		oauth_creds_plain_buffer = malloc (oauth_creds_plain_buffer_sz);
		total_read = 0;
		while (1) {
			bytes_read = gpgme_data_read(gpg_data_plain, oauth_creds_plain_buffer+total_read, read_size);
			total_read += bytes_read;
			if (bytes_read < 0) {
				fprintf(stderr, "gpgme_data_read on file %s returned %lu\n", gpg_globals.oauth_creds_file, bytes_read);
				free (oauth_creds_plain_buffer);
				exit (-1);
			}
			if (bytes_read == 0) {
				break;
			}
			oauth_creds_plain_buffer_sz += read_size;
			oauth_creds_plain_buffer = realloc (oauth_creds_plain_buffer, oauth_creds_plain_buffer_sz);
		}
		if (parse_token_file(oauth_creds_plain_buffer, data) != 0) {
			fprintf(stderr, "File %s exists but failed to parse it.\n", gpg_globals.oauth_creds_file);
			//fprintf(stderr, "raw data in file (do not show this to anyone as it may grant access to your email): \n%s\n\n", oauth_creds_plain_buffer);
			free (oauth_creds_plain_buffer);
			exit (-1);
		}
		free (oauth_creds_plain_buffer);
		if (data->expires_at && time(NULL) < data->expires_at-60) {
			debug_print("access token found in %s hasn't expired yet, returning it without refresh.\n", gpg_globals.oauth_creds_file);
			printf ("%s\n%lu\n", data->access_token, (uint64_t)data->expires_at);
			gpgme_data_release(gpg_data_crypt);
			gpgme_data_release(gpg_data_plain);
			gpgme_release(gpg_globals.gpg_ctx);
			free(data->access_token);
			free(data->refresh_token);
			exit(0);
		} else {
			debug_print("access token found in %s has expired or is about to soon, refreshing it.\n", gpg_globals.oauth_creds_file);
			free(data->access_token);
			if  (!lock_file_path) {
				//On 1st call, lock_file_path isn't defined,
				//and this will be called again after
				//lock_file_path is set, so free
				//refresh_token now, otherwise it leaks.
				free(data->refresh_token);
			}
			ret = TRUE;
		}
		gpgme_data_release(gpg_data_crypt);
		gpgme_data_release(gpg_data_plain);
	} else {
		debug_print("OAuth creds file %s not found, doing initial auth flow and will create it if successful.\n", gpg_globals.oauth_creds_file);
		ret = FALSE;
	}
	return ret;
}

void save_creds_to_storage(TokenData *data)
{
	gpgme_error_t gpg_err;
	gpgme_data_t gpg_data_crypt;
	gpgme_data_t gpg_data_plain;
	const size_t read_size = 4096;
	gchar *oauth_creds_plain_buffer;
	size_t oauth_creds_plain_buffer_sz;
	gchar *oauth_creds_crypt_buffer;
	size_t oauth_creds_crypt_buffer_sz = read_size;
	size_t total_read;
	ssize_t bytes_read;

	oauth_creds_plain_buffer_sz = strlen(data->access_token) + strlen(data->refresh_token) + 20;
	oauth_creds_plain_buffer = malloc (oauth_creds_plain_buffer_sz);
	sprintf (oauth_creds_plain_buffer, "%s\n%s\n%lu\n", data->access_token, data->refresh_token, (uint64_t)data->expires_at);
	free(data->refresh_token);
	gpg_err = gpgme_data_new_from_mem(&gpg_data_plain, oauth_creds_plain_buffer, strlen(oauth_creds_plain_buffer), 0);
	fail_if_err(gpg_err);
	gpg_err = gpgme_data_new(&gpg_data_crypt);
	fail_if_err(gpg_err);
	gpg_err = gpgme_op_encrypt(gpg_globals.gpg_ctx, gpg_globals.recipient_key, GPGME_ENCRYPT_ALWAYS_TRUST, gpg_data_plain, gpg_data_crypt);
	fail_if_err(gpg_err);
	gpgme_key_unref(gpg_globals.recipient_key[0]);
	//In gpgme 1.24.0 and later can use gpgme_data_set_file_name(gpg_data_crypt, oauth_creds_file) instead of all the following:
	gpgme_data_seek(gpg_data_crypt, 0, SEEK_SET);
	oauth_creds_crypt_buffer = malloc (oauth_creds_crypt_buffer_sz);
	total_read = 0;
	while (1) {
		bytes_read = gpgme_data_read(gpg_data_crypt, oauth_creds_crypt_buffer+total_read, read_size);
		total_read += bytes_read;
		if (bytes_read < 0) {
			fprintf(stderr, "gpgme_data_read on encrypted data in mem returned %ld\n", bytes_read);
			free (oauth_creds_crypt_buffer);
			gpgme_data_release(gpg_data_crypt);
			gpgme_data_release(gpg_data_plain);
			gpgme_release(gpg_globals.gpg_ctx);
			exit (-1);
		}
		if (bytes_read == 0) {
			break;
		}
		oauth_creds_crypt_buffer_sz += read_size;
		oauth_creds_crypt_buffer = realloc (oauth_creds_crypt_buffer, oauth_creds_crypt_buffer_sz);
	}
	gpgme_data_release(gpg_data_crypt);
	gpgme_data_release(gpg_data_plain);
	free(oauth_creds_plain_buffer);
	unlink (gpg_globals.oauth_creds_file);
	umask(0077);
	FILE *fp;
	fp = fopen(gpg_globals.oauth_creds_file, "w");
	if (fp == NULL) {
		int saved_errno = errno;
		fprintf(stderr, "Error opening file %s to save the token: ", gpg_globals.oauth_creds_file);
		errno = saved_errno;
		perror(NULL);
		exit (-1);
	}
	if (fwrite(oauth_creds_crypt_buffer, 1, total_read, fp) != total_read) {
		int saved_errno = errno;
		fprintf(stderr, "Error writing encrypted token to file %s: ", gpg_globals.oauth_creds_file);
		errno = saved_errno;
		perror(NULL);
		exit (-1);
	}
	fclose(fp);
	free(oauth_creds_crypt_buffer);
}

int lock_for_single_instance(){
	int fd = open(lock_file_path, O_CREAT | O_RDWR, 0666);
	if (fd == -1) {
		int saved_errno = errno;
		fprintf(stderr, "Error opening lock file %s\n", lock_file_path);
		errno = saved_errno;
		perror(NULL);
		exit(-1);
	}
	debug_print("calling flock on %s\n",lock_file_path);
	if (flock(fd, LOCK_EX) == -1) {
		int saved_errno = errno;
		fprintf(stderr, "Error acquiring file lock on file %s\n", lock_file_path);
		errno = saved_errno;
		perror(NULL);
		exit(-1);
	}
	debug_print("got lock\n");
	return fd;
}

static void unlock (){
	debug_print("unlocking, removing lock file %s\n", lock_file_path);
	flock(lock_fd, LOCK_UN);
	close(lock_fd);
	unlink(lock_file_path);
	free(lock_file_path);
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
	TokenData data = {0, 0, 0, NULL, NULL};
	gint i;
	const gchar *address = NULL;
	APIInfo *api;
	gchar *code_challenge = NULL;
	gchar *code_verifier = NULL;
	gboolean refresh = FALSE;

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

	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--debug", 7)) {
			set_debug_mode(TRUE);
		} else if (!address && argv[i] && argv[i][0] != '-') {
			address = argv[i];
		}
	}

	debug_print("load %s\n", file);
	g_free(file);

	if (!address) {
		g_warning("You must specify user id (address).");
		return EXIT_FAILURE;
	}
	api = get_api_info(key_file, address);
	g_key_file_free(key_file);
	if (!api) {
		g_warning("could not get API info");
		return EXIT_FAILURE;
	}

	if (api->gpg_key_id) {
		gpg_globals.gpg_key_id = api->gpg_key_id;
	} else {
		gpg_globals.gpg_key_id = (char*)address;
	}
	debug_print("Will use gpg key with id %s for token storage.\n", gpg_globals.gpg_key_id);
	gpg_globals.oauth_creds_file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "oauth_credentials_", address, ".gpg", NULL);
	gpgme_check_version(NULL);
	gpgme_error_t gpg_err;
	gpg_err = gpgme_new(&gpg_globals.gpg_ctx);
	fail_if_err(gpg_err);
	gpgme_set_textmode(gpg_globals.gpg_ctx, 1);
	gpg_err = gpgme_get_key(gpg_globals.gpg_ctx, gpg_globals.gpg_key_id, gpg_globals.recipient_key, 1);
	gpgme_err_code_t gpgme_code = gpgme_err_code(gpg_err);
	if (gpgme_code == GPG_ERR_EOF) {
		fprintf(stderr, "Failed to find gpg key with secret key for id %s.  You must have this (your own key with secret), or generate a new one, or set gpg_key_id in oauth2.ini to an existing one, to be able to decrypt the token which will be stored encrypted for this account.\n", gpg_globals.gpg_key_id);
		gpgme_release(gpg_globals.gpg_ctx);
		exit (-1);
	} else {
		fail_if_err(gpg_err);
	}
	gpg_globals.recipient_key[1] = NULL; // Null-terminate the array

	refresh = get_creds_from_storage(&data);

	lock_file_path = malloc (strlen(gpg_globals.oauth_creds_file) + 6);
	sprintf(lock_file_path, "%s.lock",gpg_globals.oauth_creds_file);
	lock_fd = lock_for_single_instance();
	atexit(unlock);

	//call it again because if another instance got the lock and updated
	//it, then this will now just output it and exit.
	refresh = get_creds_from_storage(&data);

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();
	if (!curl) {
		curl_global_cleanup();
		return EXIT_FAILURE;
	}
	if (!refresh) {
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

		if (api->use_pkce) {
			g_string_append(auth_uri, "&code_challenge_method=S256");
			code_verifier = generate_state();
			debug_print("code_verifier =>%s<=\n",code_verifier);
			code_challenge = generate_challenge(code_verifier);
			debug_print("code_challenge =>%s<=\n",code_challenge);
			g_string_append_printf(auth_uri, "&code_challenge=%s",code_challenge);
			free(code_challenge);
		}
		debug_print("url: %s\n", auth_uri->str);
		//chromium prints "Opening in existing browser session." on
		//stdout, which must be prevented from appearing on this
		//program's stdout, because later the token will be printed
		//on stdout.
		int original_stdout_fd = dup(fileno(stdout));
		freopen("/dev/null", "w", stdout);
		open_uri(auth_uri->str, NULL);
		dup2(original_stdout_fd, fileno(stdout));
		close(original_stdout_fd);
		//freopen("CON", "w", stdout);//for windows, needs testing
		g_string_free(auth_uri, TRUE);
		code = http_redirect_accept(api->local_port, state);
		g_free(state);
		if (!code) {
			curl_easy_cleanup(curl);
			curl_global_cleanup();
			return EXIT_FAILURE;
		}
		//debug_print("code: %s\n",code); //possibly allows access to your account, do not uncomment and show this output to anyone else.
	}

	req_body = g_string_new(NULL);
	g_string_append_printf(req_body, "client_id=%s", api->client_id);
	if (!refresh){
		g_string_append_printf(req_body, "&code=%s", code);
		g_free(code);
		tmp = curl_easy_escape(curl, api->redirect_uri, 0);
		g_string_append_printf(req_body, "&redirect_uri=%s", tmp);
		curl_free(tmp);
		g_string_append(req_body, "&grant_type=authorization_code");
		if (api->use_pkce) {
			g_string_append_printf(req_body, "&code_verifier=%s",code_verifier);
			free(code_verifier);
		}
	} else {
		g_string_append(req_body, "&grant_type=refresh_token");
		g_string_append_printf(req_body, "&refresh_token=%s", data.refresh_token);
		free(data.refresh_token);
	}
	if (api->client_secret) {
		tmp = curl_easy_escape(curl, api->client_secret, 0);
		g_string_append_printf(req_body, "&client_secret=%s", tmp);
		curl_free(tmp);
	}

	body = http_post(api->token_uri, req_body->str, &header);
	if (!header || !body) {
		g_warning("could not get token, header or body is null");
		exit (-1);
	}

	debug_print("token response header:\n%s\n", header);
	g_free(header);
	g_string_free(req_body, TRUE);

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	if (api->auth_uri) g_free(api->auth_uri);
	if (api->token_uri) g_free(api->token_uri);
	if (api->redirect_uri) g_free(api->redirect_uri);
	if (api->client_id) g_free(api->client_id);
	if (api->client_secret) g_free(api->client_secret);
	if (api->scope) g_free(api->scope);
	g_free(api);

	//debug_print("token response body:\n%s\n\n", body); //contains refresh token, do not uncomment and show this output to anyone else.
	debug_print("token response body %lu bytes\n", strlen(body));
	if (parse_token_response(body, &data) != 0){
		fprintf(stderr, "Failed to parse body from server\n");
		//fprintf(stderr, "raw response from server (do not show this to anyone as it may grant access to your email): \n%s\n\n", body);
		exit (-1);
	}
	g_free(body);

	save_creds_to_storage(&data);
	gpgme_release(gpg_globals.gpg_ctx);

	//output it.
	printf ("%s\n%lu\n", data.access_token, (uint64_t)data.expires_at);
	free(data.access_token);
	return ret;
}
