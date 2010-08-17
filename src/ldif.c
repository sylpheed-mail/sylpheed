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
 * Functions necessary to access LDIF files (LDAP Data Interchange Format
 * files).
 */

#include <glib.h>
#include <string.h>
#include <sys/stat.h>

#include "mgutils.h"
#include "ldif.h"
#include "addritem.h"
#include "addrcache.h"

#include "base64.h"
#include "codeconv.h"
#include "utils.h"

/*
* Create new object.
*/
LdifFile *ldif_create() {
	LdifFile *ldifFile;
	ldifFile = g_new0( LdifFile, 1 );
	ldifFile->path = NULL;
	ldifFile->file = NULL;
	ldifFile->bufptr = ldifFile->buffer;
	ldifFile->hashFields = g_hash_table_new( g_str_hash, g_str_equal );
	ldifFile->tempList = NULL;
	ldifFile->dirtyFlag = TRUE;
	ldifFile->accessFlag = FALSE;
	ldifFile->retVal = MGU_SUCCESS;
	ldifFile->cbProgress = NULL;
	ldifFile->importCount = 0;
	return ldifFile;
}

/*
* Properties...
*/
void ldif_set_file( LdifFile *ldifFile, const gchar *value ) {
	g_return_if_fail( ldifFile != NULL );

	if( ldifFile->path ) {
		if( strcmp( ldifFile->path, value ) != 0 )
			ldifFile->dirtyFlag = TRUE;
	}
	else {
		ldifFile->dirtyFlag = TRUE;
	}
	ldifFile->path = mgu_replace_string( ldifFile->path, value );
	g_strstrip( ldifFile->path );
	ldifFile->importCount = 0;
}
void ldif_set_accessed( LdifFile *ldifFile, const gboolean value ) {
	g_return_if_fail( ldifFile != NULL );
	ldifFile->accessFlag = value;
}

/*
* Register a callback function. When called, the function will be passed
* the following arguments:
*	LdifFile object,
*	File size (long),
*	Current position (long)
* This can be used for a progress indicator.
*/
void ldif_set_callback( LdifFile *ldifFile, void *func ) {
	ldifFile->cbProgress = func;
}

/*
* Create field record object.
*/
static Ldif_FieldRec *ldif_create_fieldrec( gchar *field ) {
	Ldif_FieldRec *rec = g_new0( Ldif_FieldRec, 1 );
	rec->tagName = g_strdup( field );
	rec->userName = NULL;
	rec->reserved = FALSE;
	rec->selected = FALSE;
	return rec;
}

/*
* Free field record object.
*/
static void ldif_free_fieldrec( Ldif_FieldRec *rec ) {
	if( rec ) {
		g_free( rec->tagName );
		g_free( rec->userName );
		rec->tagName = NULL;
		rec->userName = NULL;
		rec->reserved = FALSE;
		rec->selected = FALSE;
		g_free( rec );
	}
}

/*
* Free hash table entry visitor function.
*/
static gint ldif_hash_free_vis( gpointer key, gpointer value, gpointer data ) {
	ldif_free_fieldrec( ( Ldif_FieldRec * ) value );
	value = NULL;
	key = NULL;
	return -1;
}

/*
* Free up object by releasing internal memory.
*/
void ldif_free( LdifFile *ldifFile ) {
	g_return_if_fail( ldifFile != NULL );

	/* Close file */
	if( ldifFile->file ) fclose( ldifFile->file );

	/* Free internal stuff */
	g_free( ldifFile->path );

	/* Free field list */
	g_hash_table_foreach_remove( ldifFile->hashFields, ldif_hash_free_vis, NULL );
	g_hash_table_destroy( ldifFile->hashFields );
	ldifFile->hashFields = NULL;

	/* Clear pointers */
	ldifFile->file = NULL;
	ldifFile->path = NULL;
	ldifFile->retVal = MGU_SUCCESS;
	ldifFile->tempList = NULL;
	ldifFile->dirtyFlag = FALSE;
	ldifFile->accessFlag = FALSE;
	ldifFile->cbProgress = NULL;

	/* Now release file object */
	g_free( ldifFile );
}

