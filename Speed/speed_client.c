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

    while (option != 'x')
    {
        printf("Select an option: ");
        scanf(" %c", &option);

        // Init variables to default values

        switch(option)
        {
            // Check balance
            case 'c':
                operation = CHECK;
                break;
            // Deposit into account
            case 'd':
                operation = DEPOSIT;
                break;
            // Withdraw from account
            case 'w':
                operation = WITHDRAW;
                break;
            // Exit the bank
            case 'x':
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
                printf("\tTesting... hello from server\n");
                break;
            case INSUFFICIENT:
                printf("\tInsufficient funds for the transaction selected\n");
                break;
            case NO_ACCOUNT:
                printf("\tInvalid acount number entered\n");
                break;
            case BYE:
                printf("\tThanks for connecting to the bank. Good bye!\n");
                break;
            case ERROR: default:
                printf("\tInvalid operation. Try again\n");
                break;
        }
    }
}
