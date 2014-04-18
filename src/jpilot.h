/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Match Grun
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

/*
 * Definitions for accessing JPilot database files.
 * JPilot is Copyright(c) by Judd Montgomery.
 * Visit http://www.jpilot.org for more details.
 */

#ifndef __JPILOT_H__
#define __JPILOT_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_JPILOT

#include <glib.h>
#include <stdio.h>

#ifdef HAVE_LIBPISOCK_PI_ADDRESS_H
#  include <libpisock/pi-address.h>
#else
#  include <pi-address.h>
#endif

#include "addritem.h"
#include "addrcache.h"
#include "utils.h"

typedef struct _JPilotFile JPilotFile;

struct _JPilotFile {
	gchar                 *name;
	FILE                  *file;
	gchar                 *path;
	AddressCache          *addressCache;
	struct AddressAppInfo addrInfo;
	gboolean              readMetadata;
	GList                 *customLabels;
	GList                 *labelInd;
	gint                  retVal;
	gboolean              accessFlag;
	gboolean              havePC3;
	stime_t               pc3ModifyTime;
};

/* Limits */
#define JPILOT_NUM_LABELS	22	/* Number of labels */
#define JPILOT_NUM_PHONELABELS	8 	/* Number of phone number labels */
#define JPILOT_NUM_CATEG	16	/* Number of categories */
#define JPILOT_LEN_LABEL	15	/* Max length of label */
#define JPILOT_LEN_CATEG	15	/* Max length of category */
#define JPILOT_NUM_ADDR_PHONE	5	/* Number of phone entries a person
					   can have */

/* Function prototypes */
JPilotFile *jpilot_create		( void );
JPilotFile *jpilot_create_path		( const gchar *path );
void jpilot_set_name			( JPilotFile* pilotFile, const gchar *value );
void jpilot_set_file			( JPilotFile* pilotFile, const gchar *value );
void jpilot_free			( JPilotFile *pilotFile );
gint jpilot_get_status			( JPilotFile *pilotFile );
gboolean jpilot_get_modified		( JPilotFile *pilotFile );
gboolean jpilot_get_accessed		( JPilotFile *pilotFile );
void jpilot_set_accessed		( JPilotFile *pilotFile, const gboolean value );
gboolean jpilot_get_read_flag		( JPilotFile *pilotFile );
ItemFolder *jpilot_get_root_folder	( JPilotFile *pilotFile );
gchar *jpilot_get_name			( JPilotFile *pilotFile );

void jpilot_force_refresh		( JPilotFile *pilotFile );
void jpilot_print_file			( JPilotFile *jpilotFile, FILE *stream );
void jpilot_print_short			( JPilotFile *pilotFile, FILE *stream );
gint jpilot_read_data			( JPilotFile *pilotFile );
GList *jpilot_get_list_person		( JPilotFile *pilotFile );
GList *jpilot_get_list_folder		( JPilotFile *pilotFile );
GList *jpilot_get_all_persons		( JPilotFile *pilotFile );

GList *jpilot_load_label		( JPilotFile *pilotFile, GList *labelList );
GList *jpilot_get_category_list		( JPilotFile *pilotFile );
gchar *jpilot_get_category_name		( JPilotFile *pilotFile, gint catID );
GList *jpilot_load_phone_label		( JPilotFile *pilotFile, GList *labelList );
GList *jpilot_load_custom_label		( JPilotFile *pilotFile, GList *labelList );

gboolean jpilot_validate		( const JPilotFile *pilotFile );
gchar *jpilot_find_pilotdb		( void );

gint jpilot_test_read_file		( const gchar *fileSpec );

void jpilot_clear_custom_labels		( JPilotFile *pilotFile );
void jpilot_add_custom_label		( JPilotFile *pilotFile, const gchar *labelName );
GList *jpilot_get_custom_labels		( JPilotFile *pilotFile );
gboolean jpilot_test_custom_label	( JPilotFile *pilotFile, const gchar *labelName );
/* gboolean jpilot_test_pilot_lib		( void ); */

gint jpilot_read_modified		( JPilotFile *pilotFile );

#endif /* USE_JPILOT */

#endif /* __JPILOT_H__ */
