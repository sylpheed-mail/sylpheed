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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_GPGME

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>

#include <gpgme.h>

#include "intl.h"
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

#define DIM(v)     (sizeof(v)/sizeof((v)[0]))

static char *content_names[] = {
    "Content-Type",
    "Content-Disposition",
    "Content-Transfer-Encoding",
    NULL
};

static char *mime_version_name[] = {
    "Mime-Version",
    NULL
};

#if 0
static void dump_mimeinfo (const char *text, MimeInfo *x)
{
    debug_print ("MimeInfo[%s] %p  level=%d\n",
               text, x, x? x->level:0 );
    if (!x)
        return;

    debug_print ("      enc=`%s' enc_type=%d mime_type=%d\n",
               x->encoding, x->encoding_type, x->mime_type );
    debug_print ("      cont_type=`%s' cs=`%s' name=`%s' bnd=`%s'\n",
               x->content_type, x->charset, x->name, x->boundary );
    debug_print ("      cont_disp=`%s' fname=`%s' fpos=%ld size=%u, lvl=%d\n",
               x->content_disposition, x->filename, x->fpos, x->size,
               x->level );
    dump_mimeinfo (".main", x->main );
    dump_mimeinfo (".sub", x->sub );
    dump_mimeinfo (".next", x->next );
    debug_print ("MimeInfo[.parent] %p\n", x ); 
    dump_mimeinfo (".children", x->children );
    dump_mimeinfo (".plaintext", x->plaintext );
}

static void dump_part ( MimeInfo *mimeinfo, FILE *fp )
{
    unsigned int size = mimeinfo->size;
    int c;

    if (fseek (fp, mimeinfo->fpos, SEEK_SET)) {
        debug_print ("dump_part: fseek error\n");
        return;
    }

    debug_print ("--- begin dump_part ----\n");
    while (size-- && (c = getc (fp)) != EOF) 
        putc (c, stderr);
    if (ferror (fp))
        debug_print ("dump_part: read error\n");
    debug_print ("--- end dump_part ----\n");
}
#endif

void
rfc2015_disable_all (void)
{
    /* FIXME: set a flag, so that we don't bother the user with failed
     * gpgme messages */
}


void
rfc2015_secure_remove (const char *fname)
{
    if (!fname)
        return;
    /* fixme: overwrite the file first */
    remove (fname);
}


static const gchar *
sig_status_to_string (GpgmeSigStat status)
{
    const gchar *result;

    switch (status) {
      case GPGME_SIG_STAT_NONE:
        result = _("Oops: Signature not verified");
        break;
      case GPGME_SIG_STAT_NOSIG:
        result = _("No signature found");
        break;
      case GPGME_SIG_STAT_GOOD:
        result = _("Good signature");
        break;
      case GPGME_SIG_STAT_BAD:
        result = _("BAD signature");
        break;
      case GPGME_SIG_STAT_NOKEY:
        result = _("No public key to verify the signature");
        break;
      case GPGME_SIG_STAT_ERROR:
        result = _("Error verifying the signature");
        break;
      case GPGME_SIG_STAT_DIFF:
        result = _("Different results for signatures");
        break;
      default:
	result = _("Error: Unknown status");
	break;
    }

    return result;
}

static const gchar *
sig_status_with_name (GpgmeSigStat status)
{
    const gchar *result;

    switch (status) {
      case GPGME_SIG_STAT_NONE:
        result = _("Oops: Signature not verified");
        break;
      case GPGME_SIG_STAT_NOSIG:
        result = _("No signature found");
        break;
      case GPGME_SIG_STAT_GOOD:
        result = _("Good signature from \"%s\"");
        break;
      case GPGME_SIG_STAT_BAD:
        result = _("BAD signature from \"%s\"");
        break;
      case GPGME_SIG_STAT_NOKEY:
        result = _("No public key to verify the signature");
        break;
      case GPGME_SIG_STAT_ERROR:
        result = _("Error verifying the signature");
        break;
      case GPGME_SIG_STAT_DIFF:
        result = _("Different results for signatures");
        break;
      default:
	result = _("Error: Unknown status");
	break;
    }

    return result;
}

