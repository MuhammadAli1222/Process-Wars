#include<stdio.h>
#include<stdlib.h>
#include<semaphore.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/mman.h>
#include<assert.h>
#include<fcntl.h>
#include<signal.h>
#include<pthread.h>
#include<string.h>
#include<time.h>
#include "war.h"
#define SEMREAD "sem_read"
#define SEMWRITE "sem_write"
#define ALIVE "alive"

int random_range(int n1, int n2) {
    return (rand() % (n2+1 - n1) + n1);
}

void int_to_string(char * buffer, int x) {
    int length;
    length = snprintf(NULL, 0, "%d", x); 
    snprintf(buffer,length+1, "%d", x); 
    buffer[length] = '\0';
}

char* concat(const char *s1, const char *s2)
{   
        char *result = malloc(strlen(s1)+strlen(s2)+1);
            strcpy(result, s1);
                strcat(result, s2);
                    return result;
}

void free_damage_receiver(char ** damage_receiver, int size) {
        /*starts from one here because fileName[0] is not freed*/
    for (int i = 0; i<size; i++) {
            free(damage_receiver[i]);
    }
}

int get_alive_index(int * soldier_health, int n) {
    for (int i = 0; i<n ;i++) {
        if (soldier_health[i] > 0) {return i;}
    }
    return -1;
}

void * farmer_routine(void * params) {
    arg_struct * arg = (arg_struct *) params;
    sem_t * sem = arg->sem;
    pthread_mutex_t * mutex = arg->mutex;
    int * child_alive = arg->child_alive;
    int * resourcearray = arg->resource;
    int * soldier_health = arg->soldier_health;
    int index = arg->index;
    int n = arg->nfarmfight;

    while(*child_alive) {
        sem_wait(sem);
        //usleep is microseconds
        usleep(random_range(400000,2200000));

        //check if soldier is dead or not
        if (soldier_health[index] == 0) {
            //soldier farmer is farming for is dead
            index = get_alive_index(soldier_health, n);
            //if all soldiers are dead, break out of loop. Child is dead.
            if (index == -1) {
                break;
            }
        }

        int * resource = &resourcearray[index];

        //lock resource - mutex
        pthread_mutex_lock(mutex);
        (*resource)++;
        //unlock resource
        pthread_mutex_unlock(mutex);
        sem_post(sem);
    }
    return (NULL);
}

void consume (int * resource, int * attack) {
    if (*resource <= 0) {return;}
    else {
        (*resource)--;
        (*attack)++;
    }
}

void * soldier_routine(void * params) {
    arg_struct * arg = (arg_struct *) params;
    pthread_mutex_t * mutex = arg->mutex;
    int * resource = arg->resource;
    int * attack = arg->attack;
    int * asleep = arg->soldier_asleep;
    int * soldier_health = arg->soldier_health;

    //while soldier is still alive
    while(soldier_health>0) {
        //main thread will wake me up
        if (!*asleep) {
            //lock resource - mutex
            pthread_mutex_lock(mutex);
            consume(resource,attack);
            //unlock resource
            pthread_mutex_unlock(mutex);
            //go back to sleep
            *asleep = 1;
        }
    }
    return (NULL);
}

int all_asleep(int * asleep, int n) {
    for (int i = 0; i<n; i++) {
        if (!asleep[i]) {
            return 0;
        }
    }
    return 1;
}

//blocking function- makes sure that I all asleep
void wait_asleep(int * asleep, int n) {
    while(!all_asleep(asleep, n)) {}
    return;
}

void awake_soldiers(int * asleep, int n) {
    for (int i = 0; i<n; i++) {
        asleep[i] = 0;
    }
}

int all_soldiers_dead(int * soldier_health, int n) {
    for (int i = 0; i<n; i++) {
        if (soldier_health[i] > 0) {
            return 0;
        }
    }
    return 1;
}

void write_integer(int fd, char * buffer, int integer, char additional_character) {
    int w;
    int length;
    length = snprintf(NULL,0,"%d",integer);
    sprintf(buffer,"%d",integer);
    buffer[length] = additional_character;
    w = write(fd, buffer, length+1);
    assert(w != -1);
}

