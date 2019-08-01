
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include "whatsappio.h"
#include <iostream>
#include <string>
#include <algorithm>

#define h_addr h_addr_list[0]
#define MAX_CLIENTS 30
#define ACK_MSG_SIZE 3
#define ACK_TRUE "tru"
#define ACK_FALSE "fal"
#define SERVER_DOWN "ext"

class wa_client{
public:
    wa_client(int i, std::string& n): id(i), name(n){};
    ~wa_client() = default;
    int id;
    std::string name;
    std::vector<std::string> groups;
};


class wa_group{
public:
    explicit wa_group(std::string& n): name(n){};
    ~wa_group() = default;
    std::string name;
    std::vector<std::string> users;
    std::vector<int> users_id;
};

int _handleWhoRequest(int client_ind);

std::vector<wa_client*> clients_vec;
std::vector<wa_group*> groups_vec;
int g_num_clients = 0;
fd_set g_clientsfds;


// Read command message from given socket.
int _serverRead(int source, char* buff, int buff_size){

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
            return (-1);
        }
    }
    return 0;
}


//Send response and notifications to given socket.
int _serverWrite(int soc, char* buff, int size){

    int bcount = 0;
    int br = 0;
    while(bcount < size){
        if((br = write(soc, buff, size-bcount)) > 0){
            bcount += br;
            buff += br;
        }
        if (br < 0){
            print_error("write", br);
            return (-1);
        }
    }
    return 0;
}


//Send acknowledgement response to client with indication true or false.
int sendAckToClient(bool msg, int client_id){
    char ack_msg[WA_MAX_INPUT + 1] = {0};
    if (msg){
        strncpy(ack_msg, ACK_TRUE, strlen(ACK_TRUE));
    } else{
        strncpy(ack_msg, ACK_FALSE, strlen(ACK_FALSE));
    }
    _serverWrite(client_id, ack_msg, WA_MAX_INPUT +1);
    return 0;
}


/**
 * Checks if name already exists
 * @param name The name that we want to check
 * @return true if the name is valid (i.e it dosn't exist), false otherwise
 */
bool nameCheck(std::string name){
    for (const auto &client : clients_vec) {
        if (client->name == name){
            return false;
        }
    }
    if (groups_vec.empty()){
        return true;
    }
    for (const auto &group : groups_vec) {
        if (group->name == name){
            return false;
        }
    }
    return true;
}


//Add new client to server and print connected message.
int _addNewClient(int server){

    char buffer[WA_MAX_NAME+1] = {0};
    int c = 0;

    c = accept(server, NULL, NULL);
    if(c < 0){
        print_error("accept", c);
        return -1;
    }
    // get client name
    if(_serverRead(c, buffer, (WA_MAX_NAME+1)) < 0){
        return -1;
    }
    if (!nameCheck(buffer)){
        sendAckToClient(false, c);
        return -1;
    }
    std::string name(buffer);
    wa_client* cl = new wa_client(c, name);
    print_connection_server(name);
    sendAckToClient(true, c);

    clients_vec.push_back(cl);
    FD_SET(c, &g_clientsfds);
    g_num_clients ++;
    return 0;
}


/**
 * Handles the members of a newly created group
 * @param members list of group members, as supplied from parse_command
 * @param group_name The new group name
 * @param new_members A pointer to an empty vector that will contain reduced 'members', in case of duplicated names
 * @param members_id A pointer to an empty vector that will contain the group members index at client_vec
 * @param caller_id Index of function caller in client_vec
 * @return True if the members are valid, false otherwise.
 */
bool membersHandler(std::vector<std::string> members, std::string group_name, std::vector<std::string>& new_members,
                    std::vector<int>& members_id, int caller_id) {
    new_members.push_back(clients_vec.at(caller_id)->name);
    members_id.push_back(clients_vec.at(caller_id)->id);
    int members_counter = 1; // Group creator is part of the group
    if (members.empty()){
        return false;
    }
    for (const auto &member: members){
        bool member_exist = false;
        if (clients_vec.empty()){
            break;
        }
        for (const auto &client : clients_vec) {
            if (client->name == member){
                member_exist = true;
                bool member_in_group = false;
                for (const auto &new_member : new_members){
                    if (new_member == member){
                        member_in_group = true;
                        break;
                    }
                }
                if (!member_in_group){
                    new_members.push_back(member);
                    members_id.push_back(client->id);
                    members_counter++;
                }
                break;
            }
        }
        if (!member_exist){
            return false;
        }
    }
    if (members_counter == 1){
        return false;
    }
    return true;
}


