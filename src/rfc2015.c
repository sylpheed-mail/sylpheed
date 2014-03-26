/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Werner Koch (dd9jn)
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto
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

#if USE_GPGME

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <errno.h>

#include <gpgme.h>

#include "procmsg.h"
#include "procmime.h"
#include "procheader.h"
#include "base64.h"
#include "uuencode.h"
#include "unmime.h"
#include "codeconv.h"
#include "utils.h"
#include "prefs_common.h"
#include "passphrase.h"
#include "select-keys.h"
#include "sigstatus.h"
#include "rfc2015.h"

#define DIM(v)     (sizeof(v) / sizeof((v)[0]))

static gboolean gpg_available = TRUE;

static gchar *content_names[] = {
	"Content-Type",
	"Content-Disposition",
	"Content-Transfer-Encoding",
	NULL
};

static gchar *mime_version_name[] = {
	"Mime-Version",
	NULL
};

#if 0
static void dump_mimeinfo(const gchar *text, MimeInfo *x)
{
	debug_print("MimeInfo[%s] %p  level=%d\n", text, x, x ? x->level : 0);
	if (!x)
		return;

	debug_print("      enc=`%s' enc_type=%d mime_type=%d\n",
	            x->encoding, x->encoding_type, x->mime_type);
	debug_print("      cont_type=`%s' cs=`%s' name=`%s' bnd=`%s'\n",
	            x->content_type, x->charset, x->name, x->boundary );
	debug_print("      cont_disp=`%s' fname=`%s' fpos=%ld size=%u, lvl=%d\n",
	            x->content_disposition, x->filename, x->fpos, x->size,
	            x->level );
	dump_mimeinfo(".main", x->main );
	dump_mimeinfo(".sub", x->sub );
	dump_mimeinfo(".next", x->next );
	debug_print("MimeInfo[.parent] %p\n", x );
	dump_mimeinfo(".children", x->children );
	dump_mimeinfo(".plaintext", x->plaintext );
}

static void dump_part(MimeInfo *mimeinfo, FILE *fp)
{
	guint size = mimeinfo->size;
	gint c;

	if (fseek(fp, mimeinfo->fpos, SEEK_SET)) {
		debug_print("dump_part: fseek error\n");
		return;
	}

	debug_print("--- begin dump_part ----\n");
	while (size-- && (c = getc (fp)) != EOF)
		putc(c, stderr);
	if (ferror(fp))
		debug_print("dump_part: read error\n");
	debug_print("--- end dump_part ----\n");
}
#endif

void rfc2015_disable_all(void)
{
	gpg_available = FALSE;
}

gboolean rfc2015_is_available(void)
{
	return gpg_available;
}

void rfc2015_secure_remove(const gchar *fname)
{
	if (!fname)
		return;
	/* fixme: overwrite the file first */
	g_remove(fname);
}

static void sig_status_for_key(GString *str, gpgme_ctx_t ctx,
			       gpgme_signature_t sig)
{
	gpgme_key_t key;
	gpgme_user_id_t user;
	gpgme_error_t err;

	err = gpgme_get_key(ctx, sig->fpr, &key, 0);
	if (err || key == NULL || key->uids->uid == NULL) {
		if (err)
			debug_print("gpgme_get_key failed: %s\n",
				    gpgme_strerror(err));
		g_string_sprintfa(str, "%s\n",
		                  gpgmegtk_sig_status_to_string (sig, FALSE));
		if ((sig->fpr != NULL) && (*(sig->fpr) != '\0'))
			g_string_sprintfa
				(str, "Key fingerprint: %s\n", sig->fpr);
		g_string_append(str, _("Cannot find user ID for this key."));
		g_string_append(str, "\n");
		return;
	}
	user = key->uids;
	g_string_sprintfa
		(str, gpgmegtk_sig_status_to_string (sig, TRUE), user->uid);
	g_string_append(str, "\n");

	user = user->next;
	while (user) {
		g_string_sprintfa
			(str, _("		aka \"%s\"\n"), user->uid);
		user = user->next;
	}
}

static gchar *sig_status_full(gpgme_ctx_t ctx, gpgme_verify_result_t result)
{
	GString *str;
	gpgme_signature_t sig;
	time_t created;
	struct tm *ctime_val;
	gchar ctime_str[80];
	gchar *ctime_str_utf8;
	gchar *retval;

	g_return_val_if_fail(result != NULL, NULL);

	str = g_string_new("");

	sig = result->signatures;
	while (sig != NULL) {
		if (sig->timestamp != 0) {
			created = sig->timestamp;
			ctime_val = localtime(&created);
			my_strftime(ctime_str, sizeof (ctime_str), "%c",
			            ctime_val);
			ctime_str_utf8 = g_locale_to_utf8(ctime_str, -1,
							  NULL, NULL, NULL);
			if (!ctime_str_utf8)
				ctime_str_utf8 = g_strdup(ctime_str);
			g_string_sprintfa(str, _("Signature made at %s\n"),
			                  ctime_str_utf8);
			g_free(ctime_str_utf8);
		}
		sig_status_for_key(str, ctx, sig);
		if (sig->next)
			g_string_append(str, "\n\n");
		sig = sig->next;
	}

	retval = str->str;
	g_string_free(str, FALSE);
	return retval;
}

