/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gaim
 *
 * Some code copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * libfaim code copyright 1998, 1999 Adam Fritzler <afritz@auk.cx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _WIN32
#include <sys/utsname.h>
#endif
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include "jabber.h"
#include "nogaim.h"
#include "bitlbee.h"
#include "proxy.h"

/* The priv member of gjconn's is a gaim_connection for now. */
#define GJ_GC(x) ((struct gaim_connection *)(x)->priv)

#define IQID_AUTH "__AUTH__"

#define IQ_NONE -1
#define IQ_AUTH 0
#define IQ_ROSTER 1

#define UC_AWAY (0x02 | UC_UNAVAILABLE)
#define UC_CHAT  0x04
#define UC_XA   (0x08 | UC_UNAVAILABLE)
#define UC_DND  (0x10 | UC_UNAVAILABLE)

#define DEFAULT_SERVER "jabber.org"
#define DEFAULT_GROUPCHAT "conference.jabber.org"
#define DEFAULT_PORT 5222

#define USEROPT_PORT 0

#define JABBER_GROUP "Friends"

/* i18n disabled - Bitlbee */
#define N_(String) String

/*
 * Note: "was_connected" may seem redundant, but it was needed and I
 * didn't want to touch the Jabber state stuff not specific to Gaim.
 */
typedef struct gjconn_struct {
	/* Core structure */
	pool p;			/* Memory allocation pool */
	int state;		/* Connection state flag */
	int was_connected;	/* We were once connected */
	int fd;			/* Connection file descriptor */
	jid user;		/* User info */
	char *pass;		/* User passwd */

	/* Stream stuff */
	int id;			/* id counter for jab_getid() function */
	char idbuf[9];		/* temporary storage for jab_getid() */
	char *sid;		/* stream id from server, for digest auth */
	XML_Parser parser;	/* Parser instance */
	xmlnode current;	/* Current node in parsing instance.. */

	/* Event callback ptrs */
	void (*on_state)(struct gjconn_struct *gjc, int state);
	void (*on_packet)(struct gjconn_struct *gjc, jpacket p);

	GHashTable *queries;	/* query tracker */

	void *priv;

} *gjconn, gjconn_struct;

typedef void (*gjconn_state_h)(gjconn gjc, int state);
typedef void (*gjconn_packet_h)(gjconn gjc, jpacket p);

static gjconn gjab_new(char *user, char *pass, void *priv);
static void gjab_delete(gjconn gjc);
static void gjab_state_handler(gjconn gjc, gjconn_state_h h);
static void gjab_packet_handler(gjconn gjc, gjconn_packet_h h);
static void gjab_start(gjconn gjc);
static void gjab_stop(gjconn gjc);
/*
static int gjab_getfd(gjconn gjc);
static jid gjab_getjid(gjconn gjc);
static char *gjab_getsid(gjconn gjc);
*/
static char *gjab_getid(gjconn gjc);
static void gjab_send(gjconn gjc, xmlnode x);
static void gjab_send_raw(gjconn gjc, const char *str);
static void gjab_recv(gjconn gjc);
static void gjab_auth(gjconn gjc);

/*
 * It is *this* to which we point the gaim_connection proto_data
 */
struct jabber_data {
	gjconn gjc;
	gboolean did_import;
	GSList *chats;
	GHashTable *hash;
	time_t idle;
	gboolean die;
};

/*
 * Jabber "chat group" info.  Pointers to these go in jabber_data
 * pending and existing chats lists.
 */
struct jabber_chat {
	jid Jid;
	struct gaim_connection *gc;
	struct conversation *b;
	int id;
	int state;
};

/*
 * Jabber chat states...
 *
 * Note: due to a bug in one version of the Jabber server, subscriptions
 * to chat groups aren't (always?) properly removed at the server.  The
 * result is clients receive Jabber "presence" notifications for JIDs
 * they no longer care about.  The problem with such vestigial notifies is
 * that we really have no way of telling if it's vestigial or if it's a
 * valid "buddy" presence notification.  So we keep jabber_chat structs
 * around after leaving a chat group and simply mark them "closed."  That
 * way we can test for such errant presence notifications.  I.e.: if we
 * get a presence notfication from a JID that matches a chat group JID,
 * we disregard it.
 */
#define JCS_PENDING 1	/* pending */
#define JCS_ACTIVE  2	/* active */
#define JCS_CLOSED  3	/* closed */


static char *jabber_name()
{
	return "Jabber";
}

#define STATE_EVT(arg) if(gjc->on_state) { (gjc->on_state)(gjc, (arg) ); }

static void jabber_remove_buddy(struct gaim_connection *gc, char *name, char *group);

static char *create_valid_jid(const char *given, char *server, char *resource)
{
	char *valid;

	if (!strchr(given, '@'))
		valid = g_strdup_printf("%s@%s/%s", given, server, resource);
	else if (!strchr(strchr(given, '@'), '/'))
		valid = g_strdup_printf("%s/%s", given, resource);
	else
		valid = g_strdup(given);

	return valid;
}

static gjconn gjab_new(char *user, char *pass, void *priv)
{
	pool p;
	gjconn gjc;

	if (!user)
		return (NULL);

	p = pool_new();
	if (!p)
		return (NULL);
	gjc = pmalloc_x(p, sizeof(gjconn_struct), 0);
	if (!gjc) {
		pool_free(p);	/* no need for this anymore! */
		return (NULL);
	}
	gjc->p = p;

	if((gjc->user = jid_new(p, user)) == NULL) {
		pool_free(p);	/* no need for this anymore! */
		return (NULL);
	}
	gjc->pass = pstrdup(p, pass);

	gjc->state = JCONN_STATE_OFF;
	gjc->was_connected = 0;
	gjc->id = 1;
	gjc->fd = -1;

	gjc->priv = priv;

	return gjc;
}

static void gjab_delete(gjconn gjc)
{
	if (!gjc)
		return;

	gjab_stop(gjc);
	pool_free(gjc->p);
}

static void gjab_state_handler(gjconn gjc, gjconn_state_h h)
{
	if (!gjc)
		return;

	gjc->on_state = h;
}

static void gjab_packet_handler(gjconn gjc, gjconn_packet_h h)
{
	if (!gjc)
		return;

	gjc->on_packet = h;
}

static void gjab_stop(gjconn gjc)
{
	if (!gjc || gjc->state == JCONN_STATE_OFF)
		return;

	gjab_send_raw(gjc, "</stream:stream>");
	gjc->state = JCONN_STATE_OFF;
	gjc->was_connected = 0;
	closesocket(gjc->fd);
	gjc->fd = -1;
	XML_ParserFree(gjc->parser);
	gjc->parser = NULL;
}

/*
static int gjab_getfd(gjconn gjc)
{
	if (gjc)
		return gjc->fd;
	else
		return -1;
}

static jid gjab_getjid(gjconn gjc)
{
	if (gjc)
		return (gjc->user);
	else
		return NULL;
}

static char *gjab_getsid(gjconn gjc)
{
	if (gjc)
		return (gjc->sid);
	else
		return NULL;
}
*/

static char *gjab_getid(gjconn gjc)
{
	g_snprintf(gjc->idbuf, 8, "%d", gjc->id++);
	return &gjc->idbuf[0];
}

static void gjab_send(gjconn gjc, xmlnode x)
{
	if (gjc && gjc->state != JCONN_STATE_OFF) {
		char *buf = xmlnode2str(x);
		if (buf)
			write(gjc->fd, buf, strlen(buf));
	}
}

static void gjab_send_raw(gjconn gjc, const char *str)
{
	if (gjc && gjc->state != JCONN_STATE_OFF) {
		/*
		 * JFIXME: No error detection?!?!
		 */
		if(write(gjc->fd, str, strlen(str)) < 0) {
			/* Do NOT write to stdout/stderr directly, IRC clients
			   might get confused, and we don't want that...
			fprintf(stderr, "DBG: Problem sending.  Error: %d\n", errno);
			fflush(stderr); */
		}
	}
}

static void gjab_reqroster(gjconn gjc)
{
	xmlnode x;

	x = jutil_iqnew(JPACKET__GET, NS_ROSTER);
	xmlnode_put_attrib(x, "id", gjab_getid(gjc));

	gjab_send(gjc, x);
	xmlnode_free(x);
}

static void gjab_reqauth(gjconn gjc)
{
	xmlnode x, y, z;
	char *user;

	if (!gjc)
		return;

	x = jutil_iqnew(JPACKET__GET, NS_AUTH);
	xmlnode_put_attrib(x, "id", IQID_AUTH);
	y = xmlnode_get_tag(x, "query");

	user = gjc->user->user;

	if (user) {
		z = xmlnode_insert_tag(y, "username");
		xmlnode_insert_cdata(z, user, -1);
	}

	gjab_send(gjc, x);
	xmlnode_free(x);
}

static void gjab_auth(gjconn gjc)
{
	xmlnode x, y, z;
	char *hash, *user;

	if (!gjc)
		return;

	x = jutil_iqnew(JPACKET__SET, NS_AUTH);
	xmlnode_put_attrib(x, "id", IQID_AUTH);
	y = xmlnode_get_tag(x, "query");

	user = gjc->user->user;

	if (user) {
		z = xmlnode_insert_tag(y, "username");
		xmlnode_insert_cdata(z, user, -1);
	}

	z = xmlnode_insert_tag(y, "resource");
	xmlnode_insert_cdata(z, gjc->user->resource, -1);

	if (gjc->sid) {
		z = xmlnode_insert_tag(y, "digest");
		hash = pmalloc(x->p, strlen(gjc->sid) + strlen(gjc->pass) + 1);
		strcpy(hash, gjc->sid);
		strcat(hash, gjc->pass);
		hash = shahash(hash);
		xmlnode_insert_cdata(z, hash, 40);
	} else {
		z = xmlnode_insert_tag(y, "password");
		xmlnode_insert_cdata(z, gjc->pass, -1);
	}

	gjab_send(gjc, x);
	xmlnode_free(x);

	return;
}

static void gjab_recv(gjconn gjc)
{
	static char buf[4096];
	int len;

	if (!gjc || gjc->state == JCONN_STATE_OFF)
		return;

	if ((len = read(gjc->fd, buf, sizeof(buf) - 1)) > 0) {
		struct jabber_data *jd = GJ_GC(gjc)->proto_data;
		buf[len] = '\0';
		XML_Parse(gjc->parser, buf, len, 0);
		if (jd->die)
			signoff(GJ_GC(gjc));
	} else if (len < 0 || errno != EAGAIN) {
		STATE_EVT(JCONN_STATE_OFF)
	}
}

static void startElement(void *userdata, const char *name, const char **attribs)
{
	xmlnode x;
	gjconn gjc = (gjconn) userdata;

	if (gjc->current) {
		/* Append the node to the current one */
		x = xmlnode_insert_tag(gjc->current, name);
		xmlnode_put_expat_attribs(x, attribs);

		gjc->current = x;
	} else {
		x = xmlnode_new_tag(name);
		xmlnode_put_expat_attribs(x, attribs);
		if (strcmp(name, "stream:stream") == 0) {
			/* special case: name == stream:stream */
			/* id attrib of stream is stored for digest auth */
			gjc->sid = g_strdup(xmlnode_get_attrib(x, "id"));
			/* STATE_EVT(JCONN_STATE_AUTH) */
			xmlnode_free(x);
		} else {
			gjc->current = x;
		}
	}
}

