#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

int n;
__thread int *board;
int solutions_count = 0;
int unique_count = 0;
int num_cores = 0;
int num_threads = 0;  // User-specified thread count (0 = auto-detect)
int print_solutions = 1;  // 1 = print solutions, 0 = quiet mode
int show_progress = 0;  // 1 = show progress, 0 = no progress
int work_completed = 0;  // Track completed work items
int total_work_items = 0;  // Total work items to process

// Forward declarations
int is_safe_with_board(int row, int col, int *b);
void generate_work_queue(int row, int *partial_board);

// Mutex for synchronizing print operations and shared data access
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t progress_mutex = PTHREAD_MUTEX_INITIALIZER;

// Thread work structure - represents a partial board state to solve from
typedef struct {
    int *board;  // Partial board configuration
    int depth;   // Starting depth (which row to start solving from)
} WorkItem;

// Work queue for distributing partial solutions
typedef struct {
    WorkItem *items;
    int capacity;
    int size;
} WorkQueue;

WorkQueue work_queue;
int parallelization_depth = 0;

// Hash set to store canonical solutions with their unique ID
typedef struct {
    char **solutions;
    int *unique_ids;  // Maps canonical form to its unique ID
    int capacity;
    int size;
} SolutionSet;

SolutionSet solution_set;

/**
 * Convert a board configuration to a string for comparison
 */
char *board_to_string(int *b) {
    char *str = (char *)malloc((n * n) + 1);
    for (int i = 0; i < n; i++) {
        str[i] = b[i] + '0';  // Store column position for each row
    }
    str[n] = '\0';
    return str;
}

/**
 * Compare two board strings lexicographically
 * Returns the lexicographically smaller one
 */
char *min_string(char *s1, char *s2) {
    int cmp = strcmp(s1, s2);
    if (cmp <= 0) {
        free(s2);
        return s1;
    } else {
        free(s1);
        return s2;
    }
}

/**
 * Apply transformations to generate all 8 symmetries
 * and return the lexicographically smallest one (canonical form)
 */
char *get_canonical_form(int *b) {
    int *temp = (int *)malloc(n * sizeof(int));
    char *canonical = board_to_string(b);
    
    // Rotation 90° clockwise: (row, col) -> (col, n-1-row)
    for (int row = 0; row < n; row++) {
        temp[b[row]] = n - 1 - row;  // If queen at (row, b[row]), new position: (b[row], n-1-row)
    }
    char *rotated90 = board_to_string(temp);
    canonical = min_string(canonical, rotated90);
    
    // Rotation 180°: (row, col) -> (n-1-row, n-1-col)
    for (int row = 0; row < n; row++) {
        temp[n - 1 - b[row]] = n - 1 - row;
    }
    char *rotated180 = board_to_string(temp);
    canonical = min_string(canonical, rotated180);
    
    // Rotation 270° clockwise: (row, col) -> (n-1-col, row)
    for (int row = 0; row < n; row++) {
        temp[n - 1 - b[row]] = row;
    }
    char *rotated270 = board_to_string(temp);
    canonical = min_string(canonical, rotated270);
    
    // Horizontal flip: (row, col) -> (row, n-1-col)
    for (int row = 0; row < n; row++) {
        temp[row] = n - 1 - b[row];
    }
    char *flipped_h = board_to_string(temp);
    canonical = min_string(canonical, flipped_h);
    
    // Vertical flip: (row, col) -> (n-1-row, col)
    for (int row = 0; row < n; row++) {
        temp[n - 1 - row] = b[row];
    }
    char *flipped_v = board_to_string(temp);
    canonical = min_string(canonical, flipped_v);
    
    // Diagonal flip (main): (row, col) -> (col, row)
    for (int row = 0; row < n; row++) {
        temp[b[row]] = row;
    }
    char *flipped_diag = board_to_string(temp);
    canonical = min_string(canonical, flipped_diag);
    
    // Anti-diagonal flip: (row, col) -> (n-1-col, n-1-row)
    for (int row = 0; row < n; row++) {
        temp[n - 1 - b[row]] = n - 1 - row;
    }
    char *flipped_antidiag = board_to_string(temp);
    canonical = min_string(canonical, flipped_antidiag);
    
    free(temp);
    return canonical;
}

