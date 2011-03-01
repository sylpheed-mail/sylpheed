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
 * General functions for accessing address index file.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "mgutils.h"
#include "addritem.h"
#include "addrcache.h"
#include "addrbook.h"
#include "addrindex.h"
#include "xml.h"
#include "utils.h"

#ifndef DEV_STANDALONE
#include "prefs.h"
#include "codeconv.h"
#endif

#include "vcard.h"

#ifdef USE_JPILOT
#include "jpilot.h"
#endif

#ifdef USE_LDAP
#include "syldap.h"
#endif

#define TAG_ADDRESS_INDEX    "addressbook"

#define TAG_IF_ADDRESS_BOOK  "book_list"
#define TAG_IF_VCARD         "vcard_list"
#define TAG_IF_JPILOT        "jpilot_list"
#define TAG_IF_LDAP          "ldap_list"

#define TAG_DS_ADDRESS_BOOK  "book"
#define TAG_DS_VCARD         "vcard"
#define TAG_DS_JPILOT        "jpilot"
#define TAG_DS_LDAP          "server"

/* XML Attribute names */
#define ATTAG_BOOK_NAME       "name"
#define ATTAG_BOOK_FILE       "file"

#define ATTAG_VCARD_NAME      "name"
#define ATTAG_VCARD_FILE      "file"

#define ATTAG_JPILOT_NAME     "name"
#define ATTAG_JPILOT_FILE     "file"
#define ATTAG_JPILOT_CUSTOM_1 "custom-1"
#define ATTAG_JPILOT_CUSTOM_2 "custom-2"
#define ATTAG_JPILOT_CUSTOM_3 "custom-3"
#define ATTAG_JPILOT_CUSTOM_4 "custom-4"
#define ATTAG_JPILOT_CUSTOM   "custom-"

#define ATTAG_LDAP_NAME       "name"
#define ATTAG_LDAP_HOST       "host"
#define ATTAG_LDAP_PORT       "port"
#define ATTAG_LDAP_BASE_DN    "base-dn"
#define ATTAG_LDAP_BIND_DN    "bind-dn"
#define ATTAG_LDAP_BIND_PASS  "bind-pass"
#define ATTAG_LDAP_CRITERIA   "criteria"
#define ATTAG_LDAP_MAX_ENTRY  "max-entry"
#define ATTAG_LDAP_TIMEOUT    "timeout"

#if 0
N_("Common address")
N_("Personal address")
#endif

#define DISP_NEW_COMMON       _("Common address")
#define DISP_NEW_PERSONAL     _("Personal address")

/* Old address book */
#define TAG_IF_OLD_COMMON     "common_address"
#define TAG_IF_OLD_PERSONAL   "personal_address"

#define DISP_OLD_COMMON       _("Common address")
#define DISP_OLD_PERSONAL     _("Personal address")

typedef struct _AddressIfAttr AddressIfAttrib;
struct _AddressIfAttr {
	gchar *name;
	gchar *value;
};

/*
* Build interface with default values.
*/
static AddressInterface *addrindex_create_interface( gint type, gchar *name, gchar *tagIf, gchar *tagDS ) {
	AddressInterface *iface = g_new0( AddressInterface, 1 );
	ADDRITEM_TYPE(iface) = ITEMTYPE_INTERFACE;
	ADDRITEM_ID(iface) = NULL;
	ADDRITEM_NAME(iface) = g_strdup( name );
	ADDRITEM_PARENT(iface) = NULL;
	ADDRITEM_SUBTYPE(iface) = type;
	iface->type = type;
	iface->name = g_strdup( name );
	iface->listTag = g_strdup( tagIf );
	iface->itemTag = g_strdup( tagDS );
	iface->legacyFlag = FALSE;
	iface->haveLibrary = TRUE;
	iface->useInterface = TRUE;
	iface->readOnly      = TRUE;
	iface->getAccessFlag = NULL;
	iface->getModifyFlag = NULL;
	iface->getReadFlag   = NULL;
	iface->getStatusCode = NULL;
	iface->getReadData   = NULL;
	iface->getRootFolder = NULL;
	iface->getListFolder = NULL;
	iface->getListPerson = NULL;
	iface->getAllPersons = NULL;
	iface->getAllGroups  = NULL;
	iface->getName       = NULL;
	iface->listSource = NULL;
	return iface;
}

/*
* Build table of interfaces.
*/
static void addrindex_build_if_list( AddressIndex *addrIndex ) {
	AddressInterface *iface;

	iface = addrindex_create_interface( ADDR_IF_BOOK, "Address Book", TAG_IF_ADDRESS_BOOK, TAG_DS_ADDRESS_BOOK );
	iface->readOnly      = FALSE;
	iface->getModifyFlag = ( void * ) addrbook_get_modified;
	iface->getAccessFlag = ( void * ) addrbook_get_accessed;
	iface->getReadFlag   = ( void * ) addrbook_get_read_flag;
	iface->getStatusCode = ( void * ) addrbook_get_status;
	iface->getReadData   = ( void * ) addrbook_read_data;
	iface->getRootFolder = ( void * ) addrbook_get_root_folder;
	iface->getListFolder = ( void * ) addrbook_get_list_folder;
	iface->getListPerson = ( void * ) addrbook_get_list_person;
	iface->getAllPersons = ( void * ) addrbook_get_all_persons;
	iface->getName       = ( void * ) addrbook_get_name;
	iface->setAccessFlag = ( void * ) addrbook_set_accessed;
	addrIndex->interfaceList = g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

	iface = addrindex_create_interface( ADDR_IF_VCARD, "vCard", TAG_IF_VCARD, TAG_DS_VCARD );
	iface->getModifyFlag = ( void * ) vcard_get_modified;
	iface->getAccessFlag = ( void * ) vcard_get_accessed;
	iface->getReadFlag   = ( void * ) vcard_get_read_flag;
	iface->getStatusCode = ( void * ) vcard_get_status;
	iface->getReadData   = ( void * ) vcard_read_data;
	iface->getRootFolder = ( void * ) vcard_get_root_folder;
	iface->getListFolder = ( void * ) vcard_get_list_folder;
	iface->getListPerson = ( void * ) vcard_get_list_person;
	iface->getAllPersons = ( void * ) vcard_get_all_persons;
	iface->getName       = ( void * ) vcard_get_name;
	iface->setAccessFlag = ( void * ) vcard_set_accessed;
	addrIndex->interfaceList = g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

	iface = addrindex_create_interface( ADDR_IF_JPILOT, "JPilot", TAG_IF_JPILOT, TAG_DS_JPILOT );
#ifdef USE_JPILOT
	/* iface->haveLibrary = jpilot_test_pilot_lib(); */
	iface->haveLibrary = TRUE;
	iface->useInterface = iface->haveLibrary;
	iface->getModifyFlag = ( void * ) jpilot_get_modified;
	iface->getAccessFlag = ( void * ) jpilot_get_accessed;
	iface->getReadFlag   = ( void * ) jpilot_get_read_flag;
	iface->getStatusCode = ( void * ) jpilot_get_status;
	iface->getReadData   = ( void * ) jpilot_read_data;
	iface->getRootFolder = ( void * ) jpilot_get_root_folder;
	iface->getListFolder = ( void * ) jpilot_get_list_folder;
	iface->getListPerson = ( void * ) jpilot_get_list_person;
	iface->getAllPersons = ( void * ) jpilot_get_all_persons;
	iface->getName       = ( void * ) jpilot_get_name;
	iface->setAccessFlag = ( void * ) jpilot_set_accessed;
#else
	iface->useInterface = FALSE;
	iface->haveLibrary = FALSE;
#endif
	addrIndex->interfaceList = g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

	iface = addrindex_create_interface( ADDR_IF_LDAP, "LDAP", TAG_IF_LDAP, TAG_DS_LDAP );
#ifdef USE_LDAP
	/* iface->haveLibrary = syldap_test_ldap_lib(); */
	iface->haveLibrary = TRUE;
	iface->useInterface = iface->haveLibrary;
	iface->getAccessFlag = ( void * ) syldap_get_accessed;
	/* iface->getModifyFlag = ( void * ) syldap_get_modified; */
	/* iface->getReadFlag   = ( void * ) syldap_get_read_flag; */
	iface->getStatusCode = ( void * ) syldap_get_status;
	iface->getReadData   = ( void * ) syldap_read_data;
	iface->getRootFolder = ( void * ) syldap_get_root_folder;
	iface->getListFolder = ( void * ) syldap_get_list_folder;
	iface->getListPerson = ( void * ) syldap_get_list_person;
	iface->getName       = ( void * ) syldap_get_name;
	iface->setAccessFlag = ( void * ) syldap_set_accessed;
#else
	iface->useInterface = FALSE;
	iface->haveLibrary = FALSE;
#endif
	addrIndex->interfaceList = g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

	/* Two old legacy data sources */
	iface = addrindex_create_interface( ADDR_IF_COMMON, "Old Address - common", TAG_IF_OLD_COMMON, NULL );
	iface->legacyFlag = TRUE;
	addrIndex->interfaceList = g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

	iface = addrindex_create_interface( ADDR_IF_COMMON, "Old Address - personal", TAG_IF_OLD_PERSONAL, NULL );
	iface->legacyFlag = TRUE;
	addrIndex->interfaceList = g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

}

