  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2003 Wilmer van der Gaast and others                *
  \********************************************************************/

/* Some stuff to fetch, save and handle nicknames for your buddies      */

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

void nick_set( irc_t *irc, char *handle, int proto, char *nick )
{
	nick_t *m = NULL, *n = irc->nicks;
	
	while( n )
	{
		if( ( strcasecmp( n->handle, handle ) == 0 ) && n->proto == proto )
		{
			free( n->nick );
			n->nick = strdup( nick );
			nick_strip( n->nick );
			
			return;
		}
		n = ( m = n )->next;	// :-P
	}
	
	if( m )
		n = m->next = malloc( sizeof( nick_t ) );
	else
		n = irc->nicks = malloc( sizeof( nick_t ) );
	memset( n, 0, sizeof( nick_t ) );
	
	n->handle = strdup( handle );
	n->proto = proto;
	n->nick = strdup( nick );
	
	nick_strip( n->nick );
}

char *nick_get( irc_t *irc, char *handle, int proto )
{
	static char nick[MAX_NICK_LENGTH+1];
	nick_t *n = irc->nicks;
	
	memset( nick, 0, MAX_NICK_LENGTH + 1 );
	
	while( n && !*nick )
		if( ( n->proto == proto ) && ( strcasecmp( n->handle, handle ) == 0 ) )
			strcpy( nick, n->nick );
		else
			n = n->next;
	
	if( !n )
	{
		char *s;
		
		snprintf( nick, MAX_NICK_LENGTH, "%s", handle );
		if( ( s = strchr( nick, '@' ) ) )
			while( *s )
				*(s++) = 0;
		nick_strip( nick );
	}
	
	while( !nick_ok( nick) || user_find( irc, nick ) )
		if( strlen( nick ) < MAX_NICK_LENGTH )
			nick[strlen(nick)] = '_';
		else
			nick[0] ++;
	
	return( nick );
}

void nick_del( irc_t *irc, char *nick )
{
	nick_t *l = NULL, *n = irc->nicks;
	
	while( n )
	{
		if( strcasecmp( n->nick, nick ) == 0 )
		{
			if( l )
				l->next = n->next;
			else
				irc->nicks = n->next;
			
			free( n->handle );
			free( n->nick );
			free( n );
			
			break;
		}
		n = (l=n)->next;
	}
}


/* Character maps, _lc_[x] == _uc_[x] (but uppercase), according to the RFC's

   Actually, the RFC forbids -, but I think - being an lowercase _ looks better... */

static char *nick_lc_chars = "0123456789abcdefghijklmnopqrstuvwxyz{}^-|";
static char *nick_uc_chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ[]~_\\";

void nick_strip( char * nick )
{
	int i, j;
	
	for( i = j = 0; nick[i] && i < MAX_NICK_LENGTH; i++ )
	{
		if(strchr(nick_lc_chars,nick[i]) || 
		   strchr(nick_uc_chars,nick[i]))
		{
			nick[j] = nick[i];
			j++;
		}
	}
	nick[j] = '\0';
}

int nick_ok( char *nick )
{
	char *s;
	
	/* Empty/long nicks are not allowed */
	if( !*nick || strlen( nick ) > MAX_NICK_LENGTH )
		return( 0 );
	
	for( s = nick; *s; s ++ )
		if( !strchr( nick_lc_chars, *s ) && !strchr( nick_uc_chars, *s ) )
			return( 0 );
	
	return( 1 );
}

int nick_lc( char *nick )
{
	char *s, *t;
	int diff = nick_lc_chars - nick_uc_chars;
	
	for( s = nick; *s; s ++ )
	{
		t = strchr( nick_uc_chars, *s );
		if( t )
			*s = *(t+diff);
		else if( !strchr( nick_lc_chars, *s ) )
			return( 0 );
	}
	
	return( 1 );
}

int nick_uc( char *nick )
{
	char *s, *t;
	int diff = nick_uc_chars - nick_lc_chars;
	
	for( s = nick; *s; s ++ )
	{
		t = strchr( nick_lc_chars, *s );
		if( t )
			*s = *(t+diff);
		else if( !strchr( nick_uc_chars, *s ) )
			return( 0 );
	}
	
	return( 1 );
}

int nick_cmp( char *a, char *b )
{
	char *aa, *bb;
	int res;
	
	aa = strdup( a );
	bb = strdup( b );
	if( nick_lc( aa ) && nick_lc( bb ) )
	{
		res = strcmp( aa, bb );
	}
	else
	{
		res = -1;	/* Hmm... Not a clear answer.. :-/ */
	}
	free( aa );
	free( bb );
	
	return( res );
}
