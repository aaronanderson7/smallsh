// Name: Aaron Anderson
// Class: CS374
// Assignment: Smallsh
// Due Date: 02/18/2024

// CODE CITATION and NOTE FOR PROFESSOR GAMBORD
// Date: 02/12/2024
// I am repeating this class from Fall 2023 and working on my previous submission from last semester.


#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

char *words[MAX_WORDS];                        // char array to hold each line of getline( *smallsh commands ) - initialized before wordsplit
size_t wordsplit(char const *line);
char * expand(char const *word);

// char *clean_commands[MAX_WORDS];              // char array to hold only clean commands and arguments
char *clean_commands[MAX_WORDS] = {0};        

pid_t recent_background_process = 777;        // global variable to hold recent background child pid - $!

int recent_foreground_process = 777;          // global variable to hold recent foreground child status - $?

int interactive_mode = 1;                     // interactive mode flag. 

// Signal Handler for SIGINT: The SIGINT signal shall be ignored at all times except when reading 
// a line of input in which it does nothing. 
void handle_SIGINT(int signo) {}

// Sig action structs to hold the dispositions of different signals (for handling)
// struct sigaction SIGINT_action = {0},  SIGINT_old_action = {0}, SIGTSTP_action = {0}, SIGTSTP_old_action = {0}, ignore_action = {0};

    
int main(int argc, char *argv[])
{
  // Read from the input file (non-interactive) or from stdin (interactive - in which signal handling must be performed)
  FILE *input = stdin;
  char *input_fn = "(stdin)";
  if (argc == 2) {                            // non-interactive mode. 
    input_fn = argv[1];
    input = fopen(input_fn, "re");            // read the file provided with CLO_EXEC ("e")
    if (!input) err(1, "%s", input_fn);
    interactive_mode = 0;                     // turn interactive mode off. 
  } else if (argc > 2) {
    errx(1, "Too many arguments");
  } 

  // variables for getline.
  char *line = NULL;
  size_t n = 0; 
  int i = 1;

  // declare structs
  struct sigaction SIGINT_action = {0},  SIGINT_old_action = {0}, SIGTSTP_action = {0}, SIGTSTP_old_action = {0}, ignore_action = {0};

  for (;;) {
    // prompt
    /* TODO: Manage background processes */
    // If background flag is set - perform non-blocking waitpid on all background processes in same group id as smallsh (opt 0 in waitpid).
    // Must also check for WNTRACED OR WNOHANG with a pipe here.
    int backChildStatus;
    pid_t backChildPid;

    // Code citation inspired from waitpid(2) man pages
    do {
      backChildPid = waitpid(0, &backChildStatus, WNOHANG | WUNTRACED);
      if (backChildPid == 0 || backChildPid == -1) {
        continue;    
      }
      if (WIFEXITED(backChildStatus)) {
        fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) backChildPid, WEXITSTATUS(backChildStatus));
      } 
      else if (WIFSIGNALED(backChildStatus)) {
        fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) backChildPid, WTERMSIG(backChildStatus));
      }
      else if (WIFSTOPPED(backChildStatus)) {
        kill(backChildPid, SIGCONT);
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) backChildPid);
      }
    } while (backChildPid > 0);

    /* TODO: prompt (below is interactive mode) */
    // Smallsh can be invoked with: one argument (provided file) or no argument (interactive)
    // Print the value of whatever p is to stderr, if nothing - then print nothing. 
    if (input == stdin) {     
      char *p = getenv("PS1");
      if (p == NULL) {
      } else {
        fprintf(stderr, "%s", p);
      }
    }

    // CLEAR OUT WORDS AND CLEANWORDS ARRAYS 
    memset(words, NULL, MAX_WORDS);
    memset(clean_commands, NULL, MAX_WORDS);

    if (interactive_mode) {    
      // Ignore action struct - sa_handler to IGNORE
      ignore_action.sa_handler = SIG_IGN;

      SIGINT_old_action.sa_handler = SIG_DFL;
      SIGTSTP_old_action.sa_handler = SIG_DFL;

      // ignore the other signals.
      sigaction(SIGTSTP, &ignore_action, &SIGTSTP_old_action);
      sigaction(SIGINT, &ignore_action, &SIGINT_old_action);

      // Right before getline we have to register SIGINT to the signal handler that does nothing. 
      SIGINT_action.sa_handler = handle_SIGINT;
      //sigaction(SIGINT, &SIGINT_action, &SIGINT_old_action);  
      
      
      sigaction(SIGINT, &SIGINT_action, NULL);      // this line of code changes test 20
      
      // getline: get a line of data from the user. (returns -1 if not successful)
      ssize_t line_len = getline(&line, &n, input);    
      if (line_len < 0) {
        // if end of file - break 
        if (feof(input)) {                       // end of file check
          // Maybe do some memory checking. 
          // if end of file should exit or break?
          // maybe move errno below clearerr.... errno after clearerr?
          break;                                                     
        } 
        else {                                   // signal interrupt?
        // if EINTP, interrupted - then clearerr, errno
          if (errno = EINTR) {
            clearerr(input);   // clearerr (stdin);
            errno = 0;
            fprintf(stderr, "%c", '\n');
            sigaction(SIGINT, &ignore_action, NULL);
            continue;
          }
        }
      }  
    } else {                                     // Non-interactive mode getline()
      // getline: get a line of data from the user. (returns -1 if not successful)
      ssize_t line_len = getline(&line, &n, input);
      if (line_len < 0) {      
        if (feof(input)) {                      // end of file check
          break;                                // break the for loop because we have reached the end of input. 
        } 
        else {                                  // signal interrupt?
          clearerr(input);
          errno = 0;
          exit(1);
        }
      }
    }

    if (interactive_mode) {
      sigaction(SIGINT, &ignore_action, NULL); 
    }

    size_t nwords = wordsplit(line);           // nwords = number of words in words[arr]
    if (nwords == 0) {
      continue;
    }
    for (size_t i = 0; i < nwords; ++i) {
      clearerr(input);
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
    }
    
    // Iterate through the words arr and filter out any instances of >, <, >> and &.
    int cwords = 0;                                    // number of clean words and commands.
    
    // Moving the background flag here so it doesn't stay global throughout the program
    int background_flag = 0;
    
    for (size_t i = 0; i < nwords; ++i) {
      if ((strcmp(words[i], "<")) == 0) {
        ++i;            // ignore the redirection operator + file path
        continue;
      } 
      if ((strcmp(words[i], ">")) == 0) {
        ++i;
        continue;
      }
      if ((strcmp(words[i], ">>")) == 0) {  
        ++i; 
        continue;
      }
      if ((strcmp(words[i], "&")) == 0) { 
        ++i;
        background_flag = 1; 
        break;                          
      }
      clean_commands[cwords] = words[i];
      ++cwords;
    }

    // Execute Built-in and Non-built-in functions.
    // First check if there are no command arguments
    if (cwords == 0) {
      // Quietly return and start for loop over again. 
      continue;
    } 
    // The cd built-in takes one argument, if not provided the argument is implied to be the expansion of cd $HOME
    // error if more than one argument provided.
    else if ((strcmp(clean_commands[0], "cd")) == 0) {
      if (cwords > 2) {
        perror("More than 1 argument provided for cd command.");
        exit(1);
      } else if (cwords == 1) {
        chdir(getenv("HOME"));
        continue;
      } else if (cwords == 2) {
        chdir(clean_commands[1]);
        continue;
      }
    }
  
    // The exit built-in command takes one argument, if not provided shall be the expansion of $? (last exit comamnd) - value of waitpid.
    // error if more than one argument provided or argument provided is not an integer.
    else if ((strcmp(clean_commands[0], "exit")) == 0) {
      if (cwords > 2) {
        perror("More than 1 argument provided for exit command.");
        exit(2);
      } else if (cwords == 1) {
        // exit with last value of fg process - $?
        // exit(recent_foreground_process);
        if (recent_foreground_process == 777) {
          exit(0);
        } else {
          exit(recent_foreground_process);
        }
      } else if (cwords == 2) { 
        // first check if exit argument code (char) provided is truly an int.
        int not_integer = 0;
        int i = 0;
        if (clean_commands[1][0] == '-') i = 1;
        for (; clean_commands[1][i] != 0; ++i) {
          // if exit command argument > 9 or < 0
          if (!isdigit(clean_commands[1][i])) not_integer = 1;
        }
        if (not_integer == 0) {
          int exit_code = atoi(clean_commands[1]);
          exit(exit_code);
        } else {
          perror("Argument provided with exit command is not an integer.");
          exit(2);
        }
      }
    }

    // Execute non-built in functions.
    else {
      int childStatus;

      // Fork a new proces
      pid_t childPid, wpid;
      childPid = fork();
      if (childPid == -1) {
          perror("fork()\n");
          fprintf(stderr, "Error in the child process.");
          exit(EXIT_FAILURE);   // changed from 1
      } else if (childPid == 0) {
          // Check each word in words for <, >, or << redirection operators (read, write, or append).
          // The word following it shall be interpreted as the path to a file. 
          // in child if input == stdin 
          
          if (interactive_mode) {
            sigaction(SIGTSTP, &SIGTSTP_old_action, NULL);
            sigaction(SIGINT, &SIGINT_old_action, NULL);
          }
          
          for (size_t i = 0; i < nwords; i++) {
            if ((strcmp(words[i], "<")) == 0) {
              int FD = open(words[i + 1], O_RDONLY);
              int result = dup2(FD, 0);                 // 0 is standard code for stdin
              if (result == -1) {
                perror("There is an issue with dup2.");
                exit(1); 
              }
              close(FD); 
            } else if ((strcmp(words[i], ">")) == 0) {
              int FD = open(words[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);                  
              int result = dup2(FD, 1);                // 1 is standard code for stdout.
              if (result == -1) {
                perror("There is an issue with dup2.");
                exit(1);
              }
              close(FD);
            } 
            // FD = open(words[i + 1], O_RDWR | O_APPEND | O_CREAT | O_TRUNC, 0777)
            // Maybe an issue with O_RDWR
            else if ((strcmp(words[i], ">>")) == 0) {
              int FD = open(words[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0777);
              int result = dup2(FD, 1);
              if (result == -1) {
                perror("There is an issue with dup2.");
                exit(1);
              }
              close(FD);
            } 
          }
          // execute the commands.
          execvp(clean_commands[0], clean_commands);
          perror("There is an error with execvp");
          exit(2);
      } else {
          // If background flag is set, run program in background and just update $! value from old ChildPid and go back to start of loop.
          // ONLY HANDLE THE FOREGROUND PROCESS HERE 
          // If background process - OLD ChildPid (NOT FROM NEW WAITPID) is saved.
          // MAYBE - get rid of if else, and just continually check. 

          if (background_flag) {
            recent_background_process = childPid;
          } else {
            // If no background operator and non-built in function - blocking wait on fg child process. 
            // int childStatus;
            wpid = waitpid(childPid, &childStatus, 0 | WUNTRACED);
            if (WIFEXITED(childStatus)) {
              recent_foreground_process = WEXITSTATUS(childStatus);
            } 
            // NEED TO CHECK FOR WSIGNALED - [128] + [n] which is the signal 
            else if (WIFSIGNALED(childStatus)) {
              recent_foreground_process = (128 + (WTERMSIG(childStatus)));
            } 
            else if (WIFSTOPPED(childStatus)) {
              kill(childPid, SIGCONT);
              fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) childPid);
              recent_background_process = childPid;
            }
          } 
       }
    }
  }
  return 0;
}


