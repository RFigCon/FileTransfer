#include <iostream>
#include <cstring>
#include <fstream>

#if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
    #define OS_WIN
    #include <winsock2.h>
    #include <ws2tcpip.h>
#elif defined(__linux__)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h> //To be able to close the socket
#endif

#include "../common_headers/message.hpp"
#include "../common_headers/srvrspcodes.hpp"

#define DEFAULT_PORT "3490"
#define FILENAME_LEN 26
#define IP_ADDR_STR_LEN 16
#define PORT_STR_LEN 6

struct ServerInfo{
    std::string server_addr;
    std::string server_port = DEFAULT_PORT;
};

int validate_and_set_args(struct ServerInfo* args, int argc, char* argv[]){
    if(argc<2){
        return 1;
    }

    int idx = 1;
    args->server_addr = argv[idx];

    if(argc>2){
        idx++;
        args->server_port = argv[idx];
    }

    return 0;
}

struct addrinfo* get_socket_fd(std::string IP_STR, std::string PORT_STR){
    
    struct addrinfo* ad_info;
    struct addrinfo hints;

    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    //Don't care if IPv4 (AF_INET) or IPV6(AF_INET6)
    hints.ai_socktype = SOCK_STREAM; //TCP stream sockets; SOCK_DGRAM for datagram
    hints.ai_flags = AI_PASSIVE;    //Fill in my ip for me

    int status = getaddrinfo( IP_STR.c_str(), PORT_STR.c_str(), &hints, &ad_info );
    if(status != 0){
        std::cout << "getaddrinfo() error; host may be offline" << std::endl;
        exit(1);
    }

    return ad_info;
}

void save_file(Message * buff){

    std::string filename = "server_file";

    std::cout << " ?> Which name should the file be saved under? ";
    std::cin >> filename;
    std::cout << "Saving file..." << std::endl;

    std::ofstream outfile(filename, std::ofstream::binary);
    std::cout << "Stop 1!" << std::endl;

    if(outfile.fail()){
        outfile.close();
        std::cout << "File could not be saved, name may be invalid." << std::endl;
        return;
    }

    unsigned char * msg = (unsigned char *)buff->get_msg();
    long idx = 0;
    std::cout << "Stop 2!" << std::endl;

    while(buff->size() != idx){
        unsigned char byte = msg[idx];

        outfile.put( byte );

        idx++;
    }
    std::cout << "Stop 3!" << std::endl;

    outfile.flush();
    outfile.close();

    std::cout << "File saved as " << filename << std::endl;
}

void write_server_msg(Message * buff){

    char c;
    long idx = 0;

    //std::cout << "----Message From Server----" << std::endl;
    std::cout << " << ";
    while(buff->size() != idx){
        c = buff->get(idx);
        std::cout << c;
        if(c=='\n'){
            std::cout << " << ";
        }
        idx++;
    }

    if(buff->get(idx - 1) != '\n'){
        std::cout << std::endl;
    }
    //std::cout << "-----Message Finished-----" << std::endl;
}

Message * get_command(){
    std::string cmd_str;

    std::cout << " >> ";
    std::cin >> cmd_str;
    
    return new Message(cmd_str, cmd_str.size());
}

int send_request(int sockfd, Message* req){

    int req_bytes = htonl(req->size());
    int bytes_to_send = req->size();
    
    
    int bytes_sent = send(sockfd, &req_bytes, 4, 0);
    if (bytes_sent == -1){
        return 1;
    }
    
    char * msg_start = req->get_msg();

    do{
        bytes_sent = send(sockfd, msg_start, bytes_to_send, 0);
        if(bytes_sent==-1){
            return 2;
        }

        msg_start += bytes_sent;
        bytes_to_send -= bytes_sent;

    }while(bytes_to_send > 0);

    return 0;
}

bool await_and_proccess_response(int sockfd){
    
    bool terminate = false;

    int bytes_received;
    int msg_size;

    bytes_received = recv(sockfd, &msg_size, 4, 0); //0 is the value passed to the *flags* argument
    if(bytes_received==-1){
        std::cout << "Error: could not receive message size!" << std::endl;
        return terminate;
    }

    msg_size = ntohl(msg_size) - 1; //Deal with status byte separately from rest of the message
    
    // if(msg_size == - 1){
    //     std::cout << "The server could not perform the desired action." << std::endl;
    //     return terminate;
    // }
    char* msg = (char*)malloc(msg_size); 

    //Receive the status byte
    char stat_byte;
    bytes_received = recv(sockfd, &stat_byte, 1, 0);
    if(bytes_received==-1){
        std::cout << "Error: could not receive response!" << std::endl;
        return terminate;
    }
    //-----------------------

    char* msg_ptr = msg;
    long bytes_to_rec = msg_size;
    do{
        bytes_received = recv(sockfd, msg_ptr, bytes_to_rec, 0); //0 is the value passed to the *flags* argument
        if(bytes_received==-1){
            std::cout << "Error: could not receive response!" << std::endl;
            return terminate;
        }
        bytes_to_rec -= bytes_received;
        msg_ptr += bytes_received;
    }while(bytes_to_rec>0);

    Message *rsp = new Message(msg, msg_size);


    char status = stat_byte & ServerCodes::CON_MASK;
    if( status != ServerCodes::CON_ON ){
        terminate = true;
    }

    status = stat_byte & ServerCodes::TYPE_MASK;
    switch(status){
        case ServerCodes::FILE :
            save_file(rsp);
            break;
        case ServerCodes::INFO :
            write_server_msg(rsp);
            break;
        default:
            std::cout << "Unrecognized server response code: " << (int)status << std::endl;
            terminate = true;
            break;
    }

    delete(rsp);
    return terminate;
}

int main(int argc, char* argv[]){

    struct ServerInfo args = ServerInfo();
    int status = validate_and_set_args(&args, argc, argv);

    if(status!=0){
        std::cout << "<!>Bad arguments/format<!>" << std::endl;
        std::cout << "Usage: \"<program_name> <server_addr> <port?>\"" << std::endl;
        exit(1);
    }

    std::cout << "----SERVER INFO----" << std::endl;
    std::cout << "Connected to Address: " << args.server_addr << std::endl;
    std::cout << "At Port: " << args.server_port << std::endl;
    std::cout << "-------------------" << std::endl;

    struct addrinfo * ad_info = get_socket_fd( args.server_addr, args.server_port );
    int sockfd = socket(ad_info->ai_family, ad_info->ai_socktype, ad_info->ai_protocol);
    status = connect(sockfd, ad_info->ai_addr, ad_info->ai_addrlen);

    if(status != 0){
        std::cout << "Cannot connect, host may not be ready to transmit." << std::endl;
        exit(1);
    }

    bool terminate;
    
    do{
        Message* usr_cmd = get_command();
        status = send_request(sockfd, usr_cmd);
        delete(usr_cmd);

        if(status != 0){
            std::cout << "Could not send request; status code " << status << std::endl;
            continue;
        }
        
        terminate = await_and_proccess_response(sockfd);
    }while(!terminate);

    std::cout << "Done!" << std::endl;

    #ifdef OS_WIN
        closesocket(sockfd);
    #else
        close(sockfd);
    #endif

    freeaddrinfo(ad_info);
    return 0;
    
}
