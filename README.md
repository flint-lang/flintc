# flintc

This project contains the custom **Flint Compiler** built in C++. The Compiler is not (yet) ready to be made public. Currently, the Tokenizer (Lexer) and AST Generation (Parser) are working.

## Todos

- [ ] IR Generation (Generator)
- [ ] Semantics Checking Stage (SemanticsChecker)
- [ ] Linking of IR Files (Linker)
- [ ] Dependency Management via FlintHub (Fetcher)

## Building

The build process is easy...just clone the `flintc` repository and execute `nix-shell` in the base directory (if you have nix installed, this will result in the correct packages and dependencies being fetched and make will be executed automatically) or just execute `make` directly if you have the llvm dependencies installed on your system, thats it!

To get correct highlighting and code completion of llvm-stuff, look at [this](llvm/Readme.md) Readme!
