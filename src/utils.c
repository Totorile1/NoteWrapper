#include "utils.h"

const char *supportedEditor[] = {"neovim", "vim"};
const int numEditors = 2;

int compareString(const void *a, const void *b) {
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;
    return strcmp(str1, str2); // strcmp returns <0, 0, >0
}

int doesEditorExist (char *editorToCheck, int debug) {     // Some exectuables have not exaclty the same name as the editor.
  char *editor;   
    if (strcmp(editorToCheck, "neovim") == 0) {
      editor = strdup("nvim"); // we must use strdup and not just copy as we would have modified editorToOpen in main
    }
    else {
      editor = strdup(editorToCheck);
    }
    char *path_env = getenv("PATH");
    if (!path_env) {
      printf("\e[0;31mERROR: getenv(\"PATH\") failed to get your path. NoteWrapper is unable to check if your desired editor is installed\n");
      exit(1);
    };
    if (debug) {printf("\e[0;32m[DEBUG]\e[0m Your PATH is %s\n", path_env);}
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

int isStringInArray(const char *string, const char **array, const int len) {
  for (int i = 0; i < len; i++) {
    if (strcmp(string, array[i]) == 0) {
      return 1;
    }
  }
  return 0;
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
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

int rmrf(char *path) {
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

char** getVaultsFromDirectory(char *dirString, size_t *count, int debug) { 
    // (TODO LATER) it might be a good idea to check if these directories exist
    // (TODO LATER) expand ~ as it does not work with opendir()
    // this function is inputed a path to a directory (which comes usually from the config file) and outpus all the suitable directories (so not the hidden ones) which will serve as separate vaults for notes
    if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening %s aka the directory of vaults\n", dirString);}

    // originally from https://www.geeksforgeeks.org/c/c-program-list-files-sub-directories-directory/
    struct dirent *vaultsDirectoryEntry;
    DIR *vaultsDirectory = opendir(dirString);
        if (vaultsDirectory == NULL)  { // opendir returns NULL if couldn't open directory
        printf("\e[0;31mERROR: Could not open current directory\e[0m\n" );
        exit(1); //something is fucked up
    }
    char **dirsArray = NULL; // will contain all the dirs/vaults
    size_t dirsCount = 0; // we need to count how many dirs there is to always readjust how many memory we alloc
    // Refer https://pubs.opengroup.org/onlinepubs/7990989775/xsh/readdir.html
    // for readdir()
    if (debug) {printf("┌------------------------------\n\e[0;32m[DEBUG]\e[0m Files and dirs from the directory\n");}
    while ((vaultsDirectoryEntry = readdir(vaultsDirectory)) != NULL) {
      if (debug) {printf("%s\n", vaultsDirectoryEntry->d_name);} // debugs every file/directory
      if (vaultsDirectoryEntry->d_name[0] != '.') { // if the entry don't start with a dot (so hidden dirs and hidden files)
        char fullPathEntry[PATH_MAX]; // creates a string of size of the maximum path lenght
        snprintf(fullPathEntry, sizeof(fullPathEntry), "%s/%s", dirString, vaultsDirectoryEntry->d_name); // sets the full absolute path to fullPathEntry
        
        struct stat metadataPathEntry;
        if (stat(fullPathEntry, &metadataPathEntry) == 0 && S_ISDIR(metadataPathEntry.st_mode)) { // if this entry is a directory
          dirsArray = realloc(dirsArray, (dirsCount + 1)*sizeof(char*)); // resize dirsArray so that
          dirsArray[dirsCount] = strdup(vaultsDirectoryEntry->d_name); // copy the dir name into dirsArray
          dirsCount++;
        }
      }
    }
    if (debug) {printf("└------------------------------\n");}
    // (TODO LATER) Alphabetically sort them

    // free's some used memory
    closedir(vaultsDirectory);
    *count = dirsCount;
    return dirsArray;
} 


char** getNotesFromVault(char *pathToVault, char *vault, int *count, int debug) {
    // this function is inputed a path to a vault (which was selected before) and outpus all the suitable notes (so not the hidden ones)
    // (TODO LATER) Check how it handles non .md files
    if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening %s aka the vault\n", vault);}

    // originally from https://www.geeksforgeeks.org/c/c-program-list-files-sub-directories-directory/
    struct dirent *notesDirectoryEntry; // (TODO LATER) change name of these variables. notesDirectory is dumb as it is the directory of vaults
    char tempPath[PATH_MAX];
    snprintf(tempPath, sizeof(tempPath), "%s/%s", pathToVault, vault); // sets the full absolute path to fullPathEntry
    DIR *vaultDirectory = opendir(tempPath);
    if (vaultDirectory == NULL) {  // opendir returns NULL if couldn't open directory
      printf("\e[0;31mERROR: Could not open current directory\e[0m\n" );
      exit(1); //something is fucked up
    }
    char **notesArray = NULL; // will contain all the notes
    size_t notesCount = 0; // we need to count how many notes there is to always readjust how many memory we alloc
    // Refer https://pubs.opengroup.org/onlinepubs/7990989775/xsh/readdir.html
    // for readdir()
    if (debug) {printf("┌------------------------------\n\e[0;32m[DEBUG]\e[0m Files and dirs from the vault:\n");}
    while ((notesDirectoryEntry = readdir(vaultDirectory)) != NULL) {

      if (debug) {printf("%s\n", notesDirectoryEntry->d_name);}
      
      if (notesDirectoryEntry->d_name[0] != '.') { // if the entry don't start with a dot (so hidden dirs and hidden files)
        char fullPathEntry[PATH_MAX]; // creates a string of size of the maximum path lenght
        snprintf(fullPathEntry, sizeof(fullPathEntry), "%s/%s/%s", pathToVault, vault, notesDirectoryEntry->d_name); // sets the full absolute path to fullPathEntry
        
        struct stat metadataPathEntry;
        if (stat(fullPathEntry, &metadataPathEntry) == 0 && !S_ISDIR(metadataPathEntry.st_mode)) { // if this entry is a file
          notesArray = realloc(notesArray, (notesCount + 1)*sizeof(char*)); // resize notesArray so that
          notesArray[notesCount] = strdup(notesDirectoryEntry->d_name); // copy the dir name into notesArray
          notesCount++;
        }
      }
    }
    if (debug) {printf("└ ------------------------------\n");}
    // (TODO LATER) Alphabetically sort them
    // free's some used memory
    closedir(vaultDirectory);
    *count = notesCount; // passes the number of files
    return notesArray;
}


int openEditor(char *path, char *editor, int render, int endOfFile, int debug) {
  // (TODO LATER) find better name for endOfFile
  //(TODO LATER) for nvim and vim we should check if there is swap files or recovery files and handle that
  pid_t pid = fork(); // this forking allows the programs to return when nvim is closed
  if (pid < 0) {
    perror("\e[0;31mERROR: fork failed\e[0m\n");
    return 1;
  } else if (pid == 0) {
      // Child process: replace with editor of choice
    if (strcmp(editor, "neovim") == 0) { // opens with Neovim 
    //(TODO LATER) we should (with a config option) append a new line every time it opens
      if (render) { // don't render using vivify
        if (endOfFile) { // goes to the end of the file on opening. (TODO LATER) find a better way to do this loops. Maybe an array of args and if () we add the arg to the array and we pass the whole array to execlp
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nnvim +:$ +:Vivify %s\n", path);} // :$ goes to the end of the file. :Vivify runs vivify
            execlp("nvim", "nvim", "+:$", "+:Vivify", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Nvim might be not installed or not in path.\e[0m\n");
            exit(1); // (TODO LATER) This exist only if error or everytime. If it does every time change to exit(0);
        } else { // don't go to the end of the file
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nnvim +:Vivify %s\n", path);}
            execlp("nvim", "nvim", "+:Vivify", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Nvim might be not installed or not in path.\e[0m\n");
            exit(1);
        }
      } else { // don't render using vivify
        if (endOfFile) { // go to end of the file on opening
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nnvim +:$ %s\n", path);}
            execlp("nvim", "nvim", "+:$", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Nvim might be not installed or not in path.\e[0m\n");
            exit(1);
        } else { // don't go to the end of the file on opening
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nnvim %s\n", path);}
            execlp("nvim", "nvim", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Nvim might be not installed or not in path.\e[0m\n");
            exit(1);
        }
      }
    } else if (strcmp(editor, "vim") == 0) { // opens with Vim // see comments for neovim for explanations
    //(TODO LATER) we should (with a config option) append a new line every time it opens
      if (render) {
        if (endOfFile) {
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nvim +:$ +:Vivify %s\n", path);}
            execlp("vim", "vim", "+:$", "+:Vivify", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Vim might be not installed or not in path.\e[0m\n");
            exit(1);
        } else {
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nvim +:Vivify %s\n", path);}
            execlp("vim", "vim", "+:Vivify", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Vim might be not installed or not in path.\e[0m\n");
            exit(1);
        }
      } else {
        if (endOfFile) {
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nvim +:$ %s\n", path);}
            execlp("vim", "vim", "+:$", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Vim might be not installed or not in path.\e[0m\n");
            exit(1);
        } else {
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nvim %s\n", path);}
            execlp("vim", "vim", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Vim might be not installed or not in path.\e[0m\n");
            exit(1);
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
