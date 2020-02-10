#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include <map>
#include "Parser.h"
#include "utils.h"
#include "semaphore.h"
#define BUFFSIZE 15000
#define MESSAGESIZE 1024
#define NAMESIZE 20
#define SHMKEY  ((key_t) 7890)
#define SEMKEY ((key_t) 7891)
#define MAX_CLIENTS 30


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
client_info* all_clients;
int shmid;
int sem;
int client_sock;
int uid;

pid_t parent_pid;
pid_t client_pid;
pid_t fork_process();
map<int, int> used_fd;
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

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        cout << "Usage: " << argv[0] << " [port_num]" << endl;
        exit(1);
    }
    int sockfd;
    uint32_t port = (uint32_t)atoi(argv[1]);
    struct sockaddr_in client_addr, server_addr;

    signal(SIGINT, shm_clear);
    signal(SIGUSR1, pass_message);
    signal(SIGUSR2, user_pipe_handler);
    signal(SIGALRM, user_exit_clear);


    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cerr << "Server can't create socket" << endl;
        exit(1);
    }

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int optval = 1;
    
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        cerr << "Server can't bind socket" << endl;
        exit(1);
    }

    if(listen(sockfd, 2*MAX_CLIENTS) < 0)
    {
        cerr << "Server can't listen socket" << endl;
        exit(1);
    }

    if(shm_init() < 0)
    {
        cerr << "shm init failed" << endl;
        exit(1);
    }

    if(sem_init() < 0)
    {
        cerr << "sem init failed" << endl;
        exit(1);
    }

    set_up_all_clients();

    while(1)
    {
        socklen_t client_len = sizeof(client_addr);
        if((client_sock = accept(sockfd, (struct sockaddr*)&client_addr, &client_len)) < 0)
        {
            cerr << "accept failed" << endl;
            continue;
        }
        string client_ip = inet_ntoa(client_addr.sin_addr);
        unsigned short client_port = ntohs(client_addr.sin_port);

        uid = client_init(client_ip, client_port);
        used_fd[uid] = client_sock;
        
        cout << "Accept Client " << client_sock << endl;

        if(client_sock < 0)
        {
            // cerr << "Can't connect to server" << endl;
            exit(1);
        }
        else
        {
            pid_t pid;

            parent_pid = getpid();
            if((pid = fork_process()) == 0)
            {
                close_fd(sockfd);
                close_other_client_fd(uid);
                client_pid = getpid();
                welcome_message(client_sock);
                client_login(client_ip, client_port);
                run_npshell(client_sock);
                client_logout();
                close_fd(client_sock);
                exit(0);
            }
        }
    }
    
    return 0;
}

