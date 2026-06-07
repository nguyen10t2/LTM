#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

void xor_cipher(char *data, size_t size) {
    static const char key = 'K'; // Key for XOR cipher

    for (size_t i = 0; i < size; i++) {
        data[i] ^= key; // XOR each byte with the key
    }
}

int main() {
    char data[] = "Hello, World!";
    size_t size = sizeof(data) - 1; // Exclude null terminator
    xor_cipher(data, size);
    printf("%s\n", data);


    xor_cipher(data, size);
    printf("%s\n", data);
    return 0;
}
