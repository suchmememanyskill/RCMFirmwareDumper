/*
 * Common Module Header
 * Copyright (C) 2018 M4xw
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include <stddef.h>
//TODO: Move it to BDK
#include "common_gfx.h"
#include "common_heap.h"

// Module Callback
typedef void (*cbMainModule_t)(const char *s);
typedef void (*memcpy_t)(void *, void *, size_t);
typedef void (*memset_t)(void *, int, size_t);

typedef struct _bdkParams_t
{
	gfx_con_t *gfxCon;
	gfx_ctxt_t *gfxCtx;
	heap_t *sharedHeap;
	memcpy_t memcpy;
	memset_t memset;
} *bdkParams_t;

// Module Entrypoint
typedef void (*moduleEntrypoint_t)(void *, bdkParams_t);
