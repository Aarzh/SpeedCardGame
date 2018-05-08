/*
    Speed Project
    Aaron Zajac, Eugenio Leal, Mauricio Rico
*/
// TODO:
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
    int rank_number;
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
    // Store the amount of clients that are stuck
    int number_of_clients_stuck;
    // A pointer to clients
    player_t players[NUM_CLIENTS];
    // Two center piles
    card_t center_pile[2];
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
    // Fixed index position in the speed_data->players array for each thread
    int index_position;
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
void closeSpeed(locks_t * data_locks);
// Signals
void setupHandlers();
void onInterrupt(int signal);
//
int processOperation(thread_data_t * connection_data, char * buffer, int operation, int center_pile_number);
// Cards Logic
void setRank(card_t * card, int card_number);
void setCenterPilesWithRandom(speed_t * speed_data);
void setPlayerCardsWithRandom(speed_t * speed_data);
int validateOperation(speed_t * speed_data, locks_t * data_locks, int players_index_position, int card_selected_hand_index, int center_pile_number);
void placeCardInCenterPile(speed_t * speed_data, locks_t * data_locks, int players_index_position, int card_selected_hand_index, int center_pile_number);
// void verify_cards(thread_data_t * board, int player_num, int card, int pile);

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
    closeSpeed(&data_locks);

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
    // Initialize the number of clients that are stuck
    speed_data->number_of_clients_stuck = 0;

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
    int timeout = 500;      // Time in milliseconds (0.5 seconds)


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

        // Random seed in a loop for better randomness
        srand(time(NULL));

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
                // printf("the index position should be: %d\n", speed_data->number_of_players);
                connection_data->index_position = speed_data->number_of_players;


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

    // Local variable to avoid using large structure/variable names
    int * number_of_players = &connection_data->speed_data->number_of_players;
    // Increment player counter by one
    *number_of_players = *number_of_players + 1;
    // printf("Number of Players: %d\n", *(number_of_players));
    // Testing
    // printf("I will be working at players[%d] index position\n", connection_data->index_position);


    // Note: don't know if this function may be placed in the wrong line and might cause unwanted game behavior
    setPlayerCardsWithRandom(connection_data->speed_data);

    char buffer[BUFFER_SIZE];
    int operation = 0;
    int center_pile_number;
    int status;
    int automatic = 0;

    // Loop to listen for messages from the client
    while(operation != EXIT || !isInterrupted) {
        //printf("The client fd is: %d\n the fixed index position: %d\n", connection_data->connection_fd, connection_data->index_position);

        // Wait for oponent before sending anything to client
        while(*number_of_players < 2) {
            // printf("waiting for oponent\n");
            if(*number_of_players == 2) {
                break;
            }else if(automatic == 1){
                ++*number_of_players;
            }
        }
        if(connection_data->speed_data->players[connection_data->index_position].draw_pile == 0){
            printf("Client %d has won!", connection_data->index_position);
            operation = EXIT;
        }

        printf(" > Sending cards to Client\n");
        // SEND
        // Send the cards to player
        sprintf(buffer, "%d %s %s %s %s %s %s %s",
            0,
            connection_data->speed_data->center_pile[0].rank,
            connection_data->speed_data->center_pile[1].rank,
            connection_data->speed_data->players[connection_data->index_position].hand[0].rank,
            connection_data->speed_data->players[connection_data->index_position].hand[1].rank,
            connection_data->speed_data->players[connection_data->index_position].hand[2].rank,
            connection_data->speed_data->players[connection_data->index_position].hand[3].rank,
            connection_data->speed_data->players[connection_data->index_position].hand[4].rank
            );
        // Testing with hardcoded values
        // sprintf(buffer, "%d %s %s %s %s %s %s %s", 0, "A\0", "10\0", "2\0", "4\0", "J\0", "9\0", "8\0");
        sendString(connection_data->connection_fd, buffer);

        // // RECV
        // // Receive the request
        if( !recvString(connection_data->connection_fd, buffer, BUFFER_SIZE) )
        {
            printf("Client closed the connection 1\n");
            connection_data->speed_data->number_of_players--;
            if(connection_data->speed_data)
            break;
        }
        // Read the data from the socket message
        sscanf(buffer, "%d %d", &operation, &center_pile_number);

        printf(" > Client requested operation '%d'\n", operation);

        // Process the request being careful of data consistency
        status = processOperation(connection_data, buffer, operation, center_pile_number);

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
            printf("Client closed the connection 2\n");
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
void closeSpeed(locks_t * data_locks)
{
    // Free all malloc'd data
    // free(bank_data->account_array);
    free(data_locks->center_pile_mutex);
}

int processOperation(thread_data_t * connection_data, char * buffer, int operation, int center_pile_number)
{
    int status;
    //Aaron on each case: verify_cards(connection_data, 1, operation, connection_data->speed_data->center_pile_1);
    printf("Client: %d entering switch(operation)\n", connection_data->connection_fd);
    switch (operation)
    {
        case FIRST_CARD:
            //validate
            if(validateOperation(connection_data->speed_data, connection_data->data_locks, connection_data->index_position, FIRST_CARD, center_pile_number)) {
                status = OK;
                // modify cards
                placeCardInCenterPile(connection_data->speed_data, connection_data->data_locks, connection_data->index_position, FIRST_CARD, center_pile_number);
            } else {
                status = INVALID_RANK;
            }
            break;
        case SECOND_CARD:
            if(validateOperation(connection_data->speed_data, connection_data->data_locks, connection_data->index_position, SECOND_CARD, center_pile_number)) {
                status = OK;
                // modify cards
                placeCardInCenterPile(connection_data->speed_data, connection_data->data_locks, connection_data->index_position, SECOND_CARD, center_pile_number);
            } else {
                status = INVALID_RANK;
            }
            break;
        case THIRD_CARD:
            if(validateOperation(connection_data->speed_data, connection_data->data_locks, connection_data->index_position, THIRD_CARD, center_pile_number)) {
                status = OK;
                // modify cards
                placeCardInCenterPile(connection_data->speed_data, connection_data->data_locks, connection_data->index_position, THIRD_CARD, center_pile_number);
            } else {
                status = INVALID_RANK;
            }
            break;
        case FOURTH_CARD:
            if(validateOperation(connection_data->speed_data, connection_data->data_locks, connection_data->index_position, FOURTH_CARD, center_pile_number)) {
                status = OK;
                // modify cards
                placeCardInCenterPile(connection_data->speed_data, connection_data->data_locks, connection_data->index_position, FOURTH_CARD, center_pile_number);
            } else {
                status = INVALID_RANK;
            }
            break;
        case FIFTH_CARD:
            if(validateOperation(connection_data->speed_data, connection_data->data_locks, connection_data->index_position, FIFTH_CARD, center_pile_number)) {
                status = OK;
                // modify cards
                placeCardInCenterPile(connection_data->speed_data, connection_data->data_locks, connection_data->index_position, FIFTH_CARD, center_pile_number);
            } else {
                status = INVALID_RANK;
            }
            break;
        case SHUFFLE:
            connection_data->speed_data->number_of_clients_stuck++;
            printf("incremented stuck: %d\n", connection_data->speed_data->number_of_clients_stuck);
            // Wait until both players are stuck
            while(connection_data->speed_data->number_of_clients_stuck < 2) {
                //printf("I will wait...\n");
                // printf("Clients stuck: %d\n", connection_data->speed_data->number_of_clients_stuck);
            }
            printf("both are stuck!\n");
            // Reset the counter
            connection_data->speed_data->number_of_clients_stuck = 0;
            setCenterPilesWithRandom(connection_data->speed_data);
            status = OK;
            break;
        case EXIT:
            status = BYE;
            break;
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
    int random_center_pile_1 = rand() % 13 + 1;
    int random_center_pile_2 = rand() % 13 + 1;
    // Setting Rank Number Integer
    speed_data->center_pile[0].rank_number = random_center_pile_1;
    speed_data->center_pile[1].rank_number = random_center_pile_2;
    // Setting rank strings
    setRank(&speed_data->center_pile[0], random_center_pile_1);
    setRank(&speed_data->center_pile[1], random_center_pile_2);
    // Testing
    printf("%s %s\n", speed_data->center_pile[0].rank, speed_data->center_pile[1].rank);
}

void setPlayerCardsWithRandom(speed_t * speed_data) {
    // printf("Setting Cards With Random Cards\n");
    // printf("\n");

    // Initialize cards with random numbers
    for (int i = 0; i < PLAYER_HAND_SIZE; ++i)
    {
        int player1_random = rand() % 13 + 1;
        int player2_random = rand() % 13 + 1;
        // Setting Rank number
        speed_data->players[0].hand[i].rank_number = player1_random;
        speed_data->players[1].hand[i].rank_number = player2_random;
        // Setting Rank string
        setRank(&speed_data->players[0].hand[i], player1_random);
        setRank(&speed_data->players[1].hand[i], player2_random);
        // Testing
        // printf("%s ", speed_data->players[0].hand[i].rank);
    }
    // printf("\n");
    // printf("\n");
}


int validateOperation(speed_t * speed_data, locks_t * data_locks, int players_index_position, int card_selected_hand_index, int center_pile_number) {

    // if center pile at center pile number is equal to player[indexpos].hand[card selected]
    if(speed_data->center_pile[center_pile_number-1].rank_number == 1)
    { // Ace special case
        if((speed_data->center_pile[center_pile_number-1].rank_number - speed_data->players[players_index_position].hand[card_selected_hand_index].rank_number) == -12 || (speed_data->center_pile[center_pile_number-1].rank_number - speed_data->players[players_index_position].hand[card_selected_hand_index].rank_number) == -1) {
            printf("Testing... ace case success\n");
            return 1;
        } else {
            return 0;
        }
    }
    else if(speed_data->center_pile[center_pile_number-1].rank_number == 13)
    { // King special case
        if((speed_data->center_pile[center_pile_number-1].rank_number - speed_data->players[players_index_position].hand[card_selected_hand_index].rank_number) == 12 || (speed_data->center_pile[center_pile_number-1].rank_number - speed_data->players[players_index_position].hand[card_selected_hand_index].rank_number) == 1) {
            printf("Testing... king case success\n");
            return 1;
        } else {
            return 0;
        }
    }
    else
    {
        if((speed_data->center_pile[center_pile_number-1].rank_number - speed_data->players[players_index_position].hand[card_selected_hand_index].rank_number) == 1 || (speed_data->center_pile[center_pile_number-1].rank_number - speed_data->players[players_index_position].hand[card_selected_hand_index].rank_number) == -1) {
            printf("Testing... any case success\n");
            return 1;
        } else {
            return 0;
        }
    }
}

void placeCardInCenterPile(speed_t * speed_data, locks_t * data_locks, int players_index_position, int card_selected_hand_index, int center_pile_number) {
    // Mutex protect center pile
    // pthread_mutex_lock(&data_locks->center_pile_mutex[center_pile_number-1]);
    // Changing Center Pile Rank String
    setRank(&speed_data->center_pile[center_pile_number-1], speed_data->players[players_index_position].hand[card_selected_hand_index].rank_number);
    // Placing chosen card in the center pile
    speed_data->center_pile[center_pile_number-1].rank_number = speed_data->players[players_index_position].hand[card_selected_hand_index].rank_number;
    // pthread_mutex_lock(&data_locks->center_pile_mutex[center_pile_number-1]);

    // Assigning new random value to the player hand
    srand(time(NULL));
    int new_random = rand() % 13 + 1;
    speed_data->players[players_index_position].hand[card_selected_hand_index].rank_number = new_random;
    setRank(&speed_data->players[players_index_position].hand[card_selected_hand_index], new_random);
}

