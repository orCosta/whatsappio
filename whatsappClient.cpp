
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include "whatsappio.h"
#include <iostream>
#include <string>

#define h_addr h_addr_list[0]
#define ACK_MSG_SIZE 3

#define ACK_TRUE "tru"
#define ACK_FALSE "fal"
#define SERVER_DOWN "ext"

int g_clientId; //socket descriptor
std::string g_clientName;

//Read response and notifications from given source.
void _clientRead(int source, char* buff, int buff_size){

    int bcount = 0;
    int br = 0;
    while (bcount < buff_size){
        br = read(source, buff, (buff_size)-bcount);
        if(br > 0){
            bcount += br;
            buff += br;
        }
        if (br < 1){
            print_error("read", br);
            exit(1);
        }
    }
}

//Write commands to server.
void _clientWrite(int soc, char* buff, int size){
    int bcount = 0;
    int br = 0;
    while(bcount < size){
        if((br = write(soc, buff, size-bcount)) > 0){
            bcount += br;
            buff += br;
        }
        if (br < 0){
            print_error("write", br);
            exit(1);
        }
    }
}


//Handle server notification - server down or message from other client.
int _handleServerNotification(){

    char ntf[WA_MAX_INPUT+1] ={0};
    _clientRead(g_clientId, ntf, WA_MAX_INPUT+1);

    if(strcmp(ntf, SERVER_DOWN) == 0){
        close(g_clientId);
        exit(1);
    }

    if(strcmp(ntf, ACK_TRUE) == 0){
        char msg[WA_MAX_INPUT+1] = {0};
        _clientRead(g_clientId, msg, WA_MAX_INPUT+1);
        std::cout << msg << "\n";
    }
    return 0;
}


//check the name contains letters and digits only.
int validName(std::string& name){
    for (char& c:name) {
        if(std::isalnum(c) == 0){
            return -1;
        }
    }
    return 0;
}


//Read acknowledgement response from server.
bool _readAckResponse(){
    char ack_res[WA_MAX_INPUT+1] = {0};
    _clientRead(g_clientId, ack_res, WA_MAX_INPUT + 1);
    if(strcmp(ack_res, ACK_TRUE) == 0){
        return true;
    } else {
        return false;
    }
}


//Verify the command.
int _verifyCommand(command_type& commandT, std::string& name, std::string& message, std::vector<std::string>& clients){

    if(commandT == INVALID){
        print_invalid_input();
        return 0;
    }
    if (commandT == SEND && name == g_clientName){
        print_invalid_input();
        return 0;
    }
    if (commandT == CREATE_GROUP){
        int num_mem = 0;
        if (validName(name) !=0 ){
            print_invalid_input();
            return 0;
        }
        for (const auto &cl : clients){
            if (cl != g_clientName){
                num_mem++;
            }
            if (cl == name){
                print_invalid_input();
                return 0;
            }
        }
        if (num_mem < 1){
            print_invalid_input();
            return 0;
        }

    }
    return 1;
}


//Handle stdin command and send it to server.
int handle_command(std::string& cmd){
    command_type commandType;
    std::string name;
    std::string message;
    std::vector<std::string> clients;
    parse_command(cmd, commandType, name, message, clients);
    if (_verifyCommand(commandType, name, message, clients) == 0){
        return 0;
    }
    char command[WA_MAX_INPUT+1] = {0};
    strncpy(command, cmd.c_str(), cmd.length());
    if (commandType == EXIT){
        bool res;
        _clientWrite(g_clientId, command, WA_MAX_INPUT+1);
        res = _readAckResponse();
        if (res){
            print_exit(false, "");
            close(g_clientId);
            exit(1);
        }
    }

    if (commandType == WHO){
        char res[WA_MAX_INPUT+1] = {0};
        _clientWrite(g_clientId, command, WA_MAX_INPUT+1);
        _clientRead(g_clientId, res, WA_MAX_INPUT+1);
        std::cout << res << "\n";
    }

    if (commandType == SEND){
        _clientWrite(g_clientId, command, WA_MAX_INPUT + 1);
        bool res = _readAckResponse();
        print_send(false, res, g_clientName, name, message);
    }

    if (commandType == CREATE_GROUP){
        _clientWrite(g_clientId, command, WA_MAX_INPUT + 1);
        bool res = _readAckResponse();
        print_create_group(false, res, g_clientName, name);
    }

    return 0;
}


//Open new socket and connect to server
void _openConnection(char* server_ip, int portnum, char* name){

    struct sockaddr_in server_addr;
    //sockaddrr_in initlization
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    /* this is our host address */
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    /* this is our port number */
    server_addr.sin_port= htons((unsigned short)portnum);
    /* create socket */
    if ((g_clientId= socket(AF_INET, SOCK_STREAM, 0)) < 0){
        print_error("socket", g_clientId);
        exit(1);
    }
    if (connect(g_clientId, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        print_fail_connection();
        exit(1);
    }
    char ack_msg[WA_MAX_INPUT+1] = {0};
    _clientWrite(g_clientId, name, WA_MAX_NAME+1);
    _clientRead(g_clientId, ack_msg, WA_MAX_INPUT +1);

    if(strcmp(ack_msg, ACK_TRUE) == 0){
        print_connection();
    }
    else if(strcmp(ack_msg, ACK_FALSE) == 0){
        print_dup_connection();
        close(g_clientId);
        exit(1);
    }
}


int main(int argc, char *argv[]){

    if ((argc != 4) || (sizeof(argv[1]) > WA_MAX_NAME)){
        print_client_usage();
        exit(1);
    }
    char* server_ip = argv[2];
    std::string s3 = argv[3];
    std::string::size_type sz;
    int portnum = std::stoi(s3,&sz);
    int ret_val = 0;

    char name[WA_MAX_NAME + 1] = {0};
    strcpy(name, argv[1]);
    g_clientName = (name);
    int valid_name = validName(g_clientName);
    if (valid_name != 0){
        print_invalid_input();
        exit(1);
    }

    _openConnection(server_ip, portnum, name);
    fd_set serverfds;
    fd_set readfds;
    FD_ZERO(&serverfds);
    FD_SET(g_clientId, &serverfds);
    FD_SET(STDIN_FILENO, &serverfds);

    while(true){
        FD_ZERO(&readfds);
        readfds = serverfds;
        //wait for an activity on one of the sockets
        if((ret_val = select(5, &readfds , NULL , NULL , NULL)) < 0){
            print_error("select", ret_val);
        }

        //handle local input.
        if(FD_ISSET(STDIN_FILENO, &readfds)){
            std::string cmd;
            getline(std::cin, cmd);
            handle_command(cmd);
        }
        //handle server response and notifications
        if (FD_ISSET(g_clientId, &readfds)){
            _handleServerNotification();
        }
    }
    return 0;
}
