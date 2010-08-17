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
 * Import LDIF data.
 */

#ifndef __IMPORT_LDIF_H__
#define __IMPORT_LDIF_H__

/* Function prototypes */
AddressBookFile *addressbook_imp_ldif( AddressIndex *addrIndex );
AddressBookFile *addressbook_imp_ldif_file( AddressIndex *addrIndex,
					    const gchar *file,
					    const gchar *book_name );

#endif /* __IMPORT_LDIF_H__ */

/*
* End of Source.
*/
