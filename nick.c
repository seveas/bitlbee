#include "bitlbee.h"

void nick_set( irc_t *irc, char *handle, int proto, char *nick )
{
	nick_t *m = NULL, *n = irc->nicks;
	
	while( n )
	{
		if( ( strcasecmp( n->handle, handle ) == 0 ) && n->proto == proto )
		{
			free( n->nick );
			n->nick = malloc( strlen( nick ) + 1 );
			strcpy( n->nick, nick );
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
}

char *nick_get( irc_t *irc, char *handle, int proto )
{
	char *s;
	static char nick[MAX_NICK_LENGTH+1];
	nick_t *n = irc->nicks;
	
	memset( nick, 0, MAX_NICK_LENGTH + 1 );
	
	while( n && !*nick )
		if( ( strcasecmp( n->handle, handle ) == 0 ) && n->proto == proto )
			strcpy( nick, n->nick );
		else
			n = n->next;
	
	if( !n )
	{
		snprintf( nick, MAX_NICK_LENGTH, "%s", handle );
		if( s = strchr( nick, ' ' ) ) *s = 0;
		if( s = strchr( nick, '@' ) ) *s = 0;
	}
	
	if( !*nick ) *nick = '_';	// Shouldn't happen anyway, right?
	
	while( user_find( irc, nick ) )
		if( strlen( nick ) < MAX_NICK_LENGTH )
			nick[strlen(nick)] = '_';
		else
			nick[0] ++;
	
	return( nick );
}
