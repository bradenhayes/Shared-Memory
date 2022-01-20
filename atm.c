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
struct account{
	int written_by_you;
	char accountNo[TEXT_SIZE];
	char encodedPIN[TEXT_SIZE];
	float fundsAvailable;
	float fundsRequested;
	};
	
/* Setting up struct to use for message queue
	msg_type the type of message being sent (either a 1 or 2 for 		our case).
	some_text what is being sent over the message queue	*/
struct my_msg{
	long int msg_type;
	char some_text[BUFSIZ];
	};



int main(int argc, char** argv)
{
/*creating asem with out sembuf struct that will be used to lock and unlock our semaphores
sem_num is our semaphore number which is iniziled to 0 
sem_op is out operation type, if 1 then adds to the semval 
if 0 then it processeds immediatley
sem_flg is either nowait or semudo which terminates our process
*/

	struct sembuf sb = {0,-1,0};
  int semflg;
  int nsems;
  int semid;
  int mutex_sem;

	union senum{
		int val;
		struct semid_ds *buf;
		ushort array[1];
	}sem_attr;
  //initilizing semaphore
	key_t key;
  key = 1234;
  nsems = 1;//creating 1 semmaphore
  semflg = 0666|IPC_CREAT ;
  semid = semget(key, nsems,semflg);
  if(semid==-1){
    perror("semget: failed");
    exit(EXIT_FAILURE);
  }
  sem_attr.val = 1; //semaphore unlocked
  if (semctl(mutex_sem, 0, SETVAL, sem_attr) == -1){
    perror("semget");
    exit(EXIT_FAILURE);
  }

char temp[BUFSIZ];
char request[BUFSIZ];
int running = 1; //while 1 while loop is running
int request_status=1; 
bool first_time=true; 

//setting up shared memory
void *shared_memory = (void *)0; //creating poiner for shared memory
struct account *new_account; //creating instance of shared memory struct 
char pin_buffer[BUFSIZ];
char account_buffer[BUFSIZ];
int shmid;
shmid = shmget((key_t)1234, sizeof(struct account), 0666 | IPC_CREAT); //creating memory with shmget
if (shmid == -1) { //testing for if shmid worked or not
fprintf(stderr, "shmget failed for server %d\n",errno);
exit(EXIT_FAILURE);
}
/*
enabling access to shared memory by attaching it to the address space of a process */
shared_memory = shmat(shmid, (void *)0, 0); 
if (shared_memory == (void *)-1) { //if shmat fails
fprintf(stderr, "shmat failed\n");
exit(EXIT_FAILURE);
}
//setting up message queue
int msgid;
int status;
struct my_msg msg_send; //creating instance of message queue struct to send
msg_send.msg_type=1; //type 1 for sending from atm to dbserver
struct my_msg msg_receive; //message queue struct for receiving
msg_receive.msg_type=2; //type 2 for receiving from dbserver
char msgBuffer[BUFSIZ];
/*Creating and accessing a message queue using msgget*/
msgid = msgget((key_t)1234,0666|IPC_CREAT); 
if(msgid==-1){ //if msgget fails
fprintf(stderr,"msgget failed with error: %d\n",errno);
exit(EXIT_FAILURE);
}

struct sembuf asem[1];
asem[0].sem_num = 0;
asem[0].sem_op = 0;
asem[0].sem_flg = 0;
new_account = (struct account*)shared_memory; //assigning shared memory to new instance of the account struct
while(running) { //while running =1 (by default set to 1);

/*if written_by_you flag is 1 then this process will sleep forever*/
while(new_account->written_by_you == 1) { 
sleep(1);
}
asem[0].sem_op = -1;
//if(semop(semid, &sb,1) == -1){
		//perror("semaphore locked");
		//return 0;
	//}
	
msgrcv(msgid,(void*)&msg_receive,BUFSIZ,2,IPC_NOWAIT); //checking for messages of type 2 to receive from dbserver, if none then move on (using flag IPC_NOWAIT);

/*If the message received back from the server is 'OK' then enter this if
statement, this will proceed to ask them what they want to do in their account */
if(strcmp(msg_receive.some_text,"OK")==0){
	while(request_status==1){
	if(first_time==true){
	printf("Pin correct, what would you like to do in your account?\n Pick between:\n 1. Show Balance (enter 1) \n 2. Withdraw Funds (enter 2)\n");
	}
	scanf("%s",request);
	if(strcmp(request,"1")==0){
	request_status=0;
	/*if they want balance then the some_text in the message becomes
	'BALANCE' to send to the dbserver*/
	strcpy(msg_send.some_text,"BALANCE");
	if(msgsnd(msgid,(void *)&msg_send,MAX_TEXT,0)==-1){ //sends message

	fprintf(stderr,"msgsnd failed\n");
	asem[0].sem_op = 1;
	exit(EXIT_FAILURE);


}

	}
	/*In the case the user wants to withdraw then this will be where
		we enter, then some_text will be turned into 'WITHDRAW'.
		In addition to this the 'fundsRequested' field in shared 
		memory will be updated to this value they want*/
	else if(strcmp(request,"2")==0){
	float funds;
	request_status=0;
	strcpy(msg_send.some_text,"WITHDRAW");
	printf("How much do you wish to withdraw? \n");
	scanf("%f",&funds);
	new_account->fundsRequested = funds;
	if(msgsnd(msgid,(void *)&msg_send,MAX_TEXT,0)==-1){//sends message

	fprintf(stderr,"msgsnd failed\n");
	asem[0].sem_op = 1;
	exit(EXIT_FAILURE);

	}
	}
	else{
	printf("Invalid entry, enter a 1 or a 2: \n"); 
	
	/*no longer first time so does not repeat the top saying of
	'"Pin correct, what would you like to do in your account?\n Pick between:\n 1. Show Balance (enter 1) \n 2. Withdraw Funds (enter 2)\n"*/

	first_time=false; 

	}
	}


	}
	/*if the message received from the dbserver is 'BalanceAvailable'
		then enter this if statement*/
	if(strcmp(msg_receive.some_text,"BalanceAvailable")==0){

		sprintf(temp,"%f",new_account->fundsAvailable);
		printf("%s",temp);


	}
	/*if the text received from the dbserver is 'NSF'
		then enter this if statement*/
	if(strcmp(msg_receive.some_text,"NSF")==0){

		printf("Not enough funds available");

	}
	/*if the text received from the dbserver is 'FUNDS_OK'
		then enter this if statement and print the new funds 		for their account available*/
	if(strcmp(msg_receive.some_text,"FUNDS_OK")==0){

		sprintf(temp,"%f",new_account->fundsAvailable);
		printf("%s",temp);


	}


/* The following is for the user input, each statement will take an 
input from the user and then update that corresponding field in the
shared memory */

printf("Enter Account Number: \n");
scanf("%s",account_buffer);
if(strcmp(account_buffer,"X")==0){
	asem[0].sem_op = 1;
	exit(0);
	}
strncpy(new_account->accountNo, account_buffer, TEXT_SIZE);
printf("Enter Pin Number: \n");
scanf("%s",pin_buffer);
strncpy(new_account->encodedPIN, pin_buffer, TEXT_SIZE);

/*once the user has entered it will send the 'PIN' message to the dbserver 
so the server can validate the user */
strcpy(msg_send.some_text,"PIN");

if(msgsnd(msgid,(void *)&msg_send,MAX_TEXT,0)==-1){
fprintf(stderr,"msgsnd failed\n");
asem[0].sem_op = 1;
exit(EXIT_FAILURE);

}

new_account->written_by_you = 1; //sets flag to 1
 
asem[0].sem_op = 1; //sem to 1



}
/* detaches the shared from the current process */
if (shmdt(shared_memory) == -1) {
fprintf(stderr, "shmdt failed\n");
exit(EXIT_FAILURE);
}
exit(EXIT_SUCCESS);
}
	
	
	
	
	