/*
* Display field record.
*/
void ldif_print_fieldrec( Ldif_FieldRec *rec, FILE *stream ) {
	fprintf( stream, "\ttag:\t%s", rec->reserved ? "yes" : "no" );
	fprintf( stream, "\t%s", rec->selected ? "yes" : "no" );
	fprintf( stream, "\t:%s:\t:%s:\n", rec->userName, rec->tagName );
}

/*
* Display field record.
 * 
*/
static void ldif_print_file_vis( gpointer key, gpointer value, gpointer data ) {
	Ldif_FieldRec *rec = value;
	FILE *stream = data;
	ldif_print_fieldrec( rec, stream );
}

/*
* Display object to specified stream.
*/
void ldif_print_file( LdifFile *ldifFile, FILE *stream ) {
	g_return_if_fail( ldifFile != NULL );
	fprintf( stream, "LDIF File:\n" );
	fprintf( stream, "file spec: '%s'\n", ldifFile->path );
	fprintf( stream, "  ret val: %d\n",   ldifFile->retVal );
	fprintf( stream, "   fields: {\n" );
	g_hash_table_foreach( ldifFile->hashFields, ldif_print_file_vis, stream );
	fprintf( stream, "} ---\n" );
}

/*
* Open file for read.
* return: TRUE if file opened successfully.
*/
static gint ldif_open_file( LdifFile* ldifFile ) {
	/* printf( "Opening file\n" ); */
	if( ldifFile->path ) {
		ldifFile->file = g_fopen( ldifFile->path, "rb" );
		if( ! ldifFile->file ) {
			/* printf( "can't open %s\n", ldifFile->path ); */
			ldifFile->retVal = MGU_OPEN_FILE;
			return ldifFile->retVal;
		}
	}
	else {
		/* printf( "file not specified\n" ); */
		ldifFile->retVal = MGU_NO_FILE;
		return ldifFile->retVal;
	}

	/* Setup a buffer area */
	ldifFile->buffer[0] = '\0';
	ldifFile->bufptr = ldifFile->buffer;
	ldifFile->retVal = MGU_SUCCESS;
	return ldifFile->retVal;
}

/*
* Close file.
*/
static void ldif_close_file( LdifFile *ldifFile ) {
	g_return_if_fail( ldifFile != NULL );
	if( ldifFile->file ) fclose( ldifFile->file );
	ldifFile->file = NULL;
}

/*
* Read line of text from file.
* Return: ptr to buffer where line starts.
*/
static gchar *ldif_get_line( LdifFile *ldifFile ) {
	gchar buf[ LDIFBUFSIZE ];

	if( feof( ldifFile->file ) )
		return NULL;

	if( fgets( buf, sizeof( buf ), ldifFile->file ) == NULL )
		return NULL;

	strretchomp( buf );

	/* Return a copy of buffer */
	return g_strdup( buf );
}

/*
* Parse tag name from line buffer.
* Enter: line   Buffer.
*        flag64 Base-64 encoder flag.
* Return: Buffer containing the tag name, or NULL if no delimiter char found.
* If a double delimiter (::) is found, flag64 is set.
*/
static gchar *ldif_get_tagname( char* line, gboolean *flag64 ) {
	gint len = 0;
	gchar *tag = NULL;
	gchar *lptr = line;
	gchar *sptr = NULL;

	while( *lptr++ ) {
		/* Check for language tag */
		if( *lptr == LDIF_LANG_TAG ) {
			if( sptr == NULL ) sptr = lptr;
		}

		/* Check for delimiter */
		if( *lptr == LDIF_SEP_TAG ) {
			if( sptr ) {
				len = sptr - line;
			}
			else {
				len = lptr - line;
			}

			/* Base-64 encoding? */
			if( * ++lptr == LDIF_SEP_TAG ) *flag64 = TRUE;

			tag = g_strndup( line, len+1 );
			tag[ len ] = '\0';
			g_strdown( tag );
			return tag;
		}
	}
	return tag;
}

