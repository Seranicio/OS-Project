/*
     Authors : Serafim Barroca nº2014213964
		 					 Pedro Coelho nº
*/
#include		<sys/time.h>
#include 		<stdio.h>
#include 		<stdlib.h>
#include 		<string.h>
#include 		<sys/types.h>
#include 		<sys/wait.h>
#include 		<sys/ipc.h>
#include 		<sys/sem.h>
#include 		<sys/shm.h>
#include		<sys/mman.h>
#include 		<semaphore.h>
#include 		<sys/socket.h>
#include 		<pthread.h>
#include 		<netinet/in.h>
#include 		<arpa/inet.h>
#include 		<unistd.h>
#include 		<ctype.h>
#include 		<signal.h>
#include    "header.h"
#include		"simplehttpd.c"
#include 		<fcntl.h>
#include 		<errno.h>

//  valgrind -- bom debuger para ver erros de stacks por exemplo.

//Processos
pid_t       fatherID, configManagerID, statisticsManagerID;

// Server config.
config 	    *server_config;

//Estatisticas
int       	shmem_stats_id;
txt 	    	*shmem_stats;


//Workers
int 				*threadsID;
pthread_t 	*threads;
pthread_t		*threads_aux;

//Configuration Manager
char 				pipebuffer[SIZE_BUF];
pthread_t 	pipeThread;
int					pipethreadID;
int fd;

// Scheduler
int 			schedulerID;
pthread_t 		scheduler;

//Buffers de Pedidos
struct 			requestBuffer initialBuffer;

//Cleaning aux && others.
int keepalive=1;
int clear_threads; // variable that helps threads on closing themselves when manager wants less threads than current.
int readBuffer;
int numStatic,numComprimido;
long staticTime,comprimidoTime;
struct timeval saida;
time_t saida2;

int main(int argc, char ** argv){
	signal(SIGINT, cleanUp);

	startup_configs(); //comecar configs de servidor
	receiver();

	return 0;
}

void startup_configs(){
  printf("Starting Server...\n");
  fatherID = getpid(); // Processo Pai.
  memory_config_manager();
}

void memory_config_manager(){

		if((shmem_stats_id = shmget(IPC_PRIVATE, sizeof(txt), IPC_CREAT|0766))!=-1){ //(IPC_PRIVATE,tamanho da memoria , IPC_CREAT)
			printf("Shared stats memory created!\n");
		}
		else{
			printf("ERROR: Couldn't create shared stats\n");
			exit(0);
		}
		shmem_stats = (txt*) shmat(shmem_stats_id, NULL, 0);
		sem_init(&shmem_stats->mutex, 1, 1);

		//Create Statistics and Configuration Manager Thread.
		create_Processes();

		//Aloca memoria para a struct do configuration  manager.
		server_config = (config*) malloc(sizeof(config));

		read_config(); //reads configs from configs.txt

		//Inicialize all structs and pthreads.
		load_structs();

		//Criar a Pool de Threads
		int i;

		//cria o scheduler.
		pthread_create(&scheduler, NULL, schedulerFunction, &schedulerID);

		//cria o pipeThread.
		pthread_create(&pipeThread, NULL,configuration_Manager, &pipethreadID);

		for (i=0; i<server_config->numThreads;i++){
			threadsID[i]=i;
			if(pthread_create(&threads[i], NULL, worker, &threadsID[i])!=-1){
				printf("Thread %d was created\n",threadsID[i]);
			}
			else{
				printf("ERROR: Thread %d not created\n",threadsID[i]);
			}
		}
}

void create_Processes(){
	statisticsManagerID = fork(); // Processo statistics manager.

	if(statisticsManagerID==0){
		statisticsManagerID = getpid();
		printf("\n\nStatistics PID: %d\n\n",statisticsManagerID);
		statisticsManager();
		exit(0);
	}
	else if(statisticsManagerID < 0)
	{
		printf("ERROR: Could not create Statistics Manager\n");
		cleanUp();
		exit(0);
	}
}

void statisticsManager(){
	signal(SIGINT,statisticsclose);
	signal(SIGURG,  writeLog);
	signal(SIGUSR1, infoStats);
	signal(SIGUSR2, resetStats);
	printf("\nStatistics Ready!\n");

	while(1){
		//printf("\nStatistics waiting for signals\n");
		pause();
	}
}

