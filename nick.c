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
	char *s;
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
		snprintf( nick, MAX_NICK_LENGTH, "%s", handle );
		if( ( s = strchr( nick, ' ' ) ) ) *s = 0;
		if( ( s = strchr( nick, '@' ) ) ) *s = 0;
		nick_strip( nick );
	}
	
	if( !nick_ok( nick ) ) strcpy( nick, "_" );
	
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

static char *nick_lc_chars = "0123456789abcdefghijklmnopqrstuvwxyz{}|^_";
static char *nick_uc_chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ[]\\~_";

void nick_strip( char * nick )
{
	int i,j = 0;
	for(i = 0; nick[i]; i++)
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
	
	return( strcasecmp( a, b ) );
	
	aa = strdup( a );
	bb = strdup( b );
	if( nick_lc( aa ) && nick_lc( bb ) )
	{
		res = strcmp( a, b );
	}
	else
	{
		res = -1;	/* Hmm... Not a clear answer.. :-/ */
	}
	free( aa );
	free( bb );
	
	return( res );
}
