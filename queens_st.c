#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int n;
int *board;
int solutions_count = 0;
int unique_count = 0;

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
 * Check if it's safe to place a queen
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
        solutions_count++;
        
        // Get canonical form
        char *canonical = get_canonical_form(board);
        
        // Check if we've seen this canonical form before
        int unique_id = get_unique_id(canonical);
        if (unique_id == -1) {
            // This is a new unique solution
            unique_count++;
            add_to_set(canonical, unique_count);
            
            printf("\n═══════════════════════════════════════════════════════════\n");
            printf("Solution #%d (UNIQUE #%d)\n", solutions_count, unique_count);
            printf("═══════════════════════════════════════════════════════════\n");
            print_solution(board, solutions_count);
        } else {
            // This is a symmetric duplicate of an existing unique solution
            free(canonical);
            
            if (n <= 8) {
                printf("\n───────────────────────────────────────────────────────────\n");
                printf("Solution #%d (variant of Unique #%d)\n", solutions_count, unique_id);
                printf("───────────────────────────────────────────────────────────\n");
                print_solution(board, solutions_count);
            }
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

int main(int argc, char *argv[]) {
    n = 8;
    
    if (argc > 1) {
        n = atoi(argv[1]);
        if (n < 1) {
            fprintf(stderr, "Error: N must be at least 1\n");
            return 1;
        }
    }
    
    board = (int *)malloc(n * sizeof(int));
    solution_set.capacity = 1000;
    solution_set.solutions = (char **)malloc(solution_set.capacity * sizeof(char *));
    solution_set.unique_ids = (int *)malloc(solution_set.capacity * sizeof(int));
    solution_set.size = 0;
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  UNIQUE SOLUTIONS (ACCOUNTING FOR SYMMETRY)  QUEENS-%3d    ║\n", n);
    printf("║  Solutions that are the same after rotation or reflection  ║\n");
    printf("║  are counted as one unique solution                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    clock_t start = clock();
    solve_nqueens(0);
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("Time: %.6f seconds\n", elapsed);
    
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
    free(board);
    
    return 0;
}
