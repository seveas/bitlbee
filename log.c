  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2004 Wilmer van der Gaast and others                *
  \********************************************************************/

/* Logging services for the bee 			*/

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

static log_t logoutput;

static void log_null(int level, char *logmessage);
static void log_irc(int level, char *logmessage);
static void log_syslog(int level, char *logmessage);
static void log_console(int level, char *logmessage);

void log_init(void) {
	openlog("bitlbee", LOG_PID, LOG_DAEMON);	

	logoutput.informational=&log_null;
	logoutput.warning=&log_null;
	logoutput.error=&log_null;
#ifdef DEBUG
	logoutput.debug=&log_null;
#endif

	return;
}

void log_link(int level, int output) {
	/* I know it's ugly, but it works and I didn't feel like messing with pointer to function pointers */

	if(level==LOGLVL_INFO) {
		if(output==LOGOUTPUT_NULL)
			logoutput.informational=&log_null;	
		else if(output==LOGOUTPUT_IRC)
			logoutput.informational=&log_irc;	
		else if(output==LOGOUTPUT_SYSLOG)
			logoutput.informational=&log_syslog;	
		else if(output==LOGOUTPUT_CONSOLE) 
			logoutput.informational=&log_console;	
	}
	else if(level==LOGLVL_WARNING) {
		if(output==LOGOUTPUT_NULL)
			logoutput.warning=&log_null;
		else if(output==LOGOUTPUT_IRC)
			logoutput.warning=&log_irc;
		else if(output==LOGOUTPUT_SYSLOG)
			logoutput.warning=&log_syslog;
		else if(output==LOGOUTPUT_CONSOLE)
			logoutput.warning=&log_console;
	}
	else if(level==LOGLVL_ERROR) {
		if(output==LOGOUTPUT_NULL)
			logoutput.error=&log_null;
		else if(output==LOGOUTPUT_IRC)
			logoutput.error=&log_irc;
		else if(output==LOGOUTPUT_SYSLOG)
			logoutput.error=&log_syslog;
		else if(output==LOGOUTPUT_CONSOLE)
			logoutput.error=&log_console;
	}
#ifdef DEBUG
	else if(level==LOGLVL_DEBUG) {
		if(output==LOGOUTPUT_NULL)
			logoutput.debug=&log_null;
		else if(output==LOGOUTPUT_IRC)
			logoutput.debug=&log_irc;
		else if(output==LOGOUTPUT_SYSLOG)
			logoutput.debug=&log_syslog;
		else if(output==LOGOUTPUT_CONSOLE)
			logoutput.debug=&log_console;
	}
#endif
	return;	

}

void log_message(int level, char *message, ... ) {

	va_list ap;
	char *msgstring;

	va_start(ap, message);
	msgstring=my_vasprintf(message, ap);
	va_end(ap);

	if(level==LOGLVL_INFO)
		(*(logoutput.informational))(level, msgstring);
	if(level==LOGLVL_WARNING) 
		(*(logoutput.warning))(level, msgstring);
	if(level==LOGLVL_ERROR)
		(*(logoutput.error))(level, msgstring);
#ifdef DEBUG
	if(level==LOGLVL_DEBUG)
		(*(logoutput.debug))(level, msgstring);
#endif

	free(msgstring);
	
	return;
}

void log_error(char *functionname) {
	log_message(LOGLVL_ERROR, "%s: %s", functionname, strerror(errno));
	
	return;
}

static void log_null(int level, char *message) {
	return;
}

static void log_irc(int level, char *message) {
	if(level==LOGLVL_ERROR)
		irc_write_all("ERROR :Error: %s", message);
	if(level==LOGLVL_WARNING)
		irc_write_all("ERROR :Warning: %s", message);
	if(level==LOGLVL_INFO)
		irc_write_all("ERROR :Informational: %s", message);	
#ifdef DEBUG
	if(level==LOGLVL_DEBUG)
		irc_write_all("ERROR :Debug: %s", message);	
#endif	

	return;
}

static void log_syslog(int level, char *message) {
	if(level==LOGLVL_ERROR)
		syslog(LOG_ERR, message);
	if(level==LOGLVL_WARNING)
		syslog(LOG_WARNING, message);
	if(level==LOGLVL_INFO)
		syslog(LOG_INFO, message);
#ifdef DEBUG
	if(level==LOGLVL_DEBUG)
		syslog(LOG_DEBUG, message);
#endif
	return;
}

static void log_console(int level, char *message) {
	if(level==LOGLVL_ERROR)
		fprintf(stderr, "Error: %s\n", message);
	if(level==LOGLVL_WARNING)
		fprintf(stderr, "Warning: %s\n", message);
	if(level==LOGLVL_INFO)
		fprintf(stdout, "Informational: %s\n", message);
#ifdef DEBUG
	if(level==LOGLVL_DEBUG)
		fprintf(stdout, "Debug: %s\n", message);
#endif
	return;
}

/* From the printf manpage with changes. Let's just hope SCO doesn't 
 * sue me now. ;) 
 */
char *my_vasprintf(const char *fmt, va_list ap) {
	/* Guess we need no more than 100 bytes. */
	int n, size = 100;
	char *p;
	p = bitlbee_alloc (size);
	while (1) {
		/* Try to print in the allocated space. */
		n = vsnprintf (p, size, fmt, ap);
		/* If that worked, return the string. */
		if (n > -1 && n < size)
			return p;
		/* Else try again with more space. */
		if (n > -1)    /* glibc 2.1 */
			size = n+1; /* precisely what is needed */
		else           /* glibc 2.0 */
			size *= 2;  /* twice the old size */
		p=bitlbee_realloc(p, size);
	}
}

