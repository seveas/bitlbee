  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2003 Wilmer van der Gaast and others                *
  \********************************************************************/

/*
 * nogaim
 *
 * Gaim without gaim - for BitlBee
 *
 * This file contains functions called by the Gaim IM-modules. It's written
 * from scratch for BitlBee and doesn't contain any code from Gaim anymore
 * (except for the function names).
 *
 * Copyright 2002-2003 Wilmer van der Gaast <lintux@lintux.cx>
 */

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

#include "nogaim.h"

struct prpl *proto_prpl[PROTO_MAX];
char proto_name[PROTO_MAX][8] = { "TOC", "OSCAR", "YAHOO", "ICQ", "MSN", "", "", "", "JABBER", "", "", "", "", "", "", "" };

static char *proto_away_alias[7][5] =
{
	{ "Away from computer", "Away", "Extended away", NULL },
	{ "NA", "N/A", "Not available", NULL },
	{ "Busy", "Do not disturb", "DND", "Occupied", NULL },
	{ "Be right back", "BRB", NULL },
	{ "On the phone", "Phone", "On phone", NULL },
	{ "Out to lunch", "Lunch", "Food", NULL },
	{ NULL }
};
static char *proto_away_alias_find( GList *gcm, char *away );

static char *set_eval_away_devoice( irc_t *irc, set_t *set, char *value );

static int remove_chat_buddy_silent( struct conversation *b, char *handle );

GSList *connections;


/* nogaim.c */

void nogaim_init()
{
	proto_prpl[PROTO_MSN] = malloc( sizeof( struct prpl ) );
	memset( proto_prpl[PROTO_MSN], 0, sizeof( struct prpl ) );
#ifdef WITH_MSN
	msn_init( proto_prpl[PROTO_MSN] );
#endif

	proto_prpl[PROTO_OSCAR] = malloc( sizeof( struct prpl ) );
	memset( proto_prpl[PROTO_OSCAR], 0, sizeof( struct prpl ) );
#ifdef WITH_OSCAR
	oscar_init( proto_prpl[PROTO_OSCAR] );
#endif
	
	proto_prpl[PROTO_YAHOO] = malloc( sizeof( struct prpl ) );
	memset( proto_prpl[PROTO_YAHOO], 0, sizeof( struct prpl ) );
#ifdef WITH_YAHOO
	yahoo_init( proto_prpl[PROTO_YAHOO] );
#endif
	
	proto_prpl[PROTO_JABBER] = malloc( sizeof( struct prpl ) );
	memset( proto_prpl[PROTO_JABBER], 0, sizeof( struct prpl ) );
#ifdef WITH_JABBER
	jabber_init( proto_prpl[PROTO_JABBER] );
#endif
	
	set_add( IRC, "html", "nostrip", NULL );
	set_add( IRC, "typing_notice", "false", set_eval_bool );
	set_add( IRC, "away_devoice", "true", set_eval_away_devoice );
#ifdef ICONV
	set_add( IRC, "charset", "none", set_eval_charset );
#endif
	set_add( IRC, "handle_unknown", "root", NULL );
	set_add( IRC, "auto_reconnect", "false", set_eval_bool );
	set_add( IRC, "reconnect_delay", "300", set_eval_int );
}

struct gaim_connection *gc_nr( int i )
{
	account_t *a;
	
	for( a = IRC->accounts; a; a = a->next )
		if( ( i-- ) == 0 )
			return( a->gc );
	
	return( NULL );
}

int proto_away( struct gaim_connection *gc, char *away )
{
	GList *m, *ms;
	char *s;
	
	if( !away ) away = "";
	ms = m = gc->prpl->away_states( gc );
	
	while( m )
	{
		if( *away )
		{
			if( strcasecmp( m->data, away ) == 0 )
				break;
		}
		else
		{
			if( strcasecmp( m->data, "Available" ) == 0 )
				break;
			if( strcasecmp( m->data, "Online" ) == 0 )
				break;
		}
		m = m->next;
	}
	
	if( m )
	{
		gc->prpl->set_away( gc, m->data, *away ? away : NULL );
	}
	else
	{
		s = proto_away_alias_find( ms, away );
		if( s )
		{
			gc->prpl->set_away( gc, s, away );
			if( set_getint( gc->irc, "debug" ) )
				irc_usermsg( gc->irc, "Setting away state for %s to %s", proto_name[gc->protocol], s );
		}
		else
			gc->prpl->set_away( gc, GAIM_AWAY_CUSTOM, away );
	}
	
	g_list_free( ms );
	
	return( 1 );
}

