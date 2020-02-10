#include "Command.h"

Command::Command()
{
    filename = "";
    pipeN_count = 0;
    is_first = false;
    is_last = false;
    has_pipe = false;
    has_exclamation = false;
}

void Command::set_args(const vector<string>& args)
{
    this->args = args;
}

void Command::set_pipe(bool has_pipe)
{
    this->has_pipe = has_pipe;
}

void Command::set_exclamation(bool exclamation)
{
    this->has_exclamation = exclamation;
}

void Command::set_filename(string filename)
{
    this->filename = filename;
}

void Command::set_pipeN_count(int num)
{
    this->pipeN_count = num;
}

void Command::set_first(bool is_first)
{
    this->is_first = is_first;
}

void Command::set_last(bool is_last)
{
    this->is_last = is_last;
}

bool Command::check_first()
{
    return this->is_first;
}

bool Command::check_last()
{
    return this->is_last;
}

bool Command::check_singlecmd()
{
    return this->is_first && this->is_last;
}

bool Command::check_pipe()
{
    return (this->has_pipe) && (pipeN_count == 0);
}

bool Command::check_pipeN()
{
    return (this->has_pipe) && (pipeN_count > 0);
}

bool Command::check_exclamation()
{
    return this->has_exclamation;
}

bool Command::check_redirection()
{
    return filename.length() != 0;
}

bool Command::check_if_pipe()
{
    return check_pipe() || check_pipeN() || check_exclamation();
}

int Command::get_pipeN_count()
{
    return this->pipeN_count;
}

string Command::get_filename()
{
    return this->filename;
}

const vector<string>& Command::get_args()
{
    return this->args;
}

void Command::info()
{
    cout << "Args:" ;
    for(int i = 0; i < this->args.size(); ++i)
    {
        cout << args[i] << " ";
    }
    cout << endl;
    cout << "Fisrt command:" << check_first() << endl;
    cout << "Last command:" << check_last() << endl;
    cout << "Pipe:" << check_pipe() << endl;
    cout << "PipeN:" << check_pipeN() << endl;
    cout << "Exclamation:" << check_exclamation() << endl;
    cout << "PipeNCount:" << pipeN_count << endl;
    cout << "Redirection:" << check_redirection() << endl;
    cout << endl ;
}
