# XML2

To build xml2 you need `nix` installed on your system. If you have nix installed, open the tetrminal in the `xml2/` directory (this directory) and execute the command

```sh
nix-build xml2.nix -o xml2
```

This will build the static versions of the xml2 library and create symlinks to the built libraries to the xml2 path. You will only need to build those static libraries if you want to build the `flintc` compiler as a static binary.
