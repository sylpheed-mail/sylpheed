/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2011 Hiroyuki Yamamoto
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

#ifndef __IMAP_H__
#define __IMAP_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <time.h>

#include "folder.h"
#include "session.h"
#include "procmsg.h"

typedef struct _IMAPFolder	IMAPFolder;
typedef struct _IMAPSession	IMAPSession;
typedef struct _IMAPNameSpace	IMAPNameSpace;

#define IMAP_FOLDER(obj)	((IMAPFolder *)obj)
#define IMAP_SESSION(obj)	((IMAPSession *)obj)

#include "prefs_account.h"

typedef enum
{
	IMAP_AUTH_LOGIN		= 1 << 0,
	IMAP_AUTH_CRAM_MD5	= 1 << 1,
	IMAP_AUTH_PLAIN		= 1 << 2
} IMAPAuthType;

struct _IMAPFolder
{
	RemoteFolder rfolder;

	/* list of IMAPNameSpace */
	GList *ns_personal;
	GList *ns_others;
	GList *ns_shared;
};

struct _IMAPSession
{
	Session session;

	gboolean authenticated;

	gchar **capability;
	gboolean uidplus;

	gchar *mbox;
	guint cmd_count;
};

struct _IMAPNameSpace
{
	gchar *name;
	gchar separator;
};

#define IMAP_SUCCESS	0
#define IMAP_SOCKET	2
#define IMAP_AUTHFAIL	3
#define IMAP_PROTOCOL	4
#define IMAP_SYNTAX	5
#define IMAP_IOERR	6
#define IMAP_ERROR	7
#define IMAP_EAGAIN	8

#define IMAPBUFSIZE	8192

typedef enum
{
	IMAP_FLAG_SEEN		= 1 << 0,
	IMAP_FLAG_ANSWERED	= 1 << 1,
	IMAP_FLAG_FLAGGED	= 1 << 2,
	IMAP_FLAG_DELETED	= 1 << 3,
	IMAP_FLAG_DRAFT		= 1 << 4,

	/* color label keywords : 1 << 7 ... 1 << 9
	   compatible with procmsg.h: MSG_CLABEL* macros */
} IMAPFlags;

#define IMAP_IS_SEEN(flags)	((flags & IMAP_FLAG_SEEN) != 0)
#define IMAP_IS_ANSWERED(flags)	((flags & IMAP_FLAG_ANSWERED) != 0)
#define IMAP_IS_FLAGGED(flags)	((flags & IMAP_FLAG_FLAGGED) != 0)
#define IMAP_IS_DELETED(flags)	((flags & IMAP_FLAG_DELETED) != 0)
#define IMAP_IS_DRAFT(flags)	((flags & IMAP_FLAG_DRAFT) != 0)

#define IMAP_GET_COLORLABEL(flags)	(flags & (7 << MSG_CLABEL_SBIT))
#define IMAP_GET_COLORLABEL_VALUE(flags) \
	(IMAP_GET_COLORLABEL(flags) >> MSG_CLABEL_SBIT)
#define IMAP_SET_COLORLABEL_VALUE(flags, v) \
	((flags) |= ((v & 7) << MSG_CLABEL_SBIT))

FolderClass *imap_get_class		(void);

gint imap_msg_set_perm_flags		(MsgInfo	*msginfo,
					 MsgPermFlags	 flags);
gint imap_msg_unset_perm_flags		(MsgInfo	*msginfo,
					 MsgPermFlags	 flags);
gint imap_msg_list_set_perm_flags	(GSList		*msglist,
					 MsgPermFlags	 flags);
gint imap_msg_list_unset_perm_flags	(GSList		*msglist,
					 MsgPermFlags	 flags);

gint imap_msg_list_set_colorlabel_flags	(GSList		*msglist,
					 guint		 color);

gboolean imap_is_session_active		(IMAPFolder	*folder);

#endif /* __IMAP_H__ */
