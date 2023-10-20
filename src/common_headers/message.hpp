class Message{

protected:
    unsigned int bytes;
    char * msg;
    bool msg_alloced = true;

public:

    Message(char * buff, unsigned int len){
        bytes = len;
        msg = buff;
    }

    Message(std::string buff) {
        bytes = buff.size();

        msg = (char*)malloc(bytes);
        strcpy(msg, buff.c_str());

    }

    ~Message(){
        if(msg_alloced){
            free(msg);
        }
    }

    unsigned int size(){
        return bytes;
    }

    char get(unsigned int idx){
        if(idx<bytes){
            return msg[idx];
        }

        return 0;
    }

    char * get_msg(){
        return msg;
    }
};
