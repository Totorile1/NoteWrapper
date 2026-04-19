#include "ui.h"
#include <stdio.h>

void createNewVault(char *dirToVault, int bypass, char *bypassvalue, int shouldDebug) {
  int duplicateWarning = 0; // set to 1 later if the vault you tried to create already existed
  int emptyWarning = 0; // set to 1 later if inpted empty name
input_screen:
  char *vaultName = malloc(PATH_MAX);
  if (!bypass) { // if won't bypass (if -v or --vault weren't set)
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
    if (emptyWarning) {
      attron(COLOR_PAIR(2));
      mvprintw(5, 0, "The vault's name must not be empty. Please input another name");
      attroff(COLOR_PAIR(2));
    }
    move(1, 1); // replace cursor 
    wgetnstr(stdscr, vaultName, PATH_MAX-1);
    refresh();
    endwin();
    reset_shell_mode();
    fflush(stdout);
    fflush(stderr);
  } else {
    strncpy(vaultName, bypassvalue, PATH_MAX -2); // -2 (and later -1) because indexing
    vaultName[PATH_MAX-1] = '\0';
  }
  // check if no empty string
  if (strcmp(vaultName, "") == 0) {
    emptyWarning = 1;
    goto input_screen;
  }
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
  free(vaultName);
}