static char *proto_away_alias_find( GList *gcm, char *away )
{
	GList *m;
	int i, j;
	
	for( i = 0; *proto_away_alias[i]; i ++ )
	{
		for( j = 0; proto_away_alias[i][j]; j ++ )
			if( strncasecmp( away, proto_away_alias[i][j], strlen( proto_away_alias[i][j] ) ) == 0 )
				break;
		
		if( !proto_away_alias[i][j] )	/* If we reach the end, this row */
			continue;		/* is not what we want. Next!    */
		
		/* Now find an entry in this row which exists in gcm */
		for( j = 0; proto_away_alias[i][j]; j ++ )
		{
			m = gcm;
			while( m )
			{
				if( strcasecmp( proto_away_alias[i][j], m->data ) == 0 )
					return( proto_away_alias[i][j] );
				m = m->next;
			}
		}
	}
	
	return( NULL );
}


/* multi.c */

struct gaim_connection *new_gaim_conn( struct aim_user *user )
{
	struct gaim_connection *gc;
	account_t *a;
	
	gc = malloc( sizeof( struct gaim_connection ) );
	memset( gc, 0, sizeof( struct gaim_connection ) );
	
	gc->protocol = user->protocol;
	gc->prpl = proto_prpl[gc->protocol];
	g_snprintf( gc->username, sizeof( gc->username ), "%s", user->username );
	g_snprintf( gc->password, sizeof( gc->password ), "%s", user->password );
	gc->inpa = 0;
	gc->permit = NULL;
	gc->deny = NULL;
	gc->irc = IRC;

	connections = g_slist_append(connections, gc);

	user->gc = gc;
	gc->user = user;
	
	// Find the account_t so we can set it's gc pointer
	for( a = gc->irc->accounts; a; a = a->next )
		if( ( struct aim_user * ) a->gc == user )
		{
			a->gc = gc;
			break;
		}

	return( gc );
}

void destroy_gaim_conn( struct gaim_connection *gc )
{
	account_t *a;
	
	/* Destroy the pointer to this connection from the account list */
	for( a = gc->irc->accounts; a; a = a->next )
		if( a->gc == gc )
		{
			a->gc = NULL;
			break;
		}
	
	connections = g_slist_remove( connections, gc );
	g_free( gc->user );
	g_free( gc );
}

void set_login_progress( struct gaim_connection *gc, int step, char *msg )
{
	irc_usermsg( gc->irc, "%s - Logging in: %s", proto_name[gc->protocol], msg );
}

/* Errors *while* logging in */
void hide_login_progress( struct gaim_connection *gc, char *msg )
{
	irc_usermsg( gc->irc, "%s - Login error: %s", proto_name[gc->protocol], msg );
}

/* Errors *after* logging in */
void hide_login_progress_error( struct gaim_connection *gc, char *msg )
{
	irc_usermsg( gc->irc, "%s - Logged out: %s", proto_name[gc->protocol], msg );
}

static gboolean send_keepalive( gpointer d )
{
	struct gaim_connection *gc = d;
	
	if( gc->prpl && gc->prpl->keepalive )
		gc->prpl->keepalive( gc );
	
	return TRUE;
}

void account_online( struct gaim_connection *gc )
{
	nick_t *n = gc->irc->nicks;
	user_t *u = user_find( gc->irc, gc->irc->nick );
	
	irc_usermsg( gc->irc, "%s - Logged in", proto_name[gc->protocol] );
	
	gc->keepalive = g_timeout_add( 60000, send_keepalive, gc );
	gc->flags |= OPT_LOGGED_IN;
	
	if( gc->protocol == PROTO_OSCAR || gc->protocol == PROTO_ICQ || gc->protocol == PROTO_TOC )
		while( n )
		{
			if( n->proto == gc->protocol )
			{
				gc->prpl->add_buddy( gc, n->handle );
				add_buddy( gc, NULL, n->handle, NULL );
			}
			n = n->next;
		}
	
	if( u && u->away ) proto_away( gc, u->away );
}

