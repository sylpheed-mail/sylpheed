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
 * Functions necessary to access LDAP servers.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_LDAP

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkmain.h>
#include <sys/time.h>
#include <string.h>
#define LDAP_DEPRECATED 1
#include <ldap.h>
#include <lber.h>
#include <pthread.h>
/* #include <dlfcn.h> */

#include "mgutils.h"
#include "addritem.h"
#include "addrcache.h"
#include "syldap.h"
#include "utils.h"

/*
* Create new LDAP server interface object.
*/
SyldapServer *syldap_create() {
	SyldapServer *ldapServer;

	debug_print("Creating LDAP server interface object\n");

	ldapServer = g_new0( SyldapServer, 1 );
	ldapServer->name = NULL;
	ldapServer->hostName = NULL;
	ldapServer->port = SYLDAP_DFL_PORT;
	ldapServer->baseDN = NULL;
	ldapServer->bindDN = NULL;
	ldapServer->bindPass = NULL;
	ldapServer->searchCriteria = NULL;
	ldapServer->searchValue = NULL;
	ldapServer->entriesRead = 0;
	ldapServer->maxEntries = SYLDAP_MAX_ENTRIES;
	ldapServer->timeOut = SYLDAP_DFL_TIMEOUT;
	ldapServer->newSearch = TRUE;
	ldapServer->addressCache = addrcache_create();
	ldapServer->thread = NULL;
	ldapServer->busyFlag = FALSE;
	ldapServer->retVal = MGU_SUCCESS;
	ldapServer->callBack = NULL;
	ldapServer->accessFlag = FALSE;
	ldapServer->idleId = 0;
	return ldapServer;
}

/*
* Specify name to be used.
*/
void syldap_set_name( SyldapServer* ldapServer, const gchar *value ) {
	ldapServer->name = mgu_replace_string( ldapServer->name, value );
	g_strstrip( ldapServer->name );
}

/*
* Specify hostname to be used.
*/
void syldap_set_host( SyldapServer* ldapServer, const gchar *value ) {
	addrcache_refresh( ldapServer->addressCache );
	ldapServer->hostName = mgu_replace_string( ldapServer->hostName, value );
	g_strstrip( ldapServer->hostName );
}

/*
* Specify port to be used.
*/
void syldap_set_port( SyldapServer* ldapServer, const gint value ) {
	addrcache_refresh( ldapServer->addressCache );
	if( value > 0 ) {
		ldapServer->port = value;
	}
	else {
		ldapServer->port = SYLDAP_DFL_PORT;
	}
}

/*
* Specify base DN to be used.
*/
void syldap_set_base_dn( SyldapServer* ldapServer, const gchar *value ) {
	addrcache_refresh( ldapServer->addressCache );
	ldapServer->baseDN = mgu_replace_string( ldapServer->baseDN, value );
	g_strstrip( ldapServer->baseDN );
}

/*
* Specify bind DN to be used.
*/
void syldap_set_bind_dn( SyldapServer* ldapServer, const gchar *value ) {
	addrcache_refresh( ldapServer->addressCache );
	ldapServer->bindDN = mgu_replace_string( ldapServer->bindDN, value );
	g_strstrip( ldapServer->bindDN );
}

/*
* Specify bind password to be used.
*/
void syldap_set_bind_password( SyldapServer* ldapServer, const gchar *value ) {
	addrcache_refresh( ldapServer->addressCache );
	ldapServer->bindPass = mgu_replace_string( ldapServer->bindPass, value );
	g_strstrip( ldapServer->bindPass );
}

/*
* Specify search criteria to be used.
*/
void syldap_set_search_criteria( SyldapServer* ldapServer, const gchar *value ) {
	addrcache_refresh( ldapServer->addressCache );
	ldapServer->searchCriteria = mgu_replace_string( ldapServer->searchCriteria, value );
	g_strstrip( ldapServer->searchCriteria );
	ldapServer->newSearch = TRUE;
}

/*
* Specify search value to be searched for.
*/
void syldap_set_search_value( SyldapServer* ldapServer, const gchar *value ) {
	addrcache_refresh( ldapServer->addressCache );
	ldapServer->searchValue = mgu_replace_string( ldapServer->searchValue, value );
	g_strstrip( ldapServer->searchValue );
	ldapServer->newSearch = TRUE;
}

/*
* Specify maximum number of entries to retrieve.
*/
void syldap_set_max_entries( SyldapServer* ldapServer, const gint value ) {
	addrcache_refresh( ldapServer->addressCache );
	if( value > 0 ) {
		ldapServer->maxEntries = value;
	}
	else {
		ldapServer->maxEntries = SYLDAP_MAX_ENTRIES;
	}
}

/*
* Specify timeout value for LDAP operation (in seconds).
*/
void syldap_set_timeout( SyldapServer* ldapServer, const gint value ) {
	addrcache_refresh( ldapServer->addressCache );
	if( value > 0 ) {
		ldapServer->timeOut = value;
	}
	else {
		ldapServer->timeOut = SYLDAP_DFL_TIMEOUT;
	}
}

