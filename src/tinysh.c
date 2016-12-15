/* *
 * tinysh.c
 *
 * A tiny UNIX shell.
 *
 * NOTES:
 *   - In accordance with the UNIX 98 standard and the convention set by glibc, we assume that
 *     malloc, calloc, and realloc set errno to ENOMEM upon failure.
 *
 *  Copyright (C) 2016 Clark Zinzow <clarkzinzow@gmail.com>
 *
 *  Distributed under terms of the MIT license.
 * */


#include "tinysh.h"
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>

#define DEFAULT_PATH_CAPACITY   5
#define DEFAULT_TOKENS_CAPACITY 3
#define TOKEN_FACTOR_HEURISTIC  4

#define READ_END 0
#define WRITE_END 1

static char **path;
static int path_flag;
static int verbose_flag;
static int saved_stdout;  // Saved stdout file descriptor.
static int stdout_flag;  // 1 if stdout has been saved, 0 if not.
// TODO:  Add static context struct for stateful verbose mode.

/* *
 * Main function.  Handles program argument processing.  The core shell driving takes place
 * in the function "driver".
 * */
int main(int argc, char *argv[]) {
  int option_index, c;
  // Long options struct for getopt_long.
  struct option long_options[] = {
    {"path", required_argument, &path_flag, 1},
    {"verbose", no_argument, &verbose_flag, 1},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  // Disabling line buffering helps provide correct output ordering.
  setvbuf(stdout, 0, _IONBF, 0);
  stdout_flag = 0;

  // Option processing.
  while((c = getopt_long(argc, argv, "p:hv", long_options, &option_index)) != -1) {
    switch(c) {
      // Option sets a flag.
      case 0:
        // Verify that option sets a flag.
        if(long_options[option_index].flag != 0)
          break;
        // Path flag set, process path option.
        if(path_flag) {
          // Optarg should be set.
          if(optarg) {
            // Set the path.
            if(set_path(optarg) == -1) {
              path_flag = 0;
            }
          }
          else {
            // Won't be reached unless we changed -p arg to "optional".
            printf("Please provide a path file.\n");
            usage(argv[0]);
            exit(EXIT_FAILURE);
          }
        }
        else if(verbose_flag) {
          printf("Running in verbose mode.\n");
        }
        else {
          fprintf(stderr, "Error processing shell arguments.\n");
          usage(argv[0]);
          exit(EXIT_FAILURE);
        }
        break;

      // Help short option.
      case 'h':
        display_help(argv[0]);
        exit(EXIT_SUCCESS);
        break;

      // Path short option.
      case 'p':
        path_flag = 1;
        // Optarg should be set.
        if(optarg) {
          // Set the path.
          if(set_path(optarg) == -1) {
            path_flag = 0;
          }
        }
        else {
          // Won't be reached unless we changed -p arg to "optional".
          printf("Please provide a path file.\n");
          usage(argv[0]);
          exit(EXIT_FAILURE);
        }
        break;

      case 'v':
        verbose_flag = 1;
        printf("Running in verbose mode.\n");
        break;

      // Unrecognized option character or missing option argument.
      case '?':
        if(optopt && (optopt == 'p')) {
          printf("Please provide a path file when using the path option.\n");
        }
        usage(argv[0]);
        exit(EXIT_FAILURE);
        break;  // Shouldn't be reached.

      default:
        fprintf(stderr, "Error, shouldn't reach the default case in switch statement.\n");
        exit(EXIT_FAILURE);
    }
  }

  // Pass off to shell driver.
  if(driver() == -1) {
    return EXIT_FAILURE;  
  }
  // If reached, user has exited the shell.
  return EXIT_SUCCESS;
}

/* *
 * Sets the path for the shell according to the user-provided path file, or uses the default
 * path defined by the user's environment if there are any errors in processing said path file.
 * This function assumes that there is one path per line in the provided file, starting on the
 * first line.  The only delimiter between paths shoud be a newline.
 *
 * The path file should be provided as an argument when opening the shell; e.g.,
 *
 * tinysh -p /path/to/path/file
 *
 * or
 *
 * tinysh --path=/path/to/path/file
 * */
int set_path(char *file_path) {
  size_t ind;
  size_t num_paths;
  size_t capacity;
  /* ssize_t *num_chars; */
  ssize_t num_chars;
  FILE *fp;
  // Attempt to open the file.
  if((fp = fopen(file_path, "r")) == NULL) {
    // If the user created a path file and we can't open it, print error information.
    if(errno != ENOENT) {
      perror("Unable to open .path file.");
    }
    // Otherwise, opt for the default path.
    path_flag = 0;
    return -1;
  }
  else {
    // Succeeded in opening the file.
    printf("Obtaining path from the following file: %s", file_path);
    capacity = DEFAULT_PATH_CAPACITY;
    // Allocate space for DEFAULT_PATH_CAPACITY path strings.
    if((path = calloc(capacity, sizeof(*path))) == NULL) {
      perror("Error allocating memory for path.");
      path_flag = 0;
      fclose(fp);
      return -1;
    }
    /* // Allocate space for DEFAULT_PATH_CAPACITY integers, each representing the number of */
    /* // characters in a particular path. */
    /* // */
    /* // TODO:  Determine if needed. */
    /* if((num_chars = calloc(capacity, sizeof(*num_chars))) == NULL) { */
    /*   perror("Error allocating memory for path lengths."); */
    /*   free(path); */
    /*   path_flag = 0; */
    /*   fclose(fp); */
    /*   return 0; */
    /* } */

    num_paths = 0;
    ind = 0;
    while((num_chars = getline(&path[ind], &num_paths, fp)) != -1) {
      if(++ind == capacity) {
        if((path = realloc(path, (capacity *= 2) * sizeof(*path))) == NULL) {
          perror("Error reallocating memory for path.");
          while(path && *path)
            free(*path++);
          free(path);
          /* free(num_chars); */
          path_flag = 0;
          fclose(fp);
          return -1;
        }
        else {
          memset(&path[ind], 0, capacity - ind - 1);
        }
        /* if((num_chars = realloc(num_chars, capacity * sizeof(*num_chars))) == NULL) { */
        /*   perror("Error reallocating memory for path lengths."); */
        /*   while(path && *path) */
        /*     free(*path++); */
        /*   free(path); */
        /*   free(num_chars); */
        /*   path_flag = 0; */
        /*   fclose(fp); */
        /*   return 0; */
        /* } */
        /* else { */
        /*   memset(&num_chars[ind], 0, capacity - ind - 1); */
        /* } */
      }
      num_paths = 0;
    }
    // Close the file.
    fclose(fp);
    return 0;
  }
}

/* *
 * The main shell driver.
 * */
int driver() {
  /* int size;                  // Holds the number of commands. */
  size_t input_size;            // Number of bytes in the input buffer.
  size_t num_cmds;              // Number of commands.
  ssize_t chars_read;           // Number of characters read by getline.
  int exit_flag;                // Exit flag is set to 1 if we receive the "exit" command.
  int command_status;           // Status indicating the successfulness of the command.
  /* CmdList *cmd_list;            // Struct to contain list of commands and number of commmands. */
  char *input;                  // Holds the commands provided by the user.
  char **cmds;                  // Holds the list of commands.
  char **temp;
  const char *delim = " \t\n";  // Command and argument delimiters.
  if(!path_flag) {
    printf("Using the path defined by your environment.\n");
  }
  else {
    printf("Using the path defined in the provided path file.\n");
  }
  // Allocates memory for the CmdList struct (defined in tinysh/include/tinysh.h)
  /* if((cmd_list = malloc(sizeof(CmdList))) == NULL) { */
  /*   perror("Error allocating memory."); */
  /*   exit(EXIT_FAILURE); */
  /* } */
  /* if(verbose_flag) */
  /*   printf("Dynamically allocating memory for the list of commands and arguments provided.\n"); */

  input = NULL;
  input_size = 0;
  exit_flag = 0;  // Exit command flag is initiall not set.
  command_status = 1;
  while(!exit_flag) {
    printf("tinysh> ");  // Prompt.

    // Reads in a line of commands from the user, storing the commands in input and the allocated
    // size in size.
    if((chars_read = getline(&input, &input_size, stdin)) < 0) {
      free(input);
      input = NULL;
      input_size = 0;
      if(!feof(stdin)) {
        perror("Error reading user commands from standard input.");
        return -1;
      }
      // At this point, we've encountered an EOF signal from stdin (i.e. CTRL + D on Linux.)
      // Standard procedure here is to exit with success.
      if(verbose_flag)
        printf("\nEncountered EOF, it looks like you pressed CTRL + D.\nExiting now...\n\n");
      exit_flag = 1;
      command_status = 1;
      continue;
    }
    
    // We should have that chars_read >= 0 here due to the previous check.  Set an initial
    // estimte for num_cmds to be chars_read.
    num_cmds = chars_read >= 0 ? (size_t) chars_read : 0;
    
    // Get the command list and the number of commands.
    cmds = tokenizer(input, delim, &num_cmds);
    free(input);

    // If no commands are provided, reprompt the user.
    if((cmds == NULL) || (cmds[0] == NULL)) {
      input = NULL;
      input_size = 0;
      command_status = 0;
      if(cmds != NULL)
        free(cmds);
      continue;
    }

    if(verbose_flag)
      printf("\n");

    // Dispatch to the correct command handler based on the first command.
    if(strcmp(cmds[0], "exit") == 0) {
      exit_flag = 1;
      command_status = 0;
    }
    else if(strcmp(cmds[0], "verbose") == 0) {
      verbose_flag = 1;
      command_status = 0;
    }
    else if(strcmp(cmds[0], "brief") == 0) {
      verbose_flag = 0;
      command_status = 0;
    }
    else if(strcmp(cmds[0], "pwd") == 0) {
      command_status = pwd_handle(cmds, num_cmds);
    }
    else if(strcmp(cmds[0], "cd") == 0) {
      command_status = cd_handle(cmds, num_cmds);
    }
    else {
      command_status = exec_dispatch(cmds, num_cmds);
    }

    if(verbose_flag && !exit_flag) {
      printf("\n");
      if(command_status == -1) {
        printf("Previous command failed.\n\n");
      }
      else {
        printf("Previous command was successful.\n\n");
      }
    }
    
    temp = cmds;
    // Free each command in the command list.
    while(temp && *temp) {
      free(*temp++);
    }
    // Free the command list itself.
    free(cmds);
    input = NULL;
    input_size = 0;
  }

  // Exit flag must have been set, so we are exiting now.
  printf("Exiting now.  Thanks for using tinysh!\n");

  return 0;
}

/* *
 * Tokenizer with the following features:
 *   - Thread-safe
 *   - Does not modify the input string
 *   - Populates cmd_list with a dynamically allocated, null-terminated list of strings
 *     and the number of said strings.
 * */
char** tokenizer(const char *input, const char *delim, size_t *tok_num) {
  char **tokens;       // Tokens to be returned.
  char *str;           // Duplicate of the input string.
  char *tok;           // Token string to hold the tokens returned by strtok_r.
  char *context;       // Context string to be passed to strtok_r.
  size_t tok_used = 0; // Number of tokens used.
  size_t capacity;

  // If supplied capacity is not positive, set to the default capacity.
  capacity = *tok_num > 0 ? (*tok_num / TOKEN_FACTOR_HEURISTIC) + 1 : DEFAULT_TOKENS_CAPACITY;
  
  // Duplicate the input string so the provided input string is not modified.
  if((str = strdup(input)) == NULL) {
    perror("Error allocating memory for input string copy.");
    exit(EXIT_FAILURE);
  }
  // Set context equal to the input string.
  context = str;
  // Allocate space for (capacity) number of commands in the token list.
  if((tokens = malloc(capacity * sizeof(*tokens))) == NULL) {
    perror("Error allocating memory.");
    free(str);
    exit(EXIT_FAILURE);
  }

  // Obtain tokens by calling strtok_r until NULL is returned.
  //
  // NOTE:  By providing context as both the input string and the save_ptr, after each strtok_r
  // call context will point to the first nondelimiter byte after the last token.  Therefore,
  // although strtok_r will register the non-NULL context as a new input string on each call, at
  // which point it will assign *save_ptr (context) to the new input string (context) and proceed,
  // preserving the tokenizing state as if we had passed in NULL as the input string!
  //
  // This pseudo-invariant seems pretty reliable, and I could not find any glibc strtok_r
  // implementation that did not have if(inputstring == NULL) inputstring = *save_ptr; as the
  // first (non-declaration) statement.  Check out the glibc strtok_r implementation at:
  //
  // https://sourceware.org/git/?p=glibc.git;a=blob_plain;f=string/strtok_r.c;hb=master
  while((tok = strtok_r(context, delim, &context)) != NULL) {
    // Check if our list of tokens is at capacity.
    if(tok_used == capacity) {
      // If so, reallocate tokens with twice the capacity.
      if((tokens = realloc(tokens, (capacity *= 2) * sizeof(*tokens))) == NULL) {
        perror("Error reallocating memory for tokens.");
        int i = tok_used;
        while((--i >= 0) && (tokens[i]))
          free(tokens[i]);
        free(tokens);
        free(str);
        exit(EXIT_FAILURE);
      }
    }

    // Store a duplicate of the token in the token list, then increment the token index.
    //
    // NOTE: We need a copy of tok since tok is a pointer to the current token within the
    //       context string, terminated by the null byte ('\0') where the token-terminating
    //       delimiter was before the previous execution of strtok_r (or, if tok is the last
    //       token in context, then it will already be terminated by the null byte.)
    tokens[tok_used++] = strdup(tok);
  }

  if(tok_used == 0) {
    free(tokens);
    tokens = NULL;
  }
  else {
    if((tokens = realloc(tokens, (tok_used + 1) * sizeof(*tokens))) == NULL) {
        perror("Error reallocating memory for tokens.");
        int i = tok_used;
        while((--i >= 0) && (tokens[i]))
          free(tokens[i]);
        free(tokens);
        free(str);
        exit(EXIT_FAILURE);
    }
    tokens[tok_used] = NULL; 
  }
  
  *tok_num = tok_used;  // Doesn't include null-terminating pointer.

  free(str);
  return tokens;
}

/* *
 * Prepares for program execution by forking a new process and directing control to appropriate
 * command handler.
 * */
int exec_dispatch(char **cmd, size_t num_cmd) {
  int p_id, status, type;
  if((p_id = fork()) < 0) {
    perror("Error forking a process.");
    return -1;
  }

  if(verbose_flag && p_id != 0) 
    printf("Creating a child process to run the command: %s\n", cmd[0]);

  // Child process
  if(p_id == 0) {
    if(verbose_flag)
      printf("Child:\n");
    if((type = is_special_feature(cmd)) > 0) {
      status = special_command(cmd, num_cmd, type);
    }
    else {
      if(verbose_flag) {
        printf("  Executing %s...\n\n", cmd[0]);
        printf("Program Output:\n\n");
      }
      status = exec(cmd);
    }

    char** temp = cmd;
    // Free each command in the command list.
    while(temp && *temp) {
      free(*temp++);
    }
    // Free the command list itself.
    free(cmd);
    _Exit(status != -1 ? EXIT_SUCCESS : EXIT_FAILURE);
  }
  // Parent process
  else {
    if(verbose_flag) {
      printf("Parent:\n  Waiting for child process to terminate.\n");
    }
    if(wait(&status) < 0) {
      perror("Error waiting for a process.");
      return -1;
    }
    
    if(WIFSIGNALED(status) && ((WTERMSIG(status) == SIGINT) || (WTERMSIG(status) == SIGQUIT))) {
      printf("Process executing a command was killed by the user.\n");
      return -1;
    }

    // Return status information.
    return WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_SUCCESS) ? EXIT_SUCCESS : -1;
  }
}


