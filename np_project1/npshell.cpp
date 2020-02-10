#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include "Parser.h"
#include "utils.h"
void run_npshell();


int main()
{

    run_npshell();

    return 0;
}

void run_npshell()
{
    string initial_path = "bin:.";
    setenv("PATH", initial_path.c_str() , 1);
    bool exit_flag = false;
    string input_line;
    vector<pipe_counter> pipes;
    
    while(1)
    {
        cout << "% ";
        getline(cin, input_line);
        if(cin.eof())
            break;
            
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
                        cout << result << endl;
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