/*
* Parse tag value from line buffer.
* Enter: line   Buffer.
* Return: Buffer containing the tag value. Empty string is returned if
* no delimiter char found.
*/
static gchar *ldif_get_tagvalue( gchar* line ) {
	gchar *value = NULL;
	gchar *start = NULL;
	gchar *lptr;
	gint len = 0;

	for( lptr = line; *lptr; lptr++ ) {
		if( *lptr == LDIF_SEP_TAG ) {
			if( ! start )
				start = lptr + 1;
		}
	}
	if( start ) {
		if( *start == LDIF_SEP_TAG ) start++;
		len = lptr - start;
		value = g_strndup( start, len+1 );
		g_strstrip( value );
	}
	else {
		/* Ensure that we get an empty string */
		value = g_strndup( "", 1 );
	}
	value[ len ] = '\0';
	return value;
}

#if 0
/*
* Dump linked lists of character strings (for debug).
*/
static void ldif_dump_lists( GSList *listName, GSList *listAddr, GSList *listRem, GSList *listID, FILE *stream ) {
	fprintf( stream, "dump name\n" );
	fprintf( stream, "------------\n" );
	mgu_print_list( listName, stdout );
	fprintf( stream, "dump address\n" );
	fprintf( stream, "------------\n" );
	mgu_print_list( listAddr, stdout );
	fprintf( stream, "dump remarks\n" );
	fprintf( stdout, "------------\n" );
	mgu_print_list( listRem, stdout );
	fprintf( stream, "dump id\n" );
	fprintf( stdout, "------------\n" );
	mgu_print_list( listID, stdout );
}
#endif

/*
* Parsed address data.
*/
typedef struct _Ldif_ParsedRec_ Ldif_ParsedRec;
struct _Ldif_ParsedRec_ {
	GSList *listCName;
	GSList *listFName;
	GSList *listLName;
	GSList *listNName;
	GSList *listAddress;
	GSList *listID;
	GSList *userAttr;
};

/*
* User attribute data.
*/
typedef struct _Ldif_UserAttr_ Ldif_UserAttr;
struct _Ldif_UserAttr_ {
	gchar *name;
	gchar *value;
};

