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
 * Functions necessary to access JPilot database files.
 * JPilot is Copyright(c) by Judd Montgomery.
 * Visit http://www.jpilot.org for more details.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_JPILOT

#include <glib.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
/* #include <dlfcn.h> */
#include <netinet/in.h>

#ifdef HAVE_LIBPISOCK_PI_ARGS_H
#  include <libpisock/pi-args.h>
#  include <libpisock/pi-appinfo.h>
#  include <libpisock/pi-address.h>
#  include <libpisock/pi-version.h>
#else
#  include <pi-args.h>
#  include <pi-appinfo.h>
#  include <pi-address.h>
#  include <pi-version.h>
#endif

#include "mgutils.h"
#include "addritem.h"
#include "addrcache.h"
#include "jpilot.h"
#include "codeconv.h"
#include "utils.h"

#define JPILOT_DBHOME_DIR   ".jpilot"
#define JPILOT_DBHOME_FILE  "AddressDB.pdb"
#define PILOT_LINK_LIB_NAME "libpisock.so"

#define IND_LABEL_LASTNAME  0 	/* Index of last name in address data */
#define IND_LABEL_FIRSTNAME 1 	/* Index of first name in address data */
#define IND_PHONE_EMAIL     4 	/* Index of E-Mail address in phone labels */
#define OFFSET_PHONE_LABEL  3 	/* Offset to phone data in address data */
#define IND_CUSTOM_LABEL    14	/* Offset to custom label names */
#define NUM_CUSTOM_LABEL    4 	/* Number of custom labels */

/* Shamelessly copied from JPilot (libplugin.h) */
typedef struct {
	unsigned char db_name[32];
	unsigned char flags[2];
	unsigned char version[2];
	unsigned char creation_time[4];
	unsigned char modification_time[4];
	unsigned char backup_time[4];
	unsigned char modification_number[4];
	unsigned char app_info_offset[4];
	unsigned char sort_info_offset[4];
	unsigned char type[4];/*Database ID */
	unsigned char creator_id[4];/*Application ID */
	unsigned char unique_id_seed[4];
	unsigned char next_record_list_id[4];
	unsigned char number_of_records[2];
} RawDBHeader;

/* Shamelessly copied from JPilot (libplugin.h) */
typedef struct {
	char db_name[32];
	unsigned int flags;
	unsigned int version;
	time_t creation_time;
	time_t modification_time;
	time_t backup_time;
	unsigned int modification_number;
	unsigned int app_info_offset;
	unsigned int sort_info_offset;
	char type[5];/*Database ID */
	char creator_id[5];/*Application ID */
	char unique_id_seed[5];
	unsigned int next_record_list_id;
	unsigned int number_of_records;
} DBHeader;

/* Shamelessly copied from JPilot (libplugin.h) */
typedef struct {
	unsigned char Offset[4];  /*4 bytes offset from BOF to record */
	unsigned char attrib;
	unsigned char unique_ID[3];
} record_header;

/* Shamelessly copied from JPilot (libplugin.h) */
typedef struct mem_rec_header_s {
	unsigned int rec_num;
	unsigned int offset;
	unsigned int unique_id;
	unsigned char attrib;
	struct mem_rec_header_s *next;
} mem_rec_header;

/* Shamelessly copied from JPilot (libplugin.h) */
#define SPENT_PC_RECORD_BIT	256

typedef enum {
	PALM_REC = 100L,
	MODIFIED_PALM_REC = 101L,
	DELETED_PALM_REC = 102L,
	NEW_PC_REC = 103L,
	DELETED_PC_REC = SPENT_PC_RECORD_BIT + 104L,
	DELETED_DELETED_PALM_REC = SPENT_PC_RECORD_BIT + 105L
} PCRecType;

/* Shamelessly copied from JPilot (libplugin.h) */
typedef struct {
	PCRecType rt;
	unsigned int unique_id;
	unsigned char attrib;
	void *buf;
	int size;
} buf_rec;

/* Shamelessly copied from JPilot (libplugin.h) */
typedef struct {
	unsigned long header_len;
	unsigned long header_version;
	unsigned long rec_len;
	unsigned long unique_id;
	unsigned long rt; /* Record Type */
	unsigned char attrib;
} PC3RecordHeader;

enum {
	FAMILY_LAST = 0,
	FAMILY_FIRST = 1
} name_order;

gboolean convert_charcode;

static gchar *jpilot_convert_encoding(const gchar *str)
{
	if (convert_charcode)
		return conv_codeset_strdup(str, CS_SHIFT_JIS, CS_INTERNAL);

	return g_strdup(str);
}

/*
* Create new pilot file object.
*/
JPilotFile *jpilot_create() {
	JPilotFile *pilotFile;
	pilotFile = g_new0( JPilotFile, 1 );
	pilotFile->name = NULL;
	pilotFile->file = NULL;
	pilotFile->path = NULL;
	pilotFile->addressCache = addrcache_create();
	pilotFile->readMetadata = FALSE;
	pilotFile->customLabels = NULL;
	pilotFile->labelInd = NULL;
	pilotFile->retVal = MGU_SUCCESS;
	pilotFile->accessFlag = FALSE;
	pilotFile->havePC3 = FALSE;
	pilotFile->pc3ModifyTime = 0;
	return pilotFile;
}

/*
* Create new pilot file object for specified file.
*/
JPilotFile *jpilot_create_path( const gchar *path ) {
	JPilotFile *pilotFile;
	pilotFile = jpilot_create();
	jpilot_set_file( pilotFile, path );
	return pilotFile;
}

/*
* Properties...
*/
void jpilot_set_name( JPilotFile* pilotFile, const gchar *value ) {
	g_return_if_fail( pilotFile != NULL );
	pilotFile->name = mgu_replace_string( pilotFile->name, value );
}
void jpilot_set_file( JPilotFile* pilotFile, const gchar *value ) {
	g_return_if_fail( pilotFile != NULL );
	addrcache_refresh( pilotFile->addressCache );
	pilotFile->readMetadata = FALSE;
	pilotFile->path = mgu_replace_string( pilotFile->path, value );
}
void jpilot_set_accessed( JPilotFile *pilotFile, const gboolean value ) {
	g_return_if_fail( pilotFile != NULL );
	pilotFile->accessFlag = value;
}

gint jpilot_get_status( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, -1 );
	return pilotFile->retVal;
}
ItemFolder *jpilot_get_root_folder( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, NULL );
	return addrcache_get_root_folder( pilotFile->addressCache );
}
gchar *jpilot_get_name( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, NULL );
	return pilotFile->name;
}

