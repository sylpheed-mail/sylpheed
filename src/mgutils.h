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
 * General definitions for common address book entries.
 */

#ifndef __MGUTILS_H__
#define __MGUTILS_H__

#include <stdio.h>
#include <glib.h>

/* Error codes */
#define MGU_SUCCESS        0
#define MGU_BAD_ARGS       -1
#define MGU_NO_FILE        -2
#define MGU_OPEN_FILE      -3
#define MGU_ERROR_READ     -4
#define MGU_EOF            -5
#define MGU_OO_MEMORY      -6
#define MGU_BAD_FORMAT     -7
#define MGU_LDAP_CONNECT   -8
#define MGU_LDAP_INIT      -9
#define MGU_LDAP_BIND      -10
#define MGU_LDAP_SEARCH    -11
#define MGU_LDAP_TIMEOUT   -12
#define MGU_LDAP_CRITERIA  -13
#define MGU_LDAP_NOENTRIES -14
#define MGU_ERROR_WRITE    -15
#define MGU_OPEN_DIRECTORY -16
#define MGU_NO_PATH        -17

/* Function prototypes */
void mgu_print_list		( GSList *list, FILE *stream );
void mgu_print_dlist		( GList *list, FILE *stream );
void mgu_free_list		( GSList *list );
void mgu_free_dlist		( GList *list );
gchar *mgu_list_coalesce	( GSList *list );
gchar *mgu_error2string		( gint err );
gchar *mgu_replace_string	( gchar *str, const gchar *value );
void mgu_clear_slist		( GSList *list );
void mgu_clear_list		( GList *list );
gchar *mgu_email_check_empty	( gchar *address );

#endif /* __MGUTILS_H__ */
