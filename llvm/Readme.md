# LLVM

To build llvm you need `nix` installed on your system. If you have nix installed, open the tetrminal in the `llvm/` directory (this directory) and execute the command

```sh
nix-build llvm.nix -o llvm
```

This will essentially just download the correct llvm dependencies and create symlinks to the llvm-dev (the package where all llvm .h files are located). You can alternatively change this line:

```
-Illvm/llvm-dev/include
```

In the `compile_flags.txt` file to point to the directory where your .h files of llvm are located!