static void
sig_status_for_key(GString *str, GpgmeCtx ctx, GpgmeSigStat status, 
		   GpgmeKey key, const gchar *fpr)
{
	gint idx = 0;
	const char *uid;

	uid = gpgme_key_get_string_attr (key, GPGME_ATTR_USERID, NULL, idx);
	if (uid == NULL) {
		g_string_sprintfa (str, "%s\n",
				   sig_status_to_string (status));
		if ((fpr != NULL) && (*fpr != '\0'))
			g_string_sprintfa (str, "Key fingerprint: %s\n", fpr);
		g_string_append (str, _("Cannot find user ID for this key."));
		return;
	}
	g_string_sprintfa (str, sig_status_with_name (status), uid);
	g_string_append (str, "\n");

	while (1) {
		uid = gpgme_key_get_string_attr (key, GPGME_ATTR_USERID,
						 NULL, ++idx);
		if (uid == NULL)
			break;
		g_string_sprintfa (str, _("                aka \"%s\"\n"),
				   uid);
	}
}

static gchar *
sig_status_full (GpgmeCtx ctx)
{
	GString *str;
	gint sig_idx = 0;
	GpgmeError err;
	GpgmeSigStat status;
	GpgmeKey key;
	const char *fpr;
	time_t created;
	struct tm *ctime_val;
	char ctime_str[80];
	gchar *retval;

	str = g_string_new ("");

	fpr = gpgme_get_sig_status (ctx, sig_idx, &status, &created);
	while (fpr != NULL) {
		if (created != 0) {
			ctime_val = localtime (&created);
			strftime (ctime_str, sizeof (ctime_str), "%c", 
				  ctime_val);
			g_string_sprintfa (str,
					   _("Signature made at %s\n"),
					   ctime_str);
		}
		err = gpgme_get_sig_key (ctx, sig_idx, &key);
		if (err != 0) {
			g_string_sprintfa (str, "%s\n",
					   sig_status_to_string (status));
			if ((fpr != NULL) && (*fpr != '\0'))
				g_string_sprintfa (str, 
						   _("Key fingerprint: %s\n"),
						   fpr);
		} else {
			sig_status_for_key (str, ctx, status, key, fpr);
			gpgme_key_unref (key);
		}
		g_string_append (str, "\n\n");

		fpr = gpgme_get_sig_status (ctx, ++sig_idx, &status, &created);
	}

	retval = str->str;
	g_string_free (str, FALSE);
	return retval;
}

static void check_signature (MimeInfo *mimeinfo, MimeInfo *partinfo, FILE *fp)
{
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData sig = NULL, text = NULL;
    GpgmeSigStat status = GPGME_SIG_STAT_NONE;
    GpgmegtkSigStatus statuswindow = NULL;
    const char *result = NULL;
    gchar *tmp_file;
    gint n_exclude_chars = 0;

    if (prefs_common.gpg_signature_popup)
	statuswindow = gpgmegtk_sig_status_create ();

    err = gpgme_new (&ctx);
    if (err) {
	debug_print ("gpgme_new failed: %s\n", gpgme_strerror (err));
	goto leave;
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
	unlink(tmp_file);
	g_free(tmp_file);
	goto leave;
    }

    err = gpgme_data_new_from_file(&text, tmp_file, 1);

    unlink(tmp_file);
    g_free(tmp_file);

    if (!err)
	err = gpgme_data_new_from_filepart (&sig, NULL, fp,
					    partinfo->fpos, partinfo->size);
    if (err) {
        debug_print ("gpgme_data_new_from_filepart failed: %s\n",
		   gpgme_strerror (err));
        goto leave;
    }

    err = gpgme_op_verify (ctx, sig, text, &status);
    if (err)  {
        debug_print ("gpgme_op_verify failed: %s\n", gpgme_strerror (err));
        goto leave;
    }

    /* FIXME: check what the heck this sig_status_full stuff is.
     * it should better go into sigstatus.c */
    g_free (partinfo->sigstatus_full);
    partinfo->sigstatus_full = sig_status_full (ctx);

leave:
    result = gpgmegtk_sig_status_to_string(status);
    debug_print("verification status: %s\n", result);
    if (prefs_common.gpg_signature_popup)
	gpgmegtk_sig_status_update (statuswindow, ctx);

    g_free (partinfo->sigstatus);
    partinfo->sigstatus = g_strdup (result);

    gpgme_data_release (sig);
    gpgme_data_release (text);
    gpgme_release (ctx);
    if (prefs_common.gpg_signature_popup)
	gpgmegtk_sig_status_destroy (statuswindow);
}

/*
 * Copy a gpgme data object to a temporary file and
 * return this filename 
 */