gboolean auto_reconnect( gpointer data )
{
	reconnect_t *r = data;
	account_t *a;
	
	if( ( a = r->account ) )
	{
		a->reconnect = NULL;
		account_on( (irc_t *) NULL, a );
	}
	free( r );
	
	return( FALSE );	/* Only have to run the timeout once */
}

void account_offline( struct gaim_connection *gc )
{
	gc->wants_to_die = TRUE;
	signoff( gc );
}

void signoff( struct gaim_connection *gc )
{
	irc_t *irc = gc->irc;
	user_t *t, *u = irc->users;
	account_t *a;
	
	irc_usermsg( gc->irc, "%s - Signing off..", proto_name[gc->protocol] );

	gaim_input_remove( gc->keepalive );
	gc->keepalive = 0;
	gc->prpl->close( gc );
	gaim_input_remove( gc->inpa );
	
	while( u )
	{
		if( u->gc == gc )
		{
			t = u->next;
			user_del( irc, u->nick );
			u = t;
		}
		else
			u = u->next;
	}
	
	for( a = irc->accounts; a; a = a->next )
		if( a->gc == gc )
			break;
	
	if( !a )
	{
		/* Uhm... This is very sick. */
	}
	else if( !gc->wants_to_die && set_getint( irc, "auto_reconnect" ) )
	{
		int delay = set_getint( irc, "reconnect_delay" );
		irc_usermsg( gc->irc, "%s - Reconnecting in %d seconds..", proto_name[gc->protocol], delay);
		a->reconnect = malloc( sizeof( reconnect_t ) );
		a->reconnect->account = a;
		g_timeout_add( delay * 1000, auto_reconnect, a->reconnect );
	}
	else
	{
		a->reconnect = NULL;
	}
	
	destroy_gaim_conn( gc );
}


/* dialogs.c */

void do_error_dialog( char *msg, char *title )
{
	irc_usermsg( IRC, "%s - Error: %s", title, msg );
}

void do_ask_dialog( char *msg, void *data, void *doit, void *dont )
{
	query_t *q = IRC->queries;
	
	if( q )
	{
		while( q->next ) q = q->next;
		q = q->next = malloc( sizeof( query_t ) );
	}
	else
	{
		IRC->queries = q = malloc( sizeof( query_t ) );
	}
	memset( q, 0, sizeof( query_t ) );
	
	q->question = strdup( msg );
	q->yes = doit;
	q->no = dont;
	q->data = data;
	
	if( q == IRC->queries )
	{
		irc_usermsg( IRC, "%s", q->question );
		irc_usermsg( IRC, "Type yes to accept or no to reject" );
	}
}


/* list.c */

int bud_list_cache_exists( struct gaim_connection *gc )
{
	return( 0 );
}

void do_import( struct gaim_connection *gc, void *null )
{
	return;
}

void add_buddy( struct gaim_connection *gc, char *group, char *handle, char *realname )
{
	user_t *u;
	char nick[MAX_NICK_LENGTH+1];
	char *s;
	irc_t *irc = gc->irc;
	
	if( set_getint( irc, "debug" ) )
		irc_usermsg( irc, "Receiving user add from protocol: %s", handle );
	
	if( user_findhandle( gc, handle ) )
	{
		if( set_getint( irc, "debug" ) )
			irc_usermsg( irc, "User already exists, ignoring add request: %s", handle );
		
		return;
		
		/* Buddy seems to exist already. Let's ignore this request then... */
	}
	
	memset( nick, 0, MAX_NICK_LENGTH + 1 );
	strcpy( nick, nick_get( gc->irc, handle, gc->protocol ) );
	
	u = user_add( gc->irc, nick );
	
	if( !realname || !*realname ) realname = nick;
	u->realname = strdup( realname );
	
	if( ( s = strchr( handle, '@' ) ) )
	{
		s ++;
		u->host = strdup( s );
		
		*(s-1) = 0;
		u->user = strdup( handle );
		*(s-1) = '@';
	}
	else if( gc->user->proto_opt[0] && *gc->user->proto_opt[0] )
	{
		u->host = strdup( gc->user->proto_opt[0] );
		u->user = strdup( handle );
		
		/* s/ /_/ ... important for AOL screennames */
		for( s = u->user; *s; s ++ )
			if( *s == ' ' )
				*s = '_';
	}
	else
	{
		u->host = strdup( proto_name[gc->user->protocol] );
		u->user = strdup( handle );
	}
	
	u->gc = gc;
	u->handle = strdup( handle );
	u->send_handler = buddy_send_handler;
}