/*
* Build an address list entry and append to list of address items. Name is formatted
* as "<first-name> <last-name>".
*/
static void ldif_build_items( LdifFile *ldifFile, Ldif_ParsedRec *rec, AddressCache *cache ) {
	GSList *nodeFirst;
	GSList *nodeAddress;
	GSList *nodeAttr;
	gchar *firstName = NULL, *lastName = NULL, *fullName = NULL, *nickName = NULL;
	gint iLen = 0, iLenT = 0;
	ItemPerson *person;
	ItemEMail *email;
	gboolean familyFirst = FALSE;

	nodeAddress = rec->listAddress;
	if( nodeAddress == NULL ) return;

	/* Find longest first name in list */
	nodeFirst = rec->listFName;
	while( nodeFirst ) {
		if( firstName == NULL ) {
			firstName = nodeFirst->data;
			iLen = strlen( firstName );
		}
		else {
			if( ( iLenT = strlen( nodeFirst->data ) ) > iLen ) {
				firstName = nodeFirst->data;
				iLen = iLenT;
			}
		}
		nodeFirst = g_slist_next( nodeFirst );
	}

	/* Format name */
	if( rec->listLName ) {
		lastName = rec->listLName->data;
	}
	if( rec->listCName ) {
		fullName = g_strdup((gchar *)rec->listCName->data);
	}

	familyFirst = conv_is_ja_locale();

	if( fullName == NULL ) {
		if( familyFirst ) {
			if( lastName ) {
				if( firstName ) {
					fullName = g_strdup_printf( "%s %s", lastName, firstName );
				}
				else {
					fullName = g_strdup_printf( "%s", lastName );
				}
			}
			else {
				if( firstName ) {
					fullName = g_strdup_printf( "%s", firstName );
				}
			}
		} else {
			if( firstName ) {
				if( lastName ) {
					fullName = g_strdup_printf( "%s %s", firstName, lastName );
				}
				else {
					fullName = g_strdup_printf( "%s", firstName );
				}
			}
			else {
				if( lastName ) {
					fullName = g_strdup_printf( "%s", lastName );
				}
			}
		}
	}
	if( fullName ) {
		g_strstrip( fullName );
	}

	if( rec->listNName ) {
		nickName = rec->listNName->data;
	}

	person = addritem_create_item_person();
	addritem_person_set_common_name( person, fullName );
	addritem_person_set_first_name( person, firstName );
	addritem_person_set_last_name( person, lastName );
	addritem_person_set_nick_name( person, nickName );
	addrcache_id_person( cache, person );
	addrcache_add_person( cache, person );
	++ldifFile->importCount;

	/* Add address item */
	while( nodeAddress ) {
		email = addritem_create_item_email();
		addritem_email_set_address( email, nodeAddress->data );
		addrcache_id_email( cache, email );
		addrcache_person_add_email( cache, person, email );
		nodeAddress = g_slist_next( nodeAddress );
	}
	g_free( fullName );
	fullName = firstName = lastName = NULL;

	/* Add user attributes */
	nodeAttr = rec->userAttr;
	while( nodeAttr ) {
		Ldif_UserAttr *attr = nodeAttr->data;
		UserAttribute *attrib = addritem_create_attribute();
		addritem_attrib_set_name( attrib, attr->name );
		addritem_attrib_set_value( attrib, attr->value );
		addritem_person_add_attribute( person, attrib );
		nodeAttr = g_slist_next( nodeAttr );
	}
	nodeAttr = NULL;
}

/*
* Add selected field as user attribute.
*/
static void ldif_add_user_attr( Ldif_ParsedRec *rec, gchar *tagName, gchar *tagValue, GHashTable *hashField ) {
	Ldif_FieldRec *fld = NULL;
	Ldif_UserAttr *attr = NULL;
	gchar *name;

	fld = g_hash_table_lookup( hashField, tagName );
	if( fld ) {
		if( fld->reserved ) return;
		if( ! fld->selected ) return;

		name = fld->tagName;
		if( fld->userName && *(fld->userName) ) {
			name = fld->userName;
		}
		attr = g_new0( Ldif_UserAttr, 1 );
		attr->name = g_strdup( name );
		attr->value = g_strdup( tagValue );
		rec->userAttr = g_slist_append( rec->userAttr, attr );
	}
}

