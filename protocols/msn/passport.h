#ifndef __PASSPORT_H__
#define __PASSPORT_H__
/* passport.h
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <netdb.h>
#include "nogaim.h"


#define BUFFER_SIZE 2048

static const int pp_cert_type_priority[3] = { GNUTLS_CRT_X509,
					      GNUTLS_CRT_OPENPGP, 0 };


/* connect to the peer and return the socket descriptor.
 * reslove the servername if necessary
 */
int tcp_connect( const char* SERVER, const char* PORT);
void tcp_disconnect( int sd ); 

char* passport_retrieve_dalogin();

char* passport_get_id( char* header_i, char* url );

/* routines necessary for MSNP8 login */
char *passport_create_header(char *reply, char *email, char *pwd);

char *url_encode(const char *msg);
char *url_decode(const char *msg);

#endif /* __PASSPORT_H__ */
