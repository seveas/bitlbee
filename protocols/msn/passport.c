/* passport.c
 *
 * Functions to login to microsoft passport service for Messenger
 * Copyright (C) 2004 Wouter Paesen <wouter@blue-gate.be>
 *
 * This program is free software; you can redistribute it and/or modify             
 * it under the terms of the GNU General Public License version 2                   
 * as published by the Free Software Foundation                                     
 *                                                                                   
 * This program is distributed in the hope that is will be useful,                  
 * bit WITHOU ANY WARRANTY; without even the implied warranty of                   
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    
 * GNU General Public License for more details.                                     
 *                                                                                   
 * You should have received a copy of the GNU General Public License                
 * along with this program; if not, write to the Free Software                      
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA          
 *
 */

#include "passport.h"
#include <ctype.h>
#include <errno.h>

#define __DEBUG__
#define MSN_BUF_LEN 8192

#ifdef __DEBUG__
void do_error_dialog(char *msg, char *title);
#define __log(msg) do_error_dialog( msg, "MSN" );
#else
#define __log(msg)
#endif

char *url_decode(const char *msg)
{
	static char buf[MSN_BUF_LEN];
	int i, j = 0;

	bzero(buf, sizeof(buf));
	for (i = 0; i < strlen(msg); i++) {
		char hex[3];
		if (msg[i] != '%') {
			buf[j++] = msg[i];
			continue;
		}
		strncpy(hex, msg + ++i, 2);
		hex[2] = 0;
		/* i is pointing to the start of the number */
		i++;		/* now it's at the end and at the start of the for loop
				   will be at the next character */
		buf[j++] = strtol(hex, NULL, 16);
	}
	buf[j] = 0;

	return buf;
}

char *url_encode(const char *msg)
{
	static char buf[MSN_BUF_LEN];
	int i, j = 0;

	bzero(buf, sizeof(buf));
	for (i = 0; i < strlen(msg); i++) {
		if (isalnum(msg[i]))
			buf[j++] = msg[i];
		else {
			sprintf(buf + j, "%%%02x", (unsigned char) msg[i]);
			j += 3;
		}
	}
	buf[j] = 0;

	return buf;
}

/* Connects to the peer and returns a socket
 * descriptor.
 */
int tcp_connect(const char *SERVER, const char *PORT)
{
	int err, sd;
	struct hostent *hostaddress;
	struct sockaddr_in sa;

	/* resolve the hostname */
	hostaddress = gethostbyname(SERVER);
	if (hostaddress == NULL) {
		__log("Hostname unresolvable(001)");
		return -1;
	}

	/* prepare socket */
	sd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&sa, '\0', sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(atoi(PORT));
	memcpy(&sa.sin_addr, hostaddress->h_addr_list[0],
	       hostaddress->h_length);

	/* unleash the beast */
	err = connect(sd, (struct sockaddr *) &sa, sizeof(sa));
	if (err < 0) {
		__log("Could not connect to host(002)");
		return -1;
	}
	return sd;
}

void tcp_disconnect(int sd)
{
	shutdown(sd, SHUT_RDWR);	/* no more receptions */
	close(sd);
}

#define PPR_BUFFERSIZE 2048
#define PPR_REQUEST "GET /rdr/pprdr.asp HTTP/1.0\r\n\r\n"
char *passport_retrieve_dalogin()
{
	int ret, sd;
	char *retv = NULL;
	char *buffer = NULL;
	gnutls_certificate_credentials xcred;
	gnutls_session session;

	buffer = (char *) malloc(PPR_BUFFERSIZE);
	if (buffer == NULL) {
		return NULL;
	}

	/* X509 stuff */
	gnutls_certificate_allocate_credentials(&xcred);
	/* initialize TLS session */
	gnutls_init(&session, GNUTLS_CLIENT);
	/* use the default priorities */
	gnutls_set_default_priority(session);
	gnutls_certificate_type_set_priority(session,
					     pp_cert_type_priority);
	/* put the x509 credentials to the current session */
	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);
	/* create a connection to the nexus */
	sd = tcp_connect("nexus.passport.com", "443");
	/* associate the connection to the session */
	gnutls_transport_set_ptr(session, (gnutls_transport_ptr) sd);
	/* perform the TLS handshake */
	ret = gnutls_handshake(session);

	if (ret < 0) {
		__log("Handshake unsuccessful(003)");
		goto end;
	}

	gnutls_record_send(session, PPR_REQUEST, strlen(PPR_REQUEST));

	ret = gnutls_record_recv(session, buffer, PPR_BUFFERSIZE);

	if (ret < 0) {
		__log("Server did not respond(004)");
		goto end;
	}

	/* parse the header to extract the login server */
	{
		char *dalogin = strstr(buffer, "DALogin=");
		char *urlend;
		dalogin += strlen("DALogin=");
		urlend = dalogin;

		urlend = strchr(dalogin, ',');
		if (urlend)
			*urlend = 0;

		/* strip the http(s):// part from the url */
		urlend = strstr(urlend, "://");
		if (urlend)
			dalogin = urlend + strlen("://");

		retv = strdup(dalogin);
	}

	gnutls_bye(session, GNUTLS_SHUT_RDWR);

      end:
	free(buffer);
	tcp_disconnect(sd);
	gnutls_deinit(session);
	gnutls_certificate_free_credentials(xcred);

	return retv;
}

