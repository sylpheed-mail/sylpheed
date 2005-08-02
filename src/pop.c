/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "pop.h"
#include "md5.h"
#include "prefs_account.h"
#include "utils.h"
#include "recv.h"

static gint pop3_greeting_recv		(Pop3Session *session,
					 const gchar *msg);
static gint pop3_getauth_user_send	(Pop3Session *session);
static gint pop3_getauth_pass_send	(Pop3Session *session);
static gint pop3_getauth_apop_send	(Pop3Session *session);
#if USE_SSL
static gint pop3_stls_send		(Pop3Session *session);
static gint pop3_stls_recv		(Pop3Session *session);
#endif
static gint pop3_getrange_stat_send	(Pop3Session *session);
static gint pop3_getrange_stat_recv	(Pop3Session *session,
					 const gchar *msg);
static gint pop3_getrange_last_send	(Pop3Session *session);
static gint pop3_getrange_last_recv	(Pop3Session *session,
					 const gchar *msg);
static gint pop3_getrange_uidl_send	(Pop3Session *session);
static gint pop3_getrange_uidl_recv	(Pop3Session *session,
					 const gchar *data,
					 guint        len);
static gint pop3_getsize_list_send	(Pop3Session *session);
static gint pop3_getsize_list_recv	(Pop3Session *session,
					 const gchar *data,
					 guint        len);
static gint pop3_retr_send		(Pop3Session *session);
static gint pop3_retr_recv		(Pop3Session *session,
					 const gchar *data,
					 guint        len);
static gint pop3_delete_send		(Pop3Session *session);
static gint pop3_delete_recv		(Pop3Session *session);
static gint pop3_logout_send		(Pop3Session *session);

static void pop3_gen_send		(Pop3Session	*session,
					 const gchar	*format, ...);

static void pop3_session_destroy	(Session	*session);

static gint pop3_write_msg_to_file	(const gchar	*file,
					 const gchar	*data,
					 guint		 len);

static Pop3State pop3_lookup_next	(Pop3Session	*session);
static Pop3ErrorValue pop3_ok		(Pop3Session	*session,
					 const gchar	*msg);

static gint pop3_session_recv_msg		(Session	*session,
						 const gchar	*msg);
static gint pop3_session_recv_data_finished	(Session	*session,
						 guchar		*data,
						 guint		 len);


static gint pop3_greeting_recv(Pop3Session *session, const gchar *msg)
{
	session->state = POP3_GREETING;

	session->greeting = g_strdup(msg);
	return PS_SUCCESS;
}

#if USE_SSL
static gint pop3_stls_send(Pop3Session *session)
{
	session->state = POP3_STLS;
	pop3_gen_send(session, "STLS");
	return PS_SUCCESS;
}

static gint pop3_stls_recv(Pop3Session *session)
{
	if (session_start_tls(SESSION(session)) < 0) {
		session->error_val = PS_SOCKET;
		return -1;
	}
	return PS_SUCCESS;
}
#endif /* USE_SSL */

static gint pop3_getauth_user_send(Pop3Session *session)
{
	g_return_val_if_fail(session->user != NULL, -1);

	session->state = POP3_GETAUTH_USER;
	pop3_gen_send(session, "USER %s", session->user);
	return PS_SUCCESS;
}

static gint pop3_getauth_pass_send(Pop3Session *session)
{
	g_return_val_if_fail(session->pass != NULL, -1);

	session->state = POP3_GETAUTH_PASS;
	pop3_gen_send(session, "PASS %s", session->pass);
	return PS_SUCCESS;
}

static gint pop3_getauth_apop_send(Pop3Session *session)
{
	gchar *start, *end;
	gchar *apop_str;
	gchar md5sum[33];

	g_return_val_if_fail(session->user != NULL, -1);
	g_return_val_if_fail(session->pass != NULL, -1);

	session->state = POP3_GETAUTH_APOP;

	if ((start = strchr(session->greeting, '<')) == NULL) {
		log_warning(_("Required APOP timestamp not found "
			      "in greeting\n"));
		session->error_val = PS_PROTOCOL;
		return -1;
	}

	if ((end = strchr(start, '>')) == NULL || end == start + 1) {
		log_warning(_("Timestamp syntax error in greeting\n"));
		session->error_val = PS_PROTOCOL;
		return -1;
	}

	*(end + 1) = '\0';

	apop_str = g_strconcat(start, session->pass, NULL);
	md5_hex_digest(md5sum, apop_str);
	g_free(apop_str);

	pop3_gen_send(session, "APOP %s %s", session->user, md5sum);

	return PS_SUCCESS;
}

