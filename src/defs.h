/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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

#ifndef __DEFS_H__
#define __DEFS_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if HAVE_PATHS_H
#  include <paths.h>
#endif

#if HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#define INBOX_DIR		"inbox"
#define OUTBOX_DIR		"sent"
#define QUEUE_DIR		"queue"
#define DRAFT_DIR		"draft"
#define TRASH_DIR		"trash"
#define RC_DIR			".sylpheed-2.0"
#define OLD_RC_DIR		".sylpheed"
#define NEWS_CACHE_DIR		"newscache"
#define IMAP_CACHE_DIR		"imapcache"
#define MIME_TMP_DIR		"mimetmp"
#define COMMON_RC		"sylpheedrc"
#define ACCOUNT_RC		"accountrc"
#define FILTER_RC		"filterrc"
#define FILTER_LIST		"filter.xml"
#define FILTER_HEADER_RC	"filterheaderrc"
#define CUSTOM_HEADER_RC	"customheaderrc"
#define DISPLAY_HEADER_RC	"dispheaderrc"
#define MENU_RC			"menurc"
#define ACTIONS_RC		"actionsrc"
#define COMMAND_HISTORY		"command_history"
#define TEMPLATE_DIR		"templates"
#define TMP_DIR			"tmp"
#define UIDL_DIR		"uidl"
#define NEWSGROUP_LIST		".newsgroup_list"
#define ADDRESS_BOOK		"addressbook.xml"
#define MANUAL_HTML_INDEX	"sylpheed.html"
#define FAQ_HTML_INDEX		"sylpheed-faq.html"
#define HOMEPAGE_URI		"http://sylpheed.good-day.net/"
#define FOLDER_LIST		"folderlist.xml"
#define CACHE_FILE		".sylpheed_cache"
#define MARK_FILE		".sylpheed_mark"
#define CACHE_VERSION		0x20
#define MARK_VERSION		2

#define DEFAULT_SIGNATURE	".signature"
#define DEFAULT_INC_PATH	"/usr/bin/mh/inc"
#define DEFAULT_INC_PROGRAM	"inc"
/* #define DEFAULT_INC_PATH	"/usr/bin/imget" */
/* #define DEFAULT_INC_PROGRAM	"imget" */
#define DEFAULT_SENDMAIL_CMD	"/usr/sbin/sendmail -t -i"
#define DEFAULT_BROWSER_CMD	"mozilla-firefox -remote 'openURL(%s,new-window)'"

#ifdef _PATH_MAILDIR
#  define DEFAULT_SPOOL_PATH	_PATH_MAILDIR
#else
#  define DEFAULT_SPOOL_PATH	"/var/spool/mail"
#endif

#define BUFFSIZE			8192

#ifndef MAXPATHLEN
#  define MAXPATHLEN			4095
#endif

#define DEFAULT_HEIGHT			460
#define DEFAULT_FOLDERVIEW_WIDTH	179
#define DEFAULT_MAINVIEW_WIDTH		600
#define DEFAULT_SUMMARY_HEIGHT		140
#define DEFAULT_HEADERVIEW_HEIGHT	40
#define DEFAULT_COMPOSE_HEIGHT		560
#define BORDER_WIDTH			2
#define CTREE_INDENT			18
#define FOLDER_SPACING			4
#define MAX_ENTRY_LENGTH		8191
#define COLOR_DIM			35000
#define UI_REFRESH_INTERVAL		50000	/* usec */
#define FOLDER_UPDATE_INTERVAL		1500	/* msec */
#define PROGRESS_UPDATE_INTERVAL	200	/* msec */
#define SESSION_TIMEOUT_INTERVAL	60	/* sec */
#define MAX_HISTORY_SIZE		16

#define DEFAULT_MESSAGE_FONT	"Sans 14"

#endif /* __DEFS_H__ */
