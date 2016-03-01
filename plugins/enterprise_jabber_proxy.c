/* This plugin turns BitlBee into an enterprise Jabber proxy. It is best used
 * with AccountStorage=ldap and achieves the following:
 *
 * - When users log in for the first time, their Jabber account is created.
 *   This queries LDAP for their e-mail address and uses that as JID. 
 * - The plugin assumes bitlbee password == jabber password and disables
 *   storing of the jabber password.
 * - The display name for the new account is copied from LDAP
 * - When joining a new channel, the plugin assumes that you intend to join a
 *   the jabber room $channel@conference.$domain and sets up the channel
 *   (auto_join, room, nick)
 */

#include "bitlbee.h"
#define LDAP_DEPRECATED 1
#include <ldap.h>

static void ldap_find_data(const char *nick, char **mail, char **name)
{
	LDAP *ldap;
	LDAPMessage *msg, *entry;
	char *filter, **values;
	char *attrs[3] = { "mail", "cn", NULL };
	int ret, count;

	if((ret = ldap_initialize(&ldap, NULL)) != LDAP_SUCCESS) {
		log_message(LOGLVL_WARNING, "ldap_initialize failed: %s", ldap_err2string(ret));
		return;
	}

	if((ret = ldap_simple_bind_s(ldap, NULL, NULL)) != LDAP_SUCCESS) {
		ldap_unbind_s(ldap);
		log_message(LOGLVL_WARNING, "Anonymous bind failed: %s", ldap_err2string(ret));
		return;
	}

	/* We search and process the result */
	filter = g_strdup_printf("(uid=%s)", nick);
	ret = ldap_search_ext_s(ldap, NULL, LDAP_SCOPE_SUBTREE, filter, attrs, 0, NULL, NULL, NULL, 1, &msg);
	g_free(filter);

	if(ret != LDAP_SUCCESS) {
		ldap_unbind_s(ldap);
		log_message(LOGLVL_WARNING, "uid search failed: %s", ldap_err2string(ret));
		return;
	}

	count = ldap_count_entries(ldap, msg);
	if (count == -1) {
		ldap_get_option(ldap, LDAP_OPT_ERROR_NUMBER, &ret);
		ldap_msgfree(msg);
		ldap_unbind_s(ldap);
		log_message(LOGLVL_WARNING, "uid search failed: %s", ldap_err2string(ret));
		return;
	}

	if (!count) {
		ldap_msgfree(msg);
		ldap_unbind_s(ldap);
		return;
	}

	entry = ldap_first_entry(ldap, msg);

	values = ldap_get_values(ldap, entry, "mail");
	if(values) {
		if (ldap_count_values(values))
			*mail = g_strdup(values[0]);
		ldap_value_free(values);
	}

	values = ldap_get_values(ldap, entry, "cn");
	if(values) {
		if (ldap_count_values(values))
			*name = g_strdup(values[0]);
		ldap_value_free(values);
	}

	ldap_msgfree(msg);
}

static void jabber_storage_load(irc_t *irc)
{
	account_t *acc;
	char *jid = NULL, *name = NULL, *domain = NULL;
	struct prpl *prpl;

	ldap_find_data(irc->user->nick, &jid, &name);

	if(!jid) {
		irc_rootmsg(irc, "Could not find your JID in LDAP, no account added");
		return;
	}

	for (acc=irc->b->accounts; acc; acc=acc->next) {
		if(!strcmp(acc->user, jid)) {
			g_free(acc->pass);
			acc->pass = g_strdup(irc->password);
			break;
		}
	}
	if (!acc) {
		prpl = find_protocol("jabber");
		acc = account_add(irc->b, prpl, jid, irc->password);
		acc->flags |= ACC_FLAG_LOCKED;
		set_setstr(&acc->set, "auto_connect", "true");
		if(name && *name)
			set_setstr(&acc->set, "display_name", name);
		domain = g_strdup_printf("conference.jabber.%s", strstr(jid, "@")+1);
		set_setstr(&acc->set, "conference_domain", domain);
		g_free(domain);
		set_setstr(&irc->b->set, "conference_account" , acc->tag);
	}
	acc->flags |= ACC_FLAG_DONT_SAVE_PASSWORD;
}

gboolean jabber_irc_new(irc_t *irc)
{
	set_add(&irc->b->set, "conference_account", "", NULL, irc);
	return TRUE;
}

void jabber_account_add(account_t *acc)
{
	if (!strcmp(acc->prpl->name, "jabber"))
	{
		set_add(&acc->set, "conference_domain", "", NULL, acc);
	}
}

void jabber_channel_new(irc_channel_t *ic)
{
	/* XXX segfaults when trying to /join before auth */
	char *account, *domain, *nick, *room;
	account_t *acc;

	if (strcmp(set_getstr(&ic->set, "type"), "chat") != 0)
		return;

	/* We joined a room. Make it point to the right conference */
	account = set_getstr(&ic->irc->b->set, "conference_account");
	if(!(account && *account))
		return;

	acc = account_get(ic->irc->b, account);
	if(!acc)
		return;

	nick = set_getstr(&acc->set, "display_name");
	domain = set_getstr(&acc->set, "conference_domain");
	if(!(domain && *domain))
		return;

	set_setstr(&ic->set, "chat_type", "room");
	set_setstr(&ic->set, "auto_join", "true");
	if(nick && *nick)
		set_setstr(&ic->set, "nick", nick);
	room = g_strdup_printf("%s@%s", ic->name+1, domain);
	set_setstr(&ic->set, "room", room);
	g_free(room);
	set_setstr(&ic->set, "account", account);
}

static const struct irc_plugin jabber_proxy_plugin =
{
	.irc_new = jabber_irc_new,
	.storage_load = jabber_storage_load,
	.account_add = jabber_account_add,
	.channel_new = jabber_channel_new,
};

void init_plugin(void)
{
	register_irc_plugin(&jabber_proxy_plugin);
}
