#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>

// Hot dog structure
typedef struct {
    int id;
    int maker_id;
} Hotdog;

// Global shared state
typedef struct {
    Hotdog *buffer;              // Circular buffer
    int size;                    // S (buffer capacity)
    int front;                   // Index to remove from
    int back;                    // Index to add to
    int count;                   // Current items in buffer
    
    pthread_mutex_t lock;        // Protects buffer and counters
    pthread_mutex_t log_lock;    // Protects log file writes
    pthread_cond_t not_full;     // Makers wait here
    pthread_cond_t not_empty;    // Packers wait here
    
    int total_produced;          // Global production counter
    int total_packed;            // Global packing counter
    int target_count;            // N (total goal)
    int production_done;         // Flag: 1 when all N produced
    int next_hotdog_id;          // Global counter for unique hotdog IDs
    
    int *maker_counts;           // Per-maker production counts
    int *packer_counts;          // Per-packer packing counts
    
    FILE *log_file;
} HotdogManager;

// Thread argument structure
typedef struct {
    HotdogManager *manager;
    int id;
} ThreadArg;

// Simulate n units of work (from lab)
void do_work(int n) {
    for (int i = 0; i < n; i++) {
        long m = 300000000L;
        while (m > 0) m--;
    }
}

// Thread-safe logging function
void log_write(HotdogManager *manager, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    pthread_mutex_lock(&manager->log_lock);
    vfprintf(manager->log_file, format, args);
    fflush(manager->log_file);  // Ensure immediate write
    pthread_mutex_unlock(&manager->log_lock);
    
    va_end(args);
}

// Producer: Add hotdog to buffer
void pool_put(HotdogManager *manager, Hotdog *hd, int maker_id) {
    pthread_mutex_lock(&manager->lock);
    
    // Wait while buffer is full AND production not complete
    while (manager->count == manager->size && 
           manager->total_produced < manager->target_count) {
        pthread_cond_wait(&manager->not_full, &manager->lock);
    }
    
    // Check if we should stop (N already produced)
    if (manager->total_produced >= manager->target_count) {
        pthread_mutex_unlock(&manager->lock);
        return;
    }
    
    // Add to circular buffer
    manager->buffer[manager->back] = *hd;
    manager->back = (manager->back + 1) % manager->size;
    manager->count++;
    
    // Update counters
    manager->total_produced++;
    manager->maker_counts[maker_id]++;
    
    // Signal packers that buffer is not empty
    pthread_cond_signal(&manager->not_empty);
    pthread_mutex_unlock(&manager->lock);
}

// Maker thread (producer)
void* maker_thread(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    HotdogManager *manager = targ->manager;
    int maker_id = targ->id;
    
    while (1) {
        // Make hot dog (4 units of work)
        do_work(4);
        
        // Check if we should stop AND get unique hotdog ID (synchronized)
        pthread_mutex_lock(&manager->lock);
        
        // Check if we've produced enough BEFORE assigning ID
        if (manager->total_produced >= manager->target_count) {
            pthread_mutex_unlock(&manager->lock);
            break;
        }
        
        // Get unique hotdog ID only if we're continuing
        int hotdog_id = manager->next_hotdog_id++;
        pthread_mutex_unlock(&manager->lock);
        
        // Create hotdog
        Hotdog hd;
        hd.id = hotdog_id;
        hd.maker_id = maker_id;
        
        // Send hot dog into pool (1 unit of work)
        do_work(1);
        pool_put(manager, &hd, maker_id);
        
        // Log the action
        log_write(manager, "m%d puts %d\n", maker_id + 1, hd.id);
    }
    
    return NULL;
}