void run_npshell(int client_fd)
{
    string initial_path = "bin:.";
    bool exit_flag = false;
    string input_line;
    vector<pipe_counter> pipes;
    char input_buff[BUFFSIZE];

    clearenv();
    setenv("PATH", initial_path.c_str() , 1);
    
    while(1)
    {
        string prompt = "% ";
        char ch;
        int index = 0;

        write(client_fd, prompt.c_str(), prompt.length());
        memset(input_buff, 0, sizeof(input_buff));
        
        while(read(client_fd, &ch, 1) == 1)
        {
            input_buff[index++] = ch;
            if(ch == '\n')
                break;
        }
        if(strcmp(input_buff, "") == 0)
        {
            break;
        }
        else if(strcmp(input_buff, "\r\n") == 0 || strcmp(input_buff, "\n") == 0)
        {
            continue;
        }
        else 
        {
            input_line = split_with_delims(input_buff, "\n")[0];
            input_line = split_with_delims(input_line, "\r")[0];
        }
        
        Parser parser = Parser(input_line);
        
        vector<Command*>  command_list = parser.get_command_list();
        vector<pid_t> pid_list;
        
        for(int i = 0; i < command_list.size(); ++i)
        {
            vector<string> vec_args = command_list[i]->get_args();
            
            int pipe_read_index;
            int pipe_write_index;
            int readfd = 0;
            int writefd = 1;
            bool has_on_time_pipe = false;
            bool has_user_pipe_in = false;
            string fifo_in;
            //update counter
            pipes_update(pipes);
            pipe_read_index = pipe_for_reading(pipes);
            has_on_time_pipe = (pipe_read_index  != -1);
            has_user_pipe_in = (command_list[i]->get_user_pipe_in() > 0);
           
            //setenv built-in
            if(vec_args[0] == "setenv")
            {
                if(vec_args.size() != 3)
                {
                    string err_msg = "setenv usage: ​ setenv [variable name] [value to assign]\n";
                    write(client_fd, err_msg.c_str(), err_msg.length());
                }
                else    
                    setenv(vec_args[1].c_str(), vec_args[2].c_str(), 1);
            }
            //printenv bulit-in
            else if(vec_args[0] == "printenv")
            {
                if(vec_args.size() != 2)
                {
                    string err_msg = "printenv usage: ​ printenv [variable name]\n";
                    write(client_fd, err_msg.c_str(), err_msg.length());
                }
                else
                {
                    char* result;
                    if((result = getenv(vec_args[1].c_str())) != NULL)
                    {
                        string result_string = string(result) + "\n";
                        write(client_fd, result_string.c_str(), result_string.length());
                    }
                }   
            }
            else if(vec_args[0] == "who")
            {
                who(client_fd);
            }
            else if(vec_args[0] == "tell")
            {
                int tell_pos = input_line.find(vec_args[0]);
                string send_msg = input_line.substr(tell_pos+vec_args[0].length());
                int tell_id = send_msg.find(vec_args[1]);

                send_msg = send_msg.substr(tell_id+vec_args[1].length());
                tell(atoi(vec_args[1].c_str()), send_msg);
                erase_pipe(pipes, pipe_read_index);
                break;
            }
            else if(vec_args[0] == "name")
            {
                rename(vec_args[1]);
            }
            else if(vec_args[0] == "yell")
            {
                int yell_pos = input_line.find(vec_args[0]);
                string send_msg = input_line.substr(yell_pos+vec_args[0].length());

                yell(send_msg);
                erase_pipe(pipes, pipe_read_index);
                break;
            }
            //exit built-in
            else if(vec_args[0] == "exit")
            {
                exit_flag = true;
                break;
            }
            else
            {
                int pipefd[2];
                pid_t pid;
                bool close_stdout = true;
                bool errpipe = false;
                bool tofile = false;
                //pipe related command
                //user pipe in
                command_list[i]->info();
                if(command_list[i]->check_if_user_pipe_in())
                {
                    int in_id = command_list[i]->get_user_pipe_in();
                    bool err = false;
                    string err_msg = "";
                    
                    if(usr_exists(in_id))
                    {
                        string fifoname =  "user_pipe/" + get_fifo_name(in_id, uid);
                        string send_msg = "*** ";

                        if(fifo_exists(fifoname))
                        {
                            sem_wait(sem);
                            readfd = all_clients[uid].sent_fd[in_id];
                            all_clients[uid].sent_fd[in_id] = -1;
                            sem_signal(sem);
                            fifo_in = fifoname;
                            
                            send_msg += get_user_name(uid) + " (#";
                            send_msg += to_string(uid) + ") just received from ";
                            send_msg += get_user_name(in_id) + " (#";
                            send_msg +=  to_string(in_id) + ") by '";
                            send_msg += input_line + "' ***\n";
                            broadcast(send_msg);
                        }
                        else
                        {
                            err = true;
                            err_msg = "*** Error: the pipe #";
                            err_msg += to_string(in_id) + "->#";
                            err_msg += to_string(uid) + " does not exist yet. ***\n";
                        }
                    }
                    else
                    {
                        err = true;
                        err_msg = "*** Error: user #";
                        err_msg += to_string(in_id) + " does not exist yet. ***\n"; 
                    }
                    if(err)
                    {
                        int null_fd;
                        write(client_fd, err_msg.c_str(), err_msg.length());
                        if((null_fd = get_null_fd()) < 0)
                         {
                             cerr << "dev/null error" << endl;
                         }
                        else
                        {
                            readfd = null_fd;
                        }
                    }
                }
                if(command_list[i]->check_if_pipe())
                {
                    int pipeN;
                    //treat normal pipe as |1
                    if(command_list[i]->check_pipe())
                    {
                        pipeN = 1;
                    }
                    else if(command_list[i]->check_pipeN())
                    {
                        pipeN = command_list[i]->get_pipeN_count();
                    }
                    else if(command_list[i]->check_exclamation())
                    {
                        pipeN = command_list[i]->get_pipeN_count() > 0 ? command_list[i]->get_pipeN_count() : 1;
                        errpipe = true;
                    }
                    //check if any pipe is in pipe buffer
                    pipe_write_index = pipe_for_writing(pipes, pipeN);
                    //no pipe in pipe buffer, create pipes
                    if(pipe_write_index == -1)
                    {
                        if(pipe(pipefd) == -1)
                        {
                            perror("Create pipes error!\n");
                            exit(1);
                        }
                        writefd = pipefd[1];
                        pipe_counter new_pipe(pipefd[0], pipefd[1], pipeN);
                        pipes.push_back(new_pipe);
                        pipe_write_index = pipe_for_writing(pipes, pipeN);
                    }
                    //pipe exists
                    else
                    {
                        writefd = pipes[pipe_write_index].writefd;
                    }  
                }
                //file redirection
                else if(command_list[i]->check_redirection())
                {
                    string filename = command_list[i]->get_filename();
                    if((writefd = open(filename.c_str(), O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR))== -1)
                    {
                        cerr << filename << " is a directory!" << endl;  
                        continue;
                    }
                    else
                    {
                        tofile = true;
                    }
                }
                //user pipe out
                else if(command_list[i]->check_if_user_pipe_out())
                {
                    int out_id = command_list[i]->get_user_pipe_out();
                    bool err = false;
                    string err_msg = "";
                    
                    if(usr_exists(out_id))
                    {
                        string fifoname =  "user_pipe/" + get_fifo_name(uid, out_id);
                        if(fifo_exists(fifoname))
                        {
                            err = true;
                            err_msg = "*** Error: the pipe #";
                            err_msg += to_string(uid) + "->#";
                            err_msg += to_string(out_id) + " already exists. ***\n";
                        }
                        else
                        {
                            pid_t out_usr_pid;
                            string send_msg = "*** ";

                            send_msg += get_user_name(uid) + " (#";
                            send_msg += to_string(uid) + ") just piped '";
                            send_msg += input_line + "' to ";
                            send_msg += get_user_name(out_id) + " (#";
                            send_msg +=  to_string(out_id) + ") ***\n";

                            sem_wait(sem);
                            out_usr_pid = all_clients[out_id].pid;
                            all_clients[out_id].sender_id = uid;
                            sem_signal(sem);
                        
                            mkfifo(fifoname.c_str(), 0666);
                            sem_wait(sem);
                            kill(out_usr_pid, SIGUSR2);
                            sem_signal(sem);
                            if((writefd = open(fifoname.c_str(),  O_WRONLY)) < 0)
                            {
                                cerr << "fifo write failed" << endl;
                                continue;
                            }
                            broadcast(send_msg);
                            tofile = true;
                        }
                    }
                    else
                    {
                        err = true;
                        err_msg = "*** Error: user #";
                        err_msg += to_string(out_id) + " does not exist yet. ***\n";
                    }
                    if(err)
                    {
                        write(client_fd, err_msg.c_str(), err_msg.length());
                        erase_pipe(pipes, pipe_read_index);
                        continue;
                    }
                }
                //simple command
                else
                {
                    //use stdout
                    close_stdout = false;
                }
                //child process
                if((pid = fork_process()) == 0)
                {
                    //read from pipe
                    if(has_on_time_pipe)
                    {
                        readfd = pipes[pipe_read_index].readfd;
                        close_fd(pipes[pipe_read_index].writefd);
                        close_fd(STDIN_FILENO);
                        set_readfd(readfd, STDIN_FILENO);
                        close_fd(readfd);
                    }
                    else if(has_user_pipe_in)
                    {
                        close_fd(STDIN_FILENO);
                        set_readfd(readfd, STDIN_FILENO);
                        close_fd(readfd);
                    }
                    else
                    {
                        close_fd(STDIN_FILENO);
                        set_readfd(client_fd, STDIN_FILENO);
                    }
                    
                    //write to pipe or to file
                    if(close_stdout)
                    {
                        if(writefd != -1)
                        {
                            if(!tofile)
                                close_fd(pipes[pipe_write_index].readfd);

                            close_fd(STDOUT_FILENO);
                            set_writefd(writefd, STDOUT_FILENO);
                            if(errpipe)
                            {
                                close_fd(STDERR_FILENO);
                                set_writefd(writefd, STDERR_FILENO);
                            }
                            else
                            {
                                close_fd(STDERR_FILENO);
                                set_writefd(client_fd, STDERR_FILENO);
                            }
                            close_fd(writefd);
                        }
                    }
                    else
                    {
                        close_fd(STDOUT_FILENO);
                        set_writefd(client_fd, STDOUT_FILENO);
                        close_fd(STDERR_FILENO);
                        set_writefd(client_fd, STDERR_FILENO);
                    }
                    
                    if(!cmd_exists(vec_args[0]))
                    {
                        string err_msg = "Unknown command: [" + vec_args[0] + "].\n";
                        write(STDERR_FILENO, err_msg.c_str(), err_msg.length());
                        _exit(0);
                    }
                    else
                    {
                        if(execute_cmd(vec_args) == -1)
                        {
                            perror("Exec error!\n");
                            exit(EXIT_FAILURE);
                        }
                    }
                }

                if((!command_list[i]->check_if_pipe() && !command_list[i]->check_if_user_pipe_out()))
                    pid_list.push_back(pid);

                erase_pipe(pipes, pipe_read_index);

                if(has_user_pipe_in)
                {
                    close_fd(readfd);
                    unlink(fifo_in.c_str());
                }
                    
                if(command_list[i]->check_if_user_pipe_out())
                    close_fd(writefd);
            
            }
        }
        while(!pid_list.empty())
        {
            int status;
            waitpid(pid_list[0], &status, 0);
            pid_list.erase(pid_list.begin());
        }
        for(int i = 0; i < command_list.size(); ++i)
        {
            delete command_list[i];
        }
        if(exit_flag)
            break;
        
    }
}

