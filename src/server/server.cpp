#include <iostream>
#include <cstring>
#include <errno.h>
#include <thread>
#include <vector>

#if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
    #define OS_WIN
    #include <winsock2.h>
    #include <ws2tcpip.h>
#elif defined(__linux__)
    #define OS_LINUX
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h> //To be able to close the socket
#endif

#include "../common_headers/srvrspcodes.hpp"
#include "srvfile.hpp"

#define DEFAULT_PORT "3490"
#define BACKLOG 5
#define FILES_DIR "./files/"

void conn_info(struct addrinfo* addr_info){
    
    void *addr;
    short port_n;
    char ipstr[INET6_ADDRSTRLEN];

    if(addr_info->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)addr_info->ai_addr;
        port_n = ntohs(ipv4->sin_port); //ntohs : Network (litle-ending) TO Host (often big-endian) Short
        addr = &(ipv4->sin_addr);
    }else {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)addr_info->ai_addr;
        port_n = ntohs(ipv6->sin6_port); //ntohs : Network (litle-ending) TO Host (often big-endian) Short
        addr = &(ipv6->sin6_addr);
    }

    inet_ntop(addr_info->ai_family, addr, ipstr, sizeof ipstr);
    std::cout << "Listening on " << ipstr << " on port " << port_n << std::endl;
    
}

Message * get_request(int new_fd){
    int bytes_rec;

    int msg_len;
    bytes_rec = recv(new_fd, &msg_len, 4, 0);
    if(bytes_rec  == -1){
        return nullptr;
    }

    msg_len = ntohl(msg_len);
    int bytes_to_rec = msg_len;

    char* req = (char*)malloc(bytes_to_rec);
    char* req_ptr = req;

    do{
        bytes_rec = recv(new_fd, req_ptr, bytes_to_rec, 0);
        if(bytes_rec == -1){
            return nullptr;
        }

        bytes_to_rec -= bytes_rec;
        req_ptr += bytes_rec;
    }while(bytes_to_rec > 0);

    Message* msg = new Message(req, msg_len);
    std::cout << "Received request || id: " << std::this_thread::get_id() << std::endl;
    return msg;
}

void send_msg(int clientfd, Message * msg){
    int bytes_sent;
    unsigned int msg_bytes = msg->size();

    unsigned int msg_b_n = htonl(msg_bytes);
    bytes_sent = send(clientfd, &msg_b_n, 4, 0); //0 is the value passed to the *flags* argument
    if(bytes_sent==-1){
        std::cout << "Error sending message size! || id: "  << std::this_thread::get_id() << std::endl;
        return;
    }

    char* msg_start = msg->get_msg();

    do{
        bytes_sent = send(clientfd, msg_start, msg_bytes, 0); //0 is the value passed to the *flags* argument
        if(bytes_sent==-1){
            std::cout << "Error sending file! Code " << strerror(errno) << " || id: "  << std::this_thread::get_id() << std::endl;
            break;
        }
        msg_bytes -= bytes_sent;
        msg_start += bytes_sent;
    }while(msg_bytes>0);

}

void cpy_str(char* str, const char* cstr){
    while(*cstr){
        *str = *cstr;
        str++;
        cstr++;
    }
}

void inform(int clientfd, const char* str){

    const char * str_pt = str;
    int len = 0;
    while(*str_pt){
        str_pt++;
        len++;
    }

    char *arr = (char *)malloc(len + 1);
    arr[0] = ServerCodes::CON_ON;
    arr[0]+= ServerCodes::INFO;

    cpy_str(arr+1, str);
    Message* msg = new Message(arr, len+1);

    send_msg(clientfd, msg);

}