struct buddy *find_buddy( struct gaim_connection *gc, char *handle )
{
	static struct buddy b[1];
	user_t *u;
	
	u = user_findhandle( gc, handle );
	
	if( !u )
		return( NULL );

	memset( b, 0, sizeof( b ) );
	strncpy( b->name, handle, 80 );
	strncpy( b->show, u->realname, BUDDY_ALIAS_MAXLEN );
	b->present = u->online;
	b->gc = u->gc;
	
	return( b );
}

void do_export( struct gaim_connection *gc )
{
	return;
}

void signoff_blocked( struct gaim_connection *gc )
{
	return; /* Make all blocked users look invisible (TODO?) */
}


/* buddy.c */

void handle_buddy_rename( struct buddy *buddy, char *handle )
{
	user_t *u = user_findhandle( buddy->gc, handle );
	
	if( !u ) return;
	
	if( u->realname != u->nick ) free( u->realname );
	u->realname = strdup( buddy->show );
}


/* prpl.c */

void show_got_added( struct gaim_connection *gc, char *id, char *handle, const char *realname, const char *msg )
{
	return;
}


/* server.c */                    

void serv_got_update( struct gaim_connection *gc, char *handle, int loggedin, int evil, time_t signon, time_t idle, int type, guint caps )
{
	user_t *u;
	int oa, oo;
	
	u = user_findhandle( gc, handle );
	
	if( !u )
	{
		if( strcasecmp( set_getstr( gc->irc, "handle_unknown" ), "add" ) == 0 )
		{
			add_buddy( gc, NULL, handle, NULL );
			u = user_findhandle( gc, handle );
		}
		else if( strcasecmp( set_getstr( gc->irc, "handle_unknown" ), "ignore" ) == 0 )
		{
			if( set_getint( gc->irc, "debug" ) || strcasecmp( set_getstr( gc->irc, "handle_unknown" ), "ignore" ) != 0 )
			{
				irc_usermsg( gc->irc, "serv_got_update() for unknown %s handle %s:", proto_name[gc->protocol], handle );
				irc_usermsg( gc->irc, "loggedin = %d, type = %d", loggedin, type );
			}
			
			return;
		}
		return;
	}
	
	oa = u->away != NULL;
	oo = u->online;
	
	if( loggedin && !u->online )
	{
		irc_spawn( gc->irc, u );
		u->online = 1;
	}
	else if( !loggedin && u->online )
	{
		struct conversation *c;
		
		irc_kill( gc->irc, u );
		u->online = 0;
		u->away = NULL;
		
		/* Remove him/her from the conversations to prevent PART messages after he/she QUIT already */
		for( c = gc->conversations; c; c = c->next )
			remove_chat_buddy_silent( c, handle );
	}
	
	if( ( type & UC_UNAVAILABLE ) && ( gc->protocol == PROTO_MSN ) )
	{
		if( ( type & 30 ) == ( MSN_BUSY << 1 ) )
			u->away = "Busy";
		else if( ( type & 30 ) == ( MSN_IDLE << 1 ) )
			u->away = "Idle";
		else if( ( type & 30 ) == ( MSN_BRB << 1 ) )
			u->away = "Be right back";
		else if( ( type & 30 ) == ( MSN_PHONE << 1 ) )
			u->away = "On the phone";
		else if( ( type & 30 ) == ( MSN_LUNCH << 1 ) )
			u->away = "Out to lunch";
		else // if( ( type & 30 ) == ( MSN_AWAY << 1 ) )
			u->away = "Away from the computer";
	}
	else if( ( type & UC_UNAVAILABLE ) && ( gc->protocol == PROTO_OSCAR || gc->protocol == PROTO_ICQ || gc->protocol == PROTO_TOC ) )
	{
		u->away = "Away";
	}
	else if( ( type & UC_UNAVAILABLE ) && ( gc->protocol == PROTO_JABBER ) )
	{
		if( type & UC_DND )
			u->away = "Do Not Disturb";
		else if( type & UC_XA )
			u->away = "Extended Away";
		else // if( type & UC_AWAY )
			u->away = "Away";
	}
	else if( ( type & UC_UNAVAILABLE ) && ( gc->protocol == PROTO_YAHOO ) )
	{
		if( set_getint( gc->irc, "debug" ) )
			irc_usermsg( gc->irc, "Away-state for %s: %d", handle, type );
		u->away = "Away";
	}
	else
		u->away = NULL;
	
	/* LISPy... */
	if( ( set_getint( gc->irc, "away_devoice" ) ) &&		/* Don't do a thing when user doesn't want it */
	    ( u->online ) &&						/* Don't touch offline people */
	    ( ( ( u->online != oo ) && !u->away ) ||			/* Voice joining people */
	      ( ( u->online == oo ) && ( oa == !u->away ) ) ) )		/* (De)voice people changing state */
	{
		irc_write( gc->irc, ":%s!%s@%s MODE %s %cv %s", gc->irc->mynick, gc->irc->mynick, gc->irc->myhost,
		                                                gc->irc->channel, u->away?'-':'+', u->nick );
	}
}

