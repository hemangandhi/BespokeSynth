# This is not yet actually a flake, so you want to run nix-shell on this to
# actually get the package in a shell. This is because I don't want to add and
# maintain against an experimental feature on my daily driver.

with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "BespokeSynth";
  # Based on the bespoke_dependencies.sh file in the ZIP for linux (sorted alphabetically)
  buildInputs =  [
    alsa-lib
    cmake
    curl
    freeglut
    freetype
    gcc
    gtk3
    libjack2
    libusb1
    mesa
    pkg-config
    python3Full
    webkitgtk
    xorg.libX11
    xorg.libXcomposite
    xorg.libXcursor
    xorg.libXinerama
    xorg.libXrandr
  ];
  shellHook = ''
  cmake -B ignore/build
  cmake --build ignore/build --parallel 4
  '';
}
