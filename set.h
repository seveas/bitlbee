  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2003 Wilmer van der Gaast and others                *
  \********************************************************************/

/* Some stuff to register, handle and save user preferences             */

/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License with
  the Debian GNU/Linux distribution in /usr/share/common-licenses/GPL;
  if not, write to the Free Software Foundation, Inc., 59 Temple Place,
  Suite 330, Boston, MA  02111-1307  USA
*/

typedef struct
{
	char *key;
	char *value;
	char *def;	/* Default */
	/* Eval: Returns NULL if the value is incorrect. Can return a
	   corrected value. set_setstr() should be able to free() this
	   string! */
	char *(*eval) ( irc_t *irc, void *set, char *value );
	void *next;
} set_t;

set_t *set_add( irc_t *irc, char *key, char *def, void *eval );
set_t *set_find( irc_t *irc, char *key );
char *set_getstr( irc_t *irc, char *key );
int set_getint( irc_t *irc, char *key );
int set_setstr( irc_t *irc, char *key, char *value );
int set_setint( irc_t *irc, char *key, int value );
void set_del( irc_t *irc, char *key );
char *set_eval_int( irc_t *irc, set_t *set, char *value );
char *set_eval_bool( irc_t *irc, set_t *set, char *value );
