  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2003 Wilmer van der Gaast and others                *
  \********************************************************************/

/* Help file control                                                    */

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

typedef union
{
	off_t file_offset;
	char *mem_offset;
} help_off_t;

typedef struct
{
	int fd;
	time_t mtime;
	char *string;
	help_off_t offset;
	int length;
	void *next;
} help_t;

help_t *help_init( irc_t *irc );
char *help_get( irc_t *irc, char *string );