/* *
 * Determines if cmd involves overwrite redirection, append redirection, or pipes.
 *
 * Returns - 0 if not a special feature, 1 if append, 2 if overwrite, and 3 if pipe.
 * */
int is_special_feature(char **cmd) {
  int i = 0;
  while(cmd[i] != NULL) {
    // Append redirection.
    if(strcmp(cmd[i], ">>") == 0) {
      return 1;
    }
    // Overwrite redirection.
    if(strcmp(cmd[i], ">") == 0) {
      return 2;
    }
    // Pipes.
    if(strcmp(cmd[i], "|") == 0) {
      return 3;
    }
    i++;
  }
  return 0;
}

/* *
 * Executes program specified by the cmd string array.
 * */
int exec(char **cmd) {
  int i;
  char *curr_path;
  // Check for existence of specified path.
  if(!path_flag) {
    if(verbose_flag) {
      if(stdout_flag) {
        //  Using 4-space indent since this should only happen on 2-depth forks (piping.)
        /* dprintf(saved_stdout, "    Using execvp to execute the command: %s\n", cmd[0]); */
        /* dup2(saved_stdout, STDOUT_FILENO); */
        close(saved_stdout);
        stdout_flag = 0;
      }
      else {
        /* printf("  Using execvp to execute the command: %s\n", cmd[0]); */
      }
    }
    // execvp, given a string without slashes, will search for said executable using
    // the user's path defined by their environment.
    if(execvp(cmd[0], cmd) == -1) {
      if(errno != ENOENT) {
        perror("Error executing program.");
      }
      if(verbose_flag)
        printf("%s is not a valid command or program.\n\n", cmd[0]);
      return -1;
    }
    return -1;  // Should never be reached.
  }
  else {
    if(verbose_flag) {
      if(stdout_flag) {
        /* dprintf(saved_stdout, "    Searching the paths provided in the path file for the command: %s\n", cmd[0]); */
        close(saved_stdout);
        stdout_flag = 0;
      }
      else {
        /* printf("  Searching the paths provided in the path file for the command: %s\n", cmd[0]); */
      }
    }

    i = 0;
    while((curr_path = path[i++]) != NULL) {
      strcat(curr_path, cmd[0]);
      if(verbose_flag) {
        if(stdout_flag) {
          //  Using 4-space indent since this should only happen on 2-depth forks (piping.)
          /* dprintf(saved_stdout, "    Using execvp to execute the command: %s\n", cmd[0]); */
          /* dup2(saved_stdout, STDOUT_FILENO); */
          close(saved_stdout);
          stdout_flag = 0;
        }
        else {
          /* printf("  Using execvp to execute the command: %s\n", cmd[0]); */
        }
      }
      if(execvp(curr_path, cmd) == -1) {
        perror("Error executing program.");
        return -1;
      }
    }
  }
  //  Should not be reached.
  fprintf(stderr, "Error:  Invalid command.\n");
  return -1;
}

