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

## Examples

### Hello World

```rs
use Core.print

def main():
    print("Hello, World!\n");
```

### Simple Raylib Example

**`.fip/config/fip.toml`**:
```toml
[fip-c]
enable = true
```

**`.fip/config/fip-c.toml`**:
```toml
[raylib]
headers = ["/usr/include/raylib.h"]
```

**`main.ft`**:
```rs
use Fip.raylib as rl

const data Globals:
	str window_name = "Raylib Example";
	str message = "Hello from Raylib!";
	u32 width = 1280;
	u32 height = 720;
	u32 font_size = 60;

const data Colors:
	u8x4 background = u8x4(245, 245, 245, 255);
	u8x4 text = u8x4(45, 45, 45, 255);

def main():
	rl.InitWindow(Globals.width, Globals.height, Globals.window_name);
	while not rl.WindowShouldClose():
		rl.BeginDrawing();
		rl.ClearBackground(Colors.background);
		rl.DrawText(Globals.message, 100, 200, Globals.font_size, Colors.text);
		rl.EndDrawing();
	rl.CloseWindow();
```

Build and run example using `flintc --file main.ft --flags="-lraylib"`

You need `raylib` installed on your system for this example to compile and run correctly.

## Installation

https://flint-lang.github.io/v0.3.2-core/user_guide/setup/1_installation.html

## Building

https://flint-lang.github.io/v0.3.2-core/developer_guide/flintc/setup.html