/*
* Free name-value pairs.
*/
static void addrindex_free_attributes( GList *list ) {
	GList *node = list;
	while( node ) {
		AddressIfAttrib *nv = node->data;
		g_free( nv->name ); nv->name = NULL;
		g_free( nv->value ); nv->value = NULL;
		g_free( nv );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( list );
}

/*
* Free up data source.
*/
void addrindex_free_datasource( AddressIndex *addrIndex, AddressDataSource *ds ) {
	AddressInterface *iface = NULL;
	g_return_if_fail( addrIndex != NULL );
	g_return_if_fail( ds != NULL );

	if( ds->iface == NULL ) {
		iface = addrindex_get_interface( addrIndex, ds->type );
	}
	if( iface == NULL ) return;

	if( iface->useInterface ) {
		if( iface->type == ADDR_IF_BOOK ) {
			AddressBookFile *abf = ds->rawDataSource;
			if( abf ) {
				addrbook_free_book( abf );
			}
		}
		else if( iface->type == ADDR_IF_VCARD ) {
			VCardFile *vcf = ds->rawDataSource;
			if( vcf ) {
				vcard_free( vcf );
			}
		}
#ifdef USE_JPILOT
		else if( iface->type == ADDR_IF_JPILOT ) {
			JPilotFile *jpf = ds->rawDataSource;
			if( jpf ) {
				jpilot_free( jpf );
			}
		}
#endif
#ifdef USE_LDAP
		else if( iface->type == ADDR_IF_LDAP ) {
			SyldapServer *server = ds->rawDataSource;
			if( server ) {
				syldap_free( server );
			}
		}
#endif
	}
	else {
		GList *list = ds->rawDataSource;
		addrindex_free_attributes( list );
	}

	g_free( ADDRITEM_ID(addrIndex) );
	g_free( ADDRITEM_NAME(addrIndex) );

	ADDRITEM_TYPE(addrIndex) = ITEMTYPE_NONE;
	ADDRITEM_ID(addrIndex) = NULL;
	ADDRITEM_NAME(addrIndex) = NULL;
	ADDRITEM_PARENT(addrIndex) = NULL;
	ADDRITEM_SUBTYPE(addrIndex) = 0;
	ds->type = ADDR_IF_NONE;
	ds->rawDataSource = NULL;
	ds->iface = NULL;

	ds->type = ADDR_IF_NONE;
	ds->rawDataSource = NULL;
	ds->iface = NULL;
	g_free( ds );
}

static void addrindex_free_all_datasources( AddressInterface *iface ) {
	GList *node = iface->listSource;
	while( node ) {
		AddressDataSource *ds = node->data;
		if( iface->useInterface ) {
			if( iface->type == ADDR_IF_BOOK ) {
				AddressBookFile *abf = ds->rawDataSource;
				if( abf ) {
					addrbook_free_book( abf );
				}
			}
			else if( iface->type == ADDR_IF_VCARD ) {
				VCardFile *vcf = ds->rawDataSource;
				if( vcf ) {
					vcard_free( vcf );
				}
			}
#ifdef USE_JPILOT
			else if( iface->type == ADDR_IF_JPILOT ) {
				JPilotFile *jpf = ds->rawDataSource;
				if( jpf ) {
					jpilot_free( jpf );
				}
			}
#endif
#ifdef USE_LDAP
			else if( iface->type == ADDR_IF_LDAP ) {
				SyldapServer *server = ds->rawDataSource;
				if( server ) {
					syldap_free( server );
				}
			}
#endif
		}
		else {
			GList *list = ds->rawDataSource;
			addrindex_free_attributes( list );
		}

		ds->type = ADDR_IF_NONE;
		ds->rawDataSource = NULL;
		ds->iface = NULL;
		g_free( ds );
		node->data = NULL;
		node = g_list_next( node );
	}
}

static void addrindex_free_interface( AddressInterface *iface ) {
	addrindex_free_all_datasources( iface );

	g_free( ADDRITEM_ID(iface) );
	g_free( ADDRITEM_NAME(iface) );
	g_free( iface->name );
	g_free( iface->listTag );
	g_free( iface->itemTag );

	ADDRITEM_TYPE(iface) = ITEMTYPE_NONE;
	ADDRITEM_ID(iface) = NULL;
	ADDRITEM_NAME(iface) = NULL;
	ADDRITEM_PARENT(iface) = NULL;
	ADDRITEM_SUBTYPE(iface) = 0;
	iface->type = ADDR_IF_NONE;
	iface->name = NULL;
	iface->listTag = NULL;
	iface->itemTag = NULL;
	iface->legacyFlag = FALSE;
	iface->useInterface = FALSE;
	iface->haveLibrary = FALSE;

	g_list_free( iface->listSource );
	iface->listSource = NULL;
}

/*
* Create new object.
*/
AddressIndex *addrindex_create_index() {
	AddressIndex *addrIndex = g_new0( AddressIndex, 1 );

	ADDRITEM_TYPE(addrIndex) = ITEMTYPE_INDEX;
	ADDRITEM_ID(addrIndex) = NULL;
	ADDRITEM_NAME(addrIndex) = g_strdup( "Address Index" );
	ADDRITEM_PARENT(addrIndex) = NULL;
	ADDRITEM_SUBTYPE(addrIndex) = 0;
	addrIndex->filePath = NULL;
	addrIndex->fileName = NULL;
	addrIndex->retVal = MGU_SUCCESS;
	addrIndex->needsConversion = FALSE;
	addrIndex->wasConverted = FALSE;
	addrIndex->conversionError = FALSE;
	addrIndex->interfaceList = NULL;
	addrIndex->lastType = ADDR_IF_NONE;
	addrIndex->dirtyFlag = FALSE;
	addrindex_build_if_list( addrIndex );
	return addrIndex;
}

/*
* Specify file to be used.
*/
void addrindex_set_file_path( AddressIndex *addrIndex, const gchar *value ) {
	g_return_if_fail( addrIndex != NULL );
	addrIndex->filePath = mgu_replace_string( addrIndex->filePath, value );
}
void addrindex_set_file_name( AddressIndex *addrIndex, const gchar *value ) {
	g_return_if_fail( addrIndex != NULL );
	addrIndex->fileName = mgu_replace_string( addrIndex->fileName, value );
}
void addrindex_set_dirty( AddressIndex *addrIndex, const gboolean value ) {
	g_return_if_fail( addrIndex != NULL );
	addrIndex->dirtyFlag = value;
}

/*
* Return list of interfaces.
*/
GList *addrindex_get_interface_list( AddressIndex *addrIndex ) {
	g_return_val_if_fail( addrIndex != NULL, NULL );
	return addrIndex->interfaceList;
}

/*
* Free up object.
*/
void addrindex_free_index( AddressIndex *addrIndex ) {
	GList *node;

	g_return_if_fail( addrIndex != NULL );

	g_free( ADDRITEM_ID(addrIndex) );
	g_free( ADDRITEM_NAME(addrIndex) );
	g_free( addrIndex->filePath );
	g_free( addrIndex->fileName );
	ADDRITEM_TYPE(addrIndex) = ITEMTYPE_NONE;
	ADDRITEM_ID(addrIndex) = NULL;
	ADDRITEM_NAME(addrIndex) = NULL;
	ADDRITEM_PARENT(addrIndex) = NULL;
	ADDRITEM_SUBTYPE(addrIndex) = 0;
	addrIndex->filePath = NULL;
	addrIndex->fileName = NULL;
	addrIndex->retVal = MGU_SUCCESS;
	addrIndex->needsConversion = FALSE;
	addrIndex->wasConverted = FALSE;
	addrIndex->conversionError = FALSE;
	addrIndex->lastType = ADDR_IF_NONE;
	addrIndex->dirtyFlag = FALSE;
	node = addrIndex->interfaceList;
	while( node ) {
		AddressInterface *iface = node->data;
		addrindex_free_interface( iface );
		node = g_list_next( node );
	}
	g_list_free( addrIndex->interfaceList );
	addrIndex->interfaceList = NULL;
	g_free( addrIndex );
}

/*
* Print address index.
*/
void addrindex_print_index( AddressIndex *addrIndex, FILE *stream ) {
	g_return_if_fail( addrIndex != NULL );
	fprintf( stream, "AddressIndex:\n" );
	fprintf( stream, "\tfile path: '%s'\n", addrIndex->filePath );
	fprintf( stream, "\tfile name: '%s'\n", addrIndex->fileName );
	fprintf( stream, "\t   status: %d : '%s'\n", addrIndex->retVal, mgu_error2string( addrIndex->retVal ) );
	fprintf( stream, "\tconverted: '%s'\n", addrIndex->wasConverted ? "yes" : "no" );
	fprintf( stream, "\tcvt error: '%s'\n", addrIndex->conversionError ? "yes" : "no" );
	fprintf( stream, "\t---\n" );
}

/*
* Retrieve specified interface from index.
*/
AddressInterface *addrindex_get_interface( AddressIndex *addrIndex, AddressIfType ifType ) {
	AddressInterface *retVal = NULL;
	GList *node;

	g_return_val_if_fail( addrIndex != NULL, NULL );

	node = addrIndex->interfaceList;
	while( node ) {
		AddressInterface *iface = node->data;
		node = g_list_next( node );
		if( iface->type == ifType ) {
			retVal = iface;
			break;
		}
	}
	return retVal;
}

AddressDataSource *addrindex_create_datasource() {
	AddressDataSource *ds = NULL;
	ds = g_new0( AddressDataSource, 1 );
	ADDRITEM_TYPE(ds) = ITEMTYPE_DATASOURCE;
	ADDRITEM_ID(ds) = NULL;
	ADDRITEM_NAME(ds) = NULL;
	ADDRITEM_PARENT(ds) = NULL;
	ADDRITEM_SUBTYPE(ds) = 0;
	ds->type = ADDR_IF_NONE;
	ds->rawDataSource = NULL;
	ds->iface = NULL;
	return ds;
}

/*
* Add data source to index.
* Enter: addrIndex  Address index object.
*        ifType     Interface type to add.
*        dataSource Actual data source to add.
* Return: TRUE if data source was added.
* Note: The raw data object (for example, AddressBookFile or VCardFile object) should be
* supplied as the dataSource argument.
*/
AddressDataSource *addrindex_index_add_datasource( AddressIndex *addrIndex, AddressIfType ifType, gpointer dataSource ) {
	AddressInterface *iface;
	AddressDataSource *ds = NULL;

	g_return_val_if_fail( addrIndex != NULL, NULL );
	g_return_val_if_fail( dataSource != NULL, NULL );

	iface = addrindex_get_interface( addrIndex, ifType );
	if( iface ) {
		ds = addrindex_create_datasource();
		ADDRITEM_PARENT(ds) = ADDRITEM_OBJECT(iface);
		ds->type = ifType;
		ds->rawDataSource = dataSource;
		ds->iface = iface;
		iface->listSource = g_list_append( iface->listSource, ds );
		addrIndex->dirtyFlag = TRUE;
	}
	return ds;
}

/*
* Remove data source from index.
* Enter: addrIndex  Address index object.
*        dataSource Data source to remove.
* Return: Data source if removed, or NULL if data source was not found in
* index. Note the this object must still be freed.
*/
AddressDataSource *addrindex_index_remove_datasource( AddressIndex *addrIndex, AddressDataSource *dataSource ) {
	AddressDataSource *retVal = FALSE;
	AddressInterface *iface;

	g_return_val_if_fail( addrIndex != NULL, NULL );
	g_return_val_if_fail( dataSource != NULL, NULL );

	iface = addrindex_get_interface( addrIndex, dataSource->type );
	if( iface ) {
		iface->listSource = g_list_remove( iface->listSource, dataSource );
		addrIndex->dirtyFlag = TRUE;
		dataSource->iface = NULL;
		retVal = dataSource;
	}
	return retVal;
}

static AddressInterface *addrindex_tag_get_interface( AddressIndex *addrIndex, gchar *tag, AddressIfType ifType ) {
	AddressInterface *retVal = NULL;
	GList *node = addrIndex->interfaceList;

	while( node ) {
		AddressInterface *iface = node->data;
		node = g_list_next( node );
		if( tag ) {
			if( strcmp( iface->listTag, tag ) == 0 ) {
				retVal = iface;
				break;
			}
		}
		else {
			if( iface->type == ifType ) {
				retVal = iface;
				break;
			}
		}
	}
	return retVal;
}

static AddressInterface *addrindex_tag_get_datasource( AddressIndex *addrIndex, AddressIfType ifType, gchar *tag ) {
	AddressInterface *retVal = NULL;
	GList *node = addrIndex->interfaceList;

	while( node ) {
		AddressInterface *iface = node->data;
		node = g_list_next( node );
		if( iface->type == ifType && iface->itemTag ) {
			if( strcmp( iface->itemTag, tag ) == 0 ) {
				retVal = iface;
				break;
			}
		}
	}
	return retVal;
}

/* **********************************************************************
* Interface XML parsing functions.
* ***********************************************************************
*/
#if 0
static void show_attribs( GList *attr ) {
	while( attr ) {
		gchar *name = ((XMLAttr *)attr->data)->name;
		gchar *value = ((XMLAttr *)attr->data)->value;
		printf( "\tattr value : %s :%s:\n", name, value );
		attr = g_list_next( attr );
	}
	printf( "\t---\n" );
}
#endif

static void addrindex_write_elem_s( FILE *fp, gint lvl, gchar *name ) {
	gint i;
	for( i = 0; i < lvl; i++ ) fputs( "  ", fp );
	fputs( "<", fp );
	fputs( name, fp );
}

static void addrindex_write_elem_e( FILE *fp, gint lvl, gchar *name ) {
	gint i;
	for( i = 0; i < lvl; i++ ) fputs( "  ", fp );
	fputs( "</", fp );
	fputs( name, fp );
	fputs( ">\n", fp );
}

static void addrindex_write_attr( FILE *fp, gchar *name, gchar *value ) {
	fputs( " ", fp );
	fputs( name, fp );
	fputs( "=\"", fp );
	xml_file_put_escape_str( fp, value );
	fputs( "\"", fp );
}

/*
* Return list of name-value pairs.
*/
static GList *addrindex_read_attributes( XMLFile *file ) {
	GList *list = NULL;
	AddressIfAttrib *nv;
	GList *attr;
	gchar *name;
	gchar *value;

	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		nv = g_new0( AddressIfAttrib, 1 );
		nv->name = g_strdup( name );
		nv->value = g_strdup( value );
		list = g_list_append( list, nv );
		attr = g_list_next( attr );
	}
	return list;
}