void serv_got_im( struct gaim_connection *gc, char *handle, char *msg, guint32 flags, time_t mtime, gint len )
{
	irc_t *irc = gc->irc;
	user_t *u;
#ifdef ICONV
	char buf[8192];
#endif
	
	u = user_findhandle( gc, handle );
	
	if( !u )
	{
		if( strcasecmp( set_getstr( irc, "handle_unknown" ), "ignore" ) == 0 )
		{
			if( set_getint( irc, "debug" ) )
				irc_usermsg( irc, "Ignoring message from unknown %s handle %s", proto_name[gc->protocol], handle );
			
			return;
		}
		else if( strcasecmp( set_getstr( irc, "handle_unknown" ), "add" ) == 0 )
		{
			add_buddy( gc, NULL, handle, NULL );
			u = user_findhandle( gc, handle );
		}
		else
		{
			irc_usermsg( irc, "Message from unknown %s handle %s:", proto_name[gc->protocol], handle );
			u = user_find( irc, irc->mynick );
		}
	}
	
	if( strcasecmp( set_getstr( gc->irc, "html" ), "strip" ) == 0 )
		strip_html( msg );

#ifdef ICONV
	if (strncasecmp(set_getstr(irc, "charset"), "none", 4)
	    && do_iconv(set_getstr(irc, "charset"), "UTF-8", msg, buf, 8192) != -1)
		msg = buf;
#endif
	
	irc_msgfrom( irc, u->nick, msg );
}

void serv_got_typing( struct gaim_connection *gc, char *handle, int timeout )
{
	user_t *u;
	
	if( !set_getint( gc->irc, "typing_notice" ) )
		return;
	
	if( ( u = user_findhandle( gc, handle ) ) )
		irc_noticefrom( gc->irc, u->nick, "* Typing a message *" );
}

void serv_got_chat_left( struct gaim_connection *gc, int id )
{
	struct conversation *c, *l = NULL;
	
	if( set_getint( gc->irc, "debug" ) )
		irc_usermsg( gc->irc, "You were removed from conversation %d", (int) id );
	
	for( c = gc->conversations; c && c->id != id; c = (l=c)->next );
	
	if( c )
	{
		if( c->joined )
		{
			user_t *u;
			
			u = user_find( gc->irc, gc->irc->mynick );
			irc_privmsg( gc->irc, u, "PRIVMSG", c->channel, "", "Cleaning up channel, bye!" );
			
			u = user_find( gc->irc, gc->irc->nick );
			irc_part( gc->irc, u, c->channel );
		}
		
		if( l )
			l->next = c->next;
		else
			gc->conversations = c->next;
		
		free( c->channel );
		free( c->title );
		g_list_free( c->in_room );
		free( c );
	}
}