/*
* Test whether file was read.
* Return: TRUE if file was read.
*/
gboolean jpilot_get_read_flag( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, FALSE );
	return pilotFile->addressCache->dataRead;
}

/*
* Free up custom label list.
*/
void jpilot_clear_custom_labels( JPilotFile *pilotFile ) {
	GList *node;

	g_return_if_fail( pilotFile != NULL );

	/* Release custom labels */
	mgu_free_dlist( pilotFile->customLabels );
	pilotFile->customLabels = NULL;

	/* Release indexes */
	node = pilotFile->labelInd;
	while( node ) {
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( pilotFile->labelInd );
	pilotFile->labelInd = NULL;

	/* Force a fresh read */
	addrcache_refresh( pilotFile->addressCache );
}

/*
* Append a custom label, representing an E-Mail address field to the
* custom label list.
*/
void jpilot_add_custom_label( JPilotFile *pilotFile, const gchar *labelName ) {
	g_return_if_fail( pilotFile != NULL );

	if( labelName ) {
		gchar *labelCopy = g_strdup( labelName );
		g_strstrip( labelCopy );
		if( *labelCopy == '\0' ) {
			g_free( labelCopy );
		}
		else {
			pilotFile->customLabels = g_list_append( pilotFile->customLabels, labelCopy );
			/* Force a fresh read */
			addrcache_refresh( pilotFile->addressCache );
		}
	}
}

/*
* Get list of custom labels.
* Return: List of labels. Must use g_free() when done.
*/
GList *jpilot_get_custom_labels( JPilotFile *pilotFile ) {
	GList *retVal = NULL;
	GList *node;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	node = pilotFile->customLabels;
	while( node ) {
		retVal = g_list_append( retVal, g_strdup( node->data ) );
		node = g_list_next( node );
	}
	return retVal;
}

/*
* Return filespec of PC3 file corresponding to JPilot PDB file.
* Note: Filespec should be g_free() when done.
*/
static gchar *jpilot_get_pc3_file( JPilotFile *pilotFile ) {
	gchar *fileSpec, *r;
	gint i, len, pos;

	if( pilotFile == NULL ) return NULL;
	if( pilotFile->path == NULL ) return NULL;

	fileSpec = g_strdup( pilotFile->path );
	len = strlen( fileSpec );
	pos = -1;
	r = NULL;
	for( i = len; i > 0; i-- ) {
		if( *(fileSpec + i) == '.' ) {
			pos = i + 1;
			r = fileSpec + pos;
			break;
		}
	}
	if( r ) {
		if( len - pos == 3 ) {
			*r++ = 'p'; *r++ = 'c'; *r = '3';
			return fileSpec;
		}
	}
	g_free( fileSpec );
	return NULL;
}

/*
* Save PC3 file time to cache.
* return: TRUE if time marked.
*/
static gboolean jpilot_mark_files( JPilotFile *pilotFile ) {
	gboolean retVal = FALSE;
	GStatBuf filestat;
	gchar *pcFile;

	/* Mark PDB file cache */
	retVal = addrcache_mark_file( pilotFile->addressCache, pilotFile->path );

	/* Now mark PC3 file */
	pilotFile->havePC3 = FALSE;
	pilotFile->pc3ModifyTime = 0;
	pcFile = jpilot_get_pc3_file( pilotFile );
	if( pcFile == NULL ) return retVal;
	if( 0 == g_lstat( pcFile, &filestat ) ) {
		pilotFile->havePC3 = TRUE;
		pilotFile->pc3ModifyTime = filestat.st_mtime;
		retVal = TRUE;
	}
	g_free( pcFile );
	return retVal;
}

/*
* Check whether JPilot PDB or PC3 file has changed by comparing
* with cached data.
* return: TRUE if file has changed.
*/
static gboolean jpilot_check_files( JPilotFile *pilotFile ) {
	gboolean retVal = TRUE;
	GStatBuf filestat;
	gchar *pcFile;

	/* Check main file */
	if( addrcache_check_file( pilotFile->addressCache, pilotFile->path ) )
		return TRUE;

	/* Test PC3 file */
	if( ! pilotFile->havePC3 ) return FALSE;
	pcFile = jpilot_get_pc3_file( pilotFile );
	if( pcFile == NULL ) return FALSE;

	if( 0 == g_lstat( pcFile, &filestat ) ) {
		if( filestat.st_mtime == pilotFile->pc3ModifyTime ) retVal = FALSE;
	}
	g_free( pcFile );
	return retVal;
}

/*
* Test whether file was modified since last access.
* Return: TRUE if file was modified.
*/
gboolean jpilot_get_modified( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, FALSE );
	return jpilot_check_files( pilotFile );
}
gboolean jpilot_get_accessed( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, FALSE );
	return pilotFile->accessFlag;
}

/*
* Free up pilot file object by releasing internal memory.
*/
void jpilot_free( JPilotFile *pilotFile ) {
	g_return_if_fail( pilotFile != NULL );

	/* Free internal stuff */
	g_free( pilotFile->path );

	/* Release custom labels */
	jpilot_clear_custom_labels( pilotFile );

	/* Clear cache */
	addrcache_clear( pilotFile->addressCache );
	addrcache_free( pilotFile->addressCache );
	pilotFile->addressCache = NULL;
	pilotFile->readMetadata = FALSE;
	pilotFile->accessFlag = FALSE;
	pilotFile->havePC3 = FALSE;
	pilotFile->pc3ModifyTime = 0;

	/* Now release file object */
	g_free( pilotFile );
}

/*
* Refresh internal variables to force a file read.
*/
void jpilot_force_refresh( JPilotFile *pilotFile ) {
	addrcache_refresh( pilotFile->addressCache );
}

/*
* Print object to specified stream.
*/
void jpilot_print_file( JPilotFile *pilotFile, FILE *stream ) {
	GList *node;

	g_return_if_fail( pilotFile != NULL );

	fprintf( stream, "JPilotFile:\n" );
	fprintf( stream, "file spec: '%s'\n", pilotFile->path );
	fprintf( stream, " metadata: %s\n", pilotFile->readMetadata ? "yes" : "no" );
	fprintf( stream, "  ret val: %d\n", pilotFile->retVal );

	node = pilotFile->customLabels;
	while( node ) {
		fprintf( stream, "  c label: %s\n", (gchar *)node->data );
		node = g_list_next( node );
	}

	node = pilotFile->labelInd;
	while( node ) {
		fprintf( stream, " labelind: %d\n", GPOINTER_TO_INT(node->data) );
		node = g_list_next( node );
	}

	addrcache_print( pilotFile->addressCache, stream );
	fprintf( stream, "  ret val: %d\n", pilotFile->retVal );
	fprintf( stream, " have pc3: %s\n", pilotFile->havePC3 ? "yes" : "no" );
	fprintf( stream, " pc3 time: %lu\n", pilotFile->pc3ModifyTime );
	addritem_print_item_folder( pilotFile->addressCache->rootFolder, stream );
}

