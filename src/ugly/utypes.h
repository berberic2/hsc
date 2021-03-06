/*
 * This source code is part of hsc, a html-preprocessor,
 * Copyright (C) 1993-1998  Thomas Aglassinger
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
#ifndef UGLY_TYPES_H
#define UGLY_TYPES_H

/*
 * ugly/types.h
 *
 * ugly data typing.
 *
 * NOTE: contains also UGLY_VER and UGLY_REV and
 * includes debuggin defines
 *
 */

/* include debugging defines */
#include "udebug.h"

#ifdef AMIGA
#include <exec/types.h>

#elif defined WINNT
#include <windef.h>
typedef void *APTR;             /* 32-bit untyped pointer */
typedef long LONG;              /* signed 32-bit quantity */
typedef unsigned long ULONG;    /* unsigned 32-bit quantity */
typedef unsigned short UWORD;   /* unsigned 16-bit quantity */
typedef unsigned char UBYTE;    /* unsigned 8-bit quantity */
typedef char *STRPTR;           /* string pointer (NULL terminated) */
typedef unsigned char TEXT;
#ifndef NULL
#define NULL            ((void*)0L)
#endif
#define BYTEMASK        (0xFF)
#define WORDMASK        (0xFFFF)

#else /* AMIGA/WINNT */

typedef void *APTR;             /* 32-bit untyped pointer */
typedef long LONG;              /* signed 32-bit quantity */
typedef unsigned long ULONG;    /* unsigned 32-bit quantity */
typedef short WORD;             /* signed 16-bit quantity */
typedef unsigned short UWORD;   /* unsigned 16-bit quantity */
#if __STDC__
typedef signed char BYTE;       /* signed 8-bit quantity */
#else
typedef char BYTE;              /* signed 8-bit quantity */
#endif
typedef unsigned char UBYTE;    /* unsigned 8-bit quantity */

typedef char *STRPTR;           /* string pointer (NULL terminated) */

/* Types with specific semantics */
#ifndef RISCOS
typedef unsigned char TEXT;
#else
typedef char TEXT;
#endif

typedef enum { FALSE=0, TRUE=1} BOOL;

#ifndef NULL
#define NULL            ((void*)0L)
#endif

#define BYTEMASK        (0xFF)
#define WORDMASK        (0xFFFF)

#endif /* AMIGA/WINNT */

/*
 *
 * global typedefs (on any system)
 *
 */

typedef const char *CONSTRPTR;         /* string constants */
typedef char STRARR;   /* string array */
typedef char CHAR;     /* single character */

/*
 * UPTR as an generic pointer. C-math will not operate on UPTR.
 * UPTR can be converted to any other pointer and the other way round.
 * It is used by ugly functions, especially the umx-functions
 */
typedef void *UPTR;             /* generic pointer ( ANSI-def ) */

/*
 * compare/delete function type
 */
typedef int cmp_func(UPTR data1, UPTR data2);
typedef void del_func(UPTR data);

#endif /* UGLY_TYPES_H */

