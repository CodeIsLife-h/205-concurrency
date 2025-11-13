#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>

// shared state
int *hotdog_id_buffer = NULL;      // Buffer for hotdog IDs
int *hotdog_maker_buffer = NULL;   // Buffer for maker IDs
int buffer_size = 0;                // S (buffer capacity)
int buffer_front = 0;               // Index to remove from
int buffer_back = 0;                // Index to add to
int buffer_count = 0;               // Current items in buffer

// initializes the mutexes and condition variables
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

// global counters 
int total_produced = 0;             // Global production counter
int total_packed = 0;               // Global packing counter
int target_count = 0;               // N (total goal)
int production_done = 0;            // Flag: 1 when all N produced
int next_hotdog_id = 1;             // Global counter for unique hotdog IDs

// counters for each thread 
int *maker_counts = NULL;           // Per-maker production counts
int *packer_counts = NULL;          // Per-packer packing counts

// Log file
FILE *log_file = NULL;

// work simulation
void do_work(int n) {
    for (int i = 0; i < n; i++) {
        long m = 300000000L;
        while (m > 0) m--;
    }
}

// logging function
void log_write(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    pthread_mutex_lock(&log_lock);
    vfprintf(log_file, format, args);
    fflush(log_file);  // Ensure immediate write
    pthread_mutex_unlock(&log_lock);
    
    va_end(args);
}

// adding hotdog to buffer
void pool_put(int hotdog_id, int maker_id) {
    pthread_mutex_lock(&lock);
    
    //wait if buffer is full and we still have reserved slots to add
    while (buffer_count == buffer_size && 
           total_produced <= target_count) {
        pthread_cond_wait(&not_full, &lock);
    }
    
    // add to back of circular buffer
    hotdog_id_buffer[buffer_back] = hotdog_id;
    hotdog_maker_buffer[buffer_back] = maker_id;
    buffer_back = (buffer_back + 1) % buffer_size;
    buffer_count++;
    
    // update counter under lock
    maker_counts[maker_id]++;
    
    //1 unit of work to put hotdog onto buffer (inside mutex)
    do_work(1);
    
    // log while under lock
    log_write("m%d puts %d\n", maker_id + 1, hotdog_id);
    
    // signal packers (inside mutex)
    pthread_cond_signal(&not_empty);

    pthread_mutex_unlock(&lock);
}

// Maker thread (producer)
void* maker_thread(void *arg) {
    int maker_id = *(int *)arg; 
    
    while (1) {
        //4 units to make hotdog
        do_work(4);
        
        //lock to reserve a production slot
        pthread_mutex_lock(&lock);
        
        //check if we've produced enough before reserving a slot
        if (total_produced >= target_count) {
            pthread_mutex_unlock(&lock);
            break;
        }
        
        //isolate hotdog_id and total hotdogs produced to "reserve"
        int hotdog_id = next_hotdog_id++;
        total_produced++; 

        pthread_mutex_unlock(&lock);
        
        pool_put(hotdog_id, maker_id);
    }
    
    return NULL;
}

// Check if buffer has items (without removing)
int pool_has_items(void) {
    pthread_mutex_lock(&lock);
    int has_items = (buffer_count > 0) && (total_packed < target_count) && !production_done;
    pthread_mutex_unlock(&lock);
    return has_items;
}

//function to get hotdog from buffer
int pool_get(int *hotdog_id, int *maker_id, int packer_id) {
    pthread_mutex_lock(&lock);
    
    //wait if buffer empty, production not done, and we haven't packed enough
    while (buffer_count == 0 && 
           !production_done && 
           total_packed < target_count) {
        pthread_cond_wait(&not_empty, &lock);
    }
    
    //if we've packed enough, stop immediately
    if (total_packed >= target_count) {
        pthread_mutex_unlock(&lock);
        return 0; 
    }
    
    //check if buffer is empty after checking total_packed
    if (buffer_count == 0) {
        //is production done?
        if (production_done) {
            pthread_mutex_unlock(&lock);
            return 0; 
        }
        //should not be reaching here 
        pthread_mutex_unlock(&lock);
        return 0;
    }
    
    //remove from front of circular buffer
    *hotdog_id = hotdog_id_buffer[buffer_front];
    *maker_id = hotdog_maker_buffer[buffer_front];
    buffer_front = (buffer_front + 1) % buffer_size;
    buffer_count--;
    
    //update counters under lock
    total_packed++;
    packer_counts[packer_id]++;
    
    //1 unit of work to take hotdog from buffer (inside mutex)
    do_work(1);
    
    //signal makers (inside mutex)
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&lock);
    
    return 1; // Success
}

//packer thread
void* packer_thread(void *arg) {
    int packer_id = *(int *)arg; 
    
    while (1) {
        // Get hotdog from buffer
        int hotdog_id, maker_id;
        if (!pool_get(&hotdog_id, &maker_id, packer_id)) {
            break; 
        }
        
        // Pack the hotdog (2 units of time) - outside the mutex
        do_work(2);
        
        // Log after packing (outside the mutex, but log_write uses log_lock for thread safety)
        log_write("p%d gets %d from m%d\n", packer_id + 1, hotdog_id, maker_id + 1);
    }
    
    return NULL;
}