void statisticsclose(){
	printf("\nStatistics Manager Closed!\n");
	exit(0);
}

void infoStats(){
	printf("\nNumero total de pedidos estáticos servidos: %d\n",numStatic);
	printf("Numero total de pedidos comprimidos servidos: %d\n",numComprimido);
	printf("Tempo médio para servir um pedido a conteúdo estático não comprimido: %ld\n",numStatic==0? 0:(staticTime/numStatic));
	printf("Tempo médio para servir um pedido a conteúdo estático comprimido: %ld\n",numComprimido==0? 0:(comprimidoTime/numComprimido));
}

void resetStats(){
	numStatic=0;
	numComprimido=0;
	staticTime=0;
	comprimidoTime=0;
	printf("\nStatistics Reset!\n");
}

void *configuration_Manager(void *arg){
	mensagem m;
	int catch_error;
	// Creates the named pipe if it doesn't exist yet
	if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)){
		perror("Cannot create pipe: ");
		pthread_exit(NULL);
	}
	// Opens the pipe for reading
	if ((fd = open(PIPE_NAME, O_RDONLY)) < 0) {
		perror("Cannot open pipe for reading: ");
		pthread_exit(NULL);
	}
	while(keepalive){
		catch_error=read(fd, &m, sizeof(mensagem));
		if(catch_error>0){ //if reads return > 0 it means that he read something .
			//printf("%s\n",m.cmd);
			configurationLoader(m);
		}
		else{
			printf("\nManager Closed! Opening a new pipe!\n");
			close(fd);
			if ((fd = open(PIPE_NAME, O_RDONLY)) < 0) {
				perror("Cannot open pipe for reading: ");
				pthread_exit(NULL);
			}
			printf("\nPipe Open\n");
		}
	}
	printf("Configuration Manager Thread Closed!");
	pthread_exit(NULL);
}

void configurationLoader(mensagem m){
	if(strcmp(m.cmd,"policy-1")==0 || strcmp(m.cmd,"policy-2")==0 || strcmp(m.cmd,"policy-3")==0){ //Policy change
		printf("\nReceaved Policy change from Manager!\n");
		changePolicy(m);
	}
	else if(m.cmd[0]=='T'){ // Thread change
		printf("\nReceaved Thread change from Manager!\n");
		changeThreads(atoi(&m.cmd[7]));
	}
	else if(m.cmd[0]=='A' || m.cmd[0]=='R'){ //Add or remove Scripts.
		printf("\nReceaved Add/Remove scripts from Manager!\n");
		changeScripts(m);
	}
}

void changePolicy(mensagem m){
	if(m.cmd[7]=='1'){
		printf("\nChanging Policy to FIFO\n");
		server_config->schedulingPolicy=1;
	}
	else if(m.cmd[7]=='2'){
		printf("\nChanging Policy to prioridade a conteúdo estático\n");
		server_config->schedulingPolicy=2;
	}
	else if(m.cmd[7]=='3'){
		printf("\nChanging Policy to prioridade a conteúdo estático comprimido\n");
		server_config->schedulingPolicy=3;
	}
}

void changeThreads(int number){
	int i;
	if(number == server_config->numThreads){
		printf("\nERROR: Number of changing Threads is equal to current number of Threads\n\n");
	}
	else{
		printf("\nChanging Threads to %d\n\n",number);
		if(number > server_config->numThreads){ // if Threads number is less than confuration manager request.
			clear_threads=number;
			threadsID = (int* ) realloc(threadsID,sizeof(int)*number);
			threads_aux=(pthread_t *) malloc(sizeof(pthread_t)*number);
			memcpy(threads_aux,threads,sizeof(threads));
			free(threads);
			threads = threads_aux;
			for(i=0;i<server_config->numThreads;i++){
				threadsID[i]=i;
			}
			for (i=server_config->numThreads;i<number;i++){
				threadsID[i]=i;
				if(pthread_create(&threads[i], NULL, worker, &threadsID[i])!=-1){
					printf("Thread %d was created\n",threadsID[i]);
				}
				else{
					printf("ERROR: Thread %d not created\n",threadsID[i]);
				}
			}
			server_config->numThreads=number;
		}
		else{ //if Threads are more than configuration manager request.
			clear_threads=number;
			printf("Killing Threads %d - %d\n",number,server_config->numThreads-1);
			handleWorkerCancelRequest();
			for(i=number;i<server_config->numThreads;i++){
				pthread_join(threads[i], NULL);
			}
			printf("\nKilled Threads %d - %d\n",number,server_config->numThreads-1);
			server_config->numThreads=number;
		}
	}
}

