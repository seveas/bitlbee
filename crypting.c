  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2003 Wilmer van der Gaast and others                *
  \********************************************************************/

/* A little bit of encryption for the users' passwords

   Copyright 2002-2003 Sjoerd Hemminga                                  */

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
#include "irc.h"
#include "md5.h"
#include "crypting.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*\
 * [SH] Do _not_ call this if it's not entirely sure that it will not cause
 * harm to another users file, since this does not check the password for
 * correctness.
\*/

/* USE WITH CAUTION!
   Sets pass without checking */
void setpassnc (irc_t *irc, char *pass) {
	if (!set_find (irc, "password"))
		set_add (irc, "password", NULL, passchange);
	
	if (irc->password) free (irc->password);
	irc->password = strdup (pass);
}

char *passchange (irc_t *irc, void *set, char *value) {
	setpassnc (irc, value);
	return (NULL);
}

int setpass (irc_t *irc, char *pass, char* md5sum) {
	md5_state_t md5state;
	md5_byte_t digest[16];
	int i, j;
	char digits[3];
	
	md5_init (&md5state);
	md5_append (&md5state, pass, strlen (pass));
	md5_finish (&md5state, digest);
	
	for (i = 0, j = 0; i < 16; i++, j += 2) {
		/* Check password for correctness */
		snprintf (digits, sizeof (digits), "%02x\n", digest[i]);
		
		if (digits[0] != md5sum[j]) return (-1);
		if (digits[1] != md5sum[j + 1]) return (-1);
	}
	
	/* If pass is correct, we end up here and we set the pass */
	setpassnc (irc, pass);
	
	return (0);
}

char *hashpass (irc_t *irc) {
	md5_state_t md5state;
	md5_byte_t digest[16];
	int i;
	char digits[3];
	char *rv;
	
	if (irc->password == NULL) return (NULL);
	
	rv = (char *)malloc (33);
	memset (rv, 0, 33);
	
	md5_init (&md5state);
	md5_append (&md5state, irc->password, strlen (irc->password));
	md5_finish (&md5state, digest);
	
	for (i = 0; i < 16; i++) {
		/* Build a hash of the pass */
		snprintf (digits, sizeof (digits), "%02x\n", digest[i]);
		strcat (rv, digits);
	}
	
	return (rv);
}

char *obfucrypt (irc_t *irc, char *line) {
	int i, j;
	char *rv;
	
	if (irc->password == NULL) return (NULL);
	
	rv = (char *)malloc (strlen (line) + 1);
	memset (rv, '\0', strlen (line) + 1);
	
	i = j = 0;
	while (*line) {
		/* Encrypt/obfuscate the line, using the password */
		if (*(signed char*)line < 0) *line = - (*line);
		if (((signed char*)irc->password)[i] < 0) irc->password[i] = - irc->password[i];
		
		rv[j] = *line + irc->password[i]; /* Overflow intended */
		
		line++;
		if (!irc->password[++i]) i = 0;
		j++;
	}
	
	return (rv);
}

char *deobfucrypt (irc_t *irc, char *line) {
	int i, j;
	char *rv;
	
	if (irc->password == NULL) return (NULL);
	
	rv = (char *)malloc (strlen (line) + 1);
	memset (rv, '\0', strlen (line) + 1);
	
	i = j = 0;
	while (*line) {
		/* Decrypt/deobfuscate the line, using the pass */
		rv[j] = *line - irc->password[i]; /* Overflow intended */
		
		line++;
		if (!irc->password[++i]) i = 0;
		j++;
	}
	
	return (rv);
}
