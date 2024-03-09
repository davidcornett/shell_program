This program runs an interactive shell for the user.  

1) Compile program using the following to create an executable 'smallsh':
        gcc --std=gnu99 -o smallsh main.c

2) Run program with the following:
        ./smallsh

Note: ignore warning on return making interger from pointer without a cast.  Return is NULL value, and program works correctly.

Requirement: same directory of executable must contain 2 header files
        1. Header.h
        2. prototypes.h

User input requirement: user must enter valid BASH shell syntax.  There is no error-handling for this.
