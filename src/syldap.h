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
 * Definitions necessary to access LDAP servers.
 */

#ifndef __SYLDAP_H__
#define __SYLDAP_H__

#ifdef USE_LDAP

#include <glib.h>
#include <pthread.h>

#include "addritem.h"
#include "addrcache.h"

#define SYLDAP_DFL_PORT        389
#define SYLDAP_MAX_ENTRIES     20
#define SYLDAP_DFL_TIMEOUT     30
#define SYLDAP_DFL_CRITERIA    "(&(mail=*)(cn=%s*))"
	
#define SYLDAP_ATTR_DN         "dn"
#define SYLDAP_ATTR_COMMONNAME "cn"
#define SYLDAP_ATTR_GIVENNAME  "givenName"
#define SYLDAP_ATTR_SURNAME    "sn"
#define SYLDAP_ATTR_EMAIL      "mail"
#define SYLDAP_ATTR_UID        "uid"

typedef struct _SyldapServer SyldapServer;
struct _SyldapServer {
	gchar        *name;
	gchar        *hostName;
	gint         port;
	gchar        *baseDN;
	gchar        *bindDN;
	gchar        *bindPass;
	gchar        *searchCriteria;
	gchar        *searchValue;
	gint         entriesRead;
	gint         maxEntries;
	gint         timeOut;
	gboolean     newSearch;
	AddressCache *addressCache;
	/* ItemFolder   *rootFolder; */
	gboolean     accessFlag;
	gint         retVal;
	pthread_t    *thread;
	gboolean     busyFlag;
	void         (*callBack)( void * );
	guint        idleId;
};

/* Function prototypes */
SyldapServer *syldap_create	( void );
void syldap_set_name		( SyldapServer* ldapServer, const gchar *value );
void syldap_set_host		( SyldapServer* ldapServer, const gchar *value );
void syldap_set_port		( SyldapServer* ldapServer, const gint value );
void syldap_set_base_dn		( SyldapServer* ldapServer, const gchar *value );
void syldap_set_bind_dn		( SyldapServer* ldapServer, const gchar *value );
void syldap_set_bind_password	( SyldapServer* ldapServer, const gchar *value );
void syldap_set_search_criteria	( SyldapServer* ldapServer, const gchar *value );
void syldap_set_search_value	( SyldapServer* ldapServer, const gchar *value );
void syldap_set_max_entries	( SyldapServer* ldapServer, const gint value );
void syldap_set_timeout		( SyldapServer* ldapServer, const gint value );
void syldap_set_callback	( SyldapServer *ldapServer, void *func );
void syldap_set_accessed	( SyldapServer *ldapServer, const gboolean value );
void syldap_force_refresh	( SyldapServer *ldapServer );
void syldap_free		( SyldapServer *ldapServer );
gint syldap_get_status		( SyldapServer *ldapServer );
gboolean syldap_get_accessed	( SyldapServer *ldapServer );
gchar *syldap_get_name		( SyldapServer *ldapServer );

void syldap_print_data		( SyldapServer *ldapServer, FILE *stream );
gboolean syldap_check_search	( SyldapServer *ldapServer );
gint syldap_read_data		( SyldapServer *ldapServer );
gint syldap_read_data_th	( SyldapServer *ldapServer );
void syldap_cancel_read		( SyldapServer *ldapServer );

/* GList *syldap_get_address_list	( const SyldapServer *ldapServer ); */
ItemFolder *syldap_get_root_folder	( SyldapServer *ldapServer );
GList *syldap_get_list_person	( SyldapServer *ldapServer );
GList *syldap_get_list_folder	( SyldapServer *ldapServer );

GList *syldap_read_basedn_s	( const gchar *host, const gint port, const gchar *bindDN,
				  const gchar *bindPW, const gint tov );
GList *syldap_read_basedn	( SyldapServer *ldapServer );
gboolean syldap_test_connect_s	( const gchar *host, const gint port );
gboolean syldap_test_connect	( SyldapServer *ldapServer );
/* gboolean syldap_test_ldap_lib	( void ); */

#endif	/* USE_LDAP */

#endif /* __SYLDAP_H__ */
