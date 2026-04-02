#include "ui.h"

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

char* ncursesSelect(char **options, char *optionsText, size_t optionsNumber, size_t extraOptionsNumber, int debug) {
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
