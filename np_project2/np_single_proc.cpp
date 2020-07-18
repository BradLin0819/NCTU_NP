#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <map>
#include <error.h>
#include "Parser.h"
#include "utils.h"
#include "np_single_proc.h"

client_info all_clients[MAX_CLIENTS+1];
int client_sock;
fd_set afds;
fd_set rfds;
map<int, int> fd_uid_map;

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        cout << "Usage: " << argv[0] << " [port_num]" << endl;
        exit(1);
    }
    int sockfd;
    int fdmax;
    int uid;
    uint32_t port = (uint32_t)atoi(argv[1]);
    string prompt = "% ";
    struct sockaddr_in client_addr, server_addr;

    signal(SIGINT, clear_all_remain_user_pipes);
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

    FD_ZERO(&afds);
    FD_ZERO(&rfds);
    FD_SET(sockfd, &afds);
    fdmax = sockfd;

    set_up_all_clients();

    while(1)
    {
        rfds = afds;
        
        if (select(fdmax+1, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
        {
            cerr << "select error " << strerror(errno) << endl;
        }
        else
        {
           for(int i = 0; i <= fdmax; ++i)
            {
                if(FD_ISSET(i, &rfds))
                {
                    if(i == sockfd)
                    {
                        socklen_t client_len = sizeof(client_addr);
                        if((client_sock = accept(sockfd, (struct sockaddr*)&client_addr, &client_len)) < 0)
                        {
                            cerr << "accept failed" << endl;
                            continue;
                        }
                        else
                        {
                            FD_SET(client_sock, &afds);
                            
                            fdmax = (client_sock > fdmax) ? client_sock : fdmax;

                            string client_ip = inet_ntoa(client_addr.sin_addr);
                            unsigned short client_port = ntohs(client_addr.sin_port);

                            uid = client_init(client_ip, client_port);
                            fd_uid_map[client_sock] = uid;
                            
                            cout << "Accept Client " << client_sock << endl;

                            welcome_message(client_sock);
                            client_login(client_ip, client_port);
                            write(client_sock, prompt.c_str(), prompt.length());
                        }
                    }
                    else
                    {
                        char input_buff[BUFFSIZE];
                        char ch;
                        int index = 0;
                        string input_line = "";
                        string initial_path = "bin:.";

                        memset(input_buff, 0, sizeof(input_buff));
                        
                        while(read(i, &ch, 1) == 1)
                        {
                            input_buff[index++] = ch;
                            if(ch == '\n')
                                break;
                        }
                        if(strcmp(input_buff, "") == 0)
                        {
                            close(i);
                            FD_CLR(i, &afds);
                        }
                        else
                        {
                            if(strcmp(input_buff, "\r\n") != 0 && strcmp(input_buff, "\n") != 0)
                            {
                                input_line = split_with_delims(input_buff, "\n")[0];
                                input_line = split_with_delims(input_line, "\r")[0];
                            }
                            if(input_line == "exit")
                            {
                                client_logout(fd_uid_map[i]);
                                close(i);
                                FD_CLR(i, &afds);
                            }
                            else
                            {
                                clearenv();
                                set_env(all_clients[fd_uid_map[i]].env);
                                run_npshell(fd_uid_map[i], i, input_line);
                                write(i, prompt.c_str(), prompt.length());
                            }
                        }    
                    }
                }
            }
        }
    }
    
    return 0;
}

void run_npshell(int uid, int client_fd, string input_line)
{
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
        int to_be_close_readfd = -1;
        int to_be_close_writefd = -1;
        bool has_on_time_pipe = false;
        bool has_user_pipe_in = false;
        
        //update counter
        pipes_update(all_clients[uid].npipes);
        pipe_read_index = pipe_for_reading(all_clients[uid].npipes);
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
            {
                 setenv(vec_args[1].c_str(), vec_args[2].c_str(), 1);
                 all_clients[uid].env[vec_args[1]] = vec_args[2];
            }    
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
            tell(uid, atoi(vec_args[1].c_str()), send_msg);
            erase_pipe(all_clients[uid].npipes, pipe_read_index);
            break;
        }
        else if(vec_args[0] == "name")
        {
            rename(uid, vec_args[1]);
        }
        else if(vec_args[0] == "yell")
        {
            int yell_pos = input_line.find(vec_args[0]);
            string send_msg = input_line.substr(yell_pos+vec_args[0].length());

            yell(uid, send_msg);
            erase_pipe(all_clients[uid].npipes, pipe_read_index);
            break;
        }
        else
        {
            int pipefd[2];
            pid_t pid;
            bool close_stdout = true;
            bool errpipe = false;
            bool tofile = false;
            bool user_pipe = false;
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
                    string send_msg = "*** ";

                    if(user_pipe_exists(uid, in_id, RECEIVE))
                    {
                        readfd = all_clients[uid].sent_fd[in_id];
                        all_clients[uid].sent_fd[in_id] = -1;
                        all_clients[in_id].send_fd[uid] = -1;
                        
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
                pipe_write_index = pipe_for_writing(all_clients[uid].npipes, pipeN);
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
                    all_clients[uid].npipes.push_back(new_pipe);
                    pipe_write_index = pipe_for_writing(all_clients[uid].npipes, pipeN);
                }
                //pipe exists
                else
                {
                    writefd = all_clients[uid].npipes[pipe_write_index].writefd;
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
                    if(user_pipe_exists(uid, out_id, SEND))
                    {
                        err = true;
                        err_msg = "*** Error: the pipe #";
                        err_msg += to_string(uid) + "->#";
                        err_msg += to_string(out_id) + " already exists. ***\n";
                    }
                    else
                    {
                        string send_msg = "*** ";
                        int pipefd[2];

                        if(pipe(pipefd) == -1)
                        {
                            perror("Create pipes error!\n");
                            exit(1);
                        }

                        send_msg += get_user_name(uid) + " (#";
                        send_msg += to_string(uid) + ") just piped '";
                        send_msg += input_line + "' to ";
                        send_msg += get_user_name(out_id) + " (#";
                        send_msg +=  to_string(out_id) + ") ***\n";

                        all_clients[out_id].sent_fd[uid] = pipefd[0];
                        all_clients[uid].send_fd[out_id] = pipefd[1];
                        to_be_close_readfd = pipefd[0];
                        writefd = pipefd[1];
                        user_pipe = true;

                        broadcast(send_msg);
                        
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
                    erase_pipe(all_clients[uid].npipes, pipe_read_index);
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
                    readfd = all_clients[uid].npipes[pipe_read_index].readfd;
                    close_fd(all_clients[uid].npipes[pipe_read_index].writefd);
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
                        if(!tofile && !user_pipe)
                            close_fd(all_clients[uid].npipes[pipe_write_index].readfd);
                        else if(user_pipe)
                            close_fd(to_be_close_readfd);
                        
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

            erase_pipe(all_clients[uid].npipes, pipe_read_index);

            if(has_user_pipe_in)
            {
                close_fd(readfd);
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

void set_up_all_clients()
{
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        all_clients[i].uid = -1;
        fd_uid_map[all_clients[i].fd] = -1;
        all_clients[i].fd = -1;
        strcpy(all_clients[i].nickname, "");
        strcpy(all_clients[i].ip, "");
        all_clients[i].port = 0;
        memset(all_clients[i].sent_fd, -1, sizeof(all_clients[i].sent_fd));
        memset(all_clients[i].send_fd, -1, sizeof(all_clients[i].send_fd));
        all_clients[i].npipes.clear();
        all_clients[i].env.clear();
    }
}

void welcome_message(int client_fd)
{
    string message = "****************************************\n** Welcome to the information server. **\n****************************************\n";
    write(client_fd, message.c_str(), message.length());
}

void broadcast(string msg)
{
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        if(all_clients[i].uid != -1)
        {
            write(all_clients[i].fd, msg.c_str(), msg.length());
        }
    }
}

void who(int client_fd)
{
    string header = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    string uid;
    string name;
    string ip;
    string is_me = "<-me";
    string port;

    write(client_fd, header.c_str(), header.length());

    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        if(all_clients[i].uid > 0)
        {
            uid = to_string(all_clients[i].uid) + "\t";
            name = all_clients[i].nickname ;
            name += "\t";
            ip = all_clients[i].ip;
            ip += ":";
            port = to_string(all_clients[i].port) + "\t";
            write(client_fd, uid.c_str(), uid.length());
            write(client_fd, name.c_str(), name.length());
            write(client_fd, ip.c_str(), ip.length());
            write(client_fd, port.c_str(), port.length());
            if(all_clients[i].fd == client_fd)
                write(client_fd, is_me.c_str(), is_me.length());
            write(client_fd, "\n", 1);
        }
    }
}

void tell(int uid, int tell_id, string msg)
{
    if(all_clients[tell_id].uid < 0)
    {
        string err_msg = "*** Error: user #" + to_string(tell_id);  
        err_msg += " does not exist yet. ***\n";
        write(all_clients[uid].fd, err_msg.c_str(), err_msg.length());
    }
    else
    {
        string send_msg = "*** ";
        send_msg += all_clients[uid].nickname;
        send_msg += " told you ***:" + msg + "\n";
        write(all_clients[tell_id].fd, send_msg.c_str(), send_msg.length());
    }
}

bool name_exists(string name)
{
    bool has_same_name = false;
    
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        if(strcmp(all_clients[i].nickname, name.c_str()) == 0)
            has_same_name = true;
    }
    return has_same_name;
}

void rename(int uid, string name)
{
    if(name_exists(name))
    {
        string err_msg = "*** User '" + name;
        err_msg += "' already exists. ***\n";
        write(all_clients[uid].fd, err_msg.c_str(), err_msg.length());
    }
    else
    {
        strcpy(all_clients[uid].nickname, name.c_str());
        string usr_ip = get_user_ip(uid);
        unsigned short usr_port = get_user_port(uid);
        string msg = "*** User from ";
        msg += usr_ip + ":";
        msg += to_string(usr_port) + " is named '" + name + "'. ***\n";
        broadcast(msg);
    }
}

void yell(int uid, string msg)
{
    string usr_name = get_user_name(uid);
    string send_msg = "*** ";
    send_msg += usr_name + " yelled ***:" + msg + "\n";

    broadcast(send_msg);
}

int client_init(string client_ip, unsigned short client_port)
{
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
            memset(all_clients[i].sent_fd, -1, sizeof(all_clients[i].sent_fd));
            memset(all_clients[i].send_fd, -1, sizeof(all_clients[i].send_fd));
            all_clients[i].env["PATH"] = "bin:.";
            all_clients[i].npipes.clear();
            break;
        }
    }
    return i;
}