void changeScripts(mensagem m){
	char auxFile[100];
	int i;
	if(m.cmd[0]=='A'){ //ADDING SCRIPT.
		memcpy(auxFile,	&m.cmd[4],strlen(m.cmd)-4); //copy file string for comparing and adding more easily.
		printf("\nFile : %s\n",auxFile);
		printf("\nAdding script to server config!\n");
		for(i=0;i<MAX_SCRIPTS;i++){
			if(strcmp(server_config->allowedScripts[i],auxFile)==0){
				printf("\nERROR: File Already exists on the Server\n");
				return;
			}
		}
		strcpy(server_config->allowedScripts[server_config->numScripts],auxFile);
		server_config->numScripts++;
		printf("\nFile Added!\n");
	}
	else if(m.cmd[0]=='R'){ //REMOVE SCRIPT.
		memcpy(auxFile,	&m.cmd[7],strlen(m.cmd)-7); //copy file string for comparing and adding more easily.
		printf("\nFile : %s\n",auxFile);
		printf("\nRemoving script to server config!\n");
		for(i=0;i<MAX_SCRIPTS;i++){
			if(strcmp(server_config->allowedScripts[i],auxFile)==0){
				server_config->numScripts--;
				strcpy(server_config->allowedScripts[i],server_config->allowedScripts[server_config->numScripts]); //Change last file in array with the position of the file u want to erase;
				memset(&server_config->allowedScripts[server_config->numScripts],0,sizeof(server_config->allowedScripts[server_config->numScripts])); //Reseting array in i position.
				printf("\nFile was removed!\n");
				return;
			}
		}
		printf("\nERROR: Could not find script in Server!\n");
	}
}

void load_structs(){

	//aloca memoria para as Threads.
	threadsID = (int* ) malloc(sizeof(int)*server_config->numThreads);
	threads = (pthread_t *) malloc(sizeof(pthread_t)*server_config->numThreads);

	//Inicializa os Buffers.
	initialBuffer.read_position = 0;
	initialBuffer.write_position = 0;
	initialBuffer.size = INITIALBUFFER_SIZE; //INITIALBUFFER_SIZE=100 (header.h).
	initialBuffer.n_element = 0;
	initialBuffer.buffer = (request *) malloc(sizeof(request)*initialBuffer.size);
	pthread_mutex_init(&initialBuffer.mutex, NULL);
	pthread_cond_init(&initialBuffer.go_on, NULL);
	pthread_cond_init(&initialBuffer.alertScheduler, NULL);
}

void read_config(){
	/*
    Porto	para	o	servidor
    Política	de	escalonamento	no	tratamento	de	pedidos	a	conteúdo	normal	ou comprimido
    Número	de	threads	a	utilizar	para	tratamento	de	pedidos	pelo	servidor
    Lista	de	ficheiros	comprimidos	autorizados	(definidos	pelo	nome	do	ficheiro)
  */
		FILE *conf;
    int i;
    char line[30];

    //open config file
    conf = fopen ("config.txt", "r");
    if(conf==NULL){
        printf("Ficheiro de configuração não encontrado");
        return;
    }

		printf("Loading configs...\n");

    fgets(line, 30, conf);
    server_config->port=atoi(line);

    fgets(line, 30, conf);
    server_config->schedulingPolicy=atoi(line);

		fgets(line, 30, conf);
    server_config->numThreads=atoi(line);
		clear_threads=server_config->numThreads;

		i=0;
		server_config->numScripts=0;

    while(fgets(line,80,conf)!=NULL){
			strcpy(server_config->allowedScripts[i], line);
			strtok(server_config->allowedScripts[i], "\n"); //removes \n , problem that i had to compare strings.
			server_config->numScripts++;
			i++;
    }

    fclose(conf);

    printf("Configs Loaded!\n");
}

