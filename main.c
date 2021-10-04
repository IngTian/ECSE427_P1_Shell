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
void executeBuiltInCommands(struct UserCommand *command, struct ChildProcessBlock* childProcesses[]) {
    
}

int main(void) {
    while (1) {
        struct UserCommand *command = getUserCommand("\n>> ");
        printf("COMMAND RECEIVED: %s\nCOMMAND LENGTH: %d\nCOMMAND IN BACKGROUND: %d\n", *command->args,
               command->commandLength, command->isRunningInBackground);
    }
    return 0;
}