static gint pop3_getrange_stat_send(Pop3Session *session)
{
	session->state = POP3_GETRANGE_STAT;
	pop3_gen_send(session, "STAT");
	return PS_SUCCESS;
}

static gint pop3_getrange_stat_recv(Pop3Session *session, const gchar *msg)
{
	if (sscanf(msg, "%d %d", &session->count, &session->total_bytes) != 2) {
		log_warning(_("POP3 protocol error\n"));
		session->error_val = PS_PROTOCOL;
		return -1;
	} else {
		if (session->count == 0) {
			session->uidl_is_valid = TRUE;
		} else {
			session->msg = g_new0(Pop3MsgInfo, session->count + 1);
			session->cur_msg = 1;
		}
	}

	return PS_SUCCESS;
}

static gint pop3_getrange_last_send(Pop3Session *session)
{
	session->state = POP3_GETRANGE_LAST;
	pop3_gen_send(session, "LAST");
	return PS_SUCCESS;
}

static gint pop3_getrange_last_recv(Pop3Session *session, const gchar *msg)
{
	gint last;

	if (sscanf(msg, "%d", &last) == 0) {
		log_warning(_("POP3 protocol error\n"));
		session->error_val = PS_PROTOCOL;
		return -1;
	} else {
		if (session->count > last) {
			session->new_msg_exist = TRUE;
			session->cur_msg = last + 1;
		} else
			session->cur_msg = 0;
	}

	return PS_SUCCESS;
}

static gint pop3_getrange_uidl_send(Pop3Session *session)
{
	session->state = POP3_GETRANGE_UIDL;
	pop3_gen_send(session, "UIDL");
	return PS_SUCCESS;
}

static gint pop3_getrange_uidl_recv(Pop3Session *session, const gchar *data,
				    guint len)
{
	gchar id[IDLEN + 1];
	gchar buf[POPBUFSIZE];
	gint buf_len;
	gint num;
	time_t recv_time;
	const gchar *p = data;
	const gchar *lastp = data + len;
	const gchar *newline;

	while (p < lastp) {
		if ((newline = memchr(p, '\r', lastp - p)) == NULL)
			return -1;
		buf_len = MIN(newline - p, sizeof(buf) - 1);
		memcpy(buf, p, buf_len);
		buf[buf_len] = '\0';

		p = newline + 1;
		if (p < lastp && *p == '\n') p++;

		if (sscanf(buf, "%d %" Xstr(IDLEN) "s", &num, id) != 2 ||
		    num <= 0 || num > session->count) {
			log_warning(_("invalid UIDL response: %s\n"), buf);
			continue;
		}

		session->msg[num].uidl = g_strdup(id);

		recv_time = (time_t)g_hash_table_lookup(session->uidl_table, id);
		session->msg[num].recv_time = recv_time;

		if (!session->ac_prefs->getall && recv_time != RECV_TIME_NONE)
			session->msg[num].received = TRUE;

		if (!session->new_msg_exist &&
		    (session->ac_prefs->getall || recv_time == RECV_TIME_NONE ||
		     session->ac_prefs->rmmail)) {
			session->cur_msg = num;
			session->new_msg_exist = TRUE;
		}
	}

	session->uidl_is_valid = TRUE;
	return PS_SUCCESS;
}

static gint pop3_getsize_list_send(Pop3Session *session)
{
	session->state = POP3_GETSIZE_LIST;
	pop3_gen_send(session, "LIST");
	return PS_SUCCESS;
}

