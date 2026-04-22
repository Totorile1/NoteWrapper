#include "utils.h"
#include "string.h"

const char *supportedEditor[] = {"helix", "kakoune", "micro", "nano", "neovim", "vi", "vim"};
const int numEditors = 7;

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
    fflush(stdout);
    fflush(stderr);
    va_list args; //variadic function stuff
    va_start(args, message);
    int h, m, s;
    getCurrentTime(&h, &m, &s);
    fprintf(stderr, "\e[0;32m[DEBUG -- %d:%d:%d] From file %s line %d function %s:\e[0m\n", h, m, s, file, line, function);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\e[0m\n");
    va_end(args);
  }
}
void _altDebug(const int d, const char *message, ...) { // use for less formal debuggin. (usefull if enumerating or making a list
  if (d) {
    fflush(stdout);
    fflush(stderr);
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

static void copyDir(const char *source, const char *destination, const char **rsyncArgs, const int rsyncArgsNumber, const int shouldDebug) {
    debug("Backuping... source: %s and destination: %s", source, destination);
    debug("Rsync has %d extra arguments", rsyncArgsNumber);
    pid_t pid = fork();
    
    error(pid < 0, "program", "fork() faild. pid = %d", pid);

    if (pid == 0) {
        // Child process: execute rsync
        char **args = malloc((4 + rsyncArgsNumber)*sizeof(char*));
        args[0] = "rsync";
        for (int i = 0; i < rsyncArgsNumber; i++) {
          args[i+1] = (char *)rsyncArgs[i];
          if (i == rsyncArgsNumber-1) { // last loop
            args[i+2] = (char*)source;
            args[i+3] = (char*)destination;
            args[i+4] = NULL; // execvp expect last arg to be NULL
            debug("rsync command:");
            for (int k = 0; k <= i+4; k++) {
              altDebug("%s ", args[k]);
            }
            altDebug("\n");
          }
        }
        execvp("rsync", args);
        // If execvp returns, there was an error
        error(1, "program", "execvp() failed");
    } else {
        debug("Backup started asynchronously with PID %d\n", pid);
    }
}

static void ensureDir(const char *path, const int shouldDebug) {
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        debug("Creating the directory %s.", path);
        error(mkdir(path, 0700) != 0, "program", "mkdir failed");
    }
}

void initAppFilesAndDirs(const char *home, const int shouldDebug) {
    char config_dir[PATH_MAX];
    char cache_dir[PATH_MAX];

    snprintf(config_dir, sizeof(config_dir),
             "%s/.config/notewrapper", home);

    snprintf(cache_dir, sizeof(cache_dir),
             "%s/.cache/notewrapper", home);

    ensureDir(config_dir, shouldDebug);
    ensureDir(cache_dir, shouldDebug);

    char config_file[PATH_MAX+12];
    snprintf(config_file, sizeof(config_file), "%s/config.json", config_dir);

    FILE *f = fopen(config_file, "r");
    if (f) {
        fclose(f);
        return;
    }
    debug("Creating default config file.");
    FILE *w = fopen(config_file, "w");
    error(!w, "program", "fopen failed opening %s");
    fprintf(w,
"{\n"
"  \"directory\": \"~/Documents/Notes/\",\n"
"  \"render\": true,\n"
"  \"jumpToEndOfFileOnLaunch\": true,\n"
"  \"editor\": \"neovim\",\n"
"  \"journalRegex\": \".*journal.*\",\n"
"  \"dateEntry\": \"# %%Y %%m %%d %%a\",\n"
"  \"newLineOnOpening\": true,\n"
"  \"backup\": {\n"
"    \"enable\": false,\n"
"    \"directory\": \"/path/to/backup\",\n"
"    \"interval\": \"weekly\",\n"
"    \"rsyncArgs\": [\"-Lqah\", \"--update\"]\n"
"  }\n"
"}\n");

    fclose(w);
}

void handleBackups(const char *pathOfVaults, const char *pathOfBackup, const char *homeDir, const int interval, const char **rsyncArguments, const int rsyncArgumentsNumber, const int shouldDebug) {
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
        copyDir(pathOfVaults, pathOfBackup, rsyncArguments, rsyncArgumentsNumber, shouldDebug);
    }
}

