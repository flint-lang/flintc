{ pkgs ? import <nixpkgs> {} }:

let
  libxml2-static = pkgs.libxml2.overrideAttrs (oldAttrs: {
    configureFlags = (oldAttrs.configureFlags or []) ++ [
      "--enable-static"
    ];
    dontDisableStatic = true;
  });
in
{
  inherit (libxml2-static) out;
}
