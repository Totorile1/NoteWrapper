#include "utils.h"

const char *supportedEditor[] = {"neovim", "vim", "nano"};
const int numEditors = 3;

int compareString(const void *a, const void *b) {
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;
    return strcmp(str1, str2); // strcmp returns <0, 0, >0
}
int reverseCompareString(const void *a, const void *b) {
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;
    return -1*strcmp(str1, str2);
}

void getCurrentTime(int *hour, int *minute, int *second) {
    time_t now = time(NULL);              // Get current time in seconds since epoch
    struct tm *local = localtime(&now);   // Convert to local time structure

    *hour = local->tm_hour;               // Extract hour
    *minute = local->tm_min;              // Extract minutes
    *second = local->tm_sec;              // Extract seconds
}

void _debug(const int d, const char *file, const int line, const char *function, const char *message, ...) { // use for formatted debug
  if (d) {
    va_list args; //variadic function stuff
    va_start(args, message);
    int h, m, s;
    getCurrentTime(&h, &m, &s);
    fprintf(stderr, "\e[0;32m[DEBUG -- %d:%d:%d] From file %s line %d function %s:\e[0m\n", h, m, s, file, line, function);
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
    int h, m, s;
    getCurrentTime(&h, &m, &s);
    fprintf(stderr, "\e[0;31m[%s ERROR -- %d:%d:%d] From file %s line %d function %s:\n", type, h, m, s, file, line, function);
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

void copyDir(const char *source, const char *destination, const int shouldDebug) {
    debug("Backuping... source: %s and destination: %s", source, destination);
    pid_t pid = fork();
    
    error(pid < 0, "program", "fork() faild. pid = %d", pid);

    if (pid == 0) {
        // Child process: execute rsync
        char *args[] = { // (TODO LATER)
            "rsync",
            "-Lqah",       // archive, follow links, human-readable // (TODO LATER) add a way to get verbose (-v) without it going on top of ncurses
            "--progress",  // show progress
            "--update",    // only update if newer
            (char *)source,
            (char *)destination,
            NULL
        };

        execvp("rsync", args);
        // If execvp returns, there was an error
        error(1, "program", "execvp() failed");
    } else {
        debug("Backup started asynchronously with PID %d\n", pid);
    }
}
void handleBackups(const char *pathOfVaults, const char *pathOfBackup, const char *homeDir, const int interval, const int shouldDebug) {
    int shouldBackup = 0;
    time_t now = time(NULL);
    debug("Time since epoch is %ld", (long)now);
    char cacheFilePATH[PATH_MAX];
    snprintf(cacheFilePATH, PATH_MAX, "%s/.cache/notewrapper/backupTime.txt", homeDir);

    struct stat st;
    if (stat(cacheFilePATH, &st) != 0) { // file does not exist
        shouldBackup = 1;
        FILE *cacheFile = fopen(cacheFilePATH, "w");
        if (!cacheFile) {
            error(1, "program", "Failed to open cache file for writing: %s", cacheFilePATH);
        }
        fprintf(cacheFile, "%ld", (long)now);
        fclose(cacheFile);
    } else {
        FILE *cacheFile = fopen(cacheFilePATH, "r");
        if (!cacheFile) {
            error(1, "program", "Failed to open cache file for reading: %s", cacheFilePATH);
        }

        char line[64];
        if (fgets(line, sizeof(line), cacheFile) == NULL) {
            error(1, "program", "Failed to read backup cache file (at %s)", cacheFilePATH);
        }
        fclose(cacheFile);

        char *endptr;
        long lastBackupTime = strtol(line, &endptr, 10);
        if (endptr == line || (*endptr != '\n' && *endptr != '\0')) {
            error(1, "program", "Invalid timestamp in cache file: %s", line);
        }

        if (difftime(now, lastBackupTime) > interval) {
            shouldBackup = 1;
            cacheFile = fopen(cacheFilePATH, "w");
            if (!cacheFile) {
                error(1, "program", "Failed to open cache file for writing");
            }
            fprintf(cacheFile, "%ld\n", (long)now);
            fclose(cacheFile);
        }
    }

    if (shouldBackup) {
        copyDir(pathOfVaults, pathOfBackup, shouldDebug);
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

// this ensures that ncurses won't affect the editor behaviour
pid_t editor_pid = fork();
error(editor_pid < 0, "program", "fork() failed.");

if (editor_pid == 0) {
  // =========================
  // CHILD: launch editor
  // =========================

  // ---- NEOVIM / VIM ----
  if (strcmp(editor, "neovim") == 0 || strcmp(editor, "vim") == 0) {
    const char *bin = (strcmp(editor, "neovim") == 0) ? "nvim" : "vim";

    if (render) {
      if (shouldJumpToEndOfFile) {
        debug("Running %s +:$ +:Vivify %s", bin, path);
        execlp(bin, bin, "+:$", "+:Vivify", path, NULL);
      } else {
        debug("Running %s +:Vivify %s", bin, path);
        execlp(bin, bin, "+:Vivify", path, NULL);
      }
    } else {
      if (shouldJumpToEndOfFile) {
        debug("Running %s +:$ %s", bin, path);
        execlp(bin, bin, "+:$", path, NULL);
      } else {
        debug("Running %s %s", bin, path);
        execlp(bin, bin, path, NULL);
      }
    }

    error(1, "program", "execlp() failed.");
  }

  // ---- NANO ----
  else if (strcmp(editor, "nano") == 0) {

    // If render enabled → spawn viv in parallel
    if (render) {
      debug("Running the editor...");
      pid_t viv_pid = fork();
      error(viv_pid < 0, "program", "fork() failed.");

      if (viv_pid == 0) {
        // GRANDCHILD → viv
        

        char viv_path[PATH_MAX];
        strncpy(viv_path, path, PATH_MAX - 1);
        viv_path[PATH_MAX - 1] = '\0';

        if (shouldJumpToEndOfFile) {
          strncat(viv_path, ":99999",
                  PATH_MAX - strlen(viv_path) - 1);
        }

        debug("Running viv %s", viv_path);
        execlp("viv", "viv", viv_path, NULL);
        error(1, "program", "execlp() failed.");
      }
      // IMPORTANT: do NOT wait for viv
    }

    // Now run nano (this replaces the child process)
    if (shouldJumpToEndOfFile) {
      debug("Running nano + %s", path);
      execlp("nano", "nano", "+", path, NULL);
    } else {
      debug("Running nano %s", path);
      execlp("nano", "nano", path, NULL);
    }

    error(1, "program", "execlp() failed.");
  }

  // ---- UNKNOWN EDITOR ----
  else {
    error(1, "program", "Unknown editor.");
  }
    }
    
    // =========================
    // PARENT: wait ONLY editor
    // =========================
    int status;
    while (waitpid(editor_pid, &status, 0) == -1) { // we can't just use waitpid(). Because when resizing the terminal, waitpid() is returned so we loop to see if there is not a problem
      if (errno != EINTR) {
        perror("waitpid");
        break;
      }
    }
    return 0;
}
