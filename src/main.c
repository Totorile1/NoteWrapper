#include "ui.h"
#include "utils.h"
#include "notes.h"

int main(int argc, char *argv[]) {
    int shouldDebug = 0;
    int overwriteConfigPath = 0; 
    for (int i = 1; i < argc; i++) {
    char *arg = argv[i];

    // Long options
    if (strncmp(arg, "--", 2) == 0) {
        if (strcmp(arg, "--verbose") == 0) {
            shouldDebug = 1;

        } else if (strcmp(arg, "--config") == 0) {
            error(i + 1 == argc, "user",
                  "Missing argument. Use --config <path>");
            overwriteConfigPath = ++i; // consume argument

        }

    // Short options (allow grouping like -V)
    } else if (arg[0] == '-' && arg[1] != '\0') {
        for (int j = 1; arg[j] != '\0'; j++) {
            char opt = arg[j];

            switch (opt) {
                case 'V':
                    shouldDebug = 1;
                    break;

                case 'c':
                    // must NOT be combined (needs argument)
                    error(arg[j+1] != '\0', "user",
                          "-c cannot be combined (use -c <path>)");
                    error(i + 1 == argc, "user",
                          "Missing argument for -c");

                    overwriteConfigPath = ++i; // consume argument
                    goto arg_next;

                default:
                    // ignore unknown here OR handle error
                    break;
            }
        }
    }

arg_next:
    ;
}
    // gets the home directory
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
 
    initAppFilesAndDirs(homedir, shouldDebug);
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------    
    // this part handles the config.json file
    // gets the home directory
    char *configPath = malloc(PATH_MAX);
    if (overwriteConfigPath) {
      configPath = argv[overwriteConfigPath];
      debug("-c or --config specified the config file %s", configPath);
    } else {
      snprintf(configPath, PATH_MAX, "%s/.config/notewrapper/config.json", homedir);
      debug("Path to the config file is %s", configPath);
    }
    // check if the config file exists
    struct stat st = {0};
    error(stat(configPath, &st) == -1, "user", "The config file %s does not exist.\nMaybe try the default path to the config ~/.config/notewrapper/config.json\nOr if you used the flag -c or --config, verifiy that you point to the correct file.", configPath); // if the config directory does not exist

    // opens config.json
    FILE *f = fopen(configPath, "r");
    error(!f, "program", "The config file does exist, but can not be open.");

    // loads and read the config file
    //gets the size
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);
    //gets the data
    char *data = malloc(size+1);
    error(!data, "program", "malloc failed allocating memory for the variable data.");
    size_t readBytes = fread(data, 1, size, f); // 1 --> size of each item
    
    if (readBytes!=size) {
      free(data);
      fclose(f);
    }
    error(readBytes!=size, "program", "Failed to read config file (%s) (%zu bytes read, expected %ld)", configPath, readBytes, size);
    data[size] = '\0';
    fclose(f);

    // parse the JSON
    debug("Parsing the JSON config");

    cJSON *json = cJSON_Parse(data);
    if (!json) {free(data);}
    error(!json, "program", "JSON parse error");
   
    cJSON *dirJson = cJSON_GetObjectItem(json, "directory");
    char *notesDirectoryString = malloc(PATH_MAX);
    if (dirJson && cJSON_IsString(dirJson) && cJSON_GetStringValue(dirJson) != NULL) { // we replaced all the ->valuestr and ->valueint to cJSON_GetStringValue() and cJSON_GetNumberValue()
      char *rawPath = cJSON_GetStringValue(dirJson);
      if (rawPath[0] == '~') { // expands ~ in the path
        snprintf(notesDirectoryString, PATH_MAX, "%s/%s", homedir, rawPath+1);
      } else {
        notesDirectoryString = rawPath;
      }
      debug("In %s, \"directory\" was set to %s.", configPath,  notesDirectoryString);
    } else {
      // default vault path if it is not set in the config
      snprintf(notesDirectoryString, PATH_MAX, "%s/Documents/Notes/", homedir);
      debug("In %s, \"directory\" wasn't set or we encountered a abnormal type. Defaulting to %s.", configPath, notesDirectoryString);
    }

    // fetch the render and jumpToEnfOfFileOnLaunch bools
    int shouldRender = 1;
    cJSON *shouldRenderJSON = cJSON_GetObjectItem(json, "render");
    if (shouldRenderJSON && cJSON_IsBool(shouldRenderJSON)) {
        shouldRender = cJSON_IsTrue(shouldRenderJSON) ? 1 : 0;
        debug("In %s, \"render\" was set to %d.", configPath, shouldRender);
    } else {
        debug("In %s, \"render\" wasn't set. Defaulting to true.", configPath);
    }
    
    int shouldJumpToEnd = 1;
    cJSON *shouldJumpToEndJSON = cJSON_GetObjectItem(json, "jumpToEndOfFileOnLaunch");
    if (shouldJumpToEndJSON && cJSON_IsBool(shouldJumpToEndJSON)) {
      shouldJumpToEnd = cJSON_IsTrue(shouldJumpToEndJSON) ? 1 : 0;
      debug("In %s, \"jumpToEndOfFileOnLaunch\" was set to %d.", configPath, shouldJumpToEnd);

    } else {
      debug("In %s, \"jumpToEndOfFileOnLaunch\" wasn't set or we encountered a abnormal type. Defaulting to true.", configPath);
    }
    
    char *editorToOpen = "neovim"; // default
    cJSON *editorToOpenJSON = cJSON_GetObjectItem(json, "editor");
    if (editorToOpenJSON && cJSON_IsString(editorToOpenJSON)) {
      editorToOpen = strdup(cJSON_GetStringValue(editorToOpenJSON)); // we must strdup and not just = as we will free all the json after (before parsing args)
      debug("In %s, \"editor\" was set to %s.", configPath, editorToOpen);
      error(!isStringInArray(editorToOpen, supportedEditor, numEditors), "user", "%s (fetched from config.json) is not a supported editor.", editorToOpen);
    } else {
      debug("In %s, \"editor\" wasn't set or we encountered a abnormal type. Defaulting to %s.", configPath, editorToOpen);
    }
    
    cJSON *journalRegexJSON = cJSON_GetObjectItem(json, "journalRegex");
    char *journalRegex = ".*journal.*"; // default regex pattern for the journal
    if (journalRegexJSON && cJSON_IsString(journalRegexJSON)) {
      journalRegex = strdup(cJSON_GetStringValue(journalRegexJSON));
      debug("In %s, \"journalRegex\" was set to %s.", configPath, journalRegex);
    } else {
      debug("In %s, \"journalRegex\" wasn't set or we encountered a abnormal type. Defaulting to %s.", configPath, journalRegex);
    }

    char *timeFormat = "# \%Y \%m \%d \%a";// default
    cJSON *timeFormatJSON = cJSON_GetObjectItem(json, "dateEntry");
    if (timeFormatJSON && cJSON_IsString(timeFormatJSON)) {
      timeFormat = strdup(cJSON_GetStringValue(timeFormatJSON));
      debug("In %s, \"dateEntry\" was set to %s.", configPath, timeFormat);
    } else {
      debug("In %s, \"dateEntry\" wasn't set or we encountered a abnormal type. Defaulting to %s.", configPath, timeFormat);
    }
    int newLineOnOpening = 1;
    cJSON *newLineOnOpeningJSON = cJSON_GetObjectItem(json, "newLineOnOpening");
    if (newLineOnOpeningJSON && cJSON_IsBool(newLineOnOpeningJSON)) {
      debug("The value for newLineOnOpening in config.json is %d", cJSON_IsTrue(newLineOnOpeningJSON));

      newLineOnOpening = cJSON_IsTrue(newLineOnOpeningJSON) ? 1 : 0;
      debug("In %s, \"newLineOnOpening\" was set to %d.", configPath, newLineOnOpening);
    } else {
      debug("In %s, \"newLineOnOpening\" wasn't set or we encountered a abnormal type. Defaulting to true.", configPath);
    }

    int doesBackup = 0;
    int interval = 0; // this is an int. But some times it will be inputed a string. We must translate it.
    char *pathToBackup = malloc(PATH_MAX);
    cJSON *backupJSON = cJSON_GetObjectItem(json, "backup");
    if (backupJSON && cJSON_IsObject(backupJSON)) {
      cJSON *doesBackupJSON = cJSON_GetObjectItem(backupJSON, "enable");
      if (doesBackupJSON && cJSON_IsBool(doesBackupJSON)) {
        doesBackup = cJSON_IsTrue(doesBackupJSON) ? 1 : 0;
        debug("doesBackup is set to %d", doesBackup);
        cJSON *pathToBackupJSON = cJSON_GetObjectItem(backupJSON, "directory");
        if (pathToBackupJSON && cJSON_IsString(pathToBackupJSON)) {
          char *temp = cJSON_GetStringValue(pathToBackupJSON); // we name it temp. As we can't directly set pathToBackup because snprintf doesn't like when a pointer is both an arg and the destination // temp should be freed when free(json), because cJSON_GetStringValue returns a pointer.
          if (temp[0] == '~') { // expands ~
            temp++; // we shift by one to remove ~
            snprintf(pathToBackup, PATH_MAX, "%s/%s", homedir, temp);
            debug("~ was expanded to %s\nThe backup path is %s", homedir, pathToBackup);
          } else {
            pathToBackup = strndup(temp, PATH_MAX); // temp will be freed later so strndup
            debug("Directory for backup is set to %s in config.json", pathToBackup);
          }
        } else {error(1, "user", "config.json did not contained a directory inside the backup section or the value is from an unexpected type");}
        cJSON *intervalJSON = cJSON_GetObjectItem(backupJSON, "interval");
        if (intervalJSON && cJSON_IsString(intervalJSON)) {
          char *temp = cJSON_GetStringValue(intervalJSON); // se comment higher. temp will be freed when calling free(json)
          if (strcmp(temp, "daily") == 0) { 
            interval = DAILY;
            debug("interval in config.json is set to \"daily\" which is %d", DAILY);
          } else if (strcmp(temp, "weekly") == 0) {
            interval = WEEKLY;
            debug("interval in config.json is set to \"weekly\" which is %d", WEEKLY);
          } else if (strcmp(temp, "monthly") == 0) {
            interval = MONTHLY;
            debug("interval in config.json is set to \"monthly\" which is %d", MONTHLY);
          } else {error(1, "user", "Unexpected string %s for entry \"interval\" in config.json. You must put an int (number of seconds) or \"daily\" or \"weekly\" or \"monthly\".", intervalJSON->valuestring);}
        } else if (intervalJSON && cJSON_IsNumber(intervalJSON)) {
          interval = (int)cJSON_GetNumberValue(intervalJSON);
          debug("interval in config.json is set to %d", interval);
        } else {error(1, "user", "config.json did not contained an interval value inside the backup section or the value is from an unexpected type");}
      } else{error(1, "user", "config.json did not contained a enable value inside the backup section or the value is from an unexpected type");}
    } else {
      debug("In %s, \"backup\" wasn't set or we encountered a abnormal type. Defaulting to {\"enable\": false}.", configPath);
    }
    //cleans up 
    cJSON_Delete(json);
    free(data);
    debug("Finished parsing the JSON config");
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    
    // flags and arguments overwrite the config
    // (TODO LATER) maybe add a way to combine flags (such which rm -fr?)
    // (TODO LATER) add a version flag
    debug("Parsing the attribute flags.\n This flags might overwrite the options in the config file.");
    // thoses bypasses are used if some specific flags are passed as args. This allows to bypass the TUI selectors
    int bypassSelectionVault = 0;
    char *bypassSelectionVaultValue = NULL;
    int bypassSelectionNote = 0;
    char *bypassSelectionNoteValue = NULL;
