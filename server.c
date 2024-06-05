//Hadeer motair
//I pleadge my honor to have abided by the stevens honor system.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <limits.h>

// Define constants used throughout the code
#define MAX_CLIENTS 3
#define BUFFER_SIZE 4096
#define MAX_QUESTIONS 50
#define MAX_NAME_LEN 128
void end_game_for_all(void); // Prototype for function that handles game termination

// Structure to store each trivia question and its options
typedef struct {
    char prompt[2048];
    char options[3][50];
    int answer_idx;
} Entry;

// Structure for each player connected to the game
typedef struct {
    int fd; // File descriptor for the socket connection
    int score; // Player's score
    char name[MAX_NAME_LEN]; // Player's name
    int has_answered; // Flag to check if the player has answered the current question
} Player;

// Global arrays to hold client and question data
Player clients[MAX_CLIENTS] = {0};
Entry questions[MAX_QUESTIONS] = {0};
int question_count = 0;
int client_count = 0;

// Error handling function
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Function to handle new client connections
void handle_new_connection(int server_fd) {
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    int newsockfd = accept(server_fd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0) error("ERROR on accept");

    if (client_count >= MAX_CLIENTS) {
        printf("Maximum player limit reached. No more connections allowed.\n");
        close(newsockfd); // Reject connection
        return;
    }

    printf("New connection detected!\n");
    Player *new_player = &clients[client_count++];
    new_player->fd = newsockfd;
    new_player->score = 0;
    new_player->has_answered = 0;

    char name_buf[MAX_NAME_LEN];
    int n = recv(newsockfd, name_buf, MAX_NAME_LEN-1, 0);
    if (n > 0) {
        name_buf[n] = '\0';
        strncpy(new_player->name, name_buf, MAX_NAME_LEN);
        printf("Hi %s, welcome to the cs 392 trivia game!\n", new_player->name);
    } else {
        if (n < 0) perror("ERROR reading from socket");
        client_count--;
        close(newsockfd);
    }

    if (client_count == MAX_CLIENTS) {
        printf("Maximum number of players connected. Game will start soon.\n");
    }
}

// Function to read trivia questions from a file
int read_questions(Entry *entries, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open question file");
        return 0;
    }

    char line[BUFFER_SIZE];
    int index = 0;
    while (index < MAX_QUESTIONS && fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '\n') continue; // Skip empty lines
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0'; // Remove newline character

        strncpy(entries[index].prompt, line, sizeof(entries[index].prompt) - 1);

        if (fgets(line, sizeof(line), file) == NULL) break;
        newline = strchr(line, '\n');
        if (newline) *newline = '\0';

        char *token = strtok(line, " ");
        int opt_count = 0;
        while (token != NULL && opt_count < 3) {
            strncpy(entries[index].options[opt_count], token, sizeof(entries[index].options[opt_count]) - 1);
            token = strtok(NULL, " ");
            opt_count++;
        }

        if (fgets(line, sizeof(line), file) == NULL) break;
        newline = strchr(line, '\n');
        if (newline) *newline = '\0';

        entries[index].answer_idx = -1;
        for (int i = 0; i < 3; i++) {
            if (strcmp(entries[index].options[i], line) == 0) {
                entries[index].answer_idx = i;
                break;
            }
        }
        index++;
    }
    fclose(file);
    return index;
}

// Function to set up the server socket
void setup_server(int *server_fd, int port, const char* ip_address) {
    struct sockaddr_in serv_addr;
    *server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_fd < 0) error("ERROR opening socket");
    
    int optval = 1;
    setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip_address);
    serv_addr.sin_port = htons(port);
    
    if (bind(*server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    
    if (listen(*server_fd, MAX_CLIENTS) < 0)
        error("ERROR on listening");
    
    printf("Welcome to the cs392 trivia game\n");
    printf("Server ready, waiting for players...\n");
}

// Function to send a trivia question to all connected clients
void send_question_to_all(int question_index) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "Question %d: %s\n1: %s\n2: %s\n3: %s\n",
        question_index + 1, questions[question_index].prompt,
        questions[question_index].options[0],
        questions[question_index].options[1],
        questions[question_index].options[2]);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != 0) {
            send(clients[i].fd, buffer, strlen(buffer), 0);
        }
    }
}