void serv_got_chat_in( struct gaim_connection *gc, int id, char *who, int whisper, char *msg, time_t mtime )
{
	struct conversation *c;
	user_t *u;
#ifdef ICONV
	char buf[8192];
#endif
	
	/* Gaim sends own messages through this too. IRC doesn't want this, so kill them */
	if( strcasecmp( who, gc->user->username ) == 0 )
		return;
	
	u = user_findhandle( gc, who );
	for( c = gc->conversations; c && c->id != id; c = c->next );
	
#ifdef ICONV
	if (strncasecmp(set_getstr(gc->irc, "charset"), "none", 4)
	    && do_iconv(set_getstr(gc->irc, "charset"), "UTF-8", msg, buf, 8192) != -1)
		msg = buf;
#endif
	
	if( c && u )
		irc_privmsg( gc->irc, u, "PRIVMSG", c->channel, "", msg );
	else
		irc_usermsg( gc->irc, "Message from/to conversation %s@%d (unknown conv/user): %s", who, id, msg );
}

struct conversation *serv_got_joined_chat( struct gaim_connection *gc, int id, char *handle )
{
	struct conversation *c;
	char *s;
	
	/* This one just creates the conversation structure, user won't see anything yet */
	
	if( gc->conversations )
	{
		for( c = gc->conversations; c->next; c = c->next );
		c = c->next = malloc( sizeof( struct conversation ) );
	}
	else
		gc->conversations = c = malloc( sizeof( struct conversation ) );
	
	memset( c, 0, sizeof( struct conversation ) );
	c->id = id;
	c->gc = gc;
	c->title = strdup( handle );
	
	s = malloc( 16 );
	sprintf( s, "#chat_%03d", gc->irc->c_id++ );
	c->channel = strdup( s );
	free( s );
	
	if( set_getint( gc->irc, "debug" ) )
		irc_usermsg( gc->irc, "Creating new conversation: (id=%d,handle=%s)", id, handle );
	
	return( c );
}

void serv_finish_login( struct gaim_connection *gc )
{
	return;
}


/* buddy_chat.c */

void add_chat_buddy( struct conversation *b, char *handle )
{
	user_t *u = user_findhandle( b->gc, handle );
	int me = 0;
	
	if( set_getint( b->gc->irc, "debug" ) )
		irc_usermsg( b->gc->irc, "User %s added to conversation %d", handle, b->id );
	
	/* It might be yourself! */
	if( strcasecmp( handle, b->gc->user->username ) == 0 )
	{
		u = user_find( b->gc->irc, b->gc->irc->nick );
		b->joined = me = 1;
	}
	
	/* Most protocols allow people to join, even when they're not in
	   your contact list. Try to handle that here */
	if( !u )
	{
		add_buddy( b->gc, NULL, handle, NULL );
		u = user_findhandle( b->gc, handle );
	}
	
	/* Send the IRC message to the client */
	if( b->joined && u )
		irc_join( b->gc->irc, u, b->channel );
	
	/* Add the handle to the room userlist, if it's not 'me' */
	if( !me )
		b->in_room = g_list_append( b->in_room, strdup( handle ) );
}

void remove_chat_buddy( struct conversation *b, char *handle, char *reason )
{
	user_t *u;
	int me = 0;
	
	if( set_getint( b->gc->irc, "debug" ) )
		irc_usermsg( b->gc->irc, "User %s removed from conversation %d (%s)", handle, b->id, reason ? reason : "" );
	
	/* It might be yourself! */
	if( strcasecmp( handle, b->gc->user->username ) == 0 )
	{
		u = user_find( b->gc->irc, b->gc->irc->nick );
		b->joined = 0;
		me = 1;
	}
	else
	{
		u = user_findhandle( b->gc, handle );
	}
	
	if( remove_chat_buddy_silent( b, handle ) )
		if( ( b->joined || me ) && u )
			irc_part( b->gc->irc, u, b->channel );
}

static int remove_chat_buddy_silent( struct conversation *b, char *handle )
{
	GList *i;
	
	/* Find the handle in the room userlist and shoot it */
	i = b->in_room;
	while( i )
	{
		if( strcasecmp( handle, i->data ) == 0 )
		{
			b->in_room = g_list_remove( b->in_room, i->data );
			return( 1 );
		}
		
		i = i->next;
	}
	
	return( 0 );
}


/* prefs.c */

/* Necessary? */
void build_block_list()
{
	return;
}

void build_allow_list()
{
	return;
}


