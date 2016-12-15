/*
 * tinysh.h
 * Copyright (C) 2016 Clark Zinzow <clarkzinzow@gmail.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef TINYSH_H
#define TINYSH_H

#include <stdlib.h>

int set_path(char *file_path);
int driver(void);
char** tokenizer(const char *input, const char *delim, size_t *tok_num);
int exec_dispatch(char **cmd, size_t num_cmd);
int is_special_feature(char **cmd);
int exec(char **cmd);
int pwd_handle(char **cmd, size_t num_cmd);
int cd_handle(char **cmd, size_t num_cmd);
int special_command(char **cmd, size_t num_cmd, int type);
int pipe_handle(char **head, char **tail);
int overwrite_handle(char **head, char **tail);
int append_handle(char **head, char **tail);
int redirection_write_handle(char **head, char **tail, int type);
void help_handle(char *cmd);
void prog_help();
void shell_help();
void print_desc();
void usage();

#endif /* !TINYSH_H */