static void endElement(void *userdata, const char *name)
{
	gjconn gjc = (gjconn) userdata;
	xmlnode x;
	jpacket p;

	if (gjc->current == NULL) {
		/* we got </stream:stream> */
		STATE_EVT(JCONN_STATE_OFF)
		    return;
	}

	x = xmlnode_get_parent(gjc->current);

	if (!x) {
		/* it is time to fire the event */
		p = jpacket_new(gjc->current);

		if (gjc->on_packet)
			(gjc->on_packet) (gjc, p);
		else
			xmlnode_free(gjc->current);
	}

	gjc->current = x;
}

static void jabber_callback(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_connection *gc = (struct gaim_connection *)data;
	struct jabber_data *jd = (struct jabber_data *)gc->proto_data;

	gjab_recv(jd->gjc);
}

static void charData(void *userdata, const char *s, int slen)
{
	gjconn gjc = (gjconn) userdata;

	if (gjc->current)
		xmlnode_insert_cdata(gjc->current, s, slen);
}

static void gjab_connected(gpointer data, gint source, GaimInputCondition cond)
{
	xmlnode x;
	char *t, *t2;
	struct gaim_connection *gc = data;
	struct jabber_data *jd;
	gjconn gjc;

	if (!g_slist_find(get_connections(), gc)) {
		closesocket(source);
		return;
	}

	jd = gc->proto_data;
	gjc = jd->gjc;

	if (gjc->fd != source)
		gjc->fd = source;

	if (source == -1) {
		STATE_EVT(JCONN_STATE_OFF)
		return;
	}

	gjc->state = JCONN_STATE_CONNECTED;
	STATE_EVT(JCONN_STATE_CONNECTED)

	/* start stream */
	x = jutil_header(NS_CLIENT, gjc->user->server);
	t = xmlnode2str(x);
	/* this is ugly, we can create the string here instead of jutil_header */
	/* what do you think about it? -madcat */
	t2 = strstr(t, "/>");
	*t2++ = '>';
	*t2 = '\0';
	gjab_send_raw(gjc, "<?xml version='1.0'?>");
	gjab_send_raw(gjc, t);
	xmlnode_free(x);

	gjc->state = JCONN_STATE_ON;
	STATE_EVT(JCONN_STATE_ON);

	gc = GJ_GC(gjc);
	gc->inpa = gaim_input_add(gjc->fd, GAIM_INPUT_READ, jabber_callback, gc);
}

static void gjab_start(gjconn gjc)
{
	struct aim_user *user;
	int port;

	if (!gjc || gjc->state != JCONN_STATE_OFF)
		return;

	user = GJ_GC(gjc)->user;
	port = user->proto_opt[USEROPT_PORT][0] ? atoi(user->proto_opt[USEROPT_PORT]) : DEFAULT_PORT;

	gjc->parser = XML_ParserCreate(NULL);
	XML_SetUserData(gjc->parser, (void *)gjc);
	XML_SetElementHandler(gjc->parser, startElement, endElement);
	XML_SetCharacterDataHandler(gjc->parser, charData);

	gjc->fd = proxy_connect(gjc->user->server, port, gjab_connected, GJ_GC(gjc));
	if (!user->gc || (gjc->fd < 0)) {
		STATE_EVT(JCONN_STATE_OFF)
		return;
	}
}

/*
 * Find chat by chat group name
 */
/* ** Not needed yet - Bitlbee **
static struct conversation *find_chat(struct gaim_connection *gc, char *name)
{
	GSList *bcs = gc->buddy_chats;
	struct conversation *b = NULL;
	char *chat = g_strdup(normalize(name));

	while (bcs) {
		b = bcs->data;
		if (!g_ascii_strcasecmp(normalize(b->name), chat))
			break;
		b = NULL;
		bcs = bcs->next;
	}

	g_free(chat);
	return b;
}
** End - Bitlbee ** */

/*
 * Find chat by "chat id"
 *
 * Returns: 0 on success and jabber_chat pointer set
 * or -EINVAL on error and jabber_chat pointer is
 * undefined.
 *
 * TBD: Slogging through the buddy_chats list seems
 * redundant since the chat i.d. is mirrored in the
 * jabber_chat struct list.  But that's the way it
 * was, so that's the way I'm leaving it--for now.
 */
/* ** Not needed yet - Bitlbee **
static int jabber_find_chat_by_convo_id(struct gaim_connection *gc, int id, struct jabber_chat **jc)
{
	GSList *bcs = gc->buddy_chats;
	struct conversation *b = NULL;
	struct jabber_data *jd = gc->proto_data;

	*jc = NULL;

	while(bcs != NULL) {
		b = bcs->data;
		if (id == b->id)
			break;
		bcs = bcs->next;
	}

	if (bcs != NULL) {
		bcs = jd->chats;
		while (bcs != NULL) {
			*jc = bcs->data;
			if ((*jc)->state == JCS_ACTIVE && (*jc)->b == b)
				break;
			bcs = bcs->next;
		}
	}

	return(bcs == NULL? -EINVAL : 0);
}
** End - Bitlbee ** */

/* [SH] Not needed now */
#if 0
/*
 * Find any chat
 */
static struct jabber_chat *find_any_chat(struct gaim_connection *gc, jid chat)
{
	GSList *jcs = ((struct jabber_data *)gc->proto_data)->chats;
	struct jabber_chat *jc = NULL;

	while (jcs) {
		jc = jcs->data;
		if (!jid_cmpx(chat, jc->Jid, JID_USER | JID_SERVER))
			break;
		jc = NULL;
		jcs = jcs->next;
	}

	return jc;
}
#endif
/* [SH] Till here */

/*
 * Find existing/active Jabber chat
 */
static struct jabber_chat *find_existing_chat(struct gaim_connection *gc, jid chat)
{
	GSList *jcs = ((struct jabber_data *)gc->proto_data)->chats;
	struct jabber_chat *jc = NULL;

	while (jcs) {
		jc = jcs->data;
		if (jc->state == JCS_ACTIVE && !jid_cmpx(chat, jc->Jid, JID_USER | JID_SERVER))
			break;
		jc = NULL;
		jcs = jcs->next;
	}

	return jc;
}

/*
 * Find pending chat
 */
static struct jabber_chat *find_pending_chat(struct gaim_connection *gc, jid chat)
{
	GSList *jcs = ((struct jabber_data *)gc->proto_data)->chats;
	struct jabber_chat *jc = NULL;

	while (jcs) {
		jc = jcs->data;
		if (jc->state == JCS_PENDING && !jid_cmpx(chat, jc->Jid, JID_USER | JID_SERVER))
			break;
		jc = NULL;
		jcs = jcs->next;
	}

	return jc;
}

static gboolean find_chat_buddy(struct conversation *b, char *name)
{
	GList *m = b->in_room;

	while (m) {
		if (!strcmp(m->data, name))
			return TRUE;
		m = m->next;
	}

	return FALSE;
}

/*
 * Remove a buddy from the (gaim) buddylist (if he's on it)
 */
static void jabber_remove_gaim_buddy(struct gaim_connection *gc, char *buddyname)
{
	struct buddy *b;

	if ((b = find_buddy(gc, buddyname)) != NULL) {
		/* struct group *group;

		group = find_group_by_buddy(gc, buddyname);
		remove_buddy(gc, group, b); */
		jabber_remove_buddy(gc, b->name, JABBER_GROUP);
	}
}

/*
 * keep track of away msg same as yahoo plugin
 */
static void jabber_track_away(gjconn gjc, jpacket p, char *name, char *type)
{
	struct jabber_data *jd = GJ_GC(gjc)->proto_data;
	gpointer val = g_hash_table_lookup(jd->hash, name);
	char *show;
	char *vshow = NULL;
	char *status = NULL;
	char *msg = NULL;

	if (type && (g_ascii_strcasecmp(type, "unavailable") == 0)) {
		vshow = _("Unavailable");
	} else {
		if((show = xmlnode_get_tag_data(p->x, "show")) != NULL) {
			if (!g_ascii_strcasecmp(show, "away")) {
				vshow = _("Away");
			} else if (!g_ascii_strcasecmp(show, "chat")) {
				vshow = _("Online");
			} else if (!g_ascii_strcasecmp(show, "xa")) {
				vshow = _("Extended Away");
			} else if (!g_ascii_strcasecmp(show, "dnd")) {
				vshow = _("Do Not Disturb");
			}
		}
	}

	status = xmlnode_get_tag_data(p->x, "status");

	if(vshow != NULL || status != NULL ) {
		/* kinda hokey, but it works :-) */
		msg = g_strdup_printf("%s%s%s",
			(vshow == NULL? "" : vshow),
			(vshow == NULL || status == NULL? "" : ": "),
			(status == NULL? "" : status));
	} else {
		msg = g_strdup(_("Online"));
	}

	if (val) {
		g_free(val);
		g_hash_table_insert(jd->hash, name, msg);
	} else {
		g_hash_table_insert(jd->hash, g_strdup(name), msg);
	}
}

static time_t iso8601_to_time(char *timestamp)
{
	struct tm t;
	time_t retval = 0;

	if(sscanf(timestamp,"%04d%02d%02dT%02d:%02d:%02d",
		&t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec))
	{
		t.tm_year -= 1900;
		t.tm_mon -= 1;
		t.tm_isdst = 0;
		retval = mktime(&t);
#		ifdef HAVE_TM_GMTOFF
			retval += t.tm_gmtoff;
#		else
#		        ifdef HAVE_TIMEZONE
				tzset();	/* making sure */
				retval -= timezone;
#		        endif
#		endif
	}

	return retval;
}

