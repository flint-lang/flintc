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

Installing Flint is really easy. Just download the `flintc` binary for your given platform from the [Releases](https://github.com/flint-lang/flintc/releases) page.

### Linux

To make the Flint compiler available from any path in your terminal, and to make it executable through just calling `flintc` in your terminal, you need to copy the `flintc` executable into the `$HOME/.local/bin/` directory (if it does not exist yet, i would highly recommend to create it) and you need to ensure it is marked as executable with this command:

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

You need `base-devel` (Arch) or `build-essential` (Ubuntu) in order for the Flint compiler to be able to compile any program. It needs the `crt1.o`, `crti.o` and `crtn.o` files available to it.

### Windows

Installation on Windows is pretty easy, it's just a one-line command:

```ps1
powershell -NoProfile -ExecutionPolicy Bypass -Command "iex (irm 'https://github.com/flint-lang/flint/releases/download/installer/flint_installer.ps1')"
```

Or if this one-liner scares you you can still [download](https://github.com/flint-lang/flint/releases/download/installer/flint_installer.ps1) the installer directly and execute the downloaded script using the command

```ps1
PowerShell -NoProfile -ExecutionPolicy Bypass -File .\flint_installer.ps1
```

The installer will always download the latest Flint release directly and add a wrapper command for it to always point at the correct latest compiler version. You can then use the compiler using

```ps1
flintc.cmd --help
```

directly in any PowerShell. Note that the `flintc.cmd` command is only available in PowerShell, not in the command prompt.

## Building

Building is easy. You obviously first need to clone this repository. There exists a single `build.sh` script in the `scripts` directory, with many cli options to choose from. Choose what you whish to do and the script will do it for you. You must be in the root directory of the `flintc` repository for the build script to work properly.

You *need* to compile the linux build at least once, the build script relies on the built `llvm-config` executable built from the LLVM source. So, execute the build script like so:

```sh
./scripts/build.sh -l
```

before compiling for windows using `./scripts/build.sh -w`, otherwise the Windows compilation will fail. After the initial compilation for Linux you can mix and match all possible commands of the build.sh script, it will work just fine.

After compiling the compiler you need `base-devel` (Arch) or `build-essential` (Ubuntu) in order for the Flint compiler to be able to compile any program. It needs the `crt1.o`, `crti.o` and `crtn.o` files available to it.
