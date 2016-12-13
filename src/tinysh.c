/* *
 * tinysh.c
 *
 * A tiny UNIX shell.
 *
 * NOTES:
 *   - In accordance with the UNIX 98 standard and the convention set by glibc, we assume that
 *     malloc, calloc, and realloc set errno to ENOMEM upon failure.
 *   - 
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

#define MAX_INPUT_LENGTH 1024
#define INIT_NUM_PATHS 20
#define DEFAULT_CMD_NUM 3

char *path[INIT_NUM_PATHS];
static int path_flag;
static int verbose_flag;
static int saved_stdout;  // Saved stdout file descriptor.
static int stdout_flag;  // 1 if stdout has been saved, 0 if not.

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
            set_path(optarg);
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
          printf("Error process shell arguments.\n");
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
          set_path(optarg);
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
        printf("Error, shouldn't reach the default case in switch statement.\n");
        exit(EXIT_FAILURE);
    }
  }
  driver();  // Pass off to shell driver.
  return 0;
}

/* *
 * Sets the path for the shell according to the user-provided path file, or uses the default
 * path defined by the user's environment if there are any errors in processing said path file.
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
  int ind;
  FILE *fp;
  if((fp = fopen(file_path, "r")) == NULL) {
    // If the user created a path file and we can't open it, print error information.
    if(errno != ENOENT) {
      perror("Unable to open .path file.");
    }
    // Otherwise, opt for the default path.
    path_flag = 0;
    return 0;
  }
  else {
    printf("Obtaining path from the following file: %s", file_path);
    ind = 0;
    if((path[ind] = malloc(PATH_MAX)) == NULL) {
      perror("Error allocating memory for path.");
      path_flag = 0;
      fclose(fp);
      return 0;
    }
    /* TODO:  Implement reallocate feature for path to allow for variable-sized path entries. */
    while((ind < INIT_NUM_PATHS) && (fgets(path[ind], PATH_MAX, fp) != NULL)) {
      if((path[++ind] = malloc(PATH_MAX)) == NULL) {
        perror("Error allocating memory for path.");
        path_flag = 0;
        fclose(fp);
        return 0;
      }
    }
    fclose(fp);
    return 1;
  }
}

/* *
 * The main shell driver.
 * */
void driver() {
  int capacity, size, exit_flag;
  CmdList *cmd_list;
  char input[MAX_INPUT];
  char **cmds;
  const char *delim = " \t\n";
  if(!path_flag) {
    printf("Using the path defined by your environment.\n");
  }
  else {
    printf("Using the path defined in the provided path file.\n");
  }
  if((cmd_list = malloc(sizeof(CmdList))) == NULL) {
    perror("Error allocating memory.");
    exit(EXIT_FAILURE);
  }
  if(verbose_flag)
    printf("Dynamically allocating memory for a struct containing a list of commands and the number of commands.\n");

  exit_flag = 0;
  while(!exit_flag) {
    printf("tinysh> ");

    if(fgets(input, sizeof(input), stdin) == NULL) {
      // No error checking of fgets since EOF with no characters read is allowed.
    }
      
    capacity = (strlen(input)/2) + 1;  // Initial capacity heuristic.
    
    // Get cmd_list.
    tokenizer(input, cmd_list, delim, capacity); 
    cmds = cmd_list->cmds;
    size = cmd_list->size;

    if(cmds[0] == NULL)
      continue;

    if(verbose_flag)
      printf("\n");

    if(strcmp(cmds[0], "exit") == 0) {
      exit_flag = 1;
    }
    else if(strcmp(cmds[0], "pwd") == 0) {
      pwd_handle(cmds, size);
    }
    else if(strcmp(cmds[0], "cd") == 0) {
      cd_handle(cmds, size);
    }
    else {
      exec_dispatch(cmds, size);
    }

    if(verbose_flag && exit_flag != 1)
      printf("\n");
    
    while(cmds && *cmds) {
      free(*cmds++);
    }
    
    free(cmd_list->cmds);
  }

  printf("Exiting now.  Thanks for using tinysh!\n");

  return;
}