static void jabber_handlemessage(gjconn gjc, jpacket p)
{
	xmlnode y, xmlns, subj, z;
	time_t time_sent = time(NULL);

	char *from = NULL, *msg = NULL, *type = NULL, *topic = NULL;
	char m[BUF_LONG * 2];

	type = xmlnode_get_attrib(p->x, "type");

	z = xmlnode_get_firstchild(p->x);

	while(z)
	{
	   if(NSCHECK(z,NS_DELAY))
	   {
	      char *timestamp = xmlnode_get_attrib(z,"stamp");
	      time_sent = iso8601_to_time(timestamp);
	   }
	   z = xmlnode_get_nextsibling(z);
	}

	if (!type || !g_ascii_strcasecmp(type, "normal") || !g_ascii_strcasecmp(type, "chat")) {

		/* XXX namespaces could be handled better. (mid) */
		if ((xmlns = xmlnode_get_tag(p->x, "x")))
			type = xmlnode_get_attrib(xmlns, "xmlns");

		from = jid_full(p->from);
		/*
		if ((y = xmlnode_get_tag(p->x, "html"))) {
			msg = xmlnode_get_data(y);
		} else
		*/
		if ((y = xmlnode_get_tag(p->x, "body"))) {
			msg = xmlnode_get_data(y);
		}


		if (!from)
			return;

		if (type && !g_ascii_strcasecmp(type, "jabber:x:conference")) {
			char *room;
			GList *m = NULL;
			char **data;

			room = xmlnode_get_attrib(xmlns, "jid");
			data = g_strsplit(room, "@", 2);
			m = g_list_append(m, g_strdup(data[0]));
			m = g_list_append(m, g_strdup(data[1]));
			m = g_list_append(m, g_strdup(gjc->user->user));
			g_strfreev(data);

			/* ** Bitlbee ** serv_got_chat_invite(GJ_GC(gjc), room, from, msg, m); */
		} else if (msg) { /* whisper */
			struct jabber_chat *jc;
			g_snprintf(m, sizeof(m), "%s", msg);
			if (((jc = find_existing_chat(GJ_GC(gjc), p->from)) != NULL) && jc->b)
				serv_got_chat_in(GJ_GC(gjc), jc->b->id, p->from->resource, 1, m, time_sent);
			else {
				int flags = 0;
				/* ** Bitlbee **
				if (xmlnode_get_tag(p->x, "gaim"))
					flags = IM_FLAG_GAIMUSER;
				if (find_conversation(jid_full(p->from)))
					serv_got_im(GJ_GC(gjc), jid_full(p->from), m, flags, time_sent, -1);
				else {
				** End - Bitlbee ** */
					if(p->from->user) {
					    from = g_strdup_printf("%s@%s", p->from->user, p->from->server);
					} else {
					    /* server message? */
					    from = g_strdup(p->from->server);
					}
					serv_got_im(GJ_GC(gjc), from, m, flags, time_sent, -1);
					g_free(from);
				/* ** Bitlbee ** } ** End - Bitlbee ** */
			}
		}

	} else if (!g_ascii_strcasecmp(type, "error")) {
		if ((y = xmlnode_get_tag(p->x, "error"))) {
			type = xmlnode_get_attrib(y, "code");
			msg = xmlnode_get_data(y);
		}

		if (msg) {
			from = g_strdup_printf("Error %s", type ? type : "");
			do_error_dialog(msg, from);
			g_free(from);
		}
	} else if (!g_ascii_strcasecmp(type, "groupchat")) {
		struct jabber_chat *jc;
		static int i = 0;

		/*
		if ((y = xmlnode_get_tag(p->x, "html"))) {
			msg = xmlnode_get_data(y);
		} else
		*/
		if ((y = xmlnode_get_tag(p->x, "body"))) {
			msg = xmlnode_get_data(y);
		}

		msg = utf8_to_str(msg);
		
		if ((subj = xmlnode_get_tag(p->x, "subject"))) {
		   	topic = xmlnode_get_data(subj);
		} 
		topic = utf8_to_str(topic);

		jc = find_existing_chat(GJ_GC(gjc), p->from);
		if (!jc) {
			/* we're not in this chat. are we supposed to be? */
			if ((jc = find_pending_chat(GJ_GC(gjc), p->from)) != NULL) {
				/* yes, we're supposed to be. so now we are. */
				jc->b = serv_got_joined_chat(GJ_GC(gjc), i++, p->from->user);
				jc->id = jc->b->id;
				jc->state = JCS_ACTIVE;
			} else {
				/* no, we're not supposed to be. */
				g_free(msg);
				return;
			}
		}
		if (p->from->resource) {
			if (!y) {
				if (!find_chat_buddy(jc->b, p->from->resource)) {
					add_chat_buddy(jc->b, p->from->resource);
				} else if ((y = xmlnode_get_tag(p->x, "status"))) {
					char *buf;

					buf = g_strdup_printf("%s@%s/%s",
						p->from->user, p->from->server, p->from->resource);
					jabber_track_away(gjc, p, buf, NULL);
					g_free(buf);

				}
			} else if (jc->b && msg) {
				char buf[8192];

				if (topic) {
					char tbuf[8192];
					g_snprintf(tbuf, sizeof(tbuf), "%s", topic);
				}
				

				g_snprintf(buf, sizeof(buf), "%s", msg);
				serv_got_chat_in(GJ_GC(gjc), jc->b->id, p->from->resource, 0, buf, time_sent);
			}
		} else { /* message from the server */
		   	if(jc->b && topic) {
			   	char tbuf[8192];
				g_snprintf(tbuf, sizeof(tbuf), "%s", topic);
			}
		}

		g_free(msg);
		g_free(topic);

	}
}
	   
static void jabber_handlepresence(gjconn gjc, jpacket p)
{
	char *to, *from, *type;
	struct buddy *b = NULL;
	jid who;
	char *buddy;
	xmlnode y;
	char *show;
	int state = 0;
	GSList *resources;
	char *res;
	struct conversation *cnv = NULL;
	struct jabber_chat *jc = NULL;

	to = xmlnode_get_attrib(p->x, "to");
	from = xmlnode_get_attrib(p->x, "from");
	type = xmlnode_get_attrib(p->x, "type");
	
	if (type && g_ascii_strcasecmp(type, "error") == 0) {
		return;
	}
	else if ((y = xmlnode_get_tag(p->x, "show"))) {
		show = xmlnode_get_data(y);
		if (!show) {
			state = 0;
		} else if (!g_ascii_strcasecmp(show, "away")) {
			state = UC_AWAY;
		} else if (!g_ascii_strcasecmp(show, "chat")) {
			state = UC_CHAT;
		} else if (!g_ascii_strcasecmp(show, "xa")) {
			state = UC_XA;
		} else if (!g_ascii_strcasecmp(show, "dnd")) {
			state = UC_DND;
		}
	} else {
		state = 0;
	}

	who = jid_new(gjc->p, from);
	if (who->user == NULL) {
		/* FIXME: transport */
		return;
	}

	buddy = g_strdup_printf("%s@%s", who->user, who->server);

	/* um. we're going to check if it's a chat. if it isn't, and there are pending
	 * chats, create the chat. if there aren't pending chats and we don't have the
	 * buddy on our list, simply bail out. */
	if ((cnv = NULL) == NULL) {
		static int i = 0x70;
		if ((jc = find_pending_chat(GJ_GC(gjc), who)) != NULL) {
			jc->b = cnv = serv_got_joined_chat(GJ_GC(gjc), i++, who->user);
			jc->id = jc->b->id;
			jc->state = JCS_ACTIVE;
		} else if ((b = find_buddy(GJ_GC(gjc), buddy)) == NULL) {
			g_free(buddy);
			return;
		}
	}

	if (!cnv) {
		resources = b->proto_data;
		res = who->resource;
		if (res)
			while (resources) {
				if (!strcmp(res, resources->data))
					break;
				resources = resources->next;
			}

		/* keep track of away msg same as yahoo plugin */
		jabber_track_away(gjc, p, normalize(b->name), type);

		if (type && (g_ascii_strcasecmp(type, "unavailable") == 0)) {
			if (resources) {
				g_free(resources->data);
				b->proto_data = g_slist_remove(b->proto_data, resources->data);
			}
			if (!b->proto_data) {
				serv_got_update(GJ_GC(gjc), buddy, 0, 0, 0, 0, 0, 0);
			}
		} else {
			if (!resources) {
				b->proto_data = g_slist_append(b->proto_data, g_strdup(res));
			}

			serv_got_update(GJ_GC(gjc), buddy, 1, 0, b->signon, b->idle, state, 0);

		}
	} else {
		if (who->resource) {
			char *buf;

			buf = g_strdup_printf("%s@%s/%s", who->user, who->server, who->resource);
			jabber_track_away(gjc, p, buf, type);
			g_free(buf);

			if (type && !g_ascii_strcasecmp(type, "unavailable")) {
				struct jabber_data *jd;
				if (!jc && !(jc = find_existing_chat(GJ_GC(gjc), who))) {
					g_free(buddy);
					return;
				}
				jd = jc->gc->proto_data;
				/* if it's not ourselves...*/
				if (strcmp(who->resource, jc->Jid->resource) && jc->b) {
					remove_chat_buddy(jc->b, who->resource, NULL);
					g_free(buddy);
					return;
				}

				jc->state = JCS_CLOSED;
				serv_got_chat_left(GJ_GC(gjc), jc->id);
				/*
				 * TBD: put back some day?
				jd->chats = g_slist_remove(jd->chats, jc);
				g_free(jc);
				 */
			} else {
				if ((!jc && !(jc = find_existing_chat(GJ_GC(gjc), who))) || !jc->b) {
					g_free(buddy);
					return;
				}
				if (!find_chat_buddy(jc->b, who->resource)) {
					add_chat_buddy(jc->b, who->resource);
				}
			}
		}
	}

	g_free(buddy);

	return;
}

/*
 * Used only by Jabber accept/deny add stuff just below
 */
struct jabber_add_permit {
	gjconn gjc;
	gchar *user;
};

/*
 * Common part for Jabber accept/deny adds
 *
 * "type" says whether we'll permit/deny the subscribe request
 */
static void jabber_accept_deny_add(struct jabber_add_permit *jap, const char *type)
{
	xmlnode g = xmlnode_new_tag("presence");

	xmlnode_put_attrib(g, "to", jap->user);
	xmlnode_put_attrib(g, "type", type);
	gjab_send(jap->gjc, g);

	xmlnode_free(g);
}

/*
 * Callback from "accept" in do_ask_dialog() invoked by jabber_handles10n()
 */
static void jabber_accept_add(gpointer w, struct jabber_add_permit *jap)
{
	jabber_accept_deny_add(jap, "subscribed");
	/*
	 * If we don't already have the buddy on *our* buddylist,
	 * ask if we want him or her added.
	 */
	if(find_buddy(GJ_GC(jap->gjc), jap->user) == NULL) {
		show_got_added(GJ_GC(jap->gjc), NULL, jap->user, NULL, NULL);
	}
	g_free(jap->user);
	g_free(jap);
}

/*
 * Callback from "deny/cancel" in do_ask_dialog() invoked by jabber_handles10n()
 */
static void jabber_deny_add(gpointer w, struct jabber_add_permit *jap)
{
	jabber_accept_deny_add(jap, "unsubscribed");
	g_free(jap->user);
	g_free(jap);
}

/*
 * Handle subscription requests
 */
static void jabber_handles10n(gjconn gjc, jpacket p)
{
	xmlnode g;
	char *Jid = xmlnode_get_attrib(p->x, "from");
	char *type = xmlnode_get_attrib(p->x, "type");

	g = xmlnode_new_tag("presence");
	xmlnode_put_attrib(g, "to", Jid);

	if (!strcmp(type, "subscribe")) {
		/*
		 * A "subscribe to us" request was received - put up the approval dialog
		 */
		struct jabber_add_permit *jap = g_new0(struct jabber_add_permit, 1);
		gchar *msg = g_strdup_printf(_("The user %s wants to add you to their buddy list."),
				Jid);

		jap->gjc = gjc;
		jap->user = g_strdup(Jid);
		do_ask_dialog(GJ_GC(gjc), msg, jap, jabber_accept_add, jabber_deny_add);

		g_free(msg);
		xmlnode_free(g);	/* Never needed it here anyway */
		return;

	} else if (!strcmp(type, "unsubscribe")) {
		/*
		 * An "unsubscribe to us" was received - simply "approve" it
		 */
		xmlnode_put_attrib(g, "type", "unsubscribed");
	} else {
		/*
		 * Did we attempt to subscribe to somebody and they do not exist?
		 */
		if (!strcmp(type, "unsubscribed")) {
			xmlnode y;
			char *status;
			if((y = xmlnode_get_tag(p->x, "status")) && (status = xmlnode_get_data(y)) &&
					!strcmp(status, "Not Found")) {
				char *msg = g_strdup_printf("%s: \"%s\"", _("No such user"), 
					xmlnode_get_attrib(p->x, "from"));
				do_error_dialog(msg, _("Jabber Error"));
				g_free(msg);
			}
		}

		xmlnode_free(g);
		return;
	}

	gjab_send(gjc, g);
	xmlnode_free(g);
}