pid_t fork_process()
{
    pid_t pid;
    do{
        signal(SIGCHLD, child_handler);
        if((pid = fork()) < 0)
            usleep(1000);
    }while(pid < 0);
    return pid;  
}

void child_handler(int signo)
{
    int status;
    while(waitpid(-1, &status, WNOHANG) > 0);
    if(getpid() == parent_pid)
        close_unused_fd();
}

void user_pipe_handler(int signo)
{
    int sender_id;
    int readfd;
    string fifoname;

    sender_id = all_clients[uid].sender_id;
    all_clients[uid].sender_id = -1;
    
    fifoname = "user_pipe/" + get_fifo_name(sender_id, uid);

    if((readfd = open(fifoname.c_str(), O_RDONLY)) < 0)
    {
        cerr << "fifo read failed" << endl;
    }
    else
    {
        all_clients[uid].sent_fd[sender_id] = readfd;  
    }
}

void user_exit_clear(int signo)
{
    sem_wait(sem);
    int close_fd = all_clients[uid].sent_fd[all_clients[uid].clear_id];
    close(close_fd);
    all_clients[uid].clear_id = -1;
    sem_signal(sem);
}

void user_pipe_clear()
{
    
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        int close_send_me_fd;
        int close_i_send_fd;
        string fifoname;

        sem_wait(sem);
        close_send_me_fd = all_clients[uid].sent_fd[i];
        close_i_send_fd = all_clients[i].sent_fd[uid];
        sem_signal(sem);
        
        //Exit when user is receiver.
        if(close_send_me_fd > 0)
        {
            fifoname = "user_pipe/" + get_fifo_name(i, uid);
            unlink(fifoname.c_str());
        }
        //Exit when user is sender.
        if(close_i_send_fd > 0)
        {
            pid_t client_i_sent;
            fifoname = "user_pipe/" + get_fifo_name(uid, i);
            sem_wait(sem);
            client_i_sent = all_clients[i].pid;
            all_clients[i].clear_id = uid;
            sem_signal(sem);

            kill(client_i_sent, SIGALRM);

            sem_wait(sem);
            all_clients[i].sent_fd[uid] = -1;
            sem_signal(sem);

            unlink(fifoname.c_str());
        }
    }
    
}

