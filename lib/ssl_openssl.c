/********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2012 Wilmer van der Gaast and others                *
  \********************************************************************/

/* SSL module - OpenSSL version                                         */

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
  if not, write to the Free Software Foundation, Inc., 51 Franklin St.,
  Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "bitlbee.h"
#include "proxy.h"
#include "ssl_client.h"
#include "sock.h"

int ssl_errno = 0;

static gboolean initialized = FALSE;

struct scd {
	ssl_input_function func;
	gpointer data;
	int fd;
	gboolean established;
	gboolean verify;
	char *hostname;

	int inpa;
	int lasterr;            /* Necessary for SSL_get_error */
	SSL *ssl;
};

static SSL_CTX *ssl_ctx;

static void ssl_conn_free(struct scd *conn);
static gboolean ssl_connected(gpointer data, gint source, b_input_condition cond);
static gboolean ssl_starttls_real(gpointer data, gint source, b_input_condition cond);
static gboolean ssl_handshake(gpointer data, gint source, b_input_condition cond);


void ssl_init(void)
{
	const SSL_METHOD *meth;

	SSL_library_init();

	meth = TLSv1_client_method();
	ssl_ctx = SSL_CTX_new(meth);

	initialized = TRUE;
}

void *ssl_connect(char *host, int port, gboolean verify, ssl_input_function func, gpointer data)
{
	struct scd *conn = g_new0(struct scd, 1);

	conn->fd = proxy_connect(host, port, ssl_connected, conn);
	if (conn->fd < 0) {
		ssl_conn_free(conn);
		return NULL;
	}

	conn->func = func;
	conn->data = data;
	conn->inpa = -1;
	conn->hostname = g_strdup(host);

	return conn;
}

void *ssl_starttls(int fd, char *hostname, gboolean verify, ssl_input_function func, gpointer data)
{
	struct scd *conn = g_new0(struct scd, 1);

	conn->fd = fd;
	conn->func = func;
	conn->data = data;
	conn->inpa = -1;
	conn->verify = verify && global.conf->cafile;
	conn->hostname = g_strdup(hostname);

	/* This function should be called via a (short) timeout instead of
	   directly from here, because these SSL calls are *supposed* to be
	   *completely* asynchronous and not ready yet when this function
	   (or *_connect, for examle) returns. Also, errors are reported via
	   the callback function, not via this function's return value.

	   In short, doing things like this makes the rest of the code a lot
	   simpler. */

	b_timeout_add(1, ssl_starttls_real, conn);

	return conn;
}

static gboolean ssl_starttls_real(gpointer data, gint source, b_input_condition cond)
{
	struct scd *conn = data;

	return ssl_connected(conn, conn->fd, B_EV_IO_WRITE);
}

static gboolean ssl_connected(gpointer data, gint source, b_input_condition cond)
{
	struct scd *conn = data;

	if (conn->verify) {
		/* Right now we don't have any verification functionality for OpenSSL. */
		conn->func(conn->data, 1, NULL, cond);
		if (source >= 0) {
			proxy_disconnect(source);
		}
		ssl_conn_free(conn);

		return FALSE;
	}

	if (source == -1) {
		goto ssl_connected_failure;
	}

	if (!initialized) {
		ssl_init();
	}


	if (ssl_ctx == NULL) {
		goto ssl_connected_failure;
	}

	conn->ssl = SSL_new(ssl_ctx);
	if (conn->ssl == NULL) {
		goto ssl_connected_failure;
	}

	/* We can do at least the handshake with non-blocking I/O */
	sock_make_nonblocking(conn->fd);
	SSL_set_fd(conn->ssl, conn->fd);

	if (conn->hostname && !g_ascii_isdigit(conn->hostname[0])) {
		SSL_set_tlsext_host_name(conn->ssl, conn->hostname);
	}

	return ssl_handshake(data, source, cond);

ssl_connected_failure:
	conn->func(conn->data, 0, NULL, cond);
	ssl_disconnect(conn);
	return FALSE;

}

