#include "md5.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char *password = NULL;

/* USE WITH CAUTION! */
/* [SH] Do _not_ call this if it's not entirely sure that it will not cause
   harm to another users file, since this does not check the password for
   correctness. */

void setpassnc (char *pass) {
	password = strdup (pass);
}

int setpass (char *pass, char* md5sum) {
	md5_state_t md5state;
	md5_byte_t digest[16];
	int i, j;
	char digits[3];
	
	md5_init (&md5state);
	md5_append (&md5state, pass, strlen (pass));
	md5_finish (&md5state, digest);
	
	for (i = 0, j = 0; i < 16; i++, j += 2) {
		snprintf (digits, sizeof (digits), "%02x\n", digest[i]);
		
		if (digits[0] != md5sum[j]) return (-1);
		if (digits[1] != md5sum[j + 1]) return (-1);
	}
	
	password = strdup (pass);
	
	return (0);
}

char *hashpass () {
	md5_state_t md5state;
	md5_byte_t digest[16];
	int i;
	char digits[3];
	char *rv;
	
	if (password == NULL) return (NULL);
	
	rv = (char *)malloc (33);
	memset (rv, 0, 33);
	
	md5_init (&md5state);
	md5_append (&md5state, password, strlen (password));
	md5_finish (&md5state, digest);
	
	for (i = 0; i < 16; i++) {
		snprintf (digits, sizeof (digits), "%02x\n", digest[i]);
		strcat (rv, digits);
	}	
	return (rv);
}

char *obfucrypt (char *line) {
	int i, j;
	char *rv;
	
	if (password == NULL) return (NULL);
	
	rv = (char *)malloc (strlen (line) + 1);
	memset (rv, '\0', strlen (line) + 1);
	
	i = j = 0;
	while (*line) {
		if (*line < 0) *line = - (*line);
		if (password[i] < 0) password[i] = - password[i];
		
		rv[j] = *line + password[i];
		
		line++;
		if (!password[++i]) i = 0;
		j++;
	}
	
	return (rv);
}

char *deobfucrypt (char *line) {
	int i, j;
	char *rv;
	
	if (password == NULL) return (NULL);
	
	rv = (char *)malloc (strlen (line) + 1);
	memset (rv, '\0', strlen (line) + 1);
	
	i = j = 0;
	while (*line) {
		rv[j] = *line - password[i];
		
		line++;
		if (!password[++i]) i = 0;
		j++;
	}
	
	return (rv);
}