char *words[MAX_WORDS] = {0};

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;

  char const *c = line;
  for (;*c && isspace(*c); ++c); /* discard leading space */

  for (; *c;) {
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
    for (;*c && !isspace(*c); ++c) {
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
      if (!tmp) err(1, "realloc");
      words[wind] = tmp;
      words[wind][wlen++] = *c; 
      words[wind][wlen] = '\0';
    }
    ++wind;
    wlen = 0;
    for (;*c && isspace(*c); ++c);
  }
  return wind;
}


/* Find next instance of a parameter within a word. Sets
 * start and end pointers to the start and end of the parameter
 * token.
 */
char
param_scan(char const *word, char const **start, char const **end)
{
  static char const *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = 0;
  *end = 0;
  for (char const *s = word; *s && !ret; ++s) {
    s = strchr(s, '$');
    if (!s) break;
    switch (s[1]) {
    case '$':
    case '!':
    case '?':
      ret = s[1];
      *start = s;
      *end = s + 2;
      break;
    case '{':;
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = s[1];
        *start = s;
        *end = e + 1;
      }
      break;
    }
  }
  prev = *end;
  return ret;
}


/* Simple string-builder function. Builds up a base string by appending supplied strings/character ranges to it.
 */
char *
build_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *base = 0;

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.
   * Returns a newly allocated string that the caller must free.
   */
  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  void *tmp = realloc(base, newsize);
  if (!tmp) err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n); // mempy copies n characters from src to destination 
  base_len += n;
  base[base_len] = '\0';

  return base;
}