void write_to_parent(int fd, int pid_of_attacker, int index_to_attack, int attack_points, int soldier_count) {
    char buffer[BUFSIZ];
    char buffer2[BUFSIZ];
    char buffer3[BUFSIZ];
    char buffer4[BUFSIZ];
    write(fd, "attack\0", 7);
    write_integer(fd, buffer, pid_of_attacker, '\0');
    write_integer(fd, buffer2, index_to_attack, '\0');
    write_integer(fd, buffer3, attack_points, '\0');
    write_integer(fd, buffer4, soldier_count, '\0');
}

void read_line(int fd, char * buffer) {
    int result;
    while((result=read(fd,buffer,1)) != 0 && *buffer!='\0') {
       assert(result != -1);
       buffer++;
    }
}    

void read_from_child(int fd, char * buffer, char * buffer2, char * buffer3, char * buffer4, char * buffer5) {
    read_line(fd, buffer);
    read_line(fd, buffer2);
    read_line(fd, buffer3);
    read_line(fd, buffer4);
    read_line(fd, buffer5);
}

void read_from_parent(int fd, char * buffer) {
    read_line(fd, buffer);
}

int count_soldiers(int * soldier_health, int n) {
    int count = 0;
    for (int i = 0; i<n; i++) {
        if (soldier_health[i] > 0) {
            count++;
        }
    }
    return count;
}

int get_highest_health_index(int * soldier_health, int n) {
    int max = -1;
    int index = -1;
    for (int i = 0; i<n; i++) {
        if (soldier_health[i] > 0 && soldier_health[i] > max) {
           max = soldier_health[i];
           index = i;
        }
    }
    //if max is -1 here it means all are dead
    return index;
}

int distribute_damage(int * soldier_health, int n, int damage) {
    while (damage) {
        int index = get_highest_health_index(soldier_health, n);
        if (index == -1) {return index;}
        soldier_health[index]--;
        damage--;
    }
    return 0;
}

void * read_routine(void * params) {
    arg_reader * arg = (arg_reader *) params;
    const char * access_point = arg->access_point;
    int * soldier_health = arg->soldier_health;
    int * child_alive = arg->child_alive;
    int nfarmfight = arg->nfarmfight;

    int fd = open(access_point, O_RDONLY);
    assert(fd != -1);

    while (*child_alive) {
        char damage[BUFSIZ];
        damage[0] = '\0';
        read_from_parent(fd, damage);
        if (damage[0] != '\0') {
            int result = distribute_damage(soldier_health, nfarmfight, atoi(damage));
            if (result == -1) {
                *child_alive = 0;
            }
        }
    }
    return (NULL);
}

int round_complete (int round, int children, int attacked_tracker[BUFSIZ][children], int * alive) {
    for(int i = 0; i<children; i++) {
        if (alive[i] && attacked_tracker[round][i] == -1) {
            return 0;
        }
    }
    return 1;
}

int only_one_alive (int * alive, int children, int index) {
    if (!alive[index]) {return 0;}
    else { 
        for (int i = 0; i<children; i++) {
            if (index != i && alive[i]) {
               return 0;
            }
        }
        return 1;
    }
}

int one_alive (int * alive, int children) {
    int count = 0;
    int index = -1;
    for (int i = 0; i<children; i++) {
        if (alive[i]) {
            count++;
            index = i;
        }
        if (count>1) {
            return -1;
        }
    }
    return index;
} 