/* *
 * Processes cmds and dispatches to the appropriate special command handler.
 * */
int special_command(char **cmd, size_t num_cmd, int type) {
  int i, j, k, capacity, handle_status;
  i = 0;
  j = 0;
  k = 0;
  // Set capacity to half the number of commands, if provided; else, default.
  capacity = num_cmd > 0 ? (num_cmd / 2) + 1 : DEFAULT_TOKENS_CAPACITY;
  char **head, **tail;
  if((head = malloc(capacity * sizeof(*head))) == NULL) {
    perror("Error allocating memory.");
    return -1;
  }
  if((tail = malloc(capacity * sizeof(*tail))) == NULL) {
    perror("Error allocating memory.");
    free(head);
    return -1;
  }
  // Add cmds to head until special feature is encountered.
  while((strcmp(cmd[i], "|") != 0) && (strcmp(cmd[i], ">") != 0) && (strcmp(cmd[i], ">>") != 0)) {
    if(j >= capacity - 1) {
      if((head = realloc(head, (capacity *= 2) * sizeof(*head))) == NULL) {
        perror("Error reallocating memory for head.");
        free(head);
        free(tail);
        return -1;
      }
    }
    head[j++] = cmd[i++];
  }
  head[j] = NULL;

  i++;
  // Add the remaining cmds to tail.
  while(cmd[i] != NULL) {
    if(k >= capacity - 1) {
      if((tail = realloc(tail, (capacity *= 2) * sizeof(*tail))) == NULL) {
        perror("Error reallocating memory for tail.");
        free(head);
        free(tail);
        return -1;
      }
    }
    tail[k++] = cmd[i++];
  }
  tail[k] = NULL;

  // Dispatch to correct special feature handler.
  switch(type) {
    case 1 :
      handle_status = append_handle(head, tail);
      break;
    case 2 :
      handle_status = overwrite_handle(head, tail);
      break;
    case 3 :
      handle_status = pipe_handle(head, tail);
      break;
    default:
      handle_status = -1;
      fprintf(stderr, "Error:  Should not be reached!");
  }
  free(head);
  free(tail);
  return handle_status;
}

