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

#include "cfg.h"
#include "cfg_certificate.h"
#include "error_facility.h"

//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_Certificate(strValues *val, strOptionsGlobal *opts) {
  int rc;

  if (val->opt_count>1 || val->next) {
    err_debug(0, "Bad certificate specification at \"%s\".", val->val);
    rc = 6;
    goto error;
  }

  rc = interpret_cfg_data_string(val->val, &opts->cert_file);
  if(rc)
    goto error;

  if(val->opt_count) {
    rc = interpret_cfg_data_string(val->opt[0], &opts->key_file);
    if(rc)
      goto error;
  } else {
    if(opts->key_file)
      free(opts->key_file);
    opts->key_file = NULL;
  }

  return 0;

error:
  free_all_options_Certificate(opts);
  return rc;
}
//------------------------------------------------------------------------------------------------------------
void free_all_options_Certificate(strOptionsGlobal *opts) {
  if(opts->cert_file)
    free(opts->cert_file);
  opts->cert_file = NULL;

  if(opts->key_file)
    free(opts->key_file);
  opts->key_file = NULL;

  return;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_Certificate(FILE *fd, strOptionsGlobal *opts) {
  if(!fd || !opts)
    return;

  if(!opts->cert_file) {
    fprintf(fd, "#certificate =\n");
    return;
  }

  fprintf(fd, "certificate = \"%s\"", opts->cert_file);
  if(opts->key_file)
    fprintf(fd, " : \"%s\"", opts->key_file);
  fprintf(fd, "\n");

  return;
}
//------------------------------------------------------------------------------------------------------------
