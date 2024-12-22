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
      echo "Entering Nix-Shell"
      make
      echo "Exitin Nix-Shell"
      exit
  '';
}
