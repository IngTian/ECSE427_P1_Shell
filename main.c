#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#define STANDARD_OUT_FD 1
#define STANDARD_IN_FD 0
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"
#define True 1
#define False 0

typedef struct user_command {
    char *args[20];
    int isRunningInBackground;
    int commandLength;
} user_command;

typedef struct child_process_block {
    int pid;
    struct user_command *command;
} child_process_block;


const int MAX_PATH_SIZE = 1024;

// This is a managing block for various child processes.
struct child_process_block *g_background_child_processes[1024];
int g_number_of_background_child_processes = 0;
int g_current_running_process_pid = -1;


/**
 * Get user commands from stdin.
 * @param prompt Define the prompt.
 * @return User Command.
 */
struct user_command *read_user_command(char *prompt) {
    struct user_command *result = malloc(sizeof(struct user_command));
    char *line = NULL;

    // Receive Input from user.
    size_t linecap = 0;
    printf("%s", prompt);
    if (getline(&line, &linecap, stdin) <= 0) exit(-1);

    // Check if background is specified.
    char *loc;
    if ((loc = index(line, '&')) != NULL)
        result->isRunningInBackground = 1;
    else
        result->isRunningInBackground = 0;
    if (loc != NULL) *loc = ' ';

    // Parse user inputs.
    char *token;
    int commandLength = 0;
    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++)
            if (token[j] <= 32)
                token[j] = '\0';
        if (strlen(token) > 0)
            result->args[commandLength++] = token;
    }
    result->commandLength = commandLength;

    return result;
}


static void handle_sigint_signal(int sig) {
    printf("SIGINT RECEIVED\n");
    if (g_current_running_process_pid != -1)
        kill(g_current_running_process_pid, SIGKILL);
}

static void handle_sigtstp_signal(int sig) {
    printf("SIGTSTP RECEIVED\n");
}

void get_working_directory(char *dir) {
    size_t size_cap = pathconf(".", _PC_PATH_MAX);
    getcwd(dir, size_cap);
}

/**
 * Execute built in commands in the parent process.
 * @param command A command.
 * @param childProcesses All of the child processes.
 */
void execute_built_in_command(
        struct user_command *command,
        struct child_process_block *childProcesses[]
) {
    char *executionFileName = command->args[0];
    if (strcmp(executionFileName, "pwd") == 0) {
        char buf[MAX_PATH_SIZE];
        get_working_directory(buf);
        printf(WHT "The current directory is: " GRN "%s\n" RESET, buf);
    } else if (strcmp(executionFileName, "cd") == 0) {
        char *dir = command->args[1];
        if (command->commandLength == 1) {
            char buf[MAX_PATH_SIZE];
            get_working_directory(buf);
            printf(WHT "The current directory is: " GRN "%s\n" RESET, buf);
        } else
            chdir(dir);
    } else if (strcmp(executionFileName, "exit") == 0) {
        signal(SIGQUIT, SIG_IGN);
        kill(0, SIGQUIT);
        exit(0);
    } else if (strcmp(executionFileName, "fg") == 0) {
        int index = atoi(command->args[1]);

        if (index < g_number_of_background_child_processes) {
            g_current_running_process_pid = childProcesses[index]->pid;
            childProcesses[index] = NULL;
            printf("The process with INDEX " YEL "%d" RESET " is found, bringing it to foreground.\n", index);
            waitpid(g_current_running_process_pid, NULL, 0);
        } else
            printf(RED "ERROR! Cannot find the specified index %d." RESET, index);

    } else if (strcmp(executionFileName, "jobs") == 0) {
        for (int i = 0; i < g_number_of_background_child_processes; i++) {
            struct child_process_block *process = childProcesses[i];
            if (process == NULL) continue;
            int status = 0;
            if (waitpid(process->pid, &status, WNOHANG) == 0)
                printf("---- BACKGROUND JOB ---- INDEX: " CYN "%d" RESET " PID: " GRN "%d" RESET " COMMAND: %s\n", i,
                       process->pid,
                       process->command->args[0]);
        }
    }
}


/**
 * Decide whether the command is a built-in command.
 * @param executionFileName The command name.
 * @return 1 for yes, and 0 otherwise.
 */
int is_built_in_function(char *executionFileName) {
    return strcmp(executionFileName, "pwd") == 0 ||
           strcmp(executionFileName, "cd") == 0 ||
           strcmp(executionFileName, "exit") == 0 ||
           strcmp(executionFileName, "fg") == 0 ||
           strcmp(executionFileName, "jobs") == 0;
}

/**
 * Executing the user command in another process.
 * @param userCommand A user command, stored in the heap.
 */
void execute_user_command(struct user_command *userCommand) {
    char **argv = userCommand->args;
    int locationOfRedirection = 0, locationOfPiping = 0;
    for (int i = 0; i < userCommand->commandLength; i++)
        if (strcmp(argv[i], ">") == 0)
            locationOfRedirection = i;
        else if (strcmp(argv[i], "|") == 0)
            locationOfPiping = i;

    if (!locationOfRedirection && !locationOfPiping) {
        execvp(argv[0], argv);
    } else if (locationOfRedirection) {
        // Get the file name.
        char *fileAddress = argv[locationOfRedirection + 1];

        // Remove redirection symbols and output file name.
        argv[locationOfRedirection] = NULL;
        argv[locationOfRedirection + 1] = NULL;

        // Open the file, alter FDT, and run commands.
        int outputFileDescriptor = open(fileAddress, O_WRONLY);
        dup2(outputFileDescriptor, STANDARD_OUT_FD);
        execvp(argv[0], argv);
    } else {
        // Separate the two commands.
        char **argv1 = argv, **argv2 = &argv[locationOfPiping + 1];
        argv[locationOfPiping] = NULL;

        // Construct a pipe.
        int p[2];
        pipe(p);

        // Fork a child process to execute the second command.
        int childPid = fork();
        if (childPid != 0) {
            close(p[0]);
            dup2(p[1], STANDARD_OUT_FD);
            execvp(argv1[0], argv1);
        } else {
            close(p[1]);
            dup2(p[0], STANDARD_IN_FD);
            execvp(argv2[0], argv2);
        }
    }
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "DanglingPointer"
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

int main() {

    // Register signal handlers.
    signal(SIGINT, handle_sigint_signal);
    signal(SIGTSTP, handle_sigtstp_signal);

    while (1) {
        g_current_running_process_pid = -1;
        struct user_command *command = read_user_command("\n>> ");
        /*
         *  If the command is a built-in command,
         *  we simply execute it in the main thread.
         *  If not, we create a child process and
         *  execute it.
         */
        if (is_built_in_function(command->args[0])) {
            execute_built_in_command(command, g_background_child_processes);
            free(command);
        } else {
            int child_process_pid = fork();
            if (child_process_pid == 0) {
                execute_user_command(command);
                exit(EXIT_SUCCESS);
            } else if (command->isRunningInBackground) {
                // If the command is meant to run in the background,
                // we shall not intervene with it except take a record
                // of its pid and relevant information.
                struct child_process_block *new_process_block = malloc(sizeof(struct child_process_block));
                new_process_block->command = command;
                new_process_block->pid = child_process_pid;
                g_background_child_processes[g_number_of_background_child_processes++] = new_process_block;
            } else if (!command->isRunningInBackground) {
                // We shall wait for the child process to complete if
                // it is not meant to run in the background.
                g_current_running_process_pid = child_process_pid;
                waitpid(child_process_pid, NULL, 0);
                free(command);
            }
        }

        sleep(1);
    }
}

#pragma clang diagnostic pop
#pragma clang diagnostic pop