/*
* Add value to parsed data.
*/
static void ldif_add_value( Ldif_ParsedRec *rec, gchar *tagName, gchar *tagValue, GHashTable *hashField ) {
	gchar *nm, *val;

	nm = g_strdup( tagName );
	g_strdown( nm );
	if( tagValue ) {
		val = g_strdup( tagValue );
	}
	else {
		val = g_strdup( "" );
	}
	g_strstrip( val );
	if( g_ascii_strcasecmp( nm, LDIF_TAG_COMMONNAME ) == 0 ) {
		rec->listCName = g_slist_append( rec->listCName, val );
	}
	else if( g_ascii_strcasecmp( nm, LDIF_TAG_FIRSTNAME ) == 0 ) {
		rec->listFName = g_slist_append( rec->listFName, val );
	}
	else if( g_ascii_strcasecmp( nm, LDIF_TAG_LASTNAME ) == 0 ) {
		rec->listLName = g_slist_append( rec->listLName, val );
	}
	else if( g_ascii_strcasecmp( nm, LDIF_TAG_NICKNAME ) == 0 ||
	         g_ascii_strcasecmp( nm, LDIF_TAG_NICKNAME2 ) == 0 ||
	         g_ascii_strcasecmp( nm, LDIF_TAG_XNICKNAME ) == 0 ) {
		rec->listNName = g_slist_append( rec->listNName, val );
	}
	else if( g_ascii_strcasecmp( nm, LDIF_TAG_EMAIL ) == 0 ) {
		if( g_slist_find_custom( rec->listAddress, val, (GCompareFunc)strcmp2 )) {
			/* skip duplicated address */
			g_free( val );
		} else {
			rec->listAddress = g_slist_append( rec->listAddress, val );
		}
	}
	else {
		/* Add field as user attribute */
		ldif_add_user_attr( rec, tagName, tagValue, hashField );
		g_free( val );
	}
	g_free( nm );
}

/*
* Clear parsed data.
*/
static void ldif_clear_rec( Ldif_ParsedRec *rec ) {
	GSList *list;

	/* Free up user attributes */
	list = rec->userAttr;
	while( list ) {
		Ldif_UserAttr *attr = list->data;
		g_free( attr->name );
		g_free( attr->value );
		g_free( attr );
		list = g_slist_next( list );
	}
	g_slist_free( rec->userAttr );

	g_slist_free( rec->listCName );
	g_slist_free( rec->listFName );
	g_slist_free( rec->listLName );
	g_slist_free( rec->listNName );
	g_slist_free( rec->listAddress );
	g_slist_free( rec->listID );

	rec->userAttr = NULL;
	rec->listCName = NULL;
	rec->listFName = NULL;
	rec->listLName = NULL;
	rec->listNName = NULL;
	rec->listAddress = NULL;
	rec->listID = NULL;
}

#if 0
/*
* Print parsed data.
*/
static void ldif_print_record( Ldif_ParsedRec *rec, FILE *stream ) {
	GSList *list;

	fprintf( stream, "LDIF Parsed Record:\n" );
	fprintf( stream, "common name:" );
	mgu_print_list( rec->listCName, stream );
	if( ! rec->listCName ) fprintf( stream, "\n" );
	fprintf( stream, "first name:" );
	mgu_print_list( rec->listFName, stream );
	if( ! rec->listFName ) fprintf( stream, "\n" );
	fprintf( stream, "last name:" );
	mgu_print_list( rec->listLName, stream );
	if( ! rec->listLName ) fprintf( stream, "\n" );
	fprintf( stream, "nick name:" );
	mgu_print_list( rec->listNName, stream );
	if( ! rec->listNName ) fprintf( stream, "\n" );
	fprintf( stream, "address:" );
	mgu_print_list( rec->listAddress, stream );
	if( ! rec->listAddress ) fprintf( stream, "\n" );
	fprintf( stream, "id:" );
	mgu_print_list( rec->listID, stream );
	if( ! rec->listID ) fprintf( stream, "\n" );

	list = rec->userAttr;
	while( list ) {
		Ldif_UserAttr *attr = list->data;
		fprintf( stream, "n/v:\t%s:\t:%s:\n", attr->name, attr->value );
		list = g_slist_next( list );
	}
	list = NULL;
}

static void ldif_dump_b64( gchar *buf ) {
	Base64Decoder *decoder = NULL;
	gchar outBuf[8192];
	gint len;

	g_print( "base-64 : inbuf : %s\n", buf );
	decoder = base64_decoder_new();
	len = base64_decoder_decode( decoder, buf, outBuf );
	if (len < 0) {
		g_print( "base-64 : Bad BASE64 content\n" );
	}
	else {
		outBuf[len] = '\0';
		g_print( "base-64 : %d : %s\n\n", len, outBuf );
	}
	base64_decoder_free( decoder );
	decoder = NULL;
}
#endif

