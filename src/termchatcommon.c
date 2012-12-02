#include <openssl/evp.h>
#include <string.h>

void SHA512(const char *source_string, unsigned char *hash_value) {
	EVP_MD_CTX *mdctx;
	const EVP_MD *md = EVP_get_digestbyname("sha512");
	//unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;
	
	mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, md, NULL);
	EVP_DigestUpdate(mdctx, source_string, strlen(source_string));
	EVP_DigestFinal_ex(mdctx, hash_value, &md_len);
	EVP_MD_CTX_destroy(mdctx);
}
