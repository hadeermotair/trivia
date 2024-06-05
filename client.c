//hadeermotair
//i pleadge my honor to have abided by the stevens honor system.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <getopt.h>
#define BUFFER_SIZE 4096
#define MAX_NAME_LEN 128


void error(const char *msg) {
    perror(msg);
    exit(1);
}

void parse_connect(int argc, char** argv, int* server_fd) {
    int opt, portno = 25555;
    char* hostname = "127.0.0.1";

    while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
        switch (opt) {
            case 'i': hostname = optarg; break;
            case 'p': portno = atoi(optarg); break;
            case 'h':
                printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n", argv[0]);
                exit(0);
            default:
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                exit(1);
        }
    }

    struct sockaddr_in server_addr;
    *server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_fd < 0) error("ERROR opening socket");

    struct hostent *server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy((char *)&server_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    server_addr.sin_port = htons(portno);

    if (connect(*server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        error("ERROR connecting");
}

void game_interaction(int server_fd) {
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    int max_fd, n;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        max_fd = server_fd > STDIN_FILENO ? server_fd : STDIN_FILENO;

        // Wait indefinitely for input either from server or from stdin
        n = select(max_fd + 1, &read_fds, NULL, NULL, NULL); // Removed the timeout parameter
        if (n == -1) {
            error("Select error");
        } else {
            if (FD_ISSET(server_fd, &read_fds)) {
                // Receive message from server
                memset(buffer, 0, BUFFER_SIZE);
                n = recv(server_fd, buffer, sizeof(buffer), 0);
                if (n <= 0) {
                    if (n == 0) {
                        printf("Server closed the connection\n");
                    } else {
                        perror("recv error");
                    }
                    break;
                }
                printf("%s\n", buffer);  // Display question and options
            }

            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                // User input
                fgets(buffer, sizeof(buffer), stdin);
                if (send(server_fd, buffer, strlen(buffer), 0) < 0) {
                    perror("send error");
                    exit(1);
                }
            }
        }
    }
}
void setup_client(int server_fd) {
    char name[MAX_NAME_LEN];
    printf("Please enter your name: ");
   
    fgets(name, sizeof(name), stdin);
    printf("Before you start please make sure your only inputs are 1,2,3. anything else will count as wrong\n");
    if (send(server_fd, name, strlen(name), 0) < 0) {
        perror("send error");
        exit(1);
    }
}
int main(int argc, char *argv[]) {
    int server_fd;
    parse_connect(argc, argv, &server_fd);
    setup_client(server_fd); 
    game_interaction(server_fd);
    close(server_fd);
    return 0;
}