static gchar *ldif_conv_base64( gchar *buf ) {
	gchar *outbuf;
	gint len;

	outbuf = g_malloc(strlen(buf) + 1);
	len = base64_decode((guchar *)outbuf, buf, -1);
	outbuf[len] = '\0';
	if (g_utf8_validate(outbuf, -1, NULL))
		return outbuf;
	else {
		gchar *utf8str;

		if (conv_is_ja_locale())
			utf8str = conv_codeset_strdup(outbuf, NULL, NULL);
		else
			utf8str = conv_localetodisp(outbuf, NULL);

		g_free(outbuf);
		return utf8str;
	}
}

/*
* Read file data into address cache.
* Note that one LDIF record identifies one entity uniquely with the
* distinguished name (dn) tag. Each person can have multiple E-Mail
* addresses. Also, each person can have many common name (cn) tags.
*/
static void ldif_read_file( LdifFile *ldifFile, AddressCache *cache ) {
	gchar *tagName = NULL, *tagValue = NULL;
	gchar *lastTag = NULL;
	GSList *listValue = NULL;
	gboolean flagEOF = FALSE, flagEOR = FALSE;
	gboolean flag64 = FALSE, last64 = FALSE;
	Ldif_ParsedRec *rec;
	long posEnd = 0L;
	long posCur = 0L;
	GHashTable *hashField;

	hashField = ldifFile->hashFields;
	rec = g_new0( Ldif_ParsedRec, 1 );
	ldif_clear_rec( rec );

	/* Find EOF for progress indicator */
	fseek( ldifFile->file, 0L, SEEK_END );
	posEnd = ftell( ldifFile->file );
	fseek( ldifFile->file, 0L, SEEK_SET );

	while( ! flagEOF ) {
		gchar *line = ldif_get_line( ldifFile );

		posCur = ftell( ldifFile->file );
		if( ldifFile->cbProgress ) {
			/* Call progress indicator */
			( ldifFile->cbProgress ) ( ldifFile, & posEnd, & posCur );
		}

		flag64 = FALSE;
		if( line == NULL ) {
			flagEOF = flagEOR = TRUE;
		}
		else if( *line == '\0' ) {
			flagEOR = TRUE;
		}

		if( flagEOR ) {
			/* EOR, Output address data */
			if( lastTag ) {
				gchar *fullValue;

				/* Save record */
				fullValue = mgu_list_coalesce( listValue );

				/* Base-64 encoded data */
				if( last64 ) {
					gchar *decValue;
					decValue = ldif_conv_base64( fullValue );
					g_free( fullValue );
					fullValue = decValue;
				}

				ldif_add_value( rec, lastTag, fullValue, hashField );
				/* ldif_print_record( rec, stdout ); */
				ldif_build_items( ldifFile, rec, cache );
				ldif_clear_rec( rec );
				g_free( fullValue );
				g_free( lastTag );
				mgu_free_list( listValue );
				lastTag = NULL;
				listValue = NULL;
				last64 = FALSE;
			}
		}
		if( line ) {
			flagEOR = FALSE;
			if( *line == ' ' ) {
				/* Continuation line */
				listValue = g_slist_append( listValue, g_strdup( line+1 ) );
			}
			else if( *line == '=' ) {
				/* Base-64 encoded continuation field */
				listValue = g_slist_append( listValue, g_strdup( line ) );
			}
			else {
				/* Parse line */
				tagName = ldif_get_tagname( line, &flag64 );
				if( tagName ) {
					tagValue = ldif_get_tagvalue( line );
					if( tagValue ) {
						if( lastTag ) {
							gchar *fullValue;

							/* Save data */
							fullValue = mgu_list_coalesce( listValue );
							/* Base-64 encoded data */
							if( last64 ) {
								gchar *decValue;
								decValue = ldif_conv_base64( fullValue );
								g_free( fullValue );
								fullValue = decValue;
							}

							ldif_add_value( rec, lastTag, fullValue, hashField );
							g_free( fullValue );
							g_free( lastTag );
							mgu_free_list( listValue );
							lastTag = NULL;
							listValue = NULL;
							last64 = FALSE;
						}

						lastTag = g_strdup( tagName );
						listValue = g_slist_append( listValue, g_strdup( tagValue ) );
						g_free( tagValue );
						last64 = flag64;
					}
					g_free( tagName );
				}
			}
		}
		g_free( line );
	}

	/* Release data */
	ldif_clear_rec( rec );
	g_free( rec );
	g_free( lastTag );
	mgu_free_list( listValue );
}

