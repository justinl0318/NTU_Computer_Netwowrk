/*
    CRC-32 sample code
*/

#include <zlib.h>
#include <stdio.h>
#include <string.h>

int main() {
    char data[1000];
    bzero(data, sizeof(char) * 1000);
    scanf("%s", data);
    unsigned long checksum = crc32(0L, (const Bytef *)data, 1000);
    printf("checksum: %lx\n", checksum);
}