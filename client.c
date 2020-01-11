#include "common.h"

int main() {
    int sockfd;
    uint16_t portno;
    long n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[BUF_LEN];
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("Cannot initialize socket.");
        exit(1);
    }

    server = gethostbyname("localhost");
    if (server == NULL) {
        perror("Cannot get hostname.");
        close(sockfd);
        goto close_socket_and_return;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *)&serv_addr.sin_addr.s_addr, (size_t) server->h_length);
    serv_addr.sin_port = htons(PORT);

    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        // This can be normal, probably server is listening.
        perror("Cannot connect");
        goto close_socket_and_return;
    }

    bzero(buffer, BUF_LEN);
    strncpy(buffer, "hello",  5);

    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) {
        perror("Invalid number of written chars to socket.");
        goto close_socket_and_return;
    }

    close_socket_and_return: close(sockfd);

    printf("Done\n");
    exit(EXIT_SUCCESS);
}
