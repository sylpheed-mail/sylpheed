/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2008 Hiroyuki Yamamoto
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
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>

#include "smtp.h"
#include "md5_hmac.h"
#include "base64.h"
#include "utils.h"

static void smtp_session_destroy(Session *session);

static gint smtp_from(SMTPSession *session);

static gint smtp_auth(SMTPSession *session);
static gint smtp_starttls(SMTPSession *session);
static gint smtp_auth_cram_md5(SMTPSession *session);
static gint smtp_auth_plain(SMTPSession *session);
static gint smtp_auth_login(SMTPSession *session);

static gint smtp_ehlo(SMTPSession *session);
static gint smtp_ehlo_recv(SMTPSession *session, const gchar *msg);

static gint smtp_helo(SMTPSession *session);
static gint smtp_rcpt(SMTPSession *session);
static gint smtp_data(SMTPSession *session);
static gint smtp_send_data(SMTPSession *session);
/* static gint smtp_rset(SMTPSession *session); */
static gint smtp_quit(SMTPSession *session);
static gint smtp_eom(SMTPSession *session);

static gint smtp_session_recv_msg(Session *session, const gchar *msg);
static gint smtp_session_send_data_finished(Session *session, guint len);


Session *smtp_session_new(void)
{
	SMTPSession *session;

	session = g_new0(SMTPSession, 1);

	session_init(SESSION(session));

	SESSION(session)->type             = SESSION_SMTP;

	SESSION(session)->recv_msg         = smtp_session_recv_msg;

	SESSION(session)->recv_data_finished = NULL;
	SESSION(session)->send_data_finished = smtp_session_send_data_finished;

	SESSION(session)->destroy          = smtp_session_destroy;

	session->state                     = SMTP_READY;

#if USE_SSL
	session->tls_init_done             = FALSE;
#endif

	session->hostname                  = NULL;
	session->user                      = NULL;
	session->pass                      = NULL;

	session->from                      = NULL;
	session->to_list                   = NULL;
	session->cur_to                    = NULL;

	session->send_data_fp              = NULL;
	session->send_data_len             = 0;

	session->avail_auth_type           = 0;
	session->forced_auth_type          = 0;
	session->auth_type                 = 0;

	session->error_val                 = SM_OK;
	session->error_msg                 = NULL;

	return SESSION(session);
}

static void smtp_session_destroy(Session *session)
{
	SMTPSession *smtp_session = SMTP_SESSION(session);

	g_free(smtp_session->hostname);
	g_free(smtp_session->user);
	g_free(smtp_session->pass);
	g_free(smtp_session->from);

	if (smtp_session->send_data_fp)
		fclose(smtp_session->send_data_fp);

	g_free(smtp_session->error_msg);
}

static gint smtp_from(SMTPSession *session)
{
	gchar buf[SMTPBUFSIZE];

	g_return_val_if_fail(session->from != NULL, SM_ERROR);

	session->state = SMTP_FROM;

	if (strchr(session->from, '<'))
		g_snprintf(buf, sizeof(buf), "MAIL FROM:%s", session->from);
	else
		g_snprintf(buf, sizeof(buf), "MAIL FROM:<%s>", session->from);

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, buf);
	log_print("SMTP> %s\n", buf);

	return SM_OK;
}

static gint smtp_auth(SMTPSession *session)
{

	g_return_val_if_fail(session->user != NULL, SM_ERROR);

	session->state = SMTP_AUTH;

	if (session->forced_auth_type == SMTPAUTH_CRAM_MD5 ||
	    (session->forced_auth_type == 0 &&
	     (session->avail_auth_type & SMTPAUTH_CRAM_MD5) != 0))
		smtp_auth_cram_md5(session);
	else if (session->forced_auth_type == SMTPAUTH_PLAIN ||
		 (session->forced_auth_type == 0 &&
		  (session->avail_auth_type & SMTPAUTH_PLAIN) != 0))
		smtp_auth_plain(session);
	else if (session->forced_auth_type == SMTPAUTH_LOGIN ||
		 (session->forced_auth_type == 0 &&
		  (session->avail_auth_type & SMTPAUTH_LOGIN) != 0))
		smtp_auth_login(session);
	else {
		log_warning(_("SMTP AUTH not available\n"));
		return SM_AUTHFAIL;
	}

	return SM_OK;
}