/*
 * Pending subscription to a buddy?
 */
#define BUD_SUB_TO_PEND(sub, ask) ((!g_ascii_strcasecmp((sub), "none") || !g_ascii_strcasecmp((sub), "from")) && \
					(ask) != NULL && !g_ascii_strcasecmp((ask), "subscribe")) 

/*
 * Subscribed to a buddy?
 */
#define BUD_SUBD_TO(sub, ask) ((!g_ascii_strcasecmp((sub), "to") || !g_ascii_strcasecmp((sub), "both")) && \
					((ask) == NULL || !g_ascii_strcasecmp((ask), "subscribe")))

/*
 * Pending unsubscription to a buddy?
 */
#define BUD_USUB_TO_PEND(sub, ask) ((!g_ascii_strcasecmp((sub), "to") || !g_ascii_strcasecmp((sub), "both")) && \
					(ask) != NULL && !g_ascii_strcasecmp((ask), "unsubscribe")) 

/*
 * Unsubscribed to a buddy?
 */
#define BUD_USUBD_TO(sub, ask) ((!g_ascii_strcasecmp((sub), "none") || !g_ascii_strcasecmp((sub), "from")) && \
					((ask) == NULL || !g_ascii_strcasecmp((ask), "unsubscribe")))

/*
 * If a buddy is added or removed from the roster on another resource
 * jabber_handlebuddy is called
 *
 * Called with roster item node.
 */
static void jabber_handlebuddy(gjconn gjc, xmlnode x)
{
	xmlnode g;
	char *Jid, *name, *sub, *ask;
	jid who;
	struct buddy *b = NULL;
	char *buddyname, *groupname = NULL;

	Jid = xmlnode_get_attrib(x, "jid");
	name = xmlnode_get_attrib(x, "name");
	sub = xmlnode_get_attrib(x, "subscription");
	ask = xmlnode_get_attrib(x, "ask");
	who = jid_new(gjc->p, Jid);

	/* JFIXME: jabber_handleroster() had a "FIXME: transport" at this
	 * equivilent point.  So...
	 *
	 * We haven't allocated any memory or done anything interesting to
	 * this point, so we'll violate Good Coding Structure here by
	 * simply bailing out.
	 */
	if (!who || !who->user) {
		return;
	}

	buddyname = g_strdup_printf("%s@%s", who->user, who->server);

	if((g = xmlnode_get_tag(x, "group")) != NULL) {
		groupname = xmlnode_get_data(g);
	}

	/*
	 * Add or remove a buddy?  Change buddy's alias or group?
	 */
	if (BUD_SUB_TO_PEND(sub, ask) || BUD_SUBD_TO(sub, ask)) {
		if ((b = find_buddy(GJ_GC(gjc), buddyname)) == NULL) {
			add_buddy(GJ_GC(gjc), groupname ? groupname : _("Buddies"), buddyname,
				name ? name : buddyname);
		} else {
			/* struct group *c_grp = find_group_by_buddy(GJ_GC(gjc), buddyname); */

			/* 
			 * If the buddy's in a new group or his/her alias is changed...
			 */
			if(groupname) {
				int present = b->present;	/* save presence state */
				int uc = b->uc;			/* and away state (?) */
				int idle = b->idle;
				int signon = b->signon;

				/*
				 * seems rude, but it seems to be the only way...
				 */
				/* remove_buddy(GJ_GC(gjc), c_grp, b); */
				jabber_remove_buddy(GJ_GC(gjc), b->name, JABBER_GROUP);
				
				add_buddy(GJ_GC(gjc), groupname, buddyname,
					name ? name : buddyname);
				if(present) {
					serv_got_update(GJ_GC(gjc), buddyname, 1, 0, signon, idle, uc, 0);
				}
			} else if(name != NULL && strcmp(b->show, name)) {
				strncpy(b->show, name, BUDDY_ALIAS_MAXLEN);
				b->show[BUDDY_ALIAS_MAXLEN - 1] = '\0';	/* cheap safety feature */
				handle_buddy_rename(b, buddyname);
			}
		}
	}  else if (BUD_USUB_TO_PEND(sub, ask) || BUD_USUBD_TO(sub, ask) || !g_ascii_strcasecmp(sub, "remove")) {
		jabber_remove_gaim_buddy(GJ_GC(gjc), buddyname);
	}
	g_free(buddyname);

}

static void jabber_handleroster(gjconn gjc, xmlnode querynode)
{
	xmlnode x;

	x = xmlnode_get_firstchild(querynode);
	while (x) {
		jabber_handlebuddy(gjc, x);
		x = xmlnode_get_nextsibling(x);
	}

	x = jutil_presnew(0, NULL, "Online");
	gjab_send(gjc, x);
	xmlnode_free(x);
}

static void jabber_handleauthresp(gjconn gjc, jpacket p)
{
	if (jpacket_subtype(p) == JPACKET__RESULT) {
		if (xmlnode_has_children(p->x)) {
			xmlnode query = xmlnode_get_tag(p->x, "query");
			set_login_progress(GJ_GC(gjc), 4, _("Authenticating"));
			if (!xmlnode_get_tag(query, "digest")) {
				g_free(gjc->sid);
				gjc->sid = NULL;
			}
			gjab_auth(gjc);
		} else {
			account_online(GJ_GC(gjc));

			if (bud_list_cache_exists(GJ_GC(gjc)))
				do_import(GJ_GC(gjc), NULL);

			((struct jabber_data *)GJ_GC(gjc)->proto_data)->did_import = TRUE;

			gjab_reqroster(gjc);
		}
	} else {
		xmlnode xerr;
		char *errmsg = NULL;
		int errcode = 0;
		struct jabber_data *jd = GJ_GC(gjc)->proto_data;

		xerr = xmlnode_get_tag(p->x, "error");
		if (xerr) {
			char msg[BUF_LONG];
			errmsg = xmlnode_get_data(xerr);
			if (xmlnode_get_attrib(xerr, "code")) {
				errcode = atoi(xmlnode_get_attrib(xerr, "code"));
				g_snprintf(msg, sizeof(msg), "Error %d: %s", errcode, errmsg);
			} else
				g_snprintf(msg, sizeof(msg), "%s", errmsg);
			hide_login_progress(GJ_GC(gjc), msg);
		} else {
			hide_login_progress(GJ_GC(gjc), _("Unknown login error"));
		}

		jd->die = TRUE;
	}
}

static void jabber_handleversion(gjconn gjc, xmlnode iqnode) {
	xmlnode querynode, x;
	char *id, *from;
	char os[1024];
#ifndef _WIN32
	struct utsname osinfo;

	uname(&osinfo);
	g_snprintf(os, sizeof os, "%s %s %s", osinfo.sysname, osinfo.release, osinfo.machine);
#else
	g_snprintf(os, sizeof os, "Windows %d %d", _winmajor, _winminor);
#endif


	id = xmlnode_get_attrib(iqnode, "id");
	from = xmlnode_get_attrib(iqnode, "from");

	x = jutil_iqnew(JPACKET__RESULT, NS_VERSION);

	xmlnode_put_attrib(x, "to", from);
	xmlnode_put_attrib(x, "id", id);
	querynode = xmlnode_get_tag(x, "query");
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "name"), PACKAGE, -1);
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "version"), BITLBEE_VERSION, -1);
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "os"), os, -1);

	gjab_send(gjc, x);

	xmlnode_free(x);
}

static void jabber_handletime(gjconn gjc, xmlnode iqnode) {
	xmlnode querynode, x;
	char *id, *from;
	time_t now_t; 
	struct tm *now;
	char buf[1024];

	time(&now_t);
	now = localtime(&now_t);

	id = xmlnode_get_attrib(iqnode, "id");
	from = xmlnode_get_attrib(iqnode, "from");

	x = jutil_iqnew(JPACKET__RESULT, NS_TIME);

	xmlnode_put_attrib(x, "to", from);
	xmlnode_put_attrib(x, "id", id);
	querynode = xmlnode_get_tag(x, "query");

	strftime(buf, 1024, "%Y%m%dT%T", now);
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "utc"), buf, -1);
	strftime(buf, 1024, "%Z", now);
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "tz"), buf, -1);
	strftime(buf, 1024, "%d %b %Y %T", now);
	xmlnode_insert_cdata(xmlnode_insert_tag(querynode, "display"), buf, -1);
	
	gjab_send(gjc, x);

	xmlnode_free(x);
}

static void jabber_handlelast(gjconn gjc, xmlnode iqnode) {
   	xmlnode x, querytag;
	char *id, *from;
	struct jabber_data *jd = GJ_GC(gjc)->proto_data;
	char idle_time[32];
	
	id = xmlnode_get_attrib(iqnode, "id");
	from = xmlnode_get_attrib(iqnode, "from");

	x = jutil_iqnew(JPACKET__RESULT, "jabber:iq:last");

	xmlnode_put_attrib(x, "to", from);
	xmlnode_put_attrib(x, "id", id);
	querytag = xmlnode_get_tag(x, "query");
	g_snprintf(idle_time, sizeof idle_time, "%ld", jd->idle ? time(NULL) - jd->idle : 0);
	xmlnode_put_attrib(querytag, "seconds", idle_time);

	gjab_send(gjc, x);
	xmlnode_free(x);
}

/*
 * delete == TRUE: delete found entry
 *
 * returns pointer to (local) copy of value if found, NULL otherwise
 *
 * Note: non-reentrant!  Local static storage re-used on subsequent calls.
 * If you're going to need to keep the returned value, make a copy!
 */
static gchar *jabber_track_queries(GHashTable *queries, gchar *key, gboolean delete)
{
	gpointer my_key, my_val;
	static gchar *ret_val = NULL;

	if(ret_val != NULL) {
		g_free(ret_val);
		ret_val = NULL;
	}

	/* self-protection */
	if(queries != NULL && key != NULL) {
		if(g_hash_table_lookup_extended(queries, key, &my_key, &my_val)) {
			ret_val = g_strdup((gchar *) my_val);
			if(delete) {
				g_hash_table_remove(queries, key);
				g_free(my_key);
				g_free(my_val);
			}
		}
	}

	return(ret_val);
}