/*
* Output name-value pairs.
*/
static void addrindex_write_attributes( FILE *fp, gchar *tag, GList *list, gint lvl ) {
	GList *node;
	AddressIfAttrib *nv;
	if( list ) {
		addrindex_write_elem_s( fp, lvl, tag );
		node = list;
		while( node ) {
			nv = node->data;
			addrindex_write_attr( fp, nv->name, nv->value );
			node = g_list_next( node );
		}
		fputs(" />\n", fp);
	}
}

#if 0
static void addrindex_print_attributes( GList *list, FILE *stream ) {
	GList *node = list;
	while( node ) {
		AddressIfAttrib *nv = node->data;
		fprintf( stream, "%s : %s\n", nv->name, nv->value );
		node = g_list_next( node );
	}
}
#endif

static AddressDataSource *addrindex_parse_book( XMLFile *file ) {
	AddressDataSource *ds = g_new0( AddressDataSource, 1 );
	AddressBookFile *abf;
	GList *attr;

	abf = addrbook_create_book();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		gchar *name = ((XMLAttr *)attr->data)->name;
		gchar *value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, ATTAG_BOOK_NAME ) == 0 ) {
			addrbook_set_name( abf, value );
		}
		else if( strcmp( name, ATTAG_BOOK_FILE ) == 0) {
			addrbook_set_file( abf, value );
		}
		attr = g_list_next( attr );
	}
	ds->rawDataSource = abf;
	return ds;
}