static gint pop3_getsize_list_recv(Pop3Session *session, const gchar *data,
				   guint len)
{
	gchar buf[POPBUFSIZE];
	gint buf_len;
	guint num, size;
	const gchar *p = data;
	const gchar *lastp = data + len;
	const gchar *newline;

	while (p < lastp) {
		if ((newline = memchr(p, '\r', lastp - p)) == NULL)
			return -1;
		buf_len = MIN(newline - p, sizeof(buf) - 1);
		memcpy(buf, p, buf_len);
		buf[buf_len] = '\0';

		p = newline + 1;
		if (p < lastp && *p == '\n') p++;

		if (sscanf(buf, "%u %u", &num, &size) != 2) {
			session->error_val = PS_PROTOCOL;
			return -1;
		}

		if (num > 0 && num <= session->count)
			session->msg[num].size = size;
		if (num > 0 && num < session->cur_msg)
			session->cur_total_bytes += size;
	}

	return PS_SUCCESS;
}

static gint pop3_retr_send(Pop3Session *session)
{
	session->state = POP3_RETR;
	pop3_gen_send(session, "RETR %d", session->cur_msg);
	return PS_SUCCESS;
}

static gint pop3_retr_recv(Pop3Session *session, const gchar *data, guint len)
{
	gchar *file;
	gint drop_ok;

	file = get_tmp_file();
	if (pop3_write_msg_to_file(file, data, len) < 0) {
		g_free(file);
		session->error_val = PS_IOERR;
		return -1;
	}

	drop_ok = session->drop_message(session, file);
	unlink(file);
	g_free(file);
	if (drop_ok < 0) {
		session->error_val = PS_IOERR;
		return -1;
	}

	session->cur_total_bytes += session->msg[session->cur_msg].size;
	session->cur_total_recv_bytes += session->msg[session->cur_msg].size;
	session->cur_total_num++;

	session->msg[session->cur_msg].received = TRUE;
	session->msg[session->cur_msg].recv_time =
		drop_ok == DROP_DONT_RECEIVE ? RECV_TIME_KEEP
			: drop_ok == DROP_DELETE ? RECV_TIME_DELETE
			: session->current_time;

	return PS_SUCCESS;
}

static gint pop3_delete_send(Pop3Session *session)
{
	session->state = POP3_DELETE;
	pop3_gen_send(session, "DELE %d", session->cur_msg);
	return PS_SUCCESS;
}

static gint pop3_delete_recv(Pop3Session *session)
{
	session->msg[session->cur_msg].deleted = TRUE;
	return PS_SUCCESS;
}

static gint pop3_logout_send(Pop3Session *session)
{
	session->state = POP3_LOGOUT;
	pop3_gen_send(session, "QUIT");
	return PS_SUCCESS;
}

static void pop3_gen_send(Pop3Session *session, const gchar *format, ...)
{
	gchar buf[POPBUFSIZE + 1];
	va_list args;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf) - 2, format, args);
	va_end(args);

	if (!g_ascii_strncasecmp(buf, "PASS ", 5))
		log_print("POP3> PASS ********\n");
	else
		log_print("POP3> %s\n", buf);

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, buf);
}

Session *pop3_session_new(PrefsAccount *account)
{
	Pop3Session *session;

	g_return_val_if_fail(account != NULL, NULL);

	session = g_new0(Pop3Session, 1);

	session_init(SESSION(session));

	SESSION(session)->type = SESSION_POP3;

	SESSION(session)->recv_msg = pop3_session_recv_msg;
	SESSION(session)->recv_data_finished = pop3_session_recv_data_finished;
	SESSION(session)->send_data_finished = NULL;

	SESSION(session)->destroy = pop3_session_destroy;

	session->state = POP3_READY;
	session->ac_prefs = account;
	session->uidl_table = pop3_get_uidl_table(account);
	session->current_time = time(NULL);
	session->error_val = PS_SUCCESS;
	session->error_msg = NULL;

	return SESSION(session);
}

static void pop3_session_destroy(Session *session)
{
	Pop3Session *pop3_session = POP3_SESSION(session);
	gint n;

	g_return_if_fail(session != NULL);

	for (n = 1; n <= pop3_session->count; n++)
		g_free(pop3_session->msg[n].uidl);
	g_free(pop3_session->msg);

	if (pop3_session->uidl_table) {
		hash_free_strings(pop3_session->uidl_table);
		g_hash_table_destroy(pop3_session->uidl_table);
	}

	g_free(pop3_session->greeting);
	g_free(pop3_session->user);
	g_free(pop3_session->pass);
	g_free(pop3_session->error_msg);
}

