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

// Custom libraries
#include "bank_codes.h"
#include "sockets.h"
#include "fatal_error.h"

#define PILE_CENTER 52
#define BUFFER_SIZE 1024
#define PILE_CORNERS 5

///// Structure definitions

typedef struct player_struct {

} player_t;

typedef struct speed_struct {
    // Store the total number of operations performed
    int total_operations;
    // A pointer to clients
    player_t * players;
} speed_t;

// Structure for the mutexes to keep the data consistent
typedef struct locks_struct {
    // Mutex for the number of transactions variable
    pthread_mutex_t hand_r;
    // Mutex array for the operations on the accounts
    pthread_mutex_t hand_l;
} locks_t;

// Data that will be sent to each structure
typedef struct data_struct {
    // The file descriptor for the socket
    int connection_fd;
    // A pointer to a locks structure
    locks_t * hand_locks;
} thread_data_t;

typedef struct card_struct{
    int num;
    int spin;
}card_t;


typedef struct board_struct{
    card_t pile_left[PILE_CORNERS];
    card_t pile_right[PILE_CORNERS];
    card_t pile_centerR[PILE_CENTER];
    card_t pile_centerL[PILE_CENTER];
}board_t;


// Global Varibles
int isInterrupted = 0;

///// FUNCTION DECLARATIONS
// HACE FALTA CHECAR SI DEBEMOS DE HACER UN STRUCT DE BARAJAS
// QUE ESTE DENTRO DE OTRO STRUCT DE PLAYER QUE VA A TENER TURNO Y BARAJAS DEL JUGADOR
void usage(char * program);
void initSpeed(locks_t * data_locks);
void waitForConnections(int server_fd, locks_t * data_locks);
void * attentionThread(void * arg);
void closeBoard(locks_t * data_locks);
// Signals
void setupHandlers();
void onInterrupt(int signal);
void shufflePile(board_t & piles);
void randomize (board_t &pile );
void swap (card_t a, card_t b);

///// MAIN FUNCTION
//SI SE HACE EL STRUCT METERLO COMO BANK_DATA
int main(int argc, char * argv[])
{
    int server_fd;
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
    initSpeed(&data_locks);

	// Show the IPs assigned to this computer
	printLocalIPs();
    // Start the server
    server_fd = initServer(argv[1], MAX_QUEUE);
	// Listen for connections from the clients
    waitForConnections(server_fd, &data_locks);
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
void initSpeed(locks_t * data_locks)
{
    // // Set the number of transactions
    // bank_data->total_transactions = 0;

    // // Allocate the arrays in the structures
    // bank_data->account_array = malloc(NUM_ACCOUNTS * sizeof (account_t));
    // // Allocate the arrays for the mutexes
    // data_locks->account_mutex = malloc(NUM_ACCOUNTS * sizeof (pthread_mutex_t));

    // // Initialize the mutexes, using a different method for dynamically created ones
    // //data_locks->transactions_mutex = PTHREAD_MUTEX_INITIALIZER;
    // pthread_mutex_init(&data_locks->transactions_mutex, NULL);
    // for (int i=0; i<NUM_ACCOUNTS; i++)
    // {
    //     //data_locks->account_mutex[i] = PTHREAD_MUTEX_INITIALIZER;
    //     pthread_mutex_init(&data_locks->account_mutex[i], NULL);
    // }
}

/*
    Main loop to wait for incomming connections
*/
//MISSING PLAYER STRUCT
void waitForConnections(int server_fd, locks_t * data_locks)
{
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

    while (1)
    {
		//// POLL
        // Create a structure array to hold the file descriptors to poll
        struct pollfd test_fds[1];
        // Fill in the structure
        test_fds[0].fd = server_fd;
        test_fds[0].events = POLLIN;    // Check for incomming data
        // Check if there is any incomming communication
        poll_response = poll(test_fds, 1, timeout);

		// Error when polling
        if (poll_response == -1)
        {
            // Test if the error was caused by an interruption
            if (errno == EINTR)
            {
                printf("Poll did not finish. The program was interrupted");
            }
            else
            {
                fatalError("ERROR: poll");
            }
        }
		// Timeout finished without reading anything
        else if (poll_response == 0)
        {
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

				// CREATE A THREAD
                status = pthread_create(&new_tid, NULL, attentionThread, (void *)connection_data);
                if (status != 0)
                {
                    perror("ERROR: pthread_create");
                    exit(EXIT_FAILURE);
                }
                printf("Thread created with ID: %ld\n", new_tid);

            }
        }
    }


}

/*
    Hear the request from the client and send an answer
*/
void * attentionThread(void * arg)
{
    // Receive the data for the bank, mutexes and socket file descriptor

    // Loop to listen for messages from the client

        // Receive the request

        // Process the request being careful of data consistency

        // Update the number of transactions

        // Send a reply

    pthread_exit(NULL);
}

/*
    Free all the memory used for the bank data
*/
//MISSING PLAYER STRUCT
//HAY UN ERROR ACA
void closeBoard(locks_t * data_locks)
{
    printf("DEBUG: Clearing the memory for the thread\n");
    // Free all malloc'd data
    // free(bank_data->account_array);
    // free(data_locks->account_mutex);
}

//testing my branch


void shufflePile(board_t &pile){
        for(int i = 0; i<PILE_CENTER; i++){
            for(int j = 0; j<PILE_CENTER; j++){
                if(pile->pile_centerL[i] != 0 && pile->pile_centerR[j] == 0){
                    pile->pile_centerR[j] = pile->pile_centerL[i];
                }else if(pile->pile_centerL[i] == 0 && pile->pile_centerR[j] == 0){
                    i++;
                }else if(pile->pile_centerL[i] != 0 && pile->pile_centerR[j] != 0){
                    j++;
                }else{//cuando L no tiene y R si
                    ++i;
                    j++;
                }
            }
        }
        randomize(&pile);
        for(int i = 0; i<PILE_CENTER/2; i++){
            for(int j = 0; j<PILE_CENTER/2; j++){
                if(pile->pile_centerR[i] != 0 && pile->pile_centerL[j] == 0){
                    pile->pile_centerL[j] = pile->pile_centerR[i];
                }else if(pile->pile_centerL[i] == 0 && pile->pile_centerR[j] == 0){
                    j++;
                }else if(pile->pile_centerL[i] != 0 && pile->pile_centerR[j] != 0){
                    i++;
                }else{
                    ++j;
                }
            }
        }
    }
}

void swap (card_t a, card_t b)
{
    card_t temp = *a;
    *a = *b;
    *b = temp;
}
 
void randomize (board_t &pile )
{
    // Use a different seed value so that we don't get same
    // result each time we run this program
    srand ( time(NULL) );
 
    // Start from the last element and swap one by one. We don't
    // need to run for the first element that's why i > 0
    for (int i = PILE_CENTER; i > 0; i--)
    {
        // Pick a random index from 0 to i
        int j = rand() % (i+1);
 
        // Swap arr[i] with the element at random index
        swap(&pile->pile_centerR[i], &pile->pile_centerR[j]);
    }
}