/**
 * Check if a canonical solution is already in the set
 * Returns the unique ID if found, or -1 if not found
 */
int get_unique_id(char *canonical) {
    for (int i = 0; i < solution_set.size; i++) {
        if (strcmp(solution_set.solutions[i], canonical) == 0) {
            return solution_set.unique_ids[i];
        }
    }
    return -1;
}

/**
 * Add a canonical solution to the set with its unique ID
 */
void add_to_set(char *canonical, int unique_id) {
    if (solution_set.size >= solution_set.capacity) {
        solution_set.capacity *= 2;
        solution_set.solutions = (char **)realloc(solution_set.solutions, 
                                                   solution_set.capacity * sizeof(char *));
        solution_set.unique_ids = (int *)realloc(solution_set.unique_ids,
                                                 solution_set.capacity * sizeof(int));
    }
    solution_set.solutions[solution_set.size] = canonical;
    solution_set.unique_ids[solution_set.size] = unique_id;
    solution_set.size++;
}

/**
 * Print a solution
 */
void print_solution(int *b, int num) {
    printf("\nSolution #%d:\n", num);
    for (int row = 0; row < n; row++) {
        for (int col = 0; col < n; col++) {
            if (b[row] == col) {
                printf("♛ ");
            } else {
                printf("· ");
            }
        }
        printf("\n");
    }
}

/**
 * Add a work item to the queue
 */
void add_work_item(int *partial_board) {
    if (work_queue.size >= work_queue.capacity) {
        work_queue.capacity *= 2;
        work_queue.items = (WorkItem *)realloc(work_queue.items,
                                               work_queue.capacity * sizeof(WorkItem));
    }
    
    WorkItem *item = &work_queue.items[work_queue.size];
    item->board = (int *)malloc(n * sizeof(int));
    memcpy(item->board, partial_board, n * sizeof(int));
    item->depth = parallelization_depth;
    work_queue.size++;
}

/**
 * Check if it's safe to place a queen (using provided board)
 */
