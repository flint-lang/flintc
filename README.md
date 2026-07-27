<div align="center">
<p>
    <img width="100" src="https://raw.githubusercontent.com/flint-lang/logo/main/logo.svg">
    <h1>The Flint Programming Language</h1>
</p>

<p>
A high level language with transparency at its core.

This repository contains the Flint compiler itself.

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
> Flint is not finished yet and many features are still missing, but we work hard at finishing it.
> Please report any issues you may encounter with the [Flint Issue Tracker](https://github.com/flint-lang/flintc/issues).

This project contains the custom **Flint Compiler** built in C++. Flint is a statically typed, compiled and automatically memory managed ([DIMA](https://github.com/flint-lang/dima)) language and aims at being as high level as possible while never sacrificing transparency. Flint is a composition-first language with clean separation of data and behaviour. It does require a mindset shift compared to an inheritance-heavy OOP mentality but you will end up writing more reusable and decoupled code as a result.

If you are interested in it, start learning it through the [Wiki](https://flint-lang.github.io/wiki) today!

## Who Flint is for

Flint is for developers who appreciate high-level ergonomics and syntax but want to understand what's actually happening under the hood. If you enjoy writing expressive code while maintaining transparency into runtime behaviour, Flint is for you.

Flint may not be the best fit if you:

- want maximum low-level control with manual memory management, Flint won't provide that level of control
- prioritize maximum ergonomics over understanding runtime details, as Flint will feel too explicit

But if you want a good mix, Flint is exactly what you're looking for.

## Examples

These examples are meant to give a broad overview of Flint and its syntax without going in-detail about it. If you want to know more about any topic it is recommended to go to the Wiki and read through the Guide, as it will teach you everything about Flint there is to learn.

### Hello World

```rs
use Core.print

def main():
    print("Hello, World!\n");
```

### Composition as a core feature

Flint is based on the the central idea of composing reusable data and behaviour in objects. While slightly more verbose upfront, this desugn enables strong static guarantees, better memory layout and highly reusable components.

You can see the slight verbosity as an upfront cost, as the fact that every object is built up from reusable parts means that codebases become very scalable and easy to extend.

<details>
  <summary>Show Code</summary>

```rs
use Core.print

data Wings:
	u32 size;
	u32 flight_time;
	Wings(size, flight_time);

data Legs:
	u32 count;
	f32 speed;
	f32 height;
	Legs(count, speed, height);

func Fly requires(Wings w):
	def fly():
		print($"Starting to fly for {w.flight_time} seconds with my {w.size}cm large wings\n");

func Run requires(Legs l):
	def run():
		print($"Starting to run with my {l.count} legs with a speed of {l.speed} km/h\n");

func Jump requires(Legs l):
	def jump():
		print($"Jumping {l.height}m high with my {l.count} legs\n");

object Dog:
	data: Legs;
	func: Run, Jump;
	Dog(Legs);

object Bird:
	data: Wings, Legs;
	func: Fly, Run, Jump;
	Bird(Wings, Legs);

def main():
	d := Dog(Legs(4, 30.0, 0.8));
	d.run();
	d.jump();

	print("\n");
	b := Bird(Wings(10, 100), Legs(2, 1.5, 0.1));
	b.run();
	b.jump();
	b.fly();
```

</details>

<details>
  <summary>Show Output</summary>

```
Starting to run with my 4 legs with a speed of 30 km/h
Jumping 0.8m high with my 4 legs

Starting to run with my 2 legs with a speed of 1.5 km/h
Jumping 0.1m high with my 2 legs
Starting to fly for 100 seconds with my 10cm large wings
```

</details>

### Functions as values

Flint's execution model enables advanced features like callables, higher order functions, variable persistence and much more. As it's transparent about what it does under the hood, you will be able to easily tell exactly what that Flint program below is doing and how it works.

<details>
  <summary>Show Code</summary>

```rs
use Core.print

def div(i32 x, i32 y) -> i32:
	return x / y;

def sub(i32 x, i32 y) -> i32:
	return x - y;

def bind_sub(mut fn<i32, i32 -> i32> c):
	c = ::sub;

def execute(fn<i32, i32 -> i32> c, i32 x, i32 y) -> i32:
	return c(x, y);

def main():
	fn<i32, i32 -> i32> c = ::div;
	i32 result = execute(c, 10, 2);
	print($"result = {result}\n");

	bind_sub(c);
	result = execute(c, 10, 2);
	print($"result = {result}\n");
```

</details>

<details>
  <summary>Show Output</summary>

```
result = 5
result = 8
```

</details>

### Simple Raylib Example

Flint comes with a unique protocol-based approach to interoperability with other languages. The **Flint Interop Protocol (FIP)** lets you use C libraries such as raylib without writing a single line of bindings code yourself.

<details>
    <summary>.fip/config/fip.toml</summary>

```toml
[fip-c]
enable = true
```

</details>

<details>
    <summary>.fip/config/fip-c.toml</summary>

```toml
[raylib]
headers = ["/usr/include/raylib.h"]
```

</details>

<details>
  <summary>main.ft</summary>

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

</details>

<details>
  <summary>Show Output</summary>
<img width="800" height="451" alt="2026-02-28-202945_hyprshot" src="https://github.com/user-attachments/assets/44eff36d-dc44-46c0-890e-bf367a732a17" />
</details>

Build and run example using `flintc main.ft --flags="-lraylib"`

You need `raylib` installed on your system for this example to compile and run correctly.

## Installation

For how to install Flint, follow the instructions of the Wiki:
https://flint-lang.github.io/wiki/v0.4.0-core/user_guide/setup/1_installation.html

## Building

For how to build Flint from source, follow the instructions of the Wiki:
https://flint-lang.github.io/wiki/v0.4.0-core/developer_guide/flintc/setup.html

## What's next?

Flint is currently in late beta. The language has mostly stabilised and breaking changes are rare. We are searching for people to try it out and give feedback on the language itself and the learning experience through the Wiki.

Join the [Discord server](https://discord.gg/efqCDaVmb) and let's chat about Flint.

## Usage of AI

TLDR: Used for debugging only, never in code generation since AIs generate suboptimal code anyways.

<details>
  <summary>Long Version</summary>

Flint, is a hand-crafted project. I hate AI auto-completion as it just gets in my way all the time and generating code with AI is also never a good idea. However, there still are some parts where AI was or still is involved in the developement process of Flint, as for some jobs they fit quite nice as a tool:

- I still often use AI for debugging the generated LLVM IR code of Flint programs. The generator produces a couple thousand lines of IR code and finding the root cause of a bug is way too time-consuming for doing it manually. LLMs are pretty good at finding bugs (even though they are horrible at finding solutions). Often times the bugs are pretty hard to find manually, like a double pointer not loaded or loaded one time too often etc, especially now that the IR is based on opaque pointer types. But once found, most bugs are quite simple to fix (manually).
- LLMs are occasionally used for indexing tasks. For example when I can remember that i have the capability to do X somewhere but having trouble to find where it is (or more often, to find out how I named the damn thing).
- In the very early stages of development I used AI to be able to learn and understand the mammoth the LLVM C++ API is as a learning augmentation next to Kaleidoscope. It was a great tool for learning the  LLVM IR code and its syntax and semantics. However, all actual code in the compiler was written by myself as those early AI-assisted parts have long been replaced and refactored with properly (and poorly) engineered solutions.

So, if you see any bad code, that's my code. The same is true for the Wiki too. This is my very first big C++ project, so some parts of it still contain my original code and will look atrocious to some (including myself) because I wasn't the best C++ dev in the early days of Flint, even though I tried hard. I leant a lot along the way of this project and I am glad to have put as much effort and love into Flint as I did.

</details>

## Star History

<a href="https://www.star-history.com/?repos=flint-lang%2Fflintc&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=flint-lang/flintc&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=flint-lang/flintc&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=flint-lang/flintc&type=date&legend=top-left" />
 </picture>
</a>