// Initialize global state
int hotdog_manager_init(int N, int S, int M, int P) {
    buffer_size = S;
    target_count = N;
    total_produced = 0;
    total_packed = 0;
    production_done = 0;
    next_hotdog_id = 1;  // Start IDs from 1
    buffer_front = 0;
    buffer_back = 0;
    buffer_count = 0;
    
    // Allocate buffers
    hotdog_id_buffer = (int *)malloc(S * sizeof(int));
    hotdog_maker_buffer = (int *)malloc(S * sizeof(int));
    if (!hotdog_id_buffer || !hotdog_maker_buffer) {
        if (hotdog_id_buffer) free(hotdog_id_buffer);
        if (hotdog_maker_buffer) free(hotdog_maker_buffer);
        return -1;
    }
    
    // Allocate counter arrays
    maker_counts = (int *)calloc(M, sizeof(int));
    packer_counts = (int *)calloc(P, sizeof(int));
    if (!maker_counts || !packer_counts) {
        free(hotdog_id_buffer);
        free(hotdog_maker_buffer);
        if (maker_counts) free(maker_counts);
        if (packer_counts) free(packer_counts);
        return -1;
    }

    
    // Open log file
    log_file = fopen("log.txt", "w");
    if (!log_file) {
        free(hotdog_id_buffer);
        free(hotdog_maker_buffer);
        free(maker_counts);
        free(packer_counts);
        return -1;
    }
    
    return 0;
}
//clean up global state
void hotdog_manager_cleanup(void) {
    if (log_file) fclose(log_file);
    if (hotdog_id_buffer) free(hotdog_id_buffer);
    if (hotdog_maker_buffer) free(hotdog_maker_buffer);
    if (maker_counts) free(maker_counts);
    if (packer_counts) free(packer_counts);
    
    // Note: With static initialization, we don't need to destroy mutexes/conds
    // But it's good practice to destroy them anyway
    pthread_cond_destroy(&not_empty);
    pthread_cond_destroy(&not_full);
    pthread_mutex_destroy(&log_lock);
    pthread_mutex_destroy(&lock);
}


//main function
int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <N> <S> <M> <P>\n", argv[0]);
        fprintf(stderr, "  N = total hot dogs to produce\n");
        fprintf(stderr, "  S = buffer size\n");
        fprintf(stderr, "  M = number of maker threads\n");
        fprintf(stderr, "  P = number of packer threads\n");
        return 1;
    }
    
    int N = atoi(argv[1]);
    int S = atoi(argv[2]);
    int M = atoi(argv[3]);
    int P = atoi(argv[4]);
    
    if (N <= 0 || S <= 0 || M <= 0 || P <= 0) {
        fprintf(stderr, "Error: All arguments must be positive integers\n");
        return 1;
    }
    
    // Validate N > S constraint
    if (N <= S) {
        fprintf(stderr, "Error: Total hot dogs (N) must be greater than buffer size (S)\n");
        return 1;
    }
    
    // Validate thread constraints
    if (M < 1) {
        fprintf(stderr, "Error: Number of maker threads (M) must be at least 1\n");
        return 1;
    }
    
    if (P > 30) {
        fprintf(stderr, "Error: Number of packer threads (P) cannot exceed 30\n");
        return 1;
    }
    
    if (hotdog_manager_init(N, S, M, P) != 0) {
        fprintf(stderr, "Error: Failed to initialize hotdog manager\n");
        return 1;
    }
    
    //header for logs 
    log_write("order:%d\n", N);
    log_write("capacity:%d\n", S);
    log_write("making machines:%d\n", M);
    log_write("packing machines:%d\n", P);
    log_write("-----\n");
    
    //allocate thread arrays
    pthread_t *makers = (pthread_t *)malloc(M * sizeof(pthread_t));
    pthread_t *packers = (pthread_t *)malloc(P * sizeof(pthread_t));
    int *maker_ids = (int *)malloc(M * sizeof(int));
    int *packer_ids = (int *)malloc(P * sizeof(int));
    
    if (!makers || !packers || !maker_ids || !packer_ids) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        hotdog_manager_cleanup();
        return 1;
    }
    
    // create maker threads 
    for (int i = 0; i < M; i++) {
        maker_ids[i] = i;
        if (pthread_create(&makers[i], NULL, maker_thread, &maker_ids[i]) != 0) {
            fprintf(stderr, "Error: Failed to create maker thread %d\n", i);
            hotdog_manager_cleanup();
            return 1;
        }
    }
    
    // create packer threads
    for (int i = 0; i < P; i++) {
        packer_ids[i] = i;
        if (pthread_create(&packers[i], NULL, packer_thread, &packer_ids[i]) != 0) {
            fprintf(stderr, "Error: Failed to create packer thread %d\n", i);
            hotdog_manager_cleanup();
            return 1;
        }
    }
    //wait for maker and packer threads to finish 

    //wait for maker threads to finish 
    for (int i = 0; i < M; i++) {
        pthread_join(makers[i], NULL);
    }
    
    //signal production is done 
    pthread_mutex_lock(&lock);
    production_done = 1;
    //wake up all packers
    pthread_cond_broadcast(&not_empty);  
    pthread_mutex_unlock(&lock);
    
    //wait for packers threads to finish 
    for (int i = 0; i < P; i++) {
        pthread_join(packers[i], NULL);
    }
    
    //summary
    log_write("-----\nsummary:\n");
    for (int i = 0; i < M; i++) {
        log_write("m%d made %d\n", i + 1, maker_counts[i]);
    }
    for (int i = 0; i < P; i++) {
        log_write("p%d packed %d\n", i + 1, packer_counts[i]);
    }
    
    // Cleanup
    free(makers);
    free(packers);
    free(maker_ids);
    free(packer_ids);
    hotdog_manager_cleanup();
    
    return 0;
}