/**
 * Tries to create a new group with included name and members
 * @param name The name of the group
 * @param members The group members
 * @param caller_id Index of function caller in client_vec
 * @return 0 if group was created, -1 otherwise.
 */
bool newGroup(std::string name, std::vector<std::string> members, int caller_id){
    bool valid_name = nameCheck(name);
    int cl_soc = clients_vec.at(caller_id)->id;
    if (!valid_name){
        sendAckToClient(false, cl_soc);
        return false;
    }
    std::vector<std::string> new_members;
    new_members.clear();
    std::vector<int> members_id;
    bool valid_members = membersHandler(members, name, new_members, members_id, caller_id);
    if (!valid_members){
        sendAckToClient(false, cl_soc);
        return false;
    }
    wa_group* new_group = new wa_group(name);
    new_group->users = new_members;
    new_group->users_id = members_id;
    groups_vec.push_back(new_group);
    sendAckToClient(true, cl_soc);
    return true;
}


//Delete client from server.
int _unregisterClient(int ind){

    wa_client* cl = clients_vec.at(ind);
    int cl_soc = cl->id;
    std::string cl_name = cl->name;
    //delete client from groups
    std::vector<wa_group*> temp_groups(groups_vec);
    int j = 0;
    for (const auto &gr : temp_groups){
        int to_del = -1;
        int i = 0;
        for(const auto &name : gr->users){
            if (cl_name == name){
                to_del = i;
            }
            i++;
        }
        if(to_del > -1){
            gr->users.erase(gr->users.begin() + to_del);
            gr->users_id.erase(gr->users_id.begin() + to_del);
            if(gr->users.empty()){
                delete groups_vec.at(j);
                groups_vec.erase(groups_vec.begin() + j);
                continue;
            }
        }
        j++;
    }
    sendAckToClient(true, cl_soc);
    FD_CLR(cl_soc, &g_clientsfds);
    delete clients_vec.at(ind);
    clients_vec.erase(clients_vec.begin() + ind);
    g_num_clients--;
    print_exit(true, cl_name);
    return 0;
}


//Create and send new message from given client to another client/s.
bool newMessage(std::string name, std::string message, int caller_ind){

    std::string name_message = clients_vec.at(caller_ind)->name + ": " + message;
    char msg[WA_MAX_INPUT + 1] = {0};
    strncpy(msg, name_message.c_str(), name_message.length());

    for (const auto &client : clients_vec) {
        if (client->name == name){
            //write ack and then write the msg to the dest client.
            sendAckToClient(true, client->id);
            _serverWrite(client->id, msg, WA_MAX_INPUT +1);

            //send ack with true indication to the sender.
            sendAckToClient(true, clients_vec.at(caller_ind)->id);
            return true;
        }
    }
    for (const auto &group : groups_vec) {
        if (group->name == name){
            bool caller_in_group = false;
            for (const auto &member_id : group->users_id){

                if (member_id == clients_vec.at(caller_ind)->id){
                    caller_in_group = true;
                    break;
                }
            }
            if (!caller_in_group){
                sendAckToClient(false, clients_vec.at(caller_ind)->id);
                return false;
            }
            for(const auto &id : group->users_id){

                if (id == clients_vec.at(caller_ind)->id){
                    continue;
                }
                sendAckToClient(true, id);
                _serverWrite(id, msg, WA_MAX_INPUT+1);
            }
            //write ack with true indiaction to sender
            sendAckToClient(true, clients_vec.at(caller_ind)->id);
            return true;
        }
    }
    sendAckToClient(false, clients_vec.at(caller_ind)->id);
    return false;
}


//Get command from specific client. (set del value if it is exit command)
int _handleClientCommand(int client, int ind, int& del){
    command_type commandType;
    std::string name;
    std::string message;
    std::vector<std::string> clients;

    char buffer [WA_MAX_INPUT+1] = {0};
    _serverRead(client, buffer, WA_MAX_INPUT+1);

    std::string buf = buffer;
    parse_command(buf, commandType, name, message, clients);
    if (commandType == CREATE_GROUP){
        bool group_succecced = newGroup(name, clients, ind);
        print_create_group(true, group_succecced, clients_vec.at(ind)->name, name);
        return 0;
    }

    if (commandType == SEND){
        bool message_succecced = newMessage(name, message, ind);
        print_send(true, message_succecced, clients_vec.at(ind)->name, name, message);
    }
    if (commandType == WHO){
        _handleWhoRequest(ind);
    }

    if (commandType == EXIT){
        _unregisterClient(ind);
        del = 1;
    }

    return 0;
}


