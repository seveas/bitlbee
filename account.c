  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2004 Wilmer van der Gaast and others                *
  \********************************************************************/

/* Account management functions                                         */

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
#include "account.h"

account_t *account_add( irc_t *irc, int protocol, char *user, char *pass )
{
	account_t *a;
	
	if( irc->accounts )
	{
		for( a = irc->accounts; a->next; a = a->next );
		a = a->next = bitlbee_alloc( sizeof( account_t ) );
	}
	else
	{
		irc->accounts = a = bitlbee_alloc( sizeof( account_t ) );
	}
	
	memset( a, 0, sizeof( account_t ) );
	
	a->protocol = protocol;
	a->user = strdup( user );
	a->pass = strdup( pass );
	
	return( a );
}

account_t *account_get( irc_t *irc, int nr )
{
	account_t *a;
	
	for( a = irc->accounts; a; a = a->next )
		if( nr == -1 )		/* Special case: nr == -1 gets the last one, we might need it sometimes */
		{
			if( !a->next )
				return( a );
		}
		else if( ( nr-- ) == 0 )
		{
			return( a );
		}
		
	return( NULL );
}

void account_del( irc_t *irc, int nr )
{
	account_t *a, *l = NULL;
	
	for( a = irc->accounts; a; a = (l=a)->next )
		if( ( nr-- ) == 0 )
		{
			if( a->gc ) return; /* Caller should have checked, accounts still in use can't be deleted. */
			
			if( l )
			{
				l->next = a->next;
			}
			else
			{
				irc->accounts = a->next;
			}
			
			free( a->user );
			free( a->pass );
			if( a->server ) free( a->server );
			if( a->reconnect )	/* This prevents any reconnect still queued to happen */
				cancel_auto_reconnect( a );
			free( a );
			
			break;
		}
}

void account_on( irc_t *irc, account_t *a )
{
	struct aim_user *u;
	
	if( a->gc )
	{
		/* Trying to enable an already-enabled account */
		return;
	}
	
	if( proto_prpl[a->protocol]->login == NULL )
	{
		irc_usermsg( irc, "Support for protocol %s is not included in this BitlBee", proto_name[a->protocol] );
		return;
	}
	
	cancel_auto_reconnect( a );
	
	u = bitlbee_alloc( sizeof( struct aim_user ) );
	memset( u, 0, sizeof( *u ) );
	u->protocol = a->protocol;
	strcpy( u->username, a->user );
	strcpy( u->password, a->pass );
	if( a->server) strcpy( u->proto_opt[0], a->server );
	
	a->gc = (struct gaim_connection *) u; /* Bit hackish :-/ */
	a->reconnect = 0;
	
	proto_prpl[a->protocol]->login( u );
}

void account_off( irc_t *irc, account_t *a )
{
	account_offline( a->gc );
	a->gc = NULL;
	if( a->reconnect )
	{
		/* Shouldn't happen */
		cancel_auto_reconnect( a );
	}
}
