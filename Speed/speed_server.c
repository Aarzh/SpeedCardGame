/*
    Speed Project
    Aaron Zajac, Eugenio Leal, Mauricio Rico
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Signals library
#include <errno.h>
#include <signal.h>
// Sockets libraries
#include <netdb.h>
#include <sys/poll.h>
// Posix threads library
#include <pthread.h>
// Pretty Unicode Card Suits
// #include <wchar.h>
// #include <locale.h>

// Custom libraries
#include "speed_codes.h"
#include "sockets.h"
#include "fatal_error.h"

#define NUM_CLIENTS 2
#define BUFFER_SIZE 1024
#define MAX_QUEUE 5
// #define BLACK_SPADE_SUIT 0x2660     // ♠
// #define WHITE_HEART_SUIT 0x2661     // ♡
// #define WHITE_DIAMOND_SUIT 0x2662   // ♢
// #define BLACK_CLUB_SUIT 0x2663      // ♣
#define MAX_RANK_SIZE 3
#define CENTER_PILE_SIZE 52
#define REPLACEMENT_PILE_SIZE 5
#define PLAYER_HAND_SIZE 5
#define DRAW_PILE_SIZE 15

///// Structure definitions

typedef struct card_struct {
    char rank[MAX_RANK_SIZE];
} card_t;

// Structure for the player's data
typedef struct player_struct {
    // Players Hand
    card_t hand[PLAYER_HAND_SIZE];
    // Players Draw Pile (how many cards left to win)
    int draw_pile;
} player_t;

// Structure for the game's data
typedef struct speed_struct {
    // Store the number of players
    int number_of_players;
    // A pointer to clients
    player_t players[NUM_CLIENTS];
    // Two center piles
    card_t center_pile_1;
    card_t center_pile_2;
    // Two replacement piles
    card_t replacement_pile_1[REPLACEMENT_PILE_SIZE];
    card_t replacement_pile_2[REPLACEMENT_PILE_SIZE];
} speed_t;

// TODO: decide if you can have multiple games at a time or not
// Structure for the mutexes to keep the data consistent
typedef struct locks_struct {
    // Mutex array
    pthread_mutex_t * center_pile_mutex;
} locks_t;

// Data that will be sent to each thread
typedef struct data_struct {
    // The file descriptor for the socket
    int connection_fd;
    // A pointer to the speed data structure
    speed_t * speed_data;
    // A pointer to a locks structure
    locks_t * data_locks;
} thread_data_t;


// Global Varibles
int isInterrupted = 0;

///// FUNCTION DECLARATIONS
// HACE FALTA CHECAR SI DEBEMOS DE HACER UN STRUCT DE BARAJAS
// QUE ESTE DENTRO DE OTRO STRUCT DE PLAYER QUE VA A TENER TURNO Y BARAJAS DEL JUGADOR
void usage(char * program);
void initSpeed(speed_t * speed_data, locks_t * data_locks);
void waitForConnections(int server_fd, speed_t * speed_data, locks_t * data_locks);
void * attentionThread(void * arg);
void closeBoard(locks_t * data_locks);
// Signals
void setupHandlers();
void onInterrupt(int signal);
//
int processOperation(speed_t * speed_data, locks_t * data_locks, char * buffer, int operation);
// Cards Logic
void setRank(card_t * card, int card_number);
void setCenterPilesWithRandom(speed_t * speed_data);
void setPlayerCardsWithRandom(speed_t * speed_data);
// void shufflePile(board_t * piles);
// void randomize (board_t * pile);
// void swap (card_t * a, card_t * b);

///// MAIN FUNCTION
int main(int argc, char * argv[])
{
    int server_fd;
    speed_t speed_data;
    locks_t data_locks;

    printf("\n=== SPEED SERVER ===\n");

    // Check the correct arguments
    if (argc != 2)
    {
        usage(argv[0]);
    }

    // Configure the handler to catch SIGINT
    setupHandlers();

    // Initialize the data structures
    initSpeed(&speed_data, &data_locks);

	// Show the IPs assigned to this computer
    printLocalIPs();
    // Start the server
    server_fd = initServer(argv[1], MAX_QUEUE);
	// Listen for connections from the clients
    waitForConnections(server_fd, &speed_data, &data_locks);
    // Close the socket
    close(server_fd);

    // Clean the memory used
    closeBoard(&data_locks);

    // Finish the main thread
    pthread_exit(NULL);

    return 0;
}

///// FUNCTION DEFINITIONS

/*
    Explanation to the user of the parameters required to run the program
*/
void usage(char * program)
{
    printf("Usage:\n");
    printf("\t%s {port_number}\n", program);
    exit(EXIT_FAILURE);
}

