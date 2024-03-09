# Shell Program

This program runs an interactive shell for the user.

## Instructions

1. Compile the program using the following command to create an executable named 'smallsh':
    ```
    gcc --std=gnu99 -o smallsh main.c
    ```
2. Run the program with the following command:
    ```
    ./smallsh
    ```

*Note: Ignore the warning "return making integer from pointer without a cast". The return is a NULL value, and the program works correctly.*

*Requirement: The same directory as the executable must contain 2 header files:*
- `Header.h`
- `prototypes.h`

*User input requirement: Users must enter valid BASH shell syntax. There is no error handling for this.*

## Features

- Syntax: `command [args] [< input] [> output] [&]`
- Input and output file redirection using `dup2()`
- Handles commands `cd`, `status`, `exit` directly
  - `cd` supports relative and absolute paths
  - `status` prints the exit status or termination signal of the last foreground process ran
  - `exit` kills all foreground and background processes and exits the shell program
- Handles all other commands by forking child processes that run `exec()`
  - Command failures set the exit status to 1
  - Child processes always terminate after running the command
- Executes commands in the background (use `&` at the end) or foreground
  - Foreground processes prevent new commands until the child terminates
  - Background processes allow the user to enter new commands as soon as the child forks
  - Terminated background processes print PID and exit status
- Ignores blank lines and comments (lines beginning with `#`)
- `SIGINT` (use CTRL-C) handling
  - The shell (parent process) and all background processes ignore `SIGINT`
  - Foreground processes terminate themselves immediately
- `SIGTSTP` (use CTRL-Z) handling
  - The shell toggles the ability to run background processes (ignoring `&`)
  - All background and foreground child processes ignore `SIGTSTP`