/*
* Register a callback function. When called, the function will be passed
* this object as an argument.
*/
void syldap_set_callback( SyldapServer *ldapServer, void *func ) {
	ldapServer->callBack = func;
}

void syldap_set_accessed( SyldapServer *ldapServer, const gboolean value ) {
	g_return_if_fail( ldapServer != NULL );
	ldapServer->accessFlag = value;
}

/*
* Refresh internal variables to force a file read.
*/
void syldap_force_refresh( SyldapServer *ldapServer ) {
	addrcache_refresh( ldapServer->addressCache );
	ldapServer->newSearch = TRUE;
}

gint syldap_get_status( SyldapServer *ldapServer ) {
	g_return_val_if_fail( ldapServer != NULL, -1 );
	return ldapServer->retVal;
}

ItemFolder *syldap_get_root_folder( SyldapServer *ldapServer ) {
	g_return_val_if_fail( ldapServer != NULL, NULL );
	return addrcache_get_root_folder( ldapServer->addressCache );
}

gchar *syldap_get_name( SyldapServer *ldapServer ) {
	g_return_val_if_fail( ldapServer != NULL, NULL );
	return ldapServer->name;
}

gboolean syldap_get_accessed( SyldapServer *ldapServer ) {
	g_return_val_if_fail( ldapServer != NULL, FALSE );
	return ldapServer->accessFlag;
}

/*
* Free up LDAP server interface object by releasing internal memory.
*/
void syldap_free( SyldapServer *ldapServer ) {
	g_return_if_fail( ldapServer != NULL );

	debug_print("Freeing LDAP server interface object\n");

	ldapServer->callBack = NULL;

	/* Free internal stuff */
	g_free( ldapServer->name );
	g_free( ldapServer->hostName );
	g_free( ldapServer->baseDN );
	g_free( ldapServer->bindDN );
	g_free( ldapServer->bindPass );
	g_free( ldapServer->searchCriteria );
	g_free( ldapServer->searchValue );
	g_free( ldapServer->thread );

	ldapServer->port = 0;
	ldapServer->entriesRead = 0;
	ldapServer->maxEntries = 0;
	ldapServer->newSearch = FALSE;

	/* Clear cache */
	addrcache_clear( ldapServer->addressCache );
	addrcache_free( ldapServer->addressCache );

	/* Clear pointers */
	ldapServer->name = NULL;
	ldapServer->hostName = NULL;
	ldapServer->baseDN = NULL;
	ldapServer->bindDN = NULL;
	ldapServer->bindPass = NULL;
	ldapServer->searchCriteria = NULL;
	ldapServer->searchValue = NULL;
	ldapServer->addressCache = NULL;
	ldapServer->thread = NULL;
	ldapServer->busyFlag = FALSE;
	ldapServer->retVal = MGU_SUCCESS;
	ldapServer->accessFlag = FALSE;

	/* Now release LDAP object */
	g_free( ldapServer );

}

/*
* Display object to specified stream.
*/
void syldap_print_data( SyldapServer *ldapServer, FILE *stream ) {
	g_return_if_fail( ldapServer != NULL );

	fprintf( stream, "SyldapServer:\n" );
	fprintf( stream, "     name: '%s'\n", ldapServer->name );
	fprintf( stream, "host name: '%s'\n", ldapServer->hostName );
	fprintf( stream, "     port: %d\n",   ldapServer->port );
	fprintf( stream, "  base dn: '%s'\n", ldapServer->baseDN );
	fprintf( stream, "  bind dn: '%s'\n", ldapServer->bindDN );
	fprintf( stream, "bind pass: '%s'\n", ldapServer->bindPass );
	fprintf( stream, " criteria: '%s'\n", ldapServer->searchCriteria );
	fprintf( stream, "searchval: '%s'\n", ldapServer->searchValue );
	fprintf( stream, "max entry: %d\n",   ldapServer->maxEntries );
	fprintf( stream, " num read: %d\n",   ldapServer->entriesRead );
	fprintf( stream, "  ret val: %d\n",   ldapServer->retVal );
	addrcache_print( ldapServer->addressCache, stream );
	addritem_print_item_folder( ldapServer->addressCache->rootFolder, stream );
}

/*
* Display object to specified stream.
*/
void syldap_print_short( SyldapServer *ldapServer, FILE *stream ) {
	g_return_if_fail( ldapServer != NULL );

	fprintf( stream, "SyldapServer:\n" );
	fprintf( stream, "     name: '%s'\n", ldapServer->name );
	fprintf( stream, "host name: '%s'\n", ldapServer->hostName );
	fprintf( stream, "     port: %d\n",   ldapServer->port );
	fprintf( stream, "  base dn: '%s'\n", ldapServer->baseDN );
	fprintf( stream, "  bind dn: '%s'\n", ldapServer->bindDN );
	fprintf( stream, "bind pass: '%s'\n", ldapServer->bindPass );
	fprintf( stream, " criteria: '%s'\n", ldapServer->searchCriteria );
	fprintf( stream, "searchval: '%s'\n", ldapServer->searchValue );
	fprintf( stream, "max entry: %d\n",   ldapServer->maxEntries );
	fprintf( stream, " num read: %d\n",   ldapServer->entriesRead );
	fprintf( stream, "  ret val: %d\n",   ldapServer->retVal );
}

