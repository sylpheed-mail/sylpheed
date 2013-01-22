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
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>

#include "news.h"
#include "nntp.h"
#include "socket.h"
#include "recv.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "session.h"
#include "codeconv.h"
#include "utils.h"
#include "prefs_common.h"
#include "prefs_account.h"
#if USE_SSL
#  include "ssl.h"
#endif
#include "socks.h"

#define NNTP_PORT	119
#if USE_SSL
#define NNTPS_PORT	563
#endif

static void news_folder_init		 (Folder	*folder,
					  const gchar	*name,
					  const gchar	*path);

static Folder	*news_folder_new	(const gchar	*name,
					 const gchar	*folder);
static void	 news_folder_destroy	(Folder		*folder);

static GSList *news_get_article_list	(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 use_cache);
static gchar *news_fetch_msg		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
static MsgInfo *news_get_msginfo	(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);

static gint news_close			(Folder		*folder,
					 FolderItem	*item);

static gint news_scan_group		(Folder		*folder,
					 FolderItem	*item);

#if USE_SSL
static Session *news_session_new	 (const gchar	*server,
					  gushort	 port,
					  SocksInfo	*socks_info,
					  const gchar	*userid,
					  const gchar	*passwd,
					  SSLType	 ssl_type);
#else
static Session *news_session_new	 (const gchar	*server,
					  gushort	 port,
					  SocksInfo	*socks_info,
					  const gchar	*userid,
					  const gchar	*passwd);
#endif

static gint news_get_article_cmd	 (NNTPSession	*session,
					  const gchar	*cmd,
					  gint		 num,
					  gchar		*filename);
static gint news_get_article		 (NNTPSession	*session,
					  gint		 num,
					  gchar		*filename);
#if 0
static gint news_get_header		 (NNTPSession	*session,
					  gint		 num,
					  gchar		*filename);
#endif

static gint news_select_group		 (NNTPSession	*session,
					  const gchar	*group,
					  gint		*num,
					  gint		*first,
					  gint		*last);
static GSList *news_get_uncached_articles(NNTPSession	*session,
					  FolderItem	*item,
					  gint		 cache_last,
					  gint		*rfirst,
					  gint		*rlast);
static MsgInfo *news_parse_xover	 (const gchar	*xover_str);
static gchar *news_parse_xhdr		 (const gchar	*xhdr_str,
					  MsgInfo	*msginfo);
static GSList *news_delete_old_articles	 (GSList	*alist,
					  FolderItem	*item,
					  gint		 first);
static void news_delete_all_articles	 (FolderItem	*item);
static void news_delete_expired_caches	 (GSList	*alist,
					  FolderItem	*item);

static FolderClass news_class =
{
	F_NEWS,

	news_folder_new,
	news_folder_destroy,

	NULL,
	NULL,

	news_get_article_list,
	NULL,
	news_fetch_msg,
	news_get_msginfo,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	news_close,
	news_scan_group,

	NULL,
	NULL,
	NULL,
	NULL
};


FolderClass *news_get_class(void)
{
	return &news_class;
}

static Folder *news_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(NewsFolder, 1);
	news_folder_init(folder, name, path);

	return folder;
}

static void news_folder_destroy(Folder *folder)
{
	if (REMOTE_FOLDER(folder)->remove_cache_on_destroy) {
		gchar *dir;
		gchar *server;

		dir = folder_get_path(folder);
		if (is_dir_exist(dir))
			remove_dir_recursive(dir);
		g_free(dir);

		server = uriencode_for_filename(folder->account->nntp_server);
		dir = g_strconcat(get_news_cache_dir(), G_DIR_SEPARATOR_S,
				  server, NULL);
		if (is_dir_exist(dir))
			g_rmdir(dir);
		g_free(dir);
		g_free(server);
	}

	folder_remote_folder_destroy(REMOTE_FOLDER(folder));
}

static void news_folder_init(Folder *folder, const gchar *name,
			     const gchar *path)
{
	folder->klass = news_get_class();
	folder_remote_folder_init(folder, name, path);
}

#if USE_SSL
static Session *news_session_new(const gchar *server, gushort port,
				 SocksInfo *socks_info,
				 const gchar *userid, const gchar *passwd,
				 SSLType ssl_type)
#else
static Session *news_session_new(const gchar *server, gushort port,
				 SocksInfo *socks_info,
				 const gchar *userid, const gchar *passwd)