GHashTable *pop3_get_uidl_table(PrefsAccount *ac_prefs)
{
	GHashTable *table;
	gchar *path;
	FILE *fp;
	gchar buf[POPBUFSIZE];
	gchar uidl[POPBUFSIZE];
	time_t recv_time;
	time_t now;

	table = g_hash_table_new(g_str_hash, g_str_equal);

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   UIDL_DIR, G_DIR_SEPARATOR_S, ac_prefs->recv_server,
			   "-", ac_prefs->userid, NULL);
	if ((fp = fopen(path, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
		g_free(path);
		return table;
	}
	g_free(path);

	now = time(NULL);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		recv_time = RECV_TIME_NONE;
		if (sscanf(buf, "%s\t%ld", uidl, &recv_time) != 2) {
			if (sscanf(buf, "%s", uidl) != 1)
				continue;
			else
				recv_time = now;
		}
		if (recv_time == RECV_TIME_NONE)
			recv_time = RECV_TIME_RECEIVED;
		g_hash_table_insert(table, g_strdup(uidl),
				    GINT_TO_POINTER(recv_time));
	}

	fclose(fp);
	return table;
}

gint pop3_write_uidl_list(Pop3Session *session)
{
	gchar *path;
	FILE *fp;
	Pop3MsgInfo *msg;
	gint n;

	if (!session->uidl_is_valid) return 0;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl", G_DIR_SEPARATOR_S,
			   session->ac_prefs->recv_server,
			   "-", session->ac_prefs->userid, NULL);
	if ((fp = fopen(path, "wb")) == NULL) {
		FILE_OP_ERROR(path, "fopen");
		g_free(path);
		return -1;
	}

	for (n = 1; n <= session->count; n++) {
		msg = &session->msg[n];
		if (msg->uidl && msg->received && !msg->deleted)
			fprintf(fp, "%s\t%ld\n", msg->uidl, msg->recv_time);
	}

	if (fclose(fp) == EOF) FILE_OP_ERROR(path, "fclose");
	g_free(path);

	return 0;
}