/*
* Print summary of object to specified stream.
*/
void jpilot_print_short( JPilotFile *pilotFile, FILE *stream ) {
	GList *node;
	g_return_if_fail( pilotFile != NULL );
	fprintf( stream, "JPilotFile:\n" );
	fprintf( stream, "file spec: '%s'\n", pilotFile->path );
	fprintf( stream, " metadata: %s\n", pilotFile->readMetadata ? "yes" : "no" );
	fprintf( stream, "  ret val: %d\n", pilotFile->retVal );

	node = pilotFile->customLabels;
	while( node ) {
		fprintf( stream, "  c label: %s\n", (gchar *)node->data );
		node = g_list_next( node );
	}

	node = pilotFile->labelInd;
	while( node ) {
		fprintf( stream, " labelind: %d\n", GPOINTER_TO_INT(node->data) );
		node = g_list_next( node );
	}
	addrcache_print( pilotFile->addressCache, stream );
	fprintf( stream, " have pc3: %s\n", pilotFile->havePC3 ? "yes" : "no" );
	fprintf( stream, " pc3 time: %lu\n", pilotFile->pc3ModifyTime );
}

/* Shamelessly copied from JPilot (libplugin.c) */
static unsigned int bytes_to_bin(unsigned char *bytes, unsigned int num_bytes) {
unsigned int i, n;
	n=0;
	for (i=0;i<num_bytes;i++) {
		n = n*256+bytes[i];
	}
	return n;
}

/* Shamelessly copied from JPilot (utils.c) */
/* These next 2 functions were copied from pi-file.c in the pilot-link app */
/* Exact value of "Jan 1, 1970 0:00:00 GMT" - "Jan 1, 1904 0:00:00 GMT" */
#define PILOT_TIME_DELTA (unsigned)(2082844800)

time_t pilot_time_to_unix_time ( unsigned long raw_time ) {
   return (time_t)(raw_time - PILOT_TIME_DELTA);
}

/* Shamelessly copied from JPilot (libplugin.c) */
static int raw_header_to_header(RawDBHeader *rdbh, DBHeader *dbh) {
	unsigned long temp;

	strncpy(dbh->db_name, (gchar *)rdbh->db_name, 31);
	dbh->db_name[31] = '\0';
	dbh->flags = bytes_to_bin(rdbh->flags, 2);
	dbh->version = bytes_to_bin(rdbh->version, 2);
	temp = bytes_to_bin(rdbh->creation_time, 4);
	dbh->creation_time = pilot_time_to_unix_time(temp);
	temp = bytes_to_bin(rdbh->modification_time, 4);
	dbh->modification_time = pilot_time_to_unix_time(temp);
	temp = bytes_to_bin(rdbh->backup_time, 4);
	dbh->backup_time = pilot_time_to_unix_time(temp);
	dbh->modification_number = bytes_to_bin(rdbh->modification_number, 4);
	dbh->app_info_offset = bytes_to_bin(rdbh->app_info_offset, 4);
	dbh->sort_info_offset = bytes_to_bin(rdbh->sort_info_offset, 4);
	strncpy(dbh->type, (gchar *)rdbh->type, 4);
	dbh->type[4] = '\0';
	strncpy(dbh->creator_id, (gchar *)rdbh->creator_id, 4);
	dbh->creator_id[4] = '\0';
	strncpy(dbh->unique_id_seed, (gchar *)rdbh->unique_id_seed, 4);
	dbh->unique_id_seed[4] = '\0';
	dbh->next_record_list_id = bytes_to_bin(rdbh->next_record_list_id, 4);
	dbh->number_of_records = bytes_to_bin(rdbh->number_of_records, 2);
	return 0;
}

/* Shamelessly copied from JPilot (libplugin.c) */
/* returns 1 if found */
/*         0 if eof */
static int find_next_offset( mem_rec_header *mem_rh, long fpos,
	unsigned int *next_offset, unsigned char *attrib, unsigned int *unique_id )
{
	mem_rec_header *temp_mem_rh;
	unsigned char found = 0;
	unsigned long found_at;

	found_at=0xFFFFFF;
	for (temp_mem_rh=mem_rh; temp_mem_rh; temp_mem_rh = temp_mem_rh->next) {
		if ((temp_mem_rh->offset > fpos) && (temp_mem_rh->offset < found_at)) {
			found_at = temp_mem_rh->offset;
			/* *attrib = temp_mem_rh->attrib; */
			/* *unique_id = temp_mem_rh->unique_id; */
		}
		if ((temp_mem_rh->offset == fpos)) {
			found = 1;
			*attrib = temp_mem_rh->attrib;
			*unique_id = temp_mem_rh->unique_id;
		}
	}
	*next_offset = found_at;
	return found;
}

/* Shamelessly copied from JPilot (libplugin.c) */
static void free_mem_rec_header(mem_rec_header **mem_rh) {
	mem_rec_header *h, *next_h;
	for (h=*mem_rh; h; h=next_h) {
		next_h=h->next;
		free(h);
	}
	*mem_rh = NULL;
}

#if 0
/* Shamelessly copied from JPilot (libplugin.c) */
static int jpilot_free_db_list( GList **br_list ) {
	GList *temp_list, *first;
	buf_rec *br;

	/* Go to first entry in the list */
	first=NULL;
	for( temp_list = *br_list; temp_list; temp_list = temp_list->prev ) {
		first = temp_list;
	}
	for (temp_list = first; temp_list; temp_list = temp_list->next) {
		if (temp_list->data) {
			br=temp_list->data;
			if (br->buf) {
				free(br->buf);
				temp_list->data=NULL;
			}
			free(br);
		}
	}
	g_list_free(*br_list);
	*br_list=NULL;
	return 0;
}
#endif