/* *
 * Handle piping functionality.
 * */
int pipe_handle(char **head, char **tail) {
  int p_id, status, pipefd[2], tail_type;
  if(verbose_flag)
    printf("  Piping:  %s --> %s\n", head[0], tail[0]);
  // Successful piping.
  if(pipe(pipefd) < 0) {
    perror("Error creating pipe.");
    return -1;
  } 
  if(verbose_flag)
    printf("  Creating a pipe for interprocess communication.\n");
  // Create child process for executing command in head.
  if((p_id = fork()) < 0) {
    perror("Error forking a process.");
    if(close(pipefd[READ_END]) < 0)
      perror("Error closing file descriptor.");
    if(close(pipefd[WRITE_END]) < 0)
      perror("Error closing file descriptor.");
    return -1;
  }
  if(verbose_flag && p_id != 0)
    printf("  Creating a child process for the command:  %s\n", head[0]);

  // Child process used to execute the head command.
  if(p_id == 0) {
    if(verbose_flag)
      printf("  Child:\n");
    // Close unused read end of the pipe.
    if(close(pipefd[READ_END]) < 0) {
      perror("Error closing file descriptor.");
      if(close(pipefd[WRITE_END]) < 0)
        perror("Error closing file descriptor.");
      return -1;
    }
    if(verbose_flag)
      printf("    Closing the read end of the pipe.\n");
    // Save stdout so we can continue to print in verbose mode.
    if(verbose_flag && !stdout_flag) {
      if((saved_stdout = dup(STDOUT_FILENO)) == -1) {
        perror("Error duplicating stdout file descriptor.");
        if(close(pipefd[WRITE_END]) < 0)
          perror("Error closing file descriptor.");
        return -1;
      }
      stdout_flag = 1;
    }
    // Redirect standard output to the write end of the pipe.
    if(dup2(pipefd[WRITE_END], STDOUT_FILENO) < 0) {
      perror("Error duplicating file descriptor.");
      if(close(pipefd[WRITE_END]) < 0)
        perror("Error closing file descriptor.");
      return -1;
    }
    if(verbose_flag) {
      dprintf(saved_stdout, "    Duplicating the file descriptor of the write end of the pipe as stdout.\n");
      dprintf(saved_stdout, "    Executing the head command:  %s\n", head[0]);
    }
    // Close write end of the pipe.
    if(close(pipefd[WRITE_END]) < 0) {
      perror("Error closing file descriptor.");
      return -1;
    }
    // Execute head command, which will output into the write end of the pipe.
    exec(head);
    return -1;  // Shouldn't be reached.
  }
  // Parent process.
  else {
    // Wait for child process to finish.
    if(waitpid(p_id, &status, 0) < 0) {
      perror("Error waiting for child process.");
      if(close(pipefd[READ_END]) < 0)
        perror("Error closing file descriptor.");
      if(close(pipefd[WRITE_END]) < 0)
        perror("Error closing file descriptor.");
      return -1;
    }
    if(verbose_flag)
      printf("  Parent:\n    Waiting for child process to terminate.\n");

    // Redirect standard input to the read end of the pipe.
    if(dup2(pipefd[READ_END], STDIN_FILENO) < 0) {
      perror("Error duplicating file descriptor.");
      if(close(pipefd[READ_END]) < 0)
        perror("Error closing file descriptor.");
      if(close(pipefd[WRITE_END]) < 0)
        perror("Error closing file descriptor.");
      return -1;
    }
    if(verbose_flag)
      printf("    Duplicating the file descriptor of the read end of the pipe as stdin.\n");

    // Close read end of pipe.
    if(close(pipefd[READ_END]) < 0) {
      perror("Error closing file descriptor.");
      if(close(pipefd[WRITE_END]) < 0)
        perror("Error closing file descriptor.");
      return -1;
    }

    // Close write end of pipe.
    if(close(pipefd[WRITE_END]) < 0) {
      perror("Error closing file descriptor.");
      return -1;
    }
    if(verbose_flag)
      printf("    Closing both ends of the pipe.\n");

    // If tail consists of a special feature, route tail to special_command.
    if((tail_type = is_special_feature(tail)) > 0) {
      if(verbose_flag)
        printf("    Tail command consists of special feature.\n");
      if(special_command(tail, 0, tail_type) == -1)
        return -1;
      return 0;
    }
    if(verbose_flag) {
      printf("    Executing the tail command:  %s\n\n", tail[0]);
      printf("Program Output:\n\n");
    }
    // Execute tail command.  Note that the input to the tail command will be the
    // read end of the pipe, which is the buffered data written to the write end of the
    // pipe by the head command executed in the child process.  We have successfully piped
    // these commands!
    exec(tail);
    return -1;  // Shouldn't be reached.
  }
}

