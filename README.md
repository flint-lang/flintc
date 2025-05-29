# flintc

This project contains the custom **Flint Compiler** built in C++. Flint is a statically typed, compiled and fully deterministic language which sets code clarity, ease of use and performance as its very top priorities.

The Compiler is not done yet because a lot of planned features are still missing. The whole compiler pipeline is fully working and the core of the Flint compiler is getting closer to its finished state, but we are not there yet. You can look at the Wiki page to get a grasp of Flint's syntax but some parts of the wiki are already deprecated, as the design of Flint became much clearer during its developement.

## Todos

- [x] Tokenizer (Lexer)
- [x] AST Generation (Parser)
- [x] IR Generation (Generator)
- [ ] Comprehensive Compile Error output (Error)
- [ ] Semantics Checking Stage (Checker / Linter)
- [x] Linking of Files (Linker)
- [x] Signature Pattern Matcher (Matcher)
- [ ] Fetching libraries from FlintHub (Fetcher)
- [x] Creation and Management of dependencies (Resolver)
- [x] Compilation of IR code to a binary (Compiler)

## What's working

Nearly all Core features are already working:

- Declaring functions (main and custom ones)
- Calling functions (n arguments, m return values)
- Calling functions as expressions (as the condition of the while loop, or in binary operations)
- Groups work in general (inferred declaration of multiple variables `(x, y) := (1, 2)` etc.)
- Variable swaps through groups (`(x, y) = (y, x)`)
- Catching errors of functions (`catch` keyword) and accessing the error value with `catch err:`
- `if` chains (`if`, `else if`, `else`)
- `while` loops
- `for` loops
- Creating custom `data` types, also nested data works properly
- Initialization of custom data types, grouped variable access (`v2.(x, y)`) of custom data types
- Variables of custom `data` types are _always_ passed by reference to functions
- With the `mut` keyword, function parameters now can become mutable, both primitive and `data` parameters
- The builtin `print` function, with overloaded variants to print all primitive types
- Unary operators such as `++`, `--` or `not`
- safe `i32`, `i64`, `u32`, `u64`, `f32` and `f64` variables, addition, substraction and multiplication
- implicit primitive type conversions
- explicit primitive type conversions (`i32(5.4)`, `str(69)`)
- `str` variables + string mutability + concatenation + interpolation
- `bool` variables
- rectangular n-dimensional arrays (`i32[]`, `i32[,]` ...) and accessing the length(s) via `.length` which returns n `u64` values, one for each dimension length
- SIMD multi-types such as `i32x2`, `f32x3`, `i64x4`, `bool8`, etc. and native interoperability with groups
- `enum` type
- declaring and using tuple types `data<i32, f32, str> t = (1, 3.3, "yes");`, `f32 x = t.$1` etc.
- enhanced for loops: `for (index, element) in iterable:` with direct index and element unpacking (and not using them through `_`) or `for pair in iterable:` where the pair is a tuple of the index and element
- accessing strings as if they were arrays with `string[idx]` which returns an `u8`

## Available Core modules
- `print` (`use Core.print`) for the print function with its 9 overloads (`i32`, `i64`, `u32`, `u64`, `f32`, `f64`, `str`, `char` and `bool`
- `read` (`use Core.read`) for the 7 `read_str`, `read_i32`, `read_i64`, `read_u32`, `read_u64`, `read_f32` and `read_f64` functions
- `assert` (`use Core.assert`) for the `assert` function
- `filesystem` (`use Core.filesystem`) for the `read_file`, `read_lines`, `file_exists`, `write_file`, `append_file` and `is_filr` functions
- `env` (`use Core.env`) for the `get_env` and `set_env` functions

## What's not working

- `flint` variables dont behave as expected and are always wrong
- The optional type
- Custom `error` sets
- `variant` type
- The pow operator `**`
- Everything about DIMA (`data` and `entity` types saved in dima blocks)
- Everything about DOCP (`func`, `entity`, `link`)
- etc.

## Building

Building is easy. There exists a single `build.sh` script in the `scripts` directory, with many cli options to choose from. Choose what you whish to do and the script will do it for you. You must be in the root directory of the `flintc` repository for the build script to work properly (cause CMake). After compiling the compiler you need `base-devel` (Arch) or `build-essential` (Ubuntu) in order for the Flint compiler to be able to compile any program. It needs the `crt1.o`, `crti.o` and `crtn.o` files available to it.