#if 0
/*
* Build an address list entry and append to list of address items. Name is formatted
* as it appears in the common name (cn) attribute.
*/
static void syldap_build_items_cn( SyldapServer *ldapServer, GSList *listName, GSList *listAddr ) {
	ItemPerson *person;
	ItemEMail *email;
	GSList *nodeName = listName;

	while( nodeName ) {
		GSList *nodeAddress = listAddr;
		person = addritem_create_item_person();
		addritem_person_set_common_name( person, nodeName->data );
		addrcache_id_person( ldapServer->addressCache, person );
		addrcache_add_person( ldapServer->addressCache, person );

		while( nodeAddress ) {
			email = addritem_create_item_email();
			addritem_email_set_address( email, nodeAddress->data );
			addrcache_id_email( ldapServer->addressCache, email );
			addrcache_person_add_email( ldapServer->addressCache, person, email );
			nodeAddress = g_slist_next( nodeAddress );
			ldapServer->entriesRead++;
		}
		nodeName = g_slist_next( nodeName );
	}
}
#endif

/*
* Build an address list entry and append to list of address items. Name is formatted
* as "<first-name> <last-name>".
*/
static void syldap_build_items_fl( SyldapServer *ldapServer, GSList *listAddr, GSList *listFirst, GSList *listLast  ) {
	GSList *nodeFirst = listFirst;
	GSList *nodeAddress = listAddr;
	gchar *firstName = NULL, *lastName = NULL, *fullName = NULL;
	gint iLen = 0, iLenT = 0;
	ItemPerson *person;
	ItemEMail *email;

	/* Find longest first name in list */
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
	if( listLast ) {
		lastName = listLast->data;
	}

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
	if( fullName ) {
		g_strchug( fullName ); g_strchomp( fullName );
	}

	if( nodeAddress ) {
		person = addritem_create_item_person();
		addritem_person_set_common_name( person, fullName );
		addritem_person_set_first_name( person, firstName );
		addritem_person_set_last_name( person, lastName );
		addrcache_id_person( ldapServer->addressCache, person );
		addrcache_add_person( ldapServer->addressCache, person );
	}

	/* Add address item */
	while( nodeAddress ) {
		email = addritem_create_item_email();
		addritem_email_set_address( email, nodeAddress->data );
		addrcache_id_email( ldapServer->addressCache, email );
		addrcache_person_add_email( ldapServer->addressCache, person, email );
		nodeAddress = g_slist_next( nodeAddress );
		ldapServer->entriesRead++;
	}
	g_free( fullName );
	fullName = firstName = lastName = NULL;

}

/*
* Add all attribute values to a list.
*/
static GSList *syldap_add_list_values( LDAP *ld, LDAPMessage *entry, char *attr ) {
	GSList *list = NULL;
	gint i;
	gchar **vals;

	if( ( vals = ldap_get_values( ld, entry, attr ) ) != NULL ) {
		for( i = 0; vals[i] != NULL; i++ ) {
			/* printf( "lv\t%s: %s\n", attr, vals[i] ); */
			list = g_slist_append( list, g_strdup( vals[i] ) );
		}
	}
	ldap_value_free( vals );
	return list;
}

/*
* Add a single attribute value to a list.
*/
static GSList *syldap_add_single_value( LDAP *ld, LDAPMessage *entry, char *attr ) {
	GSList *list = NULL;
	gchar **vals;

	if( ( vals = ldap_get_values( ld, entry, attr ) ) != NULL ) {
		if( vals[0] != NULL ) {
			/* printf( "sv\t%s: %s\n", attr, vals[0] ); */
			list = g_slist_append( list, g_strdup( vals[0] ) );
		}
	}
	ldap_value_free( vals );
	return list;
}

/*
* Free linked lists of character strings.
*/
static void syldap_free_lists( GSList *listName, GSList *listAddr, GSList *listID, GSList *listDN, GSList *listFirst, GSList *listLast ) {
	mgu_free_list( listName );
	mgu_free_list( listAddr );
	mgu_free_list( listID );
	mgu_free_list( listDN );
	mgu_free_list( listFirst );
	mgu_free_list( listLast );
}

