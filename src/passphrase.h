/* passphrase.h - GTK+ based passphrase callback
 *      Copyright (C) 2001 Werner Koch (dd9jn)
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

#ifndef GPGMEGTK_PASSPHRASE_H
#define GPGMEGTK_PASSPHRASE_H

#include <glib.h>
#include <gpgme.h>

struct passphrase_cb_info_s {
    GpgmeCtx c;
    int did_it;
};

void gpgmegtk_set_passphrase_grab (gint yesno);
const char* gpgmegtk_passphrase_cb(void *opaque, const char *desc, void **r_hd);
void gpgmegtk_free_passphrase();

#endif /* GPGMEGTK_PASSPHRASE_H */
