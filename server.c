/*** File: server.c
Name: Johnathan Vu
Pledge: I pledge my honor that I have abided by the Stevens Honor System. ***/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char** argv){
    int    server_fd;
    int    client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in in_addr;
    socklen_t addr_size = sizeof(in_addr);

    int h_flag = 0;
    int portnumber = 25555;
    char* ipaddress = "127.0.0.1";
    char* questions = "questions.txt";
    int opt;
    opterr = 0;

    // use getopt() to parse arguments
    while ((opt = getopt(argc, argv, "f:i:p:h")) != -1) {
        switch (opt)
        {
        case 'h':
            h_flag = 1;
            break;
        case 'f':
            questions = optarg;
            break;
        case 'i':
            ipaddress = optarg;
            break;
        case 'p':
            portnumber = atoi(optarg);
            break;
        case '?':
            if (optopt == 'f' || optopt == 'i' || optopt == 'p') {
                fprintf(stderr, "Error: option '-%c' requires an argument\n", optopt);
            }
            else {
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
            }
            exit(EXIT_FAILURE);
        }
    }

    //printf("h_flag: %d, portnumber: %d, ipaddress: %s, questions: %s\n", h_flag, portnumber, ipaddress, questions);

    //printing help message
    if (h_flag == 1) {
        printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number] [-h]\n", argv[0]);
        printf("\n");
        printf("  -f question_file     Default to \"question.txt\";\n");
        printf("  -i IP_address        Default to \"127.0.0.1\";\n");
        printf("  -p port_number       Default to 25555;\n");
        printf("  -h                   Display this help info.\n");
        exit(EXIT_SUCCESS);
    }

    struct Entry {
        char prompt[1024];
        char options[3][50];
        int answer_idx;
    };

    //Reading the question database
    //define the function to read our file and process questions into our entry array
    int read_questions(struct Entry* arr, char* filename) {
        FILE* file;
        file = fopen(filename, "r");

        if (file == NULL) {
            fprintf(stderr, "Unable to open file.\n");
            fclose(file);
            exit(EXIT_FAILURE);
        }

        char line[1024];
        int count = 0;

        //loop through file
        while (fgets(line, sizeof(line), file)) {

            //remove newlines
            line[strcspn(line, "\n")] = 0;
            
            //check for space
            if (strlen(line) == 0) continue;

            //copy question for this entry
            strcpy(arr[count].prompt,line);

            //get options
            if (fgets(line, sizeof(line), file)) {
                line[strcspn(line, "\n")] = 0;

                //tokenize choices
                char* choice = strtok(line, " ");
                int index = 0;
                while (choice != NULL && index < 3) {
                    strcpy(arr[count].options[index], choice);
                    index++;

                    choice = strtok(NULL, " ");
                }
            }

            //get index of answer
            if (fgets(line, sizeof(line), file)) {
                line[strcspn(line, "\n")] = 0;

                for (int i = 0; i < 3; i++) {
                    if (strcmp(line, arr[count].options[i]) == 0) {
                        arr[count].answer_idx = i;
                        break;
                    }
                }
            }

            count++;

            //skip space
            fgets(line, sizeof(line), file);
        }

        fclose(file);
        return count;
    }

    //create our entry struct array and use our read_questions function
    struct Entry trivia[50];
    int entries = read_questions(trivia, questions);

    //printf("Entries: %d\n", entries);

    /* STEP 1
        Create and set up a socket
    */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(25555); //port number
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //ip address

    /* STEP 2
        Bind the file descriptor with address structure
        so that clients can find the address
    */
    int i =
    bind(server_fd,
            (struct sockaddr *) &server_addr,
            sizeof(server_addr));

    if (i < 0) { perror(""); exit(1); }

    /* STEP 3
        Listen to at most 3 incoming connections
    */
    #define MAX_CONN 3
    if (listen(server_fd, 3) == 0)
        printf("Listening\n");
    else perror("listen");

    /* STEP 4
        Accept connections from clients
        to enable communication
    */
    fd_set myset;
    FD_SET(server_fd, &myset);
    int max_fd = server_fd;
    int n_conn = 0;
    int begingame = 0;
    int question_no = 1; //current question number
    int wait = 0; //bool check if you're wait for responses 
    int ask = 0; //bool check for when to send out question
    int printanswer = 0; //bool check for printing answer
    //int cfds[MAX_CONN];
    //for (int i = 0; i < MAX_CONN; i++) cfds[i] = -1;

    char* receipt = "Read\n";
    int   recvbytes = 0;
    char  buffer[1024];

    struct Player {
        int fd;
        int score;
        char name[128];
        int hasname;
    };

    struct Player players[MAX_CONN];

    //initialize a player struct array
    for (int i = 0; i < MAX_CONN; i++) {
        players[i].fd = -1;
        players[i].score = 0;
        players[i].hasname = 0;
        for (int j = 0; j < 128; j++) {
            players[i].name[j] = 0;
        }
    }

    while (1) {

        // re-initialization
        FD_ZERO(&myset);
        FD_SET(server_fd, &myset);
        max_fd = server_fd;
        for (int i = 0; i < MAX_CONN; i++) {
            if (players[i].fd != -1) {
                FD_SET(players[i].fd, &myset);
                if (players[i].fd > max_fd) max_fd = players[i].fd;
            }
        }

        //Monitoring fds
        select(max_fd + 1, &myset, NULL, NULL, NULL);

        // incoming connections detected
        if (FD_ISSET(server_fd, &myset)) {
            client_fd = accept(server_fd,
                                  (struct sockaddr*)&in_addr,
                                  &addr_size);
            if (n_conn < MAX_CONN) {
                printf("New connection detected!\n");
                n_conn++;
                for (int i = 0; i < MAX_CONN; i++) {
                    if (players[i].fd == -1) {
                        players[i].fd = client_fd;
                        

                        //reading player name
                        char namebuff[128] = { 0 };
                        int namebytes = read(players[i].fd, namebuff, sizeof(namebuff) - 1);
                        if (namebytes > 0) {
                            namebuff[strcspn(namebuff, "\n")] = 0;
                            strcpy(players[i].name, namebuff);
                            players[i].hasname = 1;
                            printf("Hi %s!\n\n", players[i].name); //print player name on server
                        }
                        break;
                    }
                }
                if (n_conn == MAX_CONN && !begingame) {
                    if (players[0].hasname == 1 && players[1].hasname == 1 && players[2].hasname == 1) {
                        printf("The game starts now!\n\n");
                        begingame = 1;
                        ask = 1;
                    }
                }
            }
            else {
                printf("Max connection reached!\n");
                close(client_fd);
            }
        }

        if (begingame) {
            //check if we're out of questions and print out winner
            if (question_no == entries) {
                int highest = players[0].score;
                int winner = 0;
                for (int i = 1; i < 3; i++) {

                    if (players[i].score > highest) {
                        highest = players[i].score;
                        winner = i;
                    }
                }

                printf("Congrats %s!\n", players[winner].name);

                char congratulations[1024];
                sprintf(congratulations, "Congrats %s!\n\n", players[winner].name);
                for (int i = 0; i < MAX_CONN; i++) {
                    if (players[i].fd != -1) {
                        write(players[i].fd, congratulations, strlen(congratulations));
                    }
                }

                //closing all connections and exit loop
                for (int i = 0; i < MAX_CONN; i++) {
                    if (players[i].fd != -1) {
                        close(players[i].fd);
                        players[i].fd = -1;
                    }
                }
                break;
            }

            //wait for responses
            if (wait && !ask && !printanswer) {
                int receivedresponse = 0;

                for (int i = 0; i < MAX_CONN; i++) {
                    if (players[i].fd != -1 && FD_ISSET(players[i].fd, &myset)) {
                        char response[3] = { 0 };
                        int responsebytes = read(players[i].fd, response, 1);

                        //player lost connection
                        if (responsebytes == 0) {
                            printf("Lost Connection!\n");
                            close(players[i].fd);
                            players[i].fd = -1;
                            n_conn--;
                            continue;
                        }

                        //check response and player points
                        if (responsebytes > 0) {
                            response[responsebytes] = '\0';

                            int responseint = response[0] - '0' - 1; //subtract '0' to ascii char to get integer value
                            if (responseint == trivia[question_no - 1].answer_idx) {
                                players[i].score = players[i].score + 1;
                            }
                            else {
                                players[i].score = players[i].score - 1;
                            }
                            receivedresponse = 1;
                            break;
                        }
                    }
                }
                if (receivedresponse) {
                    printanswer = 1; //set flag for printanswer when response received
                    wait = 0;
                    continue;
                }
                continue;
            }

            //send out answer
            if (printanswer && !ask && !wait) {
                int answerindex = trivia[question_no - 1].answer_idx;
                char answer[1024];
                sprintf(answer, "Answer: %s\n\n", trivia[question_no - 1].options[answerindex]);
                for (int i = 0; i < MAX_CONN; i++) {
                    if (players[i].fd != -1) {
                        write(players[i].fd, answer, strlen(answer));
                    }
                }

                //reset answer flag and increment question number
                ask = 1;
                wait = 0;
                printanswer = 0;
                continue;
            }


            //send out a question
            if (ask && !wait && !printanswer) {
                //after sending questions set flags
                wait = 1;
                ask = 0;

                //print question on server
                printf("Question %d: %s\n", question_no, trivia[question_no - 1].prompt);
                printf("1: %s\n", trivia[question_no - 1].options[0]);
                printf("2: %s\n", trivia[question_no - 1].options[1]);
                printf("3: %s\n", trivia[question_no - 1].options[2]);
                printf("\n");

                char buff[2048];
                sprintf(buff, "Question %d: %s\nPress 1: %s\nPress 2: %s\nPress 3: %s\n\n", question_no,
                    trivia[question_no - 1].prompt,
                    trivia[question_no - 1].options[0],
                    trivia[question_no - 1].options[1],
                    trivia[question_no - 1].options[2]);

                //sending players the question
                for (int i = 0; i < MAX_CONN; i++) {
                    if (players[i].fd != -1) {
                        write(players[i].fd, buff, strlen(buff));
                    }
                }
                question_no++;
                continue;
            }
        }
    }

    close(server_fd);

    return 0;
}