/*
* Check parameters that are required for a search. This should
* be called before performing a search.
* Return: TRUE if search criteria appear OK.
*/
gboolean syldap_check_search( SyldapServer *ldapServer ) {
	g_return_val_if_fail( ldapServer != NULL, FALSE );

	ldapServer->retVal = MGU_LDAP_CRITERIA;

	/* Test search criteria */
	if( ldapServer->searchCriteria == NULL ) {
		return FALSE;
	}
	if( strlen( ldapServer->searchCriteria ) < 1 ) {
		return FALSE;
	}

	if( ldapServer->searchValue == NULL ) {
		return FALSE;
	}
	if( strlen( ldapServer->searchValue ) < 1 ) {
		return FALSE;
	}

	ldapServer->retVal = MGU_SUCCESS;
	return TRUE;
}

/*
* Perform the LDAP search, reading LDAP entries into cache.
* Note that one LDAP entry can have multiple values for many of its
* attributes. If these attributes are E-Mail addresses; these are
* broken out into separate address items. For any other attribute,
* only the first occurrence is read.
*/
gint syldap_search( SyldapServer *ldapServer ) {
	LDAP *ld;
	LDAPMessage *result, *e;
	char *attribs[10];
	char *attribute;
	gchar *criteria;
	BerElement *ber;
	gint rc;
	GSList *listName = NULL, *listAddress = NULL, *listID = NULL;
	GSList *listFirst = NULL, *listLast = NULL, *listDN = NULL;
	struct timeval timeout;
	gboolean entriesFound = FALSE;

	g_return_val_if_fail( ldapServer != NULL, -1 );

	ldapServer->retVal = MGU_SUCCESS;
	if( ! syldap_check_search( ldapServer ) ) {
		return ldapServer->retVal;
	}

	/* Set timeout */
	timeout.tv_sec = ldapServer->timeOut;
	timeout.tv_usec = 0L;

	ldapServer->entriesRead = 0;
	if( ( ld = ldap_init( ldapServer->hostName, ldapServer->port ) ) == NULL ) {
		ldapServer->retVal = MGU_LDAP_INIT;
		return ldapServer->retVal;
	}

	/* printf( "connected to LDAP host %s on port %d\n", ldapServer->hostName, ldapServer->port ); */

	/* Bind to the server, if required */
	if( ldapServer->bindDN ) {
		if( * ldapServer->bindDN != '\0' ) {
			/* printf( "binding...\n" ); */
			rc = ldap_simple_bind_s( ld, ldapServer->bindDN, ldapServer->bindPass );
			/* printf( "rc=%d\n", rc ); */
			if( rc != LDAP_SUCCESS ) {
				/* printf( "LDAP Error: ldap_simple_bind_s: %s\n", ldap_err2string( rc ) ); */
				ldap_unbind( ld );
				ldapServer->retVal = MGU_LDAP_BIND;
				return ldapServer->retVal;
			}
		}
	}

	/* Define all attributes we are interested in. */
	attribs[0] = SYLDAP_ATTR_DN;
	attribs[1] = SYLDAP_ATTR_COMMONNAME;
	attribs[2] = SYLDAP_ATTR_GIVENNAME;
	attribs[3] = SYLDAP_ATTR_SURNAME;
	attribs[4] = SYLDAP_ATTR_EMAIL;
	attribs[5] = SYLDAP_ATTR_UID;
	attribs[6] = NULL;

	/* Create LDAP search string and apply search criteria */
	criteria = g_strdup_printf( ldapServer->searchCriteria, ldapServer->searchValue );
	rc = ldap_search_ext_s( ld, ldapServer->baseDN, LDAP_SCOPE_SUBTREE, criteria, attribs, 0, NULL, NULL,
		       &timeout, 0, &result );
	g_free( criteria );
	criteria = NULL;
	if( rc == LDAP_TIMEOUT ) {
		ldap_unbind( ld );
		ldapServer->retVal = MGU_LDAP_TIMEOUT;
		return ldapServer->retVal;
	}
	if( rc != LDAP_SUCCESS ) {
		/* printf( "LDAP Error: ldap_search_st: %s\n", ldap_err2string( rc ) ); */
		ldap_unbind( ld );
		ldapServer->retVal = MGU_LDAP_SEARCH;
		return ldapServer->retVal;
	}

	/* printf( "Total results are: %d\n", ldap_count_entries( ld, result ) ); */

	/* Clear the cache if we have new entries, otherwise leave untouched. */
	if( ldap_count_entries( ld, result ) > 0 ) {
		addrcache_clear( ldapServer->addressCache );
	}

	/* Process results */
	ldapServer->entriesRead = 0;
	for( e = ldap_first_entry( ld, result ); e != NULL; e = ldap_next_entry( ld, e ) ) {
		entriesFound = TRUE;
		if( ldapServer->entriesRead >= ldapServer->maxEntries ) break;		
		/* printf( "DN: %s\n", ldap_get_dn( ld, e ) ); */

		/* Process all attributes */
		for( attribute = ldap_first_attribute( ld, e, &ber ); attribute != NULL;
			       attribute = ldap_next_attribute( ld, e, ber ) ) {
			if( g_ascii_strcasecmp( attribute, SYLDAP_ATTR_COMMONNAME ) == 0 ) {
				listName = syldap_add_list_values( ld, e, attribute );
			}
			if( g_ascii_strcasecmp( attribute, SYLDAP_ATTR_EMAIL ) == 0 ) {
				listAddress = syldap_add_list_values( ld, e, attribute );
			}
			if( g_ascii_strcasecmp( attribute, SYLDAP_ATTR_UID ) == 0 ) {
				listID = syldap_add_single_value( ld, e, attribute );
			}
			if( g_ascii_strcasecmp( attribute, SYLDAP_ATTR_GIVENNAME ) == 0 ) {
				listFirst = syldap_add_list_values( ld, e, attribute );
			}
			if( g_ascii_strcasecmp( attribute, SYLDAP_ATTR_SURNAME ) == 0 ) {
				listLast = syldap_add_single_value( ld, e, attribute );
			}
			if( g_ascii_strcasecmp( attribute, SYLDAP_ATTR_DN ) == 0 ) {
				listDN = syldap_add_single_value( ld, e, attribute );
			}

			/* Free memory used to store attribute */
			ldap_memfree( attribute );
		}

		/* Format and add items to cache */
		syldap_build_items_fl( ldapServer, listAddress, listFirst, listLast );

		/* Free up */
		syldap_free_lists( listName, listAddress, listID, listDN, listFirst, listLast );
		listName = listAddress = listID = listFirst = listLast = listDN = NULL;

		if( ber != NULL ) {
			ber_free( ber, 0 );
		}
	}

	syldap_free_lists( listName, listAddress, listID, listDN, listFirst, listLast );
	listName = listAddress = listID = listFirst = listLast = listDN = NULL;
	
	/* Free up and disconnect */
	ldap_msgfree( result );
	ldap_unbind( ld );
	ldapServer->newSearch = FALSE;
	if( entriesFound ) {
		ldapServer->retVal = MGU_SUCCESS;
	}
	else {
		ldapServer->retVal = MGU_LDAP_NOENTRIES;
	}
	return ldapServer->retVal;
}

