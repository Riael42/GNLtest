#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


/*

  gcc -Wall -Wextra -Werror -D BUFFER_SIZE=1 gnltest.c get_next_line.c get_next_line_utils.c

*/


// Declare your get_next_line function
char *get_next_line(int fd);

// Global variables to track malloc failure logic
static int g_malloc_counter = 0;
static int g_fail_after = -1; // -1 means never fail

// Override the standard malloc
void *malloc(size_t size)
{
    if (g_fail_after >= 0 && g_malloc_counter >= g_fail_after)
    {
        return (NULL);
    }
    g_malloc_counter++;
    
    extern void *__libc_malloc(size_t);
    return __libc_malloc(size);
}

// Helper to parse the -malfail flag
int parse_malfail(const char *arg)
{
    const char *prefix = "-malfail=";
    size_t len = strlen(prefix);

    if (strncmp(arg, prefix, len) == 0)
    {
        return atoi(arg + len);
    }
    return (-1);
}

int main(int argc, char **argv)
{
    int arg_idx = 1;

    if (argc < 2)
    {
        printf("Usage: %s [-malfail=X] file1 file2 ...\n", argv[0]);
        return (1);
    }

    // Check if the first argument is the malloc failure flag
    int fail_val = parse_malfail(argv[arg_idx]);
    if (fail_val >= 0)
    {
        g_fail_after = fail_val;
        arg_idx++; // Move to the first file argument
    }

    int total_files = argc - arg_idx;
    if (total_files <= 0)
    {
        printf("Error: No files provided.\n");
        return (1);
    }

    // Arrays to store FDs, filenames, and tracking status
    int *fds = malloc(sizeof(int) * total_files);
    char **filenames = malloc(sizeof(char *) * total_files);
    int *active = malloc(sizeof(int) * total_files);
    
    int open_files_count = 0;

    // Phase 1: Open all files upfront
    for (int j = 0; j < total_files; j++)
    {
        char *filename = argv[arg_idx + j];
        int fd = open(filename, O_RDONLY);
        if (fd < 0)
        {
            perror(filename);
            active[j] = 0; // Mark as inactive/failed to open
        }
        else
        {
            fds[j] = fd;
            filenames[j] = filename;
            active[j] = 1; // Mark as active
            open_files_count++;
        }
    }

    printf("\n--- Starting Interleaved Reading ---\n\n");

    // Phase 2: Loop round-robin over active FDs until all are fully read (EOF)
    while (open_files_count > 0)
    {
        for (int j = 0; j < total_files; j++)
        {
            if (!active[j])
                continue;

            char *line = get_next_line(fds[j]);
            
            if (line != NULL)
            {
                // Print which file the line came from to easily verify multi-FD logic
                printf("[%s]: %s", filenames[j], line);
                free(line);
            }
            else
            {
                // EOF reached or error for this specific file descriptor
                close(fds[j]);
                active[j] = 0;
                open_files_count--;
                printf("\n--- Reached EOF for: %s ---\n\n", filenames[j]);
            }
        }
    }

    // Clean up our tracking arrays
    free(fds);
    free(filenames);
    free(active);

    printf("--- Total successful malloc calls: %d ---\n", g_malloc_counter);
    return (0);
}


/*

  gcc -Wall -Wextra -Werror -D BUFFER_SIZE=1 gnltest.c get_next_line.c get_next_line_utils.c

*/

