/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#include <ctype.h>
#include <time.h>

#ifdef HAVE_LOCKFILE_H
#  include <lockfile.h>
#endif

#include "mbox.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "filter.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "account.h"
#include "utils.h"

#define FPUTS_TO_TMP_ABORT_IF_FAIL(s) \
{ \
	if (fputs(s, tmp_fp) == EOF) { \
		g_warning(_("can't write to temporary file\n")); \
		fclose(tmp_fp); \
		fclose(mbox_fp); \
		g_unlink(tmp_file); \
		g_free(tmp_file); \
		return -1; \
	} \
}

gint proc_mbox(FolderItem *dest, const gchar *mbox, GHashTable *folder_table)
{
	return proc_mbox_full(dest, mbox, folder_table,
			      folder_table ? TRUE : FALSE,
			      folder_table && prefs_common.enable_junk &&
			      prefs_common.filter_junk_on_recv ? TRUE : FALSE);
}

gint proc_mbox_full(FolderItem *dest, const gchar *mbox,
		    GHashTable *folder_table, gboolean apply_filter,
		    gboolean filter_junk)
{
	FILE *mbox_fp;
	gchar buf[BUFFSIZE], from_line[BUFFSIZE];
	gchar *tmp_file;
	gint new_msgs = 0;
	guint count = 0;
	Folder *folder;
	FilterRule *junk_rule = NULL;
	GSList junk_fltlist = {NULL, NULL};
	FolderItem *junk;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(dest->folder != NULL, -1);
	g_return_val_if_fail(mbox != NULL, -1);

	debug_print(_("Getting messages from %s into %s...\n"), mbox, dest->path);

	folder = dest->folder;

	if ((mbox_fp = g_fopen(mbox, "rb")) == NULL) {
		FILE_OP_ERROR(mbox, "fopen");
		return -1;
	}

	/* ignore empty lines on the head */
	do {
		if (fgets(buf, sizeof(buf), mbox_fp) == NULL) {
			g_warning(_("can't read mbox file.\n"));
			fclose(mbox_fp);
			return -1;
		}
	} while (buf[0] == '\n' || buf[0] == '\r');

	if (strncmp(buf, "From ", 5) != 0) {
		g_warning(_("invalid mbox format: %s\n"), mbox);
		fclose(mbox_fp);
		return -1;
	}

	strcpy(from_line, buf);
	if (fgets(buf, sizeof(buf), mbox_fp) == NULL) {
		g_warning(_("malformed mbox: %s\n"), mbox);
		fclose(mbox_fp);
		return -1;
	}

	tmp_file = get_tmp_file();

	if (filter_junk) {
		junk = folder_get_junk(folder);
		junk_rule = filter_junk_rule_create(NULL, junk, FALSE);
		junk_fltlist.data = junk_rule;
	}

	do {
		FILE *tmp_fp;
		GSList *cur;
		gchar *startp, *endp, *rpath;
		gint empty_line;
		gboolean is_next_msg = FALSE;
		gboolean is_junk = FALSE;
		FilterInfo *fltinfo;
		MsgInfo *msginfo;

		count++;
		if (folder->ui_func)
			folder->ui_func(folder, dest, folder->ui_func_data ? dest->folder->ui_func_data : GUINT_TO_POINTER(count));
		if (folder_call_ui_func2(folder, dest, count, 0) == FALSE) {
			debug_print("Import of mbox cancelled at %u\n", count);
			break;
		}

		if ((tmp_fp = g_fopen(tmp_file, "wb")) == NULL) {
			FILE_OP_ERROR(tmp_file, "fopen");
			g_warning(_("can't open temporary file\n"));
			filter_rule_free(junk_rule);
			g_free(tmp_file);
			fclose(mbox_fp);
			return -1;
		}
		if (change_file_mode_rw(tmp_fp, tmp_file) < 0)
			FILE_OP_ERROR(tmp_file, "chmod");

		/* convert unix From into Return-Path */
		startp = from_line + 5;
		endp = strchr(startp, ' ');
		if (endp == NULL)
			rpath = g_strdup(startp);
		else
			rpath = g_strndup(startp, endp - startp);
		g_strstrip(rpath);
		g_snprintf(from_line, sizeof(from_line),
			   "Return-Path: %s\n", rpath);
		g_free(rpath);

		FPUTS_TO_TMP_ABORT_IF_FAIL(from_line);
		FPUTS_TO_TMP_ABORT_IF_FAIL(buf);
		from_line[0] = '\0';

		empty_line = 0;

		while (fgets(buf, sizeof(buf), mbox_fp) != NULL) {
			if (buf[0] == '\n' || buf[0] == '\r') {
				empty_line++;
				buf[0] = '\0';
				continue;
			}

			/* From separator */
			while (!strncmp(buf, "From ", 5)) {
				strcpy(from_line, buf);
				if (fgets(buf, sizeof(buf), mbox_fp) == NULL) {
					buf[0] = '\0';
					break;
				}

				if (is_header_line(buf)) {
					is_next_msg = TRUE;
					break;
				} else if (!strncmp(buf, "From ", 5)) {
					continue;
				} else if (!strncmp(buf, ">From ", 6)) {
					g_memmove(buf, buf + 1, strlen(buf));
					is_next_msg = TRUE;
					break;
				} else {
					g_warning(_("unescaped From found:\n%s"),
						  from_line);
					break;
				}
			}
			if (is_next_msg) break;

			if (empty_line > 0) {
				while (empty_line--)
					FPUTS_TO_TMP_ABORT_IF_FAIL("\n");
				empty_line = 0;
			}

			if (from_line[0] != '\0') {
				FPUTS_TO_TMP_ABORT_IF_FAIL(from_line);
				from_line[0] = '\0';
			}

			if (buf[0] != '\0') {
				if (!strncmp(buf, ">From ", 6)) {
					FPUTS_TO_TMP_ABORT_IF_FAIL(buf + 1);
				} else
					FPUTS_TO_TMP_ABORT_IF_FAIL(buf);

				buf[0] = '\0';
			}
		}

		if (empty_line > 0) {
			while (--empty_line)
				FPUTS_TO_TMP_ABORT_IF_FAIL("\n");
		}

		if (fclose(tmp_fp) == EOF) {
			FILE_OP_ERROR(tmp_file, "fclose");
			g_warning(_("can't write to temporary file\n"));
			filter_rule_free(junk_rule);
			g_unlink(tmp_file);
			g_free(tmp_file);
			fclose(mbox_fp);
			return -1;
		}

		fltinfo = filter_info_new();
		fltinfo->flags.perm_flags = MSG_NEW|MSG_UNREAD;
		fltinfo->flags.tmp_flags = MSG_RECEIVED;

		msginfo = procheader_parse_file(tmp_file, fltinfo->flags,
						FALSE);
		if (!msginfo) {
			g_warning("proc_mbox_full: procheader_parse_file failed");
			filter_info_free(fltinfo);
			filter_rule_free(junk_rule);
			g_unlink(tmp_file);
			g_free(tmp_file);
			fclose(mbox_fp);
			return -1;
		}
		fltinfo->flags = msginfo->flags;
		msginfo->file_path = g_strdup(tmp_file);

		if (filter_junk && prefs_common.enable_junk &&
		    prefs_common.filter_junk_before && junk_rule) {
			filter_apply_msginfo(&junk_fltlist, msginfo, fltinfo);
			if (fltinfo->drop_done)
				is_junk = TRUE;
		}

		if (!fltinfo->drop_done && apply_filter)
			filter_apply_msginfo(prefs_common.fltlist, msginfo,
					     fltinfo);

		if (!fltinfo->drop_done &&
		    filter_junk && prefs_common.enable_junk &&
		    !prefs_common.filter_junk_before && junk_rule) {
			filter_apply_msginfo(&junk_fltlist, msginfo, fltinfo);
			if (fltinfo->drop_done)
				is_junk = TRUE;
		}

		if (fltinfo->actions[FLT_ACTION_MOVE] == FALSE &&
		    fltinfo->actions[FLT_ACTION_DELETE] == FALSE) {
			msginfo->flags = fltinfo->flags;
			if (folder_item_add_msg_msginfo(dest, msginfo, FALSE) < 0) {
				procmsg_msginfo_free(msginfo);
				filter_info_free(fltinfo);
				filter_rule_free(junk_rule);
				g_unlink(tmp_file);
				g_free(tmp_file);
				fclose(mbox_fp);
				return -1;
			}
			fltinfo->dest_list = g_slist_append(fltinfo->dest_list,
							    dest);
		}

		for (cur = fltinfo->dest_list; cur != NULL; cur = cur->next) {
			FolderItem *drop_folder = (FolderItem *)cur->data;
			gint val = 0;

			if (folder_table) {
				val = GPOINTER_TO_INT(g_hash_table_lookup
						      (folder_table,
						       drop_folder));
			}
			if (val == 0) {
				if (folder_table) {
					g_hash_table_insert(folder_table,
							    drop_folder,
							    GINT_TO_POINTER(1));
				}
			}
		}

		if (!is_junk &&
		    fltinfo->actions[FLT_ACTION_DELETE] == FALSE &&
		    fltinfo->actions[FLT_ACTION_MARK_READ] == FALSE)
			new_msgs++;

		procmsg_msginfo_free(msginfo);
		filter_info_free(fltinfo);
		g_unlink(tmp_file);
	} while (from_line[0] != '\0');

	if (junk_rule)
		filter_rule_free(junk_rule);

	g_free(tmp_file);
	fclose(mbox_fp);
	debug_print("%d new messages found.\n", new_msgs);

	return new_msgs;
}