/* *
 * Handle overwriting functionality.
 * */
int overwrite_handle(char **head, char **tail) {
  /* return redirection_write_handle(head, tail, 1); */
  int p_id, fd, status;
  if(verbose_flag)
    printf("  Overwriting the output of %s onto %s\n", head[0], tail[0]);

  // Creating a child process for executing the head command.
  if((p_id = fork()) < 0) {
    perror("Error forking a process.");
    return -1;
  }
  if(verbose_flag && p_id != 0)
    printf("  Creating a child process for the command:  %s\n", head[0]);

  // Child process.
  if(p_id == 0) {
    if(verbose_flag)
      printf("  Child:\n");

    // Open file for writing.
    if((fd = open(tail[0], O_CREAT | O_WRONLY | O_TRUNC, 0666)) < 0) {
      perror("Error opening file.");
      return -1;
    }
    if(verbose_flag)
      printf("    Opening %s for writing (overwrite).\n", tail[0]);
    
    // Save stdout so we can continue to print in verbose mode.
    if(verbose_flag && !stdout_flag) {
      if((saved_stdout = dup(STDOUT_FILENO)) == -1) {
        perror("Error duplicating stdout file descriptor.");
        return -1;
      }
      stdout_flag = 1;
    }

    // Duplicate output file descriptor to stdout file descriptor.
    if(dup2(fd, STDOUT_FILENO) < 0) {
      perror("Error duplicating file descriptor.");
      return -1;
    }
    if(verbose_flag)
      dprintf(saved_stdout, "    Duplicating the file descriptor for file %s as stdout.\n", tail[0]);

    // Close output file descriptor.
    if(close(fd) < 0) {
      perror("Error closing a file descriptor.");
      return -1;
    }
    if(verbose_flag)
      dprintf(saved_stdout, "    Closing output file descriptor.\n");
    if(verbose_flag)
      dprintf(saved_stdout, "    Executing the head command:  %s\n", head[0]);
    
    // Execute the head command.
    exec(head);

    return -1;  // Shouldn't be reached.
  }
  // Parent process.
  else {
    // Wait for child process to finish.
    if(waitpid(p_id, &status, 0) < 0) {
      perror("Error waiting for a process.");
      return -1;
    }
    if(verbose_flag) {
      printf("  Parent:\n    Waiting for child process to terminate.\n");
    }

    return 0;  // Successful child process, so return success flag, 0.
  }
}

