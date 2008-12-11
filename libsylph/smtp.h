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

#ifndef __SMTP_H__
#define __SMTP_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdio.h>

#include "session.h"

typedef struct _SMTPSession	SMTPSession;

#define SMTP_SESSION(obj)	((SMTPSession *)obj)

#define SMTPBUFSIZE		8192

typedef enum
{
	SM_OK			= 0,
	SM_ERROR		= 128,
	SM_UNRECOVERABLE	= 129,
	SM_AUTHFAIL		= 130
} SMTPErrorValue;

typedef enum
{
	ESMTP_8BITMIME	= 1 << 0,
	ESMTP_SIZE	= 1 << 1,
	ESMTP_ETRN	= 1 << 2
} ESMTPFlag;

typedef enum
{
	SMTPAUTH_LOGIN      = 1 << 0,
	SMTPAUTH_CRAM_MD5   = 1 << 1,
	SMTPAUTH_DIGEST_MD5 = 1 << 2,
	SMTPAUTH_PLAIN      = 1 << 3
} SMTPAuthType;

typedef enum
{
	SMTP_READY,
	SMTP_CONNECTED,
	SMTP_HELO,
	SMTP_EHLO,
	SMTP_STARTTLS,
	SMTP_FROM,
	SMTP_AUTH,
	SMTP_AUTH_PLAIN,
	SMTP_AUTH_LOGIN_USER,
	SMTP_AUTH_LOGIN_PASS,
	SMTP_AUTH_CRAM_MD5,
	SMTP_RCPT,
	SMTP_DATA,
	SMTP_SEND_DATA,
	SMTP_EOM,
	SMTP_RSET,
	SMTP_QUIT,
	SMTP_ERROR,
	SMTP_DISCONNECTED,

	N_SMTP_PHASE
} SMTPState;

struct _SMTPSession
{
	Session session;

	SMTPState state;

	gboolean tls_init_done;

	gchar *hostname;

	gchar *user;
	gchar *pass;

	gchar *from;
	GSList *to_list;
	GSList *cur_to;

	FILE *send_data_fp;
	gint send_data_len;

	SMTPAuthType avail_auth_type;
	SMTPAuthType forced_auth_type;
	SMTPAuthType auth_type;

	SMTPErrorValue error_val;
	gchar *error_msg;
};

Session *smtp_session_new	(void);

#endif /* __SMTP_H__ */
