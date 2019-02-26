/*
 * Copyright (C) 2001 Rok Papez <rok.papez@lugos.si>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>

#include "cfg.h"
#include "cfg_debug.h"

//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_Debug(strValues *val, strOptionsGlobal *opts) {
  if (val->opt_count || val->next || !val->val)
    return 6;

  return interpret_cfg_data_int(val->val, &opts->debug);
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_Debug(FILE *fd, strOptionsGlobal *opts) {
  if(!fd)
    return;
  fprintf(fd,"debug = %d\n", opts->debug);
}
//------------------------------------------------------------------------------------------------------------