/* Shamelessly copied from JPilot (libplugin.c) */
/* Read file size */
static int jpilot_get_info_size( FILE *in, unsigned int *size ) {
	RawDBHeader rdbh;
	DBHeader dbh;
	unsigned int offset;
	record_header rh;

	fseek(in, 0, SEEK_SET);
	fread(&rdbh, sizeof(RawDBHeader), 1, in);
	if (feof(in)) {
		return MGU_EOF;
	}

	raw_header_to_header(&rdbh, &dbh);
	if (dbh.app_info_offset==0) {
		*size=0;
		return MGU_SUCCESS;
	}
	if (dbh.sort_info_offset!=0) {
		*size = dbh.sort_info_offset - dbh.app_info_offset;
		return MGU_SUCCESS;
	}
	if (dbh.number_of_records==0) {
		fseek(in, 0, SEEK_END);
		*size=ftell(in) - dbh.app_info_offset;
		return MGU_SUCCESS;
	}

	fread(&rh, sizeof(record_header), 1, in);
	offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];
	*size=offset - dbh.app_info_offset;

	return MGU_SUCCESS;
}

/*
 * Read address file into address list. Based on JPilot's
 * libplugin.c (jp_get_app_info)
 */
static gint jpilot_get_file_info( JPilotFile *pilotFile, unsigned char **buf, unsigned int *buf_size ) {
	FILE *in;
 	int num;
	unsigned int rec_size;
	RawDBHeader rdbh;
	DBHeader dbh;

	if( ( !buf_size ) || ( ! buf ) ) {
		return MGU_BAD_ARGS;
	}

	*buf = NULL;
	*buf_size=0;

	if( pilotFile->path ) {
		in = g_fopen( pilotFile->path, "rb" );
		if( !in ) {
			return MGU_OPEN_FILE;
		}
	}
	else {
		return MGU_NO_FILE;
	}

	num = fread( &rdbh, sizeof( RawDBHeader ), 1, in );
	if( num != 1 ) {
	  	if( ferror(in) ) {
			fclose(in);
			return MGU_ERROR_READ;
		}
	}
	if (feof(in)) {
		fclose(in);
		return MGU_EOF;
	}

	/* Convert header into something recognizable */
	raw_header_to_header(&rdbh, &dbh);

	num = jpilot_get_info_size(in, &rec_size);
	if (num) {
		fclose(in);
		return MGU_ERROR_READ;
	}

	fseek(in, dbh.app_info_offset, SEEK_SET);
	*buf = ( unsigned char * ) malloc(rec_size);
	if (!(*buf)) {
		fclose(in);
		return MGU_OO_MEMORY;
	}
	num = fread(*buf, rec_size, 1, in);
	if (num != 1) {
		if (ferror(in)) {
			fclose(in);
			free(*buf);
			return MGU_ERROR_READ;
		}
	}
	fclose(in);

	*buf_size = rec_size;

	return MGU_SUCCESS;
}

/* Shamelessly copied from JPilot (libplugin.c) */
static int unpack_header(PC3RecordHeader *header, unsigned char *packed_header) {
	unsigned char *p;
	unsigned long l;

	p = packed_header;

	memcpy(&l, p, sizeof(l));
	header->header_len=ntohl(l);
	p+=sizeof(l);

	memcpy(&l, p, sizeof(l));
	header->header_version=ntohl(l);
	p+=sizeof(l);

	memcpy(&l, p, sizeof(l));
	header->rec_len=ntohl(l);
	p+=sizeof(l);

	memcpy(&l, p, sizeof(l));
	header->unique_id=ntohl(l);
	p+=sizeof(l);

	memcpy(&l, p, sizeof(l));
	header->rt=ntohl(l);
	p+=sizeof(l);

	memcpy(&(header->attrib), p, sizeof(unsigned char));
	p+=sizeof(unsigned char);

	return 0;
}

/* Shamelessly copied from JPilot (libplugin.c) */
static int read_header(FILE *pc_in, PC3RecordHeader *header) {
	unsigned long l, len;
	unsigned char packed_header[256];
	int num;

	num = fread(&l, sizeof(l), 1, pc_in);
	if (feof(pc_in)) {
		return -1;
	}
	if (num!=1) {
		return num;
	}
	memcpy(packed_header, &l, sizeof(l));
	len=ntohl(l);
	if (len > 255) {
		return -1;
	}
	num = fread(packed_header+sizeof(l), len-sizeof(l), 1, pc_in);
	if (feof(pc_in)) {
		return -1;
	}
	if (num!=1) {
		return num;
	}
	unpack_header(header, packed_header);
	return 1;
}

/* Read next record from PC3 file. Based on JPilot's
 * pc_read_next_rec (libplugin.c) */
static gint jpilot_read_next_pc( FILE *in, buf_rec *br ) {
	PC3RecordHeader header;
	int rec_len, num;
	char *record;

	if(feof(in)) {
		return MGU_EOF;
	}
	num = read_header(in, &header);
	if (num < 1) {
		if (ferror(in)) {
			return MGU_ERROR_READ;
		}
		if (feof(in)) {
			return MGU_EOF;
		}
	}
	rec_len = header.rec_len;
	record = malloc(rec_len);
	if (!record) {
		return MGU_OO_MEMORY;
	}
	num = fread(record, rec_len, 1, in);
	if (num != 1) {
		if (ferror(in)) {
			free(record);
			return MGU_ERROR_READ;
		}
	}
	br->rt = header.rt;
	br->unique_id = header.unique_id;
	br->attrib = header.attrib;
	br->buf = record;
	br->size = rec_len;

	return MGU_SUCCESS;
}

/*
 * Read address file into a linked list. Based on JPilot's
 * jp_read_DB_files (from libplugin.c)
 */