bool load_and_send_file(int clientfd, Message* msg){

    std::string filename(FILES_DIR);

    filename.append(msg->get_msg(), msg->size());

    char stat_byte = ServerCodes::CON_OFF;
    stat_byte += ServerCodes::FILE;

    std::cout << "Loading file..." << std::endl;

    ServerFile* file = new ServerFile(filename, stat_byte);

    if(file->failed()){
        std::cout << "Loading the file failed with error code " << (int)(file->errcode()) << ".\n";
        inform(clientfd, "File could not be retrieved.");

        delete file;
        return false;
    }

    std::cout << "Sending file... || id: "  << std::this_thread::get_id() << std::endl;
    send_msg(clientfd, file);
    std::cout << "File sent. || id: "  << std::this_thread::get_id() << std::endl;

    delete file;
    return true;
}

bool proccess_request(int clientfd, Message * msg){

    //NOT THREAD SAFE
    static bool awaiting_filename = false;
    //---------------
    bool terminate = false;

    if(!awaiting_filename){
        char c = msg->get(0);
            switch(c){
            case 'h':
                inform(clientfd, "TODO: Show available commands.");
                //TODO:
                //print_help();
                break;
            case 'l':
                inform(clientfd, "TODO: Show available files.");
                //TODO:
                //list_files();
                break;
            case 'g':
                inform(clientfd, "Which file do you wish to retrieve?");
                awaiting_filename = true;
                break;
            default:
                inform(clientfd, "Invalid command.");
        }
    }else{
        
        awaiting_filename = false;

        terminate = load_and_send_file(clientfd, msg);

    }

    delete msg;

    return terminate;
}

void handle_client(int clientfd){
    bool terminate = false;
    std::cout << "Handling client! || id: " << std::this_thread::get_id() << std::endl;

    while(!terminate){
        std::cout << "Awaiting request... || id: "  << std::this_thread::get_id() << std::endl;
        Message* msg = get_request(clientfd);
        if(msg == nullptr){
            std::cout << "Could not proccess request. || id: "  << std::this_thread::get_id() << std::endl;
            exit(1);
        }

        terminate = proccess_request(clientfd, msg);
    }

    #ifdef OS_WIN
        closesocket(clientfd);
    #elif defined(OS_LINUX)
        close(clientfd);
    #endif

    std::cout << "Client connection closed; ending thread "  << std::this_thread::get_id() << std::endl;

}

int main(int argc, char* argv[]){

    const char* PORT_STR = DEFAULT_PORT; //For C compatibility; getaddrinfo
    if(argc>=2){
        PORT_STR = argv[1];
    }

    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    struct addrinfo* servinfo;
    struct addrinfo hints;
    int sockfd, new_fd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    //Don't care if IPv4 (AF_INET) or IPV6(AF_INET6)
    hints.ai_socktype = SOCK_STREAM; //TCP stream sockets; SOCK_DGRAM for datagram
    hints.ai_flags = AI_PASSIVE;    //Fill in my ip for me

    int status = getaddrinfo( "192.168.1.100", PORT_STR, &hints, &servinfo );

    if(status != 0){
        std::cout << "getaddrinfo error" << std::endl;
        exit(1);
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    
    status = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);

    if(status == -1){
        std::cout << "'bind()' failed; port may be in use" << std::endl;
        exit(1);
    }

    conn_info(servinfo);
    
    //int listen ( int sockfd, int backlog ); -> backlog is the number of connections allowed on the incoming queue
    status = listen(sockfd, BACKLOG);

    if(status == -1){
        std::cout << "'listen()' failed" << std::endl;
        exit(1);
    }

    addr_size = sizeof client_addr;
    std::vector< std::thread* > thrds;

    while(true){
        new_fd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_size);
        std::thread* th = new std::thread(handle_client, new_fd);
        thrds.push_back(th);
    }

    for(auto t : thrds){
        t->join();
        delete t;
    }

    #ifdef OS_WIN
        closesocket(sockfd);
    #elif defined(OS_LINUX)
        close(sockfd);
    #endif

    std::cout << "Server shutting down." << std::endl;
    freeaddrinfo(servinfo);
    return 0;
}
