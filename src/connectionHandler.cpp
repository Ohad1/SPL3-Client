#include <connectionHandler.h>
#include <hwloc.h>
#include <fstream>
#include <sstream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <iostream>
using namespace std;

using boost::asio::ip::tcp;

using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::to_string;
 
ConnectionHandler::ConnectionHandler(string host, short port): host_(host), port_(port), io_service_(),
socket_(io_service_), terminate(0){}
    
ConnectionHandler::~ConnectionHandler() {
    close();
}
 
bool ConnectionHandler::connect() {
    std::cout << "Starting connect to " 
        << host_ << ":" << port_ << std::endl;
    try {
        tcp::endpoint endpoint(boost::asio::ip::address::from_string(host_), port_); // the server endpoint
		boost::system::error_code error;
		socket_.connect(endpoint, error);
        std::cout << error << std::endl;
		if (error)
			throw boost::system::system_error(error);

    }
    catch (std::exception& e) {
        std::cerr << "Connection failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}
 
bool ConnectionHandler::getBytes(char bytes[], unsigned int bytesToRead) {
    size_t tmp = 0;
	boost::system::error_code error;
    try {
        while (!error && bytesToRead > tmp ) {
			tmp += socket_.read_some(boost::asio::buffer(bytes+tmp, bytesToRead-tmp), error);
        }
		if(error)
			throw boost::system::system_error(error);
    } catch (std::exception& e) {
        std::cerr << "recv failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

bool ConnectionHandler::sendBytes(const char bytes[], int bytesToWrite) {
    int tmp = 0;
	boost::system::error_code error;
    try {
        while (!error && bytesToWrite > tmp ) {
			tmp += socket_.write_some(boost::asio::buffer(bytes + tmp, bytesToWrite - tmp), error);
        }
		if(error)
			throw boost::system::system_error(error);
    } catch (std::exception& e) {
        std::cerr << "recv failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

bool ConnectionHandler::getLine(std::string& line) {
    return getFrameAscii(line, '\n');
}

bool ConnectionHandler::sendLine(std::string& line) {
    return sendFrameAscii(line, '\n');
}
 
bool ConnectionHandler::getFrameAscii(std::string& frame, char delimiter) {
    char ch;
    char serverToClientOpcodeArray[2];
    char messageOpcodeArray[2];
    short serverToClientOpcode;
    short messageOpcode;
    try {
        getBytes(&ch, 1);
        serverToClientOpcodeArray[0] = ch;
        getBytes(&ch, 1);
        serverToClientOpcodeArray[1] = ch;
        serverToClientOpcode = bytesToShort(serverToClientOpcodeArray);
        if (serverToClientOpcode==9) {
            frame.append("NOTIFICATION ");
            getBytes(&ch, 1);
            if (ch=='\0') {
                frame.append("PM ");
            }
            else {
                frame.append("Public ");
            }
            while (1) {
                getBytes(&ch, 1);
                if (ch=='\0') {
                    break;
                }
                frame.append(1, ch);
            }
            frame.append(" ");
            while (1) {
                getBytes(&ch, 1);
                if (ch=='\0') {
                    break;
                }
                frame.append(1, ch);
            }
            return true;
        }
        else {
            getBytes(&ch, 1);
            messageOpcodeArray[0] = ch;
            getBytes(&ch, 1);
            messageOpcodeArray[1] = ch;
            messageOpcode = bytesToShort(messageOpcodeArray);
            if (serverToClientOpcode==11) {
                frame.append("ERROR " + std::to_string(messageOpcode));
                if (messageOpcode == 3) {
                    terminate = -1;
                }
                return true;
            }
            else if (serverToClientOpcode==10) {
                if (messageOpcode == 3) {
                    frame.append("ACK " + std::to_string(messageOpcode));
                    terminate = 1;
                    close();
                    return true;
                }
                else if (messageOpcode == 4 | messageOpcode == 7) {
                    char numOfUsersArray[2];
                    getBytes(&ch, 1);
                    numOfUsersArray[0] = ch;
                    getBytes(&ch, 1);
                    numOfUsersArray[1] = ch;
                    short numOfUsers = bytesToShort(numOfUsersArray);
                    frame.append("ACK " + std::to_string(messageOpcode) + " " + std::to_string(numOfUsers) + " ");
                    for (int i = 0; i < numOfUsers; i++) {
                        while (1) {
                            getBytes(&ch, 1);
                            if (ch=='\0') {
                                break;
                            }
                            frame.append(1, ch);
                        }
                        if (i!=numOfUsers-1) {
                            frame.append(" ");
                        }
                    }
                    return true;
                }
                else if (messageOpcode==8) {
                    char numOfPostsArray[2];
                    getBytes(&ch, 1);
                    numOfPostsArray[0] = ch;
                    getBytes(&ch, 1);
                    numOfPostsArray[1] = ch;
                    short numOfPosts = bytesToShort(numOfPostsArray);
                    char numOfFollowersArray[2];
                    getBytes(&ch, 1);
                    numOfFollowersArray[0] = ch;
                    getBytes(&ch, 1);
                    numOfFollowersArray[1] = ch;
                    short numOfFollowers = bytesToShort(numOfFollowersArray);
                    char numOfFollowingsArray[2];
                    getBytes(&ch, 1);
                    numOfFollowingsArray[0] = ch;
                    getBytes(&ch, 1);
                    numOfFollowingsArray[1] = ch;
                    short numOfFollowing = bytesToShort(numOfFollowingsArray);
                    frame.append("ACK " +
                    std::to_string(8) + " " +
                    std::to_string(numOfPosts) + " " +
                    std::to_string(numOfFollowers) + " " +
                    std::to_string(numOfFollowing));
                    return true;
                }
                else {
                    frame.append("ACK " + std::to_string(messageOpcode));
                    return true;
                }
            }
        }
        return false;
    }
    catch (std::exception& e) {
        std::cerr << "recv failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
}
 
bool ConnectionHandler::sendFrameAscii(const std::string& frame, char delimiter) {
	vector<char> line_after_encode = encode(frame);
    int size = line_after_encode.size();
    char output[size];
    for (int k = 0; k < size; k++) {
        output[k] = line_after_encode[k];
    }
    short result;
    return sendBytes(output,size);
}
 
// Close down the connection properly.
void ConnectionHandler::close() {
    try{
        socket_.close();
    } catch (...) {
        std::cout << "closing failed: connection already closed" << std::endl;
    }
}

int ConnectionHandler::getTerminate() const {
    return terminate;
}

void ConnectionHandler::setTerminate(int terminate) {
    ConnectionHandler::terminate = terminate;
}

short ConnectionHandler::bytesToShort(char* bytesArr)
{
    short result = (short)((bytesArr[0] & 0xff) << 8);
    result += (short)(bytesArr[1] & 0xff);
    return result;
}

void ConnectionHandler::shortToBytes(short num, char* bytesArr)
{
    bytesArr[0] = ((num >> 8) & 0xFF);
    bytesArr[1] = (num & 0xFF);
}


vector<char> ConnectionHandler::encode(std::string msg) {
    vector<char> line_after_encode;
    istringstream iss(msg);
    string part;
    int i = 0;
    while (getline(iss, part, ' ') && i == 0) // the name of the action is in name_action
    {
        if (part.compare("REGISTER") == 0) {
            line_after_encode =  buildRegister(msg);
        }

        if (part.compare("LOGIN") == 0) {
            line_after_encode =  buildLogin(msg);
        }

        if (part.compare("LOGOUT") == 0) {
            line_after_encode =  buildLogout(msg);
        }
        if (part.compare("FOLLOW") == 0) {
            line_after_encode =  buildFollow(msg);
        }
        if (part.compare("POST") == 0) {
            line_after_encode =  buildPost(msg);
        }
        if (part.compare("PM") == 0) {
            line_after_encode =  buildPM(msg);
        }
        if (part.compare("USERLIST") == 0) {
            line_after_encode =  buildUserList(msg);
        }
        if (part.compare("STAT") == 0) {
            line_after_encode =  buildSTAT(msg);

        }
        i++;
    }
    return line_after_encode;

}

std::vector<char> ConnectionHandler::buildLogin(std::string msg) {
    istringstream iss(msg);
    string part;
    int i = 0;
    char opcode[2];
    short  j=2;
    shortToBytes(j,opcode);
    std::vector<char> charVector;
    charVector.push_back(opcode[0]);
    charVector.push_back(opcode[1]);
    while (getline(iss, part, ' ') && i <= 3) // the name of the action is in name_action
    {
        if (i == 1 | i == 2) {
            for (int k = 0; k < part.length(); k++) {
                charVector.push_back(part[k]);
            }
            charVector.push_back('\0');
        }
        i++;
    }
    return charVector;
}

std::vector<char> ConnectionHandler::buildRegister(std::string msg) {
    istringstream iss(msg);
    string part;
    int i = 0;
    char opcode[2];
    short  j=1;
    shortToBytes(j,opcode);
    std::vector<char> charVector;
    charVector.push_back(opcode[0]);
    charVector.push_back(opcode[1]);
    while (getline(iss, part, ' ') && i <= 3) // the name of the action is in name_action
    {
        if (i == 1 | i == 2) {
            for (int k = 0; k < part.length(); k++) {
                charVector.push_back(part[k]);
            }
            charVector.push_back('\0');
        }
        i++;
    }
    return charVector;
}

std::vector<char> ConnectionHandler::buildLogout(std::string msg) {
    char opcode[2];
    shortToBytes(3,opcode);
    std::vector<char> output;
    output.push_back(opcode[0]);
    output.push_back(opcode[1]);
    return  output;
}

std::vector<char> ConnectionHandler::buildFollow(std::string msg) {
    istringstream iss(msg);
    string part;
    int i = 0;
    char opcode[2];
    shortToBytes(4,opcode);
    std::vector<char> output;
    output.push_back(opcode[0]);
    output.push_back(opcode[1]);
    while (getline(iss, part, ' ')) // the name of the action is in name_action
    {
        if (i == 1) { //type_follow
            if (part == "0") {
                output.push_back('\0');
            }
            else {
                output.push_back('\1');
            }
        }
        if (i == 2) { // num of followers
            char num[2];
            short myShort = boost::lexical_cast<short>(part);
            shortToBytes(myShort, num);
            output.push_back(num[0]);
            output.push_back(num[1]);
        }

        if(i>=3)// user list names
        {
            for(int j=0;j<part.length();j++)
            {
                output.push_back(part[j]);
            }
            output.push_back('\0');
        }
        i++;
    }
    return output;
}

std::vector<char> ConnectionHandler::buildPost(std::string msg) {
    istringstream iss(msg);
    string part;
    int i = 0;
    string content;
    char opcode[2];
    shortToBytes(5,opcode);
    std::vector<char> output;
    output.push_back(opcode[0]);
    output.push_back(opcode[1]);

    while (getline(iss, part, ' ')) // the name of the action is in name_action
    {
        if (i >=1) {
            for(int j=0;j<part.length();j++)
            {
                output.push_back(part[j]);
            }
            output.push_back(' ');
        }
        i++;
    }
   output.pop_back();
   output.push_back('\0');
   return output;
}

std::vector<char>  ConnectionHandler::buildPM(std::string msg) {
    istringstream iss(msg);
    string part;
    int i = 0;
    char opcode[2];
    shortToBytes(6,opcode);
    std::vector<char> output;
    output.push_back(opcode[0]);
    output.push_back(opcode[1]);
    while (getline(iss, part, ' ')) // the name of the action is in name_action
    {
        if (i==1) { //username
            for(int j=0;j<part.length();j++)
            {
                output.push_back(part[j]);
            }
            output.push_back('\0');
        }
        if(i>=2){
            for(int j=0;j<part.length();j++)
            {
                output.push_back(part[j]);
            }
            output.push_back(' ');
        }
        i++;
    }
    output.pop_back();
    output.push_back('\0');
    return output;
}

std::vector<char> ConnectionHandler::buildUserList(std::string msg) {
    char opcode[2];
    short j = 7;
    shortToBytes(j,opcode);
    std::vector<char> output;
    output.push_back(opcode[0]);
    output.push_back(opcode[1]);
    return  output;
}

std::vector<char> ConnectionHandler::buildSTAT(std::string msg) {
    istringstream iss(msg);
    string part;
    int i = 0;
    string username;
    char opcode[2];
    shortToBytes(8,opcode);
    std::vector<char> output;
    output.push_back(opcode[0]);
    output.push_back(opcode[1]);
    while (getline(iss, part, ' ') && i <2) // the name of the action is in name_action
    {
        if (i == 1) {
            for(int j=0;j<part.length();j++)
            {
                output.push_back(part[j]);
            }
            output.push_back('\0');
        }
        i++;
    }
    return output;

}