static gint jpilot_read_db_files( JPilotFile *pilotFile, GList **records ) {
	FILE *in, *pc_in;
	char *buf;
	GList *temp_list;
	int num_records, recs_returned, i, num, r;
	unsigned int offset, prev_offset, next_offset, rec_size;
	int out_of_order;
	long fpos;  /*file position indicator */
	unsigned char attrib;
	unsigned int unique_id;
	mem_rec_header *mem_rh, *temp_mem_rh, *last_mem_rh;
	record_header rh;
	RawDBHeader rdbh;
	DBHeader dbh;
	buf_rec *temp_br;
	gchar *pcFile;

	mem_rh = last_mem_rh = NULL;
	*records = NULL;
	recs_returned = 0;

	if( pilotFile->path == NULL ) {
		return MGU_BAD_ARGS;
	}

	in = g_fopen( pilotFile->path, "rb" );
	if (!in) {
		return MGU_OPEN_FILE;
	}

	/* Read the database header */
	num = fread(&rdbh, sizeof(RawDBHeader), 1, in);
	if (num != 1) {
		if (ferror(in)) {
			fclose(in);
			return MGU_ERROR_READ;
		}
		if (feof(in)) {
			fclose(in);
			return MGU_EOF;
		}
	}
	raw_header_to_header(&rdbh, &dbh);

	/* Read each record entry header */
	num_records = dbh.number_of_records;
	out_of_order = 0;
	prev_offset = 0;

	for (i = 1; i < num_records + 1; i++) {
		num = fread(&rh, sizeof(record_header), 1, in);
		if (num != 1) {
			if (ferror(in)) {
				break;
			}
			if (feof(in)) {
				free_mem_rec_header(&mem_rh);
				fclose(in);
				return MGU_EOF;
			}
		}

		offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];
		if (offset < prev_offset) {
			out_of_order = 1;
		}
		prev_offset = offset;
		temp_mem_rh = (mem_rec_header *)malloc(sizeof(mem_rec_header));
		if (!temp_mem_rh) {
			break;
		}
		temp_mem_rh->next = NULL;
		temp_mem_rh->rec_num = i;
		temp_mem_rh->offset = offset;
		temp_mem_rh->attrib = rh.attrib;
		temp_mem_rh->unique_id = (rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2];
		if (mem_rh == NULL) {
			mem_rh = temp_mem_rh;
			last_mem_rh = temp_mem_rh;
		} else {
			last_mem_rh->next = temp_mem_rh;
			last_mem_rh = temp_mem_rh;
		}
	}

	temp_mem_rh = mem_rh;

	if (num_records) {
		attrib = unique_id = 0;
		if (out_of_order) {
			find_next_offset(mem_rh, 0, &next_offset, &attrib, &unique_id);
		} else {
			next_offset = 0xFFFFFF;
			if (mem_rh) {
				next_offset = mem_rh->offset;
				attrib = mem_rh->attrib;
				unique_id = mem_rh->unique_id;
			}
		}
		fseek(in, next_offset, SEEK_SET);
		while(!feof(in)) {
			fpos = ftell(in);
			if (out_of_order) {
				find_next_offset(mem_rh, fpos, &next_offset, &attrib, &unique_id);
			} else {
				next_offset = 0xFFFFFF;
				if (temp_mem_rh) {
					attrib = temp_mem_rh->attrib;
					unique_id = temp_mem_rh->unique_id;
					if (temp_mem_rh->next) {
						temp_mem_rh = temp_mem_rh->next;
						next_offset = temp_mem_rh->offset;
					}
				}
			}
			rec_size = next_offset - fpos;
			buf = malloc(rec_size);
			if (!buf) break;
			num = fread(buf, rec_size, 1, in);
			if ((num != 1)) {
				if (ferror(in)) {
					free(buf);
					break;
				}
			}

			temp_br = malloc(sizeof(buf_rec));
			if (!temp_br) {
				free(buf);
				break;
			}
			temp_br->rt = PALM_REC;
			temp_br->unique_id = unique_id;
			temp_br->attrib = attrib;
			temp_br->buf = buf;
			temp_br->size = rec_size;

			*records = g_list_append(*records, temp_br);

			recs_returned++;
		}
	}
	fclose(in);
	free_mem_rec_header(&mem_rh);

	/* Read the PC3 file, if present */
	pcFile = jpilot_get_pc3_file( pilotFile );
	if( pcFile == NULL ) return MGU_SUCCESS;
	pc_in = g_fopen( pcFile, "rb");
	g_free( pcFile );

	if( pc_in == NULL ) {
		return MGU_SUCCESS;
	}

	while( ! feof( pc_in ) ) {
		temp_br = malloc(sizeof(buf_rec));
		if (!temp_br) {
			break;
		}
		r = jpilot_read_next_pc( pc_in, temp_br );
		if ( r != MGU_SUCCESS ) {
			free(temp_br);
			break;
		}
		if ((temp_br->rt!=DELETED_PC_REC)
			&&(temp_br->rt!=DELETED_PALM_REC)
			&&(temp_br->rt!=MODIFIED_PALM_REC)
			&&(temp_br->rt!=DELETED_DELETED_PALM_REC)) {
				*records = g_list_append(*records, temp_br);
				recs_returned++;
		}
		if ((temp_br->rt==DELETED_PALM_REC) || (temp_br->rt==MODIFIED_PALM_REC)) {
			temp_list=*records;
			if (*records) {
				while(temp_list->next) {
					temp_list=temp_list->next;
				}
			}
			for (; temp_list; temp_list=temp_list->prev) {
				if (((buf_rec *)temp_list->data)->unique_id == temp_br->unique_id) {
					((buf_rec *)temp_list->data)->rt = temp_br->rt;
				}
			}
		}
		free(temp_br);
	}
	fclose(pc_in);

	return MGU_SUCCESS;
}

#define FULLNAME_BUFSIZE	256
#define EMAIL_BUFSIZE		256
/*
 * Unpack address, building new data inside cache.
 */