static gint smtp_auth_recv(SMTPSession *session, const gchar *msg)
{
	gchar buf[SMTPBUFSIZE];

	switch (session->auth_type) {
	case SMTPAUTH_LOGIN:
		session->state = SMTP_AUTH_LOGIN_USER;

		if (!strncmp(msg, "334 ", 4)) {
			base64_encode(buf, (guchar *)session->user,
				      strlen(session->user));

			session_send_msg(SESSION(session), SESSION_MSG_NORMAL,
					 buf);
			log_print("ESMTP> [USERID]\n");
		} else {
			/* Server rejects AUTH */
			session_send_msg(SESSION(session), SESSION_MSG_NORMAL,
					 "*");
			log_print("ESMTP> *\n");
		}
		break;
	case SMTPAUTH_CRAM_MD5:
		session->state = SMTP_AUTH_CRAM_MD5;

		if (!strncmp(msg, "334 ", 4)) {
			gchar *response;
			gchar *response64;
			guchar *challenge;
			gint challengelen;
			gchar hexdigest[33];

			challenge = g_malloc(strlen(msg + 4) + 1);
			challengelen = base64_decode(challenge, msg + 4, -1);
			challenge[challengelen] = '\0';
			log_print("ESMTP< [Decoded: %s]\n", challenge);

			g_snprintf(buf, sizeof(buf), "%s", session->pass);
			md5_hex_hmac(hexdigest, challenge, challengelen,
				     (guchar *)buf, strlen(buf));
			g_free(challenge);

			response = g_strdup_printf
				("%s %s", session->user, hexdigest);
			log_print("ESMTP> [Encoded: %s]\n", response);

			response64 = g_malloc((strlen(response) + 3) * 2 + 1);
			base64_encode(response64, (guchar *)response,
				      strlen(response));
			g_free(response);

			session_send_msg(SESSION(session), SESSION_MSG_NORMAL,
					 response64);
			log_print("ESMTP> %s\n", response64);
			g_free(response64);
		} else {
			/* Server rejects AUTH */
			session_send_msg(SESSION(session), SESSION_MSG_NORMAL,
					 "*");
			log_print("ESMTP> *\n");
		}
		break;
	case SMTPAUTH_DIGEST_MD5:
        default:
        	/* stop smtp_auth when no correct authtype */
		session_send_msg(SESSION(session), SESSION_MSG_NORMAL, "*");
		log_print("ESMTP> *\n");
		break;
	}

	return SM_OK;
}

static gint smtp_auth_login_user_recv(SMTPSession *session, const gchar *msg)
{
	gchar buf[SMTPBUFSIZE];

	session->state = SMTP_AUTH_LOGIN_PASS;

	if (!strncmp(msg, "334 ", 4))
		base64_encode(buf, (guchar *)session->pass,
			      strlen(session->pass));
	else
		/* Server rejects AUTH */
		g_snprintf(buf, sizeof(buf), "*");

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, buf);
	log_print("ESMTP> [PASSWORD]\n");

	return SM_OK;
}

static gint smtp_ehlo(SMTPSession *session)
{
	gchar buf[SMTPBUFSIZE];

	session->state = SMTP_EHLO;

	session->avail_auth_type = 0;

	g_snprintf(buf, sizeof(buf), "EHLO %s",
		   session->hostname ? session->hostname : get_domain_name());
	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, buf);
	log_print("ESMTP> %s\n", buf);

	return SM_OK;
}

static gint smtp_ehlo_recv(SMTPSession *session, const gchar *msg)
{
	if (strncmp(msg, "250", 3) == 0) {
		const gchar *p = msg;
		p += 3;
		if (*p == '-' || *p == ' ') p++;
		if (g_ascii_strncasecmp(p, "AUTH", 4) == 0 && p[4] != '\0') {
			p += 5;
			if (strcasestr(p, "PLAIN"))
				session->avail_auth_type |= SMTPAUTH_PLAIN;
			if (strcasestr(p, "LOGIN"))
				session->avail_auth_type |= SMTPAUTH_LOGIN;
			if (strcasestr(p, "CRAM-MD5"))
				session->avail_auth_type |= SMTPAUTH_CRAM_MD5;
			if (strcasestr(p, "DIGEST-MD5"))
				session->avail_auth_type |= SMTPAUTH_DIGEST_MD5;
		}
		return SM_OK;
	} else if ((msg[0] == '1' || msg[0] == '2' || msg[0] == '3') &&
	    (msg[3] == ' ' || msg[3] == '\0'))
		return SM_OK;
	else if (msg[0] == '5' && msg[1] == '0' &&
		 (msg[2] == '4' || msg[2] == '3' || msg[2] == '1'))
		return SM_ERROR;

	return SM_ERROR;
}