/* syldap_display_search_results() - updates the ui. this function is called from the
 * main thread (the thread running the GTK event loop). */
static gboolean syldap_display_search_results(SyldapServer *ldapServer)
{
	/* NOTE: when this function is called the accompanying thread should
	 * already be terminated. */
	gdk_threads_enter();
	ldapServer->callBack(ldapServer);
	gdk_threads_leave();
	/* FIXME:  match should know whether to free this SyldapServer stuff. */
	g_free(ldapServer->thread);
	ldapServer->thread = NULL;
	return FALSE;
}

/* ============================================================================================ */
/*
* Read data into list. Main entry point
* Return: TRUE if file read successfully.
*/
/* ============================================================================================ */
gint syldap_read_data( SyldapServer *ldapServer ) {
	g_return_val_if_fail( ldapServer != NULL, -1 );

	ldapServer->accessFlag = FALSE;
	pthread_detach( pthread_self() );
	if( ldapServer->newSearch ) {
		/* Read data into the list */
		syldap_search( ldapServer );

		/* Mark cache */
		ldapServer->addressCache->modified = FALSE;
		ldapServer->addressCache->dataRead = TRUE;
		ldapServer->accessFlag = FALSE;
	}

	/* Callback */
	ldapServer->busyFlag = FALSE;
	if( ldapServer->callBack ) {
		/* make the ui thread update the search results */
		ldapServer->idleId = g_idle_add((GSourceFunc)syldap_display_search_results, ldapServer);
	}

	return ldapServer->retVal;
}

/* ============================================================================================ */
/*
* Cancel read with thread.
*/
/* ============================================================================================ */
void syldap_cancel_read( SyldapServer *ldapServer ) {
	g_return_if_fail( ldapServer != NULL );

	/* DELETEME: this is called from inside UI thread so it's OK, Christoph! */
	if( ldapServer->thread ) {
		/* printf( "thread cancelled\n" ); */
		pthread_cancel( *ldapServer->thread );
	}
	g_free(ldapServer->thread);
	ldapServer->thread = NULL;
	ldapServer->busyFlag = FALSE;
}

/* ============================================================================================ */
/*
* Read data into list using a background thread.
* Return: TRUE if file read successfully. Callback function will be
* notified when search is complete.
*/
/* ============================================================================================ */
gint syldap_read_data_th( SyldapServer *ldapServer ) {
	g_return_val_if_fail( ldapServer != NULL, -1 );

	ldapServer->busyFlag = FALSE;
	syldap_check_search( ldapServer );
	if( ldapServer->retVal == MGU_SUCCESS ) {
		/* debug_print("Staring LDAP read thread\n"); */

		ldapServer->busyFlag = TRUE;
		ldapServer->thread = g_new0(pthread_t, 1);
		pthread_create( ldapServer->thread, NULL, (void *) syldap_read_data, (void *) ldapServer );
	}
	return ldapServer->retVal;
}

