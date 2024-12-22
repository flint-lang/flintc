{ pkgs ? import <nixpkgs> {} }:

let
  libllvm = pkgs.llvmPackages_19.libllvm.dev;
in
{
  inherit libllvm;
}
