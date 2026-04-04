#include "utils.h"

const char *supportedEditor[] = {"neovim", "vim"};
const int numEditors = 2;

int compareString(const void *a, const void *b) {
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;
    return strcmp(str1, str2); // strcmp returns <0, 0, >0
}

void _debug(const int d, const char *file, const int line, const char *function, const char *message, ...) { // use for formatted debug
  if (d) {
    va_list args; //variadic function stuff
    va_start(args, message);

    fprintf(stderr, "\e[0;32m[DEBUG] From file %s line %d function %s:\e[0m\n", file, line, function);
    vfprintf(stderr, message, args);
    printf("\e[0m\n");
    va_end(args);
  }
}
void _altDebug(const int d, const char *message, ...) { // use for less formal debuggin. (usefull if enumerating or making a list
  if (d) {
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
  }
}

void _error(const int shouldDebug, const int condition, const char *type, const char *file, const int line, const char *function, const char *message, ...) { // used for formatted errors
  if (condition) {
    fprintf(stderr, "\e[0;31m[%s ERROR] From file %s line %d function %s:\n", type, file, line, function);
    if (errno != 0) {
        fprintf(stderr, " (System-level error message: %s)\n", strerror(errno));
    } else {
        fprintf(stderr, " (No system-level error; issue is application-level)\n");
    }
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    if (!shouldDebug) {
      fprintf(stderr, "\nRunning notewrapper -V might give you more information.");
    }
    fprintf(stderr, "\e[0m\n");
    exit(1);
  }
}


int doesEditorExist (char *editorToCheck, int shouldDebug) {     // Some exectuables have not exaclty the same name as the editor.
  char *editor;   
    if (strcmp(editorToCheck, "neovim") == 0) {
      editor = strdup("nvim"); // we must use strdup and not just copy as we would have modified editorToOpen in main
    }
    else {
      editor = strdup(editorToCheck);
    }
    char *path_env = getenv("PATH");
    error(!path_env, "program", "getenv(\"PATH\") failed to get your path. NoteWrapper is unable to check if your desired editor is installed\n");
    debug("Your PATH is %s", path_env);
    char *paths = strdup(path_env); // duplicate because strtok modifies the string
    char *dir = strtok(paths, ":");
    while (dir) {
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, editor);
        if (access(fullpath, X_OK) == 0) { // program found
            free(paths);
            free(editor);
            return 1;
        }
        dir = strtok(NULL, ":");
    }

    free(paths);
    free(editor);
    return 0; // program not found
}

char *getFormatedTime(char *format, int shouldDebug) {
  debug("Inputed time format is %s", format);
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char *buf = malloc(BUFFER_SIZE);
  strftime(buf, BUFFER_SIZE, format, tm);
  debug("Formated time is %s", buf);
  return buf;
}

