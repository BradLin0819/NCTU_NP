#include "Parser.h"
#include <string.h>
#include <ctype.h>
#include <regex>
vector<Command*>  Parser::get_command_list( )
{
    vector<Command*> cmd_list;
    string temp,  pipe_temp;
    //check if it's end of pipe
    bool flag = false;
    int i = 0;
    while(i < command_line.length())
    {
        int num = 0;
        bool exclamation = false, pipe_flag = false;
        vector<string> split_cmd;
        
        //split with delimiter |, |N, !N
        if( command_line[i] == '|' || command_line[i] == '!')
        {
            if(command_line[i] == '!')
                exclamation = true;
            if( command_line[i] == '|')
                pipe_flag = true;
            //look forward 1 digit
            i++;
            while(isdigit(command_line[i]))
            {
                num = num * 10 + (command_line[i] - '0');
                i++;
            }
            flag = true;
            pipe_temp = temp;
            temp.clear();
        }
        //not end of line
        if(command_line[i] != '\n'  && command_line[i] != '\r' &&  command_line[i] != '\0')
            temp += command_line[i++];
        
        //insert commands before pipe sign and the last command into command_list
        if(i == command_line.length() || flag)
        {
            string delims = "\t ";
            string final_cmd = temp, filename = "";
            int pos = 0;
            int number_of_user_pipe_out_cmd = 0;
            
            //command before pipe sign
            if(flag)
                final_cmd = pipe_temp;
            split_cmd = split_with_delims(final_cmd, delims);
            number_of_user_pipe_out_cmd = number_of_user_pipe_out(split_cmd);
            do
            {
                Command* cmd = new Command();
                vector<string>  args;
                bool user_pipe_flag = false;
                int user_pipe_in = 0;
                int user_pipe_out = 0;

                for(; pos < split_cmd.size();)
                {
                    string current_arg = split_cmd[pos];
                    
                    if(is_redirection(current_arg))
                    {
                        filename = split_cmd[++pos];
                    }
                    else if(is_user_pipe_in(current_arg))
                    {
                        user_pipe_in = atoi(current_arg.substr(1).c_str());
                    }
                    else if(is_user_pipe_out(current_arg))
                    {
                        user_pipe_out = atoi(current_arg.substr(1).c_str());
                        user_pipe_flag = true;
                        number_of_user_pipe_out_cmd--;
                    }
                    else
                    {
                        if(!user_pipe_flag)
                            args.push_back(current_arg);
                        else
                            break;
                    }
                    pos++;
                }
                
                if(args.size() != 0)
                {
                    //set command object
                    cmd->set_args(args);
                    cmd->set_exclamation(exclamation);
                    cmd->set_filename(filename);
                    cmd->set_pipe(pipe_flag);
                    cmd->set_pipeN_count(num);
                    cmd->set_user_pipe_in(user_pipe_in);
                    cmd->set_user_pipe_out(user_pipe_out);
                    if(cmd_list.size() == 0)
                        cmd->set_first(true);
                    if(i == command_line.length() && temp.length() == 0)
                        cmd->set_last(true);
                    cmd_list.push_back(cmd);
                }
                else
                {
                    delete cmd;
                }
            }while(number_of_user_pipe_out_cmd > 0);
            
            //handle |alphabet
            // if(i == command_line.length() && flag && temp.length() != 0)
            // {
            //     if(!isspace(command_line[i-1]) && !isdigit(command_line[i-1]))
            //     {
            //         Command* last_char_cmd = new Command();
            //         string str ;
            //         str += command_line[i-1];
            //         vector<string> char_cmd = split_with_delims(str, delims);
            //         last_char_cmd->set_args(char_cmd);
            //         last_char_cmd->set_last(true);
            //         cmd_list.push_back(last_char_cmd);
            //     }
            // }
            exclamation = false;
            pipe_flag = false;
            flag = false;
        }
    }
    return cmd_list;
}

vector<string> split_with_delims(const string& cmd, const string delims)
{
    vector<string> args;
    string result;
    char* temp_dup = strdup(cmd.c_str());
    char* delimiter = (char*)delims.c_str();
    char* pch;
    
    pch = strtok(temp_dup, delimiter);
    while(pch != NULL)
    {
        result = pch;
        args.push_back(result);
        pch = strtok(NULL, delimiter);
    }
    free(temp_dup);
    return args;
}

bool is_redirection(string arg)
{
    return regex_match(arg, regex("^>$"));
}

bool is_user_pipe_in(string arg)
{
    return regex_match(arg, regex("^<[0-9]+$"));
}

bool is_user_pipe_out(string arg)
{
    return regex_match(arg, regex("^>[0-9]+$"));
}

int number_of_user_pipe_out(const vector<string>& split_cmd)
{
    int number= 0;
    for(int i = 0; i < split_cmd.size(); ++i)
    {
        if(is_user_pipe_out(split_cmd[i]))
            number++;
    }
    return number;
}