int get_null_fd()
{
    int null_fd;

    if((null_fd = open("/dev/null", O_RDWR)) < 0)
    {
        cerr <<  "null fd failed!" << endl;  
        return -1;
    }
    return null_fd;
}

bool fifo_exists(string fifoname)
{
    if(access(fifoname.c_str(), F_OK) != -1)
        return true;
    else
        return false;
}

void set_up_all_clients()
{
    sem_wait(sem);
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        all_clients[i].uid = -1;
        all_clients[i].pid = -1;
        all_clients[i].fd = -1;
        strcpy(all_clients[i].nickname, "");
        strcpy(all_clients[i].ip, "");
        all_clients[i].port = 0;
        memset(all_clients[i].sent_fd, -1, sizeof(all_clients[i].sent_fd));
        all_clients[i].sender_id = -1;
        all_clients[i].clear_id = -1;
        used_fd[i] = -1;
    }
    sem_signal(sem);
}

void close_unused_fd()
{
    sem_wait(sem);
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        if(used_fd[i] > 0)
        {
            if(all_clients[i].fd == -1)
            {
                close_fd(used_fd[i]);
                used_fd[i] = -1;
            }
        }
    }
    sem_signal(sem);
}

void welcome_message(int client_fd)
{
    string message = "****************************************\n** Welcome to the information server. **\n****************************************\n";
    write(client_fd, message.c_str(), message.length());
}