/*  
 * Expands all instances of $! $$ $? and ${param} in a string. Returns a newly allocated string that the caller must free.
 */
char *
expand(char const *word)
{
  char const *pos = word;
  char *start, *end;
  char c = param_scan(pos, &start, &end);
  build_str(NULL, NULL);
  build_str(pos, start);
  while (c) {
    if (c == '!') { 
      // Replace with the process ID of most recent background process (waiting), default to an empty string ("") if no background process available yet. 
      if (recent_background_process == 777) {
        build_str("", NULL);
      } else {
        char b_pid[1024];
        snprintf(b_pid, 1024, "%d", recent_background_process);
        build_str(b_pid, NULL);
      }
    } else if (c == '$') {
      // Replace with the process ID of smallsh process.
      pid_t pid = getpid();
      char s_pid[1024];
      snprintf(s_pid, 1024, "%d", pid); 
      build_str(s_pid, NULL);

    } else if (c == '?') {
      // Replace with the exit status of the last foreground command (waiting), defaults to 0 ("0").
      if (recent_foreground_process == 777){
        build_str("0", NULL);
      } else {
        char f_pid[1024];
        snprintf(f_pid, 1024, "%d", recent_foreground_process);
        build_str(f_pid, NULL);

      }
    } else if (c == '{') {
      // Pointer arithmetic: len = (end - 1) - (start + 2) (Use the start and end pointers to isolate the part in the middle)
      // Create a small null terminated buffer, and then use strncpy with start / end pointers to get the parameter.
      // Then you can pass that string in to getenv(), which will return a pointer to a string that you can give to build_str.
      char arr[1024] = {'\0'};
      strncpy(arr, (start + 2), ((end - 1) - (start + 2)));
      char *p = getenv(arr);
      // Check if process is NULL, then expand as empty string.
      if (p == NULL) {
        build_str("", NULL);
      } else {
        build_str(p, NULL);
      }
    }
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);
  }
  return build_str(start, NULL);
}