static gint smtp_starttls(SMTPSession *session)
{
	session->state = SMTP_STARTTLS;

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, "STARTTLS");
	log_print("ESMTP> STARTTLS\n");

	return SM_OK;
}

static gint smtp_auth_cram_md5(SMTPSession *session)
{
	session->state = SMTP_AUTH;
	session->auth_type = SMTPAUTH_CRAM_MD5;

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, "AUTH CRAM-MD5");
	log_print("ESMTP> AUTH CRAM-MD5\n");

	return SM_OK;
}

static gint smtp_auth_plain(SMTPSession *session)
{
	gchar *authstr;
	gint authlen;
	gchar *outbuf;
	gchar *p;

	session->state = SMTP_AUTH_PLAIN;
	session->auth_type = SMTPAUTH_PLAIN;

	/*
	 * construct the string: \0<user>\0<pass>
	 */

	authlen = 1 + strlen(session->user) + 1 + strlen(session->pass);
	authstr = g_malloc(authlen + 1);

	p = authstr;

	*p++ = '\0';
	strcpy(p, session->user);
	p += strlen(p) + 1;
	strcpy(p, session->pass);

	outbuf = g_malloc(sizeof("AUTH PLAIN ") + authlen * 2 + 1);

	strcpy(outbuf, "AUTH PLAIN ");
	p = outbuf + strlen(outbuf);
	base64_encode(p, (guchar *)authstr, authlen);

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, outbuf);
	log_print("ESMTP> AUTH PLAIN ********\n");

	g_free(outbuf);
	g_free(authstr);

	return SM_OK;
}

static gint smtp_auth_login(SMTPSession *session)
{
	session->state = SMTP_AUTH;
	session->auth_type = SMTPAUTH_LOGIN;

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, "AUTH LOGIN");
	log_print("ESMTP> AUTH LOGIN\n");

	return SM_OK;
}

static gint smtp_helo(SMTPSession *session)
{
	gchar buf[SMTPBUFSIZE];

	session->state = SMTP_HELO;

	g_snprintf(buf, sizeof(buf), "HELO %s",
		   session->hostname ? session->hostname : get_domain_name());
	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, buf);
	log_print("SMTP> %s\n", buf);

	return SM_OK;
}

static gint smtp_rcpt(SMTPSession *session)
{
	gchar buf[SMTPBUFSIZE];
	gchar *to;

	g_return_val_if_fail(session->cur_to != NULL, SM_ERROR);

	session->state = SMTP_RCPT;

	to = (gchar *)session->cur_to->data;

	if (strchr(to, '<'))
		g_snprintf(buf, sizeof(buf), "RCPT TO:%s", to);
	else
		g_snprintf(buf, sizeof(buf), "RCPT TO:<%s>", to);
	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, buf);
	log_print("SMTP> %s\n", buf);

	session->cur_to = session->cur_to->next;

	return SM_OK;
}

static gint smtp_data(SMTPSession *session)
{
	session->state = SMTP_DATA;

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, "DATA");
	log_print("SMTP> DATA\n");

	return SM_OK;
}

static gint smtp_send_data(SMTPSession *session)
{
	session->state = SMTP_SEND_DATA;

	session_send_data(SESSION(session), session->send_data_fp,
			  session->send_data_len);

	return SM_OK;
}

#if 0
static gint smtp_rset(SMTPSession *session)
{
	session->state = SMTP_RSET;

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, "RSET");
	log_print("SMTP> RSET\n");

	return SM_OK;
}
#endif

static gint smtp_quit(SMTPSession *session)
{
	session->state = SMTP_QUIT;

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, "QUIT");
	log_print("SMTP> QUIT\n");

	return SM_OK;
}

static gint smtp_eom(SMTPSession *session)
{
	session->state = SMTP_EOM;

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, ".");
	log_print("SMTP> . (EOM)\n");

	return SM_OK;
}

