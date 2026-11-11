{
  lib,
  stdenv,
  autoPatchelfHook,
  makeWrapper,

  zig,
  cmake,
  ninja,
  python3,
  llvm_22,
  lld_22,
  zlib,

  glibc,
  musl,

  fipSrc,
  tomlc17Src,

  commitHash,
  commitDate,
}:

stdenv.mkDerivation {
  pname = "flintc";
  version = "0.4.2-core";

  outputs = [
    "out"
    "lib"
    "dev"
  ];

  outputBin = "out";
  outputLib = "lib";
  outputDev = "dev";
  outputInclude = "dev";

  src = lib.cleanSourceWith {
    src = ./.;

    filter =
      path: type:

      let
        name = builtins.baseNameOf (toString path);
      in
      lib.cleanSourceFilter path type
      && !(builtins.elem name [
        "result"
        "zig-out"
        ".zig-cache"
        "vendor"
      ]);
  };

  strictDeps = true;
  __structuredAttrs = true;

  nativeBuildInputs = [
    zig
    cmake
    ninja
    python3
    llvm_22
    lld_22
    autoPatchelfHook
    makeWrapper
  ];

  buildInputs = [
    llvm_22
    lld_22
    stdenv.cc.cc.lib
    zlib
  ];

  # Disable Nixpkgs' default Zig CPU/optimization flags
  dontSetZigDefaultFlags = true;
  dontUseZigInstall = true;
  enableParallelBuilding = true;

  # Tests are not part of the initial package build yet
  doCheck = false;

  # These flags are used by both zigBuildPhase and zigInstallPhase
  zigBuildFlags = [
    "-Dllvm-dynamic"
    "-Dstdcpp-include=${stdenv.cc.cc}/include/c++/${stdenv.cc.cc.version}:${stdenv.cc.cc}/include/c++/${stdenv.cc.cc.version}/x86_64-unknown-linux-gnu:${stdenv.cc.cc}/lib/gcc/x86_64-unknown-linux-gnu/${stdenv.cc.cc.version}/include"
    "-Dstdcpp-lib=${stdenv.cc.cc.lib}/lib/libstdc++.so"
    "-Dglibc-include=${glibc.dev}/include"
    "-Dglibc-lib=${glibc}/lib"
    "-Dmusl-lib=${musl}/lib"
    "-Dllvm-include=${llvm_22.dev}/include"
    "-Dllvm-lib=${llvm_22.lib}/lib"
    "-Dlld-include=${lld_22.dev}/include"
    "-Dlld-lib=${lld_22.lib}/lib"
    "-Dzlib-lib=${zlib}/lib"
    "-Dfip-source=${fipSrc}"
    "-Dtomlc17-source=${tomlc17Src}"
    "-Dcommit-hash=${commitHash}"
    "-Dcommit-date=${commitDate}"
    "-j2"
    "--release=small"
  ];

  installPhase = ''
    runHook preInstall

    # Verify that Zig produced all expected output directories.
    for dir in bin lib include; do
      if [ ! -d "zig-out/$dir" ]; then
        echo "Missing expected Zig output directory: zig-out/$dir" >&2
        exit 1
      fi
    done

    install -d "zig-out/bin/"     "$out/bin"
    install -d "zig-out/lib/"     "$lib/lib"
    install -d "zig-out/include/" "$dev/include"

    runHook postInstall
  '';

  # Flint invokes ld.lld by name when compiling programs.
  # Make sure the matching LLD executable is available at runtime.
  postFixup = ''
    wrapProgram "$out/bin/flintc" \
      --prefix PATH : "${lib.makeBinPath [ lld_22 ]}"
  '';

  meta = {
    description = "Flint programming language compiler and language server";
    homepage = "https://github.com/flint-lang/flintc";
    license = lib.licenses.mit;
    platforms = [ "x86_64-linux" ];
    mainProgram = "flintc";

    # A normal installation should install the executable output only.
    outputsToInstall = [ "out" ];
  };
}
