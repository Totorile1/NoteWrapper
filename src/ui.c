#include "ui.h"

void createNewVault(char *dirToVault, int shouldDebug) {
  // (TODO LATER) warn if it matches the regex for the journal
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
  error(strcmp(vaultName, "") == 0, "user", "vaultName is empty"); // (TODO LATER) replace that with a warning
  debug("Inputed vaultName=%s", vaultName);
  sanitize(vaultName);
  debug("Sanitized vaultName=%s", vaultName);

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

char *createNewNote(char dirToVault[PATH_MAX], char *vaultFromDir, char *bypass, char *journalRegex, int shouldDebug) {
  // (TODO LATER) Add code to create journal
  // (TODO LATER) Add check. If the user creates a note with a name that already exists. it erases the old one
  // input from user for the name
  char *fileName = malloc(BUFFER_SIZE);
  if (strcmp(bypass, HASH_MACRO) == 0) {
    initscr();
    echo();
    keypad(stdscr, FALSE);
    clear();
    printw("Enter the name of the new note: ");
    mvprintw(3, 0, "Unsafe characters such as \\, and / will be replaced by _");
    mvprintw(4, 0, "If the name matches with the regex for a journal (%s), it will create a journal instead of a note.", journalRegex);
    move(1,1);
    wgetnstr(stdscr, fileName, BUFFER_SIZE-4); //limits the buffer to prevent overflow (-4 to account indexing and from ".md" in case we need to add it later)
    refresh();
    endwin();
  } else { // bypasses user input if we bypass is different than HASH_MACRO
    strncpy(fileName, bypass, BUFFER_SIZE-1); // (TODO LATER) Maybe add a warning if string is too big. It gets truncated
    fileName[BUFFER_SIZE-1] = '\0';
  }
  // (TODO LATER) add a way to go back to note selection
  error(strcmp(fileName, "") == 0, "user", "fileName is empty"); // replace this with a warning and add a warning if duplicate file and handle case where multiple warnings (if such case is possible)
  // check/sanitize the input
  debug("Inputed fileName=%s", fileName);
  sanitize(fileName);
  debug("Sanitized fileName=%s (We might append .md later", fileName);
  // if it matches with the journalRegex we treat it as a journal instead of a note
  regex_t regex;
  int regexReturn = regcomp(&regex, journalRegex, 0);
  error(regexReturn, "program", "Regex compilation failed.");
  regexReturn = regexec(&regex, fileName, 0, NULL, 0); // (TODO LATER) This might be an extrem edge case but
                                                       // if journalRegex is something like [...].md
                                                       // and the inputed file name does not match it
                                                       // we create it as a note
                                                       // but if fileName + ".md" matches the regex
                                                       // we will open it as a journal
  if (!regexReturn) {
    debug("%s matches with %s treating it as a journal", fileName, journalRegex);
    char **options = malloc(32); // the number of bytes is exactly what in the two strings // (TODO LATER) There might be a cleaner way
    options[0] = "Divided journal";
    options[1] = "Unified journal";
    char *optionSelected = ncursesSelect(options, "Select which type of journal you want to create  (Use arrows or WASD, Enter to select):", 2, 0, "", "", " ", shouldDebug);
    debug("%s was selected to be a %s", fileName, optionSelected);
    if (strcmp(optionSelected, options[0]) == 0) { // if it is a divided journal
      char *fileFullPath = malloc(PATH_MAX); // (TODO LATER) we use a lot of malloc. We should check for memory leaks
      snprintf(fileFullPath, PATH_MAX, "%s/%s/%s/", dirToVault, vaultFromDir, fileName);
      struct stat st = {0};
      if (stat(fileFullPath, &st) == -1) {
        mkdir(fileFullPath, 0744); // (TODO LATER) might wanna add an error if we couldn't create the dir
      } else {
        error(1, "program", "%s could not be created", fileFullPath);
      }
      free(fileFullPath);
    } else { // if it is a unified journal
      char *fileFullPath = malloc(PATH_MAX);
      // we must add .md if it doesn't have // (TODO LATER) add a warning to the journalRegex. It must match with fileName and fileName + ".md" in case we append the extension
      int len = strlen(fileName);
      if (fileName[len-3] != '.' || fileName[len-2] != 'm' || fileName[len-1] != 'd') { // there might be a cleaner way to do this
        strncat(fileName, ".md", PATH_MAX);
      }
      snprintf(fileFullPath, PATH_MAX, "%s/%s/%s", dirToVault, vaultFromDir, fileName);
      FILE *filePointer;
      filePointer = fopen(fileFullPath, "w"); // creates and opens the file (TODO LATER) Maybe check if the file really doesn't exist
      error(filePointer == NULL, "program", "The %s couldn't be created.", fileFullPath);
      fprintf(filePointer, "### %s\n", fileName); //(TODO LATER) Add a way to configure default behaviour when creating a file
      fclose(filePointer); // closes the file so that nvim could open it
      free(fileFullPath);
    }
    free(options); // they are useless now
  } else { // normal process for a note
    debug("%s does not match with %s treating it as a note", fileName, journalRegex);
    // if there is no .md add an .md
    int len = strlen(fileName);
    if (fileName[len-3] != '.' || fileName[len-2] != 'm' || fileName[len-1] != 'd') { // there might be a cleaner way to do this
      strcat(fileName, ".md"); // this should not cause an overflow issue as we get at most 252 chars (+'.'+'m'+'d'+'\0' makes it to 256) with wgetnstr
    }
    char *fileFullPath = malloc(PATH_MAX); // this dinamically allocated because we use it in the main function to call openEditor
    sprintf(fileFullPath, "%s/%s/%s", dirToVault, vaultFromDir, fileName);
    FILE *filePointer;
    filePointer = fopen(fileFullPath, "w"); // creates and opens the file (TODO LATER) Maybe check if the file really doesn't exist
    error(filePointer == NULL, "program", "The %s couldn't be created.", fileFullPath);
    fprintf(filePointer, "### %s\n", fileName); //(TODO LATER) Add a way to configure default behaviour when creating a file
    fclose(filePointer); // closes the file so that nvim could open it
    free(fileFullPath);
  }
  return fileName;
}

char* ncursesSelect(char **options, char *optionsText, size_t optionsNumber, size_t extraOptionsNumber, char *bottomText, char *middleText, char *topText, int shouldDebug) {
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
      mvprintw(0,0, "%s", optionsText);
      int offset = 1;
      if (strcmp(bottomText, "") != 0) {
        mvprintw(offset, 1, "%s", bottomText);
        offset++;
      }
      // Print options with highlighting
      for (int i = 0; i < optionsNumber; i++) {
        if (i == highlight) {
          attron(A_REVERSE);
        }   // highlight selected
        mvprintw(i+offset, 2, "%s", options[i]);
        if (i == highlight) {
          attroff(A_REVERSE);
        }
      }
      attroff(COLOR_PAIR(1));
      if (strcmp(middleText, "") != 0) {
        mvprintw(offset+optionsNumber, 1, "%s", middleText);
        offset++;
      }
      // Printf extraOptions with highlighting
      attron(COLOR_PAIR(2));
      for (int k = optionsNumber; k < optionsNumber + extraOptionsNumber; k++) {
        if (k == highlight) {
          attron(A_REVERSE);
        }
        mvprintw(k+offset, 2, "%s", options[k]);
        if (k == highlight) {
          attroff(A_REVERSE);
        }
      }
      attroff(COLOR_PAIR(2));
      attron(COLOR_PAIR(1));
      mvprintw(offset+optionsNumber+extraOptionsNumber, 1, "%s", topText);
      attroff(COLOR_PAIR(1));
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
