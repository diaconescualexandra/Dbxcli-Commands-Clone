#include <unistd.h>
#include <fcntl.h>      // pentru O_RDONLY, si altele
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>    // pentru bool
#include <dirent.h>     // pentru DIR
#include <sys/stat.h>   // pentru stat
#include <readline/readline.h>
#include <linux/limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define BUFSIZE	8192
#define MAX_CAPACITY 1e9

char** command;
char shell_prompt[BUFSIZE];

pid_t revs_pid;



void print_command_not_found() {
    printf("%s: command not found\n", command[0]);
    printf("Use \"help\" for available commands.\n");
}

void custom_help() {
    printf("Use dbxcli to quickly interact with your Dropbox, upload/download files, \n");
    printf("manage your team and more. It is easy, scriptable and works on all platforms!\n");
    printf("\nAvailable commands: \n");
    printf("  cp              Copy a file or folder to a different location in the user's Dropbox. If the source path is a folder all its contents will be copied.\n");
    printf("  ls              List files and folders\n");
    printf("  mkdir           Create a new directory\n");
    printf("  mv              Move files\n");
    printf("  restore         Restore files\n");
    printf("  revs            List file revisions\n");
    printf("  rm              Remove files\n");
    printf("  search          Search\n");
    printf("  exit            Normal process termination\n");
    printf("  help            Help about any command\n");

    printf("\nUse [command] --help for more information about a command\n");
}

void read_input(unsigned int *index) {
    // char* str = readline(shell_prompt);
    char* str = readline("dbxcli> ");

    add_history(str);

    char* current_line = strtok(str, " \n");
    while (current_line) {

        // Allocate memory for the current token
        command[*index] = malloc(strlen(current_line) + 1);

        strcpy(command[*index], current_line);
        (*index) += 1;
        current_line = strtok(NULL, " \n");
    }

    // free(command[*index]);
    command[*index] = NULL;
    free(str);
}

void update_prompt() {
    // Update the shell prompt after changing the directory
    // snprintf(shell_prompt, BUFSIZE, "\033[1;35m%s\033[0m:\033[1;36m%s\033[0m $ ", getenv("USER"), getcwd(NULL, 1024));
    snprintf(shell_prompt, BUFSIZE, "%s:%s $ ", getenv("USER"), getcwd(NULL, 1024));
}

void run_cd(unsigned int length) {
    if (length != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", command[0]);
        return;
    }

    const char *path = command[1];

    if (chdir(path) != 0) {
        perror("cd");
        return;
    }

    // Update the shell prompt after changing the directory
    update_prompt();
}

void run_command(char** this_command) {
    pid_t pid = fork();

    if (pid < 0) {
        exit(1);
        // return;
    } else if (pid == 0) {
        setpgid(0, 0);
        if (execvp(this_command[0], this_command) == -1) {
            perror("execvp"), exit(1);
        }
    } else {
        wait(NULL);
    }
}

int is_directory(const char *path) {
    struct stat path_stat;
    
    if (stat(path, &path_stat) != 0) {
        perror("Error checking file or directory");
        return -1; // Indicate error
    }

    // The stat function follows symbolic links and 
    //      provides information about the file or directory
    //      to which the link points.
    // The lstat function is similar to stat but does not 
    //      follow symbolic links. It provides information 
    //      about the link itself, not the target of the link.

    return S_ISDIR(path_stat.st_mode);
}

// void custom_ls(const char *dir_path) {
//     DIR *dir;
//     struct dirent *entry;

//     dir = opendir(dir_path);

//     if(dir == NULL)
//     {
//         perror ("error opening directory");
//         return ;
//     }

//     printf("listing of directory: %s\n", dir_path);

//     while ((entry = readdir(dir)) != NULL) {
//         //         if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
//         //             continue;
//         //         }
//         printf("%s\n", entry->d_name);
//     }

//     closedir(dir);
// }

void custom_ls(unsigned int length) {
    const char *dir_path;

    if (length == 1) {
        dir_path = "."; 
        printf ("file listing:\n") ;// If no directory is specified, use the current directory
    } else if (length == 2) {
        dir_path = command[1];
        printf ("file listing:\n");

        if (is_directory(dir_path) != 1) {
            // fprintf(stderr, "A file was given, please insert a directory\n", command[0]);
            printf(stderr, "A file was given, please insert a directory\n", command[0]);
            return;
        }
    } else {
        // fprintf(stderr, "A file was given, please insert a directory\n", command[0]);
        printf(stderr, "A file was given, please insert a directory\n", command[0]);
        return;
    }

    // Build the command for listing using run_command
    char* ls_args[] = {"ls", dir_path, NULL};

    // Run the ls command
    run_command(ls_args);
}

