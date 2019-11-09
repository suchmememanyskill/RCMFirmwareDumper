/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018 shuffle2
 * Copyright (c) 2018 balika011
 * Copyright (c) 2019 CTCaer
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

#include <string.h>

#include "../soc/fuse.h"
#include "../soc/t210.h"

void fuse_disable_program()
{
	FUSE(FUSE_DISABLEREGPROGRAM) = 1;
}

u32 fuse_read_odm(u32 idx)
{
	return FUSE(FUSE_RESERVED_ODMX(idx));
}
