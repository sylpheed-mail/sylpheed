/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Werner Koch (dd9jn)
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

#ifndef __RFC2015_H__
#define __RFC2015_H__

#include <glib.h>
#include <stdio.h>

#include "procmsg.h"
#include "procmime.h"

void rfc2015_disable_all		(void);
gboolean rfc2015_is_available		(void);

void rfc2015_secure_remove		(const gchar	*fname);

MimeInfo **rfc2015_find_signature	(MimeInfo	*mimeinfo);
gboolean rfc2015_has_signature		(MimeInfo	*mimeinfo);
void rfc2015_check_signature		(MimeInfo	*mimeinfo,
					 FILE		*fp);
gboolean rfc2015_is_pgp_signature_part	(MimeInfo	*mimeinfo);
gboolean rfc2015_is_pkcs7_signature_part(MimeInfo	*mimeinfo);
gboolean rfc2015_is_signature_part	(MimeInfo	*mimeinfo);

gint rfc2015_is_encrypted		(MimeInfo	*mimeinfo);
gboolean rfc2015_msg_is_encrypted	(const gchar	*file);
void rfc2015_decrypt_message		(MsgInfo	*msginfo,
					 MimeInfo	*mimeinfo,
					 FILE		*fp);
FILE *rfc2015_open_message_decrypted	(MsgInfo	*msginfo,
					 MimeInfo      **mimeinfo);

GSList *rfc2015_create_signers_list	(const gchar	*keyid);
gint rfc2015_encrypt			(const gchar	*file,
					 GSList		*recp_list);
gint rfc2015_encrypt_armored		(const gchar	*file,
					 GSList		*recp_list);
gint rfc2015_sign			(const gchar	*file,
					 GSList		*key_list);
gint rfc2015_clearsign			(const gchar	*file,
					 GSList		*key_list);
gint rfc2015_encrypt_sign		(const gchar	*file,
					 GSList		*recp_list,
					 GSList		*key_list);
gint rfc2015_encrypt_sign_armored	(const gchar	*file,
					 GSList		*recp_list,
					 GSList		*key_list);

#endif /* __RFC2015_H__ */
