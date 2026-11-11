{
  lib,
  stdenv,
  autoPatchelfHook,
  makeWrapper,

  zig,
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

  dontSetZigDefaultFlags = true;
  enableParallelBuilding = true;
  dontAddPrefix = true;

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

  zigInstallFlags = [
    "--prefix-exe-dir"
    "${placeholder "out"}/bin"
    "--prefix-lib-dir"
    "${placeholder "lib"}/lib"
    "--prefix-include-dir"
    "${placeholder "dev"}/include"
  ];

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
    outputsToInstall = [ "out" ];
  };
}