/* *
 * Tokenizer with the following features:
 *   - Thread-safe
 *   - Does not modify the input string
 *   - Populates cmd_list with a dynamically allocated, null-terminated list of strings
 *     and the number of said strings.
 * */
void tokenizer(const char *input, CmdList *cmd_list, const char *delim, int capacity) {
  char *str, *context, *tok;
  int tok_ind = 0;

  if((str = strdup(input)) == NULL) {
    perror("Error allocating memory.");
    exit(EXIT_FAILURE);
  }
  context = str;
  if((cmd_list->cmds = malloc(capacity * sizeof(*cmd_list->cmds))) == NULL) {
    perror("Error allocating memory.");
    free(str);
    exit(EXIT_FAILURE);
  }

  while((tok = strtok_r(context, delim, &context)) != NULL) {
    cmd_list->cmds[tok_ind++] = strdup(tok);
    if(tok_ind >= capacity - 1) {
      if((cmd_list->cmds = realloc(cmd_list->cmds, (capacity *= 2) * sizeof(*cmd_list->cmds))) == NULL) {
        perror("Error reallocating memory.");
        free(cmd_list->cmds);
        free(str);
        exit(EXIT_FAILURE);
      }
    }
  }
  cmd_list->cmds[tok_ind] = NULL; 
  cmd_list->size = tok_ind;  // Doesn't include null-terminating pointer.

  free(str);
}

/* *
 * Prepares for program execution by forking a new process and directing control to appropriate
 * command handler.
 * */
void exec_dispatch(char **cmd, int num_cmd) {
  int p_id, status, type;
  if((p_id = fork()) < 0) {
    perror("Error forking a process.");
    exit(EXIT_FAILURE);
  }

  if(verbose_flag && p_id != 0) 
    printf("Creating a child process to run the command: %s\n", cmd[0]);

  // Child process
  if(p_id == 0) {
    if(verbose_flag)
      printf("Child:\n");
    if((type = is_special_feature(cmd)) > 0) {
      special_command(cmd, num_cmd, type);
    }
    else {
      if(verbose_flag) {
        printf("  Executing %s...\n\n", cmd[0]);
        printf("Program Output:\n\n");
      }
      exec(cmd);
    }
  }
  // Parent process
  else {
    if(verbose_flag) {
      printf("Parent:\n  Waiting for child process to terminate.\n");
    }
    if(wait(&status) < 0) {
      perror("Error waiting for a process.");
      exit(EXIT_FAILURE);
    }
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
void exec(char **cmd) {
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
      perror("Error executing program.");
      exit(EXIT_FAILURE);
    }
  }
  else {
    if(verbose_flag) {
      if(stdout_flag) {
        /* dprintf(saved_stdout, "    Searching the paths provided in the path file for the command: %s\n", cmd[0]); */
      }
      else {
        /* printf("  Searching the paths provided in the path file for the command: %s\n", cmd[0]); */
      }
    }

    for(i = 0; (i < INIT_NUM_PATHS) && (path[i] != NULL) && (strcmp(path[i], "") != 0); i++) {
      curr_path = path[i];
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
        exit(EXIT_FAILURE);
      }
    }
  }
  //  Should not be reached.
  printf("Error:  Invalid command.\n");
  exit(EXIT_FAILURE);
}

/* *
 * Processes cmds and dispatches to the appropriate special command handler.
 * */