#endif
{
	gchar buf[NNTPBUFSIZE];
	Session *session;

	g_return_val_if_fail(server != NULL, NULL);

	log_message(_("creating NNTP connection to %s:%d ...\n"), server, port);

#if USE_SSL
	session = nntp_session_new_full(server, port, socks_info, buf, userid, passwd, ssl_type);
#else
	session = nntp_session_new_full(server, port, socks_info, buf, userid, passwd);
#endif

	return session;
}

static Session *news_session_new_for_folder(Folder *folder)
{
	Session *session;
	PrefsAccount *ac;
	SocksInfo *socks_info = NULL;
	const gchar *userid = NULL;
	gchar *passwd = NULL;
	gushort port;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	ac = folder->account;
	if (ac->use_nntp_auth && ac->userid && ac->userid[0]) {
		userid = ac->userid;
		if (ac->passwd && ac->passwd[0])
			passwd = g_strdup(ac->passwd);
		else
			passwd = input_query_password(ac->nntp_server, userid);
	}

	if (ac->use_socks && ac->use_socks_for_recv && ac->proxy_host) {
		socks_info = socks_info_new(ac->socks_type, ac->proxy_host, ac->proxy_port, ac->use_proxy_auth ? ac->proxy_name : NULL, ac->use_proxy_auth ? ac->proxy_pass : NULL);
	}

#if USE_SSL
	port = ac->set_nntpport ? ac->nntpport
		: ac->ssl_nntp ? NNTPS_PORT : NNTP_PORT;
	session = news_session_new(ac->nntp_server, port, socks_info,
				   userid, passwd, ac->ssl_nntp);
#else
	port = ac->set_nntpport ? ac->nntpport : NNTP_PORT;
	session = news_session_new(ac->nntp_server, port, socks_info,
				   userid, passwd);
#endif

	if (socks_info)
		socks_info_free(socks_info);

	g_free(passwd);

	return session;
}

static NNTPSession *news_session_get(Folder *folder)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_TYPE(folder) == F_NEWS, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	if (!prefs_common.online_mode)
		return NULL;

	if (!rfolder->session) {
		rfolder->session = news_session_new_for_folder(folder);
		return NNTP_SESSION(rfolder->session);
	}

	if (time(NULL) - rfolder->session->last_access_time <
		SESSION_TIMEOUT_INTERVAL) {
		return NNTP_SESSION(rfolder->session);
	}

	if (nntp_mode(NNTP_SESSION(rfolder->session), FALSE)
	    != NN_SUCCESS) {
		log_warning(_("NNTP connection to %s:%d has been"
			      " disconnected. Reconnecting...\n"),
			    folder->account->nntp_server,
			    folder->account->set_nntpport ?
			    folder->account->nntpport : NNTP_PORT);
		session_destroy(rfolder->session);
		rfolder->session = news_session_new_for_folder(folder);
	}

	if (rfolder->session)
		session_set_access_time(rfolder->session);

	return NNTP_SESSION(rfolder->session);
}

static GSList *news_get_article_list(Folder *folder, FolderItem *item,
				     gboolean use_cache)
{
	GSList *alist;
	NNTPSession *session;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(FOLDER_TYPE(folder) == F_NEWS, NULL);

	session = news_session_get(folder);

	if (!session) {
		alist = procmsg_read_cache(item, FALSE);
		item->last_num = procmsg_get_last_num_in_msg_list(alist);
	} else if (use_cache) {
		GSList *newlist;
		gint cache_last;
		gint first, last;

		alist = procmsg_read_cache(item, FALSE);

		cache_last = procmsg_get_last_num_in_msg_list(alist);
		newlist = news_get_uncached_articles
			(session, item, cache_last, &first, &last);
		if (newlist)
			item->cache_dirty = TRUE;
		if (first == 0 && last == 0) {
			news_delete_all_articles(item);
			procmsg_msg_list_free(alist);
			alist = NULL;
			item->cache_dirty = TRUE;
		} else {
			alist = news_delete_old_articles(alist, item, first);
			news_delete_expired_caches(alist, item);
		}

		alist = g_slist_concat(alist, newlist);

		item->last_num = last;
	} else {
		gint last;

		alist = news_get_uncached_articles
			(session, item, 0, NULL, &last);
		news_delete_all_articles(item);
		item->last_num = last;
		item->cache_dirty = TRUE;
	}

	procmsg_set_flags(alist, item);

	alist = procmsg_sort_msg_list(alist, item->sort_key, item->sort_type);

	if (item->mark_queue)
		item->mark_dirty = TRUE;

	debug_print("cache_dirty: %d, mark_dirty: %d\n",
		    item->cache_dirty, item->mark_dirty);

	if (!item->opened) {
		if (item->cache_dirty)
			procmsg_write_cache_list(item, alist);
		if (item->mark_dirty)
			procmsg_write_flags_list(item, alist);
	}

	return alist;
}

