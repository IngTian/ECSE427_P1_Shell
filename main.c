#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

const int MAX_PATH_SIZE = 1024;

struct UserCommand {
    char *args[20];
    int isRunningInBackground;
    int commandLength;
};

struct ChildProcessBlock {
    int pid;
    struct UserCommand *command;
};

/**
 * Get user commands from stdin.
 * @param prompt Define the prompt.
 * @return User Command.
 */
struct UserCommand *getUserCommand(char *prompt) {
    struct UserCommand *result = malloc(sizeof(struct UserCommand));
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

/**
 * Execute built in commands in the parent process.
 * @param command A command.
 * @param childProcesses All of the child processes.
 */
void executeBuiltInCommands(
        struct UserCommand *command,
        struct ChildProcessBlock *childProcesses[],
        int numberOfChildProcesses
) {
    char *executionFileName = command->args[0];
    if (strcmp(executionFileName, "pwd") == 0) {
        char buf[MAX_PATH_SIZE];
        size_t size_cap = pathconf(".", _PC_PATH_MAX);
        getcwd(buf, size_cap);
        printf("The current directory is: %s\n", buf);
    } else if (strcmp(executionFileName, "cd") == 0) {
        chdir(command->args[1]);
    } else if (strcmp(executionFileName, "exit") == 0) {
        exit(0);
    } else if (strcmp(executionFileName, "fg") == 0) {
        // TODO Complete the fg function.
    } else if (strcmp(executionFileName, "jobs") == 0) {
        // TODO Complete the jobs function.
    }
}

/**
 * Decide whether the command is a built-in command.
 * @param executionFileName The command name.
 * @return 1 for yes, and 0 otherwise.
 */
int isBuiltInFunction(char *executionFileName) {
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
void executeUserCommands(struct UserCommand *userCommand) {
    char **argv = userCommand->args;
    int locationOfRedirection = 0, locationOfPiping = 0;
    for (int i = 0; i < userCommand->commandLength; i++)
        if (strcmp(argv[i], ">") == 0)
            locationOfRedirection = i;
        else if (strcmp(argv[i], "|") == 0)
            locationOfPiping = i;

    printf("HAS REDIRECTION: %d\n", locationOfRedirection);
    printf("HAS PIPING: %d\n", locationOfPiping);

    if (!locationOfRedirection && !locationOfPiping) {
        execvp(argv[0], argv);
    } else if (locationOfRedirection) {
        // Get the file name.
        char *fileAddress = argv[locationOfRedirection + 1];

        // Remove redirection symbols and output file name.
        argv[locationOfRedirection] = NULL;
        argv[locationOfRedirection + 1] = NULL;

        // Open the file, alter FDT, and run commands.
        printf("Opening File Address: %s\n", fileAddress);
        int outputFileDescriptor = open(fileAddress, O_WRONLY | O_APPEND);
        printf("The File Descriptor: %d\n", outputFileDescriptor);
        dup2(outputFileDescriptor, 1);
        execvp(argv[0], argv);
    } else if (locationOfPiping) {
        // Separate the two commands.
        char **argv1 = argv, **argv2 = &argv[locationOfPiping + 1];
        argv[locationOfPiping] = NULL;

        // Construct a pipe.
        int p[2];
        pipe(p);

        char MSG[20] = "Hello World!";

        printf("Pipes Created: p0-%d p1-%d\n", p[0], p[1]);

        // Fork a child process to execute the second command.
        int childPid = fork();
        if (childPid != 0) {
            close(p[0]);
            dup2(p[1], 1);
            execvp(argv1[0], argv1);
            close(p[1]);
            wait(NULL);
        } else {
            close(p[1]);
            dup2(p[0], 0);
            execvp(argv2[0], argv2);
            close(p[0]);
        }
    }
}

int main() {

    // This is a managing block for various child processes.
    struct ChildProcessBlock *backgroundChildProcessBlocks[1000];
    int numberOfChildProcesses = 0;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (1) {
        struct UserCommand *command = getUserCommand("\n>> ");
        printf("COMMAND RECEIVED: %s\nCOMMAND LENGTH: %d\nCOMMAND IN BACKGROUND: %d\n", *command->args,
               command->commandLength, command->isRunningInBackground);
        /*
         *  If the command is a built-in command,
         *  we simply execute it in the main thread.
         *  If not, we create a child process and
         *  execute it.
         */
        if (isBuiltInFunction(command->args[0]))
            executeBuiltInCommands(command, backgroundChildProcessBlocks, numberOfChildProcesses);
        else {
            int *childProcessStatus = 0, childProcess = fork();
            if (childProcess == 0) {
                printf("Executing Commands...\n");
                executeUserCommands(command);
                exit(0);
            } else if (command->isRunningInBackground) {
                // If the command is meant to run in the background,
                // we shall not intervene with it except take a record
                // of its pid and relevant information.
                struct ChildProcessBlock *aNewBackgroundChildProcess = malloc(sizeof(struct ChildProcessBlock));
                aNewBackgroundChildProcess->command = command;
                aNewBackgroundChildProcess->pid = childProcess;
                backgroundChildProcessBlocks[numberOfChildProcesses++] = aNewBackgroundChildProcess;
            } else if (!command->isRunningInBackground)
                // We shall wait for the child process to complete if
                // it is not meant to run in the background.
                waitpid(childProcess, childProcessStatus, 0);
        }

        free(command);
    }
#pragma clang diagnostic pop
    return 0;
}