#include "Parser.h"
#include <string.h>
#include <ctype.h>
vector<Command*>  Parser::get_command_list( )
{
    vector<Command*> cmd_list;
    vector<string>  args;
    string temp,  pipe_temp;
    //check if it's end of pipe
    bool flag = false;
    int i = 0;
    while(i < command_line.length())
    {
        int num = 0;
        bool exclamation = false, pipe_flag = false;
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
            Command* cmd = new Command();
            string final_cmd = temp, filename = "";
            //command before pipe sign
            if(flag)
                final_cmd = pipe_temp;
            //split with delimiter '>'
            size_t redirection_pos;
            vector<string> args_after_redirection;
            redirection_pos = final_cmd.find('>');
            if(redirection_pos !=  string::npos)
            {
                //first word after '>'
                filename = final_cmd.substr(redirection_pos+1);
                args_after_redirection = split_with_delims(filename, delims);
                filename = args_after_redirection[0];
                //erase filename and get remained arguments
                args_after_redirection.erase(args_after_redirection.begin());
                final_cmd = final_cmd.substr(0, redirection_pos);
            }
            
            //split command string into args with spaces
            args = split_with_delims(final_cmd, delims);
            //concatenate args before '>' and args after '>'
            args.insert(args.end(), args_after_redirection.begin(), args_after_redirection.end());
            if(args.size() != 0)
            {
                //set command object
                cmd->set_args(args);
                cmd->set_exclamation(exclamation);
                cmd->set_filename(filename);
                cmd->set_pipe(pipe_flag);
                cmd->set_pipeN_count(num);
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
            //handle |alphabet
            if(i == command_line.length() && flag && temp.length() != 0)
            {
                if(!isspace(command_line[i-1]) && !isdigit(command_line[i-1]))
                {
                    Command* last_char_cmd = new Command();
                    string str ;
                    str += command_line[i-1];
                    vector<string> char_cmd = split_with_delims(str, delims);
                    last_char_cmd->set_args(char_cmd);
                    last_char_cmd->set_last(true);
                    cmd_list.push_back(last_char_cmd);
                }
            }
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


