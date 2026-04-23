#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>
#include <pwd.h>
#include <ncurses.h>
#include <ctype.h>
#include <sys/wait.h>
#include <limits.h>
#include <cjson/cJSON.h>
#include <regex.h>
#include <errno.h> 
#include <time.h>
#include <ftw.h>
#ifndef VERSION
#define VERSION "dev"
#endif
#define BUFFER_SIZE 256 //standard buffer size.
// the three supported values are "daily" "weekly" and "monthly" (for months we use the avarage lenght of a month in a non leap year). All values were calculated on my loyal TI-30 ECO RS
#define DAILY 86400
#define WEEKLY 604800
#define MONTHLY 2628000 // calculated from the average lenght of a non leap year
#define debug(message, ...) \
    _debug(shouldDebug, __FILE__, __LINE__, __func__, message, ##__VA_ARGS__)
#define altDebug(message, ...) \
    _altDebug(shouldDebug, message, ##__VA_ARGS__)
#define error(condition, type, message, ...) \
  _error(shouldDebug, condition, type, __FILE__, __LINE__, __func__, message, ##__VA_ARGS__)
// you must edit this two values if you want to add suport for an editor
extern const char *supportedEditor[]; // array of supported editors
extern const int numEditors; // number of supported editors


//compares two strings alphabetically.
//this function is used for qsort
int compareString(const void *a, const void *b);
// compares two strings in reversed alphabetical order
// this function is used for qsort
int reverseCompareString(const void *a, const void *b);
// this basically checks all the dirs from your path for the editor. This is a safety check.
// If the executable from an editor is not the editor name (for example neovim and nvim), you must handle at the start of the function.
int doesEditorExist(char *editorToCheck, int debug);
// please use the macro debug instead of _debug.
//formated debugging.
void _debug(const int d, const char *file, const int line, const char *function, const char *message, ...);
//use for less formal debuggin. (usefull if enumerating or making a list).
void _altDebug(const int d, const char *message, ...);
// formated error.
void _error(const int shouldDebug, const int condition, const char *type, const char *file, const int line, const char *function, const char *message, ...);
// Returns 1 if the string is in the array.
// Returns 0 if the string is not in the array.
// If you want to only check the first n elements of the array, pass n as len.
int isStringInArray(const char *string, const char **array, const int len);
// initialize the cache directory and creates (if it doesn't already exists) the config file
void initAppFilesAndDirs(const char *home, const int shouldDebug);
// returns 1 if the string is in the file.
// returns 0 if the string is not in the file.
int isStringInFile(const char *path, const char *string, const int shouldDebug);
void appendToFile(const char *path, const char *string, const int shouldDebug);
// replace unwanted chars by '_'.
// '.' is replaced if it is only the first two chars
void sanitize(char *string);
//from https://stackoverflow.com/a/5467788.
//deletes an entire directory. Use with parsimony and carefullness.
int rmrf(char *path, int shouldDebug);
// Inputs are the path to the file, the editor to open and some rendering option.
// render: if we render the .md file with Vivify.
// shouldJumpToEndOfFile: if we put the cursor at the end of the file when opening.
// The program resumes when the editor is closed.
int openEditor(char *path, char *editor, int render, int shouldJumpToEndOfFile, int debug);
// see https://pubs.opengroup.org/onlinepubs/7908799/xsh/strftime.html? for formats.
// returns a string (buffered at 256 chars).
// you should not forgot to free this string as it is in the heap.
char *getFormatedTime(char *format, int shouldDebug);
/* Caclulates if we need to do another backup (by reading ~/.cache/NoteWrapper/backupTime.txt.
If need launches in the background rsync to do the backuping.
rsyncArgs is the array of arguments to be passed to rsync. Do not inclue destination or source. It will be added in the function.*/
void handleBackups(const char *pathOfVaults, const char *pathOfBackup, const char *homeDir, const int interval, const char **rsyncArgs, const int rsyncArgsNumber,  const int shouldDebug);
#endif