/* *
 * Handle append functionality.
 * */
int append_handle(char **head, char **tail) {
  /* return redirection_write_handle(head, tail, 0); */
  int p_id, fd, status;
  if(verbose_flag)
    printf("  Appending the output of %s onto the end of %s\n", head[0], tail[0]);

  // Creating a child process for executing the head command.
  if((p_id = fork()) < 0) {
    perror("Error forking a process.");
    return -1;
  }
  if(verbose_flag && p_id != 0)
    printf("  Creating a child process for the command:  %s\n", head[0]);

  // Child process.
  if(p_id == 0) {
    if(verbose_flag)
      printf("  Child:\n");

    // Open file for writing.
    if((fd = open(tail[0], O_CREAT | O_WRONLY | O_APPEND, 0666)) < 0) {
      perror("Error opening file.");
      return -1;
    }
    if(verbose_flag)
      printf("    Opening %s for writing (append).\n", tail[0]);
    
    // Save stdout so we can continue to print in verbose mode.
    if(verbose_flag && !stdout_flag) {
      if((saved_stdout = dup(STDOUT_FILENO)) == -1) {
        perror("Error duplicating stdout file descriptor.");
        return -1;
      }
      stdout_flag = 1;
    }

    // Duplicate output file descriptor to stdout file descriptor.
    if(dup2(fd, STDOUT_FILENO) < 0) {
      perror("Error duplicating file descriptor.");
      return -1;
    }
    if(verbose_flag)
      dprintf(saved_stdout, "    Duplicating the file descriptor for file %s as stdout.\n", tail[0]);

    // Close output file descriptor.
    if(close(fd) < 0) {
      perror("Error closing a file descriptor.");
      return -1;
    }
    if(verbose_flag)
      dprintf(saved_stdout, "    Closing output file descriptor.\n");
    if(verbose_flag)
      dprintf(saved_stdout, "    Executing the head command:  %s\n", head[0]);
    
    // Execute the head command.
    exec(head);

    return -1;  // Shouldn't be reached.
  }
  // Parent process.
  else {
    // Wait for child process to finish.
    if(waitpid(p_id, &status, 0) < 0) {
      perror("Error waiting for a process.");
      return -1;
    }
    if(verbose_flag) {
      printf("  Parent:\n    Waiting for child process to terminate.\n");
    }

    return 0;  // Successful child process, so return success flag, 0.
  }
}

