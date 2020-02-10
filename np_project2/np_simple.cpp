#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "Parser.h"
#include "utils.h"
#define BUFFSIZE 15000
#define MAX_CLIENTS 30
void run_npshell();
pid_t fork_process();
void child_handler(int signo);


int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        cout << "Usage: " << argv[0] << " [port_num]" << endl;
        exit(1);
    }
    int sockfd, client_fd;
    uint32_t port = (uint32_t)atoi(argv[1]);
    struct sockaddr_in client_addr, server_addr;

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

    if(listen(sockfd, MAX_CLIENTS) < 0)
    {
        cerr << "Server can't listen socket" << endl;
        exit(1);
    }
    while(1)
    {
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        cout << "Accept Client " << client_fd << endl;
        if(client_fd < 0)
        {
            // cerr << "Can't connect to server" << endl;
            exit(1);
        }
        else
        {
            pid_t pid;
            if((pid = fork_process()) == 0)
            {
                close_fd(STDIN_FILENO);
                close_fd(STDOUT_FILENO);
                close_fd(STDERR_FILENO);
                close_fd(sockfd);
                set_readfd(client_fd, STDIN_FILENO);
                set_writefd(client_fd, STDOUT_FILENO);
                set_writefd(client_fd, STDERR_FILENO);
                close_fd(client_fd);
                run_npshell();
                exit(0);
            }
            else if(pid > 0)
            {
                close_fd(client_fd);
            }
        }
    }
    
    return 0;
}

void run_npshell()
{
    string initial_path = "bin:.";
    clearenv();
    setenv("PATH", initial_path.c_str() , 1);
    bool exit_flag = false;
    string input_line;
    vector<pipe_counter> pipes;
    char input_buff[BUFFSIZE];
    
    while(1)
    {
        string prompt = "% ";
        write(1, prompt.c_str(), prompt.length());
        char ch;
        int index = 0;
        memset(input_buff, 0, sizeof(input_buff));
        while(read(0, &ch, 1) == 1)
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
        else {
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
            //update counter
            pipes_update(pipes);
            pipe_read_index = pipe_for_reading(pipes);
            has_on_time_pipe = (pipe_read_index  != -1);
           
            //setenv built-in
            if(vec_args[0] == "setenv")
            {
                if(vec_args.size() != 3)
                    cerr << "setenv usage: ​ setenv [variable name] [value to assign]" << endl;
                else    
                    setenv(vec_args[1].c_str(), vec_args[2].c_str(), 1);
            }
            //printenv bulit-in
            else if(vec_args[0] == "printenv")
            {
                if(vec_args.size() != 2)
                    cerr << "printenv usage: ​ printenv [variable name]" << endl;
                else
                {
                    char* result;
                    if((result = getenv(vec_args[1].c_str())) != NULL)
                    {
                        string result_string = string(result) + "\n";
                        write(1, result_string.c_str(), result_string.length());
                    }
                }   
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
                            close_fd(writefd);
                        }
                    }

                    if(!cmd_exists(vec_args[0]))
                    {
                        cerr << "Unknown command: [" << vec_args[0] << "]." << endl;
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

                if(!command_list[i]->check_if_pipe())
                    pid_list.push_back(pid);
                erase_pipe(pipes, pipe_read_index);
            
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
}