for (int i = 1; i < argc; i++) {
    char *arg = argv[i];

    if (strncmp(arg, "--", 2) == 0) {

        if (strcmp(arg, "--verbose") == 0) {
            shouldDebug = 1;
            debug("--verbose enabled");

        } else if (strcmp(arg, "--config") == 0) {
            error(i + 1 == argc, "user", "Missing argument for --config");
            overwriteConfigPath = ++i;
            debug("--config set to %s", argv[i]);

        } else if (strcmp(arg, "--directory") == 0) {
            error(i + 1 == argc, "user", "Missing argument for --directory");
            notesDirectoryString = argv[++i];
            debug("--directory set to %s", notesDirectoryString);

        } else if (strcmp(arg, "--editor") == 0) {
            error(i + 1 == argc, "user", "Missing argument for --editor");
            editorToOpen = argv[++i];
            debug("--editor set to %s", editorToOpen);

        } else if (strcmp(arg, "--note") == 0) {
            error(i + 1 == argc, "user", "Missing argument for --note");
            bypassSelectionNote = 1;
            bypassSelectionNoteValue = argv[++i];
            debug("--note set to %s", bypassSelectionNoteValue);

        } else if (strcmp(arg, "--vault") == 0) {
            error(i + 1 == argc, "user", "Missing argument for --vault");
            bypassSelectionVault = 1;
            bypassSelectionVaultValue = argv[++i];
            debug("--vault set to %s", bypassSelectionVaultValue);

        } else if (strcmp(arg, "--render") == 0) {
            shouldRender = 1;
            debug("--render enabled");

        } else if (strcmp(arg, "--no-render") == 0) {
            shouldRender = 0;
            debug("--render disabled");

        } else if (strcmp(arg, "--jump") == 0) {
            shouldJumpToEnd = 1;
            debug("--jump enabled");

        } else if (strcmp(arg, "--no-jump") == 0) {
            shouldJumpToEnd = 0;
            debug("--jump disabled");

        } else if (strcmp(arg, "--help") == 0) {
            printf("Usage: notewrapper [options]\n");
            return 0;

        } else if (strcmp(arg, "--version") == 0) {
            printf("No released version yet...\n");
            return 0;

        } else {
            error(1, "user", "Unknown option \"%s\"", arg);
        }

    } else if (arg[0] == '-' && arg[1] != '\0') {

        for (int j = 1; arg[j] != '\0'; j++) {
            char opt = arg[j];

            switch (opt) {

                // -------- flags without arguments (can be combined) --------
                case 'r':
                    shouldRender = 1;
                    debug("-r enabled");
                    break;

                case 'R':
                    shouldRender = 0;
                    debug("-R disabled");
                    break;

                case 'j':
                    shouldJumpToEnd = 1;
                    debug("-j enabled");
                    break;

                case 'J':
                    shouldJumpToEnd = 0;
                    debug("-J disabled");
                    break;

                case 'V':
                    shouldDebug = 1;
                    debug("-V enabled");
                    break;

                case 'h':
                    printf("Usage: notewrapper [options]\n");
                    return 0;

                // -------- flags with arguments (MUST be last in group) --------
                case 'd':
                case 'e':
                case 'n':
                case 'v':
                case 'c': {

                    error(arg[j + 1] != '\0',
                          "user",
                          "-%c must not be combined with other flags", opt);

                    error(i + 1 == argc,
                          "user",
                          "Missing argument for -%c", opt);

                    char *value = argv[++i];

                    switch (opt) {
                        case 'd':
                            notesDirectoryString = value;
                            debug("-d set directory to %s", value);
                            break;

                        case 'e':
                            editorToOpen = value;
                            debug("-e set editor to %s", value);
                            break;

                        case 'n':
                            bypassSelectionNote = 1;
                            bypassSelectionNoteValue = value;
                            debug("-n set note to %s", value);
                            break;

                        case 'v':
                            bypassSelectionVault = 1;
                            bypassSelectionVaultValue = value;
                            debug("-v set vault to %s", value);
                            break;

                        case 'c':
                            overwriteConfigPath = i;
                            debug("-c set config to %s", value);
                            break;
                    }

                    goto next_arg;
                }

                default:
                    error(1, "user", "Unknown option -%c", opt);
            }
        }

    } else {
        error(1, "user", "Unexpected argument \"%s\"", arg);
    }

next_arg:
    ;
}
    // if -n or --note is set but not -v or --vaults it gives an error
    error(!bypassSelectionVault && bypassSelectionNote, "user", "If you want to specify the note, you must also specify the vault with -v <vault's name> or --vault <vault's name>.");


    error(!doesEditorExist(editorToOpen, shouldDebug), "user", "%s is either not in your path or not installed.", editorToOpen);
    debug("Finished parsing the attribute flags");

    if (doesBackup) { // (TODO LATER) when implementing multiple directories for vault we should verifiy this works.
      handleBackups(notesDirectoryString, pathToBackup, homedir, interval, shouldDebug);
    }
    
    initscr(); //initialize ncurses

    int shouldExit = 0;
    while(!shouldExit) {
      // this loop is the vault selector
      // select a vault
      char *vaultSelected = NULL;

      int vaultsCount = 0;
      char **vaultsArray = getVaultsFromDirectory(notesDirectoryString, &vaultsCount, shouldDebug);

      // bypass if -v or --vault is set
      if (bypassSelectionVault) {
        // bypasses the vault selection if the flag is -v. If the vault doesn't exist, just create a new one
        vaultSelected = bypassSelectionVaultValue;
        debug("bypassing vault selection");
        if (!isStringInArray(bypassSelectionVaultValue, (const char **)vaultsArray, vaultsCount)) { // we pass just vaultsCount and not vaultsCount + extraOptions to avoid matching with the extraOptions.
          debug("[BYPASS] %s did not exist. Creating a new vault");
          goto vault_creation;
        } else {
          goto note_selection;
          debug("[BYPASS] %s does exist. Going to note selection");

        }
      }

      qsort(vaultsArray, vaultsCount, sizeof(const char *), compareString); // sorts the vaults alphabetically
      debug("Available vaults");
      if (shouldDebug) {
        for (int i = 0; i < vaultsCount; i++) {
          altDebug("%s\n", vaultsArray[i]);
        }
        altDebug("└ ------------------------------\n");
      }
      
      // adds "create a new vault" into the vaultsArray
      const int extraOptions = 3;
      vaultsArray = realloc(vaultsArray, (vaultsCount + extraOptions)*sizeof(char*)); // resize vaultsArray to fit the extra options
      vaultsArray[vaultsCount] = "Create a new vault"; // some more options that are not vaults
      vaultsArray[vaultsCount+1] = "Settings";
      vaultsArray[vaultsCount+2] = "Quit (Ctrl+C)";
      
      
      vaultSelected = ncursesSelect(vaultsArray, "Select vault to open (Use arrows or WASD, Enter to select):", vaultsCount, extraOptions, " ", "Or select an option below", "", shouldDebug);
     
      // now that we won't use vaultsArray in this iteration of the loop, we should free it and all its elements. (As this is memory in the heap and not the stack and thus is our responsability to manage)
      for (int i = 0; i < vaultsCount; i++) {
        if (vaultSelected != vaultsArray[i]) { // i forgot this condition before. and freed the pointer equal to vaultSelected... So don't remove this condition
          free(vaultsArray[i]); // we must only free the vaults options and not the extraOptions to avoid segfault
        }
      }
      free(vaultsArray);

      debug("Selected vault: %s", vaultSelected);
      if (strcmp(vaultSelected,"Create a new vault") != 0 && strcmp(vaultSelected,"Settings") != 0 && strcmp(vaultSelected,"Quit (Ctrl+C)") != 0) {
note_selection:
        bypassSelectionVault = 0; // we must reset bypassSelectionVault to not get stuck in a infinite loop of bypassing
        int shouldChangeVault = 0;
        while (!shouldExit && !shouldChangeVault) {
          // this loop is the note selector
          int filesCount = 0;
          char **filesArray = getNotesFromVault(notesDirectoryString, vaultSelected, journalRegex, &filesCount, shouldDebug);
          
          int journalCount = 0;
          char **journalArray = getJournalsFromVault(notesDirectoryString, vaultSelected, journalRegex, &journalCount, shouldDebug);

          // appends the journal at the end of filesArray
          filesArray = realloc(filesArray, (filesCount + journalCount)*sizeof(char*));
          for (int i = 0; i < journalCount; i++) {
            filesArray[i + filesCount] = journalArray[i];
          }
          filesCount = filesCount + journalCount;

          // we sort them // (TODO LATER) THis doesn't seem to sort them
          qsort(filesArray, filesCount, sizeof(const char *), compareString);
          debug("Available notes and journals:");
          if (shouldDebug) {
            for (int i = 0; i < filesCount; i++) {
              altDebug("%s\n", filesArray[i]);
            }
            altDebug("└------------------------------\n");
          }
          // adds options
          int extraNotesOptions = 4;
          filesArray = realloc(filesArray, (filesCount + extraNotesOptions)*sizeof(char*)); // resize filesArray to fit the extra options
          filesArray[filesCount] = "Create new note";
          filesArray[filesCount+1] = "Back to vault selection";
          filesArray[filesCount+2] = "Delete vault";
          filesArray[filesCount+3] = "Quit (Ctrl+C)";
          char *noteSelected;
          // if we set to bypass the note selector
          if (bypassSelectionNote) {
            debug("We are bypassing note selection.");
            noteSelected = bypassSelectionNoteValue;
            if (isStringInArray(noteSelected, (const char **)filesArray, filesCount)) {// we just give filesCount and not filesCount + extraOptions to avoid matching with an extra options.
              debug("The note specified with -n or --note does exist. Opening it.");
              goto open_note;
            } else { // if the specified note doesn't exist. We creat it
              debug("The note specified with -n or --note doesn't exist. Creating it.");
              goto note_creation;
            }
          }
          noteSelected = ncursesSelect(filesArray, "Select note or journal to open (Use arrows or WASD, Enter to select):", filesCount, extraNotesOptions, " ", "Or select an option below", "", shouldDebug);
          // now that we won't use filesArray in this iteration of the loop, we should free it and all its elements. (As this is memory in the heap and not the stack and thus is our responsability to manage)
          for (int i = 0; i < filesCount; i++) {
            if (noteSelected != filesArray[i]) { // we must prevent noteSelected to be freed. It will cause a lot of problems
              free(filesArray[i]); // we must only free the files options and not the extraOptions to avoid segfault
            }
          }
          free(filesArray);
          debug("Selected note: %s", noteSelected);
          if (strcmp(noteSelected, "Create new note") != 0 && strcmp(noteSelected,"Back to vault selection") != 0 && strcmp(noteSelected, "Delete vault") != 0 && strcmp(noteSelected,"Quit (Ctrl+C)") != 0) {
open_note:
            bypassSelectionNote = 0; // we must reset bypassSelectionNote to avoid getting into an infinite loop of bypassing the note selection
            char *fullPath = malloc(PATH_MAX); // (TODO LATER) Find a more appropriate and descriptive name for the variable
            sprintf(fullPath, "%s/%s/%s", notesDirectoryString, vaultSelected, noteSelected); // (TODO LATER) change all sprintf to snprintf which checks for buffer size
            // if it is a journal we must update it before
            regex_t regex;
            int regexReturn = regcomp(&regex, journalRegex, 0);
            error(regexReturn, "program", "Regex compilation failed.");
            regexReturn = regexec(&regex, noteSelected, 0, NULL, 0);
            if (!regexReturn) { // if the regex matches -> it's a journal
              debug("%s is a journal. Updating it...", noteSelected);
              fullPath = updateJournal(fullPath, noteSelected, timeFormat, shouldDebug); // we return the path. As if it is a divided journal we must point to the correct entry
            }
            if (newLineOnOpening) { //(TODO LATER) For some reason this does not applies to journals? --- it does but only if we don't create a new file/entry
              appendToFile(fullPath, "\n", shouldDebug);
            }
            openEditor(fullPath, editorToOpen, shouldRender, shouldJumpToEnd, shouldDebug);
            free(fullPath);
          } else if (strcmp(noteSelected,"Create new note") == 0) {
note_creation:
            noteSelected = createNewNote(notesDirectoryString, vaultSelected, bypassSelectionNote, bypassSelectionNoteValue, journalRegex, shouldDebug);
            // we can just go back to open_note
            goto open_note;
          } else if (strcmp(noteSelected,"Back to vault selection") == 0) {
            shouldChangeVault = 1;
          } else if (strcmp(noteSelected, "Delete vault") == 0) {
            const char *yesNo[] = {"No, go back to note selection.", "Yes."};
            char *answer = ncursesSelect((char **)yesNo, "Are you sure you want to delete the entire vault? This can not be undone (Use arrows or WASD, Enter to select):", 1, 1, " ", "", "", shouldDebug); // (TODO LATER) This is ugly with Select Are you sure[...]
            debug("You answered: %s for deleting the vault %s", answer, vaultSelected);
            if (strcmp(answer, "Yes.") == 0) {
              // delete the vault after confirmation by the user
              char pathToRMRF[PATH_MAX];
              sprintf(pathToRMRF, "%s/%s", notesDirectoryString, vaultSelected);
              debug("Removed the directory: %s", pathToRMRF);
              rmrf(pathToRMRF, shouldDebug);
              shouldChangeVault = 1;
            }
          } else if (strcmp(noteSelected,"Quit (Ctrl+C)") == 0) {
            debug("The program was exited.");
            shouldExit = 1;
          }
        }

      } else if (strcmp(vaultSelected,"Create a new vault") == 0) {
vault_creation:
        createNewVault(notesDirectoryString, bypassSelectionVault, bypassSelectionVaultValue, shouldDebug);
        bypassSelectionVault = 0; // we need to reset bypassSelectionVault to avoid getting into an infinite loop of bypassing 
      } else if (strcmp(vaultSelected,"Settings") == 0) {
        openEditor(configPath, editorToOpen, 0, 0, shouldDebug); // as this is not a md file we set render and jumptoEnfOfFile to 0
      } else if (strcmp(vaultSelected,"Quit (Ctrl+C)") == 0) {
        debug("The program was exited");
        shouldExit = 1;
      }
    }
    free(configPath);
    return 0;
}