static gchar *news_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path, *filename;
	NNTPSession *session;
	gchar nstr[16];
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, utos_buf(nstr, num),
			       NULL);
	g_free(path);

	if (is_file_exist(filename) && get_file_size(filename) > 0) {
		debug_print(_("article %d has been already cached.\n"), num);
		return filename;
	}

	session = news_session_get(folder);
	if (!session) {
		g_free(filename);
		return NULL;
	}

	ok = news_select_group(session, item->path, NULL, NULL, NULL);
	if (ok != NN_SUCCESS) {
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
		}
		g_free(filename);
		return NULL;
	}

	debug_print(_("getting article %d...\n"), num);
	ok = news_get_article(NNTP_SESSION(REMOTE_FOLDER(folder)->session),
			      num, filename);
	if (ok != NN_SUCCESS) {
		g_warning(_("can't read article %d\n"), num);
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
		}
		g_free(filename);
		return NULL;
	}

	return filename;
}

static MsgInfo *news_get_msginfo(Folder *folder, FolderItem *item, gint num)
{
	MsgInfo *msginfo;
	MsgFlags flags = {0, 0};
	gchar *file;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	file = news_fetch_msg(folder, item, num);
	if (!file) return NULL;

	msginfo = procheader_parse_file(file, flags, FALSE);

	g_free(file);

	return msginfo;
}

static gint news_close(Folder *folder, FolderItem *item)
{
	return 0;
}

static gint news_scan_group(Folder *folder, FolderItem *item)
{
	NNTPSession *session;
	gint num = 0, first = 0, last = 0;
	gint new = 0, unread = 0, total = 0;
	gint min = 0, max = 0;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);

	session = news_session_get(folder);
	if (!session) return -1;

	ok = news_select_group(session, item->path, &num, &first, &last);
	if (ok != NN_SUCCESS) {
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
		}
		return -1;
	}

	if (num == 0) {
		item->new = item->unread = item->total = item->last_num = 0;
		return 0;
	}

	procmsg_get_mark_sum(item, &new, &unread, &total, &min, &max, first);

	if (max < first || last < min)
		new = unread = total = num;
	else {
		if (min < first)
			min = first;

		if (last < max)
			max = last;
		else if (max < last) {
			new += last - max;
			unread += last - max;
		}

		if (new > num) new = num;
		if (unread > num) unread = num;
	}

	item->new = new;
	item->unread = unread;
	item->total = num;
	item->last_num = last;

	return 0;
}

static NewsGroupInfo *news_group_info_new(const gchar *name,
					  gint first, gint last, gchar type)
{
	NewsGroupInfo *ginfo;

	ginfo = g_new(NewsGroupInfo, 1);
	ginfo->name = g_strdup(name);
	ginfo->first = first;
	ginfo->last = last;
	ginfo->type = type;
	ginfo->subscribed = FALSE;

	return ginfo;
}

static void news_group_info_free(NewsGroupInfo *ginfo)
{
	g_free(ginfo->name);
	g_free(ginfo);
}

static gint news_group_info_compare(NewsGroupInfo *ginfo1,
				    NewsGroupInfo *ginfo2)
{
	return g_ascii_strcasecmp(ginfo1->name, ginfo2->name);
}

