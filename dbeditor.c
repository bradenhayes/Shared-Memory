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
    fundsAvailable is shared memory for the funds that are available in the account.
    fundsRequest is the shared memory for the funds request by the user. */

struct account
{
	int written_by_you;
	char accountNo[TEXT_SIZE];
	char encodedPIN[TEXT_SIZE];
	float fundsAvailable;
	float fundsRequested;
};

/* Setting up struct to use for message queue
	    msg_type the type of message being sent (either a 1 or 2 for our case).
	    some_text what is being sent over the message queue*/
struct my_msg
{
	long int msg_type;
	char some_text[BUFSIZ];
};

//main function
int main()
{
	//initilizing semaphore variables to be used
	struct sembuf sb = {0, -1, 0};
	int semflg;
	int nsems;
	int semid;
	int mutex_sem;
	//creating union for senum that is then used to control semaphore
	union senum
	{
		int val;
		struct semid_ds *buf;
		ushort array[1];
	} sem_attr;
	//initilizing semaphore for correct setup
	key_t key;
	key = 1234;
	nsems = 1; //creating 1 semmaphore
	semflg = 0666 | IPC_CREAT;
	semid = semget(key, nsems, semflg);
	//if our semid is -1 then we know that the semaphore was failed to get
	if (semid == -1)
	{
		perror("semget: failed");
		exit(EXIT_FAILURE);
	}
	sem_attr.val = 1; //semaphore unlocked
					  //by unlocking the semaphore we are making sure that it is unlocked
	if (semctl(mutex_sem, 0, SETVAL, sem_attr) == -1)
	{
		perror("semget");
		exit(EXIT_FAILURE);
	}

	//
	char request[BUFSIZ];
	int running = 1; //for our while loop so it is always running
	int request_status = 1;
	bool first_time = true;
	//setting up shared memory
	void *shared_memory = (void *)0;
	struct account *new_account;
	char pin_buffer[BUFSIZ];
	char account_buffer[BUFSIZ];
	char funds_buffer[BUFSIZ];

	//testing our shared memory to esure that memory isnt open already and to ensure its shared correctly
	//this is our shared memory id
	int shmid;
	shmid = shmget((key_t)1234, sizeof(struct account), 0666 | IPC_CREAT);
	if (shmid == -1)
	{
		fprintf(stderr, "shmget failed for editor\n");
		exit(EXIT_FAILURE);
	}
	//shmat attaches the shared_memory to the address space and makes sure that the address is invalid
	shared_memory = shmat(shmid, (void *)0, 0);
	if (shared_memory == (void *)-1)
	{
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}

	//setting up message queue
	int msgid;				//message id
	int status;				//message status
	struct my_msg msg_send; //initilizing a struct my_msg that will now be pointed with msg_send
	msg_send.msg_type = 1;	//creating msg_send to msg type 1
	struct my_msg msg_receive;
	msg_receive.msg_type = 2; //creating msg_recive to msg type 2
	char msgBuffer[BUFSIZ];	  //creating a a message buffer

	//testing our message id to send our message queue to dbserver with UPDATE_DB
	msgid = msgget((key_t)1234, 0666 | IPC_CREAT);
	if (msgid == -1)
	{
		fprintf(stderr, "msgget failed with error: %d\n", errno);
		exit(EXIT_FAILURE);
	}

	/*creating asem with out sembuf struct that will be used to lock and unlock our semaphores
sem_num is our semaphore number which is iniziled to 0
sem_op is out operation type, if 1 then adds to the semval
if 0 then it processeds immediatley
sem_flg is either nowait or semudo which terminates our process
*/
	struct sembuf asem[1];
	asem[0].sem_num = 0;
	asem[0].sem_op = 0;
	asem[0].sem_flg = 0;
	//new account points to account in the shared memmory space
	new_account = (struct account *)shared_memory;

	//inifite loop
	while (running)
	{

		while (new_account->written_by_you == 1)
		{
			sleep(1); //sleep for one second then continue
		}
		//locking our semaphore to ensure this is the only operation going on
		asem[0].sem_op = -1;
		//if it is unable to get the semaphore it will throw an error then return
		/*if(semop(semid, &sb,1) == -1){
		perror("semaphore locked");
		return 0;
	}*/
		//prints the prompts that the user will then enter to update the data base
		printf("Enter Account Number: \n");
		scanf("%s", account_buffer);
		strncpy(new_account->accountNo, account_buffer, TEXT_SIZE);
		printf("Enter Pin Number: \n");
		scanf("%s", pin_buffer);
		strncpy(new_account->encodedPIN, pin_buffer, TEXT_SIZE);
		printf("Enter funds Available: \n");
		scanf("%s", funds_buffer);
		new_account->fundsAvailable = atof(funds_buffer);

		//creating our update_db message that will be sent to the server
		char i[20] = "UPDATE_DB";
		strncpy(msg_send.some_text, i, 9);

		//if the message failed ot sent let the user know then it will exit if not our message is sent
		fprintf(stderr, "msgsnd failed\n");
		if (msgsnd(msgid, (void *)&msg_send, MAX_TEXT, 0) == -1)
		{
			asem[0].sem_op = 1; //release the semaphore if a failed operation to ensure the other proccesses can use
			exit(EXIT_FAILURE);
		}
		new_account->written_by_you = 1;
		wait(NULL);
		asem[0].sem_op = 1; //release semaphore after operation is done
	}
	//if shared memory address -1 terminate invalid address
	if (shmdt(shared_memory) == -1)
	{
		fprintf(stderr, "shmdt failed\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS); //exit upon exit of while loop
}