#if 0
static char *
copy_gpgmedata_to_temp (GpgmeData data, guint *length)
{
    static int id;
    char *tmp;
    FILE *fp;
    char buf[100];
    size_t nread;
    GpgmeError err;
    
    tmp = g_strdup_printf("%s%cgpgtmp.%08x",
                          get_mime_tmp_dir(), G_DIR_SEPARATOR, ++id );

    if ((fp = fopen(tmp, "wb")) == NULL) {
        FILE_OP_ERROR(tmp, "fopen");
        g_free(tmp);
        return NULL;
    }

    err = gpgme_data_rewind ( data );
    if (err)
        debug_print ("gpgme_data_rewind failed: %s\n", gpgme_strerror (err));

    while (!(err = gpgme_data_read (data, buf, 100, &nread))) {
        fwrite ( buf, nread, 1, fp );
    }

    if (err != GPGME_EOF)
        debug_print ("gpgme_data_read failed: %s\n", gpgme_strerror (err));

    fclose (fp);
    *length = nread;

    return tmp;
}
#endif

static GpgmeData
pgp_decrypt (MimeInfo *partinfo, FILE *fp)
{
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData cipher = NULL, plain = NULL;
    struct passphrase_cb_info_s info;

    memset (&info, 0, sizeof info);

    err = gpgme_new (&ctx);
    if (err) {
        debug_print ("gpgme_new failed: %s\n", gpgme_strerror (err));
        goto leave;
    }

    err = gpgme_data_new_from_filepart (&cipher, NULL, fp,
					partinfo->fpos, partinfo->size);
    if (err) {
        debug_print ("gpgme_data_new_from_filepart failed: %s\n",
                     gpgme_strerror (err));
        goto leave;
    }

    err = gpgme_data_new (&plain);
    if (err) {
        debug_print ("gpgme_new failed: %s\n", gpgme_strerror (err));
        goto leave;
    }

    if (!getenv("GPG_AGENT_INFO")) {
        info.c = ctx;
        gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
    } 

    err = gpgme_op_decrypt (ctx, cipher, plain);

leave:
    gpgme_data_release (cipher);
    if (err) {
        gpgmegtk_free_passphrase();
        debug_print ("decryption failed: %s\n", gpgme_strerror (err));
        gpgme_data_release (plain);
        plain = NULL;
    }
    else
        debug_print ("** decryption succeeded\n");

    gpgme_release (ctx);
    return plain;
}

MimeInfo * rfc2015_find_signature (MimeInfo *mimeinfo)
{
    MimeInfo *partinfo;
    int n = 0;

    if (!mimeinfo)
        return NULL;
    if (g_strcasecmp (mimeinfo->content_type, "multipart/signed"))
        return NULL;

    debug_print ("** multipart/signed encountered\n");

    /* check that we have at least 2 parts of the correct type */
    for (partinfo = mimeinfo->children;
         partinfo != NULL; partinfo = partinfo->next) {
        if (++n > 1  && !g_strcasecmp (partinfo->content_type,
				       "application/pgp-signature"))
            break;
    }

    return partinfo;
}

gboolean rfc2015_has_signature (MimeInfo *mimeinfo)
{
    return rfc2015_find_signature (mimeinfo) != NULL;
}

void rfc2015_check_signature (MimeInfo *mimeinfo, FILE *fp)
{
    MimeInfo *partinfo;

    partinfo = rfc2015_find_signature (mimeinfo);
    if (!partinfo)
        return;

#if 0
    g_message ("** yep, it is a pgp signature");
    dump_mimeinfo ("gpg-signature", partinfo );
    dump_part (partinfo, fp );
    dump_mimeinfo ("signed text", mimeinfo->children );
    dump_part (mimeinfo->children, fp);
#endif

    check_signature (mimeinfo, partinfo, fp);
}

int rfc2015_is_encrypted (MimeInfo *mimeinfo)
{
    if (!mimeinfo || mimeinfo->mime_type != MIME_MULTIPART)
        return 0;
    if (g_strcasecmp (mimeinfo->content_type, "multipart/encrypted"))
        return 0;
    /* fixme: we should check the protocol parameter */
    return 1;
}

gboolean rfc2015_msg_is_encrypted (const gchar *file)
{
	FILE *fp;
	MimeInfo *mimeinfo;
	int ret;

	if ((fp = fopen(file, "rb")) == NULL)
		return FALSE;

	mimeinfo = procmime_scan_mime_header(fp);
	if(!mimeinfo) {
		fclose(fp);
		return FALSE;
	}

	ret = rfc2015_is_encrypted(mimeinfo);
	procmime_mimeinfo_free_all(mimeinfo);
	return ret != 0 ? TRUE : FALSE;
}

