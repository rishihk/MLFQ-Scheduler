// Project 1B tester

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_MSG_SIZE 256

struct msg_t
{
    char content[MAX_MSG_SIZE];
    int receiver_id;
};

void panic(char *s) // function called panic takes in a string as an argument.
{
    fprintf(2, "%s\n", s); // prints this error message to the terminal.
    exit(1);               // exits the process with status 1 to show something went wrong.
}

// create a new process
int fork1(void) // function called fork1
{
    int pid;
    pid = fork();      // forks the parent process. so now we have a parent and a child process
    if (pid == -1)     // denotes that the fork failed
        panic("fork"); // call panic with message fork
    return pid;        // return pid of child process
}

// create a pipe
void pipe1(int fd[2]) // function called pipe1, which takes in an array of size two which are file descriptors r or w of pipe.
{
    int rc = pipe(fd); // we create the pipe with the file descriptors r and w
    if (rc < 0)        // if pipe fails and returns negative num,
    {
        panic("Fail to create a pipe."); // panic.
    }
}

int main(int argc, char *argv[])
{
    if (argc < 4) // we need 4 arguments. file name by default, num receivers, id of receiver, and msg to broadcast
    {
        panic("Usage: unicast <num_of_receivers> <id_of_receiver> <msg_to_broadcast>");
    }

    int numReceiver = atoi(argv[1]); // ascii to int 1st argument for num receivers

    int receiverId = atoi(argv[2]); // ascii to int arg 2 for receiver id

    int pipeToChildren[2], pipeToParent[2]; // create pipe ends for children and parent

    pipe1(pipeToChildren); // builds the pipe for children and parent
    pipe1(pipeToParent);

    for (int i = 0; i < numReceiver; i++) // for num receivers, we must create a child process.
    {
        if (fork1() == 0) // if fork1 is 0, we are in the child
        { // Child process
            int childId = i;
            printf("Child %d: start!\n", childId); // print child id

            struct msg_t msg; // create msg object to be read
            read(pipeToChildren[0], &msg, sizeof(struct msg_t));// read

            printf("Child %d: get msg (%s) to Child %d\n", childId, msg.content, msg.receiver_id); // print get msg to

            if (msg.receiver_id == childId) // if msgs receiver id is i
            {
                printf("Child %d: the msg is for me.\n", childId); // perform receival messages
                printf("Child %d acknowledges to Parent: received!\n", childId); // perform receival messages
                write(pipeToParent[1], "received", 9); // write to parent that we acknowledged.
                exit(0); // exit
            }
            else
            {
                printf("Child %d: the msg is not for me.\n", childId); // perform didnt receive messages
                printf("Child %d: write the msg back to pipe.\n", childId);
                write(pipeToChildren[1], &msg, sizeof(struct msg_t)); // write the message back to children pipe
                exit(0);
            }
        }
        else // now we are in parent process
        {
            printf("Parent: creates child process with id: %d\n", i); // parent creates child message
        }

        sleep(1);
    }

    // parent process's job

    struct msg_t msg; // msg object to send to children
    strcpy(msg.content, argv[3]); // copy message argument to msg object
    msg.receiver_id = receiverId; // set receiver id to object

    write(pipeToChildren[1], &msg, sizeof(struct msg_t)); // write the message to children pipe
    printf("Parent sends to Child %d: %s\n", receiverId, msg.content); // write confirmation message

    char recBuf[100]; // to receive the acknowledgement
    read(pipeToParent[0], recBuf, sizeof(recBuf)); // reads parent pipe end
    printf("Parent receives: %s\n", recBuf); // prints the received message
    

    exit(0); // exit
}

// initially, the pipe is empty and the child processes would block (wait) until there's something to read unless the message has already been sent by the parent ahead of time. However, in this code, the parent writes to the pipe after all children have been forked, so children will not find a message immediately and this part of the code does not execute
