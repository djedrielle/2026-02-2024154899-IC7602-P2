#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>

int PUERTO_IN = 9777;

int main(void) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address, client_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PUERTO_IN);

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    listen(server_socket, 1);

    socklen_t client_size = sizeof(client_address);
    int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_size);
    printf("Cliente conectado, IP: %s, puerto: %i\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

    char entrada[1024];
    ssize_t bytes_leidos = read(client_socket, entrada, sizeof(entrada));

    printf("Entrada: %s", entrada);

    close(client_socket);
    close(server_socket);

    return 0;
}