#include "bitlbee.h"
#include "help.h"

#define BUFSIZE 1024

help_t *help_init( irc_t *irc )
{
	int i, buflen = 0;
	help_t *h; //, *h2;
	char *s, *t;
	time_t mtime;
	struct stat stat[1];
	
	irc->help = h = malloc( sizeof( help_t ) );
	memset( h, 0, sizeof( help_t ) );
	h->fd = open( HELP_FILE, O_RDONLY );
	
	if( h->fd == -1 )
	{
		free( h );
		return( irc->help = NULL );
	}
	
	if( fstat( h->fd, stat ) != 0 )
	{
		free( h );
		return( irc->help = NULL );
	}
	mtime = stat->st_mtime > stat->st_ctime ? stat->st_mtime : stat->st_ctime;
	
	s = malloc( BUFSIZE + 1 );
	s[BUFSIZE] = 0;
	
	while( ( ( i = read( h->fd, s + buflen, BUFSIZE - buflen ) ) > 0 ) ||
	       ( i == 0 && strstr( s, "\n%\n" ) ) )
	{
		buflen += i;
		memset( s + buflen, 0, BUFSIZE - buflen );
		if( !( t = strstr( s, "\n%\n" ) ) || s[0] != '?' )
		{
			/* FIXME: Clean up */
//			help_close( irc->help );
			irc->help = NULL;
			free( s );
			return( NULL );
		}
		i = strchr( s, '\n' ) - s;
		
		if( h->string )
		{
			h = h->next = malloc( sizeof( help_t ) );
			memset( h, 0, sizeof( help_t ) );
		}
		h->string = malloc( i );
		
		strncpy( h->string, s + 1, i - 1 );
		h->string[i-1] = 0;
		h->fd = ((help_t *)irc->help)->fd;
		h->offset.file_offset = lseek( h->fd, 0, SEEK_CUR ) - buflen + i + 1;
		h->length = t - s - i - 1;
		h->mtime = mtime;
		
		buflen -= ( t + 3 - s );
		t = strdup( t + 3 );
		free( s );
		s = realloc( t, BUFSIZE + 1 );
		s[BUFSIZE] = 0;
	}
	
	free( s );
	
	return( irc->help );
}

char *help_get( irc_t *irc, char *string )
{
	help_t *h;
	time_t mtime;
	struct stat stat[1];
	
	h = irc->help;
	while( h )
	{
		if( strcasecmp( h->string, string ) == 0 ) break;
		h = h->next;
	}
	if( h )
	{
		char *s = malloc( h->length + 1 );
		
		if( fstat( h->fd, stat ) != 0 )
		{
			free( h );
			return( irc->help = NULL );
		}
		mtime = stat->st_mtime > stat->st_ctime ? 
		        stat->st_mtime : stat->st_ctime ;
		
		if( mtime > h->mtime )
			return( NULL );
//			return( strdup( "Help file changed during this session. Please restart to get help back." ) );
		
		s[h->length] = 0;
		if( h->fd > 0 )
		{
			lseek( h->fd, h->offset.file_offset, SEEK_SET );
			read( h->fd, s, h->length );
		}
		else
		{
			strncpy( s, h->offset.mem_offset, h->length );
		}
		return( s );
	}
	
	return( NULL );
}
