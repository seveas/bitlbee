  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2003 Wilmer van der Gaast and others                *
  \********************************************************************/

/* User manager (root) commands                                         */

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

#include "commands.h"
#include "crypting.h"
#include "bitlbee.h"
#include "help.h"

#include <unistd.h>
#include <string.h>

command_t commands[] = {
	{ "help",       0, cmd_help }, 
	{ "identify",   1, cmd_identify },
	{ "register",   1, cmd_register },
	{ "account",    1, cmd_account },
	{ "add",        2, cmd_add },
	{ "info",       1, cmd_info },
	{ "rename",     2, cmd_rename },
	{ "remove",     1, cmd_remove },
	{ "block",      1, cmd_block },
	{ "allow",      1, cmd_allow },
	{ "save",       0, cmd_save },
	{ "set",        0, cmd_set },
	{ "yes",        0, cmd_yesno },
	{ "no",         0, cmd_yesno },
	{ "blist",      0, cmd_blist },
	{ "nick",       1, cmd_nick },
	{ "qlist",	0, cmd_qlist },
	{ NULL }
};

int cmd_help( irc_t *irc, char **cmd )
{
	char param[80];
	int i;
	char *s;
	
	memset( param, 0, sizeof(param) );
	for ( i = 1; (cmd[i] != NULL && ( strlen(param) < (sizeof(param)-1) ) ); i++ ) {
		if ( i != 1 )	// prepend space except for the first parameter
			strcat(param, " ");
		strncat( param, cmd[i], sizeof(param) - strlen(param) - 1 );
	}

	s = help_get( irc, param );
	if( !s ) s = help_get( irc, "" );
	
	if( s )
	{
		irc_usermsg( irc, "%s", s );
		free( s );
		return( 1 );
	}
	else
	{
		irc_usermsg( irc, "Error opening helpfile." );
		return( 0 );
	}
}

int cmd_identify( irc_t *irc, char **cmd )
{
	int checkie = bitlbee_load( irc, cmd[1] );
	
	if( checkie == -1 )
	{
		irc_usermsg( irc, "Incorrect password" );
	}
	else if( checkie == 0 )
	{
		irc_usermsg( irc, "The nick is (probably) not registered" );
	}
	else if( checkie == 1 )
	{
		irc_usermsg( irc, "Password accepted" );
	}
	else
	{
		irc_usermsg( irc, "Something very weird happened" );
	}
	
	return( 0 );
}

int cmd_register( irc_t *irc, char **cmd )
{
	int checkie;
	char *str;
	
	if( conf->authmode == REGISTERED )
	{
		irc_usermsg( irc, "This server does not allow registering new accounts" );
		return( 0 );
	}
	
	str = (char *) malloc( strlen( irc->nick ) +
	      strlen( CONFIG ) +
	      strlen( ".accounts" ) + 1 );
	
	strcpy( str, CONFIG );
	strcat( str, irc->nick );
	strcat( str, ".accounts" );
	
	checkie = access( str, F_OK );
	
	strcpy( str, CONFIG );
	strcat( str, irc->nick );
	strcat( str, ".nicks" );
	
	checkie += access( str, F_OK );
	
	if( checkie == -2 )
	{
		setpassnc( irc, cmd[1] );
		root_command_string( irc, user_find( irc, irc->mynick ), "save" );
	}
	else
	{
		irc_usermsg( irc, "Nick is already registered" );
	}
	
	return( 0 );
}

