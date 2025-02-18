# flintc

This project contains the custom **Flint Compiler** built in C++. The Compiler is not (yet) ready to be made public. Currently, the Tokenizer (Lexer) and AST Generation (Parser) are working.

## Todos

- [x] Tokenizer (Lexer)
- [x] AST Generation (Parser)
- [x] IR Generation (Generator)
- [ ] Comprehensive Compile Error output (Error)
- [ ] Semantics Checking Stage (Checker / Linter)
- [x] Linking of IR Files (Linker)
- [ ] Fetching libraries from FlintHub (Fetcher)
- [x] Creation and Management of dependencies (Resolver)
- [x] Compilation of IR code to a binary (Compiler)

## What's working

Currently, only the most basic features are working:

- Declaring functions (main and custom ones)
- Calling functions (n arguments, currently only 1 return value)
- Catching errors of functions (`catch` keyword)
- `if` chains (`if`, `else if`, `else`)
- `while` loops
- The builtin `print` function, with overloaded variants to print `int`, `flint`, `str` and `bool` variables or literals
- `int` variables, addition, substraction and multiplication
- `str` variables, alltough currently not able to be changed
- `bool` variables

## What's not working

- Calling functions as expressions (as the condition of the while loop, or in binary operations), but when the function is the only thing on the RHS of an assignment or declaration, it works fine!
- `flint` variables dont behave as expected and are always wrong
- `for` loops
- enhanced `for` loops (because arrays dont exist yet, because DIMA doesnt exist yet)
- Arrays `[]`
- The optional type
- Custom `error` sets
- `enum` type
- `variant` type
- Unary operators such as `++` or `--`
- The pow operator `**`
- Everything about DIMA (`str` manipulation, `data` saving, memory management overall)
- Everything about DOCP (`data`, `func`, `entity`, `link`)
- etc.

## Building

You need `nkx` installed on your machine ([https://nix.dev/install-nix.html](https://nix.dev/install-nix.html)).
The build process then is easy. Just clone the `flintc` repository and execute
```sh
nix-shell
```
in the base directory (if you have nix installed). This will result in the correct packages and dependencies being fetched.
When inside the nix-shell, execute
```sh
./scripts/build.sh
```
to build the project. You can optionally add the `debug` option when calling the build script, it then will create a debug build.
You could also, alternatively, ensure manually that all dependencies are installed and just call the build script without `nix`.

To get correct highlighting and code completion of llvm-stuff, look at [this](llvm/Readme.md) Readme!