static void jabber_handlepacket(gjconn gjc, jpacket p)
{
	char *id;
	switch (p->type) {
	case JPACKET_MESSAGE:
		jabber_handlemessage(gjc, p);
		break;
	case JPACKET_PRESENCE:
		jabber_handlepresence(gjc, p);
		break;
	case JPACKET_IQ:
		id = xmlnode_get_attrib(p->x, "id");
		if (id != NULL && !strcmp(id, IQID_AUTH)) {
			jabber_handleauthresp(gjc, p);
			break;
		}

		if (jpacket_subtype(p) == JPACKET__SET) {
			xmlnode querynode;
			querynode = xmlnode_get_tag(p->x, "query");
			if (NSCHECK(querynode, "jabber:iq:roster")) {
				jabber_handlebuddy(gjc, xmlnode_get_firstchild(querynode));
			}
		} else if (jpacket_subtype(p) == JPACKET__GET) {
		   	xmlnode querynode;
			querynode = xmlnode_get_tag(p->x, "query");
		   	if (NSCHECK(querynode, NS_VERSION)) {
			   	jabber_handleversion(gjc, p->x);
			} else if (NSCHECK(querynode, NS_TIME)) {
			   	jabber_handletime(gjc, p->x);
			} else if (NSCHECK(querynode, "jabber:iq:last")) {
			   	jabber_handlelast(gjc, p->x);
			}
		} else if (jpacket_subtype(p) == JPACKET__RESULT) {
			xmlnode querynode, vcard;
			/* char *xmlns; */
			char *from;

			/*
			 * TBD: ISTM maybe this part could use a serious re-work?
			 */
			from = xmlnode_get_attrib(p->x, "from");
			querynode = xmlnode_get_tag(p->x, "query");
			vcard = xmlnode_get_tag(p->x, "vCard");
			if (!vcard)
				vcard = xmlnode_get_tag(p->x, "VCARD");

			if (NSCHECK(querynode, NS_ROSTER)) {
				jabber_handleroster(gjc, querynode);
			} else if (NSCHECK(querynode, NS_VCARD)) {
				jabber_track_queries(gjc->queries, id, TRUE);	/* delete query track */
			} else if (vcard) {
				jabber_track_queries(gjc->queries, id, TRUE);	/* delete query track */
			} else {
				char *val;

				/* handle "null" query results */
				if((val = jabber_track_queries(gjc->queries, id, TRUE)) != NULL) {
					/* No-op */
				}
			}

		} else if (jpacket_subtype(p) == JPACKET__ERROR) {
			xmlnode xerr;
			char *from, *errmsg = NULL;
			int errcode = 0;

			from = xmlnode_get_attrib(p->x, "from");
			xerr = xmlnode_get_tag(p->x, "error");
			if (xerr) {
				errmsg = xmlnode_get_data(xerr);
				if (xmlnode_get_attrib(xerr, "code"))
					errcode = atoi(xmlnode_get_attrib(xerr, "code"));
			}

			from = g_strdup_printf("Error %d (%s)", errcode, from);
			do_error_dialog(errmsg, from);
			g_free(from);

		}

		break;
	case JPACKET_S10N:
		jabber_handles10n(gjc, p);
		break;
	}

	xmlnode_free(p->x);

	return;
}

static void jabber_handlestate(gjconn gjc, int state)
{
	switch (state) {
	case JCONN_STATE_OFF:
		if(gjc->was_connected) {
			hide_login_progress_error(GJ_GC(gjc), _("Connection lost"));
		} else {
			hide_login_progress(GJ_GC(gjc), _("Unable to connect"));
		}
		signoff(GJ_GC(gjc));
		break;
	case JCONN_STATE_CONNECTED:
		gjc->was_connected = 1;
		set_login_progress(GJ_GC(gjc), 2, _("Connected"));
		break;
	case JCONN_STATE_ON:
		set_login_progress(GJ_GC(gjc), 3, _("Requesting Authentication Method"));
		gjab_reqauth(gjc);
		break;
	}
	return;
}

static void jabber_login(struct aim_user *user)
{
	struct gaim_connection *gc = new_gaim_conn(user);
	struct jabber_data *jd = gc->proto_data = g_new0(struct jabber_data, 1);
	char *loginname = create_valid_jid(user->username, DEFAULT_SERVER, "GAIM");

	jd->hash = g_hash_table_new(g_str_hash, g_str_equal);
	jd->chats = NULL;	/* we have no chats yet */

	set_login_progress(gc, 1, _("Connecting"));

	if (!(jd->gjc = gjab_new(loginname, user->password, gc))) {
		g_free(loginname);
		hide_login_progress(gc, _("Unable to connect"));
		signoff(gc);
		return;
	}

	g_free(loginname);
	gjab_state_handler(jd->gjc, jabber_handlestate);
	gjab_packet_handler(jd->gjc, jabber_handlepacket);
	jd->gjc->queries = g_hash_table_new(g_str_hash, g_str_equal);
	gjab_start(jd->gjc);
}

static gboolean jabber_destroy_hash(gpointer key, gpointer val, gpointer data) {
   	g_free(key);
	g_free(val);
	return TRUE;
}

static gboolean jabber_free(gpointer data)
{
	struct jabber_data *jd = data;

	if(jd->gjc != NULL) {
		gjab_delete(jd->gjc);
		g_free(jd->gjc->sid);
		jd->gjc = NULL;
	}
	g_free(jd);

	return FALSE;
}

static void jabber_close(struct gaim_connection *gc)
{
	struct jabber_data *jd = gc->proto_data;

	if(jd) {
		GSList *jcs = jd->chats;

		/* Free-up the jabber_chat struct allocs and the list */
		while (jcs) {
			g_free(jcs->data);
			jcs = jcs->next;
		}
		g_slist_free(jd->chats);

		/* Free-up the away status memories and the list */
		if(jd->hash != NULL) {
			g_hash_table_foreach_remove(jd->hash, jabber_destroy_hash, NULL);
			g_hash_table_destroy(jd->hash);
			jd->hash = NULL;
		}

		/* Free-up the pending queries memories and the list */
		if(jd->gjc != NULL && jd->gjc->queries != NULL) {
			g_hash_table_foreach_remove(jd->gjc->queries, jabber_destroy_hash, NULL);
			g_hash_table_destroy(jd->gjc->queries);
			jd->gjc->queries = NULL;
		}
	}
	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	if(jd) {
		g_timeout_add(50, jabber_free, jd);
		if(jd->gjc != NULL)
			xmlnode_free(jd->gjc->current);
	}
	gc->proto_data = NULL;
}

static int jabber_send_im(struct gaim_connection *gc, char *who, char *message, int len, int flags)
{
	xmlnode x, y;
	char *realwho;
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;

	if (!who || !message)
		return 0;

	x = xmlnode_new_tag("message");
	/* Bare username and "username" not the server itself? */
	if (!strchr(who, '@') && strcmp(who, gjc->user->server) != 0)
		realwho = g_strdup_printf("%s@%s", who, gjc->user->server);
	else
		realwho = g_strdup(who);
	xmlnode_put_attrib(x, "to", realwho);
	g_free(realwho);

	xmlnode_insert_tag(x, "bitlbee");
	xmlnode_put_attrib(x, "type", "chat");

	if (message && strlen(message)) {
		y = xmlnode_insert_tag(x, "body");
		xmlnode_insert_cdata(y, message, -1);
	}

	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);
	return 1;
}

/*
 * Add/update buddy's roster entry on server
 */
static void jabber_roster_update(struct gaim_connection *gc, char *name)
{
	xmlnode x, y;
	char *realwho;
	gjconn gjc;
	struct buddy *buddy = NULL;
	/* struct group *buddy_group = NULL; */
	
	if(gc && gc->proto_data && ((struct jabber_data *)gc->proto_data)->gjc && name) {
		gjc = ((struct jabber_data *)gc->proto_data)->gjc;

		if (!strchr(name, '@'))
			realwho = g_strdup_printf("%s@%s", name, gjc->user->server);
		else {
			jid who = jid_new(gjc->p, name);
			if (who->user == NULL) {
				/* FIXME: transport */
				return;
			}
			realwho = g_strdup_printf("%s@%s", who->user, who->server);
		}


		x = jutil_iqnew(JPACKET__SET, NS_ROSTER);
		y = xmlnode_insert_tag(xmlnode_get_tag(x, "query"), "item");
		xmlnode_put_attrib(y, "jid", realwho);


		/* If we can find the buddy, there's an alias for him, it's not 0-length
		 * and it doesn't match his JID, add the "name" attribute.
		 */
		if((buddy = find_buddy(gc, realwho)) != NULL &&
			buddy->show != NULL && buddy->show[0] != '\0' && strcmp(realwho, buddy->show)) {

			xmlnode_put_attrib(y, "name", buddy->show);
		}

		/*
		 * Find out what group the buddy's in and send that along
		 * with the roster item.
		 */
		/* ** Bitlbee disabled **
		if((buddy_group = NULL) != NULL) {
			xmlnode z;
			z = xmlnode_insert_tag(y, "group");
			xmlnode_insert_cdata(z, buddy_group->name, -1);
		}
		** End - Bitlbee ** */

		gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);

		xmlnode_free(x);
		g_free(realwho);
	}
}

/*
 * Change buddy's group on server roster
 */
static void jabber_group_change(struct gaim_connection *gc, char *name, char *old_group, char *new_group)
{
	if(strcmp(old_group, new_group)) {
		jabber_roster_update(gc, name);
	}
}

static void jabber_add_buddy(struct gaim_connection *gc, char *name)
{
	xmlnode x;
	char *realwho;
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;

	if (!((struct jabber_data *)gc->proto_data)->did_import)
		return;

	if (!name)
		return;

	if (!strcmp(gc->username, name))
		return;

	if (!strchr(name, '@'))
		realwho = g_strdup_printf("%s@%s", name, gjc->user->server);
	else {
		jid who;
		
		if((who = jid_new(gjc->p, name)) == NULL) {
			char *msg = g_strdup_printf("%s: \"%s\"", _("Invalid Jabber I.D."), name);
			do_error_dialog(msg, _("Jabber Error"));
			g_free(msg);
			jabber_remove_gaim_buddy(gc, name);
			return;
		}
		if (who->user == NULL) {
			/* FIXME: transport */
			return;
		}
		realwho = g_strdup_printf("%s@%s", who->user, who->server);
	}

	x = xmlnode_new_tag("presence");
	xmlnode_put_attrib(x, "to", realwho);
	xmlnode_put_attrib(x, "type", "subscribe");
	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);

	jabber_roster_update(gc, realwho);

	g_free(realwho);
}

static void jabber_remove_buddy(struct gaim_connection *gc, char *name, char *group)
{
	xmlnode x;
	char *realwho;
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;

	if (!name)
		return;

	if (!strchr(name, '@'))
		realwho = g_strdup_printf("%s@%s", name, gjc->user->server);
	else
		realwho = g_strdup(name);

	x = xmlnode_new_tag("presence");
	xmlnode_put_attrib(x, "to", realwho);
	xmlnode_put_attrib(x, "type", "unsubscribe");
	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	g_free(realwho);
	xmlnode_free(x);
}