/*
* Return link list of persons.
*/
GList *syldap_get_list_person( SyldapServer *ldapServer ) {
	g_return_val_if_fail( ldapServer != NULL, NULL );
	return addrcache_get_list_person( ldapServer->addressCache );
}

/*
* Return link list of folders. This is always NULL since there are
* no folders in GnomeCard.
* Return: NULL.
*/
GList *syldap_get_list_folder( SyldapServer *ldapServer ) {
	g_return_val_if_fail( ldapServer != NULL, NULL );
	return NULL;
}

#define SYLDAP_TEST_FILTER   "(objectclass=*)"
#define SYLDAP_SEARCHBASE_V2 "cn=config"
#define SYLDAP_SEARCHBASE_V3 ""
#define SYLDAP_V2_TEST_ATTR  "database"
#define SYLDAP_V3_TEST_ATTR  "namingcontexts"

/*
* Attempt to discover the base DN for the server.
* Enter:
*	host	Host name
*	port	Port number
*	bindDN	Bind DN (optional).
*	bindPW	Bind PW (optional).
*	tov	Timeout value (seconds), or 0 for none, default 30 secs.
* Return: List of Base DN's, or NULL if could not read. Base DN should
* be g_free() when done.
*/
GList *syldap_read_basedn_s( const gchar *host, const gint port, const gchar *bindDN, const gchar *bindPW, const gint tov ) {
	GList *baseDN = NULL;
	LDAP *ld;
	gint rc, i;
	LDAPMessage *result, *e;
	gchar *attribs[10];
	BerElement *ber;
	gchar *attribute;
	gchar **vals;
	struct timeval timeout;

	if( host == NULL ) return baseDN;
	if( port < 1 ) return baseDN;

	/* Set timeout */
	timeout.tv_usec = 0L;
	if( tov > 0 ) {
		timeout.tv_sec = tov;
	}
	else {
		timeout.tv_sec = 30L;
	}

	/* Connect to server. */
	if( ( ld = ldap_init( host, port ) ) == NULL ) {
		return baseDN;
	}

	/* Bind to the server, if required */
	if( bindDN ) {
		if( *bindDN != '\0' ) {
			rc = ldap_simple_bind_s( ld, bindDN, bindPW );
			if( rc != LDAP_SUCCESS ) {
				/* printf( "LDAP Error: ldap_simple_bind_s: %s\n", ldap_err2string( rc ) ); */
				ldap_unbind( ld );
				return baseDN;
			}
		}
	}

	/* Test for LDAP version 3 */
	attribs[0] = SYLDAP_V3_TEST_ATTR;
	attribs[1] = NULL;
	rc = ldap_search_ext_s( ld, SYLDAP_SEARCHBASE_V3, LDAP_SCOPE_BASE, SYLDAP_TEST_FILTER, attribs,
		       0, NULL, NULL, &timeout, 0, &result );
	if( rc == LDAP_SUCCESS ) {
		/* Process entries */
		for( e = ldap_first_entry( ld, result ); e != NULL; e = ldap_next_entry( ld, e ) ) {
			/* printf( "DN: %s\n", ldap_get_dn( ld, e ) ); */

			/* Process attributes */
			for( attribute = ldap_first_attribute( ld, e, &ber ); attribute != NULL;
					attribute = ldap_next_attribute( ld, e, ber ) ) {
				if( g_ascii_strcasecmp( attribute, SYLDAP_V3_TEST_ATTR ) == 0 ) {
					if( ( vals = ldap_get_values( ld, e, attribute ) ) != NULL ) {
						for( i = 0; vals[i] != NULL; i++ ) {
							/* printf( "\t%s: %s\n", attribute, vals[i] ); */
							baseDN = g_list_append( baseDN, g_strdup( vals[i] ) );
						}
					}
					ldap_value_free( vals );
				}
				ldap_memfree( attribute );
			}
			if( ber != NULL ) {
				ber_free( ber, 0 );
			}
		}
		ldap_msgfree( result );
	}
	else {
	}

	if( baseDN == NULL ) {
		/* Test for LDAP version 2 */
		attribs[0] = NULL;
		rc = ldap_search_ext_s( ld, SYLDAP_SEARCHBASE_V2, LDAP_SCOPE_BASE, SYLDAP_TEST_FILTER, attribs,
			       0, NULL, NULL, &timeout, 0, &result );
		if( rc == LDAP_SUCCESS ) {
			/* Process entries */
			for( e = ldap_first_entry( ld, result ); e != NULL; e = ldap_next_entry( ld, e ) ) {
				/* if( baseDN ) break;			 */
				/* printf( "DN: %s\n", ldap_get_dn( ld, e ) ); */

				/* Process attributes */
				for( attribute = ldap_first_attribute( ld, e, &ber ); attribute != NULL;
					       attribute = ldap_next_attribute( ld, e, ber ) ) {
					/* if( baseDN ) break;			 */
					if( g_ascii_strcasecmp( attribute, SYLDAP_V2_TEST_ATTR ) == 0 ) {
						if( ( vals = ldap_get_values( ld, e, attribute ) ) != NULL ) {
							for( i = 0; vals[i] != NULL; i++ ) {
								char *ch;
								/* Strip the 'ldb:' from the front of the value */
								ch = ( char * ) strchr( vals[i], ':' );
								if( ch ) {
									gchar *bn = g_strdup( ++ch );
									g_strchomp( bn );
									g_strchug( bn );
									baseDN = g_list_append( baseDN, g_strdup( bn ) );
								}
							}
						}
						ldap_value_free( vals );
					}
					ldap_memfree( attribute );
				}
				if( ber != NULL ) {
					ber_free( ber, 0 );
				}
			}
			ldap_msgfree( result );
		}
	}
	ldap_unbind( ld );
	return baseDN;
}