/*
* Add list of field names to hash table.
*/
static void ldif_hash_add_list( GHashTable *table, GSList *list ) {
	GSList *node = list;

	/* mgu_print_list( list, stdout ); */
	while( node ) {
		gchar *tag = node->data;
		if( ! g_hash_table_lookup( table, tag ) ) {
			Ldif_FieldRec *rec = NULL;
			gchar *key = g_strdup( tag );

			rec = ldif_create_fieldrec( tag );
			if( g_ascii_strcasecmp( tag, LDIF_TAG_COMMONNAME ) == 0 ) {
				rec->reserved = TRUE;
			}
			else if( g_ascii_strcasecmp( tag, LDIF_TAG_FIRSTNAME ) == 0 ) {
				rec->reserved = TRUE;
			}
			else if( g_ascii_strcasecmp( tag, LDIF_TAG_LASTNAME ) == 0 ) {
				rec->reserved = TRUE;
			}
			else if( g_ascii_strcasecmp( tag, LDIF_TAG_NICKNAME ) == 0 ||
				 g_ascii_strcasecmp( tag, LDIF_TAG_XNICKNAME) == 0 ||
				 g_ascii_strcasecmp( tag, LDIF_TAG_NICKNAME2) == 0 ) {
				rec->reserved = TRUE;
			}
			else if( g_ascii_strcasecmp( tag, LDIF_TAG_EMAIL ) == 0 ) {
				rec->reserved = TRUE;
			}
			g_hash_table_insert( table, key, rec );
		}
		node = g_slist_next( node );
	}
}

/*
* Sorted list comparison function.
*/
static int ldif_field_compare( gconstpointer ptr1, gconstpointer ptr2 ) {
	const Ldif_FieldRec *rec1 = ptr1;
	const Ldif_FieldRec *rec2 = ptr2;
	return g_ascii_strcasecmp( rec1->tagName, rec2->tagName );
}

/*
* Append hash table entry to list - visitor function.
*/
static void ldif_hash2list_vis( gpointer key, gpointer value, gpointer data ) {
	LdifFile *ldf = data;
	ldf->tempList = g_list_insert_sorted( ldf->tempList, value, ldif_field_compare );
}