char *passport_create_header(char *reply, char *email, char *pwd)
{
	char *buffer = malloc(2048);
	char *currenttoken;

	currenttoken = strstr(reply, "lc=");
	if (currenttoken == NULL)
		return NULL;

	snprintf(buffer, 2048,
		 "Authorization: Passport1.4 OrgVerb=GET,"
		 "OrgURL=http%%3A%%2F%%2Fmessenger%%2Emsn%%2Ecom,"
		 "sign-in=%s,pwd=%s,%s", url_encode(email), pwd,
		 currenttoken);

	return buffer;
}

#define PPG_BUFFERSIZE 4096
char *passport_get_id(char *header_i, char *url)
{
	int ret, sd;
	char *retv = NULL;
	char server[512];

	char *buffer = NULL;
	char *dummy;
	int redirects = 0;

	gnutls_certificate_credentials xcred;
	gnutls_session session;

	buffer = (char *) malloc(PPG_BUFFERSIZE + 1);
	if (buffer == NULL) {
		return NULL;
	}
	strncpy(server, url, 512);

      redirect:
	++redirects;

	dummy = strchr(server, '/');
	if (dummy)
		*dummy = 0;

	sd = tcp_connect(server, "443");
	
	gnutls_certificate_allocate_credentials(&xcred);
	gnutls_init(&session, GNUTLS_CLIENT);
	gnutls_set_default_priority(session);
	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);
	gnutls_transport_set_ptr(session, (gnutls_transport_ptr) sd);
	
	ret = gnutls_handshake(session);

	if (ret < 0) {
		__log("Handshake unsuccessful(005)");
		tcp_disconnect(sd);
		goto end;
	}

	snprintf(buffer, PPG_BUFFERSIZE - 1, "GET /%s HTTP/1.0\r\n"
		 "%s\r\n\r\n", dummy + 1, header_i);

	gnutls_record_send(session, buffer, strlen(buffer));
	memset(buffer, 0, PPG_BUFFERSIZE + 1);

	{
		char *buffer2 = buffer;
		ret = gnutls_record_recv(session, buffer2, BUFFER_SIZE);

		while ((ret > 0) && (buffer + PPG_BUFFERSIZE - buffer2 - ret - 512 >= 0)) {
			buffer2 += ret;
			ret = gnutls_record_recv(session, buffer2, 512);
		}
		
		gnutls_bye(session, GNUTLS_SHUT_WR);
		tcp_disconnect(sd);
	}

	if (*buffer == 0) {
		__log("Server did not respond(006)");
		goto end;
	}

	/* test for a redirect */
	dummy = strstr(buffer, "Location:");
	if (dummy != NULL) {
		char *urlend;
		dummy += strlen("Location:");
		// need to redirect, parse the redirect request, and goto
		while (isspace(*dummy))
			++dummy;
		// now dummy pointer to the start of the url
		urlend = dummy;
		while (!isspace(*urlend))
			++urlend;
		*urlend = 0;
		urlend = strstr(dummy, "://");
		if (urlend)
			dummy = urlend + strlen("://");

		strncpy(server, dummy, 512);

		if (redirects > 10) {
			__log("Too many redirect(007)");
			goto end;
		}
		gnutls_deinit(session);
		gnutls_certificate_free_credentials(xcred);
		goto redirect;
	}

	/* no redirect found, check the response code */
	if (strstr(buffer, "200 OK")) {
		char *responseend;
		dummy = strstr(buffer, "from-PP='");
		if (dummy) {
			dummy += strlen("from-PP='");
			responseend = strchr(dummy, '\'');
			if (responseend)
				*responseend = 0;
			retv = strdup(dummy);
		}
	}

      end:
	gnutls_deinit(session);
	gnutls_certificate_free_credentials(xcred);
	free(buffer);
	
	return retv;
}
