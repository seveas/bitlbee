#define BITLBEE_CORE
#include "bitlbee.h"
#include "base64.h"
#include "arc.h"
#include "md5.h"
#include "ssl_client.h"

size_t password_hash(const char *password, char **hash)
{
	*hash = g_new0(char, SCRYPT_MCF_LEN);
	return scrypt_hash(*hash, (char*)password) ? strlen(*hash) : -1;
}

/* Returns values: -1 == Failure (base64-decoded to something unexpected)
                    0 == Okay
                    1 == Password doesn't match the hash. */
static int password_verify_old(const char *password, const char *hash)
{
	md5_byte_t *pass_dec = NULL;
	md5_byte_t pass_md5[16];
	md5_state_t md5_state;
	int ret = -1, i;

	if (base64_decode(hash, &pass_dec) == 21) {
		md5_init(&md5_state);
		md5_append(&md5_state, (md5_byte_t *) password, strlen(password));
		md5_append(&md5_state, (md5_byte_t *) pass_dec + 16, 5);  /* Hmmm, salt! */
		md5_finish(&md5_state, pass_md5);

		for (i = 0; i < 16; i++) {
			if (pass_dec[i] != pass_md5[i]) {
				ret = 1;
				break;
			}
		}

		/* If we reached the end of the loop, it was a match! */
		if (i == 16) {
			ret = 0;
		}
	}

	g_free(pass_dec);

	return ret;
}

int password_verify(const char *password, const char *hash) {
	int ret;
	if (hash[0] == '$') {
		char hash_copy[SCRYPT_MCF_LEN+1];
		g_strlcpy(hash_copy, hash, SCRYPT_MCF_LEN+1);
		ret = scrypt_check(hash_copy, (char*)password);
		/* Change the exitcode to match the md5sum checker */
		if (ret == 0) {
			ret = 1;
		} else if (ret > 0) {
			ret = 0;
		} else if (ret < 0) {
			ret = -1;
		}
	} else {
		ret = password_verify_old(password, hash);
	}
	return ret;
}

size_t password_encode(const char *password, char **encoded)
{
	*encoded = base64_encode((unsigned char *)password, strlen(password));
	return *encoded ? -1 : strlen(*encoded);
}

size_t password_decode(const char *encoded, char **password)
{
	return base64_decode(encoded, (unsigned char **)password);
}

size_t password_encrypt(const char *password, const char *encryption_key, char **crypt)
{
	int pass_len;
	unsigned char *encrypted;
	pass_len = ssl_aes_encrypt((unsigned char *)password, strlen(password), (unsigned char *)encryption_key, strlen(encryption_key), &encrypted);
	if(pass_len > 0) {
		*crypt = base64_encode(encrypted, pass_len);
		g_free(encrypted);
		pass_len = strlen(*crypt);
	}
	return pass_len;
}

int password_decrypt(const char *encrypted, const char *encryption_key, char **password)
{
	unsigned char *decoded;
	size_t pass_len = base64_decode(encrypted, &decoded);
	int pass_len2;
	/* An aes-encrypted password is at least 24 bytes long: the nonce and tag are 12 bytes each */
	if (pass_len >= 24) {
		pass_len2 = ssl_aes_decrypt(decoded, pass_len, (unsigned char *)encryption_key, strlen(encryption_key), (unsigned char **)password);
		if (pass_len2 >= 0) {
			pass_len = pass_len2;
		} else {
			/* Try the old password decryption scheme */
			fprintf(stderr, "Trying old encryption scheme\n");
			pass_len = arc_decode(decoded, pass_len, password, encryption_key);
		}
		g_free(decoded);
	} else {
		pass_len = arc_decode(decoded, pass_len, password, encryption_key);
	}
	return pass_len;
}
