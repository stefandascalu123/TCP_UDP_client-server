#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>


#define MAX_CLIENTS 30
#define MAX_MSG_SIZE 1552
#define MAX_ID_SIZE 11
#define MAX_TOPICS 17



struct client {
    int fd;
    char id[MAX_ID_SIZE];
    struct sockaddr_in address;
};

struct client clients[MAX_CLIENTS];
char clients_ids[MAX_CLIENTS][MAX_ID_SIZE];
int ids_count = 0;

struct udp_message {
    char topic[51];
    uint8_t type;
    char payload[1501];
};

struct noSF_topic{
    char topic[51];
    char clients[MAX_CLIENTS][MAX_ID_SIZE];
    int clients_count;
};

struct noSF_topic noSF_topics[MAX_TOPICS];
int noSF_topics_count = 0;

struct noSF_topic SF_topics[MAX_TOPICS];
int SF_topics_count = 0; 

struct saved_message {
    char id[MAX_ID_SIZE];
    struct udp_message messages[10];
    int messages_count;
};

struct saved_message saved_messages[MAX_CLIENTS];
int saved_messages_count = 0;

int main(int argc, char *argv[]) {

    setvbuf(stdout, NULL, _IONBF, 0);

    // Check if port is provided
    if(argc < 2) {
        fprintf(stderr, "\n[SERVER ERR] No port provided\n");
        return -1;
    }
    
    // Port
    int port = atoi(argv[1]);
    if(port == 0) {
        fprintf(stderr, "\n[SERVER ERR] Invalid port\n");
        return -1;
    }

    int tcpfd, udpfd;

    struct sockaddr_in server_address;

    // Create TCP socket

    tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    if(tcpfd == -1) {
        fprintf(stderr, "\n[SERVER ERR] Could not create TCP socket\n");
        return -1;
    }

    if(setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        fprintf(stderr, "\n[SERVER ERR] Could not set TCP socket options\n");
        return -1;
    }

    // Intialize server address
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    // Bind TCP socket
    if(bind(tcpfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "\n[SERVER ERR] Could not bind TCP socket\n");
        return -1;
    }

    // Create UDP socket

    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udpfd == -1) {
        fprintf(stderr, "\n[SERVER ERR] Could not create UDP socket\n");
        return -1;
    }

    // Bind UDP socket
    if(bind(udpfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "\n[SERVER ERR] Could not bind UDP socket\n");
        return -1;
    }

    // Initialize pollfd
    struct pollfd fds[MAX_CLIENTS + 3];
    int clients_count = 0;
    
    // Listen for connections
    if(listen(tcpfd, MAX_CLIENTS) < 0) {
        fprintf(stderr, "\n[SERVER ERR] Could not listen for connections\n");
        return -1;
    }

    // Add TCP socket to pollfd
    fds[0].fd = tcpfd;
    fds[0].events = POLLIN;

    // Add UDP socket to pollfd
    fds[1].fd = udpfd;
    fds[1].events = POLLIN;

    // Add STDIN to pollfd
    fds[2].fd = 0;
    fds[2].events = POLLIN;

    while(1) {
        if(poll(fds, clients_count + 3, -1) < 0) {
            fprintf(stderr, "\n[SERVER ERR] Poll failed\n");
            return -1;
        }

        // Read from STDIN
        if(fds[2].revents & POLLIN) {
            char buffer[MAX_MSG_SIZE];
            memset(buffer, 0, MAX_MSG_SIZE);
            if(fgets(buffer, MAX_MSG_SIZE, stdin) == NULL) {
                fprintf(stderr, "\n[SERVER ERR] Could not read from STDIN\n");
                return -1;
            }
            if(strcmp(buffer, "exit\n") == 0) {
                // Close all connections
                for(int i = 3; i < clients_count + 3; i++) {
                    if(send(fds[i].fd, "exit", 4, 0) < 0) {
                        fprintf(stderr, "\n[SERVER ERR] Could not send exit message to client\n");
                        return -1;
                    }
                    close(fds[i].fd);
                }
                close(tcpfd);
                close(udpfd);
                return 0;
            }
        }

        // Accept new TCP connections
        if(fds[0].revents & POLLIN) {
            struct sockaddr_in client_address;
            socklen_t client_address_size = sizeof(client_address);
            int newfd = accept(tcpfd, (struct sockaddr *)&client_address, &client_address_size);
            if(newfd < 0) {
                fprintf(stderr, "\n[SERVER ERR] Could not accept new TCP connection\n");
                return -1;
            }
            fds[clients_count + 3].fd = newfd;
            fds[clients_count + 3].events = POLLIN;

            clients[clients_count].fd = newfd;
            clients[clients_count].address.sin_addr = client_address.sin_addr;
            clients[clients_count].address.sin_port = client_address.sin_port;
            clients_count++;
        }

        // Read from UDP socket
        if(fds[1].revents & POLLIN) {
            char buffer[1552];
            memset(buffer, 0, MAX_MSG_SIZE);
            if(recvfrom(udpfd, buffer, MAX_MSG_SIZE, 0, NULL, NULL) < 0) {
                fprintf(stderr, "\n[SERVER ERR] Could not read from UDP socket\n");
                return -1;
            }


            struct udp_message message;
            memcpy(&message.topic, buffer, 50);
            message.topic[50] = '\0';
            memcpy(&message.type, buffer + 50, sizeof(uint8_t));
            memcpy(&message.payload, buffer + 50 + sizeof(message.type), 1500);
            message.payload[sizeof(message.payload) - 1] = '\0';
            // Send message to all clients subscribed to the topic
            for (int i = 0; i < noSF_topics_count; i ++) {
                if(strcmp(noSF_topics[i].topic, message.topic) == 0) {
                    for(int j = 0; j < noSF_topics[i].clients_count; j++) {
                        for(int k = 0 ; k < clients_count; k++) {
                            if(strcmp(clients[k].id, noSF_topics[i].clients[j]) == 0) {
                                if(send(clients[k].fd, &message, sizeof(message), 0) < 0) {
                                    fprintf(stderr, "\n[SERVER ERR] Could not send message to client\n");
                                    return -1;
                                }
                            }
                        }
                    }
                }
            }

            for (int i = 0; i < SF_topics_count; i++) {
                if(strcmp(SF_topics[i].topic, message.topic) == 0) {
                    for(int j = 0; j < SF_topics[i].clients_count; j++) {
                        int is_active = 0;
                        for(int k = 0 ; k < clients_count; k++) {
                            if(strcmp(clients[k].id, SF_topics[i].clients[j]) == 0) {
                                is_active = 1;
                                if(send(clients[k].fd, &message, sizeof(message), 0) < 0) {
                                    fprintf(stderr, "\n[SERVER ERR] Could not send message to client\n");
                                    return -1;
                                }
                            }
                        }
                        if(is_active == 0) {
                            for(int k = 0; k < saved_messages_count; k ++) {
                                if(strcmp(saved_messages[k].id, SF_topics[i].clients[j]) == 0) {
                                    memcpy(&saved_messages[k].messages[saved_messages[k].messages_count], &message, sizeof(message));
                                    saved_messages[k].messages_count ++;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Read from TCP sockets
        for(int i = 3; i < clients_count + 3; i++) {
            if(fds[i].revents & POLLIN) {
                char buffer[MAX_MSG_SIZE];
                memset(buffer, 0, MAX_MSG_SIZE);
                if(recv(fds[i].fd, buffer, MAX_MSG_SIZE, 0) < 0) {
                    fprintf(stderr, "\n[SERVER ERR] Could not read from TCP socket\n");
                    return -1;
                }

                if(strncmp(buffer, "id", 2) == 0) {
                    //Check if ID is valid
                    int valid = 1;
                    for(int j = 0; j < clients_count - 1; j++) {
                        if(strcmp(clients[j].id, buffer + 3) == 0) {
                            valid = 0;
                            break;
                        }
                    }

                    if(valid) {
                        strcpy(clients[i - 3].id, buffer + 3);
                        fprintf(stdout, "New client %s connected from %s:%d.\n", clients[i - 3].id, inet_ntoa(clients[i - 3].address.sin_addr), ntohs(clients[clients_count].address.sin_port));

                        int is_added = 0;
                        for(int j = 0; j < saved_messages_count; j++) {
                            if(strcmp(saved_messages[j].id, clients[i - 3].id) == 0) {
                                is_added = 1;
                                for(int k = 0; k < saved_messages[j].messages_count; k++) {
                                    if(send(clients[i - 3].fd, &saved_messages[j].messages[k], sizeof(struct udp_message), 0) < 0) {
                                        fprintf(stderr, "\n[SERVER ERR] Could not send message to client\n");
                                        return -1;
                                    }
                                }
                                memset(saved_messages[j].messages, 0, sizeof(struct udp_message) * saved_messages[j].messages_count);
                                saved_messages[j].messages_count = 0;
                                break;
                            }
                        }
                        if(is_added == 0) {
                            strcpy(saved_messages[saved_messages_count].id, clients[i - 3].id);
                            saved_messages[saved_messages_count].messages_count = 0;
                            saved_messages_count++;
                        }

                    } else {
                        fprintf(stdout, "Client %s already connected.\n", buffer + 3);
                        send(fds[i].fd, "exit", 4, 0);
                        close(fds[i].fd);
                        for(int j = i; j < clients_count + 2; j++) {
                            fds[j] = fds[j + 1];
                            clients[j - 3] = clients[j - 2];
                        }
                        clients_count--;
                        i--;
                        continue;
                    }
                }else if(strncmp(buffer, "exit", 4) == 0) {
                    
                    for(int j = 0; j < clients_count; j++) {
                        if(fds[i].fd == clients[j].fd) {
                            fprintf(stdout, "Client %s disconnected.\n", clients[j].id);
                            break;
                        }
                    }
                    close(fds[i].fd);

                    for(int j = i; j < clients_count + 2; j++) {
                        fds[j] = fds[j + 1];
                        clients[j - 3] = clients[j - 2];
                    }
                    clients_count--;
                    i--;
                    continue;
                } else if(strncmp(buffer, "subscribe", 9) == 0) {
                    if(strlen(buffer) < 11) {
                        fprintf(stderr, "\n[SERVER ERR] Invalid subscribe message\n");
                        return -1;
                    }

                    char *topic = strtok(buffer, " \n");
                    topic = strtok(NULL, " \n");
                    if(topic == NULL) {
                        fprintf(stderr, "\n[SERVER ERR] Invalid subscribe message\n");
                        return -1;
                    }

                    char *sf = strtok(NULL, " \n");
                    if(sf == NULL) {
                        fprintf(stderr, "\n[SERVER ERR] Invalid subscribe message\n");
                        return -1;
                    }

                    int sf_value = atoi(sf);

                    if(sf_value != 0 && sf_value != 1) {
                        fprintf(stderr, "\n[SERVER ERR] Invalid subscribe message\n");
                        return -1;
                    }

                    if(strlen(topic) > 50) {
                        fprintf(stderr, "\n[SERVER ERR] Invalid subscribe message\n");
                        return -1;
                    }

                    if(sf_value == 0) {
                        int is_topic = 0;
                        char *client_id = NULL;
                        for(int j = 0; j < clients_count; j++) {
                            if(fds[i].fd == clients[j].fd) {
                                client_id = clients[j].id;
                                break;
                            }
                        }
                        for(int j = 0 ; j < noSF_topics_count; j ++) {
                            if(strcmp(noSF_topics[j].topic, topic) == 0) {
                                is_topic = 1;
                                int is_subscribed = 0;
                                for(int k = 0; k < noSF_topics[j].clients_count; k++) {
                                    if(strcmp(noSF_topics[j].clients[k], client_id) == 0) {
                                        fprintf(stderr, "\n[SERVER ERR] Client %s already subscribed to topic %s\n", client_id, topic);
                                        is_subscribed = 1;
                                        break;
                                    }
                                }
                                if(is_subscribed == 0) {
                                    strcpy(noSF_topics[j].clients[noSF_topics[j].clients_count], client_id);
                                    noSF_topics[j].clients_count++;
                                }
                            }
                            if(is_topic == 1) {
                                break;
                            }
                        }
                        if(is_topic == 0) {
                            strcpy(noSF_topics[noSF_topics_count].topic, topic);
                            strcpy(noSF_topics[noSF_topics_count].clients[0], client_id);
                            noSF_topics[noSF_topics_count].clients_count = 1;
                            noSF_topics_count++;
                        }
                    }

                    if(sf_value == 1) {
                        int is_topic = 0;
                        char *client_id = NULL;
                        for(int j = 0; j < clients_count; j++) {
                            if(fds[i].fd == clients[j].fd) {
                                client_id = clients[j].id;
                                break;
                            }
                        }
                        for(int j = 0 ; j < SF_topics_count; j ++) {
                            if(strcmp(SF_topics[j].topic, topic) == 0) {
                                is_topic = 1;
                                int is_subscribed = 0;
                                for(int k = 0; k < SF_topics[j].clients_count; k++) {
                                    if(strcmp(SF_topics[j].clients[k], client_id) == 0) {
                                        fprintf(stderr, "\n[SERVER ERR] Client %s already subscribed to topic %s\n", client_id, topic);
                                        is_subscribed = 1;
                                        break;
                                    }
                                }
                                if(is_subscribed == 0) {
                                    strcpy(SF_topics[j].clients[SF_topics[j].clients_count], client_id);
                                    SF_topics[j].clients_count++;
                                }
                            }
                            if(is_topic == 1) {
                                break;
                            }
                        }
                        if(is_topic == 0) {
                            strcpy(SF_topics[SF_topics_count].topic, topic);
                            strcpy(SF_topics[SF_topics_count].clients[0], client_id);
                            SF_topics[SF_topics_count].clients_count = 1;
                            SF_topics_count++;
                        }
                    }

                } else if(strncmp(buffer, "unsubscribe", 11) == 0) {
                    if(strlen(buffer) < 13) {
                        fprintf(stderr, "\n[SERVER ERR] Invalid unsubscribe message\n");
                        return -1;
                    }

                    char *topic = strtok(buffer, " \n");
                    topic = strtok(NULL, " \n");
                    if(topic == NULL) {
                        fprintf(stderr, "\n[SERVER ERR] Invalid unsubscribe message\n");
                        return -1;
                    }

                    if(strlen(topic) > 50) {
                        fprintf(stderr, "\n[SERVER ERR] Invalid unsubscribe message\n");
                        return -1;
                    }

                    char *client_id = NULL;
                    for(int j = 0; j < clients_count; j++) {
                        if(fds[i].fd == clients[j].fd) {
                            client_id = clients[j].id;
                            break;
                        }
                    }
                    for(int j = 0; j < noSF_topics_count; j++) {
                        if(strcmp(noSF_topics[j].topic, topic) == 0) {
                            for(int k = 0; k < noSF_topics[j].clients_count; k++) {
                                if(strcmp(noSF_topics[j].clients[k], client_id) == 0) {
                                    for(int l = k; l < noSF_topics[j].clients_count; l++) {
                                        strcpy(noSF_topics[j].clients[l], noSF_topics[j].clients[l + 1]);
                                    }
                                    noSF_topics[j].clients_count--;
                                }
                            }
                            break;
                        }
                    }
                    for(int j = 0; j < SF_topics_count; j++) {
                        if(strcmp(SF_topics[j].topic, topic) == 0) {
                            for(int k = 0; k < SF_topics[j].clients_count; k++) {
                                if(strcmp(SF_topics[j].clients[k], client_id) == 0) {
                                    for(int l = k; l < SF_topics[j].clients_count; l++) {
                                        strcpy(SF_topics[j].clients[l], SF_topics[j].clients[l + 1]);
                                    }
                                    SF_topics[j].clients_count--;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    close(tcpfd);
    close(udpfd);

    return 0;
}