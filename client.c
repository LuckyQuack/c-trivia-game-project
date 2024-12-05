/*** File: client.c
Name: Johnathan Vu
Pledge: I pledge my honor that I have abided by the Stevens Honor System.***/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>

void parse_connect(int argc, char** argv, int* server_fd) {
    char* ipaddress = "127.0.0.1";
    int portnumber = 25555;
    int h_flag = 0;
    int opt;
    opterr = 0;
    struct sockaddr_in server_addr;
    socklen_t addr_size = sizeof(server_addr);

    while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
        switch (opt)
        {
        case 'h':
            h_flag = 1;
            break;
        case 'i':
            ipaddress = optarg;
            break;
        case 'p':
            portnumber = atoi(optarg);
            break;
        case '?':
            if (optopt == 'i' || optopt == 'p') {
                fprintf(stderr, "Error: option '-%c' requires an argument\n", optopt);
            }
            else {
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
            }
            exit(EXIT_FAILURE);
        }
    }

    //printing help message
    if (h_flag == 1) {
        printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n", argv[0]);
        printf("\n");
        printf("  -i IP_address        Default to \"127.0.0.1\";\n");
        printf("  -p port_number       Default to 25555;\n");
        printf("  -h                   Display this help info.\n");
        exit(EXIT_SUCCESS);
    }

    /* STEP 1:
    Create a socket to talk to the server;
    */
    *server_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(25555);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* STEP 2:
    Try to connect to the server.
    */
    int connection = connect(*server_fd,
            (struct sockaddr *) &server_addr,
            addr_size);

    if (connection < 0) { perror(""); exit(1); }
}

int main(int argc, char** argv) {
    int    server_fd;

    parse_connect(argc, argv, &server_fd);

    char name[1024];

    //ask player for their name
    printf("Please type your name: "); fflush(stdout);
    scanf("%s", name);
    write(server_fd, name, strlen(name));

    fd_set cset;
    int max_fd = server_fd;

    while (1) {

        //stdin and server
        FD_ZERO(&cset);
        FD_SET(server_fd, &cset);
        FD_SET(STDIN_FILENO, &cset);
        max_fd = server_fd;

        //monitor fds
        select(max_fd + 1, &cset, NULL, NULL, NULL);

        // Receive response from player
        if (FD_ISSET(STDIN_FILENO, &cset)) {
            char rbuffer[1024] = { 0 };
            if (fgets(rbuffer, sizeof(rbuffer), stdin) == NULL) break;
            write(server_fd, rbuffer, strlen(rbuffer));
        }

        /* Receive response from the server */
        if (FD_ISSET(server_fd, &cset)) {
            char buffer[4096] = { 0 }; 
            int totalBytesRead = 0;
            int recvbytes = read(server_fd, buffer, sizeof(buffer) - 1);
            if (recvbytes > 0) {
                totalBytesRead += recvbytes;
                buffer[totalBytesRead] = '\0'; // Null-terminate the current end of string
                
                printf("%s", buffer); fflush(stdout);
            }
            else {
                break;
            }
        }

    }

    close(server_fd);

    return 0;
}