int is_safe_with_board(int row, int col, int *b) {
    for (int i = 0; i < row; i++) {
        if (b[i] == col) {
            return 0;
        }
        if (abs(b[i] - col) == abs(i - row)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Generate all partial board configurations up to parallelization_depth
 */
void generate_work_queue(int row, int *partial_board) {
    if (row == parallelization_depth) {
        // Found a valid partial board - add to work queue
        add_work_item(partial_board);
        return;
    }
    
    for (int col = 0; col < n; col++) {
        if (is_safe_with_board(row, col, partial_board)) {
            partial_board[row] = col;
            generate_work_queue(row + 1, partial_board);
        }
    }
}

/**
 * Check if it's safe to place a queen (using thread-local board)
 */
int is_safe(int row, int col) {
    for (int i = 0; i < row; i++) {
        if (board[i] == col) {
            return 0;
        }
        if (abs(board[i] - col) == abs(i - row)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Solve N-Queens using backtracking
 */
void solve_nqueens(int row) {
    if (row == n) {
        // Protect all shared data access with mutex
        pthread_mutex_lock(&data_mutex);
        
        solutions_count++;
        
        // Get canonical form
        char *canonical = get_canonical_form(board);
        
        // Check if we've seen this canonical form before
        int unique_id = get_unique_id(canonical);
        if (unique_id == -1) {
            // This is a new unique solution
            unique_count++;
            add_to_set(canonical, unique_count);
        } else {
            // This is a symmetric duplicate of an existing unique solution
            free(canonical);
        }
        
        pthread_mutex_unlock(&data_mutex);
        
        // Now print with print_mutex (separate to not hold data_mutex while printing)
        if (print_solutions) {
            pthread_mutex_lock(&print_mutex);
            
            if (unique_id == -1) {
                printf("\n═══════════════════════════════════════════════════════════\n");
                printf("Solution #%d (UNIQUE #%d)\n", solutions_count, unique_count);
                printf("═══════════════════════════════════════════════════════════\n");
                print_solution(board, solutions_count);
            } else {
                printf("\n───────────────────────────────────────────────────────────\n");
                printf("Solution #%d (variant of Unique #%d)\n", solutions_count, unique_id);
                printf("───────────────────────────────────────────────────────────\n");
                print_solution(board, solutions_count);
            }
            
            pthread_mutex_unlock(&print_mutex);
        }
        return;
    }
    
    for (int col = 0; col < n; col++) {
        if (is_safe(row, col)) {
            board[row] = col;
            solve_nqueens(row + 1);
        }
    }
}

// Mutex for work queue access
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
int queue_index = 0;

/**
 * Update and display progress
 */
void update_progress(void) {
    if (!show_progress) return;
    
    pthread_mutex_lock(&progress_mutex);
    work_completed++;
    double percent = (double)work_completed / total_work_items * 100.0;
    
    // Create a simple progress bar
    int bar_length = 30;
    int filled = (int)(percent / 100.0 * bar_length);
    
    // Use carriage return to update in place
    fprintf(stderr, "\r[");
    for (int i = 0; i < bar_length; i++) {
        fprintf(stderr, "%c", i < filled ? '=' : ' ');
    }
    fprintf(stderr, "] %.1f%% (%d/%d)", percent, work_completed, total_work_items);
    fflush(stderr);
    
    pthread_mutex_unlock(&progress_mutex);
}

/**
 * Thread worker function
 * Each thread picks work items from the queue and solves them
 */
void *thread_worker(void *arg) {
    // Each thread gets its own board (thread-local storage)
    board = (int *)malloc(n * sizeof(int));
    
    while (1) {
        // Get next work item from queue
        pthread_mutex_lock(&queue_mutex);
        if (queue_index >= work_queue.size) {
            pthread_mutex_unlock(&queue_mutex);
            break;  // No more work
        }
        
        WorkItem *item = &work_queue.items[queue_index];
        queue_index++;
        
        pthread_mutex_unlock(&queue_mutex);
        
        // Copy the partial board to thread-local board
        memcpy(board, item->board, n * sizeof(int));
        
        // Solve from the parallelization depth
        solve_nqueens(item->depth);
        
        // Update progress
        update_progress();
    }
    
    free(board);
    return NULL;
}

/**
 * Print usage information
 */
void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Solve the N-Queens problem using multi-threaded backtracking with symmetry detection.\n\n");
    printf("OPTIONS:\n");
    printf("  n [N]              Board size (default: 8)\n");
    printf("  --threads NUM      Number of threads to use (default: auto-detect)\n");
    printf("  --quiet            Don't print intermediate solutions, only final summary\n");
    printf("  --progress         Show progress bar during solving\n");
    printf("  --help             Show this help message\n\n");
    printf("EXAMPLES:\n");
    printf("  %s                    # Solve 8-queens with auto-detected threads\n", program_name);
    printf("  %s 10                 # Solve 10-queens\n", program_name);
    printf("  %s 8 --threads 4      # Solve 8-queens using exactly 4 threads\n", program_name);
    printf("  %s 12 --progress      # Solve 12-queens and show progress\n", program_name);
    printf("  %s 12 --threads 8 --quiet --progress  # All options\n", program_name);
}

int main(int argc, char *argv[]) {
    n = 8;
    num_threads = 0;  // 0 means auto-detect
    print_solutions = 1;  // 1 means print solutions
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            print_solutions = 0;
        } else if (strcmp(argv[i], "--progress") == 0 || strcmp(argv[i], "-p") == 0) {
            show_progress = 1;
        } else if (strcmp(argv[i], "--threads") == 0 || strcmp(argv[i], "-t") == 0) {
            if (i + 1 < argc) {
                num_threads = atoi(argv[++i]);
                if (num_threads < 1) {
                    fprintf(stderr, "Error: Thread count must be at least 1\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: --threads requires a number argument\n");
                return 1;
            }
        } else if (argv[i][0] != '-') {
            // Positional argument - board size
            n = atoi(argv[i]);
            if (n < 1) {
                fprintf(stderr, "Error: N must be at least 1\n");
                return 1;
            }
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Detect number of CPU cores
    num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 1) {
        num_cores = 1;
    }
    
    // Use user-specified thread count or auto-detected cores
    int actual_threads = (num_threads > 0) ? num_threads : num_cores;
    
    solution_set.capacity = 1000;
    solution_set.solutions = (char **)malloc(solution_set.capacity * sizeof(char *));
    solution_set.unique_ids = (int *)malloc(solution_set.capacity * sizeof(int));
    solution_set.size = 0;
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  UNIQUE SOLUTIONS (ACCOUNTING FOR SYMMETRY)  QUEENS-%3d    ║\n", n);
    printf("║  Solutions that are the same after rotation or reflection  ║\n");
    printf("║  are counted as one unique solution                        ║\n");
    printf("║  Detected %d CPU core(s)                                   ║\n", num_cores);
    printf("║  Using %d thread(s) | %s intermediate solutions           ║\n", 
           actual_threads, print_solutions ? "Printing" : "Suppressing");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    clock_t start = clock();
    
    // Calculate parallelization depth
    // Higher depth = more granular work items = better load balancing on many cores
    // For most cases, depth 3-4 provides good balance between generation cost and work granularity
    parallelization_depth = (n > 6) ? 4 : (n > 4) ? 3 : 2;
    if (parallelization_depth > n - 1) {
        parallelization_depth = n - 1;
    }
    
    // Initialize work queue
    work_queue.capacity = 10000;
    work_queue.items = (WorkItem *)malloc(work_queue.capacity * sizeof(WorkItem));
    work_queue.size = 0;
    
    // Generate all partial boards up to parallelization depth
    int *partial_board = (int *)malloc(n * sizeof(int));
    memset(partial_board, -1, n * sizeof(int));
    generate_work_queue(0, partial_board);
    free(partial_board);
    
    // Set total work items for progress tracking
    total_work_items = work_queue.size;
    
    printf("║  Parallelization depth: %d | Work items: %d           ║\n", 
           parallelization_depth, work_queue.size);
    if (show_progress) {
        printf("║  Progress tracking: ENABLED                               ║\n");
    }
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    // Create threads
    pthread_t *threads = (pthread_t *)malloc(actual_threads * sizeof(pthread_t));
    int *thread_created = (int *)malloc(actual_threads * sizeof(int));
    memset(thread_created, 0, actual_threads * sizeof(int));
    
    for (int i = 0; i < actual_threads; i++) {
        pthread_create(&threads[i], NULL, thread_worker, NULL);
        thread_created[i] = 1;
    }
    
    // Wait for all created threads to complete
    for (int i = 0; i < actual_threads; i++) {
        if (thread_created[i]) {
            pthread_join(threads[i], NULL);
        }
    }
    
    // Clear progress line if it was shown
    if (show_progress) {
        fprintf(stderr, "\r%-60s\r", "");  // Overwrite progress line with spaces
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("\nTime: %.6f seconds\n", elapsed);
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║ Total solutions found:          %-27d║\n", solutions_count);
    printf("║ Unique solutions (no symmetry): %-27d║\n", unique_count);
    if (solutions_count > 0) {
        printf("║ Reduction: %.1f%%                                           ║\n",
               100.0 * (solutions_count - unique_count) / solutions_count);
    }
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // Cleanup
    for (int i = 0; i < solution_set.size; i++) {
        free(solution_set.solutions[i]);
    }
    free(solution_set.solutions);
    free(solution_set.unique_ids);
    
    // Cleanup work queue
    for (int i = 0; i < work_queue.size; i++) {
        free(work_queue.items[i].board);
    }
    free(work_queue.items);
    
    free(threads);
    free(thread_created);
    
    // Destroy mutexes
    pthread_mutex_destroy(&print_mutex);
    pthread_mutex_destroy(&data_mutex);
    pthread_mutex_destroy(&progress_mutex);
    pthread_mutex_destroy(&queue_mutex);
    
    return 0;
}
