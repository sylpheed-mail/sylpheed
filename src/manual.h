/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#ifndef __MANUAL_H__
#define __MANUAL_H__

typedef enum
{
	MANUAL_LANG_DE,
	MANUAL_LANG_EN,
	MANUAL_LANG_ES,
	MANUAL_LANG_FR,
	MANUAL_LANG_IT,
	MANUAL_LANG_JA,
} ManualLang;

void manual_open(ManualLang lang);
void faq_open	(ManualLang lang);

#endif /* __MANUAL_H__ */
