#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#define LINE_SIZE 100
/* Union semun */
union semun {
    int val;                  /* value for SETVAL */
    struct semid_ds *buf;     /* buffer for IPC_STAT, IPC_SET */
    unsigned short *array;    /* array for GETALL, SETALL */
};

void free_resources(int shm_id, int sem_id,int key) { 
    /* Delete the shared memory segment */
    shmctl(shm_id,IPC_RMID,NULL);
    /* Delete the semaphore */
    semctl(sem_id,1,IPC_RMID,0);
}

/* Semaphore P - down operation, using semop */
int sem_down(int sem_id,int sem_num) {
    struct sembuf sem_d;

    sem_d.sem_num = sem_num;
    sem_d.sem_op = -1;
    sem_d.sem_flg = 0;
    if (semop(sem_id, &sem_d, 1) == -1) {
        perror("# Semaphore down (P) operation ");
        return -1;
    }
    return 0;
}

/* Semaphore V - up operation, using semop */
int sem_up(int sem_id,int sem_num) {
    struct sembuf sem_d;

    sem_d.sem_num = sem_num;
    sem_d.sem_op = 1;
    sem_d.sem_flg = 0;
    if (semop(sem_id, &sem_d, 1) == -1) {
        perror("# Semaphore up (V) operation ");
        return -1;
    }
    return 0;
}

struct sh_buffer {
    int request;
    char response[LINE_SIZE];
};

int main (int argc,char** argv) { 
    int shm_id;
    int sem_id;
    struct sh_buffer *shared_memory;
    int pid;
    int line_number;
    int key;
    int NUM_LINES;
    int K;
    int N;
    int writer_block=0;
    int reader_block=1;
    int printer_block=2;
    long int seed;
    FILE *stream;
    if (argc==2)  {
        stream=fopen(argv[1],"r");
        if (stream==NULL) {
            perror("could not open file");
            exit(1);
        }
    }
    else {
        puts("Please provide a file");
    }
    char line[LINE_SIZE];
    NUM_LINES=0;
    while(fgets(line,LINE_SIZE,stream)!=NULL) {
        NUM_LINES++;
    }
    rewind(stream);
    key=ftok("Reader_Writers.c",'A');
    /* Create a new shared memory segment */
    shm_id = shmget(key, sizeof(struct sh_buffer), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Shared memory creation");
        exit(EXIT_FAILURE);
    }

    /* Create a new semaphore set id with three semaphores  */
    sem_id = semget(key, 3, IPC_CREAT | IPC_EXCL | 0666);
    if (sem_id == -1) {
        perror("Semaphore creation ");
        shmctl(sem_id, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    union semun arg;

    arg.val =1;
    if (semctl(sem_id, writer_block, SETVAL, arg) == -1) {  //semaphore for allowing writers to write in shared memory
        perror("# Semaphore setting value ");
    }
    arg.val=0;
    if (semctl(sem_id, reader_block, SETVAL, arg) == -1) {  //semaphore for allowing reader to read from shared memory only after a writer has requested a line
      perror("# Semaphore setting value ");
    }
    arg.val=0;
    if (semctl(sem_id, printer_block, SETVAL, arg) == -1) {  //semaphore for allowing printer to read from shared memory only after the parent has responded with the line requested
      perror("# Semaphore setting value ");
    }

    /* Attach the shared memory segment */
    shared_memory = shmat(shm_id, NULL, 0);
    if (shared_memory == NULL) {
        perror("Shared memory attach ");
        free_resources(shm_id, sem_id,key);
        exit(EXIT_FAILURE);
    }
    printf("Enter number of child proccesses:\n");
    scanf("%d",&K);
    printf("Enter number of iterations :\n");
    scanf("%d",&N);
        for (int i=0;i<K;i++)  {
            if ((pid = fork()) == -1) { 
                perror("fork");
                free_resources(shm_id, sem_id,key);
                exit(EXIT_FAILURE);
            }
            if (pid==0) {
                seed=time(NULL)*getpid();
                srand(seed);
                double d_time=0.0;
                for (int j=0;j<N;j++) {           
                    double start_time=time(NULL);         
                    sem_down(sem_id,writer_block);
                    shared_memory->request=rand()%NUM_LINES;
                    sem_up(sem_id,reader_block);
                    sem_down(sem_id,printer_block);
                    double end_time=time(NULL);
                    printf("%s",shared_memory->response);
                    fflush(NULL);
                    sem_up(sem_id,writer_block);
                    d_time+=end_time-start_time;
                }
                double mean_response_time=(double)d_time/((double)N);
                printf("Proccess %d finished with an average time between request and response of: %lf\n",getpid(),mean_response_time);
                exit(0);
            }

        }  
        for (int total_rep=0;total_rep<K*N;total_rep++) {
            sem_down(sem_id,reader_block);
            line_number=shared_memory->request;
            char line[LINE_SIZE];
            int iter=0;
            while (fgets(line,LINE_SIZE,stream)!=NULL) {
                if (iter==line_number) {
                    strcpy(shared_memory->response,line);
                    sem_up(sem_id,printer_block);
                    rewind(stream);
                    break;
                }
                iter++;
            }

            
        }
    /* Wait for child processes */
    printf("Waiting \n");
    for (int i=0;i<K;i++) {
        wait(NULL);
    }
    printf("DONE\n");

    /* Clear recourses */
    shmdt(shared_memory);
    free_resources(shm_id,sem_id,key);

    return 0;
}