/* **Chats not yet enabled - Bitlbee** ** NOTE! comments disabled **
static GList *jabber_chat_info(struct gaim_connection *gc)
{
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;

	static char *confserv = NULL;	/#* this pointer must be persistent *#/
	gchar *server;

	GList *m = NULL;
	struct proto_chat_entry *pce;

	/#* This is a scientific wild-ass guess...
	 *
	 * If there are more than two "components" to the current server name,
	 * lop-off the left-most component and replace with "conference."
	 *#/
	if(confserv != NULL) {
		g_free(confserv);	/#* dispose of the old value *#/
	}

	if((server = g_strdup(gjc->user->server)) == NULL) {
		confserv = g_strdup(DEFAULT_GROUPCHAT);
	} else {
		gchar **splits, **index;
		gchar *tmp;
		int cnt = 0;


		index = splits = g_strsplit(server, ".", -1);	/#* split the connected server *#/

		while(*(index++))	/#* index to the end--counting the parts *#/
			++cnt;

		/#*
		 * If we've more than two parts, point to the second part.  Else point
		 * to the start.
		 *#/
		if(cnt > 2) {
			index -= cnt;
		} else {
			index = splits;
		}

		/#* Put it together *#/
		confserv = g_strjoin(".", "conference", (tmp = g_strjoinv(".", index)), NULL);

		g_free(server);		/#* we don't need this stuff no more *#/
		g_free(tmp);
		g_strfreev(splits);
	}

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Room:");
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Server:");
	pce->def = confserv;
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Handle:");
	pce->def = gjc->user->user;
	m = g_list_append(m, pce);

	return m;
}

static void jabber_join_chat(struct gaim_connection *gc, GList *data)
{
	xmlnode x;
	char *realwho;
	gjconn gjc = ((struct jabber_data *)gc->proto_data)->gjc;
	GSList *jcs = ((struct jabber_data *)gc->proto_data)->chats;
	struct jabber_chat *jc;
	jid Jid;

	if (!data || !data->next || !data->next->next)
		return;

	realwho = create_valid_jid(data->data, data->next->data,
			data->next->next->data);

	if((Jid = jid_new(gjc->p, realwho)) == NULL) {
		char *msg = g_strdup_printf("%s: \"%s\"", _("Invalid Jabber I.D."), realwho);
		do_error_dialog(msg, _("Jabber Error"));
		g_free(msg);
		return;
	}

	if((jc = find_any_chat(gc, Jid)) != NULL) {
		free(Jid);	/#* don't need it, after all *#/
		switch(jc->state) {
			case JCS_PENDING:
				g_free(realwho);	/#* yuck! *#/
				return;
			case JCS_ACTIVE:
				g_free(realwho);	/#* yuck! *#/
				return;
			case JCS_CLOSED:
				break;
			default:
				g_free(realwho);	/#* yuck! *#/
				return;
		}
	} else {
		jc = g_new0(struct jabber_chat, 1);
		jc->Jid = Jid;
		jc->gc = gc;
		((struct jabber_data *)gc->proto_data)->chats = g_slist_append(jcs, jc);
	}

	jc->state = JCS_PENDING;

	x = jutil_presnew(0, realwho, NULL);
	gjab_send(gjc, x);
	xmlnode_free(x);
	g_free(realwho);
}

static void jabber_chat_invite(struct gaim_connection *gc, int id, char *message, char *name)
{
	xmlnode x, y;
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;
	struct jabber_chat *jc = NULL;
	char *realwho, *subject;

	if (!name)
		return;

	/#* find which chat we're inviting to *#/
	if(jabber_find_chat_by_convo_id(gc, id, &jc) != 0)
		return;

	x = xmlnode_new_tag("message");
	if (!strchr(name, '@'))
		realwho = g_strdup_printf("%s@%s", name, gjc->user->server);
	else
		realwho = g_strdup(name);
	xmlnode_put_attrib(x, "to", realwho);
	g_free(realwho);

	y = xmlnode_insert_tag(x, "x");
	xmlnode_put_attrib(y, "xmlns", "jabber:x:conference");
	subject = g_strdup_printf("%s@%s", jc->Jid->user, jc->Jid->server);
	xmlnode_put_attrib(y, "jid", subject);
	g_free(subject);

	if (message && strlen(message)) {
		char *utf8 = str_to_utf8(message);
		y = xmlnode_insert_tag(x, "body");
		xmlnode_insert_cdata(y, utf8, -1);
		g_free(utf8);
	}

	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);
}

static void jabber_chat_leave(struct gaim_connection *gc, int id)
{
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;
	struct jabber_chat *jc = NULL;
	char *realwho;
	xmlnode x;

	/#* Find out which chat we're leaving *#/
	if(jabber_find_chat_by_convo_id(gc, id, &jc) != 0)
		return;

	realwho = g_strdup_printf("%s@%s", jc->Jid->user, jc->Jid->server);
	x = jutil_presnew(0, realwho, NULL);
	g_free(realwho);
	xmlnode_put_attrib(x, "type", "unavailable");
	gjab_send(gjc, x);
	xmlnode_free(x);
	jc->b = NULL;
}

static int jabber_chat_send(struct gaim_connection *gc, int id, char *message)
{
	xmlnode x, y;
	struct jabber_chat *jc = NULL;
	char *chatname;
	int retval = 0;

	/#* Find out which chat we're sending to *#/
	if((retval = jabber_find_chat_by_convo_id(gc, id, &jc)) != 0)
		return(retval);

	x = xmlnode_new_tag("message");
	xmlnode_put_attrib(x, "from", jid_full(jc->Jid));
	chatname = g_strdup_printf("%s@%s", jc->Jid->user, jc->Jid->server);
	xmlnode_put_attrib(x, "to", chatname);
	g_free(chatname);
	xmlnode_put_attrib(x, "type", "groupchat");

	if (message && strlen(message) > strlen("/topic ") &&
			!g_strncasecmp(message, "/topic ", strlen("/topic "))) {
		char buf[8192];
		char *utf8 = str_to_utf8(message + strlen("/topic "));
		y = xmlnode_insert_tag(x, "subject");
		xmlnode_insert_cdata(y, utf8, -1);
		y = xmlnode_insert_tag(x, "body");
		g_snprintf(buf, sizeof(buf), "/me has changed the subject to: %s", utf8);
		xmlnode_insert_cdata(y, buf, -1);
		g_free(utf8);
	} else if (message && strlen(message)) {
		char *utf8 = str_to_utf8(message);
		y = xmlnode_insert_tag(x, "body");
		xmlnode_insert_cdata(y, utf8, -1);
		g_free(utf8);
	}

	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);
	return 0;
}

static void jabber_chat_whisper(struct gaim_connection *gc, int id, char *who, char *message)
{
	xmlnode x, y;
	struct jabber_chat *jc = NULL;
	char *chatname;

	/#* Find out which chat we're whispering to *#/
	if(jabber_find_chat_by_convo_id(gc, id, &jc) != 0)
		return;

	x = xmlnode_new_tag("message");
	xmlnode_put_attrib(x, "from", jid_full(jc->Jid));
	chatname = g_strdup_printf("%s@%s/%s", jc->Jid->user, jc->Jid->server, who);
	xmlnode_put_attrib(x, "to", chatname);
	g_free(chatname);
	xmlnode_put_attrib(x, "type", "normal");

	if (message && strlen(message)) {
		char *utf8 = str_to_utf8(message);
		y = xmlnode_insert_tag(x, "body");
		xmlnode_insert_cdata(y, utf8, -1);
		g_free(utf8);
	}

	gjab_send(((struct jabber_data *)gc->proto_data)->gjc, x);
	xmlnode_free(x);
}
** End of chat - Bitlbee ** */

static void jabber_get_info(struct gaim_connection *gc, char *who) {
	xmlnode x;
	char *id;
	char *realwho;
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;

	x = jutil_iqnew(JPACKET__GET, NS_VCARD);
	/* Bare username? */
	if (!strchr(who, '@')) {
		realwho = g_strdup_printf("%s@%s", who, gjc->user->server);
	} else {
		realwho = g_strdup(who);
	}
	xmlnode_put_attrib(x, "to", realwho);
	g_free(realwho);

	id = gjab_getid(gjc);
	xmlnode_put_attrib(x, "id", id);

	g_hash_table_insert(jd->gjc->queries, g_strdup(id), g_strdup("vCard"));

	gjab_send(gjc, x);

	xmlnode_free(x);
	
}

static void jabber_get_away_msg(struct gaim_connection *gc, char *who) {
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;
	char *status;

	/* space for all elements: Jabber I.D. + "status" + NULL (list terminator) */
	gchar **str_arr = (gchar **) g_new(gpointer, 3);
	gchar **ap = str_arr;
	gchar *realwho, *final;

	/* Bare username? */
	if (!strchr(who, '@')) {
		realwho = g_strdup_printf("%s@%s", who, gjc->user->server);
	} else {
		realwho = g_strdup(who);
	}
	*ap++ = g_strdup_printf("<B>Jabber ID:</B> %s<BR>\n", realwho);

	if((status = g_hash_table_lookup(jd->hash, realwho)) == NULL) {
		status = _("Unknown");
	}
	*ap++ = g_strdup_printf("<B>Status:</B> %s<BR>\n", status);

	*ap = NULL;

	final= g_strjoinv(NULL, str_arr);
	g_strfreev(str_arr);

	g_free(realwho);
	g_free(final);
	
}

/* ** Ooooh... What's this button do? - Bitlbee **
static void jabber_get_cb_info(struct gaim_connection *gc, int cid, char *who) {
	struct jabber_chat *jc = NULL;
	char *realwho;

	/#* Find out which chat *#/
	if(jabber_find_chat_by_convo_id(gc, cid, &jc) != 0)
		return;

	realwho = g_strdup_printf("%s@%s/%s", jc->Jid->user, jc->Jid->server, who);

	jabber_get_info(gc, realwho);
	g_free(realwho);
}

static void jabber_get_cb_away_msg(struct gaim_connection *gc, int cid, char *who) {
	struct jabber_chat *jc = NULL;
	char *realwho;

	/#* Find out which chat *#/
	if(jabber_find_chat_by_convo_id(gc, cid, &jc) != 0)
		return;

	realwho = g_strdup_printf("%s@%s/%s", jc->Jid->user, jc->Jid->server, who);

	jabber_get_away_msg(gc, realwho);

	g_free(realwho);

}
** End - Bitlbee ** */

static GList *jabber_away_states(struct gaim_connection *gc) {
	GList *m = NULL;

	m = g_list_append(m, "Online");
	m = g_list_append(m, "Chatty");
	m = g_list_append(m, "Away");
	m = g_list_append(m, "Extended Away");
	m = g_list_append(m, "Do Not Disturb");

	return m;
}