void log_attack(FILE * fp, int round, int children, int * pidarray, int * alive, int at[BUFSIZ][children], int aat[BUFSIZ][children], int sct[BUFSIZ][children], int dt[BUFSIZ][children]) {
    char buff[20];
    struct tm *sTm;
    time_t now = time (0);
    sTm = gmtime (&now);
    strftime (buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", sTm);
    fprintf(fp, "Round %d, Time: %s\n", round, buff);
    for (int i = 0; i<children; i++) {
        if (alive[i]) {
            fprintf(fp, "Attacker PID is %d\n", pidarray[i]);
            fprintf(fp, "Attacked PID is %d\n", pidarray[at[round][i]]);
            fprintf(fp, "Attacked amount is %d\n", aat[round][i]);
            fprintf(fp, "Soldier Count is %d\n", sct[round][i]);
	    }
    }
    fprintf(fp, "Damage Calcuation:\n");
    for (int i = 0; i<children; i++) {
        if (alive[i]) {
            fprintf(fp, "Process %d gets damage of %d\n", pidarray[i], dt[round][i]);
        }
    }
    fprintf(fp, "---\n");
}


int main(int argc, char ** argv) {
    int children = -1;
    int nfarmfight = -1;

    if (argc>4) {
        for (int i = 1; i<argc; i++) {
            if (!strcmp(argv[i],"-children")) {
                if (i<argc-1) {
                    children = atoi(argv[++i]);
                }    
			}
            else if(!strcmp(argv[i],"-fighters")) {
                if (i<argc-1) {
                    nfarmfight = atoi(argv[++i]);
                }   
            }   
            else {fprintf(stderr,"usage: war [-children X | -fighters Y]\n");exit(0);}
        }   
    } else {fprintf(stderr,"usage: war [-children X | -fighters Y]\n");exit(0);}

    if (children<=0 || nfarmfight<=0) {fprintf(stderr,"usage: war [-children X | -fighters Y]\n");exit(0);}

    assert(nfarmfight>0);

    pid_t pidval;
    pid_t pidarray[children];
    int child_index;

    /*open semaphore for pipe*/
    sem_unlink(SEMREAD);
    sem_unlink(SEMWRITE);
    sem_t *pipe_read = sem_open(SEMREAD, O_CREAT, 0666, 0);
    sem_t *pipe_write = sem_open(SEMWRITE, O_CREAT, 0666, 1); 
    assert(pipe_read != SEM_FAILED && pipe_write != SEM_FAILED);

    /*Setup for shared memory alive integer array*/
    int shm_fd = shm_open (ALIVE, (O_CREAT | O_RDWR), (S_IRUSR | S_IWUSR));
    assert(shm_fd != -1);
    int result = ftruncate(shm_fd, sizeof(int)*children);
    assert(result == 0); 
    //map 
    int * alive = (int *) mmap(NULL, sizeof(int)*children, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    assert(alive != MAP_FAILED);

	//all children are alive
	for (int i = 0; i<children; i++) {
		alive[i] = 1;
	}

    /*I make a fifo at the access point for children to write to parent*/
	const char * access_point = "warfifo";
    result = mkfifo(access_point, 0666);
    assert(result == 0);

    /*<Make access points for each child to receive damage*/
    char * damage_receiver[children];
    char number_buffer[BUFSIZ];
    for (int i = 0; i<children; i++) {
        int_to_string(number_buffer,i);
        damage_receiver[i] = concat("fifo",number_buffer);
        result = mkfifo(damage_receiver[i], 0666);
        assert(result == 0);
    }

    /*Create X children*/
    for (int i = 0; i<children; i++) {
        pidval = fork();
        if (pidval == 0) {child_index = i; break;}
        pidarray[i] = pidval;
    }
    
    assert(pidval!=-1);

    if(pidval == 0) {
        
        printf("Child created! PID: %d\n", getpid());

        //this is on shared heap, so that threads can have access to it too
        sem_t * sem = malloc(sizeof(sem_t));
        pthread_mutex_t * thread_mutex  = malloc(sizeof(pthread_mutex_t));
        int * resource = malloc(sizeof(int)* nfarmfight);
        int * attack = malloc(sizeof(int));
        //1 for asleep, 0 for awake, -1 for dead 
        int * soldier_asleep = malloc(sizeof(int)* nfarmfight);
        int * soldier_health = malloc(sizeof(int)* nfarmfight);
        arg_struct * farmer_argument = malloc(sizeof(arg_struct)* nfarmfight);
        arg_struct * soldier_argument = malloc(sizeof(arg_struct)* nfarmfight);
        pthread_t * farmer_threads = malloc(sizeof(pthread_t)*nfarmfight);
        pthread_t * soldier_threads = malloc(sizeof(pthread_t)*nfarmfight);
        
        /*set up semaphore*/
        sem_t *pipe_read = sem_open(SEMREAD, 0); /* Open a preexisting semaphore. */
        sem_t *pipe_write = sem_open(SEMWRITE, 0); /* Open a preexisting semaphore. */
        assert(pipe_read != SEM_FAILED && pipe_write != SEM_FAILED);

		int fdwrite = open(access_point, O_WRONLY);
		assert(fdwrite != -1);

        int semval = sem_init(sem, 0, 6);
        assert(semval != -1);
        int mutexval = pthread_mutex_init(thread_mutex, NULL);
        assert(mutexval != -1);
        * attack = 0;
        for (int i = 0; i<nfarmfight; i++) {
            resource[i] = 0;
            soldier_asleep[i] = 1;
            soldier_health[i] = 3;
        }

        for (int i = 0; i<nfarmfight; i++) {

            /*All the farmer thread info*/
            arg_struct arg;
            arg.sem = sem;
            arg.mutex = thread_mutex;
            arg.child_alive = &alive[child_index];
            //in this case the whole resource array is passed
            arg.resource = resource;
            arg.attack = attack;
            arg.soldier_health = soldier_health;
            arg.index = i;
            arg.nfarmfight = nfarmfight;
            farmer_argument[i] = arg;

            //Create farmer thread!
            pthread_create(&farmer_threads[i], NULL, farmer_routine, &farmer_argument[i]);

            /*All the soldier thread info*/
            arg_struct arg2;
            arg2.mutex = thread_mutex;
            arg2.resource = &resource[i];
            arg2.attack = attack;
            arg2.soldier_asleep = &soldier_asleep[i];
            arg2.soldier_health = &soldier_health[i];
            arg2.index = i;
            soldier_argument[i] = arg2;

            //Create soldier thread!
            pthread_create(&soldier_threads[i], NULL, soldier_routine, &soldier_argument[i]);
        }

        /*another thread to read data from parent process and update soldier health*/
        arg_reader arg;
        arg.access_point = damage_receiver[child_index];
        arg.mutex = thread_mutex;
        arg.child_alive = &alive[child_index];
        //in this case the whole resource array is passed
        arg.soldier_health = soldier_health;
        arg.nfarmfight = nfarmfight;

        pthread_t read_thread;
        pthread_create(&read_thread, NULL, read_routine, &arg);

        while(alive[child_index]) {
            sleep(1); 

            //awake all soldiers
            awake_soldiers(soldier_asleep, nfarmfight);

            //indiv soldier threads will go back to sleep on their own after updating
            //wait for all soldier threads to be asleep again
            wait_asleep(soldier_asleep, nfarmfight);

            //randomly choose an index from 0 to nfarmfight to get child to attack
            //BUT CANNOT BE ITS PID, which is child_index
            int child_to_attack;
            while ((child_to_attack = random_range(0, children-1)) == child_index || !alive[child_to_attack]) {
                //if I'm the only one alive then no enemies left
                if(only_one_alive(alive, children, child_index)) {break;}
                //If somehow I'm dead, then I can't attack any more.
                if(!alive[child_index]) {break;}
            }

            //victory condition
            if (only_one_alive(alive, children, child_index)) {
                //at this time reader in main process will be waiting for the read token.
                //We give a token back, and the main process will check who has won.
                sem_post(pipe_read);
                break;
            }

            if (alive[child_index]) {
                sem_wait(pipe_write);
                write_to_parent(fdwrite, child_index, child_to_attack, *attack, count_soldiers(soldier_health,nfarmfight));
                sem_post(pipe_read);
            }   
        }

        sem_close(pipe_read);
        sem_close(pipe_write);
        sem_destroy(sem);
        free(sem);
        free(thread_mutex);
        free(resource);
        free(attack);
        free(soldier_asleep); 
        free(soldier_health);
        free(farmer_argument);
        free(soldier_argument);
        free(farmer_threads);
        free(soldier_threads); 
    }

    //main process should have a blocking wait that reads for all living children to report
    //Once they have reported, tally up who takes damage
    else {
        FILE * log_fp = fopen("log.txt", "w");
        assert(log_fp != NULL);
        int fdread = open(access_point, O_RDONLY);
        int round = 0;
        int fdwriter[children];
        int winning_index = -1;
        int informed[children];
        memset(informed, 0, sizeof(informed));
       
		//2D array containing the child that each child attacks. First index is the round number
        int attacked_tracker[BUFSIZ][children];
        memset(attacked_tracker, -1, sizeof(attacked_tracker[0][0]) * BUFSIZ * children);

        //We have a second copy for the log file
        int attacked_tracker_copy[BUFSIZ][children];
        memset(attacked_tracker_copy, -1, sizeof(attacked_tracker_copy[0][0]) * BUFSIZ * children);

        //2D array containing the damage done by each child
        int attack_amount_tracker[BUFSIZ][children];

        int soldier_count_tracker[BUFSIZ][children];

        //2D array. The first contains the number of rounds, second contains damage taken for each child
        int damage_tracker[BUFSIZ][children];
        memset(damage_tracker, 0, sizeof(damage_tracker[0][0]) * BUFSIZ * children);

        for (int i = 0; i<children; i++) {
            fdwriter[i] = open(damage_receiver[i], O_WRONLY);
            assert(fdwriter[i]!=-1);
        }

        while (1) {
            char instruction[BUFSIZ];
            char attacker_index[BUFSIZ];
            char attacked_index[BUFSIZ];
            char attack_points[BUFSIZ];
            char soldier_count[BUFSIZ];
            memset(instruction, 0, sizeof(instruction));
            memset(attacker_index, 0, sizeof(attacker_index));
            memset(attacked_index, 0, sizeof(attacked_index));
            memset(attack_points, 0, sizeof(attack_points));
            memset(soldier_count, 0, sizeof(soldier_count));
           
            //Before reading from the pipe, check to make sure that game isn't over
            //this is the exit point of the game

            sem_wait(pipe_read);
            if ((winning_index=one_alive(alive, children)) != -1) {break;}
            read_from_child(fdread,instruction,attacker_index,attacked_index,attack_points,soldier_count);
            sem_post(pipe_write);
            if (!strcmp(instruction,"attack")) {
                attacked_tracker[round][atoi(attacker_index)] = atoi(attacked_index);
                attacked_tracker_copy[round][atoi(attacker_index)] = atoi(attacked_index);
                attack_amount_tracker[round][atoi(attacker_index)] = atoi(attack_points);
                soldier_count_tracker[round][atoi(attacker_index)] = atoi(soldier_count);
            }

            if (round_complete(round, children, attacked_tracker, alive)) {
                /*Calculate the damage taken*/
                for (int i = 0; i<children; i++) {
                    int attacked = attacked_tracker[round][i];
                    //if the attack is mutual
                    if (attacked_tracker[round][attacked] == i) {
                        int attack1 = attack_amount_tracker[round][i];
                        int attack2 = attack_amount_tracker[round][attacked];
                        if (attack1>attack2) {
                            damage_tracker[round][attacked] += (attack1-attack2); 
                            damage_tracker[round][i] += 0;
                        }
                        else {
                            damage_tracker[round][attacked] += 0; 
                            damage_tracker[round][i] += (attack2-attack1);
                        }
                        //set this round to negative 1 so I don't come back and do this again
                        attacked_tracker[round][attacked] = -1;
                    }
                    else {
                        //if -1 it would mean that previously it was involved in a mutual attack
                        if (attacked != -1) {
                            damage_tracker[round][attacked] += attack_amount_tracker[round][i];
                        }
                    }
                }

                /*log this stuff*/
                log_attack(log_fp, round, children, pidarray, alive, attacked_tracker_copy, attack_amount_tracker, soldier_count_tracker, damage_tracker);
                for (int i = 0; i<children; i++) {
                    char buffer[BUFSIZ];
                    if (alive[i]) {
                        write_integer(fdwriter[i], buffer, damage_tracker[round][i], '\0');
                    }
                }

                /*increment round to next round*/
                round++;
            }

            for (int i = 0; i<children; i++) {
                if (!alive[i] && !informed[i]) {
                    printf("Child %d died in round %d\n", pidarray[i], round-1);
                    fprintf(log_fp, "Child %d died this round.\n", pidarray[i]);
                    fprintf(log_fp, "---\n");
                    informed[i] = 1;
                }
            }
        }

        //details of the finished game, who won etc
        assert (winning_index != -1);

        //Print any remaining dead children that has not been reported
        for (int i = 0; i<children; i++) {
            if (!alive[i] && !informed[i]) {
                printf("Child %d died in round %d\n", pidarray[i], round);
                fprintf(log_fp, "Child %d died this round.\n", pidarray[i]);
                fprintf(log_fp, "---\n");
                informed[i] = 1;
            }
        }

        //get the time. 
        char buff[20];
        struct tm *sTm;
        time_t now = time (0);
        sTm = gmtime (&now);
        strftime (buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", sTm);
        //report game over
        printf("Game over at %s! Winner is %d. See log.txt for more details.\n", buff, pidarray[winning_index]);
        fprintf(log_fp, "Game over at %s! Winner is %d\n", buff, pidarray[winning_index]);

        for (int i = 0; i<children; i++) {
            int status;
            int exitpid = wait(&status);
            if (exitpid > 0) {
                printf("Child process #%d exited. Exit status is %d.\n", exitpid, status);
            }
        }

        fclose(log_fp);
        sem_close(pipe_read);
        sem_close(pipe_write);
        free_damage_receiver(damage_receiver, children);
    }
    return 0;
}