static void check_signature(MimeInfo *mimeinfo, MimeInfo *partinfo, FILE *fp)
{
	gpgme_ctx_t ctx = NULL;
	gpgme_error_t err;
	gpgme_data_t sig = NULL, text = NULL;
	gpgme_verify_result_t verifyresult = NULL;
	GpgmegtkSigStatus statuswindow = NULL;
	const gchar *result = NULL;
	gchar *tmp_file;
	gint n_exclude_chars = 0;

	if (prefs_common.gpg_signature_popup)
		statuswindow = gpgmegtk_sig_status_create();

	err = gpgme_new(&ctx);
	if (err) {
		debug_print("gpgme_new failed: %s\n", gpgme_strerror(err));
		goto leave;
	}

	if (rfc2015_is_pkcs7_signature_part(partinfo)) {
		debug_print("pkcs7 signature detected\n");
		gpgme_set_protocol(ctx, GPGME_PROTOCOL_CMS);
	}

	/* don't include the last empty line.
	   It does not belong to the signed text */
	if (mimeinfo->children->size > 0) {
		if (fseek(fp, mimeinfo->children->fpos + mimeinfo->children->size - 1,
		          SEEK_SET) < 0) {
			perror("fseek");
			goto leave;
		}
		if (fgetc(fp) == '\n') {
			n_exclude_chars++;
			if (mimeinfo->children->size > 1) {
				if (fseek(fp, mimeinfo->children->fpos + mimeinfo->children->size - 2,
				          SEEK_SET) < 0) {
					perror("fseek");
					goto leave;
				}
				if (fgetc(fp) == '\r')
					n_exclude_chars++;
			}
		}
	}

	/* canonicalize the file part. */
	tmp_file = get_tmp_file();
	if (copy_file_part(fp, mimeinfo->children->fpos,
	                   mimeinfo->children->size - n_exclude_chars,
	                   tmp_file) < 0) {
		g_free(tmp_file);
		goto leave;
	}
	if (canonicalize_file_replace(tmp_file) < 0) {
		g_unlink(tmp_file);
		g_free(tmp_file);
		goto leave;
	}

	err = gpgme_data_new_from_file(&text, tmp_file, 1);

	g_unlink(tmp_file);
	g_free(tmp_file);

	if (!err)
		err = gpgme_data_new_from_filepart(&sig, NULL, fp,
		                                   partinfo->fpos,
						   partinfo->size);
	if (err) {
		debug_print("gpgme_data_new_from_filepart failed: %s\n",
		            gpgme_strerror (err));
		goto leave;
	}

#if 0
	if (partinfo->encoding_type == ENC_BASE64) {
		err = gpgme_data_set_encoding(sig, GPGME_DATA_ENCODING_BASE64);
		if (err) {
			debug_print("gpgme_data_set_encoding failed: %s\n",
			            gpgme_strerror (err));
			goto leave;
		}
	}
#endif

	err = gpgme_op_verify(ctx, sig, text, NULL);
	if (err)  {
		debug_print("gpgme_op_verify failed: %s\n",
		            gpgme_strerror (err));
		goto leave;
	}
	verifyresult = gpgme_op_verify_result(ctx);

	/* FIXME: check what the heck this sig_status_full stuff is.
	 * Maybe it belongs in sigstatus.c 
	 *
	 * I think it belongs here as it is interfacing with gmime (Toshio). */
	g_free (partinfo->sigstatus_full);
	partinfo->sigstatus_full = sig_status_full(ctx, verifyresult);

leave:
	if (verifyresult) {
		result = gpgmegtk_sig_status_to_string
			(verifyresult->signatures, FALSE);
	} else {
		result = _("Error verifying the signature");
	}
	debug_print("verification status: %s\n", result);
	if (prefs_common.gpg_signature_popup)
		gpgmegtk_sig_status_update(statuswindow, ctx);

	g_free (partinfo->sigstatus);
	partinfo->sigstatus = g_strdup (result);

	gpgme_data_release(sig);
	gpgme_data_release(text);
	if (ctx)
		gpgme_release(ctx);
	if (prefs_common.gpg_signature_popup)
		gpgmegtk_sig_status_destroy(statuswindow);
}

/*
 * Copy a gpgme data object to a temporary file and
 * return this filename 
 */
