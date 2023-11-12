#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

struct udp_message {
    char topic[51];
    uint8_t type;
    char payload[1501];
};

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    if(argc < 4) {
        fprintf(stderr, "\n[SUBSCRIBER ERR] No port provided\n");
        return -1;
    }

    int port = atoi(argv[3]);
    if(port == 0) {
        fprintf(stderr, "\n[SUBSCRIBER ERR] Invalid port\n");
        return -1;
    }

    int sockfd;
    struct sockaddr_in server_address;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        fprintf(stderr, "\n[SUBSCRIBER ERR] Could not create TCP socket\n");
        return -1;
    }

    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        fprintf(stderr, "\n[SUBSCRIBER ERR] Could not set TCP socket options\n");
        return -1;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(argv[2]);

    if(connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "\n[SUBSCRIBER ERR] Could not connect to server\n");
        return -1;
    }

    struct pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    // Send ID
    char id[11];
    sprintf(id, "id %s", argv[1]);
    if(send(sockfd, id, strlen(id), 0) < 0) {
        fprintf(stderr, "\n[SUBSCRIBER ERR] Could not send ID\n");
        return -1;
    }

    while(1) {
        if(poll(fds, 2, -1) < 0) {
            fprintf(stderr, "\n[SUBSCRIBER ERR] Could not poll\n");
            return -1;
        }

        if(fds[0].revents & POLLIN) {
            char buffer[100];
            memset(buffer, 0, sizeof(buffer));
            if(read(0, buffer, sizeof(buffer)) < 0) {
                fprintf(stderr, "\n[SUBSCRIBER ERR] Could not read from stdin\n");
                return -1;
            }
            if(strncmp(buffer, "exit", 4) == 0) {
                char exit[5];
                sprintf(exit, "exit");
                if(send(sockfd, exit, strlen(exit), 0) < 0) {
                    fprintf(stderr, "\n[SUBSCRIBER ERR] Could not send exit\n");
                    return -1;
                }
                close(sockfd);
                return 0;
            }
            if(strncmp(buffer, "subscribe", 9) == 0) {
                if(strncmp(buffer + 10, " ", 1) == 0) {
                    fprintf(stderr, "\n[SUBSCRIBER ERR] Invalid topic\n");
                    continue;
                }
                
                char subscribe[100];
                sprintf(subscribe, "subscribe %s", buffer + 10);
                if(send(sockfd, subscribe, strlen(subscribe), 0) < 0) {
                    fprintf(stderr, "\n[SUBSCRIBER ERR] Could not send subscribe\n");
                    return -1;
                }
                fprintf(stdout, "Subscribed to topic.\n");
            }
            if(strncmp(buffer, "unsubscribe", 11) == 0) {
                if(strncmp(buffer + 12, " ", 1) == 0) {
                    fprintf(stderr, "\n[SUBSCRIBER ERR] Invalid topic\n");
                    continue;
                }

                char unsubscribe[100];
                sprintf(unsubscribe, "unsubscribe %s", buffer + 12);
                if(send(sockfd, unsubscribe, strlen(unsubscribe), 0) < 0) {
                    fprintf(stderr, "\n[SUBSCRIBER ERR] Could not send unsubscribe\n");
                    return -1;
                }
                fprintf(stdout, "Unsubscribed to topic.\n");
            }
        }

        if(fds[1].revents & POLLIN) {
            char buffer[1553];
            memset(buffer, 0, sizeof(buffer));
            if(recv(sockfd, buffer, sizeof(buffer), 0) < 0) {
                fprintf(stderr, "\n[SUBSCRIBER ERR] Could not receive message\n");
                return -1;
            }
            if(strncmp(buffer, "exit", 4) == 0) {
                close(sockfd);
                return 0;
            }

            struct udp_message *message = (struct udp_message *)buffer;

            if(message->type == 0) {
                uint8_t sign =  *(uint8_t *)message->payload;
                uint32_t content =  ntohl(*(uint32_t *)(message->payload + 1));
                if(sign == 0) {
                    fprintf(stdout, "%s - INT - %d\n", message->topic, content);
                }
                else {
                    fprintf(stdout, "%s - INT - -%d\n", message->topic, content);
                }
            }
            if(message->type == 1) {
                uint16_t content =  ntohs(*(uint16_t *)message->payload);
                fprintf(stdout, "%s - SHORT_REAL - %.2f\n", message->topic, (float)content / 100);
            }
            if(message->type == 2) {
                uint8_t sign =  *(uint8_t *)message->payload;
                uint32_t content =  ntohl(*(uint32_t *)(message->payload + 1));
                uint8_t power = *(uint8_t *)(message->payload + 5);
                int toDivide = 1;
                for(int i = 0; i < power; i++) {
                    toDivide *= 10;
                }
                if(sign == 0) {
                    fprintf(stdout, "%s - FLOAT - %.4f\n", message->topic, (float)content / toDivide);
                }
                else {
                    fprintf(stdout, "%s - FLOAT - -%.4f\n", message->topic, (float)content / toDivide);
                }
            }
            if(message->type == 3) {
                fprintf(stdout, "%s - STRING - %s\n", message->topic, message->payload);
            }
        }
    }

    close(sockfd);
    return 0;
}