static int
name_cmp(const char *a, const char *b)
{
    for( ; *a && *b; a++, b++) {
        if(*a != *b
           && toupper(*(unsigned char *)a) != toupper(*(unsigned char *)b))
            return 1;
    }

    return *a != *b;
}

static int
headerp(char *p, char **names)
{
    int i, c;
    char *p2;

    p2 = strchr(p, ':');
    if(!p2 || p == p2) {
        return 0;
    }
    if(p2[-1] == ' ' || p2[-1] == '\t') {
        return 0;
    }

    if(!names[0])
        return 1;  

    c = *p2;
    *p2 = 0;
    for(i = 0 ; names[i] != NULL; i++) {
        if(!name_cmp (names[i], p))
            break;
    }
    *p2 = c;

    return names[i] != NULL;
}


#define DECRYPTION_ABORT() \
{ \
    procmime_mimeinfo_free_all(tmpinfo); \
    msginfo->decryption_failed = 1; \
    return; \
}

void rfc2015_decrypt_message (MsgInfo *msginfo, MimeInfo *mimeinfo, FILE *fp)
{
    static int id;
    MimeInfo *tmpinfo, *partinfo;
    int ver_ok = 0;
    char *fname;
    GpgmeData plain;
    FILE *dstfp;
    size_t nread;
    char buf[BUFFSIZE];
    int in_cline;
    GpgmeError err;

    g_return_if_fail (msginfo != NULL);
    g_return_if_fail (mimeinfo != NULL);
    g_return_if_fail (fp != NULL);
    g_return_if_fail (mimeinfo->mime_type == MIME_MULTIPART);

    debug_print ("** decrypting multipart/encrypted message\n");

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
    if (!g_strcasecmp (partinfo->content_type, "application/pgp-encrypted")) {
        /* Fixme: check that the version is 1 */
        ver_ok = 1;
    }
    partinfo = partinfo->next;
    if (ver_ok &&
        !g_strcasecmp (partinfo->content_type, "application/octet-stream")) {
        if (partinfo->next)
            g_warning ("oops: pgp_encrypted with more than 2 parts");
    }
    else {
        DECRYPTION_ABORT();
    }

    debug_print ("** yep, it is pgp encrypted\n");

    plain = pgp_decrypt (partinfo, fp);
    if (!plain) {
        DECRYPTION_ABORT();
    }

    fname = g_strdup_printf("%s%cplaintext.%08x",
			    get_mime_tmp_dir(), G_DIR_SEPARATOR, ++id);

    if ((dstfp = fopen(fname, "wb")) == NULL) {
        FILE_OP_ERROR(fname, "fopen");
        g_free(fname);
        DECRYPTION_ABORT();
    }

    /* write the orginal header to the new file */
    if (fseek(fp, tmpinfo->fpos, SEEK_SET) < 0)
        perror("fseek");

    in_cline = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        if (headerp (buf, content_names)) {
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
        fputs (buf, dstfp);
    }

    err = gpgme_data_rewind (plain);
    if (err)
        debug_print ("gpgme_data_rewind failed: %s\n", gpgme_strerror (err));

    while (!(err = gpgme_data_read (plain, buf, sizeof(buf), &nread))) {
        fwrite (buf, nread, 1, dstfp);
    }

    if (err != GPGME_EOF) {
        debug_print ("gpgme_data_read failed: %s\n", gpgme_strerror (err));
    }

    fclose (dstfp);
    procmime_mimeinfo_free_all(tmpinfo);

    msginfo->plaintext_file = fname;
    msginfo->decryption_failed = 0;
}

#undef DECRYPTION_ABORT


/*
 * plain contains an entire mime object.
 * Encrypt it and return an GpgmeData object with the encrypted version of
 * the file or NULL in case of error.
 */
static GpgmeData
pgp_encrypt ( GpgmeData plain, GpgmeRecipients rset )
{
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData cipher = NULL;

    err = gpgme_new (&ctx);
    if (!err)
	err = gpgme_data_new (&cipher);
    if (!err) {
        gpgme_set_armor (ctx, 1);
	err = gpgme_op_encrypt (ctx, rset, plain, cipher);
    }

    if (err) {
        debug_print ("encryption failed: %s\n", gpgme_strerror (err));
        gpgme_data_release (cipher);
        cipher = NULL;
    }
    else {
        debug_print ("** encryption succeeded\n");
    }

    gpgme_release (ctx);
    return cipher;
}

/*
 * Create and return a list of keys matching a key id
 */

