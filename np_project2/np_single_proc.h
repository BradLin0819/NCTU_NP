#ifndef _NP_SINGLE_PROC_H_
#define _NP_SINGLE_PROC_H_

#define BUFFSIZE 15000
#define MESSAGESIZE 1024
#define NAMESIZE 20
#define MAX_CLIENTS 30

struct client_info
{
    int uid;
    int fd;
    char nickname[NAMESIZE+1];
    char ip[20];
    unsigned short port;
    int sent_fd[MAX_CLIENTS+1];
    int send_fd[MAX_CLIENTS+1];
    vector<pipe_counter> npipes;
    map<string, string> env;
};

enum MODE{
    RECEIVE = 0,
    SEND
};

pid_t fork_process();
void child_handler(int signo);
void run_npshell(int uid, int client_fd, string input_line);
void welcome_message(int client_fd);
int sem_init();
void set_up_all_clients();
void pass_message();
void set_up_all_clients();
int client_init(string client_ip, unsigned short client_port);
void client_login(string client_ip, unsigned short client_port);
void client_logout(int uid);
void broadcast(string msg);
void who(int client_fd);
void tell(int uid, int tell_id, string msg);
bool name_exists(string name);
bool user_pipe_exists(int uid, int other_uid, MODE mode);
void user_pipe_clear(int uid);
void clear_all_remain_user_pipes(int signo);
void rename(int uid, string name);
void yell(int uid, string msg);
void set_env(map<string, string> environment);
string get_user_ip(int uid);
unsigned short get_user_port(int uid);
string get_user_name(int uid);
bool usr_exists(int uid);
int get_null_fd();

#endif