char *createNewNote(char dirToVault[PATH_MAX], char *vaultFromDir, int bypass, char *bypassvalue, char *journalRegex, int shouldDebug) {
  // input from user for the name
  char *fileName = malloc(BUFFER_SIZE);
  if (!bypass) { // if we don't bypass. (if -n or --note weren't set.)
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
    reset_shell_mode();
    fflush(stdout);
    fflush(stderr);
  } else { // bypasses user input if we bypass is set to 1
    strncpy(fileName, bypassvalue, BUFFER_SIZE);
    fileName[BUFFER_SIZE-1] = '\0';
  }
  error(strcmp(fileName, "") == 0, "user", "fileName is empty"); // replace this with a warning and add a warning if duplicate file and handle case where multiple warnings (if such case is possible)
  // check/sanitize the input
  debug("Inputed fileName=%s", fileName);
  sanitize(fileName);
  debug("Sanitized fileName=%s (We might append .md later", fileName);
  // we trow an error if the file already exists (previous behaviour was overwriting the file)
  char path[PATH_MAX];
  snprintf(path, PATH_MAX, "%s/%s/%s", dirToVault, vaultFromDir, fileName);
  struct stat st;
  error(stat(path, &st) == 0, "user", "%s already exists", fileName);
  // if it matches with the journalRegex we treat it as a journal instead of a note
  regex_t regex;
  int regexReturn = regcomp(&regex, journalRegex, 0);
  error(regexReturn, "program", "Regex compilation failed.");
  regexReturn = regexec(&regex, fileName, 0, NULL, 0);
  if (!regexReturn) {
    debug("%s matches with %s treating it as a journal", fileName, journalRegex);
    char **options = malloc(32); // the number of bytes is exactly what in the two strings
    options[0] = "Divided journal";
    options[1] = "Unified journal";
    char *optionSelected = ncursesSelect(options, "Select which type of journal you want to create  (Use arrows or WASD, Enter to select):", 2, 0, "", "", " ", shouldDebug);
    debug("%s was selected to be a %s", fileName, optionSelected);
    if (strcmp(optionSelected, options[0]) == 0) { // if it is a divided journal
      char *fileFullPath = malloc(PATH_MAX);
      snprintf(fileFullPath, PATH_MAX, "%s/%s/%s/", dirToVault, vaultFromDir, fileName);
      struct stat st = {0};
      if (stat(fileFullPath, &st) == -1) {
        error(!mkdir(fileFullPath, 0744), "program", "mkdir failed");
      } else {
        error(1, "program", "%s could not be created", fileFullPath);
      }
      free(fileFullPath);
    } else { // if it is a unified journal
      char *fileFullPath = malloc(PATH_MAX);
      snprintf(fileFullPath, PATH_MAX, "%s/%s/%s", dirToVault, vaultFromDir, fileName);
      int len = strlen(fileFullPath);
      if (fileFullPath[len-3] != '.' || fileFullPath[len-2] != 'm' || fileFullPath[len-1] != 'd') { // there might be a cleaner way to do this
        error(len > PATH_MAX - 3, "user", "%s is too big (greater than PATH_MAX-3) and we can't append .md", fileFullPath);
        strncat(fileFullPath, ".md", PATH_MAX);
        // we checked before if fileName didn't already exist.
        // we must redo it as we add a .mode
        struct stat st;
        error(stat(fileFullPath, &st) == 0, "user", "%s already exists", fileFullPath);
      }
      FILE *filePointer;
      filePointer = fopen(fileFullPath, "w"); // creates and opens the file
      error(filePointer == NULL, "program", "The %s couldn't be created.", fileFullPath);
      fprintf(filePointer, "### %s\n", fileName);
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
    filePointer = fopen(fileFullPath, "w"); // creates and opens the file
    error(filePointer == NULL, "program", "The %s couldn't be created.", fileFullPath);
    fprintf(filePointer, "### %s\n", fileName);
    fclose(filePointer); // closes the file so that nvim could open it
    free(fileFullPath);
  }
  return fileName;
}

char* fzfSelect(char *pathToFiles, char *selectText, int shouldDebug) {
    // we are gonna write each line of each files (with the filename and the line number to an index)
    char command[CMD_BUFFER];
    char indexFile[] = "/tmp/notewrapper_index_XXXXXX";

    int fd = mkstemp(indexFile);
    error(fd == -1, "program", "mkstemp failed");
    close(fd);

    /*
     * STEP 1:
     * Build index once into temp file
     * format: file:line:content
     */
    snprintf(command, sizeof(command),
        "rg --line-number --no-heading --color=never . \"%s\" > %s",
        pathToFiles,
        indexFile
    );
    
    debug("INDEX CMD: %s", command);

    if (system(command) != 0) {
        unlink(indexFile);
        error(1, "program", "rg indexing failed");
    }

    /*
     * STEP 2:
     * Use fzf on the index file (NO rg anymore)
     */
    /*
      EXPLANATION:
      
      cat %s
        → feeds prebuilt index file (format: file:line:content) into fzf
      
      fzf --delimiter ':'
        → splits each line into fields:
           {1} = file path
           {2} = line number
           {3} = content
      
      --prompt
        → UI prompt text shown in fzf
      
      --preview
        → runs a shell command for selected item:
           file={1}  → current file
           line={2}  → line number of match
      
      nl -ba "$file"
        → prints file with line numbers
      
      sed -n "$((line-5)),$((line+5))p"
        → extracts a window of 5 lines above and below match
      
      sed "... -> ..."
        → highlights the matched line (middle of window) with red arrow
      
      --preview-window=right:60%:hidden
        → preview appears on right, 60% width, initially hidden
      
      --bind 'change:show-preview'
        → preview only appears after first selection change (not at startup)
    */
    snprintf(command, sizeof(command),
             "cat %s | fzf --delimiter ':' --prompt='%s' "
             "--preview 'file={1}; line={2}; nl -ba \"$file\" | sed -n \"$((line-5)),$((line+5))p\" | sed \"$((line-$(($((line-5))))+1))s/^/\\x1b[31m-> \\x1b[0m/\"' "
             "--preview-window=right:60%%:hidden "
             "--bind 'change:show-preview'",
             indexFile,
             selectText ? selectText : "> "
    );
    debug("FZF CMD: %s", command);

    FILE *fzfPipe = popen(command, "r");
    if (!fzfPipe) {
        unlink(indexFile);
        error(1, "program", "popen failed");
    }

    char buffer[RESULT_BUFFER];
    char *result = NULL;

    if (fgets(buffer, sizeof(buffer), fzfPipe)) {
        buffer[strcspn(buffer, "\n")] = '\0';
        result = strdup(buffer);
    }

    // cut at first ':'
    result = strtok(result, ":");
    // cut at / (because we need the full paths before and we only need to return the journal entry)
    char *token = strtok(result, "/");
    result = NULL;
    while (token != NULL) {
      result = token;
      token = strtok(NULL, "/"); //Passing NULL means “don’t start a new string, resume
    }

    // clean up
    pclose(fzfPipe);
    unlink(indexFile);

    return result;
}

char* ncursesSelect(char **options, char *optionsText, int optionsNumber, int extraOptionsNumber, char *bottomText, char *middleText, char *topText, int shouldDebug) {
    int highlight = 0; //curently highlighted option
    int key;

    cbreak();               // disable line buffering
    noecho();               // don't echo key presses
    keypad(stdscr, TRUE);   // enable arrow keys 
    start_color();
    curs_set(0); // sets the cursor to invisible. (We will emulate the cursor with a hightlight that go up and down).
    use_default_colors();
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
    fflush(stderr);
    debug("Selected option: %s", options[highlight]);
    return options[highlight];
}
