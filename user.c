  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2003 Wilmer van der Gaast and others                *
  \********************************************************************/

/* Stuff to handle, save and search buddies                             */

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

#include "bitlbee.h"

user_t *user_add( irc_t *irc, char *nick )
{
	user_t *u;
	
	u = irc->users;
	if( u )
	{
		while( 1 )
		{
			if( strcasecmp( u->nick, nick ) == 0 )
				return( NULL );
			if( u->next )
				u = u->next;
			else
				break;
		}
		u->next = malloc( sizeof( user_t ) );
		u = u->next;
	}
	else
	{
		irc->users = u = malloc( sizeof( user_t ) );
	}
	memset( u, 0, sizeof( user_t ) );
	
	u->user = u->realname = u->host = u->nick = strdup( nick );
	u->private = set_getint( irc, "private" );
	
	return( u );
}

int user_del( irc_t *irc, char *nick )
{
	user_t *u, *t;
	
	u = irc->users;
	t = NULL;
	while( u )
	{
		if( strcasecmp( u->nick, nick ) == 0 )
		{
			if( t )
				t->next = u->next;
			else
				irc->users = u->next;
			if( u->online )
				irc_kill( irc, u );
			free( u->nick );
			if( u->nick != u->user ) free( u->user );
			if( u->nick != u->host ) free( u->host );
			if( u->nick != u->realname ) free( u->realname );
			free( u );
			return( 1 );
		}
		t = u;
		u = u->next;
	}
	
	return( 0 );
}

user_t *user_find( irc_t *irc, char *nick )
{
	user_t *u = irc->users;
	
	while( u )
	{
		if( nick_cmp( u->nick, nick ) == 0 )
			break;
		u = u->next;
	}
	
	return( u );
}

user_t *user_findhandle( struct gaim_connection *gc, char *handle )
{
	user_t *u = gc->irc->users;
	
	while( u )
	{
		if( u->gc == gc && u->handle && strcasecmp( u->handle, handle ) == 0 )
			break;
		u = u->next;
	}
	
	return( u );
}