GSList *news_get_group_list(Folder *folder)
{
	gchar *path, *filename;
	FILE *fp;
	GSList *list = NULL;
	GSList *last = NULL;
	gchar buf[NNTPBUFSIZE];

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_TYPE(folder) == F_NEWS, NULL);

	path = folder_item_get_path(FOLDER_ITEM(folder->node->data));
	if (!is_dir_exist(path))
		make_dir_hier(path);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, NEWSGROUP_LIST, NULL);
	g_free(path);

	if ((fp = g_fopen(filename, "rb")) == NULL) {
		NNTPSession *session;
		gint ok;

		session = news_session_get(folder);
		if (!session) {
			g_free(filename);
			return NULL;
		}

		ok = nntp_list(session);
		if (ok != NN_SUCCESS) {
			if (ok == NN_SOCKET) {
				session_destroy(SESSION(session));
				REMOTE_FOLDER(folder)->session = NULL;
			}
			g_free(filename);
			return NULL;
		}
		if (recv_write_to_file(SESSION(session)->sock, filename) < 0) {
			log_warning("can't retrieve newsgroup list\n");
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
			g_free(filename);
			return NULL;
		}

		if ((fp = g_fopen(filename, "rb")) == NULL) {
			FILE_OP_ERROR(filename, "fopen");
			g_free(filename);
			return NULL;
		}
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *p = buf;
		gchar *name;
		gint last_num;
		gint first_num;
		gchar type;
		NewsGroupInfo *ginfo;

		p = strchr(p, ' ');
		if (!p) {
			strretchomp(buf);
			log_warning("invalid LIST response: %s\n", buf);
			continue;
		}
		*p = '\0';
		p++;
		name = buf;

		if (sscanf(p, "%d %d %c", &last_num, &first_num, &type) < 3) {
			strretchomp(p);
			log_warning("invalid LIST response: %s %s\n", name, p);
			continue;
		}

		ginfo = news_group_info_new(name, first_num, last_num, type);

		if (!last)
			last = list = g_slist_append(NULL, ginfo);
		else {
			last = g_slist_append(last, ginfo);
			last = last->next;
		}
	}

	fclose(fp);
	g_free(filename);

	list = g_slist_sort(list, (GCompareFunc)news_group_info_compare);

	return list;
}

void news_group_list_free(GSList *group_list)
{
	GSList *cur;

	if (!group_list) return;

	for (cur = group_list; cur != NULL; cur = cur->next)
		news_group_info_free((NewsGroupInfo *)cur->data);
	g_slist_free(group_list);
}

void news_remove_group_list_cache(Folder *folder)
{
	gchar *path, *filename;

	g_return_if_fail(folder != NULL);
	g_return_if_fail(FOLDER_TYPE(folder) == F_NEWS);

	path = folder_item_get_path(FOLDER_ITEM(folder->node->data));
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, NEWSGROUP_LIST, NULL);
	g_free(path);

	if (is_file_exist(filename)) {
		if (remove(filename) < 0)
			FILE_OP_ERROR(filename, "remove");
	}
	g_free(filename);
}

gint news_post(Folder *folder, const gchar *file)
{
	FILE *fp;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(FOLDER_TYPE(folder) == F_NEWS, -1);
	g_return_val_if_fail(file != NULL, -1);

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	ok = news_post_stream(folder, fp);

	fclose(fp);

	return ok;
}

gint news_post_stream(Folder *folder, FILE *fp)
{
	NNTPSession *session;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(FOLDER_TYPE(folder) == F_NEWS, -1);
	g_return_val_if_fail(fp != NULL, -1);

	session = news_session_get(folder);
	if (!session) return -1;

	ok = nntp_post(session, fp);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't post article.\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
		}
		return -1;
	}

	return 0;
}

static gint news_get_article_cmd(NNTPSession *session, const gchar *cmd,
				 gint num, gchar *filename)
{
	gchar *msgid;
	gint ok;

	ok = nntp_get_article(session, cmd, num, &msgid);
	if (ok != NN_SUCCESS)
		return ok;

	debug_print("Message-Id = %s, num = %d\n", msgid, num);
	g_free(msgid);

	ok = recv_write_to_file(SESSION(session)->sock, filename);
	if (ok < 0) {
		log_warning(_("can't retrieve article %d\n"), num);
		if (ok == -2)
			return NN_SOCKET;
		else
			return NN_IOERR;
	}

	return NN_SUCCESS;
}

static gint news_get_article(NNTPSession *session, gint num, gchar *filename)
{
	return news_get_article_cmd(session, "ARTICLE", num, filename);
}

#if 0
static gint news_get_header(NNTPSession *session, gint num, gchar *filename)
{
	return news_get_article_cmd(session, "HEAD", num, filename);
}
#endif