// Function to receive and process answers from clients
int receive_answers(int question_index) {
    fd_set read_fds;
    int max_fd = 0;
    
    // Loop until a valid answer is received
    while (1) {
        FD_ZERO(&read_fds);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &read_fds);
                if (clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            error("Select error");
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0 && FD_ISSET(clients[i].fd, &read_fds)) {
                char buffer[1024];
                int nbytes = recv(clients[i].fd, buffer, sizeof(buffer) - 1, 0);
                if (nbytes <= 0) {
                    printf("Player %s disconnected. Ending game for all.\n", clients[i].name);
                    end_game_for_all();  // Handle disconnections
                    return 0;
                }

                buffer[nbytes] = '\0';
                int answer = atoi(buffer) - 1;

                // Message for whether the answer was correct
                char response_msg[1024];
                if (answer == questions[question_index].answer_idx) {
                    clients[i].score += 1;
                    snprintf(response_msg, sizeof(response_msg), "Correct! The answer was: %s\n", questions[question_index].options[answer]);
                } else {
                    clients[i].score -= 1;
                    snprintf(response_msg, sizeof(response_msg), "Wrong! The correct answer was: %s\n", questions[question_index].options[questions[question_index].answer_idx]);
                }

                // Broadcast the response and score updates to all clients
                char score_update[1024];
                snprintf(score_update, sizeof(score_update), "Current Scores:\n");
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (clients[j].fd != 0) {  // Make sure the client is still connected
                        char score_line[256];
                        snprintf(score_line, sizeof(score_line), "%s: %d\n", clients[j].name, clients[j].score);
                        strncat(score_update, score_line, sizeof(score_update) - strlen(score_update) - 1);
                    }
                }

                // Combine messages and send to all clients
                strcat(response_msg, score_update);
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (clients[j].fd != 0) {
                        send(clients[j].fd, response_msg, strlen(response_msg), 0);
                    }
                }

                return 1; // Return after processing at least one answer
            }
        }
    }
}

// Function to terminate the game and close all connections when a client disconnects
void end_game_for_all() {
    char *message = "Game over: a player has disconnected. Closing all connections.\n";
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd > 0) {
                    send(clients[i].fd, message, strlen(message), 0);
                    close(clients[i].fd);
                    clients[i].fd = 0; // Mark as disconnected
                }
            }
            exit(EXIT_FAILURE); // Optionally exit the server completely, or reset game state
        }

// Function to determine the winner(s) of the game based on scores
void declare_winner() {
    int max_score = INT_MIN;
    int num_winners = 0;
    Player* winners[MAX_CLIENTS]; // Array to hold pointers to tied players

    // Determine the highest score
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd > 0 && clients[i].score > max_score) {
            max_score = clients[i].score;
            winners[0] = &clients[i];
            num_winners = 1;
        } else if (clients[i].fd > 0 && clients[i].score == max_score) {
            winners[num_winners++] = &clients[i];
        }
    }

    // Prepare the message based on the number of winners
    char buffer[BUFFER_SIZE];
    if (num_winners > 1) {
        snprintf(buffer, sizeof(buffer), "It's a tie! Winners are: ");
        for (int i = 0; i < num_winners; i++) {
            char player_info[256];
            snprintf(player_info, sizeof(player_info), "%s (%d points), ", winners[i]->name, winners[i]->score);
            strncat(buffer, player_info, sizeof(buffer) - strlen(buffer) - 1);
        }
        // Remove the last comma and space
        size_t len = strlen(buffer);
        buffer[len - 2] = '\0'; // Overwrite the last comma with null terminator
        strcat(buffer, "\n");
    } else if (num_winners == 1) {
        snprintf(buffer, sizeof(buffer), "Congrats, %s! You are the winner with a score of %d points!\n", winners[0]->name, winners[0]->score);
    }

    // Broadcast the result to all clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd > 0) {
            send(clients[i].fd, buffer, strlen(buffer), 0);
        }
    }
    printf("%s", buffer); // Log result on server
}

// Main function to execute the server logic
int main(int argc, char *argv[]) {
    int server_fd, opt;
    char *ip_address = "127.0.0.1";
    int port = 25555;
    char *filename = "question.txt";

    // Process command line arguments
    while ((opt = getopt(argc, argv, "f:i:p:h")) != -1) {
        switch (opt) {
            case 'f':
                filename = optarg;
                break;
            case 'i':
                ip_address = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number]\n", argv[0]);
                return 0;
            default:
                fprintf(stderr, "Unknown option '-%c'\n", optopt);
                return 1;
        }
    }

    // Load questions from file
    question_count = read_questions(questions, filename);
    if (question_count == 0) {
        fprintf(stderr, "No questions loaded.\n");
        return 1;
    }

    // Initialize the server
    setup_server(&server_fd, port, ip_address);

    // Handle new connections until the maximum number of clients is reached
    while (client_count < MAX_CLIENTS) {
        handle_new_connection(server_fd);
    }
    printf("Game starts now!\n");
    for (int q = 0; q < question_count; q++) {
        send_question_to_all(q);
        if (!receive_answers(q)) break; // Exit if error in receiving answers
    }

    // Declare the winner and end the game
    declare_winner();

    close(server_fd);
    return 0;
}

