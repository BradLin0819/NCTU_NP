#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <cerrno>

using namespace std;
using namespace boost::asio;
using namespace boost::algorithm;

io_service global_io_service;
void child_handler(int signo);
void set_fd(int oldfd, int newfd);

class http_session : public enable_shared_from_this<http_session>
{
    private:
        enum { max_length = 1024 };
        ip::tcp::socket _socket;
        char data[max_length];
        string http_header;
        string request_method;
        string request_uri;
        string query_string;
        string server_protocol;
        string http_host;
        string server_addr;
        string server_port;
        string remote_addr;
        string remote_port;
        string exec_file;

    public:
        http_session(ip::tcp::socket socket) : _socket(move(socket))
        {
            //do nothing
        }
        void start()
        { 
            do_read();
        }

    private:
        void do_read()
        {
            auto self(shared_from_this());
            _socket.async_read_some(
                buffer(data, max_length),
                [this, self](boost::system::error_code ec, size_t length)
                {
                    if (!ec)
                    {
                        http_header = data;
                        setup_http_session();
                    }
                });
        }

        void setup_http_session()
        {
            auto self(shared_from_this());
            string status_ok = "HTTP/1.1 200 OK\r\n";
            vector<string> each_line;
            vector<string> first_line;
            vector<string> uri_and_query;
            vector<string> host_line;
            vector<string> host_port;
            boost::system::error_code ec;
            ip::tcp::endpoint server_endpoint = _socket.local_endpoint(ec);
            if (!ec)
            {
                server_addr = server_endpoint.address().to_string();
                server_port = to_string(server_endpoint.port());
            }
            ip::tcp::endpoint client_endpoint = _socket.remote_endpoint(ec);
            if (!ec)
            {
                remote_addr = client_endpoint.address().to_string();
                remote_port = to_string(client_endpoint.port());
            }

            split(each_line, http_header, is_any_of("\r\n"));
            split(first_line, each_line[0], is_any_of(" "));
            split(host_line, each_line[2], is_any_of(" "));
            request_method = first_line[0];
            request_uri = first_line[1];
            if (first_line[1].find("?") != string::npos)
            {
                split(uri_and_query, first_line[1], is_any_of("?"));
                exec_file = uri_and_query[0].substr(1);
                query_string = uri_and_query[1];
            }
            else
            {
                exec_file = request_uri.substr(1);
                query_string = "";
            }
            server_protocol =  first_line[2];
            split(host_port, host_line[1], is_any_of(":"));
            http_host = host_port[0];

            _socket.async_send(
                buffer(status_ok, status_ok.length()),
                [this, self](boost::system::error_code ec, size_t /* length */)
                {
                    if (!ec)
                    {
                        pid_t pid;
                        int socket_fd = _socket.native_handle();
                        string path =  "./";
                        string cgi_file = path + exec_file;

                        global_io_service.notify_fork(io_service::fork_prepare);
                        if ((pid = fork()) < 0)
                        {
                            perror("fork error!\n");
                            exit(EXIT_FAILURE);
                        }
                        else if (pid  == 0)
                        {
                            global_io_service.notify_fork(io_service::fork_child);
                            set_fd(socket_fd, STDIN_FILENO);
                            set_fd(socket_fd, STDOUT_FILENO);
                            setup_env();

                            if (execlp(cgi_file.c_str(), cgi_file.c_str(), NULL) < 0)
                            {
                                cerr << strerror(errno) << endl;
                                exit(EXIT_FAILURE);
                            }
                        }
                        else
                        {
                            global_io_service.notify_fork(io_service::fork_parent);
                            _socket.close();
                        }
                    }
                });
        }

        void setup_env()
        {
            setenv("REQUEST_METHOD", request_method.c_str(), 1);
            setenv("REQUEST_URI", request_uri.c_str(), 1);
            setenv("QUERY_STRING", query_string.c_str(), 1);
            setenv("SERVER_PROTOCOL", server_protocol.c_str(), 1);
            setenv("HTTP_HOST", http_host.c_str(), 1);
            setenv("SERVER_ADDR", server_addr.c_str(), 1);
            setenv("SERVER_PORT", server_port.c_str(), 1);
            setenv("REMOTE_ADDR", remote_addr.c_str(), 1);
            setenv("REMOTE_PORT", remote_port.c_str(), 1);
        }

        void print_http_header()
        {
            cout << request_method << endl;
            cout << request_uri << endl;
            cout << query_string << endl;
            cout << server_protocol << endl;
            cout << http_host << endl;
            cout << server_addr << endl;
            cout << server_port << endl;
            cout << remote_addr << endl;
            cout << remote_port << endl;
        }
};

class httpserver 
{
    private:
        ip::tcp::acceptor _acceptor;
        ip::tcp::socket _socket;

    public:
        httpserver(short port)
            : _acceptor(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), port)),
            _socket(global_io_service)
        {
            do_accept();
        }

    private:
        void do_accept()
        {
            _acceptor.async_accept(_socket, [this](boost::system::error_code ec)
            {
                if (!ec)
                {
                    make_shared<http_session>(move(_socket))->start();
                }
                do_accept();
            });
        }
};

int main(int argc, char* const argv[])
{
    if (argc != 2)
    {
        cerr << "Usage:" << argv[0] << " [port]" << endl;
        return 1;
    }
    signal(SIGCHLD, child_handler);

    try
    {
        unsigned short port = atoi(argv[1]);
        httpserver server(port);
        global_io_service.run();
    }
    catch (exception& e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}

void child_handler(int signo)
{
    int status;
    while(waitpid(-1, &status, WNOHANG) > 0);
}

void set_fd(int oldfd, int newfd)
{
    if(dup2(oldfd, newfd) == -1)
    {
        perror("Modify read fd error!\n");
        exit(1);
    }
}