void onInterrupt(int signal) {
    // Set the global variable to be true
    isInterrupted = 1;
}

/*
    Modify the signal handlers for specific events
*/
void setupHandlers()
{
    struct sigaction new_action;

    // Change the action for the Ctrl-C input (SIGINT)
    new_action.sa_handler = onInterrupt;
    // Set the mask to the empty set
    if ( sigemptyset(&new_action.sa_mask) == -1 )
    {
        perror("ERROR: sigemptyset");
        exit(EXIT_FAILURE);
    }
    // Set the signal handler
    if ( sigaction(SIGINT, &new_action, NULL) == -1 )
    {
        perror("ERROR: sigaction");
        exit(EXIT_FAILURE);
    }
}

/*
    Function to initialize all the information necessary
    This will allocate memory for the accounts, and for the mutexes
*/
// HACEN FALTA CORRECCIONES DEPENDIENDO EL STRUCT
void initSpeed(speed_t * speed_data, locks_t * data_locks){
    // Initialize the number of players to zero
    speed_data->number_of_players = 0;

    // Allocate the arrays for the mutexes
    data_locks->center_pile_mutex = malloc(NUM_CLIENTS * sizeof (pthread_mutex_t));

    // Initializing mutex array to protect center piles for as many clients the program allows
    for (int i=0; i<NUM_CLIENTS; i++)
    {
        // Initializing mutex array using either of the following methods
        // data_locks->center_pile_mutex[i] = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_init(&data_locks->center_pile_mutex[i], NULL);
    }

    // Initialize players draw piles
    speed_data->players[0].draw_pile = DRAW_PILE_SIZE;
    speed_data->players[1].draw_pile = DRAW_PILE_SIZE;

    setCenterPilesWithRandom(speed_data);

}

/*
    Main loop to wait for incomming connections
*/
void waitForConnections(int server_fd, speed_t * speed_data, locks_t * data_locks){

    struct sockaddr_in client_address;
    socklen_t client_address_size;
    char client_presentation[INET_ADDRSTRLEN];
    int client_fd;
    pthread_t new_tid;
    thread_data_t * connection_data = NULL;
    int status;
    int poll_response;
    int timeout = 500;		// Time in milliseconds (0.5 seconds)

    // Get the size of the structure to store client information
    client_address_size = sizeof client_address;

    while (!isInterrupted){
        //// POLL
        // Create a structure array to hold the file descriptors to poll
        struct pollfd test_fds[1];
        // Fill in the structure
        test_fds[0].fd = server_fd;
        test_fds[0].events = POLLIN;    // Check for incomming data
        // Testing
        // printf("Testing... Number of Players: %d\n", speed_data->number_of_players);
        // Check if there is any incomming communication
        poll_response = poll(test_fds, 1, timeout);

		// Error when polling
        if (poll_response == -1){
            // Test if the error was caused by an interruption
            if (errno == EINTR){
                printf("\nThe program was interrupted\n");
            } else {
                fatalError("ERROR: poll");
            }
        } else if (poll_response == 0) { // Timeout finished without reading anything
            //printf("No response after %d seconds\n", timeout);
        }
    	// There is something ready at the socket
        else
        {
            // Check the type of event detected
            if (test_fds[0].revents & POLLIN)
            {
    			// ACCEPT
    			// Wait for a client connection
                printf("Ready to accept\n");
                client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_size);
                if (client_fd == -1)
                {
                    fatalError("ERROR: accept");
                }

    			// Get the data from the client
                inet_ntop(client_address.sin_family, &client_address.sin_addr, client_presentation, sizeof client_presentation);
                printf("Received incomming connection from %s on port %d\n", client_presentation, client_address.sin_port);

    			// Prepare the structure to send to the thread
                connection_data = malloc(sizeof (thread_data_t));
                connection_data->connection_fd = client_fd;
                connection_data->speed_data = speed_data;
                connection_data->data_locks = data_locks;

    			// CREATE A THREAD
                status = pthread_create(&new_tid, NULL, attentionThread, (void *)connection_data);
                if (status != 0)
                {
                    perror("ERROR: pthread_create");
                    exit(EXIT_FAILURE);
                }
                long id = (long)new_tid; // Cast to long to avoid warnings

                printf("Thread created with ID: %ld\n", id); // Print thread id

            }
        }
    }
}

