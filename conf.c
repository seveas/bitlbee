  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2004 Wilmer van der Gaast and others                *
  \********************************************************************/

/* Configuration reading code						*/

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "conf.h"
#include "ini.h"

static int conf_loadini( conf_t *conf, char *file );

conf_t *conf_load( int argc, char *argv[] )
{
	conf_t *conf;
	int opt, i;
	
	conf = bitlbee_alloc( sizeof( conf_t ) );
	memset( conf, 0, sizeof( conf_t ) );
	
	conf->interface = "0.0.0.0";
	conf->port = 6667;
	conf->nofork = 0;
	conf->verbose = 0;
	conf->runmode = RUNMODE_INETD;
	conf->authmode = OPEN;
	conf->password = NULL;
	conf->configdir = strdup( CONFIG );
	conf->motdfile = strdup( ETCDIR "/motd.txt" );
	conf->ping_interval = 180;
	conf->ping_timeout = 300;
	
	conf_loadini( conf, CONF_FILE );
	
	while( ( opt = getopt( argc, argv, "i:p:nvIDc:d:h" ) ) >= 0 )
	{
		if( opt == 'i' )
		{
			conf->interface = strdup( optarg );
		}
		else if( opt == 'p' )
		{
			if( ( sscanf( optarg, "%d", &i ) != 1 ) || ( i <= 0 ) || ( i > 65535 ) )
			{
				fprintf( stderr, "Invalid port number: %s\n", optarg );
				return( NULL );
			}
			conf->port = i;
		}
		else if( opt == 'n' )
			conf->nofork=1;
		else if( opt == 'v' )
			conf->verbose=1;
		else if( opt == 'I' )
			conf->runmode=RUNMODE_INETD;
		else if( opt == 'D' )
			conf->runmode=RUNMODE_DAEMON;
		else if( opt == 'c' )
		{
			if( strcmp( CONF_FILE, optarg ) != 0 )
			{
				free( CONF_FILE );
				CONF_FILE = strdup( optarg );
				free( conf );
				return( conf_load( argc, argv ) );
			}
		}
		else if( opt == 'd' )
		{
			free( conf->configdir );
			conf->configdir = strdup( optarg );
		}
		else if( opt == 'h' )
		{
			printf( "Usage: bitlbee [-D [-i <interface>] [-p <port>] [-n] [-v]] [-I]\n"
			        "               [-c <file>] [-d <dir>] [-h]\n"
			        "\n"
			        "An IRC-to-other-chat-networks gateway\n"
			        "\n"
			        "  -I  Classic/InetD mode. (Default)\n"
			        "  -D  Daemon mode. (Experimental!)\n"
			        "  -i  Specify the interface (by IP address) to listen on.\n"
			        "      (Default: 0.0.0.0 (any interface))\n"
			        "  -p  Port number to listen on. (Default: 6667)\n"
			        "  -n  Don't fork.\n"
			        "  -v  Be verbose (only works in combination with -n)\n"
			        "  -c  Load alternative configuration file\n"
			        "  -d  Specify alternative user configuration directory\n"
			        "  -h  Show this help page.\n" );
			return( NULL );
		}
	}
	
	if( conf->configdir[strlen(conf->configdir)-1] != '/' )
	{
		char *s = malloc( strlen( conf->configdir ) + 2 );
		
		sprintf( s, "%s/", conf->configdir );
		free( conf->configdir );
		conf->configdir = s;
	}
	
	return( conf );
}

static int conf_loadini( conf_t *conf, char *file )
{
	ini_t *ini;
	int i;
	
	ini = ini_open( file );
	if( ini == NULL ) return( 0 );
	while( ini_read( ini ) )
	{
		if( strcasecmp( ini->section, "settings" ) == 0 )
		{
			if( strcasecmp( ini->key, "runmode" ) == 0 )
			{
				if( strcasecmp( ini->value, "daemon" ) == 0 )
					conf->runmode = RUNMODE_DAEMON;
				else
					conf->runmode = RUNMODE_INETD;
			}
			else if( strcasecmp( ini->key, "daemoninterface" ) == 0 )
			{
				conf->interface = strdup( ini->value );
			}
			else if( strcasecmp( ini->key, "daemonport" ) == 0 )
			{
				if( ( sscanf( ini->value, "%d", &i ) != 1 ) || ( i <= 0 ) || ( i > 65535 ) )
				{
					fprintf( stderr, "Invalid port number: %s\n", ini->value );
					return( 0 );
				}
				conf->port = i;
			}
			else if( strcasecmp( ini->key, "authmode" ) == 0 )
			{
				if( strcasecmp( ini->value, "registered" ) == 0 )
					conf->authmode = REGISTERED;
				else if( strcasecmp( ini->value, "closed" ) == 0 )
					conf->authmode = CLOSED;
				else
					conf->authmode = OPEN;
			}
			else if( strcasecmp( ini->key, "authpassword" ) == 0 )
			{
				conf->password = strdup( ini->value );
			}
			else if( strcasecmp( ini->key, "hostname" ) == 0 )
			{
				conf->hostname = strdup( ini->value );
			}
			else if( strcasecmp( ini->key, "configdir" ) == 0 )
			{
				free( conf->configdir );
				conf->configdir = strdup( ini->value );
			}
			else if( strcasecmp( ini->key, "motdfile" ) == 0 )
			{
				free( conf->motdfile );
				conf->motdfile = strdup( ini->value );
			}
			else if( strcasecmp( ini->key, "pinginterval" ) == 0 )
			{
				if( sscanf( ini->value, "%d", &i ) != 1 )
				{
					fprintf( stderr, "Invalid PingInterval value: %s\n", ini->value );
					return( 0 );
				}
				conf->ping_interval = i;
			}
			else if( strcasecmp( ini->key, "pingtimeout" ) == 0 )
			{
				if( sscanf( ini->value, "%d", &i ) != 1 )
				{
					fprintf( stderr, "Invalid PingTimeOut value: %s\n", ini->value );
					return( 0 );
				}
				conf->ping_timeout = i;
			}
			else
			{
				/* For now just ignore unknown keys... */
			}
		}
		else /* Ignore the other sections for now */ ;
	}
	ini_close( ini );
	
	return( 1 );
}

void conf_loaddefaults( irc_t *irc )
{
	ini_t *ini;
	
	ini = ini_open( CONF_FILE );
	if( ini == NULL ) return;
	while( ini_read( ini ) )
	{
		if( strcasecmp( ini->section, "defaults" ) == 0 )
		{
			set_t *s = set_find( irc, ini->key );
			
			if( s )
			{
				if( s->def ) free( s->def );
				s->def = strdup( ini->value );
			}
		}
	}
	ini_close( ini );
}
