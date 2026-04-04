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
    // all of the argument parsing is done after so flags overwrite config options. The only config options that can't be set in the config is the shouldDebug
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            shouldDebug = 1;
        }
    }

    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    // this part handles the config.json file
    
    // gets the home directory
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    //(TODO LATER) maybe add a flag to specify path to config 
    // check if the config file exists
    char configPath[PATH_MAX];
    snprintf(configPath, sizeof(configPath), "%s/.config/notewrapper/config.json", homedir);
    debug("Path to the config file is %s", configPath);
    error(stat(configPath, &(struct stat){0}) == -1, "program", "The config file %s does not exist.\nMaybe try the default path to the config ~/.config/notewrapper/config.json\nIf it still does not work, compiling the program with make should create a valid config file.", configPath); // if the config directory does not exist

    // (TODO LATER) This might lack of shouldDebug info
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
    error(readBytes!=size, "program", "Failed to read config file (&zu bytes read, expected %ld)", readBytes, size);
    data[size] = '\0';
    fclose(f);

    // parse the JSON
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {free(data);}
    error(!json, "program", "JSON parse error");
    
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
    } else {debug("config.json did not contained a correct \"render\" value. The default is 1.");}
    
    int shouldJumpToEnd = 1;
    cJSON *shouldJumpToEndJSON = cJSON_GetObjectItem(json, "jumpToEndOfFileOnLaunch");
    if (shouldJumpToEndJSON && cJSON_IsBool(shouldJumpToEndJSON)) {
      shouldJumpToEnd = cJSON_IsTrue(shouldJumpToEndJSON) ? 1 : 0;
    } else {debug("config.json did not contained a correct \"jumpToEndOfFileOnLaunch\" value. The default is 1.");}
    
    char *editorToOpen = "neovim"; // default
    cJSON *editorToOpenJSON = cJSON_GetObjectItem(json, "editor");
    if (editorToOpenJSON && cJSON_IsString(editorToOpenJSON)) {
      debug("Editor in config.json is %s", editorToOpenJSON->valuestring);
      error(!isStringInArray(editorToOpenJSON->valuestring, supportedEditor, numEditors), "user", "%s (fetched from config.json) is not a supported editor.");
      editorToOpen = strdup(editorToOpenJSON->valuestring); // we must strdup and not just = as we will free all the json after (before parsing args)
    } else {debug("config.json did not contained a correct \"editor\" value. The default is neovim.");}
    
    cJSON *journalRegexJSON = cJSON_GetObjectItem(json, "journalRegex");
    char *journalRegex = ".*journal.*"; // default regex pattern for the journal
    if (journalRegexJSON && cJSON_IsString(journalRegexJSON)) {
      debug("The regex in config.json is %s", journalRegexJSON->valuestring);
      journalRegex = strdup(journalRegexJSON->valuestring);
    } else {debug("config.json did not contained a correct \"journalRegex\" value. The default is .*journal.*");}

    char *timeFormat = "# \%a \%d \%m \%Y";// default
    cJSON *timeFormatJSON = cJSON_GetObjectItem(json, "dateEntry");
    if (timeFormatJSON && cJSON_IsString(timeFormatJSON)) {
      debug("The time format in config.json is %s", timeFormatJSON->valuestring);
      timeFormat = strdup(timeFormatJSON->valuestring);
    } else {debug("config.json did not contained a correct \"dateEntry\" value. The default is \"# \%a \%d \%m \%Y\".");}

    int newLineOnOpening = 1;
    cJSON *newLineOnOpeningJSON = cJSON_GetObjectItem(json, "newLineOnOpening");
    if (newLineOnOpeningJSON && cJSON_IsBool(newLineOnOpeningJSON)) {
      debug("The value for newLineOnOpening in config.json is %d", cJSON_IsTrue(newLineOnOpeningJSON));
      newLineOnOpening = cJSON_IsTrue(newLineOnOpeningJSON) ? 1 : 0;
    } else {debug("config.json did not contained a correct \"newLineOnOpening\" value. The default is true.");}

    //cleans up 
    cJSON_Delete(json);
    free(data);
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    
    // flags and arguments overwrite the config
    // (TODO LATER) maybe add a way to combine flags (such which rm -fr?)
    // (TODO LATER) add a version flag
    // (TODO LATER) separate -n for journals from notes
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
          error(i+1==argc, "user", "Missing argument. Please use -d <path/to/directory> or --directory <path/to/directory>.");
          notesDirectoryString = argv[i+1];
          // (TODO LATER) Add a check if there is a arg after, if it is a directory, expand $, work with . and .., check if there is a dir.
          // it works with .. and . if the dir exists
          // (TODO LATER) Add shouldDebug info for this flag and others
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--vault") == 0) {
          error(i+1==argc, "user", "Missing argument. Pleaase use -v <vault's name> or --vault <vault's name>");
          bypassVaultSelection = argv[i+1]; // (TODO LATER) Add security checks pass ti strndup. and if vault don't exist create one. SEE (TODO LATER) where bypassVaultSelection is checked
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--note") == 0) { // (TODO LATER) Broken if we put the -v flag after the -n flag
          error(i+1==argc, "user", "Missing argument. Please user -n <note's name> or --note <note's name>.");
          error(strcmp(bypassVaultSelection, HASH_MACRO) == 0, "user", "If you want to specify the note, you must also specify the vault with -v <vault's name> or --vault <vault's name>.");
          bypassNoteSelection = argv[i+1];
        } else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--jump") == 0) {
          shouldJumpToEnd = 1;
        } else if (strcmp(argv[i], "-J") == 0 || strcmp(argv[i], "--no-jump") == 0) {
          shouldJumpToEnd = 0;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--render") == 0) {
          shouldRender = 1;
        } else if (strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--no-render") == 0) {
          shouldRender = 0;
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--editor") == 0) {
          error(i+1==argc, "user", "Missing argument. Please use -e <editor> or --editor <editor>.");
          editorToOpen = argv[i+1];
          debug("Editor specified with -e or --editor is %s", editorToOpen);
        }
    }
    error(!doesEditorExist(editorToOpen, shouldDebug), "user", "%s is either not in your path or not installed.", editorToOpen);

    int shouldExit = 0;
    while(!shouldExit) {
      // this loop is the vault selector
      size_t vaultsCount = 0;
      char **vaultsArray = getVaultsFromDirectory(notesDirectoryString, &vaultsCount, shouldDebug);
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
        bypassVaultSelection = HASH_MACRO; // we must reset bypassVaultSelection to not get stuck in a infinite loop of bypassing
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
          if (strcmp(bypassNoteSelection, HASH_MACRO) != 0) {
            // (TODO LATER) Add shouldDebug info
            noteSelected = bypassNoteSelection;
            if (isStringInArray(noteSelected, (const char **)filesArray, filesCount + extraNotesOptions)) {// (TODO LATER) Handle the case where the note name is one of the extraOptions
              goto open_note;
            } else { // if the specified note doesn't exist. We creat it
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
            bypassNoteSelection =  HASH_MACRO; // we must reset bypassNoteSelection to avoid getting into an infinite loop of bypassing the note selection
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
            if (newLineOnOpening) { //(TODO LATER) For some reason this does not applies to journals?
              appendToFile(fullPath, "\n", shouldDebug);
            }
            openEditor(fullPath, editorToOpen, shouldRender, shouldJumpToEnd, shouldDebug);
            free(fullPath);
          } else if (strcmp(noteSelected,"Create new note") == 0) {
note_creation:
            char *pathForNoteCreation = createNewNote(notesDirectoryString, vaultSelected, bypassNoteSelection, shouldDebug);
            // (TODO LATER) Handle journal creation
            bypassNoteSelection =  HASH_MACRO; // we must reset bypassNoteSelection to avoid getting into an infinite loop of bypassing the note selection
            if (newLineOnOpening) {
              appendToFile(pathForNoteCreation, "\n", shouldDebug);
            }
            openEditor(pathForNoteCreation, editorToOpen, shouldRender, shouldJumpToEnd, shouldDebug);
            //free(pathForNoteCreation);
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
        createNewVault(notesDirectoryString, shouldDebug);
      } else if (strcmp(vaultSelected,"Settings") == 0) {
        // (TODO LATER) add a way to modify the path to config.json
        openEditor(configPath, editorToOpen, 0, 0, shouldDebug); // as this is not a md file we set render and jumptoEnfOfFile to 0
      } else if (strcmp(vaultSelected,"Quit (Ctrl+C)") == 0) {
        debug("The program was exited");
        shouldExit = 1;
      }
    }
    return 0;
}
