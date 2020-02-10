#include <iostream>
#include "semaphore.h"
using std::cerr;
using std::endl;

int sem_create(key_t key, int initval)
{
    register int id, semval;
    union semun 
    {
        int val;
        struct semid_ds *buf;
        ushort *array;
    } semctl_arg;
    
    if(key == IPC_PRIVATE)
    {
        return -1; /* not intended  for private sem */
    }
    else if(key == (key_t) -1 )
    {
        return -1; /* provaly an ftok() error by caller */
    }
    again:
        if((id = semget(key, 3, 0666 | IPC_CREAT)) <0 ){
            return -1;/* permission problem or tables full */
        }
        
        if((semop(id, &op_lock[0], 2))<0)
        {
            if(errno == EINVAL) goto again;
                cerr << "can't lock" << endl;
        }
        /*Get the value of the process counter.If it equals 0, then no one has initialized the semaphore yet.*/
        if((semval = semctl(id, 1, GETVAL, 0)) <0 )
        {
            cerr << "can't GETVAL" << endl;
        }
        if(semval == 0){ /* initial state */
        /*We could initialize by doing a SETALL, but that would clear the adjust value that we set when we locked the semaphore above.  
        Instead, we'll do 2 system calls to initialize [0] and [1].*/
            semctl_arg.val = initval;
            if(semctl(id, 0, SETVAL, semctl_arg) <0 )
            {
                cerr << "can't SETVAL[0] " << endl;
            }
            semctl_arg.val = MAX_CLIENTS + 1;
            if(semctl(id, 1, SETVAL, semctl_arg) <0 )
            {
                cerr << "can't SETVAL[1] " << endl;
            }
        }
        //Decrement the process counter and then release the lock.
        if(semop(id, &op_endcreate[0], 2) <0 )
        {
            cerr << "can't end create " << endl;
        }
    return id;
}

void sem_rm(int id){
    if(semctl(id, 0, IPC_RMID, 0) <0)
    {
        cerr << "can't IPC_RMID" << endl;
    }
}

int sem_open(key_t key)
{
    register int id;
    
    if(key == IPC_PRIVATE)
    {//not intended for private semaphores
        return -1;
    }else if(key == (key_t) -1 )
    {//probably an ftok() error by caller
        return -1;
    }
    if((id = semget(key, 3, 0)) <0 )
    {
        return -1; /* doesn't exist or tables full*/
    }
    //Decrement the process counter.  We don't need a lock to do this.
    if(semop(id, &op_open[0], 1) <0 )
    {
        cerr << "can't open " << endl;
    }
    return id;
}

void sem_close(int id){
    register int semval;
    
    if(semop(id, &op_close[0], 3) <0 )
    {
        cerr << "can't semop " << endl;
    }
    if((semval = semctl(id, 1, GETVAL, 0)) <0)
    {
        cerr << "can't GETVAL" << endl;
    }
    if(semval > MAX_CLIENTS + 1)
    {
        cerr << "sem[1] > 31 " << endl;
    }
    else if(semval == MAX_CLIENTS + 1)
    {
        sem_rm(id);
    }
    else
    {
        if(semop(id, &op_unlock[0],1) <0) 
        {
            cerr << "can't unlock " << endl;
        }
    }
}

void sem_op(int id, int value)
{
    if((op_op[0].sem_op = value) == 0)
    {
        cerr << "can't have value == 0 " << endl;
    }
    if(semop(id, &op_op[0], 1) <0 )
    {
        cerr << "sem_op error " << endl;
    }
}

void sem_wait(int id)
{
    sem_op(id, -1);
}

void sem_signal(int id)
{
    sem_op(id, 1);
}