int cmd_account( irc_t *irc, char **cmd )
{
	account_t *a;
	int i;
	
	if( conf->authmode == REGISTERED && irc->status < USTATUS_IDENTIFIED )
	{
		irc_usermsg( irc, "This server only accepts registered users" );
		return( 0 );
	}
	
	if( strcasecmp( cmd[1], "add" ) == 0 )
	{
		int prot;
		
		if( cmd[4] == NULL )
		{
			irc_usermsg( irc, "Not enough parameters" );
			return( 0 );
		}
		
		for( prot = 0; prot < PROTO_MAX; prot ++ )
			if( proto_name[prot] && *proto_name[prot] && strcasecmp( proto_name[prot], cmd[2] ) == 0 )
				break;
		
		if( ( prot == PROTO_MAX ) || ( proto_prpl[prot] == NULL ) )
		{
			irc_usermsg( irc, "Unknown protocol" );
			return( 0 );
		}

		if( prot == PROTO_OSCAR && cmd[5] == NULL )
		{
			irc_usermsg( irc, "Not enough parameters" );
			return( 0 );
		}
		
		a = account_add( irc, prot, cmd[3], cmd[4] );
		
		if( cmd[5] )
			a->server = strdup( cmd[5] );
		
		irc_usermsg( irc, "Account successfully added" );
	}
	else if( strcasecmp( cmd[1], "del" ) == 0 )
	{
		if( ( cmd[2] == NULL ) || ( sscanf( cmd[2], "%d", &i ) != 1 ) )
		{
			irc_usermsg( irc, "Not enough, or invalid parameters" );
		}
		else if( ( a = account_get( irc, i ) ) && a->gc )
		{
			irc_usermsg( irc, "Account is still logged in, can't delete" );
		}
		else if( !a )
		{
			irc_usermsg( irc, "Invalid account number" );
		}
		else
		{
			account_del( irc, i );
			irc_usermsg( irc, "Account deleted" );
		}
	}
	else if( strcasecmp( cmd[1], "list" ) == 0 )
	{
		i = 0;
		
		for( a = irc->accounts; a; a = a->next )
		{
			char *con;
			
			if( a->gc )
				con = " (connected)";
			else if( a->reconnect )
				con = " (awaiting reconnect)";
			else
				con = "";
			
			if( a->protocol == PROTO_OSCAR || a->protocol == PROTO_ICQ || a->protocol == PROTO_TOC )
				irc_usermsg( irc, "%2d. OSCAR, %s on %s%s", i, a->user, a->server, con );
			else
				irc_usermsg( irc, "%2d. %s, %s%s", i, proto_name[a->protocol], a->user, con );
			
			i ++;
		}
		irc_usermsg( irc, "End of account list" );
	}
	else if( strcasecmp( cmd[1], "on" ) == 0 )
	{
		if( cmd[2] )
		{
			if( ( sscanf( cmd[2], "%d", &i ) == 1 ) && ( a = account_get( irc, i ) ) )
			{
				if( a->gc )
				{
					irc_usermsg( irc, "Account already online" );
					return( 0 );
				}
				else
				{
					account_on( irc, a );
				}
			}
			else
			{
				irc_usermsg( irc, "Incorrect account number" );
				return( 0 );
			}
		}
		else
		{
			irc_usermsg( irc, "Trying to get all accounts connected..." );
			
			for( a = irc->accounts; a; a = a->next )
				if( !a->gc )
					account_on( irc, a );
		}
	}
	else if( strcasecmp( cmd[1], "off" ) == 0 )
	{
		if( !cmd[2] )
		{
			irc_usermsg( irc, "Not enough parameters" );
			return( 0 );
		}
		
		if( ( sscanf( cmd[2], "%d", &i ) == 1 ) && ( a = account_get( irc, i ) ) )
		{
			if( a->gc )
			{
				account_off( irc, a );
			}
			else if( a->reconnect )
			{
				cancel_auto_reconnect( a );
				irc_usermsg( irc, "Reconnect for connection %d cancelled", i );
			}
			else
			{
				irc_usermsg( irc, "Account already offline" );
				return( 0 );
			}
		}
		else
		{
			irc_usermsg( irc, "Incorrect account number" );
			return( 0 );
		}
	}
	
	return( 1 );
}