void client_login(string client_ip, unsigned short client_port)
{
    string msg = "*** User '(no name)' entered from ";
    msg += client_ip;
    msg +=  ":" + to_string(client_port); 
    msg += ". ***\n";

    broadcast(msg);
}

void client_logout(int uid)
{
    string client_name = get_user_name(uid);
    string msg = "*** User '" + client_name + "' left. ***\n";

    broadcast(msg);
    user_pipe_clear(uid);
    all_clients[uid].uid = -1;
    fd_uid_map[all_clients[uid].fd] = -1;
    all_clients[uid].fd = -1;
    strcpy(all_clients[uid].nickname, "");
    strcpy(all_clients[uid].ip, "");
    all_clients[uid].port = 0;
    memset(all_clients[uid].sent_fd, -1, sizeof(all_clients[uid].sent_fd));
    memset(all_clients[uid].send_fd, -1, sizeof(all_clients[uid].send_fd));
    all_clients[uid].npipes.clear();
    all_clients[uid].env.clear();

}

string get_user_ip(int uid)
{
    string ip;
    ip = all_clients[uid].ip;
    return ip;
}

unsigned short get_user_port(int uid)
{
    unsigned short port;
    port = all_clients[uid].port;
    return port;
}

string get_user_name(int uid)
{
    string name;
    name = all_clients[uid].nickname;
    return name;
}