void cleanUp(){
	if(getpid() == fatherID)
	{
		printf("Cleaning Up...\n");
		int i;

		//Cleaning Pipe and Thread.
		printf("Closing Manager Pipe and Thread\n");

		close(fd);
		if(pthread_cancel(pipeThread)!=0){
			printf("ERROR: Could not send cancel request to Pipe Thread\n");
		}
		printf("Manager pipe Closed!\n");

		//Closing Sockets.
		printf("Closing sockets...\n");
		close(socket_conn);
		close(new_conn);
		printf("Sockets Closed!\n");

		//Cleaning Threads.
		keepalive=0;
		printf("Cleaning Threads...\n");
		handleWorkerCancelRequest();

		for(i=0;i<server_config->numThreads;i++){
			pthread_join(threads[i], NULL);
		}

		pthread_join(scheduler, NULL);

		printf("Threads cleaned!\n");

		//Cleaning Shared Memory.
		printf("Cleaning Some Structs\n");
		free(server_config);
		printf("Shared memory cleaned!\n");

		printf("Cleaning shared statistics memory...\n");
		kill(statisticsManagerID,SIGINT);
		shmctl(shmem_stats_id, IPC_RMID, NULL);
		shmdt(shmem_stats);
		printf("Shared statistics cleaned!\n");

		if(pthread_mutex_destroy(&initialBuffer.mutex)!=0){
			perror("ERROR: Could not destroy initialBuffer.mutex\n");
		}
		printf("Cleaning Buffer...\n");
		if(pthread_cond_destroy(&initialBuffer.go_on)!=0){
			perror("ERROR: Could not destroy initialBuffer.go_on\n");
		}
		if(pthread_cond_destroy(&initialBuffer.alertScheduler)!=0){
			perror("ERROR: Could not destroy initialBuffer.alertScheduler\n");
		}

		free(initialBuffer.buffer);
		printf("Buffer Cleaned!\n");
		printf("All clean up completed!\nExiting...\n");
	}
	exit(0);
}

void *worker(void *arg){
	int myID = *(int*) arg;
	int doWork=1;
	//Variables so mutex be more effecient.
	int type;
	int socket;
	int pos;
	char fileName[SIZE_BUF];
	printf("Thread %d ready to work\n",myID);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	//sleep(100);
	while(keepalive && doWork){
		pthread_mutex_lock(&initialBuffer.mutex);
		while(initialBuffer.n_element==0 && keepalive!=0 && doWork!=0){
			pthread_cond_wait(&initialBuffer.go_on,&initialBuffer.mutex);
			if(myID>=clear_threads){ //aux for removing threads by console manager. Thread knows by it's ID if it need to die or not.
				doWork=0;
			}
		}
		if(keepalive==0){
			doWork=0;
		}
		//sleep(5); // sleep para testar se esta a organizar os pedidios a 5 em 5 segundos ele manda a informacao ao brownser.
		if(doWork==1){
			printf("\n\n\n\n\nThread %d Working!\n\n\n\n",myID);
			pos=initialBuffer.read_position;
			initialBuffer.read_position=(initialBuffer.read_position+1)%initialBuffer.size;
			initialBuffer.n_element--;
			pthread_mutex_unlock(&initialBuffer.mutex);

			type=initialBuffer.buffer[pos].requestType;
			socket=initialBuffer.buffer[pos].socketToReply;
			strcpy(fileName,initialBuffer.buffer[pos].fileName);

			// Verify if request is for a page or script
			if(type!=1){
				execute_script(socket,fileName);
				sem_wait(&shmem_stats->mutex);
				time(&shmem_stats->saida_txt);
				gettimeofday(&saida,NULL); //time of deliver for client
			}
			else{
				// Search file with html page and send to client
				send_page(socket,fileName,0);
				sem_wait(&shmem_stats->mutex);
				time(&shmem_stats->saida_txt);
				gettimeofday(&saida,NULL); //time of deliver for client
			}
			// Terminate connection with client
			close(socket);
			//sending by shared memory to statistic manager.
			shmem_stats->Type=type;
			strcpy(shmem_stats->File,initialBuffer.buffer[pos].fileName);
			shmem_stats->entrada_txt=initialBuffer.buffer[pos].entrada_txt;
			shmem_stats->time=(saida.tv_usec)-(initialBuffer.buffer[pos].entrada.tv_usec);
			kill(statisticsManagerID,SIGURG);
		}
	}
	pthread_mutex_unlock(&initialBuffer.mutex);
	printf("Thread %d exited...\n",myID);
	pthread_exit(NULL);
}