// long long current_time_us() {
//     struct timeval tv;
//     gettimeofday(&tv, NULL);
//     return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
// }

long long get_file_size(const char *filename) {
    struct stat st;
    
    if (stat(filename, &st) == 0) {
        return (long long)st.st_size;
    } else {
        // Return -1 to indicate an error
        return -1;
    }
}

int custom_cp(unsigned int length) {
    if (length != 3) {
        printf("Usage: custom_cp <source> <target>\n");
        return -1;
    }

    const char* source_path = command[1];
    const char* target_path = command[2];

    // clock_t start_time, end_time;
    // long long file_size = get_file_size(source_path);
    // long long file_size_in_kb = file_size / 1024;
    // long long file_size_in_mb = file_size / (1024 * 1024);

    // Check if source file is a regular file
    if (is_directory(source_path) == 1) {
        printf("Error: Source is a directory. Only files are supported.\n");
        return -1;
    } else if (is_directory(source_path) == -1) {
        return -1; // Error occurred while checking source
    }

    // Check if target is a directory
    if (is_directory(target_path) == 1) {
        printf("Error: Target is a directory. Specify a file for target.\n");
        return -1;
    } else if (is_directory(target_path) == -1) {
        return -1; // Error occurred while checking target
    }

    // Check if source and target are the same
    if (strcmp(source_path, target_path) == 0) {
        printf("Error: Source and target are the same. You can't overwrite the source.\n");
        return -1;
    }

    // Build the command for copying using run_command
    char* cp_args[] = {"cp", source_path, target_path, NULL};

    // start_time = clock();

    // Run the copy command
    run_command(cp_args);

    // end_time = clock();

    // double elapsed_time = ((double) (end_time - start_time) / CLOCKS_PER_SEC);
    // double transfer_speed = file_size / elapsed_time;

    // printf("File size:\n");
    // printf("    %lld bytes\n", file_size);
    // printf("    %lld KB\n", file_size_in_kb);
    // printf("    %lld MB\n", file_size_in_mb);

    // Print transfer speed
    // printf("Transfer Speed: %.2f bytes/s\n", transfer_speed);
    // printf("Transfer Speed: %.2f MB/s\n", transfer_speed / (1024 * 1024));
    // printf("Elapsed Time: %f\n", elapsed_time);

    // Print success message
    printf("Data copied successfully.\n");

    return 0;
}

int custom_mkdir(unsigned int length) {
    if (length != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", command[0]);
        return 1;
    }

    const char *path = command[1];

    char *mkdir_command[] = {"mkdir", path, NULL};

    run_command(mkdir_command);

    // Check if the directory was created successfully
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("Directory '%s' created successfully.\n", path);
        return 0;
    } else {
        perror("Error creating directory");
        return 1;
    }
}

int custom_mv(unsigned int length) {
    if (length != 3) {
        fprintf(stderr, "Usage: %s <source> <destination>\n", command[0]);
        return 1;  // Failure
    }

    const char *source = command[1];
    const char *destination = command[2];

    if (is_directory(source) == 1) {
        // Source is a directory, use mv command
        char *mv_command[] = {"mv", source, destination, NULL};
        run_command(mv_command);
    } else {
        // Source is a file, use rename
        if (rename(source, destination) == 0) {
            printf("'%s' moved to '%s' successfully.\n", source, destination);
            return 0;  // Success
        } else {
            perror("Error moving file");
            return 1;  // Failure
        }
    }

    return 0;  // Success
}


void custom_ls_keyword(const char *dir_path, const char *keyword) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir ( dir_path);

    if(dir == NULL)
    {
        perror ("error opening directory");
        return ;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, keyword) != NULL) {
            printf("%s\n", entry->d_name);
        }
    }
}

void* remove_file_or_directory(void* arg) {
    const char* path = (const char*)arg;

    int type = is_directory(path);

    if (type == -1) {
        // Error occurred, handle as needed
        fprintf(stderr, "Error checking type of %s\n", path);
        exit(1);
    } else if (type) {
        // directory
        char* rmdir_args[] = {"rm", "-r", path, NULL};

        run_command(rmdir_args);
        printf("The directory %s has been removed.\n", path);
    } else {
        // file
        char* rm_args[] = {"rm", path, NULL};
        run_command(rm_args);
        printf("The file %s has been removed.\n", path);
    }

    pthread_exit(NULL);
}