void special_command(char **cmd, int num_cmd, int type) {
  int i, j, k, capacity;
  i = 0;
  j = 0;
  k = 0;
  // Set capacity to half the number of commands, if provided; else, default.
  capacity = num_cmd > 0 ? (num_cmd/2) + 1 : DEFAULT_CMD_NUM;
  char **head, **tail;
  if((head = malloc(capacity * sizeof(*head))) == NULL) {
    perror("Error allocating memory.");
    exit(EXIT_FAILURE);
  }
  if((tail = malloc(capacity * sizeof(*tail))) == NULL) {
    perror("Error allocating memory.");
    exit(EXIT_FAILURE);
  }
  // Add cmds to head until special feature is encountered.
  while((strcmp(cmd[i], "|") != 0) && (strcmp(cmd[i], ">") != 0) && (strcmp(cmd[i], ">>") != 0)) {
    if(j >= capacity - 1) {
      if((head = realloc(head, (capacity *= 2) * sizeof(*head))) == NULL) {
        perror("Error reallocating memory.");
        exit(EXIT_FAILURE);
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
        perror("Error reallocating memory.");
        exit(EXIT_FAILURE);
      }
    }
    tail[k++] = cmd[i++];
  }
  tail[k] = NULL;

  // Dispatch to correct special feature handler.
  switch(type) {
    case 1 :
      append_handle(head, tail);
      break;
    case 2 :
      overwrite_handle(head, tail);
      break;
    case 3 :
      pipe_handle(head, tail);
      break;
    default:
      printf("Error:  Should not be reached!");
  }
  free(head);
  free(tail);
}

/* *
 * Handle piping functionality.
 * */
