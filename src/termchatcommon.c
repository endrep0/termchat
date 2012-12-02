#include <openssl/evp.h>
#include <string.h>

void SHA512(char *source_string, char *hash_in_hex_string) {
	EVP_MD_CTX *mdctx;
	OpenSSL_add_all_digests();
	const EVP_MD *md = EVP_get_digestbyname("sha512");
	unsigned int md_len;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	int i;
	char buf[32];

	mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, md, NULL);
	EVP_DigestUpdate(mdctx, source_string, strlen(source_string));
	EVP_DigestFinal_ex(mdctx, md_value, &md_len);
	EVP_MD_CTX_destroy(mdctx);
	
	// represent the hash as a hex string
	for(i = 0; i < md_len; i++) {
		sprintf(buf, "%02x", md_value[i]);
		strcat(hash_in_hex_string, buf);
	}
}