void writeLog(){
	int fileD;
	int size_write;
	char aux[50];
	char *addr;
	int offset=0;
	int alocating=0;

	if(shmem_stats->Type==1){
		numStatic++;
		staticTime=shmem_stats->time;
		strcpy(aux,"Estatico");
	}
	else if(shmem_stats->Type==2){
		numComprimido++;
		comprimidoTime=shmem_stats->time;
		strcpy(aux,"Comprimido");
	}

	if ((fileD = open("server.log",O_CREAT | O_RDWR, (mode_t)0600)) < 0){
			perror("Error in file opening");
	}

	alocating=lseek(fileD, 0L, SEEK_END);
	alocating+=sizeof("Request type: \nRequested file: \nHora de recepcao: Hora de terminação de servir o pedido: \n------------------------------------------------------\n")+strlen(aux)+strlen(shmem_stats->File)+strlen(ctime(&shmem_stats->entrada_txt))+strlen(ctime(&shmem_stats->saida_txt))-1;

	lseek(fileD, alocating-1, SEEK_SET);

	write(fileD," ", 1);

	if ((addr = mmap(0,alocating,PROT_READ|PROT_WRITE,MAP_SHARED,fileD,0)) == MAP_FAILED){
    perror("Error in mmap");
  }
  offset += strlen(addr);
	sprintf(addr+offset,"Request type: %s\nRequested file: %s\nHora de recepcao: %sHora de terminação de servir o pedido: %s\n------------------------------------------------------\n",aux,shmem_stats->File,ctime(&shmem_stats->entrada_txt),ctime(&shmem_stats->saida_txt));

	if (munmap(addr,alocating) == -1) {
		perror("Error un-mmapping the file");
		/* Decide here whether to close(fd) and exit() or not. Depends... */
	}

	close(fileD);
	sem_post(&shmem_stats->mutex);
}

void receiver(){
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);

	printf("Listening for HTTP requests on port %d\n",server_config->port);

	// Configure listening port
	if ((socket_conn=fireup(server_config->port))==-1)
		exit(1);

	//sleep(10);
	while (1){
		// Accept connection on socket
		if ( (new_conn = accept(socket_conn,(struct sockaddr *)&client_name,&client_name_len)) == -1 ) {
			printf("Error accepting connection\n");
			exit(1);
		}
		pthread_mutex_lock(&initialBuffer.mutex);
		time(&initialBuffer.buffer[initialBuffer.write_position].entrada_txt);
		gettimeofday(&initialBuffer.buffer[initialBuffer.write_position].entrada, NULL); //No need for mutex
		pthread_mutex_unlock(&initialBuffer.mutex);
		// Process request
		handleRequest(new_conn);
	}
}

void *schedulerFunction(void *arg){ 	//Scheduler.
	int i;
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	int threadID = *(int*) arg;
	while(keepalive){
		pthread_mutex_lock(&initialBuffer.mutex);
		while(initialBuffer.n_element==0 && keepalive!=0){ //Verifica se existe requests.
			pthread_cond_wait(&initialBuffer.alertScheduler,&initialBuffer.mutex);
		}
		pthread_mutex_unlock(&initialBuffer.mutex);
		//tratar da informacao dependendo do tipo de scheduling
		if(server_config->schedulingPolicy==2){
			insertionSort_Static();
		}
		else if(server_config->schedulingPolicy==3){
			insertionSort_Script();
		}

		pthread_mutex_lock(&initialBuffer.mutex);
		pthread_cond_signal(&initialBuffer.go_on);
		pthread_mutex_unlock(&initialBuffer.mutex);

	}
	printf("Thread exited...\n");
	pthread_exit(NULL);
}