// Consumer: Remove hotdog from buffer
int pool_get(HotdogManager *manager, Hotdog *hd, int packer_id) {
    pthread_mutex_lock(&manager->lock);
    
    // Wait while buffer is empty AND production not done AND we haven't packed enough
    // Exit if: buffer empty AND (production done OR we've packed enough)
    while (manager->count == 0 && 
           !manager->production_done && 
           manager->total_packed < manager->target_count) {
        pthread_cond_wait(&manager->not_empty, &manager->lock);
    }
    
    // Check if we should stop (buffer empty AND either production done OR we've packed enough)
    if (manager->count == 0 && 
        (manager->production_done || manager->total_packed >= manager->target_count)) {
        pthread_mutex_unlock(&manager->lock);
        return 0; // No more items
    }
    
    // Remove from circular buffer
    *hd = manager->buffer[manager->front];
    manager->front = (manager->front + 1) % manager->size;
    manager->count--;
    
    // Update counters
    manager->total_packed++;
    manager->packer_counts[packer_id]++;
    
    // Signal makers that buffer is not full
    pthread_cond_signal(&manager->not_full);
    pthread_mutex_unlock(&manager->lock);
    
    return 1; // Success
}

// Packer thread (consumer)
void* packer_thread(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    HotdogManager *manager = targ->manager;
    int packer_id = targ->id;
    
    while (1) {
        // Take hot dog from pool (1 unit of work)
        do_work(1);
        
        Hotdog hd;
        if (!pool_get(manager, &hd, packer_id)) {
            break; // No more items
        }
        
        // Log the action
        log_write(manager, "p%d gets %d from m%d\n", 
                  packer_id + 1, hd.id, hd.maker_id + 1);
        
        // Pack the hot dog (2 units of work)
        do_work(2);
        
        // Check if we've packed enough
        pthread_mutex_lock(&manager->lock);
        if (manager->total_packed >= manager->target_count) {
            pthread_mutex_unlock(&manager->lock);
            break;
        }
        pthread_mutex_unlock(&manager->lock);
    }
    
    return NULL;
}

// Initialize hotdog manager
int hotdog_manager_init(HotdogManager *manager, int N, int S, int M, int P) {
    manager->size = S;
    manager->target_count = N;
    manager->total_produced = 0;
    manager->total_packed = 0;
    manager->production_done = 0;
    manager->next_hotdog_id = 1;  // Start IDs from 1
    manager->front = 0;
    manager->back = 0;
    manager->count = 0;
    
    // Allocate buffer
    manager->buffer = (Hotdog *)malloc(S * sizeof(Hotdog));
    if (!manager->buffer) return -1;
    
    // Allocate counter arrays
    manager->maker_counts = (int *)calloc(M, sizeof(int));
    manager->packer_counts = (int *)calloc(P, sizeof(int));
    if (!manager->maker_counts || !manager->packer_counts) {
        free(manager->buffer);
        if (manager->maker_counts) free(manager->maker_counts);
        if (manager->packer_counts) free(manager->packer_counts);
        return -1;
    }
    
    // Initialize mutexes
    if (pthread_mutex_init(&manager->lock, NULL) != 0) return -1;
    if (pthread_mutex_init(&manager->log_lock, NULL) != 0) {
        pthread_mutex_destroy(&manager->lock);
        return -1;
    }
    
    // Initialize condition variables
    if (pthread_cond_init(&manager->not_full, NULL) != 0) {
        pthread_mutex_destroy(&manager->lock);
        pthread_mutex_destroy(&manager->log_lock);
        return -1;
    }
    if (pthread_cond_init(&manager->not_empty, NULL) != 0) {
        pthread_cond_destroy(&manager->not_full);
        pthread_mutex_destroy(&manager->lock);
        pthread_mutex_destroy(&manager->log_lock);
        return -1;
    }
    
    // Open log file
    manager->log_file = fopen("log.txt", "w");
    if (!manager->log_file) {
        pthread_cond_destroy(&manager->not_empty);
        pthread_cond_destroy(&manager->not_full);
        pthread_mutex_destroy(&manager->log_lock);
        pthread_mutex_destroy(&manager->lock);
        return -1;
    }
    
    return 0;
}