/*
* Attempt to discover the base DN for the server.
* Enter:  ldapServer Server to test.
* Return: List of Base DN's, or NULL if could not read. Base DN should
* be g_free() when done. Return code set in ldapServer.
*/
GList *syldap_read_basedn( SyldapServer *ldapServer ) {
	GList *baseDN = NULL;
	LDAP *ld;
	gint rc, i;
	LDAPMessage *result, *e;
	gchar *attribs[10];
	BerElement *ber;
	gchar *attribute;
	gchar **vals;
	struct timeval timeout;

	if( ldapServer == NULL ) return baseDN;
	ldapServer->retVal = MGU_BAD_ARGS;
	if( ldapServer->hostName == NULL ) return baseDN;
	if( ldapServer->port < 1 ) return baseDN;

	/* Set timeout */
	timeout.tv_usec = 0L;
	if( ldapServer->timeOut > 0 ) {
		timeout.tv_sec = ldapServer->timeOut;
	}
	else {
		timeout.tv_sec = 30L;
	}

	/* Connect to server. */
	if( ( ld = ldap_init( ldapServer->hostName, ldapServer->port ) ) == NULL ) {
		ldapServer->retVal = MGU_LDAP_INIT;
		return baseDN;
	}

	/* Bind to the server, if required */
	if( ldapServer->bindDN ) {
		if( *ldapServer->bindDN != '\0' ) {
			rc = ldap_simple_bind_s( ld, ldapServer->bindDN, ldapServer->bindPass );
			if( rc != LDAP_SUCCESS ) {
				/* printf( "LDAP Error: ldap_simple_bind_s: %s\n", ldap_err2string( rc ) ); */
				ldap_unbind( ld );
				ldapServer->retVal = MGU_LDAP_BIND;
				return baseDN;
			}
		}
	}

	ldapServer->retVal = MGU_LDAP_SEARCH;

	/* Test for LDAP version 3 */
	attribs[0] = SYLDAP_V3_TEST_ATTR;
	attribs[1] = NULL;
	rc = ldap_search_ext_s( ld, SYLDAP_SEARCHBASE_V3, LDAP_SCOPE_BASE, SYLDAP_TEST_FILTER, attribs,
		       0, NULL, NULL, &timeout, 0, &result );
	if( rc == LDAP_SUCCESS ) {
		/* Process entries */
		for( e = ldap_first_entry( ld, result ); e != NULL; e = ldap_next_entry( ld, e ) ) {
			/* printf( "DN: %s\n", ldap_get_dn( ld, e ) ); */

			/* Process attributes */
			for( attribute = ldap_first_attribute( ld, e, &ber ); attribute != NULL;
					attribute = ldap_next_attribute( ld, e, ber ) ) {
				if( g_ascii_strcasecmp( attribute, SYLDAP_V3_TEST_ATTR ) == 0 ) {
					if( ( vals = ldap_get_values( ld, e, attribute ) ) != NULL ) {
						for( i = 0; vals[i] != NULL; i++ ) {
							/* printf( "\t%s: %s\n", attribute, vals[i] ); */
							baseDN = g_list_append( baseDN, g_strdup( vals[i] ) );
						}
					}
					ldap_value_free( vals );
				}
				ldap_memfree( attribute );
			}
			if( ber != NULL ) {
				ber_free( ber, 0 );
			}
		}
		ldap_msgfree( result );
		ldapServer->retVal = MGU_SUCCESS;
	}
	else if( rc == LDAP_TIMEOUT ) {
		ldapServer->retVal = MGU_LDAP_TIMEOUT;
	}

	if( baseDN == NULL ) {
		/* Test for LDAP version 2 */
		attribs[0] = NULL;
		rc = ldap_search_ext_s( ld, SYLDAP_SEARCHBASE_V2, LDAP_SCOPE_BASE, SYLDAP_TEST_FILTER, attribs,
			       0, NULL, NULL, &timeout, 0, &result );
		if( rc == LDAP_SUCCESS ) {
			/* Process entries */
			for( e = ldap_first_entry( ld, result ); e != NULL; e = ldap_next_entry( ld, e ) ) {
				/* if( baseDN ) break;			 */
				/* printf( "DN: %s\n", ldap_get_dn( ld, e ) ); */

				/* Process attributes */
				for( attribute = ldap_first_attribute( ld, e, &ber ); attribute != NULL;
					       attribute = ldap_next_attribute( ld, e, ber ) ) {
					/* if( baseDN ) break;			 */
					if( g_ascii_strcasecmp( attribute, SYLDAP_V2_TEST_ATTR ) == 0 ) {
						if( ( vals = ldap_get_values( ld, e, attribute ) ) != NULL ) {
							for( i = 0; vals[i] != NULL; i++ ) {
								char *ch;
								/* Strip the 'ldb:' from the front of the value */
								ch = ( char * ) strchr( vals[i], ':' );
								if( ch ) {
									gchar *bn = g_strdup( ++ch );
									g_strchomp( bn );
									g_strchug( bn );
									baseDN = g_list_append( baseDN, g_strdup( bn ) );
								}
							}
						}
						ldap_value_free( vals );
					}
					ldap_memfree( attribute );
				}
				if( ber != NULL ) {
					ber_free( ber, 0 );
				}
			}
			ldap_msgfree( result );
			ldapServer->retVal = MGU_SUCCESS;
		}
		else if( rc == LDAP_TIMEOUT ) {
			ldapServer->retVal = MGU_LDAP_TIMEOUT;
		}
	}
	ldap_unbind( ld );

	return baseDN;
}

