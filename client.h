#pragma once
#include<sys/socket.h>
#include<sys/types.h>       //pthread_t , pthread_attr_t and so on.
#include<iostream>
#include<cstdio>
#include<netinet/in.h>      //structure sockaddr_in
#include<arpa/inet.h>       //Func : htonl; htons; ntohl; ntohs
#include<cstring>          //Func :memset
#include<unistd.h>          //Func :close,write,read
#include"camera.h"
#include <unistd.h>
#include <sys/time.h>
#include "interface/vcos/vcos.h"

#define SOCK_PORT 44960
#define BUFFER_LENGTH 2048
#define IP_ADDRESS "192.168.1.100"

using namespace std;

class ClientSocket
{
private:
    int status;
    struct sockaddr_in serverAddr;
	CAMERA camera;

public:
	int serverSocket;
	bool sendMessage(const char* command);
	void parseCommad(const char* command);
	static void* recieveMessage(void* socket);
	bool initSocket();
	bool sendFile(string name);
	void savePics(string& name);
	void saveVideo(int bufferCount);
	void changeFPS(const int fps, const int frame); 
	void changeShutterSpeed(const int speed); 
};
