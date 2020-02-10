#include <iostream>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <map>
#include <fstream>
#include <regex>
#include <string.h>

using namespace std;
using namespace boost::asio;
using namespace boost::algorithm;

#define MAXSERVER 5
int host_num;

void print_console_html();
void output_shell(string session, char* content);
void output_command(string session, char* content);
string escape(string content);
string get_environment_variable(string index);
int setup_connection_conf(map< string, map<string, string> >& conf);

io_service global_io_service;
map< string, map<string, string> > server_conf;

class cgi_session : public enable_shared_from_this<cgi_session> 
{
    private:
        enum { max_length = 4096 };
        string session_name;
        ip::tcp::socket _socket;
        ip::tcp::resolver resolv;
        char data[max_length];
        ifstream file_input;
        deadline_timer delay;

    public:
        cgi_session(string session_name) : session_name(session_name), _socket(global_io_service), resolv(global_io_service), 
            delay(global_io_service), file_input(server_conf[session_name]["file"]) 
        {
            //do nothing
        }
        void start()
        {
            do_resolve();
        }

    private:
        void do_resolve()
        {
            auto self(shared_from_this());
            ip::tcp::resolver::query _query(server_conf[session_name]["host"], server_conf[session_name]["port"]);

            resolv.async_resolve(_query,
                [this, self](boost::system::error_code ec, ip::tcp::resolver::iterator it)
                {
                    if (!ec)
                    {
                        do_connect(it);
                    }
                });
        }
        
        void do_connect(ip::tcp::resolver::iterator it)
        {
            auto self(shared_from_this());

            _socket.async_connect(*it,
                [this, self](boost::system::error_code ec)
                {
                    if (!ec)
                    {
                        do_read();
                    }
                });
        }

        void do_read() 
        {
            auto self(shared_from_this());

            memset(data, 0, sizeof(data));
            _socket.async_read_some(
                buffer(data, max_length),
                [this, self](boost::system::error_code ec, size_t length)
                {
                    if (!ec)
                    {
                        output_shell(session_name, data);
                        if (string(data).find("% ") != string::npos)
                        {
                            delay.expires_from_now(boost::posix_time::millisec(200));
                            delay.async_wait(
                                [this, self](boost::system::error_code ec)
                                {
                                    if (!ec)
                                    {
                                        do_send_cmd();
                                    }
                                });
                        }
                        do_read();
                    }
                });
        }

        void do_send_cmd() 
        {
            auto self(shared_from_this());
            string input_line = "";

            getline(file_input, input_line);
            if (input_line == "exit")
            {
                file_input.close();
            }
            input_line += "\n";
            output_command(session_name, (char*)input_line.c_str());
            _socket.async_write_some(
                buffer(input_line.c_str(), input_line.length()), 
                [this, self](boost::system::error_code ec, size_t /*length*/)
                {});
        }
};

int main()
{
    host_num = 0;
    host_num = setup_connection_conf(server_conf);
    print_console_html();
    try
    {
        for (int i = 0; i < host_num; ++i)
        {
            string session_name = "s" + to_string(i);
            make_shared<cgi_session>(session_name)->start();
        }
        global_io_service.run();
    }
    catch (exception& e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}

void print_console_html()
{
    cout << "Content-type: text/html" << "\r\n\r\n";
    cout << "<!DOCTYPE html>" << endl;
    cout << "<html lang=\"en\">" << endl;
    cout << "  <head>" << endl;
    cout << "    <meta charset=\"UTF-8\" />" << endl;
    cout << "    <title>NP Project 3 Console</title>" << endl;
    cout << "    <link" << endl;
    cout << "      rel=\"stylesheet\"" << endl;
    cout << "      href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"" << endl;
    cout << "      integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"" << endl;
    cout << "      crossorigin=\"anonymous\"" << endl;
    cout << "    />" << endl;
    cout << "    <link" << endl;
    cout << "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"" << endl;
    cout << "      rel=\"stylesheet\"" << endl;
    cout << "    />" << endl;
    cout << "    <link" << endl;
    cout << "      rel=\"icon\"" << endl;
    cout << "      type=\"image/png\"" << endl;
    cout << "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"" << endl;
    cout << "    />" << endl;
    cout << "    <style>" << endl;
    cout << "      * {" << endl;
    cout << "        font-family: 'Source Code Pro', monospace;" << endl;
    cout << "        font-size: 1rem !important;" << endl;
    cout << "      }" << endl;
    cout << "      body {" << endl;
    cout << "        background-color: #212529;" << endl;
    cout << "      }" << endl;
    cout << "      pre {" << endl;
    cout << "        color: #cccccc;" << endl;
    cout << "      }" << endl;
    cout << "      b {" << endl;
    cout << "        color: #ffffff;" << endl;
    cout << "      }" << endl;
    cout << "    </style>" << endl;
    cout << "  </head>" << endl;
    cout << "  <body>" << endl;
    cout << "    <table class=\"table table-dark table-bordered\">" << endl;
    cout << "      <thead>" << endl;
    cout << "        <tr>" << endl;

    for (int i = 0; i < host_num; ++i)
    {
        string session_name = "s" + to_string(i);
        cout << "          <th scope=\"col\">" << server_conf[session_name]["host"] << ":" << server_conf[session_name]["port"] 
             << "</th>" << endl; 
    }

    cout << "        </tr>" << endl;
    cout << "      </thead>" << endl;
    cout << "      <tbody>" << endl;
    cout << "        <tr>" << endl;
    for (int i = 0; i < host_num; ++i)
    {
        string session_name = "s" + to_string(i);
        cout << "          <td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>\n";
    }
    cout << "        </tr>" << endl;
    cout << "      </tbody>" << endl;
    cout << "    </table>" << endl;
    cout << "  </body>" << endl;
    cout << "</html>" << endl;
}

string escape(string content)
{
    string result = "";

    for (int i = 0; i < content.length(); ++i)
    {
        if (isalnum(content[i]))
        {
            result += content[i];
        }
        else
        {
            result += ("&#" + to_string(int(content[i])) + ";");
        }
    }
    return result;
}

void output_shell(string session, char* content)
{
    string content_string = content;

    content_string = escape(content_string);
    cout << "<script>document.getElementById('" << session << "').innerHTML += '" << content_string << "';</script>" << endl;
}

void output_command(string session, char* content)
{
    string content_string = content;

    content_string = escape(content_string);
    cout << "<script>document.getElementById('" << session << "').innerHTML += '<b>" << content_string << "</b>';</script>" << endl;
}

string get_environment_variable(string index)
{
    char* result;
    if ((result = getenv(index.c_str())) != NULL)
    {
        return result;
    }
    return "";
}

int setup_connection_conf(map< string, map<string, string> >& conf)
{
    string query_string = get_environment_variable("QUERY_STRING");
    vector<string> parsed_query_string;
    split(parsed_query_string, query_string, is_any_of("&"));
    int temp = 0;

    for (int i = 0; i < parsed_query_string.size(); ++i)
    {
        vector<string> pairs;
        string session_no;
        
        split(pairs, parsed_query_string[i], is_any_of("="));
        session_no = "s" + pairs[0].substr(1);
        //host
        if (pairs[0][0] == 'h')
        {
            conf[session_no]["host"] = pairs[1];
            if (pairs[1] != "")
                temp++;
        }
        //port
        else if (pairs[0][0] == 'p')
        {
            conf[session_no]["port"] = pairs[1];
        }
        //file
        else if (pairs[0][0] == 'f')
        {
            conf[session_no]["file"] = "test_case/" + pairs[1];
        }
    }
    return temp;
}