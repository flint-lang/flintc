# Ubuntu / Debian packaging for flintc

To build and release the package for ubuntu run the following commands:

```sh
nix-shell
```

to enter the shell and the docker container.

```sh
./prepare-source.sh $VERSION
```

where `$VERSION` needs to be replaced with the Flint version like `0.4.1`. A new directory `flintc-0.4.1` will be created. You then need to cwd into that directory to build the package

```sh
cd flintc-$VERSION
../build.sh
```

After building, several new files were created. OUTSIDE the nix-shell and docker container (maybe in a different terminal) execute

```sh
./sign.sh $VERSION
```

this will sign the files, update cheksums etc. Then, INSIDE the container you lastly need to execute

```sh
./publish.sh $VERSION
```

to publish the new version to the PPA.