static void jpilot_load_address( JPilotFile *pilotFile, buf_rec *buf, ItemFolder *folderInd[] ) {
	struct Address addr;
	gchar **addrEnt;
	gint num, k;
	gint cat_id = 0;
	guint unique_id;
	guchar attrib;
	gchar fullName[ FULLNAME_BUFSIZE ];
	gchar bufEMail[ EMAIL_BUFSIZE ];
	ItemPerson *person;
	ItemEMail *email;
	gint *indPhoneLbl;
	gchar *labelEntry;
	GList *node;
	gchar* extID;
	struct AddressAppInfo *ai;
	gchar **firstName = NULL;
	gchar **lastName = NULL;
#if (PILOT_LINK_MAJOR > 11)
	pi_buffer_t *RecordBuffer;
#endif /* PILOT_LINK_0_12 */

	/* Retrieve address */
#if (PILOT_LINK_MAJOR < 12)
	num = unpack_Address( & addr, buf->buf, buf->size );
	if( num > 0 ) {
#else /* PILOT_LINK_0_12 */
	RecordBuffer = pi_buffer_new(buf->size);
	memcpy(RecordBuffer->data, buf->buf, buf->size);
	RecordBuffer->used = buf->size;
	num = unpack_Address( & addr, RecordBuffer, address_v1 );
	pi_buffer_free(RecordBuffer);
	if (num != -1) {
#endif
		gchar *nameConv;

		addrEnt = addr.entry;
		attrib = buf->attrib;
		unique_id = buf->unique_id;
		cat_id = attrib & 0x0F;

		*fullName = *bufEMail = '\0';

		if( addrEnt[ IND_LABEL_FIRSTNAME ] ) {
			firstName = g_strsplit( addrEnt[ IND_LABEL_FIRSTNAME ], "\01", 2 );
		}
		if( addrEnt[ IND_LABEL_LASTNAME ] ) {
			lastName = g_strsplit( addrEnt[ IND_LABEL_LASTNAME ], "\01", 2 );
		}

		if( name_order == FAMILY_LAST ) {
			g_snprintf( fullName, FULLNAME_BUFSIZE, "%s %s",
				    firstName ? firstName[0] : "",
				    lastName ? lastName[0] : "" );
		}
		else {
			g_snprintf( fullName, FULLNAME_BUFSIZE, "%s %s",
				    lastName ? lastName[0] : "",
				    firstName ? firstName[0] : "" );
		}

		if( firstName ) {
			g_strfreev( firstName );
		}
		if( lastName ) {
			g_strfreev( lastName );
		}

		g_strstrip( fullName );

		nameConv = jpilot_convert_encoding( fullName );
		strncpy2( fullName, nameConv, FULLNAME_BUFSIZE );
		g_free( nameConv );

		person = addritem_create_item_person();
		addritem_person_set_common_name( person, fullName );
		addritem_person_set_first_name( person, addrEnt[ IND_LABEL_FIRSTNAME ] );
		addritem_person_set_last_name( person, addrEnt[ IND_LABEL_LASTNAME ] );
		addrcache_id_person( pilotFile->addressCache, person );

		extID = g_strdup_printf( "%d", unique_id );
		addritem_person_set_external_id( person, extID );
		g_free( extID );
		extID = NULL;

		/* Pointer to address metadata. */
		ai = & pilotFile->addrInfo;

		/* Add entry for each email address listed under phone labels. */
		indPhoneLbl = addr.phoneLabel;
		for( k = 0; k < JPILOT_NUM_ADDR_PHONE; k++ ) {
			gint ind;

			ind = indPhoneLbl[k];
			/*
			* fprintf( stdout, "%d : %d : %20s : %s\n", k, ind,
			* ai->phoneLabels[ind], addrEnt[3+k] );
			*/
			if( indPhoneLbl[k] == IND_PHONE_EMAIL ) {
				labelEntry = addrEnt[ OFFSET_PHONE_LABEL + k ];
				if( labelEntry ) {
					strcpy( bufEMail, labelEntry );
					g_strchug( bufEMail );
					g_strchomp( bufEMail );

					email = addritem_create_item_email();
					addritem_email_set_address( email, bufEMail );
					addrcache_id_email( pilotFile->addressCache, email );
					addrcache_person_add_email
						( pilotFile->addressCache, person, email );
				}
			}
		}

		/* Add entry for each custom label */
		node = pilotFile->labelInd;
		while( node ) {
			gint ind;

			ind = GPOINTER_TO_INT( node->data );
			if( ind > -1 ) {
				/*
				* fprintf( stdout, "%d : %20s : %s\n", ind, ai->labels[ind],
				* addrEnt[ind] );
				*/
				labelEntry = addrEnt[ind];
				if( labelEntry ) {
					gchar *convertBuff;

					strcpy( bufEMail, labelEntry );
					g_strchug( bufEMail );
					g_strchomp( bufEMail );

					email = addritem_create_item_email();
					addritem_email_set_address( email, bufEMail );

					convertBuff = jpilot_convert_encoding( ai->labels[ind] );
					addritem_email_set_remarks( email, convertBuff );
					g_free( convertBuff );

					addrcache_id_email( pilotFile->addressCache, email );
					addrcache_person_add_email
						( pilotFile->addressCache, person, email );
				}
			}

			node = g_list_next( node );
		}

		if( person->listEMail ) {
			if( cat_id > -1 && cat_id < JPILOT_NUM_CATEG ) {
				/* Add to specified category */
				addrcache_folder_add_person
					( pilotFile->addressCache, folderInd[cat_id], person );
			}
			else {
				/* Add to root folder */
				addrcache_add_person( pilotFile->addressCache, person );
			}
		}
		else {
			addritem_free_item_person( person );
			person = NULL;
		}
	}
}

/*
 * Free up address list.
 */
static void jpilot_free_addrlist( GList *records ) {
	GList *node;
	buf_rec *br;

	node = records;
	while( node ) {
		br = node->data;
		free( br );
		node->data = NULL;
		node = g_list_next( node );
	}

	/* Free up list */
	g_list_free( records );
}

/*
 * Read address file into address cache.
 */
static gint jpilot_read_file( JPilotFile *pilotFile ) {
	gint retVal, i;
	GList *records = NULL;
	GList *node;
	buf_rec *br;
	ItemFolder *folderInd[ JPILOT_NUM_CATEG ];

	retVal = jpilot_read_db_files( pilotFile, &records );
	if( retVal != MGU_SUCCESS ) {
		jpilot_free_addrlist( records );
		return retVal;
	}

	/* Build array of pointers to categories */
	i = 0;
	node = addrcache_get_list_folder( pilotFile->addressCache );
	while( node ) {
		if( i < JPILOT_NUM_CATEG ) {
			folderInd[i] = node->data;
		}
		node = g_list_next( node );
		i++;
	}

	/* Load all addresses, free up old stuff as we go */
	node = records;
	while( node ) {
		br = node->data;
		if( ( br->rt != DELETED_PC_REC ) &&
		    ( br->rt != DELETED_PALM_REC ) &&
		    ( br->rt != MODIFIED_PALM_REC ) &&
		    ( br->rt != DELETED_DELETED_PALM_REC ) ) {
			jpilot_load_address( pilotFile, br, folderInd );
		}
		free( br );
		node->data = NULL;
		node = g_list_next( node );
	}

	/* Free up list */
	g_list_free( records );

	return retVal;
}


/*
* Read metadata from file.
*/
static gint jpilot_read_metadata( JPilotFile *pilotFile ) {
	gint retVal;
	unsigned int rec_size;
	unsigned char *buf;
	int num;

	g_return_val_if_fail( pilotFile != NULL, -1 );

	pilotFile->readMetadata = FALSE;
	addrcache_clear( pilotFile->addressCache );

	/* Read file info */
	retVal = jpilot_get_file_info( pilotFile, &buf, &rec_size);
	if( retVal != MGU_SUCCESS ) {
		pilotFile->retVal = retVal;
		return pilotFile->retVal;
	}

	num = unpack_AddressAppInfo( &pilotFile->addrInfo, buf, rec_size );
	if( buf ) {
		free(buf);
	}
	if( num <= 0 ) {
		pilotFile->retVal = MGU_ERROR_READ;
		return pilotFile->retVal;
	}

	pilotFile->readMetadata = TRUE;
	pilotFile->retVal = MGU_SUCCESS;
	return pilotFile->retVal;
}

/*
* Setup labels and indexes from metadata.
* Return: TRUE is setup successfully.
*/
static gboolean jpilot_setup_labels( JPilotFile *pilotFile ) {
	gboolean retVal = FALSE;
	struct AddressAppInfo *ai;
	GList *node;

	g_return_val_if_fail( pilotFile != NULL, -1 );

	/* Release indexes */
	node = pilotFile->labelInd;
	while( node ) {
		node->data = NULL;
		node = g_list_next( node );
	}
	pilotFile->labelInd = NULL;

	if( pilotFile->readMetadata ) {
		ai = & pilotFile->addrInfo;
		node = pilotFile->customLabels;
		while( node ) {
			gchar *lbl = node->data;
			gint ind = -1;
			gint i;
			for( i = 0; i < JPILOT_NUM_LABELS; i++ ) {
				gchar *labelName;
				gchar convertBuff[ JPILOT_LEN_LABEL ];

				labelName = jpilot_convert_encoding( ai->labels[i] );
				strncpy2( convertBuff, labelName, JPILOT_LEN_LABEL );
				g_free( labelName );
				labelName = convertBuff;

				if( g_ascii_strcasecmp( labelName, lbl ) == 0 ) {
					ind = i;
					break;
				}
			}
			pilotFile->labelInd = g_list_append( pilotFile->labelInd, GINT_TO_POINTER(ind) );
			node = g_list_next( node );
		}
		retVal = TRUE;
	}
	return retVal;
}

/*
* Load list with character strings of label names.
*/
GList *jpilot_load_label( JPilotFile *pilotFile, GList *labelList ) {
	int i;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	if( pilotFile->readMetadata ) {
		struct AddressAppInfo *ai = & pilotFile->addrInfo;
		for( i = 0; i < JPILOT_NUM_LABELS; i++ ) {
			gchar *labelName = ai->labels[i];

			if( labelName ) {
				labelName = jpilot_convert_encoding( labelName );
				labelList = g_list_append( labelList, labelName );
			}
			else {
				labelList = g_list_append( labelList, g_strdup( "" ) );
			}
		}
	}
	return labelList;
}

/*
* Return category name for specified category ID.
* Enter:  Category ID.
* Return: Name, or empty string if not invalid ID. Name should be g_free() when done.
*/
gchar *jpilot_get_category_name( JPilotFile *pilotFile, gint catID ) {
	gchar *catName = NULL;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	if( pilotFile->readMetadata ) {
		struct AddressAppInfo *ai = & pilotFile->addrInfo;
		struct CategoryAppInfo *cat = &	ai->category;
		if( catID < 0 || catID > JPILOT_NUM_CATEG ) {
		}
		else {
			catName = g_strdup( cat->name[catID] );
		}
	}
	if( ! catName ) catName = g_strdup( "" );
	return catName;
}

/*
* Load list with character strings of phone label names.
*/
GList *jpilot_load_phone_label( JPilotFile *pilotFile, GList *labelList ) {
	gint i;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	if( pilotFile->readMetadata ) {
		struct AddressAppInfo *ai = & pilotFile->addrInfo;
		for( i = 0; i < JPILOT_NUM_PHONELABELS; i++ ) {
			gchar	*labelName = ai->phoneLabels[i];
			if( labelName ) {
				labelList = g_list_append( labelList, g_strdup( labelName ) );
			}
			else {
				labelList = g_list_append( labelList, g_strdup( "" ) );
			}
		}
	}
	return labelList;
}

/*
* Load list with character strings of label names. Only none blank names
* are loaded.
*/
GList *jpilot_load_custom_label( JPilotFile *pilotFile, GList *labelList ) {
	gint i;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	if( pilotFile->readMetadata ) {
		struct AddressAppInfo *ai = & pilotFile->addrInfo;
		for( i = 0; i < NUM_CUSTOM_LABEL; i++ ) {
			gchar *labelName = ai->labels[i+IND_CUSTOM_LABEL];
			if( labelName ) {
				g_strchomp( labelName );
				g_strchug( labelName );
				if( *labelName != '\0' ) {
					labelName = jpilot_convert_encoding( labelName );
					labelList = g_list_append( labelList, labelName );
				}
			}
		}
	}
	return labelList;
}

/*
* Load list with character strings of category names.
*/
GList *jpilot_get_category_list( JPilotFile *pilotFile ) {
	GList *catList = NULL;
	gint i;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	if( pilotFile->readMetadata ) {
		struct AddressAppInfo *ai = & pilotFile->addrInfo;
		struct CategoryAppInfo *cat = &	ai->category;
		for( i = 0; i < JPILOT_NUM_CATEG; i++ ) {
			gchar *catName = cat->name[i];
			if( catName ) {
				catList = g_list_append( catList, g_strdup( catName ) );
			}
			else {
				catList = g_list_append( catList, g_strdup( "" ) );
			}
		}
	}
	return catList;
}

/*
* Build folder for each category.
*/
static void jpilot_build_category_list( JPilotFile *pilotFile ) {
	struct AddressAppInfo *ai = & pilotFile->addrInfo;
	struct CategoryAppInfo *cat = &	ai->category;
	gint i;

	for( i = 0; i < JPILOT_NUM_CATEG; i++ ) {
		ItemFolder *folder = addritem_create_item_folder();
		gchar *catName;

		catName = jpilot_convert_encoding( cat->name[i] );
		addritem_folder_set_name( folder, catName );
		g_free( catName );

		addrcache_id_folder( pilotFile->addressCache, folder );
		addrcache_add_folder( pilotFile->addressCache, folder );
	}
}

/*
* Remove empty folders (categories).
*/
static void jpilot_remove_empty( JPilotFile *pilotFile ) {
	GList *listFolder;
	GList *remList;
	GList *node;
	gint i = 0;

	listFolder = addrcache_get_list_folder( pilotFile->addressCache );
	node = listFolder;
	remList = NULL;
	while( node ) {
		ItemFolder *folder = node->data;
		if( ADDRITEM_NAME(folder) == NULL || *ADDRITEM_NAME(folder) == '\0' ) {
			if( folder->listPerson ) {
				/* Give name to folder */
				gchar name[20];
				sprintf( name, "? %d", i );
				addritem_folder_set_name( folder, name );
			}
			else {
				/* Mark for removal */
				remList = g_list_append( remList, folder );
			}
		}
		node = g_list_next( node );
		i++;
	}
	node = remList;
	while( node ) {
		ItemFolder *folder = node->data;
		addrcache_remove_folder( pilotFile->addressCache, folder );
		node = g_list_next( node );
	}
	g_list_free( remList );
}

/*
* ============================================================================================
* Read file into list. Main entry point
* Return: TRUE if file read successfully.
* ============================================================================================
*/
gint jpilot_read_data( JPilotFile *pilotFile ) {
	name_order = FAMILY_LAST;
	convert_charcode = FALSE;

	if( conv_is_ja_locale() ) {
		name_order = FAMILY_FIRST;
		convert_charcode = TRUE;
	}

	g_return_val_if_fail( pilotFile != NULL, -1 );

	pilotFile->retVal = MGU_SUCCESS;
	pilotFile->accessFlag = FALSE;

	if( jpilot_check_files( pilotFile ) ) {
		addrcache_clear( pilotFile->addressCache );
		jpilot_read_metadata( pilotFile );
		if( pilotFile->retVal == MGU_SUCCESS ) {
			jpilot_setup_labels( pilotFile );
			jpilot_build_category_list( pilotFile );
			pilotFile->retVal = jpilot_read_file( pilotFile );
			if( pilotFile->retVal == MGU_SUCCESS ) {
				jpilot_remove_empty( pilotFile );
				jpilot_mark_files( pilotFile );
				pilotFile->addressCache->modified = FALSE;
				pilotFile->addressCache->dataRead = TRUE;
			}
		}
	}
	return pilotFile->retVal;
}

/*
* Return link list of persons.
*/
GList *jpilot_get_list_person( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, NULL );
	return addrcache_get_list_person( pilotFile->addressCache );
}