int doesEditorExist (char *editorToCheck, int shouldDebug) {     // Some exectuables have not exaclty the same name as the editor.
  char *editor;   
    if (strcmp(editorToCheck, "neovim") == 0) {
      editor = strdup("nvim"); // we must use strdup and not just copy as we would have modified editorToOpen in main
    } else if (strcmp(editorToCheck, "helix") == 0) {
      editor = strdup("hx");
    } else if (strcmp(editorToCheck, "kakoune") == 0) {
      editor = strdup("kak");
    } else {
      editor = strdup(editorToCheck);
    }
    char *path_env = getenv("PATH");
    error(!path_env, "program", "getenv(\"PATH\") failed to get your path. NoteWrapper is unable to check if your desired editor is installed\n");
    debug("Your PATH is %s\n", path_env);
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
  size_t stringLenght = strlen(string);
  for (size_t i = 0; i < stringLenght; i++) {
    if ((!isalnum((unsigned char)string[i]) && strchr("~/\\:*?\"\'|!$[]{}<>\n\r\t", string[i]))) { // replace unwanted chars by '_'
        string[i] = '_';
    }
  }
  // removes any leading '.
  size_t i = 0;
  while (i < stringLenght && string[i] == '.') {
    string[i++] = '_';
  }
}

//  both functions are from https://stackoverflow.com/a/5467788
// from what i understood:
// remove() can't delete directories with files
// so it walks the file tree and deletes it's content before removing the directory
int unlink_cb(const char *filePath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
  (void)sb;
  (void)typeflag;
  (void)ftwbuf;

  int shouldDebug = 1; //normally shouldDebug should be passed to the function. However, it's to difficult here and the best it to set it to 1.

  // some sanity checks to be sure that we don't delete something we shouldn't
  error(strcmp(filePath, "") == 0, "program", "filePath is empty. Refusing to delete directory");
  error(filePath[0] == '.', "program", "%s starts with \".\". For security reasons, use absolute path and not relative path.", filePath);

  int rv = remove(filePath);
  error(rv, "program", "remove() failed to delete %s", filePath);
  return rv;
}

int rmrf(char *path, int shouldDebug) {
  error(strcmp(path, "/") == 0, "program", "Refusing to delete root directory");
  return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}


int openEditor(char *path, char *editor, int render, int shouldJumpToEndOfFile, int shouldDebug) {

// this ensures that ncurses won't affect the editor behaviour
pid_t editor_pid = fork();
error(editor_pid < 0, "program", "fork() failed.");

// instead of reusing part of the codes, for any new editor copy an example and adapt it. This is in case we need a custom fix for an editor.

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
  // ---- MICRO ----
  } else if (strcmp(editor, "micro") == 0) {

    if (render) {
      if (shouldJumpToEndOfFile) {
        debug("Running MICRO_VIVIFY=1 micro %s +99999", path);
        setenv("MICRO_VIVIFY", "1", 1);
        execlp("micro", "micro", path, "+999999", NULL);
      } else {
        debug("Running MICRO_VIVIFY=1 micro %s", path);
        setenv("MICRO_VIVIFY", "1", 1);
        execlp("micro", "micro", path, NULL);
      }
    } else {
      if (shouldJumpToEndOfFile) {
        debug("Running micro %s +999999", path);
        execlp("micro", "micro", path, "+999999", NULL);
      } else {
        debug("Running micro %s", path);
        execlp("micro", "micro", path, NULL);
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
      // ---- HELIX ----
  } else if (strcmp(editor, "helix") == 0) {

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

    // Now run helix (this replaces the child process)
    if (shouldJumpToEndOfFile) {
      strncat(path, ":99999", PATH_MAX);
      debug("Running hx %s", path);
      execlp("hx", "hx", path, NULL);
    } else {
      debug("Running hx %s", path);
      execlp("hx", "hx", path, NULL);
    }

    error(1, "program", "execlp() failed.");
  }
    // ---- kakoune ----
  else if (strcmp(editor, "kakoune") == 0) {

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
      debug("Running kak + %s", path);
      execlp("kak", "kak", "+", path, NULL);
    } else {
      debug("Running kak %s", path);
      execlp("kak", "kak", path, NULL);
    }

    error(1, "program", "execlp() failed.");
  // ---- VI ----
  } else if (strcmp(editor, "vi") == 0) {

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

    // Now run vi (this replaces the child process)
    if (shouldJumpToEndOfFile) {
      debug("Running vi +:$ %s", path);
      execlp("vi", "vi", "+:$", path, NULL);
    } else {
      debug("Running vi %s", path);
      execlp("vi", "vi", path, NULL);
    }

    error(1, "program", "execlp() failed.");
  // ---- UNKNOWN EDITOR ----
  } else {
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
