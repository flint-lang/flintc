{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    libxml2
    clang_19
    llvmPackages_19.libllvm
    gnumake
    libz
  ];

  shellHook = ''
      echo "Building started"
      make
      echo "Build done!"
      exit
  '';
}
