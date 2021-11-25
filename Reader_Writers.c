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

int main () { 
    int shm_id;
    int sem_id;
    struct sh_buffer *shared_memory;
    int pid;
    int key;
    int NUM_LINES=20;
    int K;
    int N;
    int line_number=5;
    int writer_block=0;
    int reader_block=1;
    int printer_block=2;
    int printer_done=3;
    long int seed;
    FILE *stream=fopen("demo.txt","r");
    if (stream==NULL) {
        perror("could not open file");
        exit(1);
    }
    key=ftok("Reader_Writers.c",'A');
    /* Create a new shared memory segment */
    shm_id = shmget(key, sizeof(struct sh_buffer), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Shared memory creation");
        exit(EXIT_FAILURE);
    }

    /* Create a new semaphore set id with three semaphores  */
    sem_id = semget(key, 4, IPC_CREAT | IPC_EXCL | 0666);
    if (sem_id == -1) {
        perror("Semaphore creation ");
        shmctl(sem_id, IPC_RMID, (struct shmid_ds *) NULL);
        exit(EXIT_FAILURE);
    }

    /* Set the value of the semaphore to 1 */
    union semun arg;

    arg.val =1;
    if (semctl(sem_id, writer_block, SETVAL, arg) == -1) {  //semaphore for allowing writers to write in shared memory
        perror("# Semaphore setting value ");
    }
    arg.val=0;
    if (semctl(sem_id, reader_block, SETVAL, arg) == -1) {  //semaphore for allowing reader to read from shared memory only after a writer has wrote something
      perror("# Semaphore setting value ");
    }
    arg.val=0;
    if (semctl(sem_id, printer_block, SETVAL, arg) == -1) {  //semaphore for allowing printer to read from shared memory only after the parent has responded with something
      perror("# Semaphore setting value ");
    }
    arg.val=0;
    if (semctl(sem_id, printer_done, SETVAL, arg) == -1) {  //semaphore for allowing printer to read from shared memory only after the parent has responded with something
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
                seed=time(NULL)^getpid();
                srand(seed);
                for (int j=0;j<N;j++) {                    
                    sem_down(sem_id,writer_block);
                    shared_memory->request=rand()%NUM_LINES;
                    sem_up(sem_id,reader_block);
                    sem_down(sem_id,printer_block);
                    printf("%s",shared_memory->response);
                    fflush(NULL);
                    sem_up(sem_id,printer_done);
                    
                    
                }
                exit(0);
            }

        }  
        for (int total_rep=0;total_rep<K*N;total_rep++) {
            
            sem_down(sem_id,reader_block);
            line_number=shared_memory->request;
            //sem_up(sem_id,writer_block);
            char line[LINE_SIZE];
            int iter=0;
            while (fgets(line,LINE_SIZE,stream)!=NULL) {
                if (iter==line_number) {
                    strcpy(shared_memory->response,line);
                    sem_up(sem_id,printer_block);
                    sem_down(sem_id,printer_done);
                    sem_up(sem_id,writer_block);
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
