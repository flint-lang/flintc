# flintc

This project contains the custom **Flint Compiler** built in C++. The Compiler is not (yet) ready to be made public. Currently, the Tokenizer (Lexer) and AST Generation (Parser) are working.

## Todos

- [x] Tokenizer (Lexer)
- [x] AST Generation (Parser)
- [x] IR Generation (Generator)
- [ ] Comprehensive Compile Error output (Error)
- [ ] Semantics Checking Stage (Checker / Linter)
- [ ] Linking of IR Files (Linker)
- [ ] Fetching libraries from FlintHub (Fetcher)
- [x] Creation and Management of dependencies (Resolver)
- [x] Compilation of IR code to a binary (Compiler)

## What's working

Currently, only the most basic features are working:

- Declaring functions (main and custom ones)
- Calling functions (n arguments, m return values)
- Groups work in general (inferred declaration of multiple variables `(x, y) := (1, 2)` etc.)
- Variable swaps through groups (`(x, y) = (y, x)`)
- Catching errors of functions (`catch` keyword) and accessing the error value with `catch err:`
- `if` chains (`if`, `else if`, `else`)
- `while` loops
- `for` loops
- The builtin `print` function, with overloaded variants to print `i32`, `flint`, `str` and `bool` variables or literals
- Unary operators such as `++`, `--` or `not`
- safe `i32`, `i64`, `u32`, `u64`, `f32` and `f64` variables, addition, substraction and multiplication
- implicit primitive type conversions
- explicit primitive type conversions (`i32(5.4)`)
- `str` variables, alltough currently not able to be changed
- `bool` variables

## What's not working

- Calling functions as expressions (as the condition of the while loop, or in binary operations), but when the function is the only thing on the RHS of an assignment or declaration, it works fine!
- `flint` variables dont behave as expected and are always wrong
- enhanced `for` loops (because arrays dont exist yet, because DIMA doesnt exist yet)
- Arrays `[]`
- The optional type
- Custom `error` sets
- `enum` type
- `variant` type
- The pow operator `**`
- Everything about DIMA (`str` manipulation, `data` saving, memory management overall)
- Everything about DOCP (`data`, `func`, `entity`, `link`)
- etc.

## Building

Building is easy. There exists a single `build.sh` script in the `scripts` directory, with many cli options to choose from. Choose what you whish to do and the script will do it for you. The script works independently of the cwd.
