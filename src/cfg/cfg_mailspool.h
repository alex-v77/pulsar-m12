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

#ifndef __CFG_MAILSPOOL_H__
#define __CFG_MAILSPOOL_H__

int  interpret_cfg_data_Mailspool(strValues *val, strOptionsRealm  *realm);
void free_all_options_Mailspool(strOptionsRealm  *realm);
void dump_cfg_data_Mailspool(FILE *fd,const char *fill, strOptionsRealm  *realm);
int  add_realm_Mailspool(strOptionsRealm *dst,strOptionsRealm *src);

#else

#ifdef DEBUG
#warning file "cfg_mailspool.h" already included.
#endif /* DEBUG */

#endif /* __CFG_MAILSPOOL_H__ */