static void addrindex_write_book( FILE *fp, AddressDataSource *ds, gint lvl ) {
	AddressBookFile *abf = ds->rawDataSource;
	if( abf ) {
		addrindex_write_elem_s( fp, lvl, TAG_DS_ADDRESS_BOOK );
		addrindex_write_attr( fp, ATTAG_BOOK_NAME, abf->name );
		addrindex_write_attr( fp, ATTAG_BOOK_FILE, abf->fileName );
		fputs( " />\n", fp );
	}
}

static AddressDataSource *addrindex_parse_vcard( XMLFile *file ) {
	AddressDataSource *ds = g_new0( AddressDataSource, 1 );
	VCardFile *vcf;
	GList *attr;

	vcf = vcard_create();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		gchar *name = ((XMLAttr *)attr->data)->name;
		gchar *value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, ATTAG_VCARD_NAME ) == 0 ) {
			vcard_set_name( vcf, value );
		}
		else if( strcmp( name, ATTAG_VCARD_FILE ) == 0) {
			vcard_set_file( vcf, value );
		}
		attr = g_list_next( attr );
	}
	ds->rawDataSource = vcf;
	return ds;
}

static void addrindex_write_vcard( FILE *fp, AddressDataSource *ds, gint lvl ) {
     	VCardFile *vcf = ds->rawDataSource;
	if( vcf ) {
		addrindex_write_elem_s( fp, lvl, TAG_DS_VCARD );
		addrindex_write_attr( fp, ATTAG_VCARD_NAME, vcf->name );
		addrindex_write_attr( fp, ATTAG_VCARD_FILE, vcf->path );
		fputs( " />\n", fp );
	}
}