static gint smtp_session_recv_msg(Session *session, const gchar *msg)
{
	SMTPSession *smtp_session = SMTP_SESSION(session);
	gboolean cont = FALSE;

	if (strlen(msg) < 4) {
		log_warning(_("bad SMTP response\n"));
		return -1;
	}

	switch (smtp_session->state) {
	case SMTP_EHLO:
	case SMTP_STARTTLS:
	case SMTP_AUTH:
	case SMTP_AUTH_PLAIN:
	case SMTP_AUTH_LOGIN_USER:
	case SMTP_AUTH_LOGIN_PASS:
	case SMTP_AUTH_CRAM_MD5:
		log_print("ESMTP< %s\n", msg);
		break;
	default:
		log_print("SMTP< %s\n", msg);
		break;
	}

	if (msg[0] == '5' && msg[1] == '0' &&
	    (msg[2] == '4' || msg[2] == '3' || msg[2] == '1')) {
		log_warning(_("error occurred on SMTP session\n"));
		smtp_session->state = SMTP_ERROR;
		smtp_session->error_val = SM_ERROR;
		g_free(smtp_session->error_msg);
		smtp_session->error_msg = g_strdup(msg);
		return -1;
	}

	if (!strncmp(msg, "535", 3)) {
		log_warning(_("error occurred on authentication\n"));
		smtp_session->state = SMTP_ERROR;
		smtp_session->error_val = SM_AUTHFAIL;
		g_free(smtp_session->error_msg);
		smtp_session->error_msg = g_strdup(msg);
		return -1;
	}

	if (msg[0] != '1' && msg[0] != '2' && msg[0] != '3') {
		log_warning(_("error occurred on SMTP session\n"));
		smtp_session->state = SMTP_ERROR;
		smtp_session->error_val = SM_ERROR;
		g_free(smtp_session->error_msg);
		smtp_session->error_msg = g_strdup(msg);
		return -1;
	}

	if (msg[3] == '-')
		cont = TRUE;
	else if (msg[3] != ' ' && msg[3] != '\0') {
		log_warning(_("bad SMTP response\n"));
		smtp_session->state = SMTP_ERROR;
		smtp_session->error_val = SM_UNRECOVERABLE;
		return -1;
	}

	/* ignore all multiline responses except for EHLO */
	if (cont && smtp_session->state != SMTP_EHLO)
		return session_recv_msg(session);

	switch (smtp_session->state) {
	case SMTP_READY:
	case SMTP_CONNECTED:
#if USE_SSL
		if (smtp_session->user || session->ssl_type != SSL_NONE)
#else
		if (smtp_session->user)
#endif
			smtp_ehlo(smtp_session);
		else
			smtp_helo(smtp_session);
		break;
	case SMTP_HELO:
		smtp_from(smtp_session);
		break;
	case SMTP_EHLO:
		smtp_ehlo_recv(smtp_session, msg);
		if (cont == TRUE)
			break;
#if USE_SSL
		if (session->ssl_type == SSL_STARTTLS &&
		    smtp_session->tls_init_done == FALSE) {
			smtp_starttls(smtp_session);
			break;
		}
#endif
		if (smtp_session->user) {
			if (smtp_auth(smtp_session) != SM_OK)
				smtp_from(smtp_session);
		} else
			smtp_from(smtp_session);
		break;
	case SMTP_STARTTLS:
#if USE_SSL
		if (session_start_tls(session) < 0) {
			log_warning(_("can't start TLS session\n"));
			smtp_session->state = SMTP_ERROR;
			smtp_session->error_val = SM_ERROR;
			return -1;
		}
		smtp_session->tls_init_done = TRUE;
		smtp_ehlo(smtp_session);
#endif
		break;
	case SMTP_AUTH:
		smtp_auth_recv(smtp_session, msg);
		break;
	case SMTP_AUTH_LOGIN_USER:
		smtp_auth_login_user_recv(smtp_session, msg);
		break;
	case SMTP_AUTH_PLAIN:
	case SMTP_AUTH_LOGIN_PASS:
	case SMTP_AUTH_CRAM_MD5:
		smtp_from(smtp_session);
		break;
	case SMTP_FROM:
		if (smtp_session->cur_to)
			smtp_rcpt(smtp_session);
		break;
	case SMTP_RCPT:
		if (smtp_session->cur_to)
			smtp_rcpt(smtp_session);
		else
			smtp_data(smtp_session);
		break;
	case SMTP_DATA:
		smtp_send_data(smtp_session);
		break;
	case SMTP_EOM:
		smtp_quit(smtp_session);
		break;
	case SMTP_QUIT:
		session_disconnect(session);
		break;
	case SMTP_ERROR:
	default:
		log_warning(_("error occurred on SMTP session\n"));
		smtp_session->error_val = SM_ERROR;
		return -1;
	}

	if (cont)
		return session_recv_msg(session);

	return 0;
}

static gint smtp_session_send_data_finished(Session *session, guint len)
{
	smtp_eom(SMTP_SESSION(session));
	return 0;
}