/*
* Return link list of folders. This is always NULL since there are
* no folders in GnomeCard.
* Return: NULL.
*/
GList *jpilot_get_list_folder( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, NULL );
	return addrcache_get_list_folder( pilotFile->addressCache );
}

/*
* Return link list of all persons. Note that the list contains references
* to items. Do *NOT* attempt to use the addrbook_free_xxx() functions...
* this will destroy the addressbook data!
* Return: List of items, or NULL if none.
*/
GList *jpilot_get_all_persons( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, NULL );
	return addrcache_get_all_persons( pilotFile->addressCache );
}

/*
* Check label list for specified label.
*/
gint jpilot_check_label( struct AddressAppInfo *ai, gchar *lblCheck ) {
	gint i;
	gchar *lblName;

	if( lblCheck == NULL ) return -1;
	if( strlen( lblCheck ) < 1 ) return -1;
	for( i = 0; i < JPILOT_NUM_LABELS; i++ ) {
		lblName = ai->labels[i];
		if( lblName ) {
			if( strlen( lblName ) ) {
				if( g_ascii_strcasecmp( lblName, lblCheck ) == 0 ) return i;
			}
		}
	}
	return -2;
}

/*
* Validate that all parameters specified.
* Return: TRUE if data is good.
*/
gboolean jpilot_validate( const JPilotFile *pilotFile ) {
	gboolean retVal;

	g_return_val_if_fail( pilotFile != NULL, FALSE );

	retVal = TRUE;
	if( pilotFile->path ) {
		if( strlen( pilotFile->path ) < 1 ) retVal = FALSE;
	}
	else {
		retVal = FALSE;
	}
	if( pilotFile->name ) {
		if( strlen( pilotFile->name ) < 1 ) retVal = FALSE;
	}
	else {
		retVal = FALSE;
	}
	return retVal;
}

