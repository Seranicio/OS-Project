#include 		<stdio.h>
#include 		<stdlib.h>
#include 		<string.h>
#include 		<sys/types.h>
#include 		<sys/wait.h>
#include 		<sys/ipc.h>
#include 		<sys/sem.h>
#include 		<sys/shm.h>
#include 		<semaphore.h>
#include 		<sys/socket.h>
#include 		<pthread.h>
#include 		<netinet/in.h>
#include 		<arpa/inet.h>
#include 		<unistd.h>
#include 		<ctype.h>
#include 		<signal.h>
#include    <fcntl.h>
#include    <errno.h>

#define 		PIPE_NAME		"np_client_server"

void menu();
void changeFiles();
void changePolicy();
void changeThreads();
void openPipe();

int fd;

typedef struct{
  char cmd[1024];
}mensagem;

int main(int argc, char ** argv){
  openPipe();
  menu();
  close(fd);
  return 0;
}

void openPipe(){
  if ((fd = open(PIPE_NAME, O_WRONLY)) < 0) {
    perror("Cannot open pipe for writing: ");
    exit(0);
  }
}

void menu(){
  char escolha[100];
  while(1){
    printf("\n1.Policy\n2.Pool of Threads\n3.Authorized File\n0.Exit\n\n");
    scanf("%s",escolha);
    if(escolha[0]=='1'){
      changePolicy();
    }
    else if(escolha[0]=='2'){
      changeThreads();
    }
    else if(escolha[0]=='3'){
      changeFiles();
    }
    else if(escolha[0]=='0'){
      return;
    }
    else{
      printf("\nWrong choic!\n\n");
    }
  }
}

void changePolicy(){
  mensagem m;
  char escolha[100];
  printf("\n1.Escalonamento	FIFO\n2.Escalonamento com prioridade a conteúdo estático\n3.Escalonamento com prioridade a conteúdo estático comprimido\n0.Return\n\n");
  scanf("%s",escolha);
  if(escolha[0]=='1'){
    strcpy(m.cmd, "policy-1");
    write(fd, &m, sizeof(mensagem));
    printf("\nSent!\n\n");
  }
  else if(escolha[0]=='2'){
    strcpy(m.cmd, "policy-2");
    write(fd, &m, sizeof(mensagem));
    printf("\nSent!\n\n");
  }
  else if(escolha[0]=='3'){
    strcpy(m.cmd, "policy-3");
    write(fd, &m, sizeof(mensagem));
    printf("\nSent!\n\n");
  }
  else if(escolha[0]=='0'){
    return;
  }
  else{
    printf("\nWrong choice!\n\n");
  }
}

void changeThreads(){
  mensagem m;
  char escolha[100];
  printf("\nNumber of Threads to change: ");
  scanf("%s",escolha);
  if(atoi(escolha) <10 && atoi(escolha) > 0){
    strcpy(m.cmd, "Thread-");
    strcat(m.cmd, escolha);
    write(fd, &m, sizeof(mensagem));
    printf("\nSent!\n\n");
  }
  else{
    printf("\nThreads can only be 1 to 9!\n\n");
  }
}

void changeFiles(){
  mensagem m;
  char escolha[100];
  char aux[1024];
  printf("\n1.Add files\n2.Remove files\n0.Return\n\n");
  scanf("%s",escolha);
  if(escolha[0]=='1'){
    printf("Add: ");
    scanf("%s",aux);
    strcpy(m.cmd, "Add-");
    strcat(m.cmd,aux );
    write(fd, &m, sizeof(mensagem));
    printf("\nSent!\n\n");
  }
  else if(escolha[0]=='2'){
    printf("Remove: ");
    scanf("%s",aux);
    strcpy(m.cmd, "Remove-");
    strcat(m.cmd, aux);
    write(fd, &m, sizeof(mensagem));
    printf("\nSent!\n\n");
  }
  else{
    printf("\nWrong choice!\n\n");
  }
}
