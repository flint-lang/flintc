{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = [ pkgs.docker ];

  shellHook = ''
    if ! docker image inspect flint-ubuntu-packaging > /dev/null 2>&1; then
      echo "Building Docker image 'flint-ubuntu-packaging'..."
      docker build -t flint-ubuntu-packaging .
    fi

    docker run -it --rm \
      -v "$(pwd)":/work \
      -e HOME=/home/ubuntu \
      -e GPG_TTY=$(tty) \
      flint-ubuntu-packaging
  '';
}
