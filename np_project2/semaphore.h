#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_
#define MAX_CLIENTS 30
#include <sys/ipc.h>
#include <sys/sem.h>

//wait for [2] (lock) to equal 0 ,then increment [2] to 1 - this locks it UNDO to release the lock if processes exits before explicitly unlocking */
static struct sembuf op_lock[2] ={2,0,0,2,1,SEM_UNDO};
//decrement [1] (proc counter) with undo on exit UNDO to adjust proc counter if process exits before explicitly calling sem_close() then decrement [2] (lock) back to 0 */
static struct sembuf op_endcreate[2] ={1,-1,SEM_UNDO,2,-1,SEM_UNDO};
//decrement [1] (proc counter) with undo on exit
static struct sembuf op_open[1] ={1,-1,SEM_UNDO};
//wait for [2] (lock) to equal 0 then increment [2] to 1 - this locks it then increment [1] (proc counter)
static struct sembuf op_close[3] ={2,0,0,2,1,SEM_UNDO,1,1,SEM_UNDO};
//decrement [2] (lock) back to 0
static struct sembuf op_unlock[1] = {2,-1,SEM_UNDO};
//decrement or increment [0] with undo on exit the 30 is set to the actual amount to add or subtract (positive or negative)
static struct sembuf op_op[1] = {0, 30,SEM_UNDO};

int sem_create(key_t key, int initval);
void sem_rm(int id);
int sem_open(key_t key);
void sem_close(int id);
void sem_op(int id, int value);
void sem_wait(int id);
void sem_signal(int id);

#endif