/* *
 * Handle overwriting functionality.
 * */
int redirection_write_handle(char **head, char **tail, int type) {
  int p_id, fd, status, flags;
  char *verb, *verb2;
  verb = type ? "  Overwriting the output of %s onto %s\n"
              : "  Appending the output of %s onto the end of %s\n";
  verb2 = type ? "overwrite.)\n" : "append.)\n";
  flags = O_CREAT | O_WRONLY | (type ? O_TRUNC : O_APPEND);
  if(verbose_flag)
      printf(verb, head[0], tail[0]);

  // Creating a child process for executing the head command.
  if((p_id = fork()) < 0) {
    perror("Error forking a process.");
    return -1;
  }
  if(verbose_flag && p_id != 0)
    printf("  Creating a child process for the command:  %s\n", head[0]);

  // Child process.
  if(p_id == 0) {
    if(verbose_flag)
      printf("  Child:\n");

    // Open file for writing.
    if((fd = open(tail[0], flags, 0666)) < 0) {
      perror("Error opening file.");
      return -1;
    }
    if(verbose_flag)
      printf(strcat("    Opening %s for writing ", verb2), tail[0]);

    // Save stdout so we can continue to print in verbose mode.
    if(verbose_flag && !stdout_flag) {
      if((saved_stdout = dup(STDOUT_FILENO)) == -1) {
        perror("Error duplicating stdout file descriptor.");
        return -1;
      }
      stdout_flag = 1;
    }

    // Duplicate output file descriptor to stdout file descriptor.
    if(dup2(fd, STDOUT_FILENO) < 0) {
      perror("Error duplicating file descriptor.");
      return -1;
    }
    if(verbose_flag)
      printf("    Duplicating the file descriptor for file %s as stdout.\n", tail[0]);

    // Close output file descriptor.
    if(close(fd) < 0) {
      perror("Error closing a file descriptor.");
      return -1;
    }
    if(verbose_flag)
      printf("    Closing output file descriptor.\n");
    if(verbose_flag)
      printf("    Executing the head command:  %s\n", head[0]);
    
    // Execute the head command.
    exec(head);

    return -1;  // Shouldn't be reached.
  }
  // Parent process.
  else {
    // Wait for child process to finish.
    if(waitpid(p_id, &status, 0) < 0) {
      perror("Error waiting for a process.");
      return -1;
    }
    if(verbose_flag) {
      printf("  Parent:\n    Waiting for child process to terminate.\n");
    }

    return 0;  // Successful child process, so return success flag, 0.
  }
}
      