void pipe_handle(char **head, char **tail) {
  int p_id, status, pipefd[2], tail_type;
  if(verbose_flag)
    printf("  Piping:  %s --> %s\n", head[0], tail[0]);
  // Successful piping.
  if(pipe(pipefd) == 0) {
    if(verbose_flag)
      printf("  Creating a pipe for interprocess communication.\n");
    if((p_id = fork()) < 0) {
      perror("Error forking a process.");
      exit(EXIT_FAILURE);
    }
    if(verbose_flag && p_id != 0)
      printf("  Creating a child process for the command:  %s\n", head[0]);

    // Child process.
    if(p_id == 0) {
      if(verbose_flag)
        printf("  Child:\n");
      if(close(pipefd[0]) < 0) {
        perror("Error closing file descriptor.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag)
        printf("    Closing the read end of the pipe.\n");
      // Save stdout so we can continue to print in verbose mode.
      if(verbose_flag && !stdout_flag) {
        if((saved_stdout = dup(STDOUT_FILENO)) == -1) {
          perror("Error duplicating stdout file descriptor.");
          exit(EXIT_FAILURE);
        }
        stdout_flag = 1;
      }
      if(dup2(pipefd[1], STDOUT_FILENO) < 0) {
        perror("Error duplicating file descriptor.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag) {
        dprintf(saved_stdout, "    Duplicating the file descriptor of the write end of the pipe as stdout.\n");
        dprintf(saved_stdout, "    Executing the head command:  %s\n", head[0]);
      }
      exec(head);
      exit(EXIT_FAILURE);  // Should never be reached.
    }
    // Parent process.
    else {
      if(waitpid(p_id, &status, 0) < 0) {
        perror("Error waiting for child process.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag)
        printf("  Parent:\n    Waiting for child process to terminate.\n");
      if(dup2(pipefd[0], STDIN_FILENO) < 0) {
        perror("Error duplicating file descriptor.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag)
        printf("    Duplicating the file descriptor of the read end of the pipe as stdin.\n");
      if((close(pipefd[0]) < 0) || (close(pipefd[1]) < 0)) {
        perror("    Error closing file descriptor.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag)
        printf("    Closing both ends of the pipe.\n");
      if((tail_type = is_special_feature(tail)) > 0) {
        special_command(tail, -1, tail_type);
      }
      else {
        if(verbose_flag) {
          printf("    Executing the tail command:  %s\n\n", tail[0]);
          printf("Program Output:\n\n");
        }
        exec(tail);
      }
      exit(EXIT_FAILURE);  // Should never be reached.
    }
  }
  else {
    perror("Error creating pipe.");
    exit(EXIT_FAILURE);
  } 
}

/* *
 * Handle overwriting functionality.
 * */
void overwrite_handle(char **head, char **tail) {
  int p_id, fd, status, pipefd[2];
  if(verbose_flag)
    printf("  Overwriting the output of %s onto %s\n", head[0], tail[0]);

  // Successful piping.
  if(pipe(pipefd) == 0) {
    if(verbose_flag)
      printf("  Creating a pipe for interprocess communication.\n");
    if((p_id = fork()) < 0) {
      perror("Error forking a process.");
      exit(EXIT_FAILURE);
    }
    if(verbose_flag && p_id != 0)
      printf("  Creating a child process for the command:  %s\n", head[0]);

    // Child process.
    if(p_id == 0) {
      if(verbose_flag)
        printf("  Child:\n");
      if((fd = open(tail[0], O_CREAT | O_WRONLY | O_TRUNC, 0666)) != -1) {
        if(verbose_flag)
          printf("    Opening %s for writing (overwrite).\n", tail[0]);
        // Save stdout so we can continue to print in verbose mode.
        if(verbose_flag && !stdout_flag) {
          if((saved_stdout = dup(STDOUT_FILENO)) == -1) {
            perror("Error duplicating stdout file descriptor.");
            exit(EXIT_FAILURE);
          }
          stdout_flag = 1;
        }
        if(dup2(fd, STDOUT_FILENO) < 0) {
          perror("Error duplicating file descriptor.");
          exit(EXIT_FAILURE);
        }
        if(verbose_flag)
          printf("    Duplicating the file descriptor for file %s as stdout.\n", tail[0]);
      }
      else {
        perror("Error opening file.");
        exit(EXIT_FAILURE);
      }
      if(close(pipefd[0]) < 0) {
        perror("Error closing file descriptor.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag)
        printf("    Closing the read end of the pipe.\n");
      if(is_special_feature(tail)) {
        if(dup2(pipefd[1], STDOUT_FILENO) < 0) {
          perror("Error duplicating file descriptor.");
          exit(EXIT_FAILURE);
        }
        if(verbose_flag)
          printf("    Duplicating the file descriptor of the write end of the pipe as stdout.\n");
        if((fd = open(tail[2], O_CREAT | O_WRONLY | O_TRUNC, 0666)) != -1) {
	        if(dup2(fd, STDOUT_FILENO) < 0) {
            perror("Error duplicating file descriptor.");
            exit(EXIT_FAILURE);
          }
          if(verbose_flag)
            printf("    Duplicating the file descriptor for file %s as stdout.\n", tail[2]);
	      }
        else {
          perror("Error opening file.");
          exit(EXIT_FAILURE);
	      }
        if(verbose_flag)
          printf("    Executing the head command:  %s\n", head[0]);
        exec(head);
      }
      else {
        if(verbose_flag)
          printf("    Executing the head command:  %s\n", head[0]);
        exec(head);
      }
      if(close(fd) < 0) {
        perror("Error closing a file descriptor.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag)
        printf("    Closing output file.\n");
    }
    // Parent process.
    else {
      if(waitpid(p_id, &status, 0) < 0) {
        perror("Error waiting for a process.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag) {
        printf("  Parent:\n    Waiting for child process to terminate.\n");
      }
    }
  }
  else {
    perror("Error creating pipe.");
    exit(EXIT_FAILURE);
  }
  exit(EXIT_FAILURE);
}

/* *
 * Handle append functionality.
 * */
void append_handle(char **head, char **tail) {
  int p_id, fd, status, pipefd[2];
  if(verbose_flag)
    printf("  Appending the output of %s onto the end of %s\n", head[0], tail[0]);
  // Successful piping.
  if(pipe(pipefd) == 0) {
    if(verbose_flag)
      printf("  Creating a pipe for interprocess communication.\n");
    if((p_id = fork()) < 0) {
      perror("Error forking a process.");
      exit(EXIT_FAILURE);
    }
    if(verbose_flag && p_id != 0)
      printf("  Creating a child process for the command:  %s\n", head[0]);
    // Child process.
    if(p_id == 0) {
      if((fd = open(tail[0], O_CREAT | O_WRONLY | O_APPEND, 0666)) != -1) {
        if(verbose_flag)
          printf("    Opening %s for writing (append).\n", tail[0]);
        // Save stdout so we can continue to print in verbose mode.
        if(verbose_flag && !stdout_flag) {
          if((saved_stdout = dup(STDOUT_FILENO)) == -1) {
            perror("Error duplicating stdout file descriptor.");
            exit(EXIT_FAILURE);
          }
          stdout_flag = 1;
        }
        if(dup2(fd, STDOUT_FILENO) < 0) {
          perror("Error duplicating file descriptor.");
          exit(EXIT_FAILURE);
        }
        if(verbose_flag)
          printf("    Duplicating the file descriptor for file %s as stdout.\n", tail[0]);
      }
      else {
        perror("Error opening file.");
        exit(EXIT_FAILURE);
      }
      if(close(pipefd[0]) < 0) {
        perror("Error closing file descriptor.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag)
        printf("    Closing the read end of the pipe.\n");
      if(is_special_feature(tail)) {
        if(dup2(pipefd[1], STDOUT_FILENO) < 0) {
          perror("Error duplicating file descriptor.");
          exit(EXIT_FAILURE);
        }
        if(verbose_flag)
          printf("    Duplicating the file descriptor of the write end of the pipe as stdout.\n");
        if((fd = open(tail[2], O_CREAT | O_WRONLY | O_APPEND, 0666)) != -1) {
          if(verbose_flag)
            printf("    Opening %s for writing (append).\n", tail[0]);
	        if(dup2(fd, STDOUT_FILENO) < 0) {
            perror("Error duplicating file descriptor.");
            exit(EXIT_FAILURE);
          }
          if(verbose_flag)
            printf("    Duplicating the file descriptor for file %s as stdout.\n", tail[0]);
	      }
        else {
          perror("Error opening file.");
          exit(EXIT_FAILURE);
	      }
        if(verbose_flag)
          printf("    Executing the head command:  %s\n", head[0]);
        exec(head);
      }
      else {
        if(verbose_flag)
          printf("    Executing the head command:  %s\n", head[0]);
        exec(head);
      }
      if(close(fd) < 0) {
        perror("Error closing file descriptor.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag)
        printf("    Closing output file.\n");
    }
    // Parent process.
    else {
      if(waitpid(p_id, &status, 0) < 0) {
        perror("Error waiting for process.");
        exit(EXIT_FAILURE);
      }
      if(verbose_flag) {
        printf("  Parent:\n    Waiting for child process to terminate.\n");
      }
    }
  }
  else {
    perror("Error piping failed.");
    exit(EXIT_FAILURE);
  }
  exit(EXIT_FAILURE);
}
      
/* *
 * Handler for cd command.
 * */
void cd_handle(char **cmd, int size) {
  char *home;
  if(verbose_flag)
    printf("Changing current directory...\n");
  // cd with no argument, change to home directory.
  if(size == 1) {
    if((home = getenv("HOME")) == NULL) {
      printf("Error:  There is no home environment variable defined in your environment.");
    }
    if(verbose_flag)
      printf("Obtained home environment variable via call to getenv.\n");
    if(chdir(home) < 0) {
      perror("Error:  Unable to change to your home directory.");
    }
    if(verbose_flag)
      printf("Changed current directory to your home directory: %s\n", home);
  }
  // cd with one argument.
  else if(size == 2) {
    if(chdir(cmd[1]) < 0) {
      perror("Error:  Changing directory failed.\n");
    }
    if(verbose_flag) {
      char cwd[PATH_MAX];
      if(getcwd(cwd, PATH_MAX) == NULL) {
        perror("Error:  Getting the current working directory failed.");
        return;
      }
      printf("Changed current directory to: %s\n", cwd);
    }
  }
  // cd with more than one argument is invalid.
  else {
    printf("Error:  Too many arguments.\nUsage: cd [dir]\n");
  }
}

/* *
 * Handler for pwd command.
 * */
void pwd_handle(char **cmd, int size) {
  if(verbose_flag)
    printf("Getting current working directory...\n");
  // pwd should not have more than one argument unless the argument is actually a special feature.
  if(size != 1 && !is_special_feature(cmd)) {
    printf("Error:  pwd should not have any arguments.\n");
    return;
  }
  char cwd[PATH_MAX];
  if(getcwd(cwd, PATH_MAX) == NULL) {
    perror("Error:  Getting the current working directory failed.");
    return;
  }
  if(verbose_flag) {
    printf("Obtained current working directory via call to getcwd.\n");
    printf("Program Output:\n\n");
  }
  printf("%s\n", cwd);
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