static gint pop3_write_msg_to_file(const gchar *file, const gchar *data,
				   guint len)
{
	FILE *fp;
	const gchar *prev, *cur;

	g_return_val_if_fail(file != NULL, -1);

	if ((fp = fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	if (change_file_mode_rw(fp, file) < 0)
		FILE_OP_ERROR(file, "chmod");

	/* +------------------+----------------+--------------------------+ *
	 * ^data              ^prev            ^cur             data+len-1^ */

	prev = data;
	while ((cur = (gchar *)my_memmem(prev, len - (prev - data), "\r\n", 2))
	       != NULL) {
		if ((cur > prev && fwrite(prev, cur - prev, 1, fp) < 1) ||
		    fputc('\n', fp) == EOF) {
			FILE_OP_ERROR(file, "fwrite");
			g_warning("can't write to file: %s\n", file);
			fclose(fp);
			unlink(file);
			return -1;
		}

		if (cur == data + len - 1) {
			prev = cur + 1;
			break;
		}

		if (*(cur + 1) == '\n')
			prev = cur + 2;
		else
			prev = cur + 1;

		if (prev - data < len - 1 && *prev == '.' && *(prev + 1) == '.')
			prev++;

		if (prev - data >= len)
			break;
	}

	if (prev - data < len &&
	    fwrite(prev, len - (prev - data), 1, fp) < 1) {
		FILE_OP_ERROR(file, "fwrite");
		g_warning("can't write to file: %s\n", file);
		fclose(fp);
		unlink(file);
		return -1;
	}
	if (data[len - 1] != '\r' && data[len - 1] != '\n') {
		if (fputc('\n', fp) == EOF) {
			FILE_OP_ERROR(file, "fputc");
			g_warning("can't write to file: %s\n", file);
			fclose(fp);
			unlink(file);
			return -1;
		}
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		unlink(file);
		return -1;
	}

	return 0;
}

static Pop3State pop3_lookup_next(Pop3Session *session)
{
	Pop3MsgInfo *msg;
	PrefsAccount *ac = session->ac_prefs;
	gint size;
	gboolean size_limit_over;

	for (;;) {
		msg = &session->msg[session->cur_msg];
		size = msg->size;
		size_limit_over =
		    (ac->enable_size_limit &&
		     ac->size_limit > 0 &&
		     size > ac->size_limit * 1024);

		if (msg->recv_time == RECV_TIME_DELETE ||
		    (ac->rmmail &&
		     msg->recv_time != RECV_TIME_NONE &&
		     msg->recv_time != RECV_TIME_KEEP &&
		     session->current_time - msg->recv_time >=
		     ac->msg_leave_time * 24 * 60 * 60)) {
			log_print(_("POP3: Deleting expired message %d\n"),
				  session->cur_msg);
			pop3_delete_send(session);
			return POP3_DELETE;
		}

		if (size_limit_over)
			log_print
				(_("POP3: Skipping message %d (%d bytes)\n"),
				  session->cur_msg, size);

		if (size == 0 || msg->received || size_limit_over) {
			session->cur_total_bytes += size;
			if (session->cur_msg == session->count) {
				pop3_logout_send(session);
				return POP3_LOGOUT;
			} else
				session->cur_msg++;
		} else
			break;
	}

	pop3_retr_send(session);
	return POP3_RETR;
}

static Pop3ErrorValue pop3_ok(Pop3Session *session, const gchar *msg)
{
	Pop3ErrorValue ok;

	log_print("POP3< %s\n", msg);

	if (!strncmp(msg, "+OK", 3))
		ok = PS_SUCCESS;
	else if (!strncmp(msg, "-ERR", 4)) {
		if (strstr(msg + 4, "lock") ||
		    strstr(msg + 4, "Lock") ||
		    strstr(msg + 4, "LOCK") ||
		    strstr(msg + 4, "wait")) {
			log_warning(_("mailbox is locked\n"));
			ok = PS_LOCKBUSY;
		} else if (strcasestr(msg + 4, "timeout")) {
			log_warning(_("session timeout\n"));
			ok = PS_ERROR;
		} else {
			switch (session->state) {
#if USE_SSL
			case POP3_STLS:
				log_warning(_("can't start TLS session\n"));
				ok = PS_ERROR;
				break;
#endif
			case POP3_GETAUTH_USER:
			case POP3_GETAUTH_PASS:
			case POP3_GETAUTH_APOP:
				log_warning(_("error occurred on authentication\n"));
				ok = PS_AUTHFAIL;
				break;
			case POP3_GETRANGE_LAST:
			case POP3_GETRANGE_UIDL:
				log_warning(_("command not supported\n"));
				ok = PS_NOTSUPPORTED;
				break;
			default:
				log_warning(_("error occurred on POP3 session\n"));
				ok = PS_ERROR;
			}
		}

		g_free(session->error_msg);
		session->error_msg = g_strdup(msg);
		fprintf(stderr, "POP3: %s\n", msg);
	} else
		ok = PS_PROTOCOL;

	session->error_val = ok;
	return ok;
}

static gint pop3_session_recv_msg(Session *session, const gchar *msg)
{
	Pop3Session *pop3_session = POP3_SESSION(session);
	Pop3ErrorValue val = PS_SUCCESS;
	const gchar *body;

	body = msg;
	if (pop3_session->state != POP3_GETRANGE_UIDL_RECV &&
	    pop3_session->state != POP3_GETSIZE_LIST_RECV) {
		val = pop3_ok(pop3_session, msg);
		if (val != PS_SUCCESS) {
			if (val != PS_NOTSUPPORTED) {
				pop3_session->state = POP3_ERROR;
				return -1;
			}
		}

		if (*body == '+' || *body == '-')
			body++;
		while (g_ascii_isalpha(*body))
			body++;
		while (g_ascii_isspace(*body))
			body++;
	}

	switch (pop3_session->state) {
	case POP3_READY:
	case POP3_GREETING:
		pop3_greeting_recv(pop3_session, body);
#if USE_SSL
		if (pop3_session->ac_prefs->ssl_pop == SSL_STARTTLS)
			pop3_stls_send(pop3_session);
		else
#endif
		if (pop3_session->ac_prefs->use_apop_auth)
			pop3_getauth_apop_send(pop3_session);
		else
			pop3_getauth_user_send(pop3_session);
		break;
#if USE_SSL
	case POP3_STLS:
		if (pop3_stls_recv(pop3_session) != PS_SUCCESS)
			return -1;
		if (pop3_session->ac_prefs->use_apop_auth)
			pop3_getauth_apop_send(pop3_session);
		else
			pop3_getauth_user_send(pop3_session);
		break;
#endif
	case POP3_GETAUTH_USER:
		pop3_getauth_pass_send(pop3_session);
		break;
	case POP3_GETAUTH_PASS:
	case POP3_GETAUTH_APOP:
		pop3_getrange_stat_send(pop3_session);
		break;
	case POP3_GETRANGE_STAT:
		if (pop3_getrange_stat_recv(pop3_session, body) < 0)
			return -1;
		if (pop3_session->count > 0)
			pop3_getrange_uidl_send(pop3_session);
		else
			pop3_logout_send(pop3_session);
		break;
	case POP3_GETRANGE_LAST:
		if (val == PS_NOTSUPPORTED)
			pop3_session->error_val = PS_SUCCESS;
		else if (pop3_getrange_last_recv(pop3_session, body) < 0)
			return -1;
		if (pop3_session->cur_msg > 0)
			pop3_getsize_list_send(pop3_session);
		else
			pop3_logout_send(pop3_session);
		break;
	case POP3_GETRANGE_UIDL:
		if (val == PS_NOTSUPPORTED) {
			pop3_session->error_val = PS_SUCCESS;
			pop3_getrange_last_send(pop3_session);
		} else {
			pop3_session->state = POP3_GETRANGE_UIDL_RECV;
			session_recv_data(session, 0, ".\r\n");
		}
		break;
	case POP3_GETSIZE_LIST:
		pop3_session->state = POP3_GETSIZE_LIST_RECV;
		session_recv_data(session, 0, ".\r\n");
		break;
	case POP3_RETR:
		pop3_session->state = POP3_RETR_RECV;
		session_recv_data(session, 0, ".\r\n");
		break;
	case POP3_DELETE:
		pop3_delete_recv(pop3_session);
		if (pop3_session->cur_msg == pop3_session->count)
			pop3_logout_send(pop3_session);
		else {
			pop3_session->cur_msg++;
			if (pop3_lookup_next(pop3_session) == POP3_ERROR)
				return -1;
		}
		break;
	case POP3_LOGOUT:
		session_disconnect(session);
		break;
	case POP3_ERROR:
	default:
		return -1;
	}

	return 0;
}

static gint pop3_session_recv_data_finished(Session *session, guchar *data,
					    guint len)
{
	Pop3Session *pop3_session = POP3_SESSION(session);
	Pop3ErrorValue val = PS_SUCCESS;

	switch (pop3_session->state) {
	case POP3_GETRANGE_UIDL_RECV:
		val = pop3_getrange_uidl_recv(pop3_session, data, len);
		if (val == PS_SUCCESS) {
			if (pop3_session->new_msg_exist)
				pop3_getsize_list_send(pop3_session);
			else
				pop3_logout_send(pop3_session);
		} else
			return -1;
		break;
	case POP3_GETSIZE_LIST_RECV:
		val = pop3_getsize_list_recv(pop3_session, data, len);
		if (val == PS_SUCCESS) {
			if (pop3_lookup_next(pop3_session) == POP3_ERROR)
				return -1;
		} else
			return -1;
		break;
	case POP3_RETR_RECV:
		if (pop3_retr_recv(pop3_session, data, len) < 0)
			return -1;

		if (pop3_session->msg[pop3_session->cur_msg].recv_time
		    == RECV_TIME_DELETE ||
		    (pop3_session->ac_prefs->rmmail &&
		     pop3_session->ac_prefs->msg_leave_time == 0 &&
		     pop3_session->msg[pop3_session->cur_msg].recv_time
		     != RECV_TIME_KEEP))
			pop3_delete_send(pop3_session);
		else if (pop3_session->cur_msg == pop3_session->count)
			pop3_logout_send(pop3_session);
		else {
			pop3_session->cur_msg++;
			if (pop3_lookup_next(pop3_session) == POP3_ERROR)
				return -1;
		}
		break;
	case POP3_ERROR:
	default:
		return -1;
	}

	return 0;
}
