<div align="center">
<p>
    <img width="100" src="https://raw.githubusercontent.com/flint-lang/logo/main/logo.svg">
    <h1>The Flint Programming Language</h1>
</p>

<p>
An approachable programming language to make power and performance accessible without bloat, in a high level package.

This repository is contains the Flint compiler itself.

</p>

<p>
    <a href="#"><img src="https://img.shields.io/badge/c++-%2300599C.svg?style=flat&logo=c%2B%2B&logoColor=white"></img></a>
    <a href="http://opensource.org/licenses/MIT"><img src="https://img.shields.io/github/license/flint-lang/flintc?color=black"></img></a>
    <a href="#"><img src="https://img.shields.io/github/stars/flint-lang/flintc"></img></a>
    <a href="#"><img src="https://img.shields.io/github/forks/flint-lang/flintc"></img></a>
    <a href="#"><img src="https://img.shields.io/github/repo-size/flint-lang/flintc"></img></a>
    <a href="https://github.com/flint-lang/flintc/graphs/contributors"><img src="https://img.shields.io/github/contributors/flint-lang/flintc?color=blue"></img></a>
    <a href="https://github.com/flint-lang/flintc/issues"><img src="https://img.shields.io/github/issues/flint-lang/flintc"></img></a>
</p>

<p align="center">
  <a href="https://flint-lang.github.io/">Documentation</a> ·
  <a href="https://github.com/flint-lang/flintc/issues">Report a Bug</a> ·
  <a href="https://github.com/flint-lang/flintc/issues">Request Feature</a> ·
  <a href="https://github.com/flint-lang/flintc/pulls">Send a Pull Request</a>
</p>

</div>

## Introduction

> [!IMPORTANT]
> Flint is not finished yet and many features are still missing, but we work hard at finishing it as quick as possible.
> Please report any issues you may encounter with the [Flint Issue Tracker](https://github.com/flint-lang/flintc/issues).

This project contains the custom **Flint Compiler** built in C++. Flint is a statically typed, compiled and fully deterministic language which sets code clarity, ease of use and performance as its very top priorities.

## Example

```rs
use Core.print

def main():
    print("Hello, World!\n");
```

## Installation

### Linux

First download the `flintc` binary for your given platform from the [Releases](https://github.com/flint-lang/flintc/releases) page. To make the Flint compiler available from any path in your terminal, and to make it executable through just calling `flintc` in your terminal, you need to copy the `flintc` executable into the `$HOME/.local/bin/` directory (if it does not exist yet, i would highly recommend to create it) and you need to ensure it is marked as executable with this command:

```sh
chmod +x $HOME/.local/bin/flintc
```

After adding the `flintc` binary to the `$HOME/.local/bin` directory you should edit your `$HOME/.bashrc` file and ensure it contains the line

```sh
PATH="$PATH:$HOME/.local/bin"
```

And then you can simply use the compiler from any terminal like so:

```sh
flintc --help
```

You need `base-devel` (Arch) or `build-essential` (Ubuntu) installed in order for the Flint compiler to be able to compile any program. It needs the `crt1.o`, `crti.o` and `crtn.o` lib files available to it.

### Windows

Installation on Windows is pretty easy, it's just a one-line command:

```ps1
powershell -NoProfile -ExecutionPolicy Bypass -Command "iex (irm 'https://github.com/flint-lang/flint/releases/download/installer/flint_installer.ps1')"
```

Or if this one-liner scares you you can still [download](https://github.com/flint-lang/flint/releases/download/installer/flint_installer.ps1) the installer directly and execute the downloaded script using the command

```ps1
PowerShell -NoProfile -ExecutionPolicy Bypass -File .\flint_installer.ps1
```

The installer will always download the latest Flint release directly and adds it to th path variable for you. You can then use the compiler using

```ps1
flintc --help
```

directly in any PowerShell or Command Prompt from any directory.

## Building

It is recommended to build the project with the `zig build` command after cloning this repository.

### Prequisites

- CMake (to build LLVM)
- Ninja (to build LLVM)
- Python (>3.8, to build LLVM)

### Linux

On Linux you need to install the packages via your package manager of choice and then it should be smooth sailing with `zig build`.

### Windows

On Windows it is recommended to simpy use `winget` to install all required dependencies for this project:

- `zig.zig` for Zig itself
- `MartinStorsjo.LLVM-MinGW.UCRT` to get `ld.lld` since Zig does not ship it, and to get the C++ `std` library headers (`<string>`, etc)
- `CMake`
- `Ninja-build.Ninja`
- `python3`

```ps1
winget install zig.zig MartinStorsjo.LLVM-MinGW.UCRT CMake Ninja-build.Ninja python3
```

After that you should have all C++ stl header libraries installed on your system, there is no need to install anything related to VS on the system. After all these packages are installed you can simply call `zig build` to compile the Flint compiler.

Compiling LLVM itself for the first time could take up to an hour, it takes quite a lot of time, be aware of that. After the initial build the `flintc` directory takes up roughly 15GB of storage space, so be aware that LLVM takes a lot of space.

> [!NOTE]
> I was not able to get `neovim` to work properly on Windows, it did not like the installed standard library from the above mentioned minimal llvm-mingw package. `Zed`, however, worked fine so it now is the recommended IDE to use on Windows. I still need to figure out why nvim doesn't work on Windows, i would much rather use it instead of Zed but hey, we can't get everything.

### After Compilation

After compiling the compiler you need `base-devel` (Arch) or `build-essential` (Ubuntu) pavkages installed in order for the Flint compiler to be able to compile any program. This is because it needs the `crt1.o`, `crti.o` and `crtn.o` files available in your lib path.
