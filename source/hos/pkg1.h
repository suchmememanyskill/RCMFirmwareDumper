/*
 * Copyright (c) 2018 naehrwert
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

#ifndef _PKG1_H_
#define _PKG1_H_

#include "../utils/types.h"

typedef struct _key_info_t
{
	u32 start_offset;
	u32 hks_offset;
	bool hks_offset_is_from_end;
	u32 alignment;
	u32 hash_max;
	u8 hash_order[13];
	u32 es_offset;
	u32 ssl_offset;
} key_info_t;

typedef struct _pkg1_id_t
{
	const char *id;
	u32 kb;
	key_info_t key_info;
} pkg1_id_t;

#define HASH_ORDER_100_100 {2, 3, 4, 0, 5, 6, 1}
#define HASH_ORDER_200_510 {2, 3, 4, 0, 5, 7, 10, 12, 11, 6, 8, 1}
#define HASH_ORDER_600_620 {6, 5, 10, 7, 8, 2, 3, 4, 0, 12, 11, 1}
#define HASH_ORDER_700_9xx {6, 5, 10, 7, 8, 2, 3, 4, 0, 12, 11, 9, 1}

const pkg1_id_t *pkg1_identify(u8 *pkg1);

#endif