static gboolean ssl_handshake(gpointer data, gint source, b_input_condition cond)
{
	struct scd *conn = data;
	int st;

	if ((st = SSL_connect(conn->ssl)) < 0) {
		conn->lasterr = SSL_get_error(conn->ssl, st);
		if (conn->lasterr != SSL_ERROR_WANT_READ && conn->lasterr != SSL_ERROR_WANT_WRITE) {
			conn->func(conn->data, 0, NULL, cond);
			ssl_disconnect(conn);
			return FALSE;
		}

		conn->inpa = b_input_add(conn->fd, ssl_getdirection(conn), ssl_handshake, data);
		return FALSE;
	}

	conn->established = TRUE;
	sock_make_blocking(conn->fd);           /* For now... */
	conn->func(conn->data, 0, conn, cond);
	return FALSE;
}

int ssl_read(void *conn, char *buf, int len)
{
	int st;

	if (!((struct scd*) conn)->established) {
		ssl_errno = SSL_NOHANDSHAKE;
		return -1;
	}

	st = SSL_read(((struct scd*) conn)->ssl, buf, len);

	ssl_errno = SSL_OK;
	if (st <= 0) {
		((struct scd*) conn)->lasterr = SSL_get_error(((struct scd*) conn)->ssl, st);
		if (((struct scd*) conn)->lasterr == SSL_ERROR_WANT_READ || ((struct scd*) conn)->lasterr ==
		    SSL_ERROR_WANT_WRITE) {
			ssl_errno = SSL_AGAIN;
		}
	}

	if (0 && getenv("BITLBEE_DEBUG") && st > 0) {
		write(1, buf, st);
	}

	return st;
}

int ssl_write(void *conn, const char *buf, int len)
{
	int st;

	if (!((struct scd*) conn)->established) {
		ssl_errno = SSL_NOHANDSHAKE;
		return -1;
	}

	st = SSL_write(((struct scd*) conn)->ssl, buf, len);

	if (0 && getenv("BITLBEE_DEBUG") && st > 0) {
		write(1, buf, st);
	}

	ssl_errno = SSL_OK;
	if (st <= 0) {
		((struct scd*) conn)->lasterr = SSL_get_error(((struct scd*) conn)->ssl, st);
		if (((struct scd*) conn)->lasterr == SSL_ERROR_WANT_READ || ((struct scd*) conn)->lasterr ==
		    SSL_ERROR_WANT_WRITE) {
			ssl_errno = SSL_AGAIN;
		}
	}

	return st;
}

int ssl_pending(void *conn)
{
	return (((struct scd*) conn) && ((struct scd*) conn)->established) ?
	       SSL_pending(((struct scd*) conn)->ssl) > 0 : 0;
}

static void ssl_conn_free(struct scd *conn)
{
	SSL_free(conn->ssl);
	g_free(conn->hostname);
	g_free(conn);

}

void ssl_disconnect(void *conn_)
{
	struct scd *conn = conn_;

	if (conn->inpa != -1) {
		b_event_remove(conn->inpa);
	}

	if (conn->established) {
		SSL_shutdown(conn->ssl);
	}

	proxy_disconnect(conn->fd);

	ssl_conn_free(conn);
}

int ssl_getfd(void *conn)
{
	return(((struct scd*) conn)->fd);
}

b_input_condition ssl_getdirection(void *conn)
{
	return(((struct scd*) conn)->lasterr == SSL_ERROR_WANT_WRITE ? B_EV_IO_WRITE : B_EV_IO_READ);
}

char *ssl_verify_strerror(int code)
{
	return g_strdup("SSL certificate verification not supported by BitlBee OpenSSL code.");
}

size_t ssl_des3_encrypt(const unsigned char *key, size_t key_len, const unsigned char *input, size_t input_len,
                        const unsigned char *iv, unsigned char **res)
{
	int output_length = 0;
	EVP_CIPHER_CTX ctx;

	*res = g_new0(unsigned char, 72);

	/* Don't set key or IV because we will modify the parameters */
	EVP_CIPHER_CTX_init(&ctx);
	EVP_CipherInit_ex(&ctx, EVP_des_ede3_cbc(), NULL, NULL, NULL, 1);
	EVP_CIPHER_CTX_set_key_length(&ctx, key_len);
	EVP_CIPHER_CTX_set_padding(&ctx, 0);
	/* We finished modifying parameters so now we can set key and IV */
	EVP_CipherInit_ex(&ctx, NULL, NULL, key, iv, 1);
	EVP_CipherUpdate(&ctx, *res, &output_length, input, input_len);
	EVP_CipherFinal_ex(&ctx, *res, &output_length);
	EVP_CIPHER_CTX_cleanup(&ctx);
	//EVP_cleanup();

	return output_length;
}


