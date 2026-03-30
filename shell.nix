{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = [
    pkgs.vivify
    pkgs.cjson
    pkgs.ncurses
    pkgs.pkg-config
    pkgs.gdb
  ];
}
