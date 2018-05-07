/*
    Aaron Zajac, Eugenio Leal, Mauricio Rico

    Client program
    This program connects to the server using sockets
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Sockets libraries
#include <netdb.h>
#include <arpa/inet.h>
// Custom libraries
#include "speed_codes.h"
#include "sockets.h"
#include "fatal_error.h"

#define BUFFER_SIZE 1024

typedef struct automatic_game_struct{
    char card;
<<<<<<< HEAD
    int pile
}automatic_t;
=======
    int pile;
} automatic_t;
>>>>>>> 4c848721612092cafadd1b5f28754de0bbf8ac5f

int exit_flag = 0; // exit flag starts as false
int attending = 0;

///// FUNCTION DECLARATIONS
void usage(char * program);
void speedOperations(int connection_fd);
void setupHandlers();
void onInterrupt(int signal);
automatic_t * play(char buffer[BUFFER_SIZE]);

///// MAIN FUNCTION
int main(int argc, char * argv[])
{
    int connection_fd;

    printf("\n=== SPEED CARD GAME ===\n");

    // Check the correct arguments
    if (argc != 3)
    {
        usage(argv[0]);
    }

    // Start the server
    connection_fd = connectSocket(argv[1], argv[2]);
	// Use the bank operations available
    speedOperations(connection_fd);
    // Close the socket
    close(connection_fd);

    return 0;
}

///// FUNCTION DEFINITIONS

/*
    Explanation to the user of the parameters required to run the program
*/
void usage(char * program)
{
    printf("Usage:\n");
    printf("\t%s {server_address} {port_number}\n", program);
    exit(EXIT_FAILURE);
}

/*
    Main menu with the options available to the user
*/
void speedOperations(int connection_fd)
{
    char buffer[BUFFER_SIZE];
    char option = 'c';
    int status;
    operation_t operation;
    int center_pile_number;
    char center_pile_1[3];
    char center_pile_2[3];
    char first_card[3];
    char second_card[3];
    char third_card[3];
    char fourth_card[3];
    char fifth_card[3];
    automatic_t * selection;


    printf("+----------------------------------+\n");
    printf("| How to Play                      |\n");
    printf("+----------------------------------+\n");
    printf("| 1-5) Select Card #               |\n");
    printf("| 6) Shuffle center piles          |\n");
    printf("| 7) Exit program                  |\n");
    printf("+----------------------------------+\n");

    // First wait until player 2 arrives
    printf("Wait for oponent to connect..\n\n");

    attending = 1;

    while (option != 'x')
    {
        printf("Testing.. Receiving cards from Server\n");
        // Receive the cards

        // RECV
        // Receive the response
        if ( !recvString(connection_fd, buffer, BUFFER_SIZE) )
        {
            printf("Server closed the connection\n");
            break;
        }
        //testing buffer
        printf("Buffer:\n %s\n", buffer);
        // Extract the data
        sscanf(buffer, "%d %s %s %s %s %s %s %s",
                &status, center_pile_1, center_pile_2,
                first_card, second_card, third_card, fourth_card, fifth_card);

        // Display cards to player
        printf("+----------------------------------+\n");
        printf("| Center Piles                     |\n");
        printf("+----------------------------------+\n");
        printf("              %s %s \n", center_pile_1, center_pile_2);
        printf("+----------------------------------+\n");
        printf("| Your Hand                        |\n");
        printf("+----------------------------------+\n");
        printf("          %s %s %s %s %s \n", first_card, second_card, third_card, fourth_card, fifth_card);
        printf("+----------------------------------+\n");
        //printf("Select an option: ");
        //scanf(" %c", &option);
        selection = play(buffer);
        selection->card = option;
        selection->pile = center_pile_number;
        // Init variables to default values
        switch(option){
            case '1':
                operation = FIRST_CARD;
                printf("First Card! Select a pile:\n");
                scanf("%d", &center_pile_number);
                break;
            case '2':
                operation = SECOND_CARD;
                printf("Second Card! Select a pile:\n");
                scanf("%d", &center_pile_number);
                break;
            case '3':
                operation = THIRD_CARD;
                printf("Third Card! Select a pile:\n");
                scanf("%d", &center_pile_number);
                break;
            case '4':
                printf("Fourth Card! Select a pile:\n");
                scanf("%d", &center_pile_number);
                operation = FOURTH_CARD;
                break;
            case '5':
                printf("Fifth Card! Select a pile:\n");
                scanf("%d", &center_pile_number);
                operation = FIFTH_CARD;
                break;
            case '6':
                printf("Request sended\n");
                scanf("%d", &center_pile_number);
                operation = SHUFFLE;
                break;
            case '7':
                printf("Thanks for using the program. Bye!\n");
                operation = EXIT;
                exit(0);
                break;
            // Incorrect option
            default:
                printf("Invalid option. Try again ...\n");
                // Skip the rest of the code in the while
                continue;
        }

        // Prepare the message to the server
        sprintf(buffer, "%d %d", operation, center_pile_number);

        // SEND
        // Send the request
        sendString(connection_fd, buffer);

        //printf("Testing.. Receiving status from Server\n");
        // RECV
        // Receive the response
        if ( !recvString(connection_fd, buffer, BUFFER_SIZE) )
        {
            printf("Server closed the connection\n");
            break;
        }
        // Extract the data
        sscanf(buffer, "%d", &status);

        // Print the result
        switch (status){
            case OK:
                printf("\tTesting... SUCCESS!\n");
                break;
            case BYE:
                printf("\tThanks for connecting to the bank. Good bye!%d\n",BYE);
                break;
            case ERROR: default:
                printf("\tInvalid operation. Try again\n");
                break;
        }

        // SEND
        // Send (this send avoids errors)
        sendString(connection_fd, buffer);
    }
}

automatic_t * play(char buffer[BUFFER_SIZE]){
    int verify = 0;
    int i = 3;
<<<<<<< HEAD
    automatic_t * operation;
=======
    automatic_t * operation = malloc(sizeof(*operation));
>>>>>>> 4c848721612092cafadd1b5f28754de0bbf8ac5f
    while(verify != 1){
        if(buffer[1] - buffer[i] == 1 || buffer[1] - buffer[i] == -1){
            operation->card = buffer[i];
            operation->pile = 1;
            return operation;
        }else if(buffer[2] - buffer[i] == 1 || buffer[1] - buffer[i] == -1){
            operation->card = buffer[i];
            operation->pile = 1;
            return operation;
        }else if(i == 8){
            operation->card = '6';
            operation->pile = 0;
            return operation;
        }else{
            i++;
        }
    }
<<<<<<< HEAD
=======
    return operation;
>>>>>>> 4c848721612092cafadd1b5f28754de0bbf8ac5f
}