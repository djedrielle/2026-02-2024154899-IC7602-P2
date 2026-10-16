// Codigo recuperado de: https://www.geeksforgeeks.org/computer-networks/udp-client-server-using-connect-c-implementation/
// y modificado a necesidad de la implementacion del proyecto 2

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<netinet/in.h>
#define PORT 5000
#define MAXLINE 1000

void imprimir_hex(const unsigned char *buf, int n) {
    for (int i = 0; i < n; i++) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

void procesar(char *buffer, int n) {
    bool qr = ((buffer[2] >> 7) & 1) == 1;
    uint8_t op_code = (buffer[2] >> 3) & 15;
    if (!qr && op_code == 0) {
        // Extraer dominio de la solicitud
        // Consultar ip to country
        printf("Es estandar query.\n");
    } else {
        printf("No estandar query.\n");
    }
}

int main() {   
    char buffer[100];
    char *message = "Hello Client";
    int listenfd, len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    // Create a UDP Socket
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);        
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET; 
 
    // bind server address to socket descriptor
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
     
    //receive the datagram
    len = sizeof(cliaddr);
    int n = recvfrom(listenfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr,&len); //receive message from server
    printf("recibidos %d bytes:\n", n);
    imprimir_hex((unsigned char *)buffer, n);
    procesar(buffer, n);
         
    // send the response
    //sendto(listenfd, message, strlen(message), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
}