#ifdef USE_JPILOT
static AddressDataSource *addrindex_parse_jpilot( XMLFile *file ) {
	AddressDataSource *ds = g_new0( AddressDataSource, 1 );
	JPilotFile *jpf;
	GList *attr;

	jpf = jpilot_create();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		gchar *name = ((XMLAttr *)attr->data)->name;
		gchar *value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, ATTAG_JPILOT_NAME ) == 0 ) {
			jpilot_set_name( jpf, value );
		}
		else if( strcmp( name, ATTAG_JPILOT_FILE ) == 0 ) {
			jpilot_set_file( jpf, value );
		}
		else if( strcmp( name, ATTAG_JPILOT_CUSTOM_1 ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		else if( strcmp( name, ATTAG_JPILOT_CUSTOM_2 ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		else if( strcmp( name, ATTAG_JPILOT_CUSTOM_3 ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		else if( strcmp( name, ATTAG_JPILOT_CUSTOM_4 ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		attr = g_list_next( attr );
	}
	ds->rawDataSource = jpf;
	return ds;
}

static void addrindex_write_jpilot( FILE *fp,AddressDataSource *ds, gint lvl ) {
	JPilotFile *jpf = ds->rawDataSource;
	if( jpf ) {
		gint ind;
		GList *node;
		GList *customLbl = jpilot_get_custom_labels( jpf );
		addrindex_write_elem_s( fp, lvl, TAG_DS_JPILOT );
		addrindex_write_attr( fp, ATTAG_JPILOT_NAME, jpf->name );
		addrindex_write_attr( fp, ATTAG_JPILOT_FILE, jpf->path );
		node = customLbl;
		ind = 1;
		while( node ) {
			gchar name[256];
			sprintf( name, "%s%d", ATTAG_JPILOT_CUSTOM, ind );
			addrindex_write_attr( fp, name, node->data );
			ind++;
			node = g_list_next( node );
		}
		fputs( " />\n", fp );
	}
}
#else
/* Just read/write name-value pairs */
static AddressDataSource *addrindex_parse_jpilot( XMLFile *file ) {
	AddressDataSource *ds = g_new0( AddressDataSource, 1 );
	GList *list = addrindex_read_attributes( file );
	ds->rawDataSource = list;
	return ds;
}

static void addrindex_write_jpilot( FILE *fp, AddressDataSource *ds, gint lvl ) {
	GList *list = ds->rawDataSource;
	if( list ) {
		addrindex_write_attributes( fp, TAG_DS_JPILOT, list, lvl );
	}
}
#endif

#ifdef USE_LDAP
static AddressDataSource *addrindex_parse_ldap( XMLFile *file ) {
	AddressDataSource *ds = g_new0( AddressDataSource, 1 );
	SyldapServer *server;
	GList *attr;

	server = syldap_create();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		gchar *name = ((XMLAttr *)attr->data)->name;
		gchar *value = ((XMLAttr *)attr->data)->value;
		gint ivalue = atoi( value );
		if( strcmp( name, ATTAG_LDAP_NAME ) == 0 ) {
			syldap_set_name( server, value );
		}
		else if( strcmp( name, ATTAG_LDAP_HOST ) == 0 ) {
			syldap_set_host( server, value );
		}
		else if( strcmp( name, ATTAG_LDAP_PORT ) == 0 ) {
			syldap_set_port( server, ivalue );
		}
		else if( strcmp( name, ATTAG_LDAP_BASE_DN ) == 0 ) {
			syldap_set_base_dn( server, value );
		}
		else if( strcmp( name, ATTAG_LDAP_BIND_DN ) == 0 ) {
			syldap_set_bind_dn( server, value );
		}
		else if( strcmp( name, ATTAG_LDAP_BIND_PASS ) == 0 ) {
			syldap_set_bind_password( server, value );
		}
		else if( strcmp( name, ATTAG_LDAP_CRITERIA ) == 0 ) {
			syldap_set_search_criteria( server, value );
		}
		else if( strcmp( name, ATTAG_LDAP_MAX_ENTRY ) == 0 ) {
			syldap_set_max_entries( server, ivalue );
		}
		else if( strcmp( name, ATTAG_LDAP_TIMEOUT ) == 0 ) {
			syldap_set_timeout( server, ivalue );
		}
		attr = g_list_next( attr );
	}

	ds->rawDataSource = server;
	return ds;
}

static void addrindex_write_ldap( FILE *fp, AddressDataSource *ds, gint lvl ) {
	SyldapServer *server = ds->rawDataSource;
	if( server ) {
		gchar value[256];

		addrindex_write_elem_s( fp, lvl, TAG_DS_LDAP );
		addrindex_write_attr( fp, ATTAG_LDAP_NAME, server->name );
		addrindex_write_attr( fp, ATTAG_LDAP_HOST, server->hostName );

		sprintf( value, "%d", server->port );	
		addrindex_write_attr( fp, ATTAG_LDAP_PORT, value );

		addrindex_write_attr( fp, ATTAG_LDAP_BASE_DN, server->baseDN );
		addrindex_write_attr( fp, ATTAG_LDAP_BIND_DN, server->bindDN );
		addrindex_write_attr( fp, ATTAG_LDAP_BIND_PASS, server->bindPass );
		addrindex_write_attr( fp, ATTAG_LDAP_CRITERIA, server->searchCriteria );

		sprintf( value, "%d", server->maxEntries );
		addrindex_write_attr( fp, ATTAG_LDAP_MAX_ENTRY, value );
		sprintf( value, "%d", server->timeOut );
		addrindex_write_attr( fp, ATTAG_LDAP_TIMEOUT, value );

		fputs(" />\n", fp);
	}
}
#else
/* Just read/write name-value pairs */
static AddressDataSource *addrindex_parse_ldap( XMLFile *file ) {
	AddressDataSource *ds = g_new0( AddressDataSource, 1 );
	GList *list = addrindex_read_attributes( file );
	ds->rawDataSource = list;
	return ds;
}

static void addrindex_write_ldap( FILE *fp, AddressDataSource *ds, gint lvl ) {
	GList *list = ds->rawDataSource;
	if( list ) {
		addrindex_write_attributes( fp, TAG_DS_LDAP, list, lvl );
	}
}
#endif

/* **********************************************************************
* Address index I/O functions.
* ***********************************************************************
*/
static void addrindex_read_index( AddressIndex *addrIndex, XMLFile *file ) {
	guint prev_level;
	/* gchar *element; */
	/* GList *attr; */
	XMLTag *xtag;
	AddressInterface *iface = NULL, *dsIFace = NULL;
	AddressDataSource *ds;

	for (;;) {
		prev_level = file->level;
		xml_parse_next_tag( file );
		if( file->level < prev_level ) return;

		xtag = xml_get_current_tag( file );
		/* printf( "tag : %s\n", xtag->tag ); */

		iface = addrindex_tag_get_interface( addrIndex, xtag->tag, ADDR_IF_NONE );
		if( iface ) {
			addrIndex->lastType = iface->type;
			if( iface->legacyFlag ) addrIndex->needsConversion = TRUE;
			/* printf( "found : %s\n", iface->name ); */
		}
		else {
			dsIFace = addrindex_tag_get_datasource( addrIndex, addrIndex->lastType, xtag->tag );
			if( dsIFace ) {
				/* Add data source to list */
				/* printf( "\tdata source: %s\n", dsIFace->name ); */
				ds = NULL;
				if( addrIndex->lastType == ADDR_IF_BOOK ) {
					ds = addrindex_parse_book( file );
					if( ds->rawDataSource ) {
						addrbook_set_path( ds->rawDataSource, addrIndex->filePath );
						/* addrbook_print_book( ds->rawDataSource, stdout ); */
					}
				}
				else if( addrIndex->lastType == ADDR_IF_VCARD ) {
					ds = addrindex_parse_vcard( file );
					/* if( ds->rawDataSource ) { */
					/*	vcard_print_file( ds->rawDataSource, stdout ); */
					/* } */
				}
				else if( addrIndex->lastType == ADDR_IF_JPILOT ) {
					ds = addrindex_parse_jpilot( file );
					/*
					if( ds->rawDataSource ) {
						jpilot_print_file( ds->rawDataSource, stdout );
						// addrindex_print_attributes( ds->rawDataSource, stdout );
					}
					*/
				}
				else if( addrIndex->lastType == ADDR_IF_LDAP ) {
					ds = addrindex_parse_ldap( file );
					/*
					if( ds->rawDataSource ) {
						syldap_print_data( ds->rawDataSource, stdout );
						// addrindex_print_attributes( ds->rawDataSource, stdout );
					}
					*/
				}
				if( ds ) {
					ds->type = addrIndex->lastType;
					ds->iface = dsIFace;
					dsIFace->listSource = g_list_append( dsIFace->listSource, ds );
				}
				/* printf( "=============================\n\n" ); */
			}
		}
		/*
		element = xml_get_element( file );
		attr = xml_get_current_tag_attr( file );
		if( _interfaceLast_ && ! _interfaceLast_->legacyFlag ) {
			show_attribs( attr );
			printf( "\ttag  value : %s :\n", element );
		}
		*/
		addrindex_read_index( addrIndex, file );
	}
}

static gint addrindex_read_file( AddressIndex *addrIndex ) {
	XMLFile *file = NULL;
	gchar *fileSpec = NULL;

	g_return_val_if_fail( addrIndex != NULL, -1 );

	fileSpec = g_strconcat( addrIndex->filePath, G_DIR_SEPARATOR_S, addrIndex->fileName, NULL );
	addrIndex->retVal = MGU_NO_FILE;
	file = xml_open_file( fileSpec );
	g_free( fileSpec );

	if( file == NULL ) {
		/* fprintf( stdout, " file '%s' does not exist.\n", addrIndex->fileName ); */
		return addrIndex->retVal;
	}

	addrIndex->retVal = MGU_BAD_FORMAT;
	if( xml_get_dtd( file ) == 0 ) {
		if( xml_parse_next_tag( file ) == 0 ) {
			if( xml_compare_tag( file, TAG_ADDRESS_INDEX ) ) {
				addrindex_read_index( addrIndex, file );
				addrIndex->retVal = MGU_SUCCESS;
			}
		}
	}
	xml_close_file( file );

	return addrIndex->retVal;
}

static void addrindex_write_index( AddressIndex *addrIndex, FILE *fp ) {
	GList *nodeIF, *nodeDS;
	gint lvlList = 1;
	gint lvlItem = 1 + lvlList;

	nodeIF = addrIndex->interfaceList;
	while( nodeIF ) {
		AddressInterface *iface = nodeIF->data;
		if( ! iface->legacyFlag ) {
			nodeDS = iface->listSource;
			addrindex_write_elem_s( fp, lvlList, iface->listTag );
			fputs( ">\n", fp );
			while( nodeDS ) {
				AddressDataSource *ds = nodeDS->data;
				if( ds ) {
					if( iface->type == ADDR_IF_BOOK ) {
						addrindex_write_book( fp, ds, lvlItem );
					}
					if( iface->type == ADDR_IF_VCARD ) {
						addrindex_write_vcard( fp, ds, lvlItem );
					}
					if( iface->type == ADDR_IF_JPILOT ) {
						addrindex_write_jpilot( fp, ds, lvlItem );
					}
					if( iface->type == ADDR_IF_LDAP ) {
						addrindex_write_ldap( fp, ds, lvlItem );
					}
				}
				nodeDS = g_list_next( nodeDS );
			}
			addrindex_write_elem_e( fp, lvlList, iface->listTag );
		}
		nodeIF = g_list_next( nodeIF );
	}
}

/*
* Write data to specified file.
* Enter: addrIndex Address index object.
*        newFile   New file name.
* return: Status code, from addrIndex->retVal.
* Note: File will be created in directory specified by addrIndex.
*/
gint addrindex_write_to( AddressIndex *addrIndex, const gchar *newFile ) {
	FILE *fp;
	gchar *fileSpec;
#ifndef DEV_STANDALONE
	PrefFile *pfile;
#endif

	g_return_val_if_fail( addrIndex != NULL, -1 );

	fileSpec = g_strconcat( addrIndex->filePath, G_DIR_SEPARATOR_S, newFile, NULL );
	addrIndex->retVal = MGU_OPEN_FILE;
#ifdef DEV_STANDALONE
	fp = g_fopen( fileSpec, "wb" );
	g_free( fileSpec );
	if( fp ) {
		fputs( "<?xml version=\"1.0\" ?>\n", fp );
#else
	pfile = prefs_file_open( fileSpec );
	g_free( fileSpec );
	if( pfile ) {
		fp = pfile->fp;
		fprintf( fp, "<?xml version=\"1.0\" encoding=\"%s\" ?>\n", CS_INTERNAL );
#endif
		addrindex_write_elem_s( fp, 0, TAG_ADDRESS_INDEX );
		fputs( ">\n", fp );

		addrindex_write_index( addrIndex, fp );
		addrindex_write_elem_e( fp, 0, TAG_ADDRESS_INDEX );

		addrIndex->retVal = MGU_SUCCESS;
#ifdef DEV_STANDALONE
		fclose( fp );
#else
		if( prefs_file_close( pfile ) < 0 ) {
			addrIndex->retVal = MGU_ERROR_WRITE;
		}
#endif
	}

	fileSpec = NULL;
	return addrIndex->retVal;
}

/*
* Save address index data to original file.
* return: Status code, from addrIndex->retVal.
*/
gint addrindex_save_data( AddressIndex *addrIndex ) {
	g_return_val_if_fail( addrIndex != NULL, -1 );

	addrIndex->retVal = MGU_NO_FILE;
	if( addrIndex->fileName == NULL || *addrIndex->fileName == '\0' ) return addrIndex->retVal;
	if( addrIndex->filePath == NULL || *addrIndex->filePath == '\0' ) return addrIndex->retVal;

	addrindex_write_to( addrIndex, addrIndex->fileName );
	if( addrIndex->retVal == MGU_SUCCESS ) {
		addrIndex->dirtyFlag = FALSE;
	}
	return addrIndex->retVal;
}

/*
* Save all address book files which may have changed.
* Return: Status code, set if there was a problem saving data.
*/
gint addrindex_save_all_books( AddressIndex *addrIndex ) {
	gint retVal = MGU_SUCCESS;
	GList *nodeIf, *nodeDS;

	nodeIf = addrIndex->interfaceList;
	while( nodeIf ) {
		AddressInterface *iface = nodeIf->data;
		if( iface->type == ADDR_IF_BOOK ) {
			nodeDS = iface->listSource;
			while( nodeDS ) {
				AddressDataSource *ds = nodeDS->data;
				AddressBookFile *abf = ds->rawDataSource;
				if( abf->dirtyFlag ) {
					if( abf->readFlag ) {
						addrbook_save_data( abf );
						if( abf->retVal != MGU_SUCCESS ) {
							retVal = abf->retVal;
						}
					}
				}
				nodeDS = g_list_next( nodeDS );
			}
			break;
		}
		nodeIf = g_list_next( nodeIf );
	}
	return retVal;
}


/* **********************************************************************
* Address book conversion to new format.
* ***********************************************************************
*/

#define ELTAG_IF_OLD_FOLDER   "folder"
#define ELTAG_IF_OLD_GROUP    "group"
#define ELTAG_IF_OLD_ITEM     "item"
#define ELTAG_IF_OLD_NAME     "name"
#define ELTAG_IF_OLD_ADDRESS  "address"
#define ELTAG_IF_OLD_REMARKS  "remarks"
#define ATTAG_IF_OLD_NAME     "name"

#define TEMPNODE_ROOT         0
#define TEMPNODE_FOLDER       1
#define TEMPNODE_GROUP        2
#define TEMPNODE_ADDRESS      3

typedef struct _AddressCvt_Node AddressCvtNode;
struct _AddressCvt_Node {
	gint  type;
	gchar *name;
	gchar *address;
	gchar *remarks;
	GList *list;
};

/*
* Parse current address item.
*/
static AddressCvtNode *addrindex_parse_item( XMLFile *file ) {
	gchar *element;
	guint level;
	AddressCvtNode *nn;

	nn = g_new0( AddressCvtNode, 1 );
	nn->type = TEMPNODE_ADDRESS;
	nn->list = NULL;

	level = file->level;

	for (;;) {
		xml_parse_next_tag(file);
		if (file->level < level) return nn;

		element = xml_get_element( file );
		if( xml_compare_tag( file, ELTAG_IF_OLD_NAME ) ) {
			nn->name = g_strdup( element );
		}
		if( xml_compare_tag( file, ELTAG_IF_OLD_ADDRESS ) ) {
			nn->address = g_strdup( element );
		}
		if( xml_compare_tag( file, ELTAG_IF_OLD_REMARKS ) ) {
			nn->remarks = g_strdup( element );
		}
		xml_parse_next_tag(file);
	}
}

/*
* Create a temporary node below specified node.
*/
static AddressCvtNode *addrindex_add_object( AddressCvtNode *node, gint type, gchar *name, gchar *addr, char *rem ) {
	AddressCvtNode *nn;
	nn = g_new0( AddressCvtNode, 1 );
	nn->type = type;
	nn->name = g_strdup( name );
	nn->remarks = g_strdup( rem );
	node->list = g_list_append( node->list, nn );
	return nn;
}

/*
* Process current temporary node.
*/
static void addrindex_add_obj( XMLFile *file, AddressCvtNode *node ) {
	GList *attr;
	guint prev_level;
	AddressCvtNode *newNode = NULL;
	gchar *name;
	gchar *value;

	for (;;) {
		prev_level = file->level;
		xml_parse_next_tag( file );
		if (file->level < prev_level) return;
		name = NULL;
		value = NULL;

		if( xml_compare_tag( file, ELTAG_IF_OLD_GROUP ) ) {
			attr = xml_get_current_tag_attr(file);
			if (attr) {
				name = ((XMLAttr *)attr->data)->name;
				if( strcmp( name, ATTAG_IF_OLD_NAME ) == 0 ) {
					value = ((XMLAttr *)attr->data)->value;
				}
			}
			newNode = addrindex_add_object( node, TEMPNODE_GROUP, value, "", "" );
			addrindex_add_obj( file, newNode );

		}
		else if( xml_compare_tag( file, ELTAG_IF_OLD_FOLDER ) ) {
			attr = xml_get_current_tag_attr(file);
			if (attr) {
				name = ((XMLAttr *)attr->data)->name;
				if( strcmp( name, ATTAG_IF_OLD_NAME ) == 0 ) {
					value = ((XMLAttr *)attr->data)->value;
				}
			}
			newNode = addrindex_add_object( node, TEMPNODE_FOLDER, value, "", "" );
			addrindex_add_obj( file, newNode );
		}
		else if( xml_compare_tag( file, ELTAG_IF_OLD_ITEM ) ) {
			newNode = addrindex_parse_item( file );
			node->list = g_list_append( node->list, newNode );
		}
		else {
			/* printf( "invalid: !!! \n" ); */
			attr = xml_get_current_tag_attr( file );
		}
	}
}

/*
* Consume all nodes below current tag.
*/
static void addrindex_consume_tree( XMLFile *file ) {
	guint prev_level;
	gchar *element;
	GList *attr;
	XMLTag *xtag;

	for (;;) {
		prev_level = file->level;
		xml_parse_next_tag( file );
		if (file->level < prev_level) return;

		xtag = xml_get_current_tag( file );
		/* printf( "tag : %s\n", xtag->tag ); */
		element = xml_get_element( file );
		attr = xml_get_current_tag_attr( file );
		/* show_attribs( attr ); */
		/* printf( "\ttag  value : %s :\n", element ); */
		addrindex_consume_tree( file );
	}
}

/*
* Print temporary tree.
*/
static void addrindex_print_node( AddressCvtNode *node, FILE *stream  ) {
	GList *list;

	fprintf( stream, "Node:\ttype :%d:\n", node->type );
	fprintf( stream, "\tname :%s:\n", node->name );
	fprintf( stream, "\taddr :%s:\n", node->address );
	fprintf( stream, "\trems :%s:\n", node->remarks );
	if( node->list ) {
		fprintf( stream, "\t--list----\n" );
	}
	list = node->list;
	while( list ) {
		AddressCvtNode *lNode = list->data;
		list = g_list_next( list );
		addrindex_print_node( lNode, stream );
	}
	fprintf( stream, "\t==list-%d==\n", node->type );
}

/*
* Free up temporary tree.
*/
static void addrindex_free_node( AddressCvtNode *node ) {
	GList *list = node->list;

	while( list ) {
		AddressCvtNode *lNode = list->data;
		list = g_list_next( list );
		addrindex_free_node( lNode );
	}
	node->type = TEMPNODE_ROOT;
	g_free( node->name );
	g_free( node->address );
	g_free( node->remarks );
	g_list_free( node->list );
	g_free( node );
}

/*
* Process address book for specified node.
*/
static void addrindex_process_node(
		AddressBookFile *abf, AddressCvtNode *node, ItemFolder *parent,
		ItemGroup *parentGrp, ItemFolder *folderGrp )
{
	GList *list;
	ItemFolder *itemFolder = NULL;
	ItemGroup *itemGParent = parentGrp;
	ItemFolder *itemGFolder = folderGrp;
	AddressCache *cache = abf->addressCache;

	if( node->type == TEMPNODE_ROOT ) {
		itemFolder = parent;
	}
	else if( node->type == TEMPNODE_FOLDER ) {
		itemFolder = addritem_create_item_folder();
		addritem_folder_set_name( itemFolder, node->name );
		addrcache_id_folder( cache, itemFolder );
		addrcache_folder_add_folder( cache, parent, itemFolder );
		itemGFolder = NULL;
	}
	else if( node->type == TEMPNODE_GROUP ) {
		ItemGroup *itemGroup;
		gchar *fName;

		/* Create a folder for group */
		fName = g_strdup_printf( "Cvt - %s", node->name );
		itemGFolder = addritem_create_item_folder();
		addritem_folder_set_name( itemGFolder, fName );
		addrcache_id_folder( cache, itemGFolder );
		addrcache_folder_add_folder( cache, parent, itemGFolder );
		g_free( fName );

		/* Add group into folder */
		itemGroup = addritem_create_item_group();
		addritem_group_set_name( itemGroup, node->name );
		addrcache_id_group( cache, itemGroup );
		addrcache_folder_add_group( cache, itemGFolder, itemGroup );
		itemGParent = itemGroup;
	}
	else if( node->type == TEMPNODE_ADDRESS ) {
		ItemPerson *itemPerson;
		ItemEMail *itemEMail;

		/* Create person and email objects */
		itemPerson = addritem_create_item_person();
		addritem_person_set_common_name( itemPerson, node->name );
		addrcache_id_person( cache, itemPerson );
		itemEMail = addritem_create_item_email();
		addritem_email_set_address( itemEMail, node->address );
		addritem_email_set_remarks( itemEMail, node->remarks );
		addrcache_id_email( cache, itemEMail );
		addrcache_person_add_email( cache, itemPerson, itemEMail );

		/* Add person into appropriate folder */
		if( itemGFolder ) {
			addrcache_folder_add_person( cache, itemGFolder, itemPerson );
		}
		else {
			addrcache_folder_add_person( cache, parent, itemPerson );
		}

		/* Add email address only into group */
		if( parentGrp ) {
			addrcache_group_add_email( cache, parentGrp, itemEMail );
		}
	}

	list = node->list;
	while( list ) {
		AddressCvtNode *lNode = list->data;
		list = g_list_next( list );
		addrindex_process_node( abf, lNode, itemFolder, itemGParent, itemGFolder );
	}
}

/*
* Process address book to specified file number.
*/
static gboolean addrindex_process_book( AddressIndex *addrIndex, XMLFile *file, gchar *displayName ) {
	gboolean retVal = FALSE;
	AddressBookFile *abf = NULL;
	AddressCvtNode *rootNode = NULL;
	gchar *newFile = NULL;
	GList *fileList = NULL;
	gint fileNum  = 0;

	/* Setup root node */
	rootNode = g_new0( AddressCvtNode, 1 );
	rootNode->type = TEMPNODE_ROOT;
	rootNode->name = g_strdup( "root" );
	rootNode->list = NULL;
	addrindex_add_obj( file, rootNode );
	/* addrindex_print_node( rootNode, stdout ); */

	/* Create new address book */
	abf = addrbook_create_book();
	addrbook_set_name( abf, displayName );
	addrbook_set_path( abf, addrIndex->filePath );

	/* Determine next available file number */
	fileList = addrbook_get_bookfile_list( abf );
	if( fileList ) {
		fileNum = 1 + abf->maxValue;
	}
	g_list_free( fileList );
	fileList = NULL;

	newFile = addrbook_gen_new_file_name( fileNum );
	if( newFile ) {
		addrbook_set_file( abf, newFile );
	}

	addrindex_process_node( abf, rootNode, abf->addressCache->rootFolder, NULL, NULL );

	/* addrbook_dump_book( abf, stdout ); */
	addrbook_save_data( abf );
	addrIndex->retVal = abf->retVal;
	if( abf->retVal == MGU_SUCCESS ) retVal = TRUE;

	addrbook_free_book( abf );
	abf = NULL;
	addrindex_free_node( rootNode );
	rootNode = NULL;

	/* Create entries in address index */
	if( retVal ) {
		abf = addrbook_create_book();
		addrbook_set_name( abf, displayName );
		addrbook_set_path( abf, addrIndex->filePath );
		addrbook_set_file( abf, newFile );
		addrindex_index_add_datasource( addrIndex, ADDR_IF_BOOK, abf );
	}

	return retVal;
}

/*
* Process tree converting data.
*/
static void addrindex_convert_tree( AddressIndex *addrIndex, XMLFile *file ) {
	guint prev_level;
	gchar *element;
	GList *attr;
	XMLTag *xtag;

	/* Process file */
	for (;;) {
		prev_level = file->level;
		xml_parse_next_tag( file );
		if (file->level < prev_level) return;

		xtag = xml_get_current_tag( file );
		/* printf( "tag : %d : %s\n", prev_level, xtag->tag ); */
		if( strcmp( xtag->tag, TAG_IF_OLD_COMMON ) == 0 ) {
			if( addrindex_process_book( addrIndex, file, DISP_OLD_COMMON ) ) {
				addrIndex->needsConversion = FALSE;
				addrIndex->wasConverted = TRUE;
				continue;
			}
			return;
		}
		if( strcmp( xtag->tag, TAG_IF_OLD_PERSONAL ) == 0 ) {
			if( addrindex_process_book( addrIndex, file, DISP_OLD_PERSONAL ) ) {
				addrIndex->needsConversion = FALSE;
				addrIndex->wasConverted = TRUE;
				continue;
			}
			return;
		}
		element = xml_get_element( file );
		attr = xml_get_current_tag_attr( file );
		/* show_attribs( attr ); */
		/* printf( "\ttag  value : %s :\n", element ); */
		addrindex_consume_tree( file );
	}
}

static gint addrindex_convert_data( AddressIndex *addrIndex ) {
	XMLFile *file = NULL;
	gchar *fileSpec;

	fileSpec = g_strconcat( addrIndex->filePath, G_DIR_SEPARATOR_S, addrIndex->fileName, NULL );
	addrIndex->retVal = MGU_NO_FILE;
	file = xml_open_file( fileSpec );
	g_free( fileSpec );

	if( file == NULL ) {
		/* fprintf( stdout, " file '%s' does not exist.\n", addrIndex->fileName ); */
		return addrIndex->retVal;
	}

	addrIndex->retVal = MGU_BAD_FORMAT;
	if( xml_get_dtd( file ) == 0 ) {
		if( xml_parse_next_tag( file ) == 0 ) {
			if( xml_compare_tag( file, TAG_ADDRESS_INDEX ) ) {
				addrindex_convert_tree( addrIndex, file );
			}
		}
	}
	xml_close_file( file );
	return addrIndex->retVal;
}

/*
* Create a new address book file.
*/
static gboolean addrindex_create_new_book( AddressIndex *addrIndex, gchar *displayName ) {
	gboolean retVal = FALSE;
	AddressBookFile *abf = NULL;
	gchar *newFile = NULL;
	GList *fileList = NULL;
	gint fileNum = 0;

	/* Create new address book */
	abf = addrbook_create_book();
	addrbook_set_name( abf, displayName );
	addrbook_set_path( abf, addrIndex->filePath );

	/* Determine next available file number */
	fileList = addrbook_get_bookfile_list( abf );
	if( fileList ) {
		fileNum = 1 + abf->maxValue;
	}
	g_list_free( fileList );
	fileList = NULL;

	newFile = addrbook_gen_new_file_name( fileNum );
	if( newFile ) {
		addrbook_set_file( abf, newFile );
	}

	addrbook_save_data( abf );
	addrIndex->retVal = abf->retVal;
	if( abf->retVal == MGU_SUCCESS ) retVal = TRUE;
	addrbook_free_book( abf );
	abf = NULL;

	/* Create entries in address index */
	if( retVal ) {
		abf = addrbook_create_book();
		addrbook_set_name( abf, displayName );
		addrbook_set_path( abf, addrIndex->filePath );
		addrbook_set_file( abf, newFile );
		addrindex_index_add_datasource( addrIndex, ADDR_IF_BOOK, abf );
	}

	return retVal;
}

/*
* Read data for address index performing a conversion if necesary.
* Enter: addrIndex Address index object.
* return: Status code, from addrIndex->retVal.
* Note: New address book files will be created in directory specified by
* addrIndex. Three files will be created, for the following:
*	"Common addresses"
*	"Personal addresses"
*	"Auto-registered" - a new address book.
*/
gint addrindex_read_data( AddressIndex *addrIndex ) {
	g_return_val_if_fail( addrIndex != NULL, -1 );

	addrIndex->conversionError = FALSE;
	addrindex_read_file( addrIndex );
	if( addrIndex->retVal == MGU_SUCCESS ) {
		if( addrIndex->needsConversion ) {
			if( addrindex_convert_data( addrIndex ) == MGU_SUCCESS ) {
				addrIndex->conversionError = TRUE;
			}
			else {
				addrIndex->conversionError = TRUE;
			}
		}
		addrIndex->dirtyFlag = TRUE;
	}
	return addrIndex->retVal;
}

/*
* Create new address books for a new address index.
* Enter: addrIndex Address index object.
* return: Status code, from addrIndex->retVal.
* Note: New address book files will be created in directory specified by
* addrIndex. Three files will be created, for the following:
*	"Common addresses"
*	"Personal addresses"
*	"Auto-registered" - a new address book.
*/
gint addrindex_create_new_books( AddressIndex *addrIndex ) {
	gboolean flg;

	g_return_val_if_fail( addrIndex != NULL, -1 );

	flg = addrindex_create_new_book( addrIndex, DISP_NEW_COMMON );
	if( flg ) {
		flg = addrindex_create_new_book( addrIndex, DISP_NEW_PERSONAL );
		flg = addrindex_create_new_book( addrIndex, ADDR_DS_AUTOREG );
		addrIndex->dirtyFlag = TRUE;
	}
	return addrIndex->retVal;
}

gint addrindex_create_extra_books( AddressIndex *addrIndex ) {
	GList *node_ds;
	AddressInterface *iface = NULL;
	AddressDataSource *ds = NULL;
	const gchar *ds_name;

	g_return_val_if_fail(addrIndex != NULL, -1);

	iface = addrindex_get_interface(addrIndex, ADDR_IF_BOOK);
	if (!iface)
		return -1;

	for (node_ds = iface->listSource; node_ds != NULL;
	     node_ds = node_ds->next) {
		ds = node_ds->data;
		ds_name = addrindex_ds_get_name(ds);
		if (!ds_name)
			continue;
		if (!strcmp(ds_name, ADDR_DS_AUTOREG)) {
			debug_print("%s found\n", ADDR_DS_AUTOREG);
			return 0;
		}
	}

	debug_print("%s not found, creating new one\n", ADDR_DS_AUTOREG);
	if (addrindex_create_new_book(addrIndex, ADDR_DS_AUTOREG)) {
		addrIndex->dirtyFlag = TRUE;
	}

	return addrIndex->retVal;
}

/* **********************************************************************
* New interface stuff.
* ***********************************************************************
*/

/*
 * Return modified flag for specified data source.
 */
gboolean addrindex_ds_get_modify_flag( AddressDataSource *ds ) {
	gboolean retVal = FALSE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getModifyFlag ) {
		retVal = ( iface->getModifyFlag ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return accessed flag for specified data source.
 */
gboolean addrindex_ds_get_access_flag( AddressDataSource *ds ) {
	gboolean retVal = FALSE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getAccessFlag ) {
		retVal = ( iface->getAccessFlag ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return data read flag for specified data source.
 */
gboolean addrindex_ds_get_read_flag( AddressDataSource *ds ) {
	gboolean retVal = TRUE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getReadFlag ) {
		retVal = ( iface->getReadFlag ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return status code for specified data source.
 */
gint addrindex_ds_get_status_code( AddressDataSource *ds ) {
	gint retVal = MGU_SUCCESS;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getStatusCode ) {
		retVal = ( iface->getStatusCode ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return data read flag for specified data source.
 */
gint addrindex_ds_read_data( AddressDataSource *ds ) {
	gint retVal = MGU_SUCCESS;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getReadData ) {
		retVal = ( iface->getReadData ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return data read flag for specified data source.
 */
ItemFolder *addrindex_ds_get_root_folder( AddressDataSource *ds ) {
	ItemFolder *retVal = NULL;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getRootFolder ) {
		retVal = ( iface->getRootFolder ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return list of folders for specified data source.
 */
GList *addrindex_ds_get_list_folder( AddressDataSource *ds ) {
	GList *retVal = FALSE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getListFolder ) {
		retVal = ( iface->getListFolder ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return list of persons in root folder for specified data source.
 */
GList *addrindex_ds_get_list_person( AddressDataSource *ds ) {
	GList *retVal = FALSE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getListPerson ) {
		retVal = ( iface->getListPerson ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return name for specified data source.
 */
gchar *addrindex_ds_get_name( AddressDataSource *ds ) {
	gchar *retVal = FALSE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getName ) {
		retVal = ( iface->getName ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Set the access flag inside the data source.
 */
void addrindex_ds_set_access_flag( AddressDataSource *ds, gboolean *value ) {
	AddressInterface *iface;

	if( ds == NULL ) return;
	iface = ds->iface;
	if( iface == NULL ) return;
	if( iface->setAccessFlag ) {
		( iface->setAccessFlag ) ( ds->rawDataSource, value );
	}
}

/*
 * Return read only flag for specified data source.
 */
gboolean addrindex_ds_get_readonly( AddressDataSource *ds ) {
	AddressInterface *iface;
	if( ds == NULL ) return TRUE;
	iface = ds->iface;
	if( iface == NULL ) return TRUE;
	return iface->readOnly;
}

/*
 * Return list of all persons for specified data source.
 */
GList *addrindex_ds_get_all_persons( AddressDataSource *ds ) {
	GList *retVal = NULL;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getAllPersons ) {
		retVal = ( iface->getAllPersons ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return list of all groups for specified data source.
 */
GList *addrindex_ds_get_all_groups( AddressDataSource *ds ) {
	GList *retVal = NULL;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->iface;
	if( iface == NULL ) return retVal;
	if( iface->getAllGroups ) {
		retVal = ( iface->getAllGroups ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
* End of Source.
*/