static void jabber_set_away(struct gaim_connection *gc, char *state, char *message)
{
	xmlnode x, y;
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;

	gc->away = NULL; /* never send an auto-response */

	x = xmlnode_new_tag("presence");

	if (!strcmp(state, GAIM_AWAY_CUSTOM)) {
		/* oh goody. Gaim is telling us what to do. */
		if (message) {
			/* Gaim wants us to be away */
			y = xmlnode_insert_tag(x, "show");
			xmlnode_insert_cdata(y, "away", -1);
			y = xmlnode_insert_tag(x, "status");
			{
				char *utf8 = str_to_utf8(message);
				xmlnode_insert_cdata(y, utf8, -1);
				g_free(utf8);
			}
			gc->away = "";
		} else {
			/* Gaim wants us to not be away */
			/* but for Jabber, we can just send presence with no other information. */
		}
	} else {
		/* state is one of our own strings. it won't be NULL. */
		if (!g_ascii_strcasecmp(state, "Online")) {
			/* once again, we don't have to put anything here */
		} else if (!g_ascii_strcasecmp(state, "Chatty")) {
			y = xmlnode_insert_tag(x, "show");
			xmlnode_insert_cdata(y, "chat", -1);
		} else if (!g_ascii_strcasecmp(state, "Away")) {
			y = xmlnode_insert_tag(x, "show");
			xmlnode_insert_cdata(y, "away", -1);
			gc->away = "";
		} else if (!g_ascii_strcasecmp(state, "Extended Away")) {
			y = xmlnode_insert_tag(x, "show");
			xmlnode_insert_cdata(y, "xa", -1);
			gc->away = "";
		} else if (!g_ascii_strcasecmp(state, "Do Not Disturb")) {
			y = xmlnode_insert_tag(x, "show");
			xmlnode_insert_cdata(y, "dnd", -1);
			gc->away = "";
		}
	}

	gjab_send(gjc, x);
	xmlnode_free(x);
}

static void jabber_set_idle(struct gaim_connection *gc, int idle) {
	struct jabber_data *jd = (struct jabber_data *)gc->proto_data;
   	jd->idle = idle ? time(NULL) - idle : idle;
}

static void jabber_keepalive(struct gaim_connection *gc) {
	struct jabber_data *jd = (struct jabber_data *)gc->proto_data;
	gjab_send_raw(jd->gjc, "  \t  ");
}

/* ** Bitlbee **
static GList *jabber_user_opts()
{
	GList *m = NULL;
	struct proto_user_opt *puo;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = "Port:";
	puo->def = "5222";
	puo->pos = USEROPT_PORT;
	m = g_list_append(m, puo);

	return m;
}
** End - Bitlbee ** */

static void jabber_buddy_free(struct buddy *b)
{
	while (b->proto_data) {
		g_free(((GSList *)b->proto_data)->data);
		b->proto_data = g_slist_remove(b->proto_data, ((GSList *)b->proto_data)->data);
	}
}

/*---------------------------------------*/
/* Jabber "set info" (vCard) support     */
/*---------------------------------------*/

/*
 * V-Card format:
 *
 *  <vCard prodid='' version='' xmlns=''>
 *    <FN></FN>
 *    <N>
 *	<FAMILY/>
 *	<GIVEN/>
 *    </N>
 *    <NICKNAME/>
 *    <URL/>
 *    <ADR>
 *	<STREET/>
 *	<EXTADD/>
 *	<LOCALITY/>
 *	<REGION/>
 *	<PCODE/>
 *	<COUNTRY/>
 *    </ADR>
 *    <TEL/>
 *    <EMAIL/>
 *    <ORG>
 *	<ORGNAME/>
 *	<ORGUNIT/>
 *    </ORG>
 *    <TITLE/>
 *    <ROLE/>
 *    <DESC/>
 *    <BDAY/>
 *  </vCard>
 *
 * See also:
 *
 *	http://docs.jabber.org/proto/html/vcard-temp.html
 *	http://www.vcard-xml.org/dtd/vCard-XML-v2-20010520.dtd
 */

/*
 * Cross-reference user-friendly V-Card entry labels to vCard XML tags
 * and attributes.
 *
 * Order is (or should be) unimportant.  For example: we have no way of
 * knowing in what order real data will arrive.
 *
 * Format: Label, Pre-set text, "visible" flag, "editable" flag, XML tag
 *         name, XML tag's parent tag "path" (relative to vCard node).
 *
 *         List is terminated by a NULL label pointer.
 *
 *	   Entries with no label text, but with XML tag and parent tag
 *	   entries, are used by V-Card XML construction routines to
 *	   "automagically" construct the appropriate XML node tree.
 *
 * Thoughts on future direction/expansion
 *
 *	This is a "simple" vCard.
 *
 *	It is possible for nodes other than the "vCard" node to have
 *      attributes.  Should that prove necessary/desirable, add an
 *      "attributes" pointer to the vcard_template struct, create the
 *      necessary tag_attr structs, and add 'em to the vcard_dflt_data
 *      array.
 *
 *	The above changes will (obviously) require changes to the vCard
 *      construction routines.
 */

static struct vcard_template {
	char *label;			/* label text pointer */
	char *text;			/* entry text pointer */
	int  visible;			/* should entry field be "visible?" */
	int  editable;			/* should entry field be editable? */
	char *tag;			/* tag text */
	char *ptag;			/* parent tag "path" text */
	char *url;			/* vCard display format if URL */
} vcard_template_data[] = {
	{N_("Full Name"),          NULL, TRUE, TRUE, "FN",        NULL,  NULL},
	{N_("Family Name"),        NULL, TRUE, TRUE, "FAMILY",    "N",   NULL},
	{N_("Given Name"),         NULL, TRUE, TRUE, "GIVEN",     "N",   NULL},
	{N_("Nickname"),           NULL, TRUE, TRUE, "NICKNAME",  NULL,  NULL},
	{N_("URL"),                NULL, TRUE, TRUE, "URL",       NULL,  "<A HREF=\"%s\">%s</A>"},
	{N_("Street Address"),     NULL, TRUE, TRUE, "STREET",    "ADR", NULL},
	{N_("Extended Address"),   NULL, TRUE, TRUE, "EXTADD",    "ADR", NULL},
	{N_("Locality"),           NULL, TRUE, TRUE, "LOCALITY",  "ADR", NULL},
	{N_("Region"),             NULL, TRUE, TRUE, "REGION",    "ADR", NULL},
	{N_("Postal Code"),        NULL, TRUE, TRUE, "PCODE",     "ADR", NULL},
	{N_("Country"),            NULL, TRUE, TRUE, "COUNTRY",   "ADR", NULL},
	{N_("Telephone"),          NULL, TRUE, TRUE, "TELEPHONE", NULL,  NULL},
	{N_("Email"),              NULL, TRUE, TRUE, "EMAIL",     NULL,  "<A HREF=\"mailto:%s\">%s</A>"},
	{N_("Organization Name"),  NULL, TRUE, TRUE, "ORGNAME",   "ORG", NULL},
	{N_("Organization Unit"),  NULL, TRUE, TRUE, "ORGUNIT",   "ORG", NULL},
	{N_("Title"),              NULL, TRUE, TRUE, "TITLE",     NULL,  NULL},
	{N_("Role"),               NULL, TRUE, TRUE, "ROLE",      NULL,  NULL},
	{N_("Birthday"),           NULL, TRUE, TRUE, "BDAY",      NULL,  NULL},
	{N_("Description"),        NULL, TRUE, TRUE, "DESC",      NULL,  NULL},
	{"", NULL, TRUE, TRUE, "N",     NULL, NULL},
	{"", NULL, TRUE, TRUE, "ADR",   NULL, NULL},
	{"", NULL, TRUE, TRUE, "ORG",   NULL, NULL},
	{NULL, NULL, 0, 0, NULL, NULL, NULL}
};

/*
 * V-Card user instructions
 */
/* ** BitlBee ** Not needed now
static char *multi_entry_instructions =
	N_("All items below are optional. Enter only the information with which you feel comfortable");
static char *entries_title = N_("User Identity");
** End of BitlBee ** */

/*
 * Used by routines to parse an XML-encoded string into an xmlnode tree
 */
typedef struct {
	XML_Parser parser;
	xmlnode current;
} *xmlstr2xmlnode_parser, xmlstr2xmlnode_parser_struct;


/*
 * Used by XML_Parse on parsing CDATA
 */
static void xmlstr2xmlnode_charData(void *userdata, const char *s, int slen)
{
	xmlstr2xmlnode_parser xmlp = (xmlstr2xmlnode_parser) userdata;

	if (xmlp->current)
		xmlnode_insert_cdata(xmlp->current, s, slen);
}

/*
 * Used by XML_Parse to start or append to an xmlnode
 */
static void xmlstr2xmlnode_startElement(void *userdata, const char *name, const char **attribs)
{
	xmlnode x;
	xmlstr2xmlnode_parser xmlp = (xmlstr2xmlnode_parser) userdata;

	if (xmlp->current) {
		/* Append the node to the current one */
		x = xmlnode_insert_tag(xmlp->current, name);
		xmlnode_put_expat_attribs(x, attribs);

		xmlp->current = x;
	} else {
		x = xmlnode_new_tag(name);
		xmlnode_put_expat_attribs(x, attribs);
		xmlp->current = x;
	}
}

/*
 * Used by XML_Parse to end an xmlnode
 */
static void xmlstr2xmlnode_endElement(void *userdata, const char *name)
{
	xmlstr2xmlnode_parser xmlp = (xmlstr2xmlnode_parser) userdata;
	xmlnode x;

	if (xmlp->current != NULL && (x = xmlnode_get_parent(xmlp->current)) != NULL) {
		xmlp->current = x;
	}
}

/*
 * Parse an XML-encoded string into an xmlnode tree
 *
 * Caller is responsible for freeing the returned xmlnode
 */
static xmlnode xmlstr2xmlnode(char *xmlstring)
{
	xmlstr2xmlnode_parser my_parser = g_new(xmlstr2xmlnode_parser_struct, 1);
	xmlnode x = NULL;

	my_parser->parser = XML_ParserCreate(NULL);
	my_parser->current = NULL;

	XML_SetUserData(my_parser->parser, (void *)my_parser);
	XML_SetElementHandler(my_parser->parser, xmlstr2xmlnode_startElement, xmlstr2xmlnode_endElement);
	XML_SetCharacterDataHandler(my_parser->parser, xmlstr2xmlnode_charData);
	XML_Parse(my_parser->parser, xmlstring, strlen(xmlstring), 0);

	x = my_parser->current;

	XML_ParserFree(my_parser->parser);
	g_free(my_parser);

	return(x);
}

/*
 * Insert a tag node into an xmlnode tree, recursively inserting parent tag
 * nodes as necessary
 *
 * Returns pointer to inserted node
 *
 * Note to hackers: this code is designed to be re-entrant (it's recursive--it
 * calls itself), so don't put any "static"s in here!
 */
