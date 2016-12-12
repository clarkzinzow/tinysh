CC = gcc
BINDIR = bin
BUILDDIR = build
INCDIR = include
LIBDIR = lib
SRCDIR = src
TESTDIR = test

SRCEXT = c
EXE = tinysh

LIB = -L$(LIBDIR)
SRC = $(wildcard $(SRCDIR)/*.$(SRCEXT))
OBJ = $(patsubst $(SRCDIR)/%, $(BUILDDIR)/%, $(SRC:.$(SRCEXT)=.o))
INC = -I$(INCDIR)

CDEBUG = -g -DDEBUG
CFDEBUG = -o -g -DDEBUG
STD = -std=gnu11
OPT = -O2
FLAGS = -Wall $(STD) $(CDEBUG)
CFLAGS = -c $(FLAGS) $(OPT)
LDFLAGS = $(FLAGS) $(OPT)
CFDEBUG = -pedantic -Wextra $(FLAGS)
MKFLAGS = -p
RM = rm
RMFLAGS = -rf
RMTARGETS = *~ $(BUILDDIR) $(BINDIR)/$(EXE)

all: $(BINDIR)/$(EXE) 

$(BINDIR)/$(EXE): $(OBJ)
	@echo "Linking objects..."
	$(CC) $(LDFLAGS) $(LIB) $^ -o $@

$(OBJ): $(BUILDDIR)/%.o : $(SRCDIR)/%.c
	@echo "Building objects..."
	@mkdir $(MKFLAGS) $(BUILDDIR)
	$(CC) $(CFLAGS) $(INC) $^ -o $@  

debug:
	$(CC) $(CFDEBUG) $(LIB) $(INC) $(SRC) -o $(BINDIR)/$(EXE)

clean:
	@echo "Cleaning up..."
	$(RM) $(RMFLAGS) $(RMTARGETS) 

.PHONY: clean
