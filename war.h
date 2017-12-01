typedef struct arg_struct {
    sem_t * sem;
    pthread_mutex_t * mutex;
    int * child_alive;
    int * resource;
    int * attack;
    int * soldier_asleep; //1 for sleep, 0 for awake
    int * soldier_health; //3 for full health, 0 for dead
    int index;
    int nfarmfight;
    const char * access_point; //Used for reader to access damage
} arg_struct;

typedef struct arg_reader {
    pthread_mutex_t * mutex;
    int * child_alive;
    int * soldier_health; //3 for full health, 0 for dead
    int nfarmfight;
    const char * access_point; //Used for reader to access damage
} arg_reader;

int random_range(int n1, int n2);
void int_to_string(char * buffer, int x);
char* concat(const char *s1, const char *s2);
void free_damage_receiver(char ** damage_receiver, int size);
int get_alive_index(int * soldier_health, int n);
void * farmer_routine(void * params);
void consume (int * resource, int * attack);
void * soldier_routine(void * params);
int all_asleep(int * asleep, int n);
void wait_asleep(int * asleep, int n);
void awake_soldiers(int * asleep, int n);
int all_soldiers_dead(int * soldier_health, int n);
void write_integer(int fd, char * buffer, int integer, char additional_character);
void write_to_parent(int fd, int pid_of_attacker, int index_to_attack, int attack_points, int soldier_count);
void read_line(int fd, char * buffer);
void read_from_child(int fd, char * buffer, char * buffer2, char * buffer3, char * buffer4, char * buffer5);
void read_from_parent(int fd, char * buffer);
int count_soldiers(int * soldier_health, int n);
int get_highest_health_index(int * soldier_health, int n);
int distribute_damage(int * soldier_health, int n, int damage);
void * read_routine(void * params);
int round_complete (int round, int children, int attacked_tracker[BUFSIZ][children], int * alive);
int only_one_alive (int * alive, int children, int index);
int one_alive (int * alive, int children);
void log_attack(FILE * fp, int round, int children, int * pidarray, int * alive, int at[BUFSIZ][children], int aat[BUFSIZ][children], int sct[BUFSIZ][children], int dt[BUFSIZ][children]);


