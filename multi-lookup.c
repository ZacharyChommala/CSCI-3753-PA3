/*
 * multi-lookup.c
 * author: Lucas Zahorik
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h> //for get time of day function
#include <time.h> //also for get time of day function
#include "queue.h" //taken from the code I wrote last semester
#include "util.h"
#include "multi-lookup.h"


pthread_mutex_t queue_mutex;
pthread_mutex_t outfile_mutex;
//code for timing system inspired by http://www.cs.loyola.edu/~jglenn/702/S2008/Projects/P3/time.html
int time1, time2;
struct timeval start, end;
int requesterWorking = 1;
int requesterThread = 0;
int filesComplete = 0;

queue q;
FILE* outfile;
FILE* serviced;


void* requester(char *infile){
  FILE* inputfp = NULL;
  char hostname[1025];
  char errorstr[1025];
  inputfp = fopen(infile,"r");
  if(inputfp == NULL){
         sprintf(errorstr, "Input file error: %s", infile);
         perror(errorstr);
  }


  while(fscanf(inputfp, INPUTFS, hostname) > 0){

     char* host = strdup(hostname);
     pthread_mutex_lock(&queue_mutex);
     while(queue_is_full(&q))
     {

              pthread_mutex_unlock(&queue_mutex);
              usleep(rand()%100);
              pthread_mutex_lock(&queue_mutex);
     }

     queue_push(&q,(void*) host);
     pthread_mutex_unlock(&queue_mutex);
   }
   filesComplete++;
   requesterThread--;
   return 0;
  }


void* resolver(){
  char ipstr[INET6_ADDRSTRLEN];
  char *hostname;
  

  while(requesterWorking || !queue_is_empty(&q)){

    pthread_mutex_lock(&queue_mutex);
    while(queue_is_empty(&q));
    hostname = queue_pop(&q);

    pthread_mutex_unlock(&queue_mutex);

    if(dnslookup(hostname, ipstr, sizeof(ipstr)) == UTIL_FAILURE){
      fprintf(stderr, "dnslookup err: %s\n", hostname);
      strncpy(ipstr, "", sizeof(ipstr));
    }
    fprintf(outfile, "%s, %s\n", hostname, ipstr);
    pthread_mutex_unlock(&queue_mutex);
  }
  pthread_exit(NULL);
  return NULL;
}

int main(int argc, char* argv[]){
  
  gettimeofday(&start, NULL); //start timer
  //int readStart = 5;
  int inputFiles = argc - 5; //Amount of input files, minus call and output
  int requestThreads = atoi(argv[(argc - inputFiles - 4)]);
  int resolveThreads = atoi(argv[(argc - inputFiles - 3)]);
  printf("%d %d\n", requestThreads, resolveThreads);
  serviced = fopen(argv[(argc- inputFiles - 1)], "w");
  outfile = fopen(argv[(argc- inputFiles - 2)], "w");
  if(!outfile){
    perror("Error with output file");
    printf(argv[argc-1]);
    return EXIT_FAILURE;
  }
  if(requestThreads > inputFiles){
    perror("Error, more requestThreads than input files, changing requestThreads to number of input files\n");
    requestThreads = inputFiles;
  }
  pthread_t requested[requestThreads];
  pthread_t resolved[resolveThreads];

  queue_init(&q, 0);

  pthread_mutex_init(&queue_mutex, NULL);
  pthread_mutex_init(&outfile_mutex, NULL);
 // if(requestThreads >= inputFiles){
    for(int i = 0; i < inputFiles; i++){
      if(requesterThread >= requestThreads){
        sleep(1);
      }
      requesterThread++;
      fprintf(serviced, "RequesterThread %d serviced file %s\n", requesterThread, argv[i+5]);
      pthread_create(&(requested[i]), NULL, requester, (void*) argv[i+5]);
    }
  //}

  /*if(requestThreads < inputFiles){
    int i = 0;
    while((filesComplete - requestThreads) < requestThreads){
      requesterThread++;
      i++;
      fprintf(serviced, "RequesterThread %d serviced file %s\n", requesterThread, "xxx");
      pthread_create(&(requested[i]), NULL, requester, (void*) argv[i+5]);
    }
  }*/

  for(int i = 0; i < resolveThreads; i++){
    pthread_create(&(resolved[i]), NULL, resolver, NULL);
  }

  for(int i = 0; i < requestThreads; i++){
    pthread_join(requested[i], NULL);
  }

  requesterWorking = 0;

  for(int i = 0; i < resolveThreads; i++){
    pthread_join(resolved[i], NULL);
  }

  fclose(outfile);
  gettimeofday(&end, NULL);

  printf("Time to complete: %ld nanoseconds\n", (((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec))));

  queue_cleanup(&q);
  pthread_mutex_destroy(&queue_mutex);
  pthread_mutex_destroy(&outfile_mutex);
  return 0;

}