/**
 * news_select_group:
 * @session: Active NNTP session.
 * @group: Newsgroup name.
 * @num: Estimated number of articles.
 * @first: First article number.
 * @last: Last article number.
 *
 * Select newsgroup @group with the GROUP command if it is not already
 * selected in @session, or article numbers need to be returned.
 *
 * Return value: NNTP result code.
 **/
static gint news_select_group(NNTPSession *session, const gchar *group,
			      gint *num, gint *first, gint *last)
{
	gint ok;
	gint num_, first_, last_;

	if (!num || !first || !last) {
		if (session->group &&
		    g_ascii_strcasecmp(session->group, group) == 0)
			return NN_SUCCESS;
		num = &num_;
		first = &first_;
		last = &last_;
	}

	g_free(session->group);
	session->group = NULL;

	ok = nntp_group(session, group, num, first, last);
	if (ok == NN_SUCCESS)
		session->group = g_strdup(group);
	else
		log_warning(_("can't select group: %s\n"), group);

	return ok;
}

static GSList *news_get_uncached_articles(NNTPSession *session,
					  FolderItem *item, gint cache_last,
					  gint *rfirst, gint *rlast)
{
	gint ok;
	gint num = 0, first = 0, last = 0, begin = 0, end = 0;
	gchar buf[NNTPBUFSIZE];
	GSList *newlist = NULL;
	GSList *llast = NULL;
	MsgInfo *msginfo;
	gint max_articles;

	if (rfirst) *rfirst = -1;
	if (rlast)  *rlast  = -1;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	g_return_val_if_fail(item->folder->account != NULL, NULL);
	g_return_val_if_fail(FOLDER_TYPE(item->folder) == F_NEWS, NULL);

	ok = news_select_group(session, item->path, &num, &first, &last);
	if (ok != NN_SUCCESS) {
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		return NULL;
	}

	/* calculate getting overview range */
	if (first > last) {
		log_warning(_("invalid article range: %d - %d\n"),
			    first, last);
		return NULL;
	}

	if (rfirst) *rfirst = first;
	if (rlast)  *rlast  = last;

	if (cache_last < first)
		begin = first;
	else if (last < cache_last)
		begin = first;
	else if (last == cache_last) {
		debug_print(_("no new articles.\n"));
		return NULL;
	} else
		begin = cache_last + 1;
	end = last;

	max_articles = item->folder->account->max_nntp_articles;
	if (max_articles > 0 && end - begin + 1 > max_articles)
		begin = end - max_articles + 1;

	log_message(_("getting xover %d - %d in %s...\n"),
		    begin, end, item->path);
	ok = nntp_xover(session, begin, end);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't get xover\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		return NULL;
	}

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
			log_warning(_("error occurred while getting xover.\n"));
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
			return newlist;
		}

		if (buf[0] == '.' && buf[1] == '\r') break;

		msginfo = news_parse_xover(buf);
		if (!msginfo) {
			log_warning(_("invalid xover line: %s\n"), buf);
			continue;
		}

		msginfo->folder = item;
		msginfo->flags.perm_flags = MSG_NEW|MSG_UNREAD;
		msginfo->flags.tmp_flags = MSG_NEWS;
		msginfo->newsgroups = g_strdup(item->path);

		if (!newlist)
			llast = newlist = g_slist_append(newlist, msginfo);
		else {
			llast = g_slist_append(llast, msginfo);
			llast = llast->next;
		}
	}

	ok = nntp_xhdr(session, "to", begin, end);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't get xhdr\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		return newlist;
	}

	llast = newlist;

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
			log_warning(_("error occurred while getting xhdr.\n"));
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
			return newlist;
		}

		if (buf[0] == '.' && buf[1] == '\r') break;
		if (!llast) {
			g_warning("llast == NULL\n");
			continue;
		}

		msginfo = (MsgInfo *)llast->data;
		msginfo->to = news_parse_xhdr(buf, msginfo);

		llast = llast->next;
	}

	ok = nntp_xhdr(session, "cc", begin, end);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't get xhdr\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		return newlist;
	}

	llast = newlist;

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
			log_warning(_("error occurred while getting xhdr.\n"));
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
			return newlist;
		}

		if (buf[0] == '.' && buf[1] == '\r') break;
		if (!llast) {
			g_warning("llast == NULL\n");
			continue;
		}

		msginfo = (MsgInfo *)llast->data;
		msginfo->cc = news_parse_xhdr(buf, msginfo);

		llast = llast->next;
	}

	session_set_access_time(SESSION(session));

	return newlist;
}

