/*
 * Attachment Tool Plug-in -- an attachment processing plug-in for Sylpheed
 * Copyright (C) 2010 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <stdio.h>
#include <sys/stat.h>

#include "sylmain.h"
#include "plugin.h"
#include "procmsg.h"
#include "procmime.h"
#include "utils.h"

#include <glib/gi18n-lib.h>

static SylPluginInfo info = {
	"Attachment Tool Plug-in",
	"0.1.0",
	"Hiroyuki Yamamoto",
	"Attachment processing plug-in for Sylpheed"
};

static void remove_attachment_cb(void);

void plugin_load(void)
{
	debug_print("initializing attachment_tool plug-in\n");

	syl_plugin_add_menuitem("/Tools", NULL, NULL, NULL);
	syl_plugin_add_menuitem("/Tools", _("Remove attachments"), remove_attachment_cb, NULL);

	debug_print("attachment_tool plug-in loading done.\n");
}

void plugin_unload(void)
{
	debug_print("attachment_tool plug-in unloaded.\n");
}

SylPluginInfo *plugin_info(void)
{
	return &info;
}

gint plugin_interface_version(void)
{
	return SYL_PLUGIN_INTERFACE_VERSION;
}

static gboolean remove_attachment_rec(MimeInfo *partinfo, FILE *fp, FILE *outfp)
{
	gchar buf[BUFFSIZE];
	gchar *boundary = NULL;
	gint boundary_len = 0;
	gboolean err = FALSE;

	while (partinfo) {
		gboolean is_attach_part = FALSE;
		gint count;

		debug_print("------------------------------------------------- begin %s\n", partinfo->content_type);
		debug_print("fpos = %ld\n", partinfo->fpos);
		debug_print("content_type = %s\n", partinfo->content_type);

		if (partinfo->filename || partinfo->name) {
			is_attach_part = TRUE;
			debug_print("skip this body\n");
		}

		if (fseek(fp, partinfo->fpos, SEEK_SET) < 0) {
			perror("fseek");
			err = TRUE;
			break;
		}
		/* write part headers */
		count = 0;
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			count++;
			fputs(buf, outfp);
			if (buf[0] == '\r' || buf[0] == '\n') break;
		}
		debug_print("wrote header: %d lines\n", count);

		/* write until first boundary */
		count = 0;
		if (partinfo->children && partinfo->boundary) {
			boundary = partinfo->boundary;
			boundary_len = strlen(boundary);
			debug_print("write until %s\n", boundary);
			while (fgets(buf, sizeof(buf), fp) != NULL) {
				count++;
				if (IS_BOUNDARY(buf, boundary, boundary_len)) {
					fputs(buf, outfp);
					debug_print("start boundary: %s", buf);
					break;
				} else {
					fputs(buf, outfp);
				}
			}
			debug_print("wrote %d lines\n", count);
		}

		if (partinfo->sub) {
			debug_print("enter rfc822 -----------------------------\n");
			err = !remove_attachment_rec(partinfo->sub, fp, outfp);
			debug_print("leave rfc822 -----------------------------\n");
			if (err)
				break;
		} else if (partinfo->children) {
			debug_print("enter child -----------------------------\n");
			err = !remove_attachment_rec
				(partinfo->children, fp, outfp);
			debug_print("leave child -----------------------------\n");
			if (err)
				break;
		}

		if (partinfo->parent && partinfo->parent->boundary) {
			boundary = partinfo->parent->boundary;
			boundary_len = strlen(boundary);
			debug_print("boundary = %s\n", boundary);
		} else {
			boundary = NULL;
			boundary_len = 0;
		}

		/* write (or skip) part body and end boundary */
		if (boundary && !partinfo->main) {
			count = 0;
			debug_print("write until %s\n", boundary);
			while (fgets(buf, sizeof(buf), fp) != NULL) {
				if (IS_BOUNDARY(buf, boundary, boundary_len)) {
					count++;
					fputs(buf, outfp);
					debug_print("end boundary: %s", buf);
					break;
				} else {
					if (!is_attach_part)
						count++;
					if (!is_attach_part)
						fputs(buf, outfp);
				}
			}
			debug_print("wrote body: %d lines\n", count);
		}

		debug_print("------------------------------------------------- end   %s\n", partinfo->content_type);

		partinfo = partinfo->next;
	}

	return !err;
}

