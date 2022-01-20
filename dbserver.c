#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#define TEXT_SIZE 2048
#define MAX_TEXT 512

/* Setting up struct to use for shared memory 
	writen_by_you is to know if we are writing to the console
	or not.
	accountNo is the shared memory for the account number.
	encodedPin is the shared memory for the encoded pin.
	fundsAvailable is shared memory for the funds that are available 		in the account.
	fundsRequest is the shared memory for the funds request by the 	  user. */
struct account
{
	int written_by_you;
	char accountNo[TEXT_SIZE];
	char encodedPIN[TEXT_SIZE];
	float fundsAvailable;
	float fundsRequested;
};

/* Setting up struct to use for message queue
	msg_type the type of message being sent (either a 1 or 2 for 		our case).
	some_text what is being sent over the message queue	*/
struct my_msg
{
	long int msg_type;
	char some_text[BUFSIZ];
};

int main(int argc, char **argv)
{
	/*creating asem with out sembuf struct that will be used to lock and unlock our semaphores
sem_num is our semaphore number which is iniziled to 0 
sem_op is out operation type, if 1 then adds to the semval 
if 0 then it processeds immediatley
sem_flg is either nowait or semudo which terminates our process
*/

	struct sembuf sb = {0, -1, 0};
	int semflg;
	int nsems;
	int semid;
	int mutex_sem;

	union senum
	{
		int val;
		struct semid_ds *buf;
		ushort array[1];
	} sem_attr;
	key_t key;
	key = 1234;
	nsems = 1; //creating 1 semaphore
	semflg = 0666 | IPC_CREAT;
	semid = semget(key, nsems, semflg);
	if (semid == -1)
	{
		perror("semget: failed");
		exit(EXIT_FAILURE);
	}
	sem_attr.val = 1; //semaphore unlocked
	if (semctl(mutex_sem, 0, SETVAL, sem_attr) == -1)
	{
		perror("semget");
		exit(EXIT_FAILURE);
	}

	FILE *in_file = fopen("db.txt", "r"); //pointer to file
	char string[100];
	char bufferFunds[100];
	char new_word[100];
	int num = 0;
	int count = 0;
	int attempt = 0;
	bool entry = false;
	int accountCount = 0;
	int pinCount = 0;
	int running = 1;
	char newPin[10];
	char newerPin[10];
	int location = 0;

	//setting up shared memory
	void *shared_memory = (void *)0; //pointer to memory
	struct account *new_account;	 //creating instance of shared memory struct
	int shmid;
	srand((unsigned int)getpid());
	/*setting up shared memory with shmget if it fails it will be equal to -1 and print a failure and exit system*/
	shmid = shmget((key_t)1234, sizeof(struct account), 0666 | IPC_CREAT);
	if (shmid == -1)
	{
		fprintf(stderr, "shmget failed for server %d\n", errno);
		exit(EXIT_FAILURE);
	}
	/*
enabling access to shared memory by attaching it to the address space of a process */
	shared_memory = shmat(shmid, (void *)0, 0);
	if (shared_memory == (void *)-1)
	{
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}

	//setting up message queue
	int msgid;
	int status;
	struct my_msg msg_receive; //creates instance of message queue struct for receiving
	msg_receive.msg_type = 1;  //makes the msg type for the receive from atm 1
	struct my_msg msg_send;	   //creates instance of message queue struct for sending
	msg_send.msg_type = 2;	   //makes the msg type 2 for sending to atm
	char msgBuffer[BUFSIZ];
	msgid = msgget((key_t)1234, 0666 | IPC_CREAT); //creates and accesses a message queue using msgget
	if (msgid == -1)
	{
		fprintf(stderr, "msgget failed with error: %d\n", errno);
		exit(EXIT_FAILURE);
	}

	/*forking one parent process and 2 child processes*/
	pid_t child_a, child_b;
	child_a = fork();
	if (child_a == -1)
	{ //if child_a fails to fork
		printf("Error forking child_a");
		exit(EXIT_FAILURE);
	}
	else if (child_a == 0)
	{
		/*opens the process atm in a new program*/
		static char *argv[] = {"./atm", NULL};
		execv(argv[0], argv);
	}
	else
	{
		child_b = fork();
		if (child_b == -1)
		{ //if child_b fails to fork

			printf("Error forking child_a");
			exit(EXIT_FAILURE);
		}
		else if (child_b == 0)
		{
			/*opens the process dbeditor in a new program*/
			static char *argv[]={"./dbeditor",NULL};
			execv(argv[0],argv);
		}
		else
		{

			//This is the parent process aka the dbserver program

			//setting semaphores
			struct sembuf asem[1];
			asem[0].sem_num = 0;
			asem[0].sem_op = 0;
			asem[0].sem_flg = 0;
			new_account = (struct account *)shared_memory; //creating new_account as a shared memory
			new_account->written_by_you = 0;			   //setting this flag as 0 so that this program is running and the others are sleeping
			while (running)
			{ //while running is 1 (1 by default
				asem[0].sem_op = -1;
				//if(semop(semid, &sb,1) == -1){
				//perror("semaphore locked");
				//return 0;
				//}

				if (new_account->written_by_you)
				{
					//Receives any message of type 1
					if (msgrcv(msgid, (void *)&msg_receive, BUFSIZ, 1, 0) == -1)
					{
						fprintf(stderr, "msgget failed with with error: %d\n", errno);
						asem[0].sem_op = 1;
						exit(EXIT_FAILURE);
					}

					else
					{

						/* Enters this if statement if the message received text is from the dbeditor and is equal to 'UBDATE_DB' */
						if (strcmp(msg_receive.some_text, "UPDATE_DB") == 0)
						{

							fseek(in_file, 0, SEEK_SET); //sets to the top of file
							if (in_file == NULL)
							{ //if no file
								printf("Error file missing\n");
								asem[0].sem_op = 1;
								exit(-1);
							}
							/*Here we increment the pin received by 1 before putting it
		in the db.txt for encryption*/
							int pin1 = atoi(new_account->encodedPIN);

							pin1 = pin1 + 1;
							/*The following code will enter the new data in a line in 
                the format 'accountNo encodedPIN fundsAvailable' */
							sprintf(newerPin, "%d", pin1);
							char first_space[5];
							strcpy(first_space, " ");
							char second_space[5];
							strcpy(second_space, " ");
							char copied_string1[100];
							strcpy(copied_string1, new_account->accountNo);
							char copied_string2[100];
							strcpy(copied_string2, newerPin);
							char funds[100];
							sprintf(funds, "%f", new_account->fundsAvailable);
							strcat(copied_string1, first_space);
							strcat(copied_string1, copied_string2);
							strcat(copied_string1, second_space);
							strcat(copied_string1, funds);
							fprintf(in_file, "%s", copied_string1);
						}

						/*This is the if statement the dbserver will enter once the user enter their credentials */
						if (strncmp(msg_receive.some_text, "PIN", 3) == 0)
						{

							if (in_file == NULL)
							{ //if no file
								printf("Error file missing\n");
								asem[0].sem_op = 1;
								exit(-1);
							}

							while (fscanf(in_file, "%s", string) == 1)
							{
								accountCount++;
								if (strstr(string, new_account->accountNo) != 0)
								{
									num++; /*everytime the accountNo is 
                                		found (0 or 1) */
								}
							}
							//if the account number exist in the database
							if (num == 1)
							{

								location = num + 1;			 //location to get the funds for 							this account number
								num = 0;					 //resets variable
								fseek(in_file, 0, SEEK_SET); //back to start of file

								/* The following will see the pin the user entered
                	subtract one from it and check if it is correct in 
                	the database */
								int pin = atoi(new_account->encodedPIN);

								pin = pin - 1;
								sprintf(newPin, "%d", pin);

								/*while scanning through the file */
								while (fscanf(in_file, "%s", string) == 1)
								{

									pinCount++;

									if (strstr(string, newPin) != 0)
									{

										count++; //if we find the pin
									}
								}

								/*If we do not find the pin then we will send back 				'PIN_WRONG' and we will then add one to the local 
			attempt variale */
								if (count == 0)
								{

									strcpy(msg_send.some_text, "PIN_WRONG");
									if (msgsnd(msgid, (void *)&msg_send, MAX_TEXT, 0) == -1)
									{

										fprintf(stderr, "msgsnd failed\n");
										asem[0].sem_op = 1;
										exit(EXIT_FAILURE);
									}
									printf("PIN_WRONG");
									attempt++;
									/*once the attempt limit of 3 attempts have 
                		been hit then the account is blocked by 
                		changing the first letter of the acccount 
                		number to 'X' */
									if (attempt == 3)
									{
										new_account->accountNo[0] = 'X';
										printf("%s\n", new_account->accountNo);
									}
								}
								/*If we find the pin enter this if
                		statement*/
								if (count == 1)
								{

									count = 0;

									/* These two variable see what line we were on, if
			they equal each other then the pin and the account 
			are for each other and not for another account */
									if (accountCount == pinCount)
									{
										accountCount = 0;
										pinCount = 0;

										//sends OK back to the atm
										strcpy(msg_send.some_text, "OK");
										char saveAccount[10];
										strcpy(saveAccount, new_account->accountNo);
										entry = true;
										/*sends 'OK' message to atm letting them 
                		know the account has been validated */

										if (msgsnd(msgid, (void *)&msg_send, MAX_TEXT, 0) == -1)
										{

											fprintf(stderr, "msgsnd failed\n");
											asem[0].sem_op = 1;
											exit(EXIT_FAILURE);
										}
									}
								}
								//}
							}

							//}
						}

						/*enter this if statement if the message received from the atm is a request for the balance in the account */
						if (strcmp(msg_receive.some_text, "BALANCE") == 0)
						{

							fseek(in_file, 0, SEEK_SET); //set to top of text file
							while (fscanf(in_file, "%99s", string) != EOF)
							{
								/* Using location variable from earlier we can get the
		funds available for the correct account*/
								if (++count == location)
								{
									new_account->fundsAvailable = atof(string);
									strcpy(msg_send.some_text, "BalanceAvailable");
									if (msgsnd(msgid, (void *)&msg_send, MAX_TEXT, 0) == -1)
									{

										fprintf(stderr, "msgsnd failed\n");
										asem[0].sem_op = 1;
										exit(EXIT_FAILURE);
									}
								}
							}
						}
						//Enter this if statement if the request from the user is withdraw
						if (strcmp(msg_receive.some_text, "WITHDRAW") == 0)
						{

							fseek(in_file, 0, SEEK_SET); //top of file
							/*The following will grab the location of the funds that
		need to be withdrawn from, and then subtract the withdrawn 
		funds from the account balance and then put the new balance
		back into the text file and send it to the atm to display
		to the user */
							while (fscanf(in_file, "%99s", string) != EOF)
							{
								if (++count == location)
								{

									new_account->fundsAvailable = atof(string);
									if ((new_account->fundsAvailable) > (new_account->fundsRequested))
									{
										strcpy(msg_send.some_text, "FUNDS_OK");

										new_account->fundsAvailable = new_account->fundsAvailable - new_account->fundsRequested;
										sprintf(new_word, "%f", new_account->fundsAvailable);
									}
									else
									{
										strcpy(msg_send.some_text, "NSF");
									}
								}
							}

							if (msgsnd(msgid, (void *)&msg_send, MAX_TEXT, 0) == -1)
							{

								fprintf(stderr, "msgsnd failed\n");
								asem[0].sem_op = 1;
								exit(EXIT_FAILURE);
							}
						}
					}

					sleep(rand() % 4);
					new_account->written_by_you = 0; //set flag to zero
				}

				asem[0].sem_op = 1;
			}
			wait(NULL);
		}
	}
	//Detaches the program from the 							memory
	if (shmdt(new_account) == -1)
	{

		exit(EXIT_FAILURE);
	}
	//Destroys memory
	if (shmctl(shmid, IPC_RMID, 0) == -1)
	{
		fprintf(stderr, "shmctl(IPC_RMID) failed\n");
		exit(EXIT_FAILURE);
	}
	//removes message queue
	if (msgctl(msgid, IPC_RMID, 0) == -1)
	{
		fprintf(stderr, "msgctl(IPC_RMID) failed\n");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
