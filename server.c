#include "common.h"

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Cannot establish socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (const struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
        perror("Cannot bind");
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 32);

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int cli_sock_fd;

    char buf[BUF_LEN];

    for (;;) {
        cli_sock_fd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_len);
        if (cli_sock_fd == -1) {
            perror("Cannot accept");
            exit(EXIT_FAILURE);
        }

        bzero(buf, BUF_LEN);
        read(cli_sock_fd, buf, BUF_LEN);

        printf("IN: <%s>\n", buf);
    }

    printf("Server end.\n");
    exit(EXIT_SUCCESS);
}
