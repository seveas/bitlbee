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
	user_t *u;
	
	u = irc->users;
	while( u )
	{
		if( strcasecmp( u->nick, nick ) == 0 )
			break;
		u = u->next;
	}
	
	return( u );
}