//Get command from the server.
int _handleServerInput(int server){

    char s_msg[WA_MAX_INPUT + 1] = {0};
    read(STDIN_FILENO, s_msg, WA_MAX_INPUT+1);
    if (strcmp(s_msg, "EXIT\n") == 0){
        char ntf_msg[WA_MAX_INPUT+1] = {0};
        strncpy(ntf_msg, SERVER_DOWN, strlen(SERVER_DOWN));
        //clean mem.
        for (const auto &cl : clients_vec){
            _serverWrite(cl->id, ntf_msg, WA_MAX_INPUT+1);
            delete (cl);
        }
        for (const auto &gr : groups_vec){
            delete (gr);
        }
        clients_vec.clear();
        groups_vec.clear();
        close(server);
        print_exit();
        exit(0);
    }
    return 0;
}


//Write to caller client the list of all connected clients.
int _handleWhoRequest(int client_ind){

    int cl_soc = clients_vec.at(client_ind)->id;
    std::vector<std::string> names_vec;
    for (const auto& cl : clients_vec){
        names_vec.push_back(cl->name);
    }
    std::sort(names_vec.begin(), names_vec.end());
    std::string all_names;
    for (const auto& n : names_vec){
        all_names += "," + n;
    }
    all_names.erase(all_names.begin());

    char who_msg[WA_MAX_INPUT+1] = {0};
    strncpy(who_msg, all_names.c_str(), all_names.length());
    _serverWrite(cl_soc, who_msg, WA_MAX_INPUT+1);

    print_who_server(clients_vec.at(client_ind)->name);
    return 0;
}


//Open socket server and return the server descriptor.
int _openServerConnection(int portnum){
    int server = 0;
    int ret_val = 0;
    char myname[WA_MAX_NAME+1];

    struct sockaddr_in sa;
    struct hostent *hp;
    //hostnet initialization
    gethostname(myname, WA_MAX_NAME);
    hp = gethostbyname(myname);
    if (hp == NULL){
        print_error("gethostbyname", -1);
        exit(1);
    }
    //sockaddrr_in initlization
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = hp->h_addrtype;
    /* this is our host address */
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    /* this is our port number */
    sa.sin_port= htons((unsigned short)portnum);
    /* create socket */
    if ((server= socket(AF_INET, SOCK_STREAM, 0)) < 0){
        print_error("socket", server);
        exit(1);
    }
    if ((ret_val = bind(server , (struct sockaddr *)&sa , sizeof(struct sockaddr_in))) < 0) {
        print_error("bind", ret_val);
        close(server);
        exit(1);
    }
    if ((ret_val = listen(server, 10)) < 0) {/* max 10 of queued connects */
        print_error("listen", ret_val);
        close(server);
        exit(1);
    }
    return server;
}


int main(int argc, char *argv[]){

    if(argc != 2){
        print_server_usage();
        exit(1);
    }
    std::string s1 = argv[1];
    std::string::size_type sz;
    int portnum = std::stoi(s1,&sz);

    int ret_val = 0;
    fd_set readfds;

    int server = _openServerConnection(portnum);

    FD_ZERO(&g_clientsfds);
    FD_SET(server, &g_clientsfds);
    FD_SET(STDIN_FILENO, &g_clientsfds);

    while(true){
        FD_ZERO(&readfds);
        readfds = g_clientsfds;
        //wait for an activity on one of the sockets
        if((ret_val = select(MAX_CLIENTS + 1 , &readfds , NULL , NULL , NULL)) < 0){
            print_error("select", ret_val);
        }
        //check for an incoming connection
        if (FD_ISSET(server, &readfds)){
            _addNewClient(server);
        }
        //handle server input.
        if(FD_ISSET(STDIN_FILENO, &readfds)){
            _handleServerInput(server);
        }
        //checks some IO operation on other socket
        int index = 0;
        std::vector<wa_client*> temp_clients(clients_vec);
        for (const auto &cl : temp_clients){
            if (FD_ISSET(cl->id, &readfds)){
                //read parsing and execute client request.
                int to_del = -1;
                _handleClientCommand(cl->id, index, to_del);
                if(to_del == 1){
                    continue;
                }
            }
            index ++;
        }
        temp_clients.clear();
    }
    return 0;
}