GSList *rfc2015_create_signers_list (const char *keyid)
{
	GSList *key_list = NULL;
	GpgmeCtx list_ctx = NULL;
	GSList *p;
	GpgmeError err;
	GpgmeKey key;

	err = gpgme_new (&list_ctx);
	if (err)
		goto leave;
	err = gpgme_op_keylist_start (list_ctx, keyid, 1);
	if (err)
		goto leave;
	while ( !(err = gpgme_op_keylist_next (list_ctx, &key)) ) {
		key_list = g_slist_append (key_list, key);
	}
	if (err != GPGME_EOF)
		goto leave;
	err = 0;
	if (key_list == NULL) {
		debug_print ("no keys found for keyid \"%s\"\n", keyid);
	}

leave:
	if (err) {
		debug_print ("rfc2015_create_signers_list failed: %s\n", gpgme_strerror (err));
		for (p = key_list; p != NULL; p = p->next)
			gpgme_key_unref ((GpgmeKey) p->data);
		g_slist_free (key_list);
	}
	if (list_ctx)
		gpgme_release (list_ctx);
	return err ? NULL : key_list;
}

/*
 * Encrypt the file by extracting all recipients and finding the
 * encryption keys for all of them.  The file content is then replaced
 * by the encrypted one.  */
int
rfc2015_encrypt (const char *file, GSList *recp_list, gboolean ascii_armored)
{
    FILE *fp = NULL;
    char buf[BUFFSIZE];
    int i, clineidx, saved_last;
    char *clines[3] = {NULL};
    GpgmeError err;
    GpgmeData header = NULL;
    GpgmeData plain = NULL;
    GpgmeData cipher = NULL;
    GpgmeRecipients rset = NULL;
    size_t nread;
    int mime_version_seen = 0;
    char *boundary;

    boundary = generate_mime_boundary ("Encrypt");

    /* Create the list of recipients */
    rset = gpgmegtk_recipient_selection (recp_list);
    if (!rset) {
        debug_print ("error creating recipient list\n" );
        goto failure;
    }

    /* Open the source file */
    if ((fp = fopen(file, "rb")) == NULL) {
        FILE_OP_ERROR(file, "fopen");
        goto failure;
    }

    err = gpgme_data_new (&header);
    if (!err)
	err = gpgme_data_new (&plain);
    if (err) {
        debug_print ("gpgme_data_new failed: %s\n", gpgme_strerror (err));
        goto failure;
    }

    /* get the content header lines from the source */
    clineidx = 0;
    saved_last = 0;
    while (!err && fgets(buf, sizeof(buf), fp)) {
        /* fixme: check for overlong lines */
        if (headerp (buf, content_names)) {
            if (clineidx >= DIM (clines)) {
                debug_print ("rfc2015_encrypt: too many content lines\n");
                goto failure;
            }
            clines[clineidx++] = g_strdup (buf);
            saved_last = 1;
            continue;
        }
        if (saved_last) {
            if (*buf == ' ' || *buf == '\t') {
                char *last = clines[clineidx - 1];
                clines[clineidx - 1] = g_strconcat (last, buf, NULL);
                g_free (last);
                continue;
            }
            saved_last = 0;
        }

        if (headerp (buf, mime_version_name)) 
            mime_version_seen = 1;

        if (buf[0] == '\r' || buf[0] == '\n')
            break;
        err = gpgme_data_write (header, buf, strlen (buf));
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fgets");
        goto failure;
    }

    /* write them to the temp data and add the rest of the message */
    for (i = 0; !err && i < clineidx; i++) {
        debug_print ("%% %s:%d: cline=`%s'", __FILE__ ,__LINE__, clines[i]);
        err = gpgme_data_write (plain, clines[i], strlen (clines[i]));
    }
    if (!err)
        err = gpgme_data_write (plain, "\r\n", 2);
    while (!err && fgets(buf, sizeof(buf), fp)) {
        err = gpgme_data_write (plain, buf, strlen (buf));
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fgets");
        goto failure;
    }
    if (err) {
        debug_print ("gpgme_data_write failed: %s\n", gpgme_strerror (err));
        goto failure;
    }

    cipher = pgp_encrypt (plain, rset);
    gpgme_data_release (plain); plain = NULL;
    gpgme_recipients_release (rset); rset = NULL;
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
    if ((fp = fopen(file, "wb")) == NULL) {
        FILE_OP_ERROR(file, "fopen");
        goto failure;
    }

    /* Write the header, append new content lines, part 1 and part 2 header */
    err = gpgme_data_rewind (header);
    if (err) {
        debug_print ("gpgme_data_rewind failed: %s\n", gpgme_strerror (err));
        goto failure;
    }
    while (!(err = gpgme_data_read (header, buf, BUFFSIZE, &nread))) {
        fwrite (buf, nread, 1, fp);
    }
    if (err != GPGME_EOF) {
        debug_print ("gpgme_data_read failed: %s\n", gpgme_strerror (err));
        goto failure;
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fwrite");
        goto failure;
    }
    gpgme_data_release (header); header = NULL;
    
    if (!mime_version_seen) 
        fputs ("MIME-Version: 1\r\n", fp);

    if (ascii_armored) {
        fprintf(fp, 
            "Content-Type: text/plain; charset=US-ASCII\r\n"
            "Content-Transfer-Encoding: 7bit\r\n"  
            "\r\n");
    } else {
        fprintf (fp,
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
    }

    /* append the encrypted stuff */
    err = gpgme_data_rewind (cipher);
    if (err) {
        debug_print ("** gpgme_data_rewind on cipher failed: %s\n",
                   gpgme_strerror (err));
        goto failure;
    }

    while (!(err = gpgme_data_read (cipher, buf, BUFFSIZE, &nread))) {
        fwrite (buf, nread, 1, fp);
    }
    if (err != GPGME_EOF) {
        debug_print ("** gpgme_data_read failed: %s\n", gpgme_strerror (err));
        goto failure;
    }

    /* and the final boundary */
    if (!ascii_armored) {
        fprintf (fp,
		 "\r\n"
		 "--%s--\r\n",
	         boundary);
    }
    fflush (fp);
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fwrite");
        goto failure;
    }
    fclose (fp);
    gpgme_data_release (cipher);
    return 0;

failure:
    if (fp) 
        fclose (fp);
    gpgme_data_release (header);
    gpgme_data_release (plain);
    gpgme_data_release (cipher);
    gpgme_recipients_release (rset);
    g_free (boundary);
    return -1; /* error */
}

