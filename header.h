// necessary defines.
#define			MAX_SCRIPTS 		20
#define 		INITIALBUFFER_SIZE 	100
#define 		PIPE_NAME		"np_client_server"
#define 		FILEMODE S_IRWXU | S_IRGRP | S_IROTH

typedef struct Request{
	int requestType,socketToReply; // requestType = 1(html) | 2 (comprimido) | 3 (ERROR);
	char fileName[100];
	struct timeval entrada;
	time_t entrada_txt;
}request;

struct requestBuffer{
	int read_position,write_position,size,n_element; //write_position/read_position = a propria struct sabe em que posicao escreve/le.

	request *buffer;

	pthread_mutex_t mutex;
	pthread_cond_t go_on;
	pthread_cond_t alertScheduler;
};

typedef struct config_txt{ //SHARED MEMORY
	int Type;
	char File[50];
	long time;
	time_t entrada_txt;
	time_t saida_txt;
	sem_t mutex;
}txt;

typedef struct configStruct{
	int port, numThreads, schedulingPolicy,numScripts;
	char allowedScripts[MAX_SCRIPTS][100];
}config;

typedef struct{ //Pipe
  char cmd[1024];
}mensagem;

int main();
void startup_configs();
void memory_config_manager();
void create_Processes();
void *configuration_Manager(void *arg);
void read_config();
void configurationLoader();
void clean();
void *worker(void *arg);
void cleanUp();
void receiver();
void *schedulerFunction(void *arg);
void handleRequest(int socket);
int checkIfScriptOrHtml(char *str);
void handleWorkerCancelRequest();
void load_structs();
void changePolicy(mensagem m);
void changeScripts(mensagem m);
void changeThreads(int number);
void insertionSort_Static();
void insertionSort_Script();
int checkifscriptExists(char *fileName);
void statisticsManager();
void infoStats();
void resetStats();
void writeLog();
void statisticsclose();