#define PARSE_ONE_PARAM(p, srcp) \
{ \
	p = strchr(srcp, '\t'); \
	if (!p) return NULL; \
	else \
		*p++ = '\0'; \
}

static MsgInfo *news_parse_xover(const gchar *xover_str)
{
	MsgInfo *msginfo;
	gchar *subject, *sender, *size, *line, *date, *msgid, *ref, *tmp;
	gchar *p;
	gint num, size_int, line_int;
	gchar *xover_buf;

	Xstrdup_a(xover_buf, xover_str, return NULL);

	PARSE_ONE_PARAM(subject, xover_buf);
	PARSE_ONE_PARAM(sender, subject);
	PARSE_ONE_PARAM(date, sender);
	PARSE_ONE_PARAM(msgid, date);
	PARSE_ONE_PARAM(ref, msgid);
	PARSE_ONE_PARAM(size, ref);
	PARSE_ONE_PARAM(line, size);

	tmp = strchr(line, '\t');
	if (!tmp) tmp = strchr(line, '\r');
	if (!tmp) tmp = strchr(line, '\n');
	if (tmp) *tmp = '\0';

	num = atoi(xover_str);
	size_int = atoi(size);
	line_int = atoi(line);

	/* set MsgInfo */
	msginfo = g_new0(MsgInfo, 1);
	msginfo->msgnum = num;
	msginfo->size = size_int;

	msginfo->date = g_strdup(date);
	msginfo->date_t = procheader_date_parse(NULL, date, 0);

	msginfo->from = conv_unmime_header(sender, NULL);
	msginfo->fromname = procheader_get_fromname(msginfo->from);

	msginfo->subject = conv_unmime_header(subject, NULL);

	extract_parenthesis(msgid, '<', '>');
	remove_space(msgid);
	if (*msgid != '\0')
		msginfo->msgid = g_strdup(msgid);

	eliminate_parenthesis(ref, '(', ')');
	if ((p = strrchr(ref, '<')) != NULL) {
		extract_parenthesis(p, '<', '>');
		remove_space(p);
		if (*p != '\0')
			msginfo->inreplyto = g_strdup(p);
	}

	return msginfo;
}

static gchar *news_parse_xhdr(const gchar *xhdr_str, MsgInfo *msginfo)
{
	gchar *p;
	gchar *tmp;
	gint num;

	p = strchr(xhdr_str, ' ');
	if (!p)
		return NULL;
	else
		p++;

	num = atoi(xhdr_str);
	if (msginfo->msgnum != num) return NULL;

	tmp = strchr(p, '\r');
	if (!tmp) tmp = strchr(p, '\n');

	if (tmp)
		return g_strndup(p, tmp - p);
	else
		return g_strdup(p);
}

static GSList *news_delete_old_articles(GSList *alist, FolderItem *item,
					gint first)
{
	GSList *cur, *next;
	MsgInfo *msginfo;
	gchar *dir;

	g_return_val_if_fail(item != NULL, alist);
	g_return_val_if_fail(item->folder != NULL, alist);
	g_return_val_if_fail(FOLDER_TYPE(item->folder) == F_NEWS, alist);

	if (first < 2) return alist;

	debug_print("Deleting cached articles 1 - %d ...\n", first - 1);

	dir = folder_item_get_path(item);
	remove_numbered_files(dir, 1, first - 1);
	g_free(dir);

	for (cur = alist; cur != NULL; ) {
		next = cur->next;

		msginfo = (MsgInfo *)cur->data;
		if (msginfo && msginfo->msgnum < first) {
			procmsg_msginfo_free(msginfo);
			alist = g_slist_remove(alist, msginfo);
			item->cache_dirty = TRUE;
		}

		cur = next;
	}

	return alist;
}

static void news_delete_all_articles(FolderItem *item)
{
	gchar *dir;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_NEWS);

	debug_print("Deleting all cached articles...\n");

	dir = folder_item_get_path(item);
	remove_all_numbered_files(dir);
	g_free(dir);
}

static void news_delete_expired_caches(GSList *alist, FolderItem *item)
{
	gchar *dir;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_NEWS);

	debug_print("Deleting expired cached articles...\n");

	dir = folder_item_get_path(item);
	remove_expired_files(dir, 24 * 7);
	g_free(dir);
}