/* 
 * plain contains an entire mime object.  Sign it and return an
 * GpgmeData object with the signature of it or NULL in case of error.
 * r_siginfo returns an XML object with information about the signature.
 */
static GpgmeData
pgp_sign (GpgmeData plain, GSList *key_list, gboolean clearsign,
	  char **r_siginfo)
{
    GSList *p;
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData sig = NULL;
    struct passphrase_cb_info_s info;

    *r_siginfo = NULL;
    memset (&info, 0, sizeof info);

    err = gpgme_new (&ctx);
    if (err)
        goto leave;
    err = gpgme_data_new (&sig);
    if (err)
        goto leave;

    if (!getenv("GPG_AGENT_INFO")) {
        info.c = ctx;
        gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
    }
    gpgme_set_textmode (ctx, 1);
    gpgme_set_armor (ctx, 1);
    gpgme_signers_clear (ctx);
    for (p = key_list; p != NULL; p = p->next) {
	err = gpgme_signers_add (ctx, (GpgmeKey) p->data);
	if (err)
	    goto leave;
    }
    for (p = key_list; p != NULL; p = p->next)
	gpgme_key_unref ((GpgmeKey) p->data);
    g_slist_free (key_list);

    if (err)
	goto leave;
    err = gpgme_op_sign
	(ctx, plain, sig,
	 clearsign ? GPGME_SIG_MODE_CLEAR : GPGME_SIG_MODE_DETACH);
    if (!err)
        *r_siginfo = gpgme_get_op_info (ctx, 0);

leave:
    if (err) {
        gpgmegtk_free_passphrase();
        debug_print ("signing failed: %s\n", gpgme_strerror (err));
        gpgme_data_release (sig);
        sig = NULL;
    }
    else {
        debug_print ("signing succeeded\n");
    }

    gpgme_release (ctx);
    return sig;
}

/*
 * Find TAG in XML and return a pointer into xml set just behind the
 * closing angle.  Return NULL if not found. 
 */
static const char *
find_xml_tag (const char *xml, const char *tag)
{
    int taglen = strlen (tag);
    const char *s = xml;
 
    while ( (s = strchr (s, '<')) ) {
        s++;
        if (!strncmp (s, tag, taglen)) {
            const char *s2 = s + taglen;
            if (*s2 == '>' || isspace (*(const unsigned char*)s2) ) {
                /* found */
                while (*s2 && *s2 != '>') /* skip attributes */
                    s2++;
                /* fixme: do need to handle angles inside attribute vallues? */
                return *s2? (s2+1):NULL;
            }
        }
        while (*s && *s != '>') /* skip to end of tag */
            s++;
    }
    return NULL;
}


/*
 * Extract the micalg from an GnupgOperationInfo XML container.
 */