int shm_init()
{
    if((shmid = shmget(SHMKEY, sizeof(client_info) * (MAX_CLIENTS + 10), IPC_CREAT | 0666)) < 0)
        return -1;
    if((all_clients = (client_info*) shmat(shmid, (char *)0, 0)) < 0)
        return -1;
    
    return 0;
}

void clear_all_remain_fifo()
{
    string filename;
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        for(int j=1; j <= MAX_CLIENTS; ++j)
        {
            filename = "user_pipe/" + get_fifo_name(i, j);
            if(fifo_exists(filename))
                unlink(filename.c_str());
        }
    }
}

int sem_init()
{
    if((sem = sem_create(SEMKEY, 1)) < 0)
    {
        cerr << "sem create failed" << endl;
        return -1;
    }
    if((sem = sem_open(SEMKEY)) < 0)
    {
        cerr << "sem open failed" << endl;
        return -1;
    }
    return 0;
}

void shm_clear(int signo)
{
    clear_all_remain_fifo();
    if(shmdt(all_clients) < 0)
    {
        cerr << "shm detach failed" << endl;
    }
    if(shmctl(shmid, IPC_RMID, (struct shmid_ds *)0) < 0)
    {
        cerr << "shm clear failed" << endl;
    }
    sem_rm(sem);
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

void pass_message(int signo)
{
    sem_wait(sem);
    string msg = all_clients[uid].message;
    write(client_sock, msg.c_str(), msg.length());
    strcpy(all_clients[uid].message,  "");
    sem_signal(sem);
}

void broadcast(string msg)
{
    sem_wait(sem);
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        if(all_clients[i].uid != -1)
        {
            strcpy(all_clients[i].message, msg.c_str());
        }
    }
    sem_signal(sem);
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        if(all_clients[i].uid != -1)
        {
            kill(all_clients[i].pid, SIGUSR1);
        }
    }
}

void who(int client_fd)
{
    string header = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    string id;
    string name;
    string ip;
    string is_me = "<-me";
    string port;

    write(client_fd, header.c_str(), header.length());

    sem_wait(sem);
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        if(all_clients[i].uid > 0)
        {
            id = to_string(all_clients[i].uid) + "\t";
            name = all_clients[i].nickname ;
            name += "\t";
            ip = all_clients[i].ip;
            ip += ":";
            port = to_string(all_clients[i].port) + "\t";
            write(client_fd, id.c_str(), id.length());
            write(client_fd, name.c_str(), name.length());
            write(client_fd, ip.c_str(), ip.length());
            write(client_fd, port.c_str(), port.length());
            if(all_clients[i].fd == client_fd)
                write(client_fd, is_me.c_str(), is_me.length());
            write(client_fd, "\n", 1);
        }
    }
    sem_signal(sem);
}

