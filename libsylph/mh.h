/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __MH_H__
#define __MH_H__

#include <glib.h>

#include "folder.h"

typedef struct _MHFolder	MHFolder;

#define MH_FOLDER(obj)		((MHFolder *)obj)

struct _MHFolder
{
	LocalFolder lfolder;
};

FolderClass *mh_get_class	(void);

#endif /* __MH_H__ */