#if 0
static gchar *copy_gpgmedata_to_temp(GpgmeData data, guint *length)
{
	static gint id;
	gchar *tmp;
	FILE *fp;
	gchar buf[100];
	size_t nread;
	GpgmeError err;

	tmp = g_strdup_printf("%s%cgpgtmp.%08x",
	                      get_mime_tmp_dir(), G_DIR_SEPARATOR, ++id);

	if ((fp = g_fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		g_free(tmp);
		return NULL;
	}

	err = gpgme_data_rewind(data);
	if (err)
		debug_print("gpgme_data_rewind failed: %s\n",
		            gpgme_strerror(err));

	while (!(err = gpgme_data_read(data, buf, 100, &nread))) {
		fwrite(buf, nread, 1, fp);
	}

	if (err != GPGME_EOF)
		debug_print("gpgme_data_read failed: %s\n",
		            gpgme_strerror(err));

	fclose (fp);
	*length = nread;

	return tmp;
}
#endif

static gpgme_data_t pgp_decrypt(MsgInfo *msginfo, MimeInfo *partinfo, FILE *fp)
{
	gpgme_ctx_t ctx = NULL;
	gpgme_error_t err;
	gpgme_data_t cipher = NULL, plain = NULL;
	struct passphrase_cb_info_s info;
	gpgme_verify_result_t verifyresult = NULL;
	const gchar *result = NULL;

	memset(&info, 0, sizeof info);

	err = gpgme_new(&ctx);
	if (err) {
		debug_print("gpgme_new failed: %s\n", gpgme_strerror(err));
		goto leave;
	}

	err = gpgme_data_new_from_filepart(&cipher, NULL, fp,
	                                   partinfo->fpos, partinfo->size);
	if (err) {
		debug_print("gpgme_data_new_from_filepart failed: %s\n",
		            gpgme_strerror(err));
		goto leave;
	}

	err = gpgme_data_new(&plain);
	if (err) {
		debug_print("gpgme_new failed: %s\n", gpgme_strerror(err));
		goto leave;
	}

	if (!g_getenv("GPG_AGENT_INFO")) {
		info.c = ctx;
		gpgme_set_passphrase_cb(ctx, gpgmegtk_passphrase_cb, &info);
	}

	err = gpgme_op_decrypt_verify(ctx, cipher, plain);

	msginfo->encinfo = g_new0(MsgEncryptInfo, 1);

	if (err) {
		gpgmegtk_free_passphrase();
		debug_print("decryption failed: %s\n", gpgme_strerror(err));
		gpgme_data_release(plain);
		plain = NULL;
		msginfo->encinfo->decryption_failed = TRUE;
		goto leave;
	}

	debug_print("** decryption succeeded\n");

	verifyresult = gpgme_op_verify_result(ctx);
	if (verifyresult && verifyresult->signatures) {
		result = gpgmegtk_sig_status_to_string(verifyresult->signatures,
						       FALSE);
		msginfo->encinfo->sigstatus = g_strdup(result);
		msginfo->encinfo->sigstatus_full =
			sig_status_full(ctx, verifyresult);
		debug_print("verification status: %s\n", result);
		debug_print("full status: %s\n",
			    msginfo->encinfo->sigstatus_full);
		if (prefs_common.gpg_signature_popup) {
			GpgmegtkSigStatus statuswindow;
			statuswindow = gpgmegtk_sig_status_create();
			gpgmegtk_sig_status_update(statuswindow, ctx);
			gpgmegtk_sig_status_destroy(statuswindow);
		}
	}


leave:
	gpgme_data_release(cipher);
	if (ctx)
		gpgme_release(ctx);
	return plain;
}

MimeInfo **rfc2015_find_signature(MimeInfo *mimeinfo)
{
	MimeInfo *partinfo;
	MimeInfo **signedinfo = NULL;
	gint n = 0;

	if (!mimeinfo)
		return NULL;

	/* We could have a signature nested within multipart/mixed so
	 * recurse to find it.
	 */
	if (!g_ascii_strcasecmp(mimeinfo->content_type, "multipart/mixed")) {
		for (partinfo = mimeinfo->children; partinfo != NULL;
		        partinfo = partinfo->next) {
			signedinfo = rfc2015_find_signature(partinfo);
			if (signedinfo) {
				return signedinfo;
			}
		}
		return NULL;
	}
	if (g_ascii_strcasecmp(mimeinfo->content_type, "multipart/signed"))
		return NULL;

	debug_print("** multipart/signed encountered\n");

	/* check that we have at least 2 parts of the correct type */
	for (partinfo = mimeinfo->children;
	        partinfo != NULL; partinfo = partinfo->next) {
		if (++n > 1  && rfc2015_is_signature_part(partinfo))
			break;
	}

	if (partinfo) {
		signedinfo = g_malloc(sizeof(MimeInfo *) * 2);
		signedinfo[0] = mimeinfo;
		signedinfo[1] = partinfo;
	}
	/* This is NULL if partinfo was not set */
	return signedinfo;
}

gboolean rfc2015_has_signature(MimeInfo *mimeinfo)
{
	return rfc2015_find_signature(mimeinfo) != NULL;
}

void rfc2015_check_signature(MimeInfo *mimeinfo, FILE *fp)
{
	MimeInfo **signedinfo;

	signedinfo = rfc2015_find_signature(mimeinfo);
	if (!signedinfo)
		return;

#if 0
	g_message("** yep, it is a pgp signature");
	dump_mimeinfo("gpg-signature", partinfo );
	dump_part(partinfo, fp );
	dump_mimeinfo("signed text", mimeinfo->children );
	dump_part(mimeinfo->children, fp);
#endif

	check_signature(signedinfo[0], signedinfo[1], fp);
	g_free(signedinfo);
}

gboolean rfc2015_is_pgp_signature_part(MimeInfo *mimeinfo)
{
	if (!mimeinfo || !mimeinfo->content_type)
		return FALSE;

	return !g_ascii_strcasecmp(mimeinfo->content_type,
				   "application/pgp-signature");
}

gboolean rfc2015_is_pkcs7_signature_part(MimeInfo *mimeinfo)
{
	const gchar *c_type;

	if (!mimeinfo || !mimeinfo->content_type)
		return FALSE;

	c_type = mimeinfo->content_type;

	return (!g_ascii_strcasecmp(c_type, "application/pkcs7-signature") ||
		!g_ascii_strcasecmp(c_type, "application/x-pkcs7-signature"));
}

gboolean rfc2015_is_signature_part(MimeInfo *mimeinfo)
{
	return (rfc2015_is_pgp_signature_part(mimeinfo) ||
		rfc2015_is_pkcs7_signature_part(mimeinfo));
}

gint rfc2015_is_encrypted(MimeInfo *mimeinfo)
{
	if (!mimeinfo || mimeinfo->mime_type != MIME_MULTIPART)
		return 0;
	if (g_ascii_strcasecmp(mimeinfo->content_type, "multipart/encrypted"))
		return 0;
	/* fixme: we should check the protocol parameter */
	return 1;
}

gboolean rfc2015_msg_is_encrypted(const gchar *file)
{
	FILE *fp;
	MimeInfo *mimeinfo;
	gint ret;

	if ((fp = g_fopen(file, "rb")) == NULL)
		return FALSE;

	mimeinfo = procmime_scan_mime_header(fp);
	if(!mimeinfo) {
		fclose(fp);
		return FALSE;
	}

	ret = rfc2015_is_encrypted(mimeinfo);
	procmime_mimeinfo_free_all(mimeinfo);
	fclose(fp);

	return ret != 0 ? TRUE : FALSE;
}

static gint name_cmp(const gchar *a, const gchar *b)
{
	for( ; *a && *b; a++, b++) {
		if (*a != *b &&
		    g_ascii_toupper(*(guchar *)a) != g_ascii_toupper(*(guchar *)b))
			return 1;
	}

	return *a != *b;
}

static gint headerp(gchar *p, gchar **names)
{
	gint i, c;
	gchar *p2;

	p2 = strchr(p, ':');
	if (!p2 || p == p2) {
		return 0;
	}
	if (p2[-1] == ' ' || p2[-1] == '\t') {
		return 0;
	}

	if (!names[0])
		return 1;

	c = *p2;
	*p2 = 0;
	for(i = 0 ; names[i] != NULL; i++) {
		if (!name_cmp(names[i], p))
			break;
	}
	*p2 = c;

	return names[i] != NULL;
}


#define DECRYPTION_ABORT() \
{ \
	procmime_mimeinfo_free_all(tmpinfo); \
	if (msginfo->encinfo) \
		msginfo->encinfo->decryption_failed = TRUE; \
	return; \
}

void rfc2015_decrypt_message(MsgInfo *msginfo, MimeInfo *mimeinfo, FILE *fp)
{
	static gint id;
	MimeInfo *tmpinfo, *partinfo;
	gint ver_ok = 0;
	gchar *fname;
	gpgme_data_t plain;
	FILE *dstfp;
	ssize_t nread;
	gchar buf[BUFFSIZE];
	gint in_cline;
	gpgme_error_t err;

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(fp != NULL);
	g_return_if_fail(mimeinfo->mime_type == MIME_MULTIPART);

	debug_print("** decrypting multipart/encrypted message\n");

	/* skip headers */
	if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0)
		perror("fseek");
	tmpinfo = procmime_scan_mime_header(fp);
	if (!tmpinfo || tmpinfo->mime_type != MIME_MULTIPART) {
		DECRYPTION_ABORT();
	}

	procmime_scan_multipart_message(tmpinfo, fp);

	/* check that we have the 2 parts */
	partinfo = tmpinfo->children;
	if (!partinfo || !partinfo->next) {
		DECRYPTION_ABORT();
	}
	if (!g_ascii_strcasecmp(partinfo->content_type,
	                        "application/pgp-encrypted")) {
		/* Fixme: check that the version is 1 */
		ver_ok = 1;
	}
	partinfo = partinfo->next;
	if (ver_ok &&
	    !g_ascii_strcasecmp(partinfo->content_type,
	                        "application/octet-stream")) {
		if (partinfo->next)
			g_warning("oops: pgp_encrypted with more than 2 parts");
	} else {
		DECRYPTION_ABORT();
	}

	debug_print("** yep, it is pgp encrypted\n");

	plain = pgp_decrypt(msginfo, partinfo, fp);
	if (!plain) {
		DECRYPTION_ABORT();
	}

	fname = g_strdup_printf("%s%cplaintext.%08x",
	                        get_mime_tmp_dir(), G_DIR_SEPARATOR, ++id);

	if ((dstfp = g_fopen(fname, "wb")) == NULL) {
		FILE_OP_ERROR(fname, "fopen");
		g_free(fname);
		DECRYPTION_ABORT();
	}

	/* write the orginal header to the new file */
	if (fseek(fp, tmpinfo->fpos, SEEK_SET) < 0)
		perror("fseek");

	in_cline = 0;
	while (fgets(buf, sizeof(buf), fp)) {
		if (headerp(buf, content_names)) {
			in_cline = 1;
			continue;
		}
		if (in_cline) {
			if (buf[0] == ' ' || buf[0] == '\t')
				continue;
			in_cline = 0;
		}
		if (buf[0] == '\r' || buf[0] == '\n')
			break;
		fputs(buf, dstfp);
	}

	err = (gpgme_data_seek(plain, 0, SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (err)
		debug_print("gpgme_data_seek failed: %s\n", gpgme_strerror(err));

	nread = gpgme_data_read(plain, buf, sizeof(buf));
	while (nread > 0) {
		fwrite (buf, nread, 1, dstfp);
		nread = gpgme_data_read(plain, buf, sizeof(buf));
	}

	if (nread != 0) {
		debug_print("gpgme_data_read failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
	}

	fclose(dstfp);
	procmime_mimeinfo_free_all(tmpinfo);

	msginfo->encinfo->plaintext_file = fname;
}

#undef DECRYPTION_ABORT

FILE *rfc2015_open_message_decrypted(MsgInfo *msginfo, MimeInfo **mimeinfo)
{
	FILE *fp;
	MimeInfo *mimeinfo_;
	glong fpos;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if (mimeinfo) *mimeinfo = NULL;

	if ((fp = procmsg_open_message(msginfo)) == NULL) return NULL;

	mimeinfo_ = procmime_scan_mime_header(fp);
	if (!mimeinfo_) {
		fclose(fp);
		return NULL;
	}

	if (!MSG_IS_ENCRYPTED(msginfo->flags) &&
	    rfc2015_is_encrypted(mimeinfo_)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_ENCRYPTED);
	}

	if (MSG_IS_ENCRYPTED(msginfo->flags) &&
	    (!msginfo->encinfo ||
	     (!msginfo->encinfo->plaintext_file &&
	      !msginfo->encinfo->decryption_failed))) {
		fpos = ftell(fp);
		rfc2015_decrypt_message(msginfo, mimeinfo_, fp);
		if (msginfo->encinfo &&
		    msginfo->encinfo->plaintext_file &&
		    !msginfo->encinfo->decryption_failed) {
			fclose(fp);
			procmime_mimeinfo_free_all(mimeinfo_);

			if ((fp = procmsg_open_message(msginfo)) == NULL)
				return NULL;
			mimeinfo_ = procmime_scan_mime_header(fp);
			if (!mimeinfo_) {
				fclose(fp);
				return NULL;
			}
		} else {
			if (fseek(fp, fpos, SEEK_SET) < 0)
				perror("fseek");
		}
	}

	if (mimeinfo) *mimeinfo = mimeinfo_;
	return fp;
}


/*
 * plain contains an entire mime object.
 * Encrypt it and return an GpgmeData object with the encrypted version of
 * the file or NULL in case of error.
 */
static gpgme_data_t pgp_encrypt(gpgme_data_t plain, gpgme_key_t kset[])
{
	gpgme_ctx_t ctx = NULL;
	gpgme_error_t err;
	gpgme_data_t cipher = NULL;

	err = gpgme_new(&ctx);
	if (!err)
		err = gpgme_data_new(&cipher);
	if (!err) {
		gpgme_set_armor(ctx, 1);
		err = (gpgme_data_seek(plain, 0, SEEK_SET) == -1) ?
			gpgme_error_from_errno(errno) : 0;
		if (!err) {
			/*
			 * Note -- it is currently the responsibility of select-keys.c::
			 * gpgmegtk_recipient_selection() to prompt the user whether to
			 * encrypt to recipients whose key is not trusted.
			 */
			err = gpgme_op_encrypt(ctx, kset,
			                       GPGME_ENCRYPT_ALWAYS_TRUST,
			                       plain, cipher);
		}
	}

	if (err) {
		g_warning("pgp_encrypt(): encryption failed: %s\n",
		          gpgme_strerror(err));
		gpgme_data_release(cipher);
		cipher = NULL;
	} else {
		debug_print("** encryption succeeded\n");
	}

	if (ctx)
		gpgme_release(ctx);
	return cipher;
}

/*
 * Create and return a list of keys matching a key id
 */

GSList *rfc2015_create_signers_list(const gchar *keyid)
{
	GSList *key_list = NULL;
	gpgme_ctx_t list_ctx = NULL;
	GSList *p;
	gpgme_error_t err;
	gpgme_key_t key;

	err = gpgme_new(&list_ctx);
	if (err)
		goto leave;
	err = gpgme_op_keylist_start(list_ctx, keyid, 1);
	if (err)
		goto leave;
	while (!(err = gpgme_op_keylist_next(list_ctx, &key))) {
		key_list = g_slist_append(key_list, key);
	}
	if (gpgme_err_code(err) != GPG_ERR_EOF)
		goto leave;
	err = 0;
	if (key_list == NULL) {
		debug_print("no keys found for keyid \"%s\"\n", keyid);
	}

leave:
	if (err) {
		debug_print("rfc2015_create_signers_list failed: %s\n",
		            gpgme_strerror(err));
		for (p = key_list; p != NULL; p = p->next)
			gpgme_key_unref((gpgme_key_t)p->data);
		g_slist_free(key_list);
	}
	if (list_ctx)
		gpgme_release(list_ctx);
	return err ? NULL : key_list;
}

/*
 * Encrypt the file by extracting all recipients and finding the
 * encryption keys for all of them.  The file content is then replaced
 * by the encrypted one.  */
gint rfc2015_encrypt(const gchar *file, GSList *recp_list)
{
	FILE *fp = NULL;
	gchar buf[BUFFSIZE];
	gint i, clineidx, saved_last;
	gchar *clines[3] = {NULL};
	gpgme_error_t err;
	gpgme_data_t header = NULL;
	gpgme_data_t plain = NULL;
	gpgme_data_t cipher = NULL;
	gpgme_key_t *kset = NULL;
	ssize_t bytesRW = 0;
	gint mime_version_seen = 0;
	gchar *boundary;

	boundary = generate_mime_boundary("Encrypt");

	/* Create the list of recipients */
	kset = gpgmegtk_recipient_selection(recp_list);
	if (!kset) {
		debug_print("error creating recipient list\n");
		goto failure;
	}

	/* Open the source file */
	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	err = gpgme_data_new(&header);
	if (!err)
		err = gpgme_data_new(&plain);
	if (err) {
		debug_print("gpgme_data_new failed: %s\n", gpgme_strerror(err));
		goto failure;
	}

	/* get the content header lines from the source */
	clineidx = 0;
	saved_last = 0;
	while (!err && fgets(buf, sizeof(buf), fp)) {
		/* fixme: check for overlong lines */
		if (headerp(buf, content_names)) {
			if (clineidx >= DIM(clines)) {
				debug_print("rfc2015_encrypt: too many content lines\n");
				goto failure;
			}
			clines[clineidx++] = g_strdup(buf);
			saved_last = 1;
			continue;
		}
		if (saved_last) {
			if (*buf == ' ' || *buf == '\t') {
				gchar *last = clines[clineidx - 1];
				clines[clineidx - 1] = g_strconcat(last, buf, NULL);
				g_free(last);
				continue;
			}
			saved_last = 0;
		}

		if (headerp(buf, mime_version_name))
			mime_version_seen = 1;

		if (buf[0] == '\r' || buf[0] == '\n')
			break;
		bytesRW = gpgme_data_write(header, buf, strlen(buf));
	}
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fgets");
		goto failure;
	}

	/* write them to the temp data and add the rest of the message */
	for (i = 0; (bytesRW != -1) && i < clineidx; i++) {
		debug_print("%% %s:%d: cline=`%s'", __FILE__ ,__LINE__, clines[i]);
		bytesRW = gpgme_data_write(plain, clines[i], strlen(clines[i]));
	}
	if (bytesRW != -1)
		bytesRW = gpgme_data_write (plain, "\r\n", 2);
	while ((bytesRW != -1) && fgets(buf, sizeof(buf), fp)) {
		bytesRW = gpgme_data_write(plain, buf, strlen(buf));
	}
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fgets");
		goto failure;
	}
	if (bytesRW == -1) {
		debug_print("gpgme_data_write failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	cipher = pgp_encrypt(plain, kset);
	gpgme_data_release(plain);
	plain = NULL;
	i = 0;
	while (kset[i] != NULL) {
		gpgme_key_unref(kset[i]);
		i++;
	}
	g_free(kset);
	kset = NULL;
	if (!cipher)
		goto failure;

	/* we have the encrypted message available in cipher and now we
	 * are going to rewrite the source file. To be sure that file has
	 * been truncated we use an approach which should work everywhere:
	 * close the file and then reopen it for writing. It is important
	 * that this works, otherwise it may happen that parts of the
	 * plaintext are still in the file (The encrypted stuff is, due to
	 * compression, usually shorter than the plaintext). 
	 * 
	 * Yes, there is a race condition here, but everyone, who is so
	 * stupid to store the temp file with the plaintext in a public
	 * directory has to live with this anyway. */
	if (fclose (fp)) {
		FILE_OP_ERROR(file, "fclose");
		goto failure;
	}
	if ((fp = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	/* Write the header, append new content lines, part 1 and part 2 header */
	err = (gpgme_data_seek(header, 0 , SEEK_SET) == -1) ?
	      gpgme_error_from_errno(errno) : 0;
	if (err) {
		debug_print("gpgme_data_seek failed: %s\n",
		            gpgme_strerror(err));
		goto failure;
	}
	bytesRW = gpgme_data_read(header, buf, BUFFSIZE);
	while (bytesRW > 0) {
		fwrite (buf, bytesRW, 1, fp);
		bytesRW = gpgme_data_read(header, buf, BUFFSIZE);
	}

	if (bytesRW != 0) {
		debug_print("gpgme_data_read failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	if (ferror (fp)) {
		FILE_OP_ERROR(file, "fwrite");
		goto failure;
	}
	gpgme_data_release(header);
	header = NULL;

	if (!mime_version_seen)
		fputs("MIME-Version: 1.0\r\n", fp);

	fprintf(fp,
	        "Content-Type: multipart/encrypted;"
	        " protocol=\"application/pgp-encrypted\";\r\n"
	        " boundary=\"%s\"\r\n"
	        "\r\n"
	        "--%s\r\n"
	        "Content-Type: application/pgp-encrypted\r\n"
	        "\r\n"
	        "Version: 1\r\n"
	        "\r\n"
	        "--%s\r\n"
	        "Content-Type: application/octet-stream\r\n"
	        "\r\n",
	        boundary, boundary, boundary);

	/* append the encrypted stuff */
	err = (gpgme_data_seek(cipher, 0 , SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (err) {
		debug_print("** gpgme_data_seek on cipher failed: %s\n",
		            gpgme_strerror(err));
		debug_print("gpgme_data_seek failed: %s\n",
		            gpgme_strerror(err));
		goto failure;
	}

	bytesRW = gpgme_data_read(cipher, buf, BUFFSIZE);
	while (bytesRW > 0) {
		fwrite(buf, bytesRW, 1, fp);
		bytesRW = gpgme_data_read(cipher, buf, BUFFSIZE);
	}

	if (bytesRW != 0) {
		debug_print("** gpgme_data_read failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	/* and the final boundary */
	fprintf(fp,
	        "\r\n"
	        "--%s--\r\n",
	        boundary);
	fflush(fp);
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fwrite");
		goto failure;
	}
	fclose(fp);
	gpgme_data_release(cipher);
	return 0;

failure:
	if (fp)
		fclose (fp);
	gpgme_data_release(header);
	gpgme_data_release(plain);
	gpgme_data_release(cipher);

	if (kset != NULL) {
		i = 0;
		while (kset[i] != NULL) {
			gpgme_key_unref(kset[i]);
			i++;
		}
		g_free(kset);
	}
	g_free(boundary);
	return -1; /* error */
}

gint rfc2015_encrypt_armored(const gchar *file, GSList *recp_list)
{
	FILE *fp = NULL;
	gchar buf[BUFFSIZE];
	gint i;
	gpgme_error_t err;
	gpgme_data_t plain = NULL;
	gpgme_data_t cipher = NULL;
	gpgme_key_t *kset = NULL;
	ssize_t bytesRW = 0;

	kset = gpgmegtk_recipient_selection(recp_list);
	if (!kset) {
		debug_print("error creating recipient list\n");
		goto failure;
	}

	/* Open the source file */
	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	err = gpgme_data_new(&plain);
	if (err) {
		g_warning("gpgme_data_new failed: %s\n", gpgme_strerror(err));
		goto failure;
	}

	while (bytesRW != -1 && fgets(buf, sizeof(buf), fp)) {
		bytesRW = gpgme_data_write(plain, buf, strlen(buf));
	}
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fgets");
		goto failure;
	}
	if (bytesRW == -1) {
		debug_print("gpgme_data_write failed: %s\n",
			    gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	cipher = pgp_encrypt(plain, kset);
	gpgme_data_release(plain);
	plain = NULL;
	i = 0;
	while (kset[i] != NULL) {
		gpgme_key_unref(kset[i]);
		i++;
	}
	g_free(kset);
	kset = NULL;
	if (!cipher)
		goto failure;

	if (fclose(fp)) {
		FILE_OP_ERROR(file, "fclose");
		goto failure;
	}
	if ((fp = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	err = (gpgme_data_seek(cipher, 0 , SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (err) {
		debug_print("** gpgme_data_seek on cipher failed: %s\n",
			    gpgme_strerror(err));
		debug_print("gpgme_data_seek failed: %s\n",
			    gpgme_strerror(err));
		goto failure;
	}

	bytesRW = gpgme_data_read(cipher, buf, sizeof(buf));
	while (bytesRW > 0) {
		fwrite(buf, bytesRW, 1, fp);
		bytesRW = gpgme_data_read(cipher, buf, sizeof(buf));
	}

	if (bytesRW != 0) {
		debug_print("** gpgme_data_read failed: %s\n",
			    gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	fflush(fp);
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fwrite");
		goto failure;
	}
	fclose(fp);
	gpgme_data_release(cipher);
	return 0;

failure:
	if (fp)
		fclose(fp);
	gpgme_data_release(plain);
	gpgme_data_release(cipher);
	if (kset != NULL) {
		i = 0;
		while (kset[i] != NULL) {
			gpgme_key_unref(kset[i]);
			i++;
		}
		g_free(kset);
	}

	return -1;
}

/*
 * plain contains an entire mime object.  Sign it and return an
 * GpgmeData object with the signature of it or NULL in case of error.
 * micalg returns the micalg information about the signature.
 */
static gpgme_data_t pgp_sign(gpgme_data_t plain, GSList *key_list,
			     gboolean clearsign, gchar **micalg)
{
	GSList *p;
	gpgme_ctx_t ctx = NULL;
	gpgme_error_t err;
	gpgme_data_t sig = NULL;
	gpgme_sign_result_t result = NULL;
	struct passphrase_cb_info_s info;

	*micalg = NULL;
	memset(&info, 0, sizeof info);

	err = gpgme_new(&ctx);
	if (err)
		goto leave;
	err = gpgme_data_new(&sig);
	if (err)
		goto leave;

	if (!g_getenv("GPG_AGENT_INFO")) {
		info.c = ctx;
		gpgme_set_passphrase_cb(ctx, gpgmegtk_passphrase_cb, &info);
	}
	gpgme_set_textmode(ctx, 1);
	gpgme_set_armor(ctx, 1);
	gpgme_signers_clear(ctx);
	for (p = key_list; p != NULL; p = p->next) {
		err = gpgme_signers_add(ctx, (gpgme_key_t) p->data);
		if (err)
			goto leave;
	}
	for (p = key_list; p != NULL; p = p->next)
		gpgme_key_unref((gpgme_key_t) p->data);
	g_slist_free(key_list);

	err = (gpgme_data_seek(plain, 0, SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (!err) {
		err = gpgme_op_sign(ctx, plain, sig,
		                    clearsign ? GPGME_SIG_MODE_CLEAR : GPGME_SIG_MODE_DETACH);
	}
	if (!err) {
		result = gpgme_op_sign_result(ctx);
		if (result && result->signatures) {
			if (gpgme_get_protocol(ctx) == GPGME_PROTOCOL_OpenPGP) {
				*micalg = g_strdup_printf
					("PGP-%s", gpgme_hash_algo_name(result->signatures->hash_algo));
			} else {
				*micalg = g_strdup(gpgme_hash_algo_name(result->signatures->hash_algo));
			}
		} else {
			/* can't get result (maybe no signing key?) */
			err = GPG_ERR_USER_1;
		}
	}

leave:
	if (err) {
		gpgmegtk_free_passphrase();
		g_warning("pgp_sign(): signing failed: %s\n", gpgme_strerror(err));
		gpgme_data_release(sig);
		sig = NULL;
	} else {
		debug_print("signing succeeded\n");
	}

	if (ctx)
		gpgme_release(ctx);
	return sig;
}

/*
 * plain contains an entire mime object.  Encrypt and sign it and return an
 * GpgmeData object with the encrypted and signed version of it or NULL in
 * case of error.
 * micalg returns the micalg information about the signature.
 */
static gpgme_data_t pgp_encrypt_sign(gpgme_data_t plain, gpgme_key_t kset[],
				     GSList *key_list, gchar **micalg)
{
	GSList *p;
	gpgme_ctx_t ctx = NULL;
	gpgme_error_t err;
	gpgme_data_t cipher = NULL;
	gpgme_sign_result_t result = NULL;
	struct passphrase_cb_info_s info;

	*micalg = NULL;
	memset(&info, 0, sizeof info);

	err = gpgme_new(&ctx);
	if (err)
		goto leave;
	err = gpgme_data_new(&cipher);
	if (err)
		goto leave;

	if (!g_getenv("GPG_AGENT_INFO")) {
		info.c = ctx;
		gpgme_set_passphrase_cb(ctx, gpgmegtk_passphrase_cb, &info);
	}
	gpgme_set_textmode(ctx, 1);
	gpgme_set_armor(ctx, 1);
	gpgme_signers_clear(ctx);
	for (p = key_list; p != NULL; p = p->next) {
		err = gpgme_signers_add(ctx, (gpgme_key_t) p->data);
		if (err)
			goto leave;
	}
	for (p = key_list; p != NULL; p = p->next)
		gpgme_key_unref((gpgme_key_t) p->data);
	g_slist_free(key_list);

	err = (gpgme_data_seek(plain, 0, SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (!err) {
		err = gpgme_op_encrypt_sign(ctx, kset, GPGME_ENCRYPT_ALWAYS_TRUST, plain, cipher);
	}
	if (!err) {
		result = gpgme_op_sign_result(ctx);
		if (result && result->signatures) {
			if (gpgme_get_protocol(ctx) == GPGME_PROTOCOL_OpenPGP) {
				*micalg = g_strdup_printf
					("PGP-%s", gpgme_hash_algo_name(result->signatures->hash_algo));
			} else {
				*micalg = g_strdup(gpgme_hash_algo_name(result->signatures->hash_algo));
			}
		} else {
			/* can't get result (maybe no signing key?) */
			err = GPG_ERR_USER_1;
		}
	}

leave:
	if (err) {
		gpgmegtk_free_passphrase();
		g_warning("pgp_sign(): encryption and signing failed: %s\n", gpgme_strerror(err));
		gpgme_data_release(cipher);
		cipher = NULL;
	} else {
		debug_print("encryption and signing succeeded\n");
	}

	if (ctx)
		gpgme_release(ctx);
	return cipher;
}

/*
 * Sign the file and replace its content with the signed one.
 */
gint rfc2015_sign(const gchar *file, GSList *key_list)
{
	FILE *fp = NULL;
	gchar buf[BUFFSIZE];
	gint i, clineidx, saved_last;
	gchar *clines[3] = {NULL};
	gpgme_error_t err;
	gpgme_data_t header = NULL;
	gpgme_data_t plain = NULL;
	gpgme_data_t sigdata = NULL;
	ssize_t bytesRW = -1;
	gint mime_version_seen = 0;
	gchar *boundary;
	gchar *micalg = NULL;

	boundary = generate_mime_boundary("Signature");

	/* Open the source file */
	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	err = gpgme_data_new(&header);
	if (!err)
		err = gpgme_data_new(&plain);
	if (err) {
		debug_print("gpgme_data_new failed: %s\n", gpgme_strerror(err));
		goto failure;
	}

	/* get the content header lines from the source */
	clineidx = 0;
	saved_last = 0;
	while (!err && fgets(buf, sizeof(buf), fp)) {
		/* fixme: check for overlong lines */
		if (headerp(buf, content_names)) {
			if (clineidx >= DIM(clines)) {
				debug_print("rfc2015_sign: too many content lines\n");
				goto failure;
			}
			clines[clineidx++] = g_strdup(buf);
			saved_last = 1;
			continue;
		}
		if (saved_last) {
			if (*buf == ' ' || *buf == '\t') {
				gchar *last = clines[clineidx - 1];
				clines[clineidx - 1] = g_strconcat(last, buf, NULL);
				g_free(last);
				continue;
			}
			saved_last = 0;
		}

		if (headerp(buf, mime_version_name))
			mime_version_seen = 1;

		if (buf[0] == '\r' || buf[0] == '\n')
			break;
		bytesRW = gpgme_data_write(header, buf, strlen (buf));
	}
	if (ferror (fp)) {
		FILE_OP_ERROR(file, "fgets");
		goto failure;
	}

	/* write them to the temp data and add the rest of the message */
	for (i = 0; (bytesRW != -1) && i < clineidx; i++) {
		bytesRW = gpgme_data_write(plain, clines[i], strlen(clines[i]));
	}
	if (bytesRW != -1)
		bytesRW = gpgme_data_write(plain, "\r\n", 2 );
	while ((bytesRW != -1) && fgets(buf, sizeof(buf), fp)) {
		bytesRW = gpgme_data_write(plain, buf, strlen(buf));
	}
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fgets");
		goto failure;
	}
	if (bytesRW == -1) {
		debug_print("gpgme_data_write failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	sigdata = pgp_sign(plain, key_list, FALSE, &micalg);
	if (!sigdata)
		goto failure;

	/* we have the signed message available in sigdata and now we are
	 * going to rewrite the original file. To be sure that file has
	 * been truncated we use an approach which should work everywhere:
	 * close the file and then reopen it for writing. */
	if (fclose(fp)) {
		FILE_OP_ERROR(file, "fclose");
		goto failure;
	}
	if ((fp = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	/* Write the rfc822 header and add new content lines */
	err = (gpgme_data_seek(header, 0, SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (err) {
		debug_print("gpgme_data_seek failed: %s\n",
		            gpgme_strerror(err));
		goto failure;
	}
	bytesRW = gpgme_data_read(header, buf, BUFFSIZE);
	while (bytesRW > 0) {
		fwrite(buf, bytesRW, 1, fp);
		bytesRW = gpgme_data_read(header, buf, BUFFSIZE);
	}
	if (bytesRW != 0) {
		debug_print("gpgme_data_read failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fwrite");
		goto failure;
	}
	gpgme_data_release(header);
	header = NULL;

	if (!mime_version_seen)
		fputs("MIME-Version: 1.0\r\n", fp);
	fprintf(fp, "Content-Type: multipart/signed; "
	        "protocol=\"application/pgp-signature\";\r\n");
	if (micalg)
		fprintf(fp, " micalg=\"%s\";\r\n", micalg);
	fprintf(fp, " boundary=\"%s\"\r\n", boundary);

	/* Part 1: signed material */
	fprintf(fp, "\r\n"
	        "--%s\r\n",
	        boundary);
	err = (gpgme_data_seek(plain, 0, SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (err) {
		debug_print("gpgme_data_seek on plain failed: %s\n",
		            gpgme_strerror(err));
		goto failure;
	}
	bytesRW = gpgme_data_read(plain, buf, BUFFSIZE);
	while (bytesRW > 0) {
		fwrite(buf, bytesRW, 1, fp);
		bytesRW = gpgme_data_read(plain, buf, BUFFSIZE);
	}
	if (bytesRW != 0) {
		debug_print("gpgme_data_read failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	/* Part 2: signature */
	fprintf(fp, "\r\n"
	        "--%s\r\n",
	        boundary);
	fputs("Content-Type: application/pgp-signature\r\n"
	      "\r\n", fp);

	err = (gpgme_data_seek(sigdata, 0, SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (err) {
		debug_print("gpgme_data_seek on sigdata failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	bytesRW = gpgme_data_read(sigdata, buf, BUFFSIZE);
	while (bytesRW > 0) {
		fwrite(buf, bytesRW, 1, fp);
		bytesRW = gpgme_data_read(sigdata, buf, BUFFSIZE);
	}
	if (bytesRW != 0) {
		debug_print("gpgme_data_read failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	/* Final boundary */
	fprintf(fp, "\r\n"
	        "--%s--\r\n",
	        boundary);
	fflush(fp);
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fwrite");
		goto failure;
	}
	fclose(fp);
	gpgme_data_release(header);
	gpgme_data_release(plain);
	gpgme_data_release(sigdata);
	g_free(boundary);
	g_free(micalg);
	return 0;

failure:
	if (fp)
		fclose(fp);
	gpgme_data_release(header);
	gpgme_data_release(plain);
	gpgme_data_release(sigdata);
	g_free(boundary);
	g_free(micalg);
	return -1; /* error */
}


/*
 * Sign the file with clear text and replace its content with the signed one.
 */
gint rfc2015_clearsign(const gchar *file, GSList *key_list)
{
	FILE *fp;
	gchar buf[BUFFSIZE];
	gpgme_error_t err;
	gpgme_data_t text = NULL;
	gpgme_data_t sigdata = NULL;
	ssize_t bytesRW = 0;
	gchar *micalg = NULL;

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	err = gpgme_data_new(&text);
	if (err) {
		debug_print("gpgme_data_new failed: %s\n", gpgme_strerror(err));
		goto failure;
	}

	while ((bytesRW != -1) && fgets(buf, sizeof(buf), fp)) {
		bytesRW = gpgme_data_write(text, buf, strlen(buf));
	}
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fgets");
		goto failure;
	}
	if (bytesRW == -1) {
		debug_print("gpgme_data_write failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	sigdata = pgp_sign(text, key_list, TRUE, &micalg);
	if (micalg) {
		g_free(micalg);
	}
	if (!sigdata)
		goto failure;

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		fp = NULL;
		goto failure;
	}
	if ((fp = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	err = (gpgme_data_seek(sigdata, 0, SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (err) {
		debug_print("gpgme_data_seek on sigdata failed: %s\n",
		            gpgme_strerror(err));
		goto failure;
	}

	bytesRW = gpgme_data_read(sigdata, buf, BUFFSIZE);
	while (bytesRW > 0) {
		fwrite(buf, bytesRW, 1, fp);
		bytesRW = gpgme_data_read(sigdata, buf, BUFFSIZE);
	}
	if (bytesRW != 0) {
		debug_print("gpgme_data_read failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		fp = NULL;
		goto failure;
	}
	gpgme_data_release(text);
	gpgme_data_release(sigdata);
	return 0;

failure:
	if (fp)
		fclose(fp);
	gpgme_data_release(text);
	gpgme_data_release(sigdata);
	return -1;
}

/*
 * Encrypt and sign the file and replace its content with the encrypted and
 * signed one.
 */
gint rfc2015_encrypt_sign(const gchar *file, GSList *recp_list,
			  GSList *key_list)
{
	FILE *fp = NULL;
	gchar buf[BUFFSIZE];
	gint i, clineidx, saved_last;
	gchar *clines[3] = {NULL};
	gpgme_error_t err;
	gpgme_data_t header = NULL;
	gpgme_data_t plain = NULL;
	gpgme_data_t cipher = NULL;
	gpgme_key_t *kset = NULL;
	ssize_t bytesRW = -1;
	gint mime_version_seen = 0;
	gchar *boundary;
	gchar *micalg = NULL;

	boundary = generate_mime_boundary("Encrypt");

	/* Create the list of recipients */
	kset = gpgmegtk_recipient_selection(recp_list);
	if (!kset) {
		debug_print("error creating recipient list\n");
		goto failure;
	}

	/* Open the source file */
	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	err = gpgme_data_new(&header);
	if (!err)
		err = gpgme_data_new(&plain);
	if (err) {
		debug_print("gpgme_data_new failed: %s\n", gpgme_strerror(err));
		goto failure;
	}

	/* get the content header lines from the source */
	clineidx = 0;
	saved_last = 0;
	while (!err && fgets(buf, sizeof(buf), fp)) {
		/* fixme: check for overlong lines */
		if (headerp(buf, content_names)) {
			if (clineidx >= DIM(clines)) {
				debug_print("rfc2015_sign: too many content lines\n");
				goto failure;
			}
			clines[clineidx++] = g_strdup(buf);
			saved_last = 1;
			continue;
		}
		if (saved_last) {
			if (*buf == ' ' || *buf == '\t') {
				gchar *last = clines[clineidx - 1];
				clines[clineidx - 1] = g_strconcat(last, buf, NULL);
				g_free(last);
				continue;
			}
			saved_last = 0;
		}

		if (headerp(buf, mime_version_name))
			mime_version_seen = 1;

		if (buf[0] == '\r' || buf[0] == '\n')
			break;
		bytesRW = gpgme_data_write(header, buf, strlen (buf));
	}
	if (ferror (fp)) {
		FILE_OP_ERROR(file, "fgets");
		goto failure;
	}

	/* write them to the temp data and add the rest of the message */
	for (i = 0; (bytesRW != -1) && i < clineidx; i++) {
		bytesRW = gpgme_data_write(plain, clines[i], strlen(clines[i]));
	}
	if (bytesRW != -1)
		bytesRW = gpgme_data_write(plain, "\r\n", 2 );
	while ((bytesRW != -1) && fgets(buf, sizeof(buf), fp)) {
		bytesRW = gpgme_data_write(plain, buf, strlen(buf));
	}
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fgets");
		goto failure;
	}
	if (bytesRW == -1) {
		debug_print("gpgme_data_write failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	cipher = pgp_encrypt_sign(plain, kset, key_list, &micalg);
	gpgme_data_release(plain);
	plain = NULL;
	for (i = 0; kset[i] != NULL; i++)
		gpgme_key_unref(kset[i]);
	g_free(kset);
	kset = NULL;
	if (!cipher)
		goto failure;

	/* we have the signed message available in sigdata and now we are
	 * going to rewrite the original file. To be sure that file has
	 * been truncated we use an approach which should work everywhere:
	 * close the file and then reopen it for writing. */
	if (fclose(fp)) {
		FILE_OP_ERROR(file, "fclose");
		goto failure;
	}
	if ((fp = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	/* Write the rfc822 header and add new content lines */
	err = (gpgme_data_seek(header, 0, SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (err) {
		debug_print("gpgme_data_seek failed: %s\n",
		            gpgme_strerror(err));
		goto failure;
	}
	bytesRW = gpgme_data_read(header, buf, BUFFSIZE);
	while (bytesRW > 0) {
		fwrite(buf, bytesRW, 1, fp);
		bytesRW = gpgme_data_read(header, buf, BUFFSIZE);
	}
	if (bytesRW != 0) {
		debug_print("gpgme_data_read failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fwrite");
		goto failure;
	}
	gpgme_data_release(header);
	header = NULL;

	if (!mime_version_seen)
		fputs("MIME-Version: 1.0\r\n", fp);
	fprintf(fp, "Content-Type: multipart/encrypted;"
	        " protocol=\"application/pgp-encrypted\";\r\n"
		" boundary=\"%s\"\r\n"
		"\r\n"
		"--%s\r\n"
		"Content-Type: application/pgp-encrypted\r\n"
		"\r\n"
		"Version: 1\r\n"
		"\r\n"
		"--%s\r\n"
		"Content-Type: application/octet-stream\r\n"
		"\r\n",
		boundary, boundary, boundary);

	/* append the encrypted stuff */
	err = (gpgme_data_seek(cipher, 0, SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (err) {
		debug_print("gpgme_data_seek on cipher failed: %s\n",
		            gpgme_strerror(err));
		goto failure;
	}

	bytesRW = gpgme_data_read(cipher, buf, BUFFSIZE);
	while (bytesRW > 0) {
		fwrite(buf, bytesRW, 1, fp);
		bytesRW = gpgme_data_read(cipher, buf, BUFFSIZE);
	}
	if (bytesRW != 0) {
		debug_print("gpgme_data_read failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	/* Final boundary */
	fprintf(fp,
		"\r\n"
	        "--%s--\r\n",
	        boundary);
	fflush(fp);
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fwrite");
		goto failure;
	}
	fclose(fp);
	gpgme_data_release(cipher);
	g_free(boundary);
	g_free(micalg);
	return 0;

failure:
	if (fp)
		fclose(fp);
	gpgme_data_release(header);
	gpgme_data_release(plain);
	gpgme_data_release(cipher);

	if (kset != NULL) {
		for (i = 0; kset[i] != NULL; i++)
			gpgme_key_unref(kset[i]);
		g_free(kset);
	}
	g_free(boundary);
	g_free(micalg);
	return -1; /* error */
}

gint rfc2015_encrypt_sign_armored(const gchar *file, GSList *recp_list,
				  GSList *key_list)
{
	FILE *fp = NULL;
	gchar buf[BUFFSIZE];
	gint i;
	gpgme_error_t err;
	gpgme_data_t plain = NULL;
	gpgme_data_t cipher = NULL;
	gpgme_key_t *kset = NULL;
	ssize_t bytesRW = 0;
	gchar *micalg = NULL;

	kset = gpgmegtk_recipient_selection(recp_list);
	if (!kset) {
		debug_print("error creating recipient list\n");
		goto failure;
	}

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	err = gpgme_data_new(&plain);
	if (err) {
		debug_print("gpgme_data_new failed: %s\n", gpgme_strerror(err));
		goto failure;
	}

	while ((bytesRW != -1) && fgets(buf, sizeof(buf), fp)) {
		bytesRW = gpgme_data_write(plain, buf, strlen(buf));
	}
	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fgets");
		goto failure;
	}
	if (bytesRW == -1) {
		debug_print("gpgme_data_write failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	cipher = pgp_encrypt_sign(plain, kset, key_list, &micalg);
	if (micalg)
		g_free(micalg);
	if (!cipher)
		goto failure;

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		fp = NULL;
		goto failure;
	}
	if ((fp = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		goto failure;
	}

	err = (gpgme_data_seek(cipher, 0, SEEK_SET) == -1) ?
		gpgme_error_from_errno(errno) : 0;
	if (err) {
		debug_print("gpgme_data_seek on cipher failed: %s\n",
		            gpgme_strerror(err));
		goto failure;
	}

	bytesRW = gpgme_data_read(cipher, buf, BUFFSIZE);
	while (bytesRW > 0) {
		fwrite(buf, bytesRW, 1, fp);
		bytesRW = gpgme_data_read(cipher, buf, BUFFSIZE);
	}
	if (bytesRW != 0) {
		debug_print("gpgme_data_read failed: %s\n",
		            gpgme_strerror(gpgme_error_from_errno(errno)));
		goto failure;
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		fp = NULL;
		goto failure;
	}
	gpgme_data_release(plain);
	gpgme_data_release(cipher);
	for (i = 0; kset[i] != NULL; i++)
		gpgme_key_unref(kset[i]);
	g_free(kset);
	return 0;

failure:
	if (fp)
		fclose(fp);
	gpgme_data_release(plain);
	gpgme_data_release(cipher);
	if (kset != NULL) {
		for (i = 0; kset[i] != NULL; i++)
			gpgme_key_unref(kset[i]);
		g_free(kset);
	}
	return -1;
}

#endif /* USE_GPGME */
