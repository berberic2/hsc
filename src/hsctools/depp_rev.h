/*
 * This source code is part of hsc, a html-preprocessor,
 * Copyright (C) 1995-1998  Thomas Aglassinger
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#define STRINGIFY(x) _STRINGIFY(x)
#define _STRINGIFY(x) #x

#define VERSION		1
#define REVISION	5
#define DATE	"17.09.05"
#define TIME	"17:14:33"

#define __VSTR STRINGIFY(VERSION)
#define __RSTR STRINGIFY(REVISION)

#define PRGNAME	"hscdepp"
#define VERS	PRGNAME __VSTR "." __RSTR
#define VSTRING	PRGNAME __VSTR "." __RSTR DATE "\n"
#define VERSTAG	"\0$VER: " PRGNAME  __VSTR "." __RSTR DATE
#define BASENAME "HSCDEPP"
#define VSTR	PRGNAME __VSTR "." __RSTR DATE