int isStringInArray(const char *string, const char **array, const int len) {
  for (int i = 0; i < len; i++) {
    if (strcmp(string, array[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int isStringInFile(const char *path, const char *string, const int shouldDebug) {
  FILE *file = fopen(path, "r");
  error(file == NULL, "program", "While searching for \"%s\", we could not open file %s", string, path);
  char buf[BUFFER_SIZE];
  
  while (fgets(buf, BUFFER_SIZE, file) != NULL) {
    if (strstr(buf, string) != NULL) {
      debug("%s contains the substring %s", buf, string);
      return 1;
    }
  }
  debug("The string %s is not inside %s", string, path);
  return 0;
}
void appendToFile(const char *path, const char *string, const int shouldDebug) {
    FILE *file = fopen(path, "r");
    char lastLine[1024] = {0};
    error(file == NULL, "program", "could not open %s", path);
    char buffer[BUFFER_SIZE]; // it does not matter if we have a small buffer. string is relatively small (most time \n or the name of the file). So it is under BUFFER_SIZE. If the last line is more than BUFFER_SIZE. It can't be equal to string

    // Read file line by line to get the last one
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        strncpy(lastLine, buffer, sizeof(lastLine) - 1);
    }
    fclose(file);

    // Compare last line with string
    if (strcmp(lastLine, string) == 0) {
        if (shouldDebug) {
            debug("Skipping append: last line already matches \"%s\"", string);
        }
        return;
    }

    // Append since it's different
    file = fopen(path, "a");
    error(file == NULL, "program", "could not open %s", path);

    fprintf(file, "%s", string);
    fclose(file);

    debug("\"%s\" was appended to %s successfully", string, path);
}

void sanitize(char *string) {
  for (int i = 0; i < strlen(string); i++) {
    if ((!isalnum((unsigned char)string[i]) && strchr("/\\:*?\"\'<>\n\r\t", string[i])) || ((i == 0 || i == 1) && string[i] == '.')) { // replace unwanted chars by '_'. '.' is replaced if it is only the first two chars
                                                                                                                    // (TODO LATER fixe case where it "*.*", "..." and so on
        string[i] = '_';
    }
  }
}

//  both functions are from https://stackoverflow.com/a/5467788
// from what i understood:
// remove() can't delete directories with files
// so it walks the file tree and deletes it's content before removing the directory
// (TODO LATER) this seems safe, but it's maybe a good idea to add some checks to not remove something it should not remove
int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
  int shouldDebug = 1; //normally shouldDebug should be passed to the function. However, it's to difficult here and the best it to set it to 1.
  int rv = remove(fpath);
  error(rv, "program", "remove() failed to delete %s", fpath);
  return rv;
}

int rmrf(char *path) {
  // (TODO LATER) Check if it not a root directory or something like this 
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}


int openEditor(char *path, char *editor, int render, int shouldJumpToEndOfFile, int shouldDebug) {
  // (TODO LATER) Bug app breaks if browser was not already launched before vivify
  // (TODO LATER) for nvim and vim we should check if there is swap files or recovery files and handle that
  pid_t pid = fork(); // this forking allows the programs to return when nvim is closed
  error(pid<0, "program", "fork() failed.");
  if (pid == 0) {
      // Child process: replace with editor of choice
    if (strcmp(editor, "neovim") == 0) { // opens with Neovim 
      if (render) { // don't render using vivify
        if (shouldJumpToEndOfFile) { // goes to the end of the file on opening.
          // :$ goes to the end of the file. :Vivify runs vivify
          debug("Running nvim +:$ +:Vivify %s", path);
          execlp("nvim", "nvim", "+:$", "+:Vivify", path, NULL);
          error(1, "program", "execlp() failed."); // if something after execlp is executed it means something failed. Normally this function is not called
        } else { // don't go to the end of the file
          debug("Running nvim +:Vivify %s", path);
          execlp("nvim", "nvim", "+:Vivify", path, NULL);
          error(1, "program", "execlp() failed.");
        }
      } else { // don't render using vivify
        if (shouldJumpToEndOfFile) { // go to end of the file on opening
          debug("Running nvim +:$ %s", path);
          execlp("nvim", "nvim", "+:$", path, NULL);
          error(1, "program", "execlp() failed.");
        } else { // don't go to the end of the file on opening
          debug("Running nvim %s", path);  
          execlp("nvim", "nvim", path, NULL);
          error(1, "program", "execlp() failed.");
        }
      }
    } else if (strcmp(editor, "vim") == 0) { // opens with Vim // see comments for neovim for explanations
      if (render) {
        if (shouldJumpToEndOfFile) {
          debug("Running vim +:$ +:Vivify %s", path);  
          execlp("vim", "vim", "+:$", "+:Vivify", path, NULL);
          error(1, "program", "execlp() failed.");
        } else {
          debug("Running vim +:Vivify %s", path);  
          execlp("vim", "vim", "+:Vivify", path, NULL);
          error(1, "program", "execlp() failed.");
        }
      } else {
        if (shouldJumpToEndOfFile) {
          debug("Running vim +:$ %s", path);  
          execlp("vim", "vim", "+:$", path, NULL);
          error(1, "program", "execlp() failed.");
        } else {
          debug("Running vim %s", path);
          execlp("vim", "vim", path, NULL);
          error(1, "program", "execlp() failed.");
        }
      }
    }
  } else {
    // Parent process: wait for child to finish
    int status;
    waitpid(pid, &status, 0);
  } // (TODO LATER) add a options to kill the browser when closing. This will solve the bug where -R does renders when the file was previously opened with -r.
  return 0;
}
