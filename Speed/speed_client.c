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

///// FUNCTION DECLARATIONS
void usage(char * program);
void speedOperations(int connection_fd);

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

    printf("+----------------------------------+\n");
    printf("| 1-5) Select Card #               |\n");
    printf("| 6) Exit program                  |\n");
    printf("+----------------------------------+\n");

    while (option != 'x')
    {
        printf("Select an option: ");
        scanf(" %c", &option);

        // Init variables to default values

        switch(option)
        {
            case '1':
                operation = FIRST_CARD;
                printf("First Card! Select a pile:\n");
                break;
            case '2':
                operation = SECOND_CARD;
                printf("Second Card! Select a pile:\n");
                break;
            case '3':
                operation = THIRD_CARD;
                printf("Third Card! Select a pile:\n");
                break;
            case '4':
                printf("Fourth Card! Select a pile:\n");
                operation = FOURTH_CARD;
                break;
            case '5':
                printf("Fifth Card! Select a pile:\n");
                operation = FIFTH_CARD;
                break;
            case '6':
                printf("Thanks for using the program. Bye!\n");
                operation = EXIT;
                break;
            // Incorrect option
            default:
                printf("Invalid option. Try again ...\n");
                // Skip the rest of the code in the while
                continue;
        }

        // Prepare the message to the server
        sprintf(buffer, "%d", operation);

        // SEND
        // Send the request
        sendString(connection_fd, buffer);

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
        switch (status)
        {
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
    }
}