/*
    Hear the request from the client and send an answer
*/
void * attentionThread(void * arg){

    // Receive the data for the bank, mutexes and socket file descriptor
    thread_data_t * connection_data = (thread_data_t *) arg;
    printf("\n");
    printf("Player %d connected!\n", ++connection_data->speed_data->number_of_players);
    printf("\n");

    setPlayerCardsWithRandom(connection_data->speed_data);

    //int count = connection_data->speed_data->number_of_players;
    char buffer[BUFFER_SIZE];
    int operation = 0;
    int status;

    // Loop to listen for messages from the client
    while(operation != EXIT || !isInterrupted) {

        // while(count < 2) {
        //     printf("Waiting for oponent...\n");
        //     if(isInterrupted == 1){
        //         count = 1;
        //     }
        // }
        printf(" > Sending cards to Client\n");
        // SEND
        // Send the cards to player
        sprintf(buffer, "%d %s %s %s %s %s %s %s",
            0,
            connection_data->speed_data->center_pile_1.rank,
            connection_data->speed_data->center_pile_2.rank,
            connection_data->speed_data->players[0].hand[0].rank,
            connection_data->speed_data->players[0].hand[1].rank,
            connection_data->speed_data->players[0].hand[2].rank,
            connection_data->speed_data->players[0].hand[3].rank,
            connection_data->speed_data->players[0].hand[4].rank
            );
        // Testing with hardcoded values
        // sprintf(buffer, "%d %s %s %s %s %s %s %s", 0, "A\0", "10\0", "2\0", "4\0", "J\0", "9\0", "8\0");
        sendString(connection_data->connection_fd, buffer);


        // // RECV
        // // Receive the request
        if( !recvString(connection_data->connection_fd, buffer, BUFFER_SIZE) )
        {
            printf("Client closed the connection\n");
            break;
        }
        // Read the data from the socket message
        sscanf(buffer, "%d", &operation);

        printf(" > Client requested operation '%d'\n", operation);

        // Process the request being careful of data consistency
        status = processOperation(connection_data->speed_data, connection_data->data_locks, buffer, operation);

        // Send a reply
        printf(" > Sending status to Client\n");

        // Prepare the response to the client
        sprintf(buffer, "%d", status);
        // SEND
        // Send the response
        sendString(connection_data->connection_fd, buffer);

        // RECV
        // Receive (this receive avoids errors)
        if( !recvString(connection_data->connection_fd, buffer, BUFFER_SIZE) )
        {
            printf("Client closed the connection\n");
            break;
        }
    }

    // Free memory sent to this thread
    free(connection_data);

    pthread_exit(NULL);

    exit(0);
}

/*
    Free all the memory used for the bank data
*/
// ADD FUTURE STRUCTS THAT YOU MALLOC'D
void closeBoard(locks_t * data_locks)
{
    // Free all malloc'd data
    // free(bank_data->account_array);
    free(data_locks->center_pile_mutex);
}

int processOperation(speed_t * speed_data, locks_t * data_locks, char * buffer, int operation)
{
    int status;

    switch (operation)
    {
        case FIRST_CARD:
        status = 0;
        break;
        case SECOND_CARD:
        status = 0;
        break;
        case THIRD_CARD:
        status = 0;
        break;
        case FOURTH_CARD:
        status = 0;
        break;
        case FIFTH_CARD:
        status = 0;
        break;
        case EXIT:
        status = BYE;
        // Invalid message
        default:
            // Print an error locally
        printf("Unknown operation requested\n");
            // Set the error status
        status = ERROR;
        break;
    }

    return status;
}

