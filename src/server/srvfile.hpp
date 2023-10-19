#include "../common_headers/message.hpp"
#include <fstream>
#include <iostream>

class ServerFile : public Message{

private:
    short error = 0;

    void set_fsize(std::string file_name){
        std::ifstream file(file_name, std::ios::binary);

        if( file.fail()){
            error = 1;
            return;
        }

        char byte;
        while( !file.eof() ){
            file.get(byte);

            if( file.bad() ){
                error = 2;
                file.close();
                return;
            }

            bytes++;
        }
        
        file.close();
    }
    
public:
    ServerFile(std::string file_name, char stat_byte) : Message( (char *)nullptr, 0 ){
        msg_alloced = false;

        set_fsize(file_name);

        if(error) return;

        std::ifstream file(file_name, std::ios::binary);

        if( file.bad()){
            error = 3;
            file.close();
            return;
        }

        //Extra space for the stat byte at the start
        msg = (char*)malloc(bytes + 1);
        msg[0] = stat_byte;
        char* bytes_ptr = msg + 1;

        while (!file.eof()){
            char byte;

            file.get(byte);

            if (file.bad()){
                error = 4;
                free(msg);
                break;
            }

            *bytes_ptr = byte;
            bytes_ptr++;
        }

        file.close();
        msg_alloced = true;
    }

    ~ServerFile(){

    }

    short errcode(){
        return error;
    }

    bool failed(){
        return error;
    }

};