/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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

#ifndef __NNTP_H__
#define __NNTP_H__

#include "session.h"
#if USE_SSL
#  include "ssl.h"
#endif

typedef struct _NNTPSession	NNTPSession;

#define NNTP_SESSION(obj)       ((NNTPSession *)obj)

struct _NNTPSession
{
	Session session;

	gchar *group;

	gchar *userid;
	gchar *passwd;
	gboolean auth_failed;
};

#define NN_SUCCESS	0
#define NN_SOCKET	2
#define NN_AUTHFAIL	3
#define NN_PROTOCOL	4
#define NN_SYNTAX	5
#define NN_IOERR	6
#define NN_ERROR	7
#define NN_AUTHREQ	8
#define NN_AUTHCONT	9

#define NNTPBUFSIZE	8192

#if USE_SSL
Session *nntp_session_new	(const gchar	*server,
				 gushort	 port,
				 gchar		*buf,
				 const gchar	*userid,
				 const gchar	*passwd,
				 SSLType	 ssl_type);
#else
Session *nntp_session_new	(const gchar	*server,
				 gushort	 port,
				 gchar		*buf,
				 const gchar	*userid,
				 const gchar	*passwd);
#endif

gint nntp_group			(NNTPSession	*session,
				 const gchar	*group,
				 gint		*num,
				 gint		*first,
				 gint		*last);
gint nntp_get_article		(NNTPSession	*session,
				 const gchar	*cmd,
				 gint		 num,
				 gchar	       **msgid);
gint nntp_article		(NNTPSession	*session,
				 gint		 num,
				 gchar	       **msgid);
gint nntp_body			(NNTPSession	*session,
				 gint		 num,
				 gchar	       **msgid);
gint nntp_head			(NNTPSession	*session,
				 gint		 num,
				 gchar	       **msgid);
gint nntp_stat			(NNTPSession	*session,
				 gint		 num,
				 gchar	       **msgid);
gint nntp_next			(NNTPSession	*session,
				 gint		*num,
				 gchar	       **msgid);
gint nntp_xover			(NNTPSession	*session,
				 gint		 first,
				 gint		 last);
gint nntp_xhdr			(NNTPSession	*session,
				 const gchar	*header,
				 gint		 first,
				 gint		 last);
gint nntp_list			(NNTPSession	*session);
gint nntp_post			(NNTPSession	*session,
				 FILE		*fp);
gint nntp_newgroups		(NNTPSession	*session);
gint nntp_newnews		(NNTPSession	*session);
gint nntp_mode			(NNTPSession	*sessio,
				 gboolean	 stream);

#endif /* __NNTP_H__ */
