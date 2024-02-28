#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

char file_arr[] = "hellohello";

// Function to convert binary data to a hex string
char *hexDigest(const void *buf, int len) {
    const unsigned char *cbuf = (const unsigned char *)buf;
    char *hx = (char *) malloc(len * 2 + 1); // Each byte requires 2 characters, plus 1 for null terminator

    for (int i = 0; i < len; ++i)
        sprintf(hx + i * 2, "%02x", cbuf[i]);

    return hx;
}

char *printsha256(){


    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    EVP_MD_CTX *sha256 = EVP_MD_CTX_new();
    EVP_DigestInit_ex(sha256, EVP_sha256(), NULL);

    // Process the entire string
    EVP_DigestUpdate(sha256, file_arr, sizeof(file_arr)-1); // Exclude null terminator

    // Calculate the final hash
    EVP_DigestFinal_ex(sha256, hash, &hash_len);
    return hexDigest(hash, hash_len);
}

int main() {

    // Print the final hash
    printf("sha256(\"%s\") = %s\n", file_arr, printsha256());

    return 0;
}