static char *
extract_micalg (char *xml)
{
    const char *s;

    s = find_xml_tag (xml, "GnupgOperationInfo");
    if (s) {
        const char *s_end = find_xml_tag (s, "/GnupgOperationInfo");
        s = find_xml_tag (s, "signature");
        if (s && s_end && s < s_end) {
            const char *s_end2 = find_xml_tag (s, "/signature");
            if (s_end2 && s_end2 < s_end) {
                s = find_xml_tag (s, "micalg");
                if (s && s < s_end2) {
                    s_end = strchr (s, '<');
                    if (s_end) {
                        char *p = g_malloc (s_end - s + 1);
                        memcpy (p, s, s_end - s);
                        p[s_end-s] = 0;
                        return p;
                    }
                }
            }
        }
    }
    return NULL;
}


/*
 * Sign the file and replace its content with the signed one.
 */
int
rfc2015_sign (const char *file, GSList *key_list)
{
    FILE *fp = NULL;
    char buf[BUFFSIZE];
    int i, clineidx, saved_last;
    char *clines[3] = {NULL};
    GpgmeError err;
    GpgmeData header = NULL;
    GpgmeData plain = NULL;
    GpgmeData sigdata = NULL;
    size_t nread;
    int mime_version_seen = 0;
    char *boundary;
    char *micalg = NULL;
    char *siginfo;

    boundary = generate_mime_boundary ("Signature");

    /* Open the source file */
    if ((fp = fopen(file, "rb")) == NULL) {
        FILE_OP_ERROR(file, "fopen");
        goto failure;
    }

    err = gpgme_data_new (&header);
    if (!err)
        err = gpgme_data_new (&plain);
    if (err) {
        debug_print ("gpgme_data_new failed: %s\n", gpgme_strerror (err));
        goto failure;
    }

    /* get the content header lines from the source */
    clineidx = 0;
    saved_last = 0;
    while (!err && fgets(buf, sizeof(buf), fp)) {
        /* fixme: check for overlong lines */
        if (headerp (buf, content_names)) {
            if (clineidx >= DIM (clines)) {
                debug_print ("rfc2015_sign: too many content lines\n");
                goto failure;
            }
            clines[clineidx++] = g_strdup (buf);
            saved_last = 1;
            continue;
        }
        if (saved_last) {
            if (*buf == ' ' || *buf == '\t') {
                char *last = clines[clineidx - 1];
                clines[clineidx - 1] = g_strconcat (last, buf, NULL);
                g_free (last);
                continue;
            }
            saved_last = 0;
        }

        if (headerp (buf, mime_version_name)) 
            mime_version_seen = 1;

        if (buf[0] == '\r' || buf[0] == '\n')
            break;
        err = gpgme_data_write (header, buf, strlen (buf));
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fgets");
        goto failure;
    }

    /* write them to the temp data and add the rest of the message */
    for (i = 0; !err && i < clineidx; i++) {
        err = gpgme_data_write (plain, clines[i], strlen (clines[i]));
    }
    if (!err)
        err = gpgme_data_write (plain, "\r\n", 2 );
    while (!err && fgets(buf, sizeof(buf), fp)) {
        err = gpgme_data_write (plain, buf, strlen (buf));
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fgets");
        goto failure;
    }
    if (err) {
        debug_print ("gpgme_data_write failed: %s\n", gpgme_strerror (err));
        goto failure;
    }

    sigdata = pgp_sign (plain, key_list, FALSE, &siginfo); 
    if (siginfo) {
	micalg = extract_micalg (siginfo);
	free (siginfo);
    }
    if (!sigdata) 
        goto failure;

    /* we have the signed message available in sigdata and now we are
     * going to rewrite the original file. To be sure that file has
     * been truncated we use an approach which should work everywhere:
     * close the file and then reopen it for writing. */
    if (fclose (fp)) {
        FILE_OP_ERROR(file, "fclose");
        goto failure;
    }
    if ((fp = fopen(file, "wb")) == NULL) {
        FILE_OP_ERROR(file, "fopen");
        goto failure;
    }

    /* Write the rfc822 header and add new content lines */
    err = gpgme_data_rewind (header);
    if (err)
        debug_print ("gpgme_data_rewind failed: %s\n", gpgme_strerror (err));
    while (!(err = gpgme_data_read (header, buf, BUFFSIZE, &nread))) {
        fwrite (buf, nread, 1, fp);
    }
    if (err != GPGME_EOF) {
        debug_print ("gpgme_data_read failed: %s\n", gpgme_strerror (err));
        goto failure;
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fwrite");
        goto failure;
    }
    gpgme_data_release (header);
    header = NULL;

    if (!mime_version_seen) 
        fputs ("MIME-Version: 1.0\r\n", fp);
    fprintf (fp, "Content-Type: multipart/signed; "
             "protocol=\"application/pgp-signature\";\r\n");
    if (micalg)
        fprintf (fp, " micalg=\"%s\";\r\n", micalg);
    fprintf (fp, " boundary=\"%s\"\r\n", boundary);

    /* Part 1: signed material */
    fprintf (fp, "\r\n"
                 "--%s\r\n",
                 boundary);
    err = gpgme_data_rewind (plain);
    if (err) {
        debug_print ("gpgme_data_rewind on plain failed: %s\n",
                   gpgme_strerror (err));
        goto failure;
    }
    while (!(err = gpgme_data_read (plain, buf, BUFFSIZE, &nread))) {
        fwrite (buf, nread, 1, fp);   
    }
    if (err != GPGME_EOF) {
        debug_print ("gpgme_data_read failed: %s\n", gpgme_strerror (err));
        goto failure;
    }

    /* Part 2: signature */
    fprintf (fp, "\r\n"
                 "--%s\r\n",
                 boundary);
    fputs ("Content-Type: application/pgp-signature\r\n"
	   "\r\n", fp);

    err = gpgme_data_rewind (sigdata);
    if (err) {
        debug_print ("gpgme_data_rewind on sigdata failed: %s\n",
                   gpgme_strerror (err));
        goto failure;
    }

    while (!(err = gpgme_data_read (sigdata, buf, BUFFSIZE, &nread))) {
        fwrite (buf, nread, 1, fp);
    }
    if (err != GPGME_EOF) {
        debug_print ("gpgme_data_read failed: %s\n", gpgme_strerror (err));
        goto failure;
    }

    /* Final boundary */
    fprintf (fp, "\r\n"
                 "--%s--\r\n",
                 boundary);
    fflush (fp);
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fwrite");
        goto failure;
    }
    fclose (fp);
    gpgme_data_release (header);
    gpgme_data_release (plain);
    gpgme_data_release (sigdata);
    g_free (boundary);
    g_free (micalg);
    return 0;

