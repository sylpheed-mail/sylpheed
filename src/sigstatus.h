/* sigstatus.h - GTK+ based signature status display
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

#ifndef GPGMEGTK_SIGSTATUS_H
#define GPGMEGTK_SIGSTATUS_H

#include <gpgme.h>

struct gpgmegtk_sig_status_s;
typedef struct gpgmegtk_sig_status_s *GpgmegtkSigStatus;

GpgmegtkSigStatus gpgmegtk_sig_status_create(void);
void gpgmegtk_sig_status_destroy(GpgmegtkSigStatus hd);
void gpgmegtk_sig_status_update(GpgmegtkSigStatus hd, gpgme_ctx_t ctx);

const gchar *gpgmegtk_sig_status_to_string(gpgme_signature_t signature,
        gboolean use_name);

#endif /* GPGMEGTK_SIGSTATUS_H */