// Cleanup hotdog manager
void hotdog_manager_cleanup(HotdogManager *manager) {
    if (manager->log_file) fclose(manager->log_file);
    if (manager->buffer) free(manager->buffer);
    if (manager->maker_counts) free(manager->maker_counts);
    if (manager->packer_counts) free(manager->packer_counts);
    
    pthread_cond_destroy(&manager->not_empty);
    pthread_cond_destroy(&manager->not_full);
    pthread_mutex_destroy(&manager->log_lock);
    pthread_mutex_destroy(&manager->lock);
}

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
    
    // Validate thread constraints
    if (M < 1) {
        fprintf(stderr, "Error: Number of maker threads (M) must be at least 1\n");
        return 1;
    }
    
    if (P > 30) {
        fprintf(stderr, "Error: Number of packer threads (P) cannot exceed 30\n");
        return 1;
    }
    
    HotdogManager manager;
    if (hotdog_manager_init(&manager, N, S, M, P) != 0) {
        fprintf(stderr, "Error: Failed to initialize hotdog manager\n");
        return 1;
    }
    
    // Write header to log
    log_write(&manager, "order:%d\n", N);
    log_write(&manager, "capacity:%d\n", S);
    log_write(&manager, "making machines:%d\n", M);
    log_write(&manager, "packing machines:%d\n", P);
    log_write(&manager, "-----\n");
    
    // Allocate thread arrays
    pthread_t *makers = (pthread_t *)malloc(M * sizeof(pthread_t));
    pthread_t *packers = (pthread_t *)malloc(P * sizeof(pthread_t));
    ThreadArg *maker_args = (ThreadArg *)malloc(M * sizeof(ThreadArg));
    ThreadArg *packer_args = (ThreadArg *)malloc(P * sizeof(ThreadArg));
    
    if (!makers || !packers || !maker_args || !packer_args) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        hotdog_manager_cleanup(&manager);
        return 1;
    }
    
    // Create maker threads
    for (int i = 0; i < M; i++) {
        maker_args[i].manager = &manager;
        maker_args[i].id = i;
        if (pthread_create(&makers[i], NULL, maker_thread, &maker_args[i]) != 0) {
            fprintf(stderr, "Error: Failed to create maker thread %d\n", i);
            hotdog_manager_cleanup(&manager);
            return 1;
        }
    }
    
    // Create packer threads
    for (int i = 0; i < P; i++) {
        packer_args[i].manager = &manager;
        packer_args[i].id = i;
        if (pthread_create(&packers[i], NULL, packer_thread, &packer_args[i]) != 0) {
            fprintf(stderr, "Error: Failed to create packer thread %d\n", i);
            hotdog_manager_cleanup(&manager);
            return 1;
        }
    }
    
    // Wait for all maker threads to finish
    for (int i = 0; i < M; i++) {
        pthread_join(makers[i], NULL);
    }
    
    // Signal that production is done
    pthread_mutex_lock(&manager.lock);
    manager.production_done = 1;
    pthread_cond_broadcast(&manager.not_empty);  // Wake all waiting packers
    pthread_mutex_unlock(&manager.lock);
    
    // Wait for all packer threads to finish
    for (int i = 0; i < P; i++) {
        pthread_join(packers[i], NULL);
    }
    
    // Write summary
    log_write(&manager, "-----\nsummary:\n");
    for (int i = 0; i < M; i++) {
        log_write(&manager, "m%d made %d\n", i + 1, manager.maker_counts[i]);
    }
    for (int i = 0; i < P; i++) {
        log_write(&manager, "p%d packed %d\n", i + 1, manager.packer_counts[i]);
    }
    
    // Cleanup
    free(makers);
    free(packers);
    free(maker_args);
    free(packer_args);
    hotdog_manager_cleanup(&manager);
    
    return 0;
}