void handleRequest(int socket){

	/*request elementsReceived;
	elementsReceived.socketToReply = socket;*/

	// Identify new client
	identify(socket);

	// Process request
	get_request(socket);

	if(checkIfScriptOrHtml(req_buf)==2 && checkifscriptExists(req_buf)==0){
		not_found(socket);
		printf("\nFile not autorized from server!\n");
		close(socket);
	}
	else{
		pthread_mutex_lock(&initialBuffer.mutex);
		int write_pos = initialBuffer.write_position;
		initialBuffer.write_position=(write_pos+1)%initialBuffer.size;
		initialBuffer.n_element++;
		pthread_mutex_unlock(&initialBuffer.mutex);

		initialBuffer.buffer[write_pos].requestType=checkIfScriptOrHtml(req_buf);
		initialBuffer.buffer[write_pos].socketToReply=socket;
		strcpy(initialBuffer.buffer[write_pos].fileName,req_buf);

		pthread_mutex_lock(&initialBuffer.mutex);
		pthread_cond_signal(&initialBuffer.alertScheduler);
		pthread_mutex_unlock(&initialBuffer.mutex);
	}
}

int checkIfScriptOrHtml(char *str){
	if(str[strlen(str)-1]=='l'&&str[strlen(str)-2]=='m'&&str[strlen(str)-3]=='t'&&str[strlen(str)-4]=='h'){
			return 1;
	}
	else if(str[strlen(str)-1]=='z' && str[strlen(str)-2]=='g'){
		return 2;
	}
	return 3;
}

void handleWorkerCancelRequest(){
	printf("Broadcast activated!\n");
	pthread_mutex_unlock(&initialBuffer.mutex);
	pthread_cond_broadcast(&initialBuffer.go_on);
	pthread_cond_broadcast(&initialBuffer.alertScheduler);
}

void insertionSort_Static(){
	int i; //get position of the first script that was not processed.
	int j; //get position of first static after script position.
	int a; // a will be equal to j -> then trade with a-1 until it get to position i.
	request *aux;
	for(i=initialBuffer.read_position;i<initialBuffer.write_position;i++){
		if(initialBuffer.buffer[i].requestType==2){
			for(j=i+1;j<initialBuffer.write_position;j++){
				if(initialBuffer.buffer[j].requestType==1){
					for(a=j;a>i;a--){
						aux=(request *)malloc(sizeof(request));
						pthread_mutex_lock(&initialBuffer.mutex);
						memcpy(aux,&initialBuffer.buffer[a],sizeof(initialBuffer.buffer[a]));
						initialBuffer.buffer[a]=initialBuffer.buffer[a-1];
						initialBuffer.buffer[a-1]=*aux;
						free(aux);
						pthread_mutex_unlock(&initialBuffer.mutex);
					}
					break;
				}
			}
		}
	}
}

void insertionSort_Script(){
	int i; //get position of the first script that was not processed.
	int j; //get position of first static after script position.
	int a; // a will be equal to j -> then trade with a-1 until it get to position i.
	request *aux;
	pthread_mutex_lock(&initialBuffer.mutex);
	for(i=initialBuffer.read_position;i<initialBuffer.write_position;i++){
		printf("\nteste :%d\n",initialBuffer.write_position);
		if(initialBuffer.buffer[i].requestType==1){
			for(j=i+1;j<initialBuffer.write_position;j++){
				if(initialBuffer.buffer[j].requestType==2){
					for(a=j;a>i;a--){
						aux=(request *)malloc(sizeof(request));
						pthread_mutex_lock(&initialBuffer.mutex);
						memcpy(aux,&initialBuffer.buffer[a],sizeof(initialBuffer.buffer[a]));
						initialBuffer.buffer[a]=initialBuffer.buffer[a-1];
						initialBuffer.buffer[a-1]=*aux;
						pthread_mutex_unlock(&initialBuffer.mutex);
					}
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&initialBuffer.mutex);
}

int checkifscriptExists(char *fileName){
	int i;
	for(i=0;i<MAX_SCRIPTS;i++){
		if(strcmp(fileName,server_config->allowedScripts[i])==0){
			return 1;
		}
	}
	return 0;
}