gint lock_mbox(const gchar *base, LockType type)
{
#ifdef G_OS_UNIX
	gint retval = 0;

	if (type == LOCK_FILE) {
#if HAVE_LIBLOCKFILE
		gchar *lockfile;

		lockfile = g_strconcat(base, ".lock", NULL);
		if (lockfile_create(lockfile, 0, L_PID) != L_SUCCESS) {
			FILE_OP_ERROR(lockfile, "lockfile_create");
			g_free(lockfile);
			return -1;
		}
		g_free(lockfile);
#else
		gchar *lockfile, *locklink;
		gint retry = 0;
		FILE *lockfp;

		lockfile = g_strdup_printf("%s.%d", base, getpid());
		if ((lockfp = g_fopen(lockfile, "wb")) == NULL) {
			FILE_OP_ERROR(lockfile, "fopen");
			g_warning(_("can't create lock file %s\n"), lockfile);
			g_warning(_("use 'flock' instead of 'file' if possible.\n"));
			g_free(lockfile);
			return -1;
		}

		fprintf(lockfp, "%d\n", getpid());
		fclose(lockfp);

		locklink = g_strconcat(base, ".lock", NULL);
		while (link(lockfile, locklink) < 0) {
			FILE_OP_ERROR(lockfile, "link");
			if (retry >= 5) {
				g_warning(_("can't create %s\n"), lockfile);
				g_unlink(lockfile);
				g_free(lockfile);
				return -1;
			}
			if (retry == 0)
				g_warning(_("mailbox is owned by another"
					    " process, waiting...\n"));
			retry++;
			sleep(5);
		}
		g_unlink(lockfile);
		g_free(lockfile);
#endif /* HAVE_LIBLOCKFILE */
	} else if (type == LOCK_FLOCK) {
		gint lockfd;

		if ((lockfd = open(base, O_RDWR)) < 0) {
			FILE_OP_ERROR(base, "open");
			return -1;
		}
#if HAVE_LOCKF
		if (lockf(lockfd, F_TLOCK, 0) < 0) {
			perror("lockf");
#elif HAVE_FLOCK
		if (flock(lockfd, LOCK_EX|LOCK_NB) < 0) {
			perror("flock");
#else
		{
#endif /* HAVE_LOCKF */
			g_warning(_("can't lock %s\n"), base);
			if (close(lockfd) < 0)
				perror("close");
			return -1;
		}
		retval = lockfd;
	} else {
		g_warning(_("invalid lock type\n"));
		return -1;
	}

	return retval;
#else
	return -1;
#endif /* G_OS_UNIX */
}

gint unlock_mbox(const gchar *base, gint fd, LockType type)
{
	if (type == LOCK_FILE) {
		gchar *lockfile;

		lockfile = g_strconcat(base, ".lock", NULL);
#if HAVE_LIBLOCKFILE
		if (lockfile_remove(lockfile) != L_SUCCESS) {
			FILE_OP_ERROR(lockfile, "lockfile_remove");
#else
		if (g_unlink(lockfile) < 0) {
			FILE_OP_ERROR(lockfile, "unlink");
#endif /* HAVE_LIBLOCKFILE */
			g_free(lockfile);
			return -1;
		}
		g_free(lockfile);

		return 0;
	} else if (type == LOCK_FLOCK) {
#if HAVE_LOCKF
		if (lockf(fd, F_ULOCK, 0) < 0) {
			perror("lockf");
#elif HAVE_FLOCK
		if (flock(fd, LOCK_UN) < 0) {
			perror("flock");
#else
		{
#endif /* HAVE_LOCKF */
			g_warning(_("can't unlock %s\n"), base);
			if (close(fd) < 0)
				perror("close");
			return -1;
		}

		if (close(fd) < 0) {
			perror("close");
			return -1;
		}

		return 0;
	}

	g_warning(_("invalid lock type\n"));
	return -1;
}

gint copy_mbox(const gchar *src, const gchar *dest)
{
	return copy_file(src, dest, TRUE);
}

void empty_mbox(const gchar *mbox)
{
#if HAVE_TRUNCATE
	if (truncate(mbox, 0) < 0) {
#endif
		FILE *fp;

#if HAVE_TRUNCATE
		FILE_OP_ERROR(mbox, "truncate");
#endif
		if ((fp = g_fopen(mbox, "wb")) == NULL) {
			FILE_OP_ERROR(mbox, "fopen");
			g_warning(_("can't truncate mailbox to zero.\n"));
			return;
		}
		fclose(fp);
#if HAVE_TRUNCATE
	}
#endif
}

/* read all messages in SRC, and store them into one MBOX file. */
gint export_to_mbox(FolderItem *src, const gchar *mbox)
{
	GSList *mlist;
	gint ret = 0;

	mlist = folder_item_get_msg_list(src, TRUE);
	if (mlist) {
		ret = export_msgs_to_mbox(src, mlist, mbox);
		procmsg_msg_list_free(mlist);
	}

	return ret;
}

/* store MLIST into one MBOX file. */
gint export_msgs_to_mbox(FolderItem *src, GSList *mlist, const gchar *mbox)
{
	GSList *cur;
	MsgInfo *msginfo;
	FILE *msg_fp;
	FILE *mbox_fp;
	gchar buf[BUFFSIZE];
	PrefsAccount *cur_ac;
	guint count = 0, length;
	time_t date_t_;

	g_return_val_if_fail(src != NULL, -1);
	g_return_val_if_fail(src->folder != NULL, -1);
	g_return_val_if_fail(mlist != NULL, -1);
	g_return_val_if_fail(mbox != NULL, -1);

	debug_print(_("Exporting messages from %s into %s...\n"),
		    src->path, mbox);

	if ((mbox_fp = g_fopen(mbox, "wb")) == NULL) {
		FILE_OP_ERROR(mbox, "fopen");
		return -1;
	}

	cur_ac = account_get_current_account();

	length = g_slist_length(mlist);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		count++;
		if (src->folder->ui_func)
			src->folder->ui_func(src->folder, src, src->folder->ui_func_data ? src->folder->ui_func_data : GUINT_TO_POINTER(count));
		if (folder_call_ui_func2(src->folder, src, count, length) == FALSE) {
			debug_print("Export to mbox cancelled at %u/%u\n", count, length);
			break;
		}

		msg_fp = procmsg_open_message(msginfo);
		if (!msg_fp)
			continue;

		strncpy2(buf,
			 msginfo->from ? msginfo->from :
			 cur_ac && cur_ac->address ?
			 cur_ac->address : "unknown",
			 sizeof(buf));
		extract_address(buf);

		date_t_ = msginfo->date_t;
		fprintf(mbox_fp, "From %s %s", buf, ctime(&date_t_));

		while (fgets(buf, sizeof(buf), msg_fp) != NULL) {
			if (!strncmp(buf, "From ", 5))
				fputc('>', mbox_fp);
			fputs(buf, mbox_fp);
		}
		fputc('\n', mbox_fp);

		fclose(msg_fp);
	}

	fclose(mbox_fp);

	return 0;
}