void setRank(card_t * card, int card_number) {
    if(card_number == 1) {
        strcpy(card->rank, "A\0");
    } else if(card_number == 2) {
        strcpy(card->rank, "2\0");
    } else if(card_number == 3) {
        strcpy(card->rank, "3\0");
    } else if(card_number == 4) {
        strcpy(card->rank, "4\0");
    } else if(card_number == 5) {
        strcpy(card->rank, "5\0");
    } else if(card_number == 6) {
        strcpy(card->rank, "6\0");
    } else if(card_number == 7) {
        strcpy(card->rank, "7\0");
    } else if(card_number == 8) {
        strcpy(card->rank, "8\0");
    } else if(card_number == 9) {
        strcpy(card->rank, "9\0");
    } else if(card_number == 10) {
        strcpy(card->rank, "10\0");
    } else if(card_number == 11) {
        strcpy(card->rank, "J\0");
    } else if(card_number == 12) {
        strcpy(card->rank, "Q\0");
    } else if(card_number == 13) {
        strcpy(card->rank, "K\0");
    }
}

void setCenterPilesWithRandom(speed_t * speed_data) {
    printf("Setting Center Piles With Random Cards\n");
    srand(time(NULL));
    // Initialize center piles with random numbers
    setRank(&speed_data->center_pile_1, rand() % 13 + 1);
    setRank(&speed_data->center_pile_2, rand() % 13 + 1);
}

void setPlayerCardsWithRandom(speed_t * speed_data) {
    printf("Setting Cards With Random Cards\n");
    printf("\n");
    srand(time(NULL));
    // Initialize cards with random numbers
    for (int i = 0; i < PLAYER_HAND_SIZE; ++i)
    {
        int random_number = rand() % 13 + 1;
        setRank(&speed_data->players[0].hand[i], random_number);
        setRank(&speed_data->players[1].hand[i], random_number);
        printf("%s ", speed_data->players[0].hand[i].rank);
    }
    printf("\n");
    printf("\n");
}

// void shufflePile(board_t * pile)
// {
//     for(int i = 0; i<PILE_CENTER; i++){
//         for(int j = 0; j<PILE_CENTER; j++){
//             if(pile->pile_centerL[i] != 0 && pile->pile_centerR[j] == 0){
//                 pile->pile_centerR[j] = pile->pile_centerL[i];
//             }else if(pile->pile_centerL[i] == 0 && pile->pile_centerR[j] == 0){
//                 i++;
//             }else if(pile->pile_centerL[i] != 0 && pile->pile_centerR[j] != 0){
//                 j++;
//             }else{//cuando L no tiene y R si
//                 ++i;
//                 j++;
//             }
//         }
//     }
//     randomize(pile);
//     for(int i = 0; i<PILE_CENTER/2; i++){
//         for(int j = 0; j<PILE_CENTER/2; j++){
//             if(pile->pile_centerR[i] != 0 && pile->pile_centerL[j] == 0){
//                 pile->pile_centerL[j] = pile->pile_centerR[i];
//             }else if(pile->pile_centerL[i] == 0 && pile->pile_centerR[j] == 0){
//                 j++;
//             }else if(pile->pile_centerL[i] != 0 && pile->pile_centerR[j] != 0){
//                 i++;
//             }else{
//                 ++j;
//             }
//         }
//     }
// }


// void swap (card_t * a, card_t * b)
// {
//     card_t temp = *a;
//     *a = *b;
//     *b = temp;
// }

// void randomize (board_t * pile )
// {
//     // Use a different seed value so that we don't get same
//     // result each time we run this program
//     srand ( time(NULL) );

//     // Start from the last element and swap one by one. We don't
//     // need to run for the first element that's why i > 0
//     for (int i = PILE_CENTER; i > 0; i--)
//     {
//         // Pick a random index from 0 to i
//         int j = rand() % (i+1);

//         // Swap arr[i] with the element at random index
//         swap(&pile->pile_centerR[i], &pile->pile_centerR[j]);
//     }
// }