/*
* Attempt to connect to the server.
* Enter:
*	host	Host name
*	port	Port number
* Return: TRUE if connected successfully.
*/
gboolean syldap_test_connect_s( const gchar *host, const gint port ) {
	gboolean retVal = FALSE;
	LDAP *ld;

	if( host == NULL ) return retVal;
	if( port < 1 ) return retVal;
	if( ( ld = ldap_open( host, port ) ) != NULL ) {
		retVal = TRUE;
	}
	if( ld != NULL ) {
		ldap_unbind( ld );
	}
	return retVal;
}

/*
* Attempt to connect to the server.
* Enter:  ldapServer Server to test.
* Return: TRUE if connected successfully. Return code set in ldapServer.
*/
gboolean syldap_test_connect( SyldapServer *ldapServer ) {
	gboolean retVal = FALSE;
	LDAP *ld;

	if( ldapServer == NULL ) return retVal;
	ldapServer->retVal = MGU_BAD_ARGS;
	if( ldapServer->hostName == NULL ) return retVal;
	if( ldapServer->port < 1 ) return retVal;
	ldapServer->retVal = MGU_LDAP_INIT;
	if( ( ld = ldap_open( ldapServer->hostName, ldapServer->port ) ) != NULL ) {
		ldapServer->retVal = MGU_SUCCESS;
		retVal = TRUE;
	}
	if( ld != NULL ) {
		ldap_unbind( ld );
	}
	return retVal;
}

#define LDAP_LINK_LIB_NAME_1 "libldap.so"
#define LDAP_LINK_LIB_NAME_2 "liblber.so"
#define LDAP_LINK_LIB_NAME_3 "libresolv.so"
#define LDAP_LINK_LIB_NAME_4 "libpthread.so"

/*
* Test whether LDAP libraries installed.
* Return: TRUE if library available.
*/
#if 0
gboolean syldap_test_ldap_lib() {
	void *handle, *fun;
	
	/* Get library */
	handle = dlopen( LDAP_LINK_LIB_NAME_1, RTLD_LAZY );
	if( ! handle ) {
		return FALSE;
	}

	/* Test for symbols we need */
	fun = dlsym( handle, "ldap_init" );
	if( ! fun ) {
		dlclose( handle );
		return FALSE;
	}
	dlclose( handle ); handle = NULL; fun = NULL;

	handle = dlopen( LDAP_LINK_LIB_NAME_2, RTLD_LAZY );
	if( ! handle ) {
		return FALSE;
	}
	fun = dlsym( handle, "ber_init" );
	if( ! fun ) {
		dlclose( handle );
		return FALSE;
	}
	dlclose( handle ); handle = NULL; fun = NULL;

	handle = dlopen( LDAP_LINK_LIB_NAME_3, RTLD_LAZY );
	if( ! handle ) {
		return FALSE;
	}
	fun = dlsym( handle, "res_query" );
	if( ! fun ) {
		dlclose( handle );
		return FALSE;
	}
	dlclose( handle ); handle = NULL; fun = NULL;

	handle = dlopen( LDAP_LINK_LIB_NAME_4, RTLD_LAZY );
	if( ! handle ) {
		return FALSE;
	}
	fun = dlsym( handle, "pthread_create" );
	if( ! fun ) {
		dlclose( handle );
		return FALSE;
	}
	dlclose( handle ); handle = NULL; fun = NULL;

	return TRUE;
}
#endif /* 0 */

#endif	/* USE_LDAP */

/*
* End of Source.
*/
