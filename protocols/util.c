/*
 * nogaim
 *
 * Gaim without gaim - for BitlBee
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * Copyright 2002 Wilmer van der Gaast <lintux@lintux.cx>
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
 
/* Parts from util.c from gaim needed by nogaim */

#include "bitlbee.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <time.h>

unsigned char *utf8_to_str(unsigned char *in)
{
	int n = 0, i = 0;
	int inlen;
	unsigned char *result;

	if (!in)
		return NULL;

	inlen = strlen(in);

	result = g_malloc(inlen + 1);

	while (n <= inlen - 1) {
		long c = (long)in[n];
		if (c < 0x80)
			result[i++] = (char)c;
		else {
			if ((c & 0xC0) == 0xC0)
				result[i++] =
				    (char)(((c & 0x03) << 6) | (((unsigned char)in[++n]) & 0x3F));
			else if ((c & 0xE0) == 0xE0) {
				if (n + 2 <= inlen) {
					result[i] =
					    (char)(((c & 0xF) << 4) | (((unsigned char)in[++n]) & 0x3F));
					result[i] =
					    (char)(((unsigned char)result[i]) |
						   (((unsigned char)in[++n]) & 0x3F));
					i++;
				} else
					n += 2;
			} else if ((c & 0xF0) == 0xF0)
				n += 3;
			else if ((c & 0xF8) == 0xF8)
				n += 4;
			else if ((c & 0xFC) == 0xFC)
				n += 5;
		}
		n++;
	}
	result[i] = '\0';

	return result;
}

char *str_to_utf8(unsigned char *in)
{
	int n = 0, i = 0;
	int inlen;
	char *result = NULL;

	if (!in)
		return NULL;

	inlen = strlen(in);

	result = g_malloc(inlen * 2 + 1);

	while (n < inlen) {
		long c = (long)in[n];
		if (c == 27) {
			n += 2;
			if (in[n] == 'x')
				n++;
			if (in[n] == '3')
				n++;
			n += 2;
			continue;
		}
		/* why are we removing newlines and carriage returns?
		if ((c == 0x0D) || (c == 0x0A)) {
			n++;
			continue;
		}
		*/
		if (c < 128)
			result[i++] = (char)c;
		else {
			result[i++] = (char)((c >> 6) | 192);
			result[i++] = (char)((c & 63) | 128);
		}
		n++;
	}
	result[i] = '\0';

	return result;
}

void strip_linefeed(gchar *text)
{
	int i, j;
	gchar *text2 = g_malloc(strlen(text) + 1);

	for (i = 0, j = 0; text[i]; i++)
		if (text[i] != '\r')
			text2[j++] = text[i];
	text2[j] = '\0';

	strcpy(text, text2);
	g_free(text2);
}

char *add_cr(char *text)
{
	char *ret = NULL;
	int count = 0, i, j;

	if (text[0] == '\n')
		count++;
	for (i = 1; i < strlen(text); i++)
		if (text[i] == '\n' && text[i - 1] != '\r')
			count++;

	if (count == 0)
		return g_strdup(text);

	ret = g_malloc0(strlen(text) + count + 1);

	i = 0; j = 0;
	if (text[i] == '\n')
		ret[j++] = '\r';
	ret[j++] = text[i++];
	for (; i < strlen(text); i++) {
		if (text[i] == '\n' && text[i - 1] != '\r')
			ret[j++] = '\r';
		ret[j++] = text[i];
	}

	return ret;
}

static char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" "0123456789+/";

/* XXX Find bug */
char *tobase64(const char *text)
{
	char *out = NULL;
	const char *c;
	unsigned int tmp = 0;
	int len = 0, n = 0;

	c = text;

	while (*c) {
		tmp = tmp << 8;
		tmp += *c;
		n++;

		if (n == 3) {
			out = g_realloc(out, len + 4);
			out[len] = alphabet[(tmp >> 18) & 0x3f];
			out[len + 1] = alphabet[(tmp >> 12) & 0x3f];
			out[len + 2] = alphabet[(tmp >> 6) & 0x3f];
			out[len + 3] = alphabet[tmp & 0x3f];
			len += 4;
			tmp = 0;
			n = 0;
		}
		c++;
	}
	switch (n) {

	case 2:
		tmp <<= 8;
		out = g_realloc(out, len + 5);
		out[len] = alphabet[(tmp >> 18) & 0x3f];
		out[len + 1] = alphabet[(tmp >> 12) & 0x3f];
		out[len + 2] = alphabet[(tmp >> 6) & 0x3f];
		out[len + 3] = '=';
		out[len + 4] = 0;
		break;
	case 1:
		tmp <<= 16;
		out = g_realloc(out, len + 5);
		out[len] = alphabet[(tmp >> 18) & 0x3f];
		out[len + 1] = alphabet[(tmp >> 12) & 0x3f];
		out[len + 2] = '=';
		out[len + 3] = '=';
		out[len + 4] = 0;
		break;
	case 0:
		out = g_realloc(out, len + 1);
		out[len] = 0;
		break;
	}
	return out;
}

char *normalize(const char *s)
{
	static char buf[BUF_LEN];
	char *t, *u;
	int x = 0;

	g_return_val_if_fail((s != NULL), NULL);

	u = t = g_strdup(s);

	strcpy(t, s);
	g_strdown(t);

	while (*t && (x < BUF_LEN - 1)) {
		if (*t != ' ') {
			buf[x] = *t;
			x++;
		}
		t++;
	}
	buf[x] = '\0';
	g_free(u);
	return buf;
}

time_t get_time(int year, int month, int day, int hour, int min, int sec)
{
	struct tm tm;

	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec >= 0 ? sec : time(NULL) % 60;
	return mktime(&tm);
}
