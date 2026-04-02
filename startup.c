#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h> // for waitpid
#include <limits.h> // for PATH_MAX
#include <ncurses.h>
#include <unistd.h>
#include <ctype.h>
#include <ftw.h> // used for deleting dirs
#include <pwd.h> // for getting home directory
#define HASH_MACRO "0ea1d20bcdd52c46c086d3dba125b9b83ad8cbea2e026d5646775f48bae8f867" // if the user inputs this hash. The program brokes. It was the best way i found to see if some values were unchanged
// small helper functions here. Big ones are after
int compareString(const void *a, const void *b) { // this will be the function used by qsort
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;
    return strcmp(str1, str2); // strcmp returns <0, 0, >0
}

int isStringInArray(char *string, char **array, int len) {
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

// big critical functions here. Small ones are before
char *createNewNote(char dirToVault[PATH_MAX], char *vaultFromDir, char *bypass, int debug) {
  // (TODO LATER) Add check. If the user creates a note with a name that already exists. it erases the old one
  // input from user for the name
  char fileName[256];
  if (strcmp(bypass, HASH_MACRO) == 0) {
    initscr();
    echo();
    keypad(stdscr, FALSE);
    clear();
    printw("Enter the name of the new note: ");
    mvprintw(3, 0, "Unsafe characters such as \\, and / will be replaced by _"); // (TODO LATER) add more info
    move(1,1);
    wgetnstr(stdscr, fileName, sizeof(fileName)-4); //limits the buffer to prevent overflow (-4 to account indexing and from ".md" in case we need to add it later)
    refresh();
    endwin();
  } else { // bypasses user input if we bypass is different than HASH_MACRO
    strncpy(fileName, bypass, sizeof(fileName)-1); // (TODO LATER) Maybe add a warning if string is too big. It gets truncated
    fileName[sizeof(fileName)-1] = '\0';
  }
  // (TODO LATER) add a way to go back to note selection
  if (strcmp(fileName, "") == 0) {
    printf("\e[0;32mERROR: fileName is empty\e[0m\n"); // (TODO LATER) it should just go back to the note creation with a warning
    exit(1);
  }
  // check/sanitize the input
  if (debug) {printf("\e[0;32m[DEBUG]\e[0m inputed fileName=%s\n", fileName);}
  sanitize(fileName);
  // if there is no .md add an .md
  int len = strlen(fileName);
  if (fileName[len-1] != 'd' || fileName[len-2] != 'm' || fileName[len-3] != '.') { // there might be a cleaner way to do this
    strcat(fileName, ".md"); // this should not cause an overflow issue as we get at most 252 chars (+'.'+'m'+'d'+'\0' makes it to 256) with wgetnstr
  }
  if (debug) {printf("\e[0;32m[DEBUG]\e[0m sanitize(fileName)=%s\n", fileName);} // (TODO LATER) Check if dirToVault+vaultFromDir+fileName > PATH_MAX

  char *fileFullPath = malloc(PATH_MAX); // this dinamically allocated because we use it in the main function to call openEditor
  sprintf(fileFullPath, "%s/%s/%s", dirToVault, vaultFromDir, fileName);
  FILE *filePointer;
  filePointer = fopen(fileFullPath, "w"); // creates and opens the file
  if (filePointer == NULL) {
    printf("\e[0;31mERROR: The file couldn't be created. Something went wrong.\e[0m\n");
    free(fileFullPath);
    exit(1);
  }
  fprintf(filePointer, "### %s\n", fileName); //(TODO LATER) Add a way to configure default behaviour when creating a file
  fclose(filePointer); // closes the file so that nvim could open it
  return fileFullPath;
}

void createNewVault(char *dirToVault, int debug) {
  //(TODO LATER) add a way to go back to vault selection
  int duplicateWarning = 0; // set to 1 later if the vault you tried to create already existed
input_screen:
  char vaultName[256];
  initscr();
  echo();
  keypad(stdscr, FALSE);
  // color code from https://stackoverflow.com/a/73396575
  start_color();
  clear();
  use_default_colors();
  init_pair(1, COLOR_WHITE, -1); // -1 is blank
  init_pair(2, COLOR_RED, -1);
  attron(COLOR_PAIR(1));
  printw("Enter the name of the new vault: ");
  mvprintw(3, 0, "Unsafe characters such as \\ and / will be replaced by _");
  attroff(COLOR_PAIR(1));
  if (duplicateWarning) {
    attron(COLOR_PAIR(2));
    mvprintw(4, 0, "A vault with this name already exists. Please input another name");
    attroff(COLOR_PAIR(2));
  }
  move(1, 1); // replace cursor 
  wgetnstr(stdscr, vaultName, sizeof(vaultName)-1);
  refresh();
  endwin();
  if (strcmp(vaultName, "") == 0) {
    printf("\e[0;31mERROR: vaultName is empty\e[0m\n");
    exit(1);
  }
  if (debug) {printf("\e[0;32m[DEBUG]\e[0m inputed vaultName=%s\n", vaultName);}
  sanitize(vaultName);
  if (debug) {printf("\e[0;32m[DEBUG]\e[0m sanitized vaultName=%s\n", vaultName);}
  
  struct stat st = {0}; // https://stackoverflow.com/a/7430262
  char vaultFullPath[PATH_MAX];
  sprintf(vaultFullPath, "%s/%s/", dirToVault, vaultName);
  if (stat(vaultFullPath, &st) == -1) {
    mkdir(vaultFullPath, 0744);
  } else {
    // if stat(...) != -1 it means the vault already exist. We will go back to the input screen with a new message.
    duplicateWarning = 1;
    goto input_screen;
  }
  
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

char* ncursesSelect(char **options, char *optionsText, size_t optionsNumber, size_t extraOptionsNumber, int debug) {
    // this function is used multiple times let the user select one options from many.
    // options is the array of strings with all the options.
    // one of this option will be returned
    // optionsText is the text that will be printed at the top (For example: "Please select ...")
    // we distinguish options from extraOptions
    // options could be the list of all notes or all vaults
    // extraOptions are options that are special and have a special color (for ex: "Delete vault", "Settings", etc.)
    // note: extraOptions should be at the end of options

    int highlight = 0; //curently highlighted option
    int key;

    initscr(); //initialize ncurses
    cbreak();               // disable line buffering
    noecho();               // don't echo key presses
    keypad(stdscr, TRUE);   // enable arrow keys 
    start_color();
    curs_set(0); // sets the cursor to invisible. (We will emulate the cursor with a hightlight that go up and down).
    use_default_colors(); //(TODO LATER) customize these colors in the config file
    init_pair(1, COLOR_WHITE, -1); // color for optionsNumber (so notes, vaults, etc.)
    init_pair(2, COLOR_BLUE, -1); // color for extraOptionsNumber (so settings, delete notes, create vault, etc.)
    while (1) {
      clear();
      attron(COLOR_PAIR(1));
      mvprintw(0,0, "Select %s (Use arrows or WASD, Enter to select):", optionsText);
      // Print options with highlighting
      for (int i = 0; i < optionsNumber; i++) {
        if (i == highlight) {
          attron(A_REVERSE);
        }   // highlight selected
        mvprintw(i+2, 2, "%s", options[i]);
        if (i == highlight) {
          attroff(A_REVERSE);
        }
      }
      attroff(COLOR_PAIR(1));
      // Printf extraOptions with highlighting
      attron(COLOR_PAIR(2));
      for (int k = optionsNumber; k < optionsNumber + extraOptionsNumber; k++) {
        if (k == highlight) {
          attron(A_REVERSE);
        }
        mvprintw(k+2, 2, "%s", options[k]);
        if (k == highlight) {
          attroff(A_REVERSE);
        }
      }
      attroff(COLOR_PAIR(2));
      key = getch(); //get some int which value correspond to some key being pressed

      switch(key) {
        case KEY_UP:
        case 'w':
        case 'W':
          highlight--;
          if (highlight < 0) {
            highlight = optionsNumber + extraOptionsNumber - 1;// can't select the -1nth option
          }
          break;
        case 's':
        case 'S':
        case KEY_DOWN:
          highlight++;
          if (highlight >= optionsNumber + extraOptionsNumber) {
            highlight = 0;
          }
          break;
        case 10: //\n
          goto end_loop;
      }
    }
    end_loop:
    endwin(); // end ncurses mode
    return options[highlight];
}


int openEditor(char *path, char *editor, int render, int endOfFile, int debug) {
  pid_t pid = fork(); // this forking allows the programs to return when nvim is closed
  if (pid < 0) {
    perror("\e[0;31mERROR: fork failed\e[0m\n");
    return 1;
  } else if (pid == 0) {
      // Child process: replace with editor of choice
    if (strcmp(editor, "neovim") == 0) { 
    //(TODO LATER) we should (with a config option) append a new line every time it opens
      if (render) { // don't render using vivify
        if (endOfFile) { // goes to the end of the file on opening. (TODO LATER) find a better way to do this loops. Maybe an array of args and if () we add the arg to the array and we pass the whole array to execlp
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nnvim +:$ +:Vivify %s\n", path);} // :$ goes to the end of the file. :Vivify runs vivify
            execlp("nvim", "nvim", "+:$", "+:Vivify", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Nvim might be not installed or not in path.\e[0m\n"); // (TODO LATER) We should verify at the start when the editor is choosen if it exists
            exit(1); // (TODO LATER) This exist only if error or everytime. If it does every time change to exit(0);
        } else { // don't go to the end of the file
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nnvim +:Vivify %s\n", path);}
            execlp("nvim", "nvim", "+:Vivify", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Nvim might be not installed or not in path.\e[0m\n"); // (TODO LATER) We should verify at the start when the editor is choosen if it exists
            exit(1);
        }
      } else { // don't render using vivify
        if (endOfFile) { // go to end of the file on opening
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nnvim +:$ %s\n", path);}
            execlp("nvim", "nvim", "+:$", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Nvim might be not installed or not in path.\e[0m\n"); // (TODO LATER) We should verify at the start when the editor is choosen if it exists
            exit(1);
        } else { // don't go to the end of the file on opening
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Opening with command:\nnvim %s\n", path);}
            execlp("nvim", "nvim", path, NULL);
            perror("\e[0;31mERROR: execlp failed. Nvim might be not installed or not in path.\e[0m\n"); // (TODO LATER) We should verify at the start when the editor is choosen if it exists
            exit(1);
        }
      }
    }
  } else {
    // Parent process: wait for child to finish
    int status;
    waitpid(pid, &status, 0);
  }
  return 0;
}


 


int main(int argc, char *argv[]) {

    int debug = 0;
    // all of the argument parsing is done after so flags overwrite config options. The only config options that can't be set in the config is the debug
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            debug = 1;
        }
    }

    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    // this part handles the config.json file
    
    // gets the home directory
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    
    // check if the config file exists
    char configPath[PATH_MAX];
    snprintf(configPath, sizeof(configPath), "%s/.config/notewrapper/config.json", homedir);
    if (debug) {printf("\e[0;32m[DEBUG]\e[0m the path to the config file is %s\n", configPath);}
    if (stat(configPath, &(struct stat){0}) == -1) { // if the config directory do not exist
      printf("\e[0;31mERROR: the config file (\e[0;32m$/.config/notewrapper/config.json\e[0;31m) does not exist.\nCompiling the program with \e[0;32mmake\e[0;31m should solve this error.\e[0m\n");
      exit(1);
    }
    
    // (TODO LATER) This might lack of debug info
    // opens config.json
    FILE *f = fopen(configPath, "r");
    if (!f) {
      printf("\e[0;31mERROR: The config file does exist, but can not be open. Something went wrong.\e[0;32m\n");
      exit(1);
    }

    // loads and read the config file
    //gets the size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    //gets the data
    char *data = malloc(size+1);
    if (!data) {
      fclose(f);
      printf("\e[0;31mERROR: malloc failed allocating memory for the variable data. Something went wrong.\e[0m\n");
      exit(1);
    }
    size_t readBytes = fread(data, 1, size, f); // 1 --> size of each item
    if (readBytes != size) {
      printf("\e[0;31mERROR: Failed to read config file (%zu bytes read, expected %ld)\e[0m\n", readBytes, size);
      free(data);
      fclose(f);
      exit(1);
    }
    data[size] = '\0';
    fclose(f);

    // parse the JSON
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {
      printf("\e[0;31mERROR: JSON parse error\e[0m\n");  // (TODO LATER) replace all printf("\e[0;31m with fpintf(stderr( "\e[0;31m and maybe debug messages too
      free(data);
      exit(1);
    }
    // (TODO LATER) maybe add a default vault option
    cJSON *dirJson = cJSON_GetObjectItem(json, "directory");
    char *notesDirectoryString = malloc(PATH_MAX);
    if (dirJson && cJSON_IsString(dirJson) && dirJson->valuestring != NULL) {
      char *rawPath = dirJson->valuestring;
      if (rawPath[0] == '$') { // expands $ in the path
        snprintf(notesDirectoryString, PATH_MAX, "%s/%s", homedir, rawPath+1);
      } else {
        notesDirectoryString = rawPath;
      }
    } else {
      // default vault path if it is not set in the config
      snprintf(notesDirectoryString, PATH_MAX, "%s/Documents/Notes/", homedir);
    }

    // fetch the render and jumpToEnfOfFileOnLaunch bools // (TODO LATER) For all this JSON output warnings if there is some json but not the expected type. And add a warning if unsupported editor. if warning -> default
    int shouldRender = 1;
    cJSON *shouldRenderJSON = cJSON_GetObjectItem(json, "render");
    if (shouldRenderJSON && cJSON_IsBool(shouldRenderJSON)) {
        shouldRender = cJSON_IsTrue(shouldRenderJSON) ? 1 : 0;
    }
    int shouldJumpToEnd = 1;
    cJSON *shouldJumpToEndJSON = cJSON_GetObjectItem(json, "jumpToEndOfFileOnLaunch");
    if (shouldJumpToEndJSON && cJSON_IsBool(shouldJumpToEndJSON)) {
      shouldJumpToEnd = cJSON_IsTrue(shouldJumpToEndJSON) ? 1 : 0;
    }
    char *editorToOpen = "neovim"; // default
    cJSON *editorToOpenJSON = cJSON_GetObjectItem(json, "editor");
    if (editorToOpenJSON || cJSON_IsString(editorToOpenJSON)) {
      editorToOpen = strdup(editorToOpenJSON->valuestring); // we must strdup and not just = as we will free all the json after (before parsing args)
    }
    //cleans up 
    cJSON_Delete(json);
    free(data);
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    
    // flags and arguments overwrite the config
    // (TODO LATER) maybe add a way to combine flags (such which rm -fr?)
    // (TODO LATER) add a version flag
    char *bypassVaultSelection = HASH_MACRO; // (TODO LATER) find a better idea. If later it detects other string than that 256 string. It will bypass the selection
    char *bypassNoteSelection = HASH_MACRO;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
          printf("Usage: notewrapper [options]\n");
          printf("Options:\n");
          printf("  -d, --directory <path/to/directory>         Specify the vaults' directory.\n");
          printf("  -h, --help                                  Display this message.\n");
          printf("  -e, --editor                                Specify the editor to open.\n");
          printf("  -j, --jump                                  Jumps to the end of the file on opening.\n");
          printf("  -J, --no-jump                               Do not jump to the end of the file\n");
          printf("  -n, --note  <note's name>                   Specify the note.");
          printf("  -r, --render                                Renders the note with Vivify.\n");
          printf("  -R, --no-render                             Do not render.\n");
          printf("  -v, --vault <vault's name>                  Specify the vault.\n");
          printf("  --version                                   Display the program version.\n");
          printf("  -V, --verbose                               Show debug information.\n");
          return 1;
        // (TODO LATER) Organize alphabetically these conditions
        } else if (strcmp(argv[i], "--version") == 0) {
          printf("There is still no released version\n");
          return 0;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--directory") == 0) {
          notesDirectoryString = argv[i+1];
          // (TODO LATER) Add a check if there is a arg after, if it is a directory, expand $, work with . and .., check if there is a dir.
          // it works with .. and . if the dir exists
          // (TODO LATER) Add debug info for this flag and others
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--vault") == 0) {
          bypassVaultSelection = argv[i+1]; // (TODO LATER) Add security checks pass ti strndup. and if vault don't exist create one. SEE (TODO LATER) where bypassVaultSelection is checked
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--note") == 0) { // (TODO LATER) Broken if we put the -v flag after the -n flag
          if (strcmp(bypassVaultSelection, HASH_MACRO) == 0) {
            printf("\e[0;31mERROR: If you want to specify the note, you must also specify the vault with -v\e[0m\n");
            exit(1);
          } else {
            bypassNoteSelection = argv[i+1];
          }

        } else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--jump") == 0) {
          shouldJumpToEnd = 1;
        } else if (strcmp(argv[i], "-J") == 0 || strcmp(argv[i], "--no-jump") == 0) {
          shouldJumpToEnd = 0;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--render") == 0) {
          shouldRender = 1;
        } else if (strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--no-render") == 0) {
          shouldRender = 0;
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--editor") == 0) {
          editorToOpen = argv[i+1];
        }
    }

    int shouldExit = 0;
    while(!shouldExit) {
      // this loop is the vault selector
      size_t vaultsCount = 0;
      char **vaultsArray = getVaultsFromDirectory(notesDirectoryString, &vaultsCount, debug);
      qsort(vaultsArray, vaultsCount, sizeof(const char *), compareString); // sorts the vaults alphabetically
      if (debug) {
        printf("┌------------------------------\n\e[0;32m[DEBUG]\e[0m Available vaults:\n");
        for (size_t i = 0; i < vaultsCount; i++) {
          printf("%s\n", vaultsArray[i]);
        }
        printf("└ ------------------------------\n");
      }
      
      // adds "create a new vault" into the vaultsArray
      const int extraOptions = 3;
      vaultsArray = realloc(vaultsArray, (vaultsCount + extraOptions)*sizeof(char*)); // resize vaultsArray to fit the extra options
      // (TODO LATER) add a way to Colorize the extraOptions
      vaultsArray[vaultsCount] = "Create a new vault"; // some more options that are not vaults
      vaultsArray[vaultsCount+1] = "Settings";
      vaultsArray[vaultsCount+2] = "Quit (Ctrl+C)";
      // select a vault
      char *vaultSelected = NULL;

      if (strcmp(bypassVaultSelection, HASH_MACRO) != 0) {
        // bypasses the vault selection if the flag is -v (TODO LATER) use isStringInArray() to check if the vault really exists  
        vaultSelected = bypassVaultSelection;
          goto note_selection;
      }
      vaultSelected = ncursesSelect(vaultsArray, "Select vault (Use arrows or WASD, Enter to select):", vaultsCount, extraOptions, debug);
      
      // now that we won't use vaultsArray in this iteration of the loop, we should free it and all its elements. (As this is memory in the heap and not the stack and thus is our responsability to manage)
      for (int i = 0; i < vaultsCount; i++) {
        if (vaultSelected != vaultsArray[i]) { // i forgot this condition before. and freed the pointer equal to vaultSelected... So don't remove this condition
          free(vaultsArray[i]); // we must only free the vaults options and not the extraOptions to avoid segfault
        }
      }
      free(vaultsArray);

      if (debug) {printf("\e[0;32m[DEBUG]\e[0m Selected vault:%s\n", vaultSelected);}
      
      if (strcmp(vaultSelected,"Create a new vault") != 0 && strcmp(vaultSelected,"Settings") != 0 && strcmp(vaultSelected,"Quit (Ctrl+C)") != 0) {
note_selection:
        bypassVaultSelection = HASH_MACRO; // we must reset bypassVaultSelection to not get stuck in a infinite loop of bypassing
        int shouldChangeVault = 0;
        while (!shouldExit && !shouldChangeVault) {
          // this loop is the note selector
          int filesCount = 0;
          char **filesArray = getNotesFromVault(notesDirectoryString, vaultSelected, &filesCount, debug);
          qsort(filesArray, filesCount, sizeof(const char *), compareString); // sorts the notes alphabetically
          
          if (debug) {
            printf("┌------------------------------\n\e[0;32m[DEBUG]\e[0m Available notes:\n");
            for (size_t i = 0; i < filesCount; i++) {
              printf("%s\n", filesArray[i]);
            }
            printf("└ ------------------------------\n");
          }
          // adds options
          int extraNotesOptions = 4;
          filesArray = realloc(filesArray, (filesCount + extraNotesOptions)*sizeof(char*)); // resize filesArray to fit the extra options
          filesArray[filesCount] = "Create new note";
          filesArray[filesCount+1] = "Back to vault selection";
          filesArray[filesCount+2] = "Delete vault";
          filesArray[filesCount+3] = "Quit (Ctrl+C)";
          char *noteSelected;
          if (strcmp(bypassNoteSelection, HASH_MACRO) != 0) {
            // (TODO LATER) Add debug info
            noteSelected = bypassNoteSelection;
            if (isStringInArray(noteSelected, filesArray, filesCount + extraNotesOptions)) {// (TODO LATER) Handle the case where the note name is one of the extraOptions
              goto open_note;
            } else { // if the specified note doesn't exist. We creat it
              goto note_creation;
            }
          }
          noteSelected = ncursesSelect(filesArray, "Select note (Use arrows or WASD, Enter to select):", filesCount, extraNotesOptions, debug);
          // now that we won't use filesArray in this iteration of the loop, we should free it and all its elements. (As this is memory in the heap and not the stack and thus is our responsability to manage)
          for (int i = 0; i < filesCount; i++) {
            if (noteSelected != filesArray[i]) { // we must prevent noteSelected to be freed. It will cause a lot of problems
              free(filesArray[i]); // we must only free the files options and not the extraOptions to avoid segfault
            }
          }
          free(filesArray);
          if (debug) {printf("\e[0;32m[DEBUG]\e[0m Selected note: %s\n", noteSelected);}
          
          if (strcmp(noteSelected, "Create new note") != 0 && strcmp(noteSelected,"Back to vault selection") != 0 && strcmp(noteSelected, "Delete vault") != 0 && strcmp(noteSelected,"Quit (Ctrl+C)") != 0) {
open_note:
            bypassNoteSelection =  HASH_MACRO; // we must reset bypassNoteSelection to avoid getting into an infinite loop of bypassing the note selection
            char fullPath[PATH_MAX]; // (TODO LATER) Find a more appropriate and descriptive name for the variable
            sprintf(fullPath, "%s/%s/%s", notesDirectoryString, vaultSelected, noteSelected); // (TODO LATER) change all sprintf to snprintf which checks for buffer size
            openEditor(fullPath, editorToOpen, shouldRender, shouldJumpToEnd, debug);
          } else if (strcmp(noteSelected,"Create new note") == 0) {
note_creation:
            char *pathForNoteCreation = createNewNote(notesDirectoryString, vaultSelected, bypassNoteSelection, debug);
            bypassNoteSelection =  HASH_MACRO; // we must reset bypassNoteSelection to avoid getting into an infinite loop of bypassing the note selection
            openEditor(pathForNoteCreation, editorToOpen, shouldRender, shouldJumpToEnd, debug);
            //free(pathForNoteCreation);
          } else if (strcmp(noteSelected,"Back to vault selection") == 0) {
            shouldChangeVault = 1;
          } else if (strcmp(noteSelected, "Delete vault") == 0) {
            const char *yesNo[] = {"No, go back to note selection.", "Yes."};
            char *answer = ncursesSelect((char **)yesNo, "Are you sure you want to delete the entire vault? This can not be undone.", 1, 1, debug);
            if (debug) {printf("\e[0;32m[DEBUG]\e[0mYou answered: \e[0;32m%s\e[0m for deleting the vault named \e[0;32m%s\e[0m\n", answer, vaultSelected);}
            if (strcmp(answer, "Yes.") == 0) {
              // delete the vault after confirmation by the user
              char pathToRMRF[PATH_MAX];
              sprintf(pathToRMRF, "%s/%s", notesDirectoryString, vaultSelected);
              if (debug) {printf("\e[0;32m[DEBUG]\e[0m Removed the directory: \e[0;32m%s\e[0m\n", pathToRMRF);}
              rmrf(pathToRMRF);
              shouldChangeVault = 1;
            }
          } else if (strcmp(noteSelected,"Quit (Ctrl+C)") == 0) {
            if (debug) {printf("\e[0;32m[DEBUG]\e[0m The program was exited,\n");}
            shouldExit = 1;
          }
        }

      } else if (strcmp(vaultSelected,"Create a new vault") == 0) {
        createNewVault(notesDirectoryString, debug);
      } else if (strcmp(vaultSelected,"Settings") == 0) {
        // (TODO LATER) add a way to modify the path to config.json
        openEditor(configPath, editorToOpen, 0, 0, debug); // as this is not a md file we set render and jumptoEnfOfFile to 0
      } else if (strcmp(vaultSelected,"Quit (Ctrl+C)") == 0) {
        if (debug) {printf("\e[0;32m[DEBUG]\e[0m The program was exited.\n");}
        shouldExit = 1;
      }
    }
    return 0;
}
