# flintc

This project contains the custom **Flint Compiler** built in C++. Flint is a statically typed, conpiled and fully deterministic lanfuage which has code clarity, ease of use and performance at its very top.

The Compiler is not done yet, as many planned features are still missing. The whole compiler pipeline is fully working and the core of the Flint compiler is getting closer to its finished state, but we are not there yet. You can look at the Wiki page to get a grasp of Flint's syntax but some parts of the wiki are already deprecated, as the design of Flint became more clear.

## Todos

- [x] Tokenizer (Lexer)
- [x] AST Generation (Parser)
- [x] IR Generation (Generator)
- [ ] Comprehensive Compile Error output (Error)
- [ ] Semantics Checking Stage (Checker / Linter)
- [x] Linking of Files (Linker)
- [ ] Fetching libraries from FlintHub (Fetcher)
- [x] Creation and Management of dependencies (Resolver)
- [x] Compilation of IR code to a binary (Compiler)

## What's working

Currently, only the most basic features are working:

- Declaring functions (main and custom ones)
- Calling functions (n arguments, m return values)
- Calling functions as expressions (as the condition of the while loop, or in binary operations)
- Groups work in general (inferred declaration of multiple variables `(x, y) := (1, 2)` etc.)
- Variable swaps through groups (`(x, y) = (y, x)`)
- Catching errors of functions (`catch` keyword) and accessing the error value with `catch err:`
- `if` chains (`if`, `else if`, `else`)
- `while` loops
- `for` loops
- Creating custom `data` types
- Initialization of custom data types, grouped variable access (`v2.(x, y)`) of custom data types
- Variables of custom `data` types are _always_ passed by reference to functions
- With the `mut` keyword, function parameters now can become mutable, both primitive and `data` parameters
- The builtin `print` function, with overloaded variants to print `i32`, `flint`, `str` and `bool` variables or literals
- Unary operators such as `++`, `--` or `not`
- safe `i32`, `i64`, `u32`, `u64`, `f32` and `f64` variables, addition, substraction and multiplication
- implicit primitive type conversions
- explicit primitive type conversions (`i32(5.4)`, `str(69)`)
- `str` variables + string mutability + concatenation
- `bool` variables

## What's not working

- `flint` variables dont behave as expected and are always wrong
- enhanced `for` loops (because arrays dont exist yet, because DIMA doesnt exist yet)
- Arrays `[]`
- The optional type
- Custom `error` sets
- `enum` type
- `variant` type
- The pow operator `**`
- Everything about DIMA (`data` and `entity` types saved in dima blocks)
- Everything about DOCP (`func`, `entity`, `link`)
- etc.

## Building

Building is easy. There exists a single `build.sh` script in the `scripts` directory, with many cli options to choose from. Choose what you whish to do and the script will do it for you. The script works independently of the cwd.
