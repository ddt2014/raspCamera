#include "client.h"
#include "camera.h"
#include <pthread.h>

int main(int argc, char* argv[])
{

//	system("./sync.sh");
	ClientSocket clientSocket;
	if(!clientSocket.initSocket())
	{
		cout << "init server failed!" << endl;
		return 0;
	}
	pthread_t thread_id;
	if(pthread_create(&thread_id, NULL, &ClientSocket :: recieveMessage, & clientSocket) != 0)
    {
        cout << "pthread_create error!" << endl;
		return 0;
    }
	getchar();
   return 1;
}
