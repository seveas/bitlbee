  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2004 Wilmer van der Gaast and others                *
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

#define BITLBEE_CORE
#include "bitlbee.h"

user_t *user_add( irc_t *irc, char *nick )
{
	user_t *u;
	char *key;
	
	u = irc->users;
	if( u )
	{
		while( 1 )
		{
			if( nick_cmp( u->nick, nick ) == 0 )
				return( NULL );
			if( u->next )
				u = u->next;
			else
				break;
		}
		u->next = g_new0( user_t, 1 );
		u = u->next;
	}
	else
	{
		irc->users = u = g_new0( user_t, 1 );
	}
	
	u->user = u->realname = u->host = u->nick = g_strdup( nick );
	u->is_private = set_getint( irc, "private" );
	
	key = g_strdup( nick );
	nick_lc( key );
	g_hash_table_insert( irc->userhash, key, u );
	
	return( u );
}

int user_del( irc_t *irc, char *nick )
{
	user_t *u, *t;
	char *key;
	gpointer okey, ovalue;
	
	u = irc->users;
	t = NULL;
	while( u )
	{
		if( nick_cmp( u->nick, nick ) == 0 )
		{
			/* Get this key now already, since "nick" might be free()d
			   at the time we start playing with the hash... */
			key = g_strdup( nick );
			nick_lc( key );
			
			if( t )
				t->next = u->next;
			else
				irc->users = u->next;
			if( u->online )
				irc_kill( irc, u );
			g_free( u->nick );
			if( u->nick != u->user ) g_free( u->user );
			if( u->nick != u->host ) g_free( u->host );
			if( u->nick != u->realname ) g_free( u->realname );
			if( u->away ) g_free( u->away );
			if( u->handle ) g_free( u->handle );
			if( u->sendbuf ) g_free( u->sendbuf );
			if( u->sendbuf_timer ) g_source_remove( u->sendbuf_timer );
			g_free( u );
			
			if( !g_hash_table_lookup_extended( irc->userhash, key, &okey, &ovalue ) || ovalue != u )
			{
				g_free( key );
				return( 1 );	/* Although this is a severe error, the user is removed from the list... */
			}
			g_hash_table_remove( irc->userhash, key );
			g_free( key );
			g_free( okey );
			
			return( 1 );
		}
		u = (t=u)->next;
	}
	
	return( 0 );
}

user_t *user_find( irc_t *irc, char *nick )
{
	char key[512] = "";
	
	strncpy( key, nick, sizeof( key ) - 1 );
	nick_lc( key );
	
	return( g_hash_table_lookup( irc->userhash, key ) );
}

user_t *user_findhandle( struct gaim_connection *gc, char *handle )
{
	user_t *u = gc->irc->users;
	
	while( u )
	{
		if( u->gc == gc && u->handle && handle_cmp( u->handle, handle, gc->protocol ) == 0 )
			break;
		u = u->next;
	}
	
	return( u );
}

void user_rename( irc_t *irc, char *oldnick, char *newnick )
{
	user_t *u = user_find( irc, oldnick );
	gpointer okey, ovalue;
	char *key;
	
	if( !u ) return;	/* Should've been checked by the caller... */
	
	g_free( u->nick );
	if( u->nick == u->user ) u->user = NULL;
	if( u->nick == u->host ) u->host = NULL;
	if( u->nick == u->realname ) u->realname = NULL;
	u->nick = g_strdup( newnick );
	if( !u->user ) u->user = u->nick;
	if( !u->host ) u->user = u->host;
	if( !u->realname ) u->user = u->realname;
	
	/* Remove the old reference to this user from the hash and create a
	   new one with the new nick. This is indeed a bit messy. */
	key = g_strdup( oldnick );
	nick_lc( key );
	if( !g_hash_table_lookup_extended( irc->userhash, key, &okey, &ovalue ) || ovalue != u )
	{
		g_free( key );
		return;		/* This really shouldn't happen! */	
	}
	g_hash_table_remove( irc->userhash, key );
	g_free( key );
	g_free( okey );
	
	key = g_strdup( newnick );
	nick_lc( key );
	g_hash_table_insert( irc->userhash, key, u );
}
