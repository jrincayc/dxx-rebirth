/*
 * This file is part of the DXX-Rebirth project <https://www.dxx-rebirth.com/>.
 * It is copyright by its individual contributors, as recorded in the
 * project's Git history.  See COPYING.txt at the top level for license
 * terms and a link to the Git history.
 */
#ifndef _CUSTOM_H
#define _CUSTOM_H

#include "pstypes.h"
#include "piggy.h"

#include "dxxsconf.h"
#include "d_array.h"

/* from piggy.c */
#define DBM_FLAG_LARGE	128		// Flags added onto the flags struct in b
#define DBM_FLAG_ABM            64

extern enumerated_array<pig_bitmap_offset, MAX_BITMAP_FILES, bitmap_index> GameBitmapOffset;

void load_custom_data(const d_fname &level_file);

void custom_close();

#endif
