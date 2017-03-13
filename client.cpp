#include "client.h"
#define CommandStart					0
#define CommandSavePicture        1
#define CommandSaveVideo          2
#define CommandSynchronize       3
#define CommandTransferFile       4
#define CommandRunCameraWithPreview       5
#define CommandRunCameraWithOUTPreview       6
#define CommandChangeFPS       7
#define CommandChangeShutterSpeed       8
#define CommandStop       9

bool ClientSocket ::  initSocket()
{
	camera = new CAMERA();
	if(!camera->initCamera())
	{
		cout << "init camera failed!" << endl;
		return false;
	}
	serverSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);       //ipv4,TCP
    if(serverSocket == -1)
    {
        cout << "socket error!" << endl;
        return false;
    }

    memset(&serverAddr,0,sizeof(serverAddr));
    serverAddr.sin_addr.s_addr = inet_addr(IP_ADDRESS);      //trans char * to in_addr_t
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SOCK_PORT);

    status = connect(serverSocket, (struct sockaddr *)(&serverAddr), sizeof(serverAddr));
    if(status == -1)
    {
        cout << "Connect error!" << endl;
        return false;
    }
	return true;
}

void* ClientSocket :: recieveMessage(void* socket)
{
	ClientSocket clientSocket = *((ClientSocket *)socket);
	int recieveLen;
	char buffer[BUFFER_LENGTH];

	while(1)
    {
        memset(buffer, 0, BUFFER_LENGTH);
        recieveLen = read(clientSocket.serverSocket, buffer, BUFFER_LENGTH);
        if(recieveLen <= 0) 
        {
			cout << "recieve error" << endl;
            break;
        }
		clientSocket.parseCommad(buffer);
    }
}

bool ClientSocket :: sendMessage(const char* command) {
	int commadLen;  
	commadLen = write(serverSocket, command, BUFFER_LENGTH);  
	if (commadLen == -1) {  
		cout << "Connection terminated" << endl;  
		return false;  
	}  
	return true;
}	

bool ClientSocket :: sendFile(string name) {
	char buffer[BUFFER_LENGTH];  
	int sendLen;
	FILE* fp = fopen(name.c_str(), "r");  
    if (fp == NULL) {
		cout << "open file error" << endl;
		return false;  
    }
	while( (sendLen = fread(buffer, sizeof(char), BUFFER_LENGTH, fp)) > 0)  
    {  
        if (write(serverSocket, buffer, sendLen) < 0)  
        {  
            cout << "Send File Failed!" << endl;  
			return false;  
        }  
       bzero(buffer, sizeof(buffer));  
    }  
    fclose(fp);  
    cout << "File Transfer Finished!" << endl;  
	return true;
}

void ClientSocket :: saveVideo(int bufferCount) {
	cout<<"saving video"<<endl;
	CAMERA :: setupSaveVideo(bufferCount);
}

void ClientSocket :: savePics() {
	cout<<"saving pic"<<endl;
	camera->setupSavePic();
}

void ClientSocket :: changeFPS(const int fps, const int frame) {
	CAMERA :: FPS = fps;
	CAMERA :: changeFPSCount = frame;
}

void ClientSocket :: changeShutterSpeed(const int speed) {
	camera->changeShutterSpeed(speed);
}

void ClientSocket :: parseCommad(const char* command) {
	int commandNum = -1;
	if(command[0] == 's')
		commandNum = CommandStart;
	else 	if(command[0] == 'p')
		commandNum = CommandSavePicture;
	else 	if(command[0] == 'v')
		commandNum = CommandSaveVideo;
	else 	if(command[0] == 'y')
		commandNum = CommandSynchronize;
	else 	if(command[0] == 'f')
		commandNum = CommandTransferFile;
	else 	if(command[0] == 'r')
		commandNum = CommandRunCameraWithPreview;
	else 	if(command[0] == 'u')
		commandNum = CommandRunCameraWithOUTPreview;
	else 	if(command[0] == 'c')
		commandNum = CommandChangeFPS;
	else 	if(command[0] == 'h')
		commandNum = CommandChangeShutterSpeed;
	else 	if(command[0] == 't')
		commandNum = CommandStop;

	switch(commandNum)
	{
		case CommandStart: 
	
		{
			cout << command << endl;
			camera = new CAMERA();
			if(!camera->initCamera())
			{
				cout << "init camera failed!" << endl;
			}
		//	camera->startCamera();
			break;
		}
		case CommandSavePicture: 
		{
			cout << command << endl; 
			//savePics(command);
			break;
		}
		case CommandSaveVideo: 
		{
			const char* temp = command + 2;
			int num;
			sscanf(temp, "%d", &num);
			cout << command << "-" << num << endl;
			saveVideo(100);
			break;
		}
		case CommandSynchronize: 
		{
			cout << "command : " << command << endl;
			string temp;
			savePics();
			vcos_sleep(500);
			temp = CAMERA :: savedFileName;
			string message = "starting transfer file";
			sendMessage(message.c_str());
			sendMessage(temp.c_str());
			if(!sendFile(temp))
				cout << "pic send failed" << endl;
			message = "transfer over";
			sendMessage(message.c_str());
			break;
		}
		case CommandTransferFile: 
		{
			cout << command << endl;
			string message = "starting transfer file";
			sendMessage(message.c_str());
			//sendFile();
			message = "transfer over";
			sendMessage(message.c_str());
			break;
		}
		case CommandRunCameraWithPreview: 
		{
			cout << command << endl;
//			startCamera(true);
			for(int i = 0; i < 10; ++i) {
				savePics();
				vcos_sleep(900);
			}
			break;
		}
		case CommandRunCameraWithOUTPreview: 
		{
			cout << command << endl;
//			startCamera(false);
			break;
		}
		case CommandChangeFPS:{
			const char *temp = command + 2;
			int fps, frame;
			sscanf(temp, "%d-%d", &fps, &frame);
			cout << command << endl;
 			changeFPS(fps, frame);
			break;
		}
		case CommandChangeShutterSpeed:{
			const char *temp = command + 2;
			int speed;
			sscanf(temp, "%d", &speed);
			cout << command << endl;
 			changeShutterSpeed(speed);
			break;
		}
		case CommandStop: 
		{
			cout << command << endl;
			camera->stopCamera();
			delete camera;
			break;
		}
         
		default:
		{
			cout << "wrong command :" << command << endl;
			string warn = "wrong command";
			sendMessage(warn.c_str());
			break;	
		}
	}
}