void tell(int tell_id, string msg)
{
    sem_wait(sem);
    if(all_clients[tell_id].uid < 0)
    {
        string err_msg = "*** Error: user #" + to_string(tell_id);  
        err_msg += " does not exist yet. ***\n";
        write(client_sock, err_msg.c_str(), err_msg.length());
    }
    else
    {
        string send_msg = "*** ";
        send_msg += all_clients[uid].nickname;
        send_msg += " told you ***:" + msg + "\n";
        strcpy(all_clients[tell_id].message, send_msg.c_str());
    }
    sem_signal(sem);
    if(all_clients[tell_id].uid > 0)
        kill(all_clients[tell_id].pid, SIGUSR1);
}

bool name_exists(string name)
{
    bool has_same_name = false;
    sem_wait(sem);
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        if(strcmp(all_clients[i].nickname, name.c_str()) == 0)
            has_same_name = true;
    }
    sem_signal(sem);

    return has_same_name;
}

void rename(string name)
{
    if(name_exists(name))
    {
        string err_msg = "*** User '" + name;
        err_msg += "' already exists. ***\n";
        write(client_sock, err_msg.c_str(), err_msg.length());
    }
    else
    {
        sem_wait(sem);
        strcpy(all_clients[uid].nickname, name.c_str());
        sem_signal(sem);
        string usr_ip = get_user_ip();
        unsigned short usr_port = get_user_port();
        string msg = "*** User from ";
        msg += usr_ip + ":";
        msg += to_string(usr_port) + " is named '" + name + "'. ***\n";
        broadcast(msg);
    }
}

void yell(string msg)
{
    string usr_name = get_user_name(uid);
    string send_msg = "*** ";
    send_msg += usr_name + " yelled ***:" + msg + "\n";

    broadcast(send_msg);
}

void close_other_client_fd(int id)
{
    sem_wait(sem);
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        if((all_clients[i].uid != id) && (all_clients[i].fd > 0))
        {
            close_fd(all_clients[i].fd);
        }
    }
    sem_signal(sem);
}

void client_login(string client_ip, unsigned short client_port)
{
    string msg = "*** User '(no name)' entered from ";
    msg += client_ip;
    msg +=  ":" + to_string(client_port); 
    msg += ". ***\n";

    sem_wait(sem);
    all_clients[uid].pid = client_pid;
    sem_signal(sem);
    broadcast(msg);
}

void client_logout()
{
    string client_name = get_user_name(uid);
    string msg = "*** User '" + client_name + "' left. ***\n";
   
    broadcast(msg);
   //user pipe clear before sent_fd are set to -1
    user_pipe_clear();
    sem_wait(sem);
    all_clients[uid].uid = -1;
    all_clients[uid].pid = -1;
    all_clients[uid].fd = -1;
    strcpy(all_clients[uid].nickname, "");
    strcpy(all_clients[uid].ip, "");
    all_clients[uid].port = 0;
    memset(all_clients[uid].sent_fd, -1, sizeof(all_clients[uid].sent_fd));
    all_clients[uid].sender_id = -1;
    all_clients[uid].clear_id = -1;
    sem_signal(sem);

}

int client_init(string client_ip, unsigned short client_port)
{
    sem_wait(sem); 
    int i = 0;
    for(i = 1; i <= MAX_CLIENTS; ++i)
    {
        if(all_clients[i].uid == -1)
        {
            all_clients[i].uid = i;
            strcpy(all_clients[i].nickname, "(no name)");
            all_clients[i].fd = client_sock;
            strcpy(all_clients[i].ip, client_ip.c_str());
            all_clients[i].port = client_port;
            all_clients[i].sender_id = -1;
            all_clients[i].clear_id = -1;
            break;
        }
    }
    sem_signal(sem);
    return i;
}

string get_user_ip()
{
    string ip;
    sem_wait(sem);
    ip = all_clients[uid].ip;
    sem_signal(sem);
    return ip;
}

unsigned short get_user_port()
{
    unsigned short port;
    sem_wait(sem);
    port = all_clients[uid].port;
    sem_signal(sem);
    return port;
}

string get_user_name(int id)
{
    string name;
    sem_wait(sem);
    name = all_clients[id].nickname;
    sem_signal(sem);
    return name;
}

bool usr_exists(int id)
{
    bool flag = false;
    sem_wait(sem);
    if(all_clients[id].uid > 0)
        flag = true;
    sem_signal(sem);
    return flag;
}

string get_fifo_name(int in_id, int out_id)
{
    string re = to_string(in_id) + "_" + to_string(out_id) + "_fifo";
    return re;
}