void run_rm(unsigned int length) {

    if (length < 2) {
        fprintf(stderr, "Usage: %s <file_to_be_removed> [<file2> <file3> ...]\n", command[0]);
        return;
    }

    // Create an array to store thread IDs
    pthread_t threads[length - 1];

    for (int i = 1; command[i] != NULL; ++i) {
        // Launch a thread for each file or directory
        if (pthread_create(&threads[i - 1], NULL, remove_file_or_directory, (void*)command[i]) != 0) {
            fprintf(stderr, "Error creating thread for %s\n", command[i]);
            exit(1);
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < length - 1; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Error joining thread\n");
            exit(1);
        }
    }
}

void run_search(unsigned int length) {
    if (length != 2) {
        fprintf(stderr, "Usage: %s <file/directory>\n", command[0]);
        return;
    }

    DIR* dir = opendir(".");

    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    const char *searchTerm = command[1];
    struct dirent *entry;       // file or directory in current directory

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, searchTerm) != NULL) {
            printf("Found: %s\n", entry->d_name);
        }
    }
}

bool search_for_file(const char *searchTerm) {

    DIR* dir = opendir(".");

    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    // file or directory in current directory
    struct dirent *entry;       

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(searchTerm, entry->d_name) == 0) {
            return true;
        }
    }

    return false;
}

void run_restore(unsigned int length) {
    if (length != 2) {
        fprintf(stderr, "Usage: %s <file>\n", command[0]);
        return;
    }

    const char *filename = command[1];
    bool found = search_for_file(filename);

    if (found) {
        printf("%s already exists.\n", filename);
        return;
    }

    char restoreFile[256];
    strcpy(restoreFile, "latest_revision_");
    strcat(restoreFile, filename);

    char source_path[256];
    strcpy(source_path, "Versions/");
    strcat(source_path, restoreFile);

    custom_mv(length);
}

void run_revs(unsigned int length) {
    if (length > 2) {
        fprintf(stderr, "Usage: %s [file]\n", command[0]);
        return;
    } else if (length == 1) {
        const char *directory_path = "Versions";

        char* custom_ls_args[] = {"ls", directory_path, NULL};

        custom_ls(directory_path);
        // run_command(custom_ls_args);
    } else if (length == 2) {
        const char *directory_path = "Versions";
        const char *keyword = command[1];
        custom_ls_keyword(directory_path, keyword);
    }
}

void run_ls(unsigned int length) {
    const char *dir_path;

    if (length == 1) {
        dir_path = "."; 
        printf ("file listing:\n") ;// If no directory is specified, use the current directory
    } else if (length == 2) {
        dir_path = command[1];
        printf ("file listing:\n");

        if (is_directory(dir_path) != 1) {
            fprintf(stderr, "A file was given, please insert a directory\n");
            return;
        }
    } else {
        fprintf(stderr, "A file was given, please insert a directory\n");
        return;
    }

    custom_ls(dir_path);
}

void sigint_handler(int signo) {
    // Handle the SIGINT signal
    kill(-revs_pid, SIGTERM); // Send SIGTERM to the process group
    exit(0);
}

int main(int argc, char *argv[], char* envp[]) {

    signal(SIGINT, sigint_handler);

    // command = (char**)malloc(BUFSIZE * sizeof(char*));
    command = (char**)malloc(BUFSIZE * sizeof(char**));

    pid_t child_pid = fork();
    if (child_pid < 0) {
        perror("fork"), exit(1);
    } else if (child_pid == 0) {
        
        char* revs_args[] = {"./revs", "revs", NULL};
        run_command(revs_args);
        exit(0);
    }

    // update_prompt();

    while(1) {
        unsigned int length = 0;
        read_input(&length);

        if (strcmp(command[0], "exit") == 0) {
            break;
        } else if (strcmp(command[0], "custom_cp") == 0) {
            custom_cp(length);
		} else if (strcmp(command[0], "custom_ls") == 0 ) {
		    custom_ls(length);
            // run_ls(length);
		} else if (strcmp(command[0], "mkdir") == 0) {
		    custom_mkdir(length);
        } else if (strcmp(command[0], "mv") == 0) {
	     	custom_mv(length);
        } else if (strcmp(command[0], "restore") == 0) {
            run_restore(length);
        } else if (strcmp(command[0], "revs") == 0) {
            run_revs(length);
        } else if (strcmp(command[0], "rm") == 0) {
            run_rm(length);
        } else if (strcmp(command[0], "search") == 0) {
            run_search(length);
        } else if (strcmp(command[0], "cd") == 0) {
            run_cd(length);
        } else if (strcmp(command[0], "help") == 0) {
            custom_help();
        } else if (strcmp(command[0], "EMPTY") == 0) {
            printf("something\n");
        } else {
            print_command_not_found();
        }
    }

    for (unsigned int i = 0; i < BUFSIZE; ++i) {
            free(command[i]);
    }

    free(command);
	exit(0);
}
