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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "cfg.h"
#include "cfg_realm_chars.h"

#include "util.h"
//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_RealmChars(strValues *val, strOptionsGlobal *opts) {
  int size = 0;
  int i;
  strValues *tmp = val;

  while(tmp) {
    if (tmp->opt_count)
      return 6;
    size += strlen(tmp->val);
    tmp = tmp->next;
  }

  if(opts->realm_chars)
    free(opts->realm_chars);
  opts->realm_chars = safe_malloc(size+1);
  if(!opts->realm_chars)
    return 2;

  opts->realm_chars[0]='\0';
  tmp = val;
  while(tmp) {
    strcat(opts->realm_chars, tmp->val);
    tmp = tmp->next;
  }

  for(i=0;i<size;i++) // only punctuations may be used
    if(!ispunct(opts->realm_chars[i])) {
      free(opts->realm_chars);
      opts->realm_chars = NULL;
      return 6;
    }
  return 0;
}
//------------------------------------------------------------------------------------------------------------
void free_all_options_RealmChars(strOptionsGlobal *opts) {
  if(opts->realm_chars) {
    free(opts->realm_chars);
    opts->realm_chars = NULL;
  }
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_RealmChars(FILE *fd, strOptionsGlobal *opts) {
  if(!fd)
    return;
  if(opts->realm_chars)
    fprintf(fd,"realm_chars = \"%s\"\n",opts->realm_chars);
  else
    fprintf(fd,"#realm_chars =\n");
  return;
}
//------------------------------------------------------------------------------------------------------------
