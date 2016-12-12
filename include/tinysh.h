/*
 * tinysh.h
 * Copyright (C) 2016 Clark Zinzow <clarkzinzow@gmail.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef TINYSH_H
#define TINYSH_H

typedef struct CmdList {
  char **cmds;
  int size;
} CmdList;

int set_path(char *file_path);
void driver(void);
void tokenizer(const char *input, CmdList *cmd_list, const char *delim, int capacity);
void exec_dispatch(char **cmd, int num_cmd);
int is_special_feature(char **cmd);
void exec(char **cmd);
void pwd_handle(char **cmd, int size);
void cd_handle(char **cmd, int size);
void special_command(char **cmd, int num_cmd, int type);
void pipe_handle(char **head, char **tail);
void overwrite_handle(char **head, char **tail);
void append_handle(char **head, char **tail);
void display_help(char *progname);
void usage(char *progname);

#endif /* !TINYSH_H */
