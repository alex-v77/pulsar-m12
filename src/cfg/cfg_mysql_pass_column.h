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

#ifndef __CFG_MYSQL_PASS_COLUMN_H__
#define __CFG_MYSQL_PASS_COLUMN_H__

int  interpret_cfg_data_mysql_PassColumn(strValues *val, strOptionsMySQL *mysql);
void free_all_options_mysql_PassColumn(strOptionsMySQL *mysql);
void dump_cfg_data_mysql_PassColumn(FILE *fd, strOptionsMySQL *mysql);

#else

#ifdef DEBUG
#warning file "cfg_mysql_pass_column.h" already included.
#endif /* DEBUG */

#endif /* __CFG_MYSQL_PASS_COLUMN_H__ */