static xmlnode insert_tag_to_parent_tag(xmlnode start, const char *parent_tag, const char *new_tag)
{
	xmlnode x = NULL;

	/*
	 * If the parent tag wasn't specified, see if we can get it
	 * from the vCard template struct.
	 */
	if(parent_tag == NULL) {
		struct vcard_template *vc_tp = vcard_template_data;

		while(vc_tp->label != NULL) {
			if(strcmp(vc_tp->tag, new_tag) == 0) {
				parent_tag = vc_tp->ptag;
				break;
			}
			++vc_tp;
		}
	}

	/*
	 * If we have a parent tag...
	 */
	if(parent_tag != NULL ) {
		/*
		 * Try to get the parent node for a tag
		 */
		if((x = xmlnode_get_tag(start, parent_tag)) == NULL) {
			/*
			 * Descend?
			 */
			char *grand_parent = strcpy(g_malloc(strlen(parent_tag) + 1), parent_tag);
			char *parent;

			if((parent = strrchr(grand_parent, '/')) != NULL) {
				*(parent++) = '\0';
				x = insert_tag_to_parent_tag(start, grand_parent, parent);
			} else {
				x = xmlnode_insert_tag(start, grand_parent);
			}
			g_free(grand_parent);
		} else {
			/*
			 * We found *something* to be the parent node.
			 * Note: may be the "root" node!
			 */
			xmlnode y;
			if((y = xmlnode_get_tag(x, new_tag)) != NULL) {
				return(y);
			}
		}
	}

	/*
	 * insert the new tag into its parent node
	 */
	return(xmlnode_insert_tag((x == NULL? start : x), new_tag));
}

/*
 * Find the tag name for a label
 *
 * Returns NULL on not found
 */
/* ** BitlBee ** Not needed now
static char *tag_for_label(const char *label)
{
	struct vcard_template *vc_tp = vcard_template_data;
	char *p = NULL;

	for(vc_tp = vcard_template_data; vc_tp->label != NULL; ++vc_tp) {
		if(strcmp(label, vc_tp->label) == 0) {
			p = vc_tp->tag;
			break;
		}
	}

	return(p);
}
** End of BitlBee ** */

/*
 * Send vCard info to Jabber server
 */
static void jabber_set_info(struct gaim_connection *gc, char *info)
{
	xmlnode x, vc_node;
	char *id;
	struct jabber_data *jd = gc->proto_data;
	gjconn gjc = jd->gjc;

	x = xmlnode_new_tag("iq");
	xmlnode_put_attrib(x,"type","set");

	id = gjab_getid(gjc);
	
	xmlnode_put_attrib(x, "id", id);

	/*
	 * Send only if there's actually any *information* to send
	 */
	if((vc_node = xmlstr2xmlnode(info)) != NULL && xmlnode_get_name(vc_node) != NULL &&
			g_strncasecmp(xmlnode_get_name(vc_node), "vcard", 5) == 0) {
		xmlnode_insert_tag_node(x, vc_node);
		gjab_send(gjc, x);
	}

	xmlnode_free(x);
}

/*
 * This is the callback from the "ok clicked" for "set vCard"
 *
 * Formats GSList data into XML-encoded string and returns a pointer
 * to said string.
 *
 * g_free()'ing the returned string space is the responsibility of
 * the caller.
 */
/** Bitlbee **
static gchar *jabber_format_info (MultiEntryDlg *b)
{
	xmlnode vc_node;
	GSList *list;
	MultiEntryData *med;
	MultiTextData *mtd;
	char *p;

	struct tag_attr *tag_attr;

	vc_node = xmlnode_new_tag("vCard");

	for(tag_attr = vcard_tag_attr_list; tag_attr->attr != NULL; ++tag_attr)
		xmlnode_put_attrib(vc_node, tag_attr->attr, tag_attr->value);

	for(list = b->multi_entry_items; list != NULL; list = list->next) {
		med = (MultiEntryData *) list->data;
		if(med->label != NULL && med->text != NULL && (med->text)[0] != '\0') {
			if((p = tag_for_label(med->label)) != NULL) {
				xmlnode xp;
				if((xp = insert_tag_to_parent_tag(vc_node, NULL, p)) != NULL) {
					xmlnode_insert_cdata(xp, med->text, -1);
				}
			}
		}
	}

	for(list = b->multi_text_items; list != NULL; list = list->next) {
		mtd = (MultiTextData *) list->data;
		if(mtd->label != NULL && mtd->text != NULL && (mtd->text)[0] != '\0') {
			if((p = tag_for_label(mtd->label)) != NULL) {
				xmlnode xp;
				if((xp = insert_tag_to_parent_tag(vc_node, NULL, p)) != NULL) {
					xmlnode_insert_cdata(xp, mtd->text, -1);
				}
			}
		}
	}


	p = g_strdup(xmlnode2str(vc_node));
	xmlnode_free(vc_node);

	return(p);
}

/#*
 * This gets executed by the proto action
 *
 * Creates a new MultiEntryDlg struct, gets the XML-formatted user_info
 * string (if any) into GSLists for the (multi-entry) edit dialog and
 * calls the set_vcard dialog.
 *#/
static void jabber_setup_set_info(struct gaim_connection *gc)
{
	MultiEntryData *data;
	const struct vcard_template *vc_tp;
	char *user_info;
	MultiEntryDlg *b = multi_entry_dialog_new();
	char *cdata;
	xmlnode x_vc_data = NULL;
	struct aim_user *tmp = gc->user;
	b->user = tmp;


	/#*
	 * Get existing, XML-formatted, user info
	 *#/
	if((user_info = g_malloc(strlen(tmp->user_info) + 1)) != NULL) {
		strcpy(user_info, tmp->user_info);
		x_vc_data = xmlstr2xmlnode(user_info);
	}

	/#*
	 * Set up GSLists for edit with labels from "template," data from user info
	 *#/
	for(vc_tp = vcard_template_data; vc_tp->label != NULL; ++vc_tp) {
		if((vc_tp->label)[0] == '\0')
			continue;
		if(vc_tp->ptag == NULL) {
			cdata = xmlnode_get_tag_data(x_vc_data, vc_tp->tag);
		} else {
			gchar *tag = g_strdup_printf("%s/%s", vc_tp->ptag, vc_tp->tag);
			cdata = xmlnode_get_tag_data(x_vc_data, tag);
			g_free(tag);
		}
		if(strcmp(vc_tp->tag, "DESC") == 0) {
			multi_text_list_update(&(b->multi_text_items),
				vc_tp->label, cdata, TRUE);
		} else {
			data = multi_entry_list_update(&(b->multi_entry_items),
				vc_tp->label, cdata, TRUE);
			data->visible = vc_tp->visible;
			data->editable = vc_tp->editable;
		}
	}


	if(x_vc_data != NULL) {
		xmlnode_free(x_vc_data);
	} else {
		/#*
		 * Early Beta versions had a different user_info storage format--let's
		 * see if that works.
		 *
		 * This goes away RSN.
		 *#/
		const char *record_separator = "<BR>";
		const char *field_separator = ": ";
		gchar **str_list, **str_list_ptr, **str_list2;

		if((str_list = g_strsplit(user_info, record_separator, 0)) != NULL) {
			for(str_list_ptr = str_list; *str_list_ptr != NULL; ++str_list_ptr) {
				str_list2 = g_strsplit(*str_list_ptr, field_separator, 2);
				if(str_list2[0] != NULL && str_list2[1] != NULL) {
					g_strstrip(str_list2[0]);
					g_strstrip(str_list2[1]);
					/#* this is ugly--so far *#/
					if(strcmp(str_list2[0], "Description") == 0) {
						multi_text_list_update(&(b->multi_text_items),
							str_list2[0], str_list2[1], FALSE);
					} else {
						multi_entry_list_update(&(b->multi_entry_items),
							str_list2[0], str_list2[1], FALSE);
					}
				}
				g_strfreev(str_list2);
			}
			g_strfreev(str_list);
		}
	}

	if(user_info != NULL) {
		g_free(user_info);
	}

	b->title = _("Gaim - Edit Jabber vCard");
	b->wmclass_name = "set_info";
	b->wmclass_class = "Gaim";
	b->instructions->text = g_strdup(multi_entry_instructions);
	b->entries_title = g_strdup(entries_title);

	b->custom = (void *) jabber_format_info;

	show_set_vcard(b);
}
** End - Bitlbee ** */
/*---------------------------------------*/
/* End Jabber "set info" (vCard) support */
/*---------------------------------------*/

/* ** Bitlbee **
static void jabber_do_action(struct gaim_connection *gc, char *act)
{
	if (!strcmp(act, _("Set User Info"))) {
		jabber_setup_set_info(gc);
	/#*
	} else if (!strcmp(act, _("Set Dir Info"))) {
		show_set_dir(gc);
	} else if (!strcmp(act, _("Change Password"))) {
		show_change_passwd(gc);
	 *#/
	}
}
** End - Bitlbee ** */

static GList *jabber_actions()
{
	GList *m = NULL;

	m = g_list_append(m, _("Set User Info"));
	/*
	m = g_list_append(m, _("Set Dir Info"));
	m = g_list_append(m, _("Change Password"));
	 */

	return m;
}

static struct prpl *my_protocol = NULL;

void jabber_init(struct prpl *ret)
{
	/* the NULL's aren't required but they're nice to have */
	ret->protocol = PROTO_JABBER;
/* ** Bitlbee **
	ret->options = OPT_PROTO_UNIQUE_CHATNAME | OPT_PROTO_CHAT_TOPIC;
** End - Bitlbee ** */
	ret->name = jabber_name;
	ret->away_states = jabber_away_states;
	ret->actions = jabber_actions;
/* ** Bitlbee **
	ret->do_action = jabber_do_action;
	ret->user_opts = jabber_user_opts;
** End - Bitlbee ** */
	ret->login = jabber_login;
	ret->close = jabber_close;
	ret->send_im = jabber_send_im;
	ret->set_info = jabber_set_info;
	ret->get_info = jabber_get_info;
/* ** Hmmm? - Bitlbee **
	ret->get_cb_info = jabber_get_cb_info;
	ret->get_cb_away = jabber_get_cb_away_msg;
** End - Bitlbee ** */
	ret->set_away = jabber_set_away;
	ret->get_away = jabber_get_away_msg;
/* ** Bitlbee **
	ret->set_dir = NULL;
	ret->get_dir = NULL;
	ret->dir_search = NULL;
** End - Bitlbee ** */
	ret->set_idle = jabber_set_idle;
/* ** Bitlbee **
	ret->change_passwd = NULL;
** End - Bitlbee ** */
	ret->add_buddy = jabber_add_buddy;
	ret->remove_buddy = jabber_remove_buddy;
	ret->add_permit = NULL;
	ret->add_deny = NULL;
	ret->rem_permit = NULL;
	ret->rem_deny = NULL;
	ret->set_permit_deny = NULL;
/* ** Bitlbee **
	ret->warn = NULL;
** End - Bitlbee ** */
/* ** Chat not yet implemented - Bitlbee **
	ret->chat_info = jabber_chat_info;
	ret->join_chat = jabber_join_chat;
	ret->chat_invite = jabber_chat_invite;
	ret->chat_leave = jabber_chat_leave;
	ret->chat_whisper = jabber_chat_whisper;
	ret->chat_send = jabber_chat_send;
** End of chat stuff - Bitlbee ** */
	ret->keepalive = jabber_keepalive;
//	ret->normalize = jabber_normalize;
	ret->buddy_free = jabber_buddy_free;
	ret->alias_buddy = jabber_roster_update;
	ret->group_buddy = jabber_group_change;

	my_protocol = ret;
}
