### The idea

config file in json

requierements neovim, nvim-tree, vivify, vivify.vim

1. launches a TUI
- asks if you want to open an already existing directory (by default in ~/Documents/Notes but can be changed in the config file) or create a new directory
(this would work like Obsidian's vaults)
when it creates a new directory creates a Welcome.md explaining

2. launches nvim inside this dir with nvim +:NvimTreeOpen + way to open the Welcome.md (or by default first file?)
3. launches the viewer with vivify
some way to specify the browser



things to do:
1. port vivify.vim to nixos (got problems see `https://discourse.nixos.org/t/help-with-adding-a-vim-plugin-in-nixpkgs/76682`)
2. i'm gonna work on the TUI for now.



a option to make a file a journal. --> the code will append the file with ### and the date. and when nvim opens the file it will go to the end

a way inside the json to modifiy the welcome.md
per vault config inside the json file
a tui configurator that writes to the json
an option or save in another part for backups (like periodically just put the directory you want to save in config)

a lot of options. via the config.json or via flags.
a option/flag to not render.


later to port it to Nix
