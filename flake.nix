{
  description = "Nix build for the Flint compiler";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

    # Zig 0.17
    zigpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

    # Flint Interop Protocol 0.4.1
    fipSrc = {
      url = "github:flint-lang/fip/306dc342e37258832bdc216c049cd3cdaa312245";
      flake = false;
    };

    # FIP's TOML dependency
    tomlc17Src = {
      url = "github:cktan/tomlc17/R250712";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      zigpkgs,
      fipSrc,
      tomlc17Src,
    }:

    let
      system = "x86_64-linux";

      pkgs = import nixpkgs { inherit system; };
      zigPkgs = import zigpkgs { inherit system; };

      commitHash =
        if self ? shortRev then
          self.shortRev
        else if self ? dirtyShortRev then
          self.dirtyShortRev
        else
          "unknown";

      sourceDate = if self ? lastModifiedDate then self.lastModifiedDate else "19700101";

      commitDate = "${builtins.substring 0 4 sourceDate}-${builtins.substring 4 2 sourceDate}-${
        builtins.substring 6 2 sourceDate
      }";

      flintc = pkgs.callPackage ./package.nix {
        zig = zigPkgs.zig_0_17;

        inherit
          fipSrc
          tomlc17Src
          commitHash
          commitDate
          ;
      };

      devShell = pkgs.mkShell {
        # Reuse the package's nativeBuildInputs and buildInputs.
        inputsFrom = [ flintc ];

        # These outputs are also needed by Flint's generated linker.
        packages = [
          pkgs.glibc
          pkgs.glibc.dev
          pkgs.musl
          pkgs.llvm_22.dev
          pkgs.llvm_22.lib
          pkgs.lld_22.dev
          pkgs.lld_22.lib
        ];

        DEV_ENVIRONMENT = "flintc";

        shellHook = ''
          export FLINTC_ROOT="$(${pkgs.git}/bin/git rev-parse --show-toplevel 2>/dev/null || pwd)"

          flintc() {
            "$FLINTC_ROOT/zig-out/bin/flintc" "$@"
          }

          flintc-debug() {
            "$FLINTC_ROOT/zig-out/bin/flintc-debug" "$@"
          }

          export LD_LIBRARY_PATH="${
            pkgs.lib.makeLibraryPath [
              pkgs.stdenv.cc.cc.lib
              pkgs.zlib
              pkgs.llvm_22.lib
              pkgs.lld_22.lib
            ]
          }''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

          zb() {
            command zig build \
            "-Dllvm-dynamic" \
            "-Dstdcpp-include=${pkgs.stdenv.cc.cc}/include/c++/${pkgs.stdenv.cc.cc.version}:${pkgs.stdenv.cc.cc}/include/c++/${pkgs.stdenv.cc.cc.version}/x86_64-unknown-linux-gnu:${pkgs.stdenv.cc.cc}/lib/gcc/x86_64-unknown-linux-gnu/${pkgs.stdenv.cc.cc.version}/include" \
            "-Dstdcpp-lib=${pkgs.stdenv.cc.cc.lib}/lib/libstdc++.so" \
            "-Dglibc-include=${pkgs.glibc.dev}/include" \
            "-Dglibc-lib=${pkgs.glibc}/lib" \
            "-Dmusl-lib=${pkgs.musl}/lib" \
            "-Dllvm-include=${pkgs.llvm_22.dev}/include" \
            "-Dllvm-lib=${pkgs.llvm_22.lib}/lib" \
            "-Dlld-include=${pkgs.lld_22.dev}/include" \
            "-Dlld-lib=${pkgs.lld_22.lib}/lib" \
            "-Dzlib-lib=${pkgs.zlib}/lib" \
            "-Dfip-source=${fipSrc}" \
            "-Dtomlc17-source=${tomlc17Src}" \
            "-j2" \
            "$@"
          }

          alias zbr="zb --release=small"
          alias zbt="zb test --release=small"

          echo "Flint development shell ready."
          echo "Available commands: ('...' are all extra args needed for Nix builds)"
          echo " - zb: Executes 'zig build ...'"
          echo " - zbr: Executes 'zig build --release=small ...'"
          echo " - zbt: Executes 'zig build test --release=small ...'"
        '';
      };

    in
    {
      packages.${system}.default = flintc;
      devShells.${system}.default = devShell;
    };
}