int cmd_add( irc_t *irc, char **cmd )
{
	int i;
	struct gaim_connection *gc;
	
	if( !cmd[1] || !sscanf( cmd[1], "%d", &i ) || !( gc = gc_nr( i ) ) )
	{
		irc_usermsg( irc, "Incorrect connection number" );
		return( 1 );
	}
	if( cmd[3] )
	{
		if( !nick_ok( cmd[3] ) )
		{
			irc_usermsg( irc, "The requested nick `%s' is invalid", cmd[3] );
			return( 0 );
		}
		else if( user_find( irc, cmd[3] ) )
		{
			irc_usermsg( irc, "The requested nick `%s' already exists", cmd[3] );
			return( 0 );
		}
		else
		{
			nick_set( irc, cmd[2], gc->protocol, cmd[3] );
		}
	}
	gc->prpl->add_buddy( gc, cmd[2] );
	add_buddy( gc, NULL, cmd[2], cmd[2] );
	
	irc_usermsg( irc, "User `%s' added to your contact list as `%s'", cmd[2], user_findhandle( gc, cmd[2] )->nick );
	
	return( 0 );
}

int cmd_info( irc_t *irc, char **cmd )
{
	struct gaim_connection *gc;
	int i;
	
	if( !cmd[2] )
	{
		user_t *u = user_find( irc, cmd[1] );
		if( !u || !u->gc )
		{
			irc_usermsg( irc, "Nick `%s' does not exist", cmd[1] );
			return( 1 );
		}
		gc = u->gc;
		cmd[2] = u->handle;
	}
	else if( !cmd[1] || !sscanf( cmd[1], "%d", &i ) || !( gc = gc_nr( i ) ) )
	{
		irc_usermsg( irc, "Incorrect connection number" );
		return( 1 );
	}
	
	if( !gc->prpl->get_info )
	{
		irc_usermsg( irc, "info-command not implemented for this protocol" );
		return( 1 );
	}
	gc->prpl->get_info( gc, cmd[2] );
	
	return( 0 );
}

int cmd_rename( irc_t *irc, char **cmd)
{
	user_t *u;
	
	if( strcasecmp( cmd[1], irc->nick ) == 0 )
	{
		irc_usermsg( irc, "Nick `%s' can't be changed", cmd[1] );
		return( 1 );
	}
	if( user_find( irc, cmd[2] ) )
	{
		irc_usermsg( irc, "Nick `%s' already exists", cmd[2] );
		return( 1 );
	}
	if( !nick_ok( cmd[2] ) )
	{
		irc_usermsg( irc, "Nick `%s' is invalid", cmd[2] );
		return( 1 );
	}
	if( !( u = user_find( irc, cmd[1] ) ) )
	{
		irc_usermsg( irc, "Nick `%s' does not exist", cmd[1] );
		return( 1 );
	}
	free( u->nick );
	if( u->nick == u->user ) u->user = NULL;
	u->nick = strdup( cmd[2] );
	if( !u->user ) u->user = u->nick;
	irc_write( irc, ":%s!%s@%s NICK %s", cmd[1], u->user, u->host, cmd[2] );
	if( strcasecmp( cmd[1], irc->mynick ) == 0 )
	{
		free( irc->mynick );
		irc->mynick = strdup( cmd[2] );
	}
	else
	{
		nick_set( irc, u->handle, ((struct gaim_connection *)u->gc)->protocol, cmd[2] );
	}
	
	irc_usermsg( irc, "Nick successfully changed" );
	
	return( 0 );
}

int cmd_remove( irc_t *irc, char **cmd )
{
	user_t *u;
	char *s;
	
	if( !( u = user_find( irc, cmd[1] ) ) || !u->gc )
	{
		irc_usermsg( irc, "Buddy `%s' not found", cmd[1] );
		return( 1 );
	}
	s = strdup( u->handle );
	
	((struct gaim_connection *)u->gc)->prpl->remove_buddy( u->gc, u->handle, NULL );
	user_del( irc, cmd[1] );
	nick_del( irc, cmd[1] );
	
	irc_usermsg( irc, "Buddy `%s' (nick %s) removed from contact list", s, cmd[1] );
	free( s );
	
	return( 0 );
}