/* *
 * Handler for cd command.
 * */
int cd_handle(char **cmd, size_t num_cmd) {
  char *home;
  if(verbose_flag)
    printf("Changing current directory...\n");
  // cd with no argument, change to home directory.
  if(num_cmd == 1) {
    if((home = getenv("HOME")) == NULL) {
      printf("Error:  There is no home environment variable defined in your environment.");
      return -1;
    }
    if(verbose_flag)
      printf("Obtained home environment variable via call to getenv.\n");
    if(chdir(home) < 0) {
      perror("Error:  Unable to change to your home directory.");
      return -1;
    }
    if(verbose_flag)
      printf("Changed current directory to your home directory: %s\n", home);
  }
  // cd with one argument.
  else if(num_cmd == 2) {
    if(chdir(cmd[1]) < 0) {
      perror("Error:  Changing directory failed.\n");
      return -1;
    }
    if(verbose_flag) {
      char cwd[PATH_MAX];
      if(getcwd(cwd, PATH_MAX) == NULL) {
        perror("Error:  Getting the current working directory failed.");
        return -1;
      }
      printf("Changed current directory to: %s\n", cwd);
    }
  }
  // cd with more than one argument is invalid.
  else {
    printf("Error:  Too many arguments.\nUsage: cd [dir]\n");
    return -1;
  }
  return 0;
}

/* *
 * Handler for pwd command.
 * */
int pwd_handle(char **cmd, size_t num_cmd) {
  if(verbose_flag)
    printf("Getting current working directory...\n");
  // pwd should not have more than one argument unless the argument is actually a special feature.
  if(num_cmd != 1 && !is_special_feature(cmd)) {
    printf("Error:  pwd should not have any arguments.\n");
    return -1;
  }
  char cwd[PATH_MAX];
  if(getcwd(cwd, PATH_MAX) == NULL) {
    perror("Error:  Getting the current working directory failed.");
    return -1;
  }
  if(verbose_flag) {
    printf("Obtained current working directory via call to getcwd.\n");
    printf("Program Output:\n\n");
  }
  printf("%s\n", cwd);
  return 0;
}

/* *
 * Displays help information.
 * */
void display_help(char *progname) {
  // TODO:  Create more comprehensive help information.
  usage(progname);
}

/* *
 * Displays usage information.
 * */
void usage(char *progname) {
  fprintf(stderr, "usage: %s [-p|--path file] [-h|--help] [-v|--verbose]\n", progname);
}