/* Misc. BitlBee stuff which shouldn't really be here */

struct conversation *conv_findchannel( char *channel )
{
	struct gaim_connection *gc;
	struct conversation *c;
	GSList *l;
	
	/* This finds the connection which has a conversation which belongs to this channel */
	for( l = connections; l; l = l->next )
	{
		gc = l->data;
		for( c = gc->conversations; c && strcasecmp( c->channel, channel ) != 0; c = c->next );
		if( c )
			return( c );
	}
	
	return( NULL );
}

static char *set_eval_away_devoice( irc_t *irc, set_t *set, char *value )
{
	int st;
	
	if( ( strcasecmp( value, "true" ) == 0 ) || ( strcasecmp( value, "yes" ) == 0 ) || ( strcasecmp( value, "on" ) == 0 ) )
		st = 1;
	else if( ( strcasecmp( value, "false" ) == 0 ) || ( strcasecmp( value, "no" ) == 0 ) || ( strcasecmp( value, "off" ) == 0 ) )
		st = 0;
	else if( sscanf( value, "%d", &st ) != 1 )
		return( NULL );
	
	st = st != 0;
	
	/* Horror.... */
	
	if( st != set_getint( irc, "away_devoice" ) )
	{
		char list[80] = "";
		user_t *u = irc->users;
		int i = 0, count = 0;
		char pm;
		char v[80];
		
		if( st )
			pm = '+';
		else
			pm = '-';
		
		while( u )
		{
			if( u->gc && u->online && !u->away )
			{
				if( ( strlen( list ) + strlen( u->nick ) ) >= 79 )
				{
					for( i = 0; i < count; v[i++] = 'v' ); v[i] = 0;
					irc_write( irc, ":%s!%s@%s MODE %s %c%s%s",
					           irc->mynick, irc->mynick, irc->myhost,
		        			   irc->channel, pm, v, list );
					
					*list = 0;
					count = 0;
				}
				
				sprintf( list + strlen( list ), " %s", u->nick );
				count ++;
			}
			u = u->next;
		}
		
		/* $v = 'v' x $i */
		for( i = 0; i < count; v[i++] = 'v' ); v[i] = 0;
		irc_write( irc, ":%s!%s@%s MODE %s %c%s%s", irc->mynick, irc->mynick, irc->myhost,
		                                            irc->channel, pm, v, list );
	}
	
	return( set_eval_bool( irc, set, value ) );
}

int serv_send_im(irc_t *irc, user_t *u, char *msg)
{
#ifdef ICONV
	char buf[8192];
	
	if (strncasecmp(set_getstr(irc, "charset"), "none", 4)
	    && do_iconv("UTF-8", set_getstr(irc, "charset"), msg, buf, 8192) != -1)
		msg = buf;
#endif
	
	return( ((struct gaim_connection *)u->gc)->prpl->send_im( u->gc, u->handle, msg, strlen( msg ), 0 ) );
}

int serv_send_chat(irc_t *irc, struct gaim_connection *gc, int id, char *msg )
{
#ifdef ICONV
	char buf[8192];
	
	if (strncasecmp(set_getstr(irc, "charset"), "none", 4)
	    && do_iconv("UTF-8", set_getstr(irc, "charset"), msg, buf, 8192) != -1)
		msg = buf;
#endif
	
	return( gc->prpl->chat_send( gc, id, msg ) );
}

#ifdef ICONV
int do_iconv(char *to, char *from, char *src, char *dst, size_t size)
{
	iconv_t cd;
	size_t res;
	size_t inbytesleft, outbytesleft;
	char *inbuf = src;
	char *outbuf = dst;

	cd = iconv_open(to, from);
	if (cd == (iconv_t)(-1))
		return -1;

	inbytesleft = strlen(src);
	outbytesleft = size - 1;
	res = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	*outbuf= '\0';
	iconv_close(cd);

	if (res == (size_t)(-1))
		return -1;

	return 0;
}

char *set_eval_charset(irc_t *irc, set_t *set, char *value)
{
	iconv_t cd;

	if (!strncasecmp(value, "none", 4))
		return value;

	cd = iconv_open("UTF-8", value);
	if (cd == (iconv_t)(-1))
		return NULL;

	iconv_close(cd);
	return value;
}
#endif
