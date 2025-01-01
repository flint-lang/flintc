{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    libxml2
    clang_19
    llvmPackages_19.libllvm
    gnumake
    zlib.static
    glibc.static
  ];

  shellHook = ''
      echo "Building started"
      export GLIBCPATH="${pkgs.glibc.static}/lib"
      export ZLIBPATH="${pkgs.zlib.static}/lib"
      make static
      echo "Build done!"
      exit
  '';
}