#define WORK_BUFLEN 1024

/*
* Attempt to find a valid JPilot file.
* Return: Filename, or home directory if not found, or empty string if
* no home. Filename should be g_free() when done.
*/
gchar *jpilot_find_pilotdb( void ) {
	const gchar *homedir;
	gchar str[ WORK_BUFLEN ];
	gint len;
	FILE *fp;

	homedir = get_home_dir();
	if( ! homedir ) return g_strdup( "" );

	strcpy( str, homedir );
	len = strlen( str );
	if( len > 0 ) {
		if( str[ len-1 ] != G_DIR_SEPARATOR ) {
			str[ len ] = G_DIR_SEPARATOR;
			str[ ++len ] = '\0';
		}
	}
	strcat( str, JPILOT_DBHOME_DIR );
	strcat( str, G_DIR_SEPARATOR_S );
	strcat( str, JPILOT_DBHOME_FILE );

	/* Attempt to open */
	if( ( fp = g_fopen( str, "rb" ) ) != NULL ) {
		fclose( fp );
	}
	else {
		/* Truncate filename */
		str[ len ] = '\0';
	}
	return g_strdup( str );
}

/*
* Attempt to read file, testing for valid JPilot format.
* Return: TRUE if file appears to be valid format.
*/
gint jpilot_test_read_file( const gchar *fileSpec ) {
	JPilotFile *pilotFile;
	gint retVal;

	if( fileSpec ) {
		pilotFile = jpilot_create_path( fileSpec );
		retVal = jpilot_read_metadata( pilotFile );
		jpilot_free( pilotFile );
		pilotFile = NULL;
	}
	else {
		retVal = MGU_NO_FILE;
	}
	return retVal;
}

/*
* Check whether label is in custom labels.
* Return: TRUE if found.
*/
gboolean jpilot_test_custom_label( JPilotFile *pilotFile, const gchar *labelName ) {
	gboolean retVal;
	GList *node;

	g_return_val_if_fail( pilotFile != NULL, FALSE );

	retVal = FALSE;
	if( labelName ) {
		node = pilotFile->customLabels;
		while( node ) {
			if( g_ascii_strcasecmp( labelName, node->data ) == 0 ) {
				retVal = TRUE;
				break;
			}
			node = g_list_next( node );
		}
	}
	return retVal;
}

/*
* Test whether pilot link library installed.
* Return: TRUE if library available.
*/
#if 0
gboolean jpilot_test_pilot_lib( void ) {
	void *handle, *fun;

	handle = dlopen( PILOT_LINK_LIB_NAME, RTLD_LAZY );
	if( ! handle ) {
		return FALSE;
	}

	/* Test for symbols we need */
	fun = dlsym( handle, "unpack_Address" );
	if( ! fun ) {
		dlclose( handle );
		return FALSE;
	}

	fun = dlsym( handle, "unpack_AddressAppInfo" );
	if( ! fun ) {
		dlclose( handle );
		return FALSE;
	}
	dlclose( handle );
	return TRUE;
}
#endif /* 0 */

#endif	/* USE_JPILOT */

/*
* End of Source.
*/
