#include "bitlbee.h"

set_t *set_add( irc_t *irc, char *key, char *def, void *eval )
{
	set_t *s = set_find( irc, key );
	
	if( !s )
	{
		if( s = irc->set )
		{
			while( s->next ) s = s->next;
			s->next = malloc( sizeof( set_t ) );
			s = s->next;
		}
		else
		{
			s = irc->set = malloc( sizeof( set_t ) );
		}
		memset( s, 0, sizeof( set_t ) );
		s->key = strdup( key );
	}
	
	if( s->def )
	{
		free( s->def );
		s->def = NULL;
	}
	if( def ) s->def = strdup( def );
	
	if( s->eval )
	{
		free( s->eval );
		s->eval = NULL;
	}
	if( eval ) s->eval = eval;
	
	return( s );
}

set_t *set_find( irc_t *irc, char *key )
{
	set_t *s = irc->set;
	
	while( s )
	{
		if( strcasecmp( s->key, key ) == 0 )
			break;
		s = s->next;
	}
	
	return( s );
}

char *set_getstr( irc_t *irc, char *key )
{
	set_t *s = set_find( irc, key );
	
	if( !s || ( !s->value && !s->def ) )
		return( NULL );
	
	return( s->value?s->value:s->def );
}

int set_getint( irc_t *irc, char *key )
{
	char *s = set_getstr( irc, key );
	int i;
	
	if( !s )
		return( 0 );
	
	if( ( strcasecmp( s, "true" ) == 0 ) || ( strcasecmp( s, "yes" ) == 0 ) || ( strcasecmp( s, "on" ) == 0 ) )
		return( 1 );
	
	if( sscanf( s, "%d", &i ) != 1 )
		return( 0 );
	
	return( i );
}

int set_setstr( irc_t *irc, char *key, char *value )
{
	set_t *s = set_find( irc, key );
	char *nv = value;
	
	if( !s )
		s = set_add( irc, key, NULL, NULL );
	
	if( s->eval && !( nv = s->eval( irc, s, value ) ) )
		return( 0 );
	
	if( s->value )
	{
		free( s->value );
		s->value = NULL;
	}
	
	if( !s->def || ( strcmp( value, s->def ) != 0 ) )
		s->value = strdup( value );
	
	if( nv != value )
		free( nv );
	
	return( 1 );
}

int set_setint( irc_t *irc, char *key, int value )
{
	char s[24];	/* Not quite 128-bit clean eh? ;-) */
	
	sprintf( s, "%d", value );
	return( set_setstr( irc, key, s ) );
}

void set_del( irc_t *irc, char *key )
{
	set_t *s = irc->set, *t = NULL;
	
	while( s )
	{
		if( strcasecmp( s->key, key ) == 0 )
			break;
		s = (t=s)->next;
	}
	if( s )
	{
		t->next = s->next;
		free( s->key );
		if( s->value ) free( s->value );
		if( s->def ) free( s->def );
		free( s );
	}
}

char *set_eval_int( irc_t *irc, set_t *set, char *value )
{
	char *s = value;
	
	for( ; *s; s ++ )
		if( *s < '0' || *s > '9' )
			return( NULL );
	
	return( value );
}

char *set_eval_bool( irc_t *irc, set_t *set, char *value )
{
	if( ( strcasecmp( value, "true" ) == 0 ) || ( strcasecmp( value, "yes" ) == 0 ) || ( strcasecmp( value, "on" ) == 0 ) )
		return( value );
	if( ( strcasecmp( value, "false" ) == 0 ) || ( strcasecmp( value, "no" ) == 0 ) || ( strcasecmp( value, "off" ) == 0 ) )
		return( value );
	return( set_eval_int( irc, set, value ) );
}
