{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    libxml2
    clang_19
    llvmPackages_19.libllvm
    gnumake
    cmake
    zlib.static
    glibc.static
  ];

  shellHook = ''
      export GLIBCPATH="${pkgs.glibc.static}/lib"
      export ZLIBPATH="${pkgs.zlib.static}/lib"

      echo "Nix shell entered successfully!"
      echo "The next step is to call './build.sh'"
      echo "This script will do the rest for you!"
  '';
}