size_t ssl_aes_encrypt(const unsigned char *plain, size_t plain_len, const unsigned char *pkey, size_t pkey_len, unsigned char **crypt) {
	unsigned int ret = -1;
	unsigned char dkey[32];
	int outl, outl2;

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

	*crypt = g_malloc(plain_len+12+12);
	random_bytes(*crypt, 12);
	scrypt_kdf(pkey, pkey_len, *crypt, 12, dkey, 32);

	if(!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
		fprintf(stderr, "EVP_EncryptInit_ex failed\n");
		ret = -1;
	} else {
		EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL);
		EVP_CIPHER_CTX_set_key_length(ctx, 32);

		if(!EVP_EncryptInit_ex(ctx, NULL, NULL, dkey, *crypt)) {
			fprintf(stderr, "EVP_EncryptInit_ex failed\n");
			ret = -1;
		} else if (!EVP_EncryptUpdate(ctx, NULL, &outl, (unsigned char *)"bitlbee", 7)) {
			fprintf(stderr, "EVP_EncryptUpdate (AAD) failed\n");
			ret = -1;
		} else if (!EVP_EncryptUpdate(ctx, *crypt+12, &outl, plain, plain_len)) {
			fprintf(stderr, "EVP_EncryptUpdate failed\n");
			ret = -1;
		} else if (!EVP_EncryptFinal_ex(ctx, *crypt+12+outl, &outl2)) {
			fprintf(stderr, "EVP_EncryptFinal failed\n");
			ret = -1;
		} else if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 12, *crypt+12+outl+outl2)) {
			fprintf(stderr, "EVP_Encrypt get tag failed\n");
			ret = -1;
		} else {
			ret = outl + outl2 + 12 + 12;
		}
	}

	EVP_CIPHER_CTX_cleanup(ctx);
	EVP_CIPHER_CTX_free(ctx);
	if (ret < 0) {
		g_free(*crypt);
		*crypt = NULL;
	}
	return ret;
}

int ssl_aes_decrypt(const unsigned char *crypt, size_t crypt_len, const unsigned char *pkey, size_t pkey_len, unsigned char **plain) {
	unsigned int ret = -1;
	unsigned char dkey[32];
	int outl, outl2;

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

	*plain = g_malloc(crypt_len-12-12+1);
	scrypt_kdf(pkey, pkey_len, crypt, 12, dkey, 32);

	if(!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
		fprintf(stderr, "EVP_DecryptInit_ex failed\n");
		ret = -1;
	} else {
		EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL);
		EVP_CIPHER_CTX_set_key_length(ctx, 32);

		if(!EVP_DecryptInit_ex(ctx, NULL, NULL, dkey, crypt)) {
			fprintf(stderr, "EVP_DecryptInit_ex failed\n");
			ret = -1;
		} else if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 12, crypt+crypt_len-12)) {
			fprintf(stderr, "EVP_Decrypt set tag failed\n");
			ret = -1;
		} else if (!EVP_DecryptUpdate(ctx, NULL, &outl, (unsigned char *)"bitlbee", 7)) {
			fprintf(stderr, "EVP_DecryptUpdate (AAD) failed\n");
			ret = -1;
		} else if (!EVP_DecryptUpdate(ctx, *plain, &outl, crypt+12, crypt_len-12-12)) {
			fprintf(stderr, "EVP_DecryptUpdate failed\n");
			ret = -1;
		} else if (!EVP_DecryptFinal_ex(ctx, *plain+outl, &outl2)) {
			fprintf(stderr, "EVP_DecryptFinal failed\n");
			ret = -1;
		} else {
			ret = outl + outl2;
			plain[outl+outl2] = '\0';
		}
	}

	EVP_CIPHER_CTX_cleanup(ctx);
	EVP_CIPHER_CTX_free(ctx);
	if (ret < 0) {
		g_free(*plain);
		*plain = NULL;
	}
	return ret;
}
