#include "ui.h"
#include "utils.h"
#include "notes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <ftw.h>
#include <limits.h>
#include <ncurses.h>

int main(int argc, char *argv[]) {
    int shouldDebug = 0;
    int overwriteConfigPath = 0; 
    // all of the argument parsing is done after so flags overwrite config options. The only config options that can't be set in the config is the shouldDebug and specifing the path to the config
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            shouldDebug = 1;
        } if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
          error(i + 1 == argc, "user", "Missing argument. Please use -c <path/to/config> or --c <path/to/config>");
          // if we overwrite the config path we change overwriteConfigPath to i.
          overwriteConfigPath = i + 1;
        } 
    }
    // gets the home directory
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
 
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
    error(stat(configPath, &st) == -1, "program", "The config file %s does not exist.\nMaybe try the default path to the config ~/.config/notewrapper/config.json\nOr if you used the flag -c or --config, verifiy that you point to the correct file.\nIf it still does not work, compiling the program with make should create a valid config file in ~/.config/notewrapper.", configPath); // if the config directory does not exist

    // opens config.json
    FILE *f = fopen(configPath, "r");
    error(!f, "program", "The config file does exist, but can not be open.");

    // loads and read the config file
    //gets the size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    //gets the data
    char *data = malloc(size+1);
    error(!data, "program", "malloc failed allocating memory for the variable data.");
    size_t readBytes = fread(data, 1, size, f); // 1 --> size of each item
    
    if (readBytes!=size) {free(data);fclose(f);}
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
      if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--directory") == 0) {
          error(i+1==argc, "user", "Missing argument. Please use -d <path/to/directory> or --directory <path/to/directory>.");
          notesDirectoryString = argv[i+1];
          // (TODO LATER) does it works with .. and . if the dir exists?  
      } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
          printf("Usage: notewrapper [options]\n");
          printf("Options:\n");
          printf("  -c, --config <path/to/config>               Specify the config file.\n");
          printf("  -d, --directory <path/to/directory>         Specify the vaults' directory.\n");
          printf("  -h, --help                                  Display this message.\n");
          printf("  -e, --editor                                Specify the editor to open.\n");
          printf("  -j, --jump                                  Jumps to the end of the file on opening.\n");
          printf("  -J, --no-jump                               Do not jump to the end of the file\n");
          printf("  -n, --note  <note's name>                   Specify the note (or journal).\n");
          printf("  -r, --render                                Renders the note with Vivify.\n");
          printf("  -R, --no-render                             Do not render.\n");
          printf("  -v, --vault <vault's name>                  Specify the vault.\n");
          printf("  --version                                   Display the program version and the GPL3 notice.\n");
          printf("  -V, --verbose                               Show debug information.\n");
          return 1;
      } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--editor") == 0) {
          error(i+1==argc, "user", "Missing argument. Please use -e <editor> or --editor <editor>.");
          editorToOpen = argv[i+1];
          debug("Editor specified with -e or --editor is %s", editorToOpen);
          error(!isStringInArray(editorToOpen, supportedEditor, numEditors), "user", "%s (specified with -e or --editor) is not a supported editor.", editorToOpen);
      } else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--jump") == 0) {
          debug("-j or --jump set jumpToEnfOfFileOnLaunch to true");
          shouldJumpToEnd = 1;
      } else if (strcmp(argv[i], "-J") == 0 || strcmp(argv[i], "--no-jump") == 0) {
          debug("-j or --jump set jumpToEnfOfFileOnLaunch to false"); 
          shouldJumpToEnd = 0;
      } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--note") == 0) {// if -n is set and not -v -> problems. We verify this case at the end of the parsing.
          error(i+1==argc, "user", "Missing argument. Please user -n <note's name> or --note <note's name>.");
          bypassSelectionNote = 1;
          bypassSelectionNoteValue = argv[i+1]; // if somehow we pass a flag like -v it will create a new note with the name. So no need to worry if something is invalid. Just verifing that there is an arg after is good.
      } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--render") == 0) {
          debug("-r or --render set shouldRender to true");
          shouldRender = 1;
      } else if (strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--no-render") == 0) {
          shouldRender = 0;
          debug("-r or --render set shouldRender to false");
      } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--vault") == 0) {
          error(i+1==argc, "user", "Missing argument. Pleaase use -v <vault's name> or --vault <vault's name>");
          bypassSelectionVault = 1;
          bypassSelectionVaultValue = argv[i+1]; // (TODO LATER) Add security checks pass ti strndup. and if vault don't exist create one. SEE (TODO LATER) where bypassVaultSelection is checked (TODO LATER) Add check if vault exist
      } else if (strcmp(argv[i], "--version") == 0) {
          printf("There is still no released version\n\n     Copyright (C) 2026 Tomás Rivera\n     License GPLv3: GNU GPL version 3 <https://gnu.org/licenses/gpl.html>.\n     This is free software: you are free to change and redistribute it.\n     There is NO WARRANTY, to the extent permitted by law.\n\n     Written by Tomás Rivera.\n");
          return 0;
      } else if (argv[i][0] == '-' && strcmp(argv[i], "-V") != 0 && strcmp(argv[i], "--verbose") != 0 && strcmp(argv[i], "-c") != 0 && strcmp(argv[i], "--config")) {
          error(1, "user", "unexpected argument \"%s\" found\nFor more information, try \"--help\".", argv[i]);
      }
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

      size_t vaultsCount = 0;
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
        for (size_t i = 0; i < vaultsCount; i++) {
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
          qsort(filesArray, filesCount, sizeof(const char *), compareString); // sorts the notes alphabetically
          int journalCount = 0;
          char **journalArray = getJournalsFromVault(notesDirectoryString, vaultSelected, journalRegex, &journalCount, shouldDebug);
          qsort(journalArray, journalCount, sizeof(const char *), compareString);

          // appends the journal at the end of filesArray
          filesArray = realloc(filesArray, (filesCount + journalCount)*sizeof(char*));
          for (int i = 0; i < journalCount; i++) {
            filesArray[i + filesCount] = journalArray[i];
          }
          filesCount = filesCount + journalCount;
          debug("Available notes and journals:");
          if (shouldDebug) {
            for (size_t i = 0; i < filesCount; i++) {
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
              rmrf(pathToRMRF);
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
