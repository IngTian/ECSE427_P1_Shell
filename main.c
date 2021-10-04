#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

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
        // TODO Complete the pwd function.
    } else if (strcmp(executionFileName, "cd") == 0) {
        // TODO Complete the cd function.
    } else if (strcmp(executionFileName, "exit") == 0) {
        // TODO Complete the exit function.
    } else if (strcmp(executionFileName, "fg") == 0) {
        // TODO Complete the fg function.
    } else if (strcmp(executionFileName, "jobs") == 0) {
        // TODO Complete the jobs function.
    }
}

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
    // TODO Execute a user command, take care of the piping and output redirection if needed.
}

int main(void) {

    // This is a managing block for various child processes.
    struct ChildProcessBlock *backgroundChildProcessBlocks[1000];
    int numberOfChildProcesses = 0;

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
            if (childProcess == 0)
                executeUserCommands(command);
            else if (command->isRunningInBackground) {
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

        // Since the previous command is no longer needed,
        // we are going to destroy it.
        free(command);
    }
    return 0;
}