int cmd_block( irc_t *irc, char **cmd )
{
	struct gaim_connection *gc;
	int i;
	
	if( !cmd[2] )
	{
		user_t *u = user_find( irc, cmd[1] );
		if( !u || !u->gc )
		{
			irc_usermsg( irc, "Nick `%s' does not exist", cmd[1] );
			return( 1 );
		}
		gc = u->gc;
		cmd[2] = u->handle;
	}
	else if( !cmd[1] || !sscanf( cmd[1], "%d", &i ) || !( gc = gc_nr( i ) ) )
	{
		irc_usermsg( irc, "Incorrect connection number" );
		return( 1 );
	}
	if( !gc->prpl->add_deny || !gc->prpl->rem_permit )
	{
		irc_usermsg( irc, "Command `%s' not supported by this protocol", cmd[0] );
		return( 1 );
	}
	else
	{
		gc->prpl->rem_permit( gc, cmd[2] );
		gc->prpl->add_deny( gc, cmd[2] );
		
		irc_usermsg( irc, "Buddy `%s' moved from your permit- to your deny-list", cmd[2] );
	}
	
	return( 0 );
}

int cmd_allow( irc_t *irc, char **cmd )
{
	struct gaim_connection *gc;
	int i;
	
	if( !cmd[2] )
	{
		user_t *u = user_find( irc, cmd[1] );
		if( !u || !u->gc )
		{
			irc_usermsg( irc, "Nick `%s' does not exist", cmd[1] );
			return( 1 );
		}
		gc = u->gc;
		cmd[2] = u->handle;
	}
	else if( !cmd[1] || !sscanf( cmd[1], "%d", &i ) || !( gc = gc_nr( i ) ) )
	{
		irc_usermsg( irc, "Incorrect connection number" );
		return( 1 );
	}
	if( !gc->prpl->rem_deny || !gc->prpl->add_permit )
	{
		irc_usermsg( irc, "Command `%s' not supported by this protocol", cmd[0] );
		return( 1 );
	}
	else
	{
		gc->prpl->rem_deny( gc, cmd[2] );
		gc->prpl->add_permit( gc, cmd[2] );
		
		irc_usermsg( irc, "Buddy `%s' moved from your deny- to your permit-list", cmd[2] );
	}
	
	return( 0 );
}

int cmd_yesno( irc_t *irc, char **cmd )
{
	query_t *q = irc->queries, *prevq = q;
	int numq = 0;

	if( irc->queries == NULL )
	{
		irc_usermsg( irc, "Did I ask you something?" );
		return( 0 );
	}
	
	/* If no argument was given, leave numq at 0, otherwise read and check for validity */
	if( cmd[1] != NULL && sscanf( cmd[1], "%d", &numq ) != 1 )
	{
		irc_usermsg( irc, "Invalid query number" );
		return( 0 );
	}

	for( ; q != NULL; q = q->next, numq -- )
	{
		if( numq == 0)
		{
			if( strcasecmp( cmd[0], "yes" ) == 0 )
			{
				irc_usermsg( irc, "Accepting: %s", q->question );
				q->yes( NULL, q->data );
			}
			else if( strcasecmp( cmd[0], "no" ) == 0 )
			{
				irc_usermsg( irc, "Rejecting: %s", q->question );
				q->no( NULL, q->data );
			}
			
			/* If our question is first in the list, move the irc->queries pointer */
			if( q == irc->queries )
				irc->queries = q->next;

			prevq->next = q->next;

			free( q->question );
			free( q );
			return( 0 );
		}
		prevq = q;
	}
	
	irc_usermsg( irc, "Uhm, I never asked you something like that" );
	
	return( 0 );
}

int cmd_set( irc_t *irc, char **cmd )
{
	if( cmd[2] )
	{
		set_setstr( irc, cmd[1], cmd[2] );
	}
	if( cmd[1] ) /* else 'forgotten' on purpose.. Must show new value after changing */
	{
		char *s = set_getstr( irc, cmd[1] );
		if( s )
			irc_usermsg( irc, "%s = `%s'", cmd[1], s );
	}
	else
	{
		set_t *s = irc->set;
		while( s )
		{
			if( s->value || s->def )
				irc_usermsg( irc, "%s = `%s'", s->key, s->value?s->value:s->def );
			s = s->next;
		}
	}
	
	return( 0 );
}

int cmd_save( irc_t *irc, char **cmd )
{
	if( bitlbee_save( irc ) )
		irc_usermsg( irc, "Configuration saved" );
	else
		irc_usermsg( irc, "Configuration could not be saved!" );
	
	return( 0 );
}

int cmd_blist( irc_t *irc, char **cmd )
{
	int online = 0, away = 0, offline = 0;
	user_t *u;
	char s[64];
	int n_online = 0, n_away = 0, n_offline = 0;
	
	if( cmd[1] && strcasecmp( cmd[1], "all" ) == 0 )
		online = offline = away = 1;
	else if( cmd[1] && strcasecmp( cmd[1], "offline" ) == 0 )
		offline = 1;
	else if( cmd[1] && strcasecmp( cmd[1], "away" ) == 0 )
		away = 1;
	else if( cmd[1] && strcasecmp( cmd[1], "online" ) == 0 )
		online = 1;
	else
		online =  away = 1;
	
	irc_usermsg( irc, "%-16.16s  %-40.40s  %s", "Nick", "User/Host/Network", "Status" );
	
	if( online == 1 ) for( u = irc->users; u; u = u->next ) if( u->gc && u->online && !u->away )
	{
		snprintf( s, 63, "%s@%s (%s)", u->user, u->host, proto_name[u->gc->user->protocol] );
		irc_usermsg( irc, "%-16.16s  %-40.40s  %s", u->nick, s, "Online" );
		n_online ++;
	}

	if( away == 1 ) for( u = irc->users; u; u = u->next ) if( u->gc && u->online && u->away )
	{
		snprintf( s, 63, "%s@%s (%s)", u->user, u->host, proto_name[u->gc->user->protocol] );
		irc_usermsg( irc, "%-16.16s  %-40.40s  %s", u->nick, s, u->away );
		n_away ++;
	}
	
	if( offline == 1 ) for( u = irc->users; u; u = u->next ) if( u->gc && !u->online )
	{
		snprintf( s, 63, "%s@%s (%s)", u->user, u->host, proto_name[u->gc->user->protocol] );
		irc_usermsg( irc, "%-16.16s  %-40.40s  %s", u->nick, s, "Offline" );
		n_offline ++;
	}
	
	irc_usermsg( irc, "%d buddies (%d available, %d away, %d offline)", n_online + n_away + n_offline, n_online, n_away, n_offline );
	
	return( 0 );
}

int cmd_nick( irc_t *irc, char **cmd ) 
{
	int i;
	struct gaim_connection *gc;

	if ( !cmd[1] || !sscanf( cmd[1], "%d", &i ) || !( gc = gc_nr(i)) )
	{
		irc_usermsg( irc, "Incorrect connection number");
		return( 1 );
	}

	if ( !cmd[2] ) 
	{
		irc_usermsg( irc, "Your name is `%s'" , gc->displayname ? gc->displayname : "NULL" );
	}
	else if ( !gc->prpl->set_info ) 
	{
		irc_usermsg( irc, "Command `%s' not supported by this protocol", cmd[0] );
		return( 1 );
	}
	else
	{
		irc_usermsg( irc, "Setting your name on connection %d to `%s'", i, cmd[2] );
		gc->prpl->set_info( gc, cmd[2] );
	}
	
	return( 0 );
}

int cmd_qlist( irc_t *irc, char **cmd )
{
	query_t *q = irc->queries;
	int num;
	
	if( !q )
	{
		irc_usermsg( irc, "There are no pending questions." );
		return( 0 );
	}
	
	irc_usermsg( irc, "Pending queries:" );
	
	for( num = 0; q; q = q->next, num ++ )
		irc_usermsg( irc, "%d, %s", num, q->question );
	
	return( 0 );
}
