#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int n;  // Number of queens (set at runtime)
int *board;  // Dynamically allocated board
int solutions_count = 0;

/**
 * Print the current solution with UTF-8 queen character
 */
void print_solution() {
    printf("\nSolution #%d:\n", solutions_count);
    for (int row = 0; row < n; row++) {
        for (int col = 0; col < n; col++) {
            if (board[row] == col) {
                printf("♛ ");
            } else {
                printf("· ");
            }
        }
        printf("\n");
    }
}

/**
 * Check if it's safe to place a queen at board[row][col]
 * We only need to check rows above since we place queens row by row
 */
int is_safe(int row, int col) {
    // Check each previously placed queen
    for (int i = 0; i < row; i++) {
        // Check if there's a queen in the same column
        if (board[i] == col) {
            return 0;
        }
        
        // Check if there's a queen on the diagonal
        // Two queens are on the same diagonal if the absolute difference
        // of their rows equals the absolute difference of their columns
        if (abs(board[i] - col) == abs(i - row)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Solve N-Queens using backtracking
 * Place queens row by row, trying each column
 */
void solve_nqueens(int row) {
    // Base case: all queens placed successfully
    if (row == n) {
        solutions_count++;
        // Only print solutions for small N to avoid massive output
        if (n <= 8) {
            print_solution();
        }
        return;
    }
    
    // Try placing a queen in each column of the current row
    for (int col = 0; col < n; col++) {
        if (is_safe(row, col)) {
            board[row] = col;  // Place queen
            solve_nqueens(row + 1);  // Recurse to next row
            // Backtrack is implicit here as we overwrite board[row] in the next iteration
        }
    }
}

int main(int argc, char *argv[]) {
    // Default to 8 queens if no argument provided
    n = 8;
    
    // Parse command-line argument if provided
    if (argc > 1) {
        n = atoi(argv[1]);
        if (n < 1) {
            fprintf(stderr, "Error: N must be at least 1\n");
            return 1;
        }
    }
    
    // Allocate the board array dynamically
    board = (int *)malloc(n * sizeof(int));
    if (board == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║        %d-QUEENS PROBLEM - FINDING ALL SOLUTIONS           ║\n", n);
    printf("║  Find all ways to place %d queens on a chessboard such     ║\n", n);
    printf("║  that no queen threatens any other queen                  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    clock_t start = clock();
    solve_nqueens(0);
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("Time: %.6f seconds\n", elapsed);
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║ Total solutions found: %-36d║\n", solutions_count);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // Free allocated memory
    free(board);
    
    return 0;
}
