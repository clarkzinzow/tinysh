# tinysh
A tiny, simple, well-commented UNIX shell, with a (very) verbose mode.  The verbose mode is designed
to give the user a decent idea of the shell program flow, as well as the lower-level mechanisms being
employed in the shell's effort to carry out the user's various commands.

### Purpose:
You might be asking yourself a very valid question:

![y tho](http://i.imgur.com/yNlQWRM.jpg?fb)

Tinysh is obviously not meant for any serious shell work.  However, running it in verbose mode,
inspecting the code, and implementing your own features can serve as a useful
pedagogical tool for learning the basic underpinnings of a simplified shell architecture.  If you're
in the midst of taking your first operating systems class, or if you have just started to learn how
to use a UNIX shell and you're curious about what's under the hood, hopefully messing around with
tinysh will help solidify some foundational concepts.

### Usage:
There are two ways to start using the shell:
##### Download and Run the Executable:
1. Download tinysh executable from bin directory.
2. Three quick ways to run the shell:
   1. Run
      ```
      $ /path/to/file/tinysh
      ```
   2. or, add `/path/to/file/` to your path and run
      ```
      $ tinysh
      ```
   3. or, navigate to the download directory where tinysh is located
      ```
      $ cd /path/to/file
      ```
      and run
      ```
      $ ./tinysh
      ```
##### Build It Yourself
1. Clone or download this repository.
2. Navigate to the repository directory.
```
$ cd /path/to/repository/tinysh 
```
3. Clean the repository of temporary files, object files, and executables.
```
$ make clean
```
4. Compile tinysh.
```
$ make
```
5. Run tinysh.
```
$ ./bin/tinysh
```

The shell has the following usage:
```
tinysh [-p|--path file] [-h|--help] [-v|--verbose]
```

* `-p file, --path file`
  * Instead of using the path defined by your environment, use the paths defined in the file whose
  name is the string `file`.  tinysh assumes that `file` consists of paths to commands and programs
  that the user wishes to execute, and it assumes that each line in the file corresponds to a path 
  (i.e., there is only a newline delimiter between paths.)
* `-h, --help`
  * Prints help information (for now, just the above usage statement.)
* `-v, --verbose`
  * Enables verbose mode.  In verbose mode, tinysh prints out every little thing that it is doing
    (within reason.)  This includes forks, the opening and closing of pipes, the opening and closing
    of file descriptors, dynamic memory allocations and deallocations thereof, and most system
    calls.


### Features:
* Tinysh can run any typical shell command.
* It is also able to run any program in the path defined by your environment, or the path specified
by an optional path file that you can provide.
* Implements three "special" features:
  * **Overwrite redirection:** `program args > outfile` saves the output from the execution of
  `program` with arguments `args` to `outfile`, overwriting `outfile` if it already exists. 
  * **Append redirection:** `program args >> outfile` appends the output from the execution of
  `program` with arguments `args` onto the end of `outfile`, not overwriting any of `outfile`. 
  * **Pipes:** `program1 args1 | program2 args2` uses the output from the execution of `program1`,
  with arguments `args1`, as input to `program 2`, with arguments `args2`.
* Tinysh makes virtually no assumptions about the number of commands, number of paths in your path,
length of pipe chains, etc.
* Contains a very detailed verbose mode that provides implementation details and control flow
information (forks, opening and closing of pipes, opening and closing of file descriptors, dynamic
memory allocations and deallocations thereof, other system calls, etc.) as the shell carries out the
given command.

### Implementation Details:

* Tinysh creates a child process for each new command, protecting the main shell process from any
errant commands.
* It uses the execvp system call to execute programs.
* As a fun bonus, I implemented a tokenizer with the following features:
  * Thread-safe (i.e., use of `strtok_r`.)
  * Does not modify the input string.
  * Populates a provided `char**` with a dynamically allocated, null-terminated list of commands and
    corresponding arguments.

### Future Work:

Ultimately, creating a full-fledged shell in such a verbose manner would be nonsensical; very few
people would want that much uninteresting information cluttering up their terminal (image writing a
decently sized shell script where every fork, dynamic memory allocation and deallocation, and
opening/closing of file descriptors was explicitly remarked on!)  Moreover, as you add features, the
shell becomes more convoluted and loses some of it's instructional value.  However, there are
certainly some features that could be implemented while still maintaining the shell's pedagogical
use.

So far, I think the following would be worthwhile:

* variables - both shell-defined and user-defined
* command substitution
* here documents
* basic control flow
* filename wildcarding
* condition testing
* lists
* redirection