static gboolean has_attach_part(MimeInfo *mimeinfo)
{
	for (; mimeinfo != NULL; mimeinfo = procmime_mimeinfo_next(mimeinfo)) {
		if (mimeinfo->filename || mimeinfo->name)
			return TRUE;
	}

	return FALSE;
}

static gboolean remove_attachment(MsgInfo *msginfo)
{
	MimeInfo *mimeinfo;
	FILE *fp, *outfp;
	gchar *infile, *outfile;
	gboolean err = FALSE;

	mimeinfo = procmime_scan_message(msginfo);
	g_return_val_if_fail(mimeinfo != NULL, FALSE);

	if (!has_attach_part(mimeinfo)) {
		debug_print("remove_attachment: this message doesn't have attachments\n");
		procmime_mimeinfo_free_all(mimeinfo);
		return TRUE;
	}

	if ((fp = procmsg_open_message(msginfo)) == NULL) {
		procmime_mimeinfo_free_all(mimeinfo);
		return FALSE;
	}

	infile = procmsg_get_message_file(msginfo);
	outfile = g_strconcat(infile, ".tmp", NULL);
	debug_print("infile: %s\n", infile);
	debug_print("outfile: %s\n", outfile);

	if ((outfp = g_fopen(outfile, "wb")) == NULL) {
		g_free(outfile);
		g_free(infile);
		fclose(fp);
		procmime_mimeinfo_free_all(mimeinfo);
		return FALSE;
	}

	err = !remove_attachment_rec(mimeinfo, fp, outfp);

	if (fclose(outfp) == EOF) {
		FILE_OP_ERROR(outfile, "fclose");
		err = TRUE;
	}

	fclose(fp);

	if (!err) {
		debug_print("overwriting original message file: %s\n", infile);
		if (copy_file(outfile, infile, FALSE) == 0) {
			GStatBuf s;
			if (g_stat(infile, &s) == 0) {
				msginfo->size = s.st_size;
				msginfo->mtime = s.st_mtime;
			}
			if (msginfo->folder)
				msginfo->folder->cache_dirty = TRUE;
		} else
			err = TRUE;
	}

	g_unlink(outfile);
	g_free(outfile);
	g_free(infile);
	procmime_mimeinfo_free_all(mimeinfo);

	return !err;
}

static void remove_attachment_cb(void)
{
	FolderItem *item;
	GSList *mlist, *cur;
	gint val;

	debug_print("remove_attachment_cb\n");

	if (syl_plugin_summary_is_locked())
		return;

	item = syl_plugin_summary_get_current_folder();
	if (!item || !item->folder || FOLDER_TYPE(item->folder) != F_MH ||
	    item->stype == F_VIRTUAL) {
		syl_plugin_alertpanel_message(_("Error"), _("This tool is available on local folders only."), 3);
		return;
	}

	mlist = syl_plugin_summary_get_selected_msg_list();
	if (!mlist)
		return;

	val = syl_plugin_alertpanel(_("Remove attachments"),
				    _("Do you really remove attached files from the selected messages?"),
				    GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	if (val != 0) {
		g_slist_free(mlist);
		return;
	}

	for (cur = mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;

		if (!msginfo)
			continue;
		if (!MSG_IS_MIME(msginfo->flags)) {
			debug_print("message %u is not multi-part. skipping.\n", msginfo->msgnum);
			continue;
		}
		debug_print("Removing attachments of message: %u: %s\n", msginfo->msgnum, msginfo->subject ? msginfo->subject : "(No Subject)");

		if (!remove_attachment(msginfo)) {
			g_warning("Cannot remove attachments from %u: %s\n", msginfo->msgnum, msginfo->subject ? msginfo->subject : "(No Subject)");
			break;
		}

		syl_plugin_summary_update_by_msgnum(msginfo->msgnum);
	}

	g_slist_free(mlist);

	syl_plugin_summary_redisplay_msg();
}
