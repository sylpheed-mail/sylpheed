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
 * General functions for create common address book entries.
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "mgutils.h"

/*
* Dump linked list of character strings (for debug).
*/
void mgu_print_list( GSList *list, FILE *stream ) {
	GSList *node = list;
	while( node ) {
		fprintf( stream, "\t- >%s<\n", (gchar *)node->data );
		node = g_slist_next( node );
	}
}

/*
* Dump linked list of character strings (for debug).
*/
void mgu_print_dlist( GList *list, FILE *stream ) {
	GList *node = list;
	while( node ) {
		fprintf( stream, "\t- >%s<\n", (gchar *)node->data );
		node = g_list_next( node );
	}
}

/*
* Free linked list of character strings.
*/
void mgu_free_list( GSList *list ) {
	GSList *node = list;
	while( node ) {
		g_free( node->data );
		node->data = NULL;
		node = g_slist_next( node );
	}
	g_slist_free( list );
}

/*
* Free linked list of character strings.
*/
void mgu_free_dlist( GList *list ) {
	GList *node = list;
	while( node ) {
		g_free( node->data );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( list );
}

/*
* Coalesce linked list of characaters into one long string.
*/
gchar *mgu_list_coalesce( GSList *list ) {
	gchar *str = NULL;
	gchar *buf = NULL;
	gchar *start = NULL;
	GSList *node = NULL;
	gint len;

	if( ! list ) return NULL;

	/* Calculate maximum length of text */
	len = 0;
	node = list;
	while( node ) {
		str = node->data;
		len += 1 + strlen( str );
		node = g_slist_next( node );
	}

	/* Create new buffer. */
	buf = g_new0( gchar, len+1 );
	start = buf;
	node = list;
	while( node ) {
		str = node->data;
		len = strlen( str );
		strcpy( start, str );
		start += len;
		node = g_slist_next( node );
	}
	return buf;
}

struct mgu_error_entry {
	gint	e_code;
	gchar	*e_reason;
};

static const struct mgu_error_entry mgu_error_list[] = {
	{ MGU_SUCCESS,		"Success" },
	{ MGU_BAD_ARGS,		"Bad arguments" },
	{ MGU_NO_FILE,		"File not specified" },
	{ MGU_OPEN_FILE,	"Error opening file" },
	{ MGU_ERROR_READ,	"Error reading file" },
	{ MGU_EOF,		"End of file encountered" },
	{ MGU_OO_MEMORY,	"Error allocating memory" },
	{ MGU_BAD_FORMAT,	"Bad file format" },
	{ MGU_LDAP_CONNECT,	"Error connecting to LDAP server" },
	{ MGU_LDAP_INIT,	"Error initializing LDAP" },
	{ MGU_LDAP_BIND,	"Error binding to LDAP server" },
	{ MGU_LDAP_SEARCH,	"Error searching LDAP database" },
	{ MGU_LDAP_TIMEOUT,	"Timeout performing LDAP operation" },
	{ MGU_LDAP_CRITERIA,	"Error in LDAP search criteria" },
	{ MGU_LDAP_CRITERIA,	"Error in LDAP search criteria" },
	{ MGU_LDAP_NOENTRIES,	"No LDAP entries found for search criteria" },
	{ MGU_ERROR_WRITE,	"Error writing to file" },
	{ MGU_OPEN_DIRECTORY,	"Error opening directory" },
	{ MGU_NO_PATH,      	"No path specified" },
	{ -999,			NULL }
};

static const struct mgu_error_entry *mgu_error_find( gint err ) {
	gint i;
	for ( i = 0; mgu_error_list[i].e_code != -999; i++ ) {
		if ( err == mgu_error_list[i].e_code )
			return & mgu_error_list[i];
	}
	return NULL;
}

/*
* Return error message for specified error code.
*/
gchar *mgu_error2string( gint err ) {
	const struct mgu_error_entry *e;
	e = mgu_error_find( err );
	return ( e != NULL ) ? e->e_reason : "Unknown error";
}

/*
* Replace existing string with new string.
*/
gchar *mgu_replace_string( gchar *str, const gchar *value ) {
	if( str ) g_free( str );
	if( value ) {
		str = g_strdup( value );
		g_strstrip( str );
	}
	else {
		str = NULL;
	}
	return str;
}

/*
* Clear a linked list by setting node data pointers to NULL. Note that
* items are not freed.
*/
void mgu_clear_slist( GSList *list ) {
	GSList *node = list;
	while( node ) {
		node->data = NULL;
		node = g_slist_next( node );
	}
}

/*
* Clear a linked list by setting node data pointers to NULL. Note that
* items are not freed.
*/
void mgu_clear_list( GList *list ) {
	GList *node = list;
	while( node ) {
		node->data = NULL;
		node = g_list_next( node );
	}
}

/*
* Test and reformat an email address.
* Enter:  address.
* Return: Address, or NULL if address is empty.
* Note: Leading and trailing white space is removed.
*/
gchar *mgu_email_check_empty( gchar *address ) {
	gchar *retVal = NULL;
	if( address ) {
		retVal = g_strdup( address );
		retVal = g_strchug( retVal );
		retVal = g_strchomp( retVal );
		if( *retVal == '\0' ) {
			g_free( retVal );
			retVal = NULL;
		}
	}
	return retVal;
}

/*
* End of Source.
*/
