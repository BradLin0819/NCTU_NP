#ifndef _NP_MULTI_PROC_H_
#define _NP_MULTI_PROC_H_

#define NAMESIZE 20
#define MAX_CLIENTS 30
#define BUFFSIZE 15000
#define MESSAGESIZE 1024
#define SHMKEY  ((key_t) 7890)
#define SEMKEY ((key_t) 7891)

struct client_info
{
    pid_t pid;
    int uid;
    int fd;
    char nickname[NAMESIZE+1];
    char ip[20];
    unsigned short port;
    char message[MESSAGESIZE+1];
    int sender_id;
    int clear_id;
    int sent_fd[MAX_CLIENTS+1];
};

pid_t fork_process();
void child_handler(int signo);
void run_npshell(int client_fd);
void welcome_message(int client_fd);
int shm_init();
void shm_clear(int signo);
int sem_init();
void set_up_all_clients();
void user_pipe_clear();
void pass_message(int signo);
void user_pipe_handler(int signo);
void user_exit_clear(int signo);
void set_up_all_clients();
int client_init(string client_ip, unsigned short client_port);
void close_other_client_fd(int id);
void close_unused_fd();
void client_login(string client_ip, unsigned short client_port);
void client_logout();
void clear_all_remain_fifo();
void broadcast(string msg);
void who(int client_fd);
void tell(int tell_id, string msg);
bool name_exists(string name);
void rename(string name);
void yell(string msg);
string get_user_ip();
unsigned short get_user_port();
string get_user_name(int id);
bool usr_exists(int id);
bool fifo_exists(string fifoname);
int get_null_fd();
string get_fifo_name(int in_id, int out_id);

#endif