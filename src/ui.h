#ifndef UI_H
#define UI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <ctype.h>
#include <limits.h>
#include "utils.h"
char *createNewNote(char dirToVault[PATH_MAX], char *vaultFromDir, char *bypass, int debug);
// Uses ncurses to get an input from the user
// Creates a new note with this input
// returns the path to the note
void createNewVault(char *dirToVault, int debug);
// Uses ncurses to get an input from the user
// Creates a new vault with this input  
// If vault already exists, prints a warning
// returns nothing
char* ncursesSelect(char **options, char *optionsText, size_t optionsNumber, size_t extraOptionsNumber, int debug);
// this function is used multiple times let the user select one options from many with ncurses in a TUI.
// options is the array of strings with all the options.
// optionsText is the text that will be printed at the top (For example: "Please select ...")
// we distinguish options from extraOptions
// options could be the list of all notes or all vaults
// extraOptions are options that are special and have a special color (for ex: "Delete vault", "Settings", etc.)
// note: extraOptions should be at the end of options
// returns the selected option
#endif