bool usr_exists(int uid)
{
    bool flag = false;
    if(all_clients[uid].uid > 0)
        flag = true;
    return flag;
}

bool user_pipe_exists(int uid, int other_uid, MODE mode)
{
    //send
    if(mode == SEND)
    {
        return all_clients[uid].send_fd[other_uid] > 0;
    }
    //receive
    else
    {
        return all_clients[uid].sent_fd[other_uid] > 0;
    }
}

void user_pipe_clear(int uid)
{
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        int close_send_me_fd;
        int close_i_send_fd;

        close_send_me_fd = all_clients[uid].sent_fd[i];
        close_i_send_fd = all_clients[uid].send_fd[i];
        
        //Exit when user is receiver.
        if(close_send_me_fd > 0)
        {
            close_fd(close_send_me_fd);
            all_clients[i].send_fd[uid] = -1;
        }
        //Exit when user is sender.
        if(close_i_send_fd > 0)
        {
            close_fd(all_clients[i].sent_fd[uid]);
            all_clients[i].sent_fd[uid] = -1;
        }
    }
}

void clear_all_remain_user_pipes(int signo)
{
    for(int i = 1; i <= MAX_CLIENTS; ++i)
    {
        for(int j=1; j <= MAX_CLIENTS; ++j)
        {
            if(all_clients[i].sent_fd[j] > 0)
            {
                close_fd(all_clients[i].sent_fd[j]);
                all_clients[i].sent_fd[j] = -1;
            }
        }
    }
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

void set_env(map<string, string> environment)
{
    map<string, string>::iterator it;
    for(it = environment.begin(); it != environment.end(); ++it)
    {
        setenv(it->first.c_str(), it->second.c_str(), 1);
    }
}