failure:
    if (fp) 
        fclose (fp);
    gpgme_data_release (header);
    gpgme_data_release (plain);
    gpgme_data_release (sigdata);
    g_free (boundary);
    g_free (micalg);
    return -1; /* error */
}


/*
 * Sign the file with clear text and replace its content with the signed one.
 */
gint
rfc2015_clearsign (const gchar *file, GSList *key_list)
{
    FILE *fp;
    gchar buf[BUFFSIZE];
    GpgmeError err;
    GpgmeData text = NULL;
    GpgmeData sigdata = NULL;
    size_t nread;
    gchar *siginfo;

    if ((fp = fopen(file, "rb")) == NULL) {
	FILE_OP_ERROR(file, "fopen");
	goto failure;
    }

    err = gpgme_data_new(&text);
    if (err) {
	debug_print("gpgme_data_new failed: %s\n", gpgme_strerror(err));
	goto failure;
    }

    while (!err && fgets(buf, sizeof(buf), fp)) {
	err = gpgme_data_write(text, buf, strlen(buf));
    }
    if (ferror(fp)) {
	FILE_OP_ERROR(file, "fgets");
	goto failure;
    }
    if (err) {
	debug_print("gpgme_data_write failed: %s\n", gpgme_strerror(err));
	goto failure;
    }

    sigdata = pgp_sign(text, key_list, TRUE, &siginfo);
    if (siginfo) {
	g_free(siginfo);
    }
    if (!sigdata)
	goto failure;

    if (fclose(fp) == EOF) {
	FILE_OP_ERROR(file, "fclose");
	fp = NULL;
	goto failure;
    }
    if ((fp = fopen(file, "wb")) == NULL) {
	FILE_OP_ERROR(file, "fopen");
	goto failure;
    }

    err = gpgme_data_rewind(sigdata);
    if (err) {
	debug_print("gpgme_data_rewind on sigdata failed: %s\n",
		    gpgme_strerror(err));
	goto failure;
    }

    while (!(err = gpgme_data_read(sigdata, buf, sizeof(buf), &nread))) {
	fwrite(buf, nread, 1, fp);
    }
    if (err != GPGME_EOF) {
	debug_print("gpgme_data_read failed: %s\n", gpgme_strerror(err));
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

#endif /* USE_GPGME */