/*
* Read tag names for file data.
*/
static void ldif_read_tag_list( LdifFile *ldifFile ) {
	gchar *tagName = NULL;
	GSList *listTags = NULL;
	gboolean flagEOF = FALSE, flagEOR = FALSE, flagMail = FALSE;
	gboolean flag64 = FALSE;
	long posEnd = 0L;
	long posCur = 0L;

	/* Clear hash table */
	g_hash_table_foreach_remove( ldifFile->hashFields, ldif_hash_free_vis, NULL );

	/* Find EOF for progress indicator */
	fseek( ldifFile->file, 0L, SEEK_END );
	posEnd = ftell( ldifFile->file );
	fseek( ldifFile->file, 0L, SEEK_SET );

	/* Process file */
	while( ! flagEOF ) {
		gchar *line = ldif_get_line( ldifFile );

		posCur = ftell( ldifFile->file );
		if( ldifFile->cbProgress ) {
			/* Call progress indicator */
			( ldifFile->cbProgress ) ( ldifFile, & posEnd, & posCur );
		}

		flag64 = FALSE;
		if( line == NULL ) {
			flagEOF = flagEOR = TRUE;
		}
		else if( *line == '\0' ) {
			flagEOR = TRUE;
		}

		if( flagEOR ) {
			/* EOR, Output address data */
			/* Save field list to hash table */
			if( flagMail ) {
				ldif_hash_add_list( ldifFile->hashFields, listTags );
			}
			mgu_free_list( listTags );
			listTags = NULL;
			flagMail = FALSE;
		}
		if( line ) {
			flagEOR = FALSE;
			if( *line == ' ' ) {
				/* Continuation line */
			}
			else if( *line == '=' ) {
				/* Base-64 encoded continuation field */
			}
			else {
				/* Parse line */
				tagName = ldif_get_tagname( line, &flag64 );
				if( tagName ) {
					/* Add tag to list */
					listTags = g_slist_append( listTags, tagName );
					if( g_ascii_strcasecmp( tagName, LDIF_TAG_EMAIL ) == 0 ) {
						flagMail = TRUE;
					}
				}
			}
		}
		g_free( line );
	}

	/* Release data */
	mgu_free_list( listTags );
	listTags = NULL;
}

/*
* ============================================================================
* Read file into list. Main entry point
* Enter:  ldifFile LDIF control data.
*         cache    Address cache to load.
* Return: Status code.
* ============================================================================
*/
gint ldif_import_data( LdifFile *ldifFile, AddressCache *cache ) {
	g_return_val_if_fail( ldifFile != NULL, MGU_BAD_ARGS );
	ldifFile->retVal = MGU_SUCCESS;
	addrcache_clear( cache );
	cache->dataRead = FALSE;
	ldif_open_file( ldifFile );
	if( ldifFile->retVal == MGU_SUCCESS ) {
		/* Read data into the cache */
		ldif_read_file( ldifFile, cache );
		ldif_close_file( ldifFile );

		/* Mark cache */
		cache->modified = FALSE;
		cache->dataRead = TRUE;
	}
	return ldifFile->retVal;
}

/*
* ============================================================================
* Process entire file reading list of unique fields. List of fields may be
* accessed with the ldif_get_fieldlist() function.
* Enter:  ldifFile LDIF control data.
* Return: Status code.
* ============================================================================
*/
gint ldif_read_tags( LdifFile *ldifFile ) {
	g_return_val_if_fail( ldifFile != NULL, MGU_BAD_ARGS );
	ldifFile->retVal = MGU_SUCCESS;
	if( ldifFile->dirtyFlag ) {
		ldif_open_file( ldifFile );
		if( ldifFile->retVal == MGU_SUCCESS ) {
			/* Read data into the cache */
			ldif_read_tag_list( ldifFile );
			ldif_close_file( ldifFile );
			ldifFile->dirtyFlag = FALSE;
			ldifFile->accessFlag = TRUE;
		}
	}
	return ldifFile->retVal;
}

/*
* Return list of fields for LDIF file.
* Enter: ldifFile LdifFile object.
* Return: Linked list of Ldif_FieldRec objects. This list may be g_free'd.
* Note that the objects in the list should not be freed since they refer to
* objects inside the internal cache. These objects will be freed when
* LDIF file object is freed.
*/
GList *ldif_get_fieldlist( LdifFile *ldifFile ) {
	GList *list = NULL;

	g_return_val_if_fail( ldifFile != NULL, NULL );
	if( ldifFile->hashFields ) {
		ldifFile->tempList = NULL;
		g_hash_table_foreach( ldifFile->hashFields, ldif_hash2list_vis, ldifFile );
		list = ldifFile->tempList;
		ldifFile->tempList = NULL;
	}
	return list;
}

/*
* End of Source.
*/

