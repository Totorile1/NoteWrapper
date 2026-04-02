#include "ui.h"
#include "utils.h"
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
      if (debug) {printf("\e[0;32m[DEBUG]\e[0m Editor in config.json is %s\n", editorToOpenJSON->valuestring);}
      if (!isStringInArray(editorToOpenJSON->valuestring, supportedEditor, numEditors)) { // if we don't support this editor
        printf("\e[0;31mERROR: %s (fetched from config.json) is not a supported editor. Supported editors are ", editorToOpenJSON->valuestring);
        for (int i = 0; i < numEditors; i++) {
          if (i < numEditors-2) {printf("\e[0;32m%s\e[0;31m, ", supportedEditor[i]);} // this if () {} else {} is used to get something like this "editor1, editor2, [...], editorn-2, editorn-1 and editorn"
          else if (i == numEditors-2) {printf("\e[0;32m%s\e[0;31m and ", supportedEditor[i]);}
          else {printf("\e[0;32m%s\e[0;31m.", supportedEditor[i]);}
        }
        printf("\e[0m\n");
        exit(1);
      }
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
          if (i+1 == argc) { // if -nd or --directory was the last argument
              printf("\e[0;31mERROR: Missing argument. Please use -d <path/to/directory> or --directory <path/to/directory>.\e[0m\n");
              exit(1);
          }
          notesDirectoryString = argv[i+1];
          // (TODO LATER) Add a check if there is a arg after, if it is a directory, expand $, work with . and .., check if there is a dir.
          // it works with .. and . if the dir exists
          // (TODO LATER) Add debug info for this flag and others
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--vault") == 0) {
          if (i+1 == argc) { // if -v or --vault was the last argument
              printf("\e[0;31mERROR: Missing argument. Please use -v <vault's name> or --vault <vault's name>.\e[0m\n");
              exit(1);
          }
          bypassVaultSelection = argv[i+1]; // (TODO LATER) Add security checks pass ti strndup. and if vault don't exist create one. SEE (TODO LATER) where bypassVaultSelection is checked
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--note") == 0) { // (TODO LATER) Broken if we put the -v flag after the -n flag
          if (i+1 == argc) { // if -n or --note was the last argument
              printf("\e[0;31mERROR: Missing argument. Please use -n <note's name> or --note <note's name>.\e[0m\n");
              exit(1);
          } else if (strcmp(bypassVaultSelection, HASH_MACRO) == 0) { // bypassVaultSelection is initialized to HASH_MACRO, if it is not changed it means -v wasn't specified
            printf("\e[0;31mERROR: If you want to specify the note, you must also specify the vault with -v <vault's name> or --vault <vault's name>\e[0m\n");
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
            if (i+1 == argc) { // if -e or --editor was the last argument
              printf("\e[0;31mERROR: Missing argument. Please use -e <editor> or --editor <editor>.\e[0m\n");
              exit(1);
            }
            editorToOpen = argv[i+1];
            if (debug) {
              printf("\e[0;32m[DEBUG]\e[0m Editor specified with -e or --editor is %s\n", editorToOpen);
            }
            if (!isStringInArray(editorToOpen, supportedEditor, numEditors)) { // if we don't support this editor
              printf("\e[0;31mERROR: %s (specified with -e or --editor) is not a supported editor. Supported editors are ", editorToOpen);
              for (int i = 0; i < numEditors; i++) {
                if (i < numEditors-2) {
                  printf("\e[0;32m%s\e[0;31m, ", supportedEditor[i]); // this if () {} else {} is used to get something like this "editor1, editor2, [...], editorn-2, editorn-1 and editorn"
                } else if (i == numEditors-2) {
                  printf("\e[0;32m%s\e[0;31m and ", supportedEditor[i]);
                } else {printf("\e[0;32m%s\e[0;31m.", supportedEditor[i]);
                }
              }
              printf("\e[0m\n");
              exit(1);
            }
        }
    }

    if (!doesEditorExist(editorToOpen, debug)) { // check if the editor is in your PATH
      printf("\e[0;31mERROR: %s is either not in your path or not installed.\e[0m\n", editorToOpen);
      exit(1);
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
            if (isStringInArray(noteSelected, (const char **)filesArray, filesCount + extraNotesOptions)) {// (TODO LATER) Handle the case where the note name is one of the extraOptions
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
