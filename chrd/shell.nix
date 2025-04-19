{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  buildInputs = [
    pkgs.python313
    # Include pynput and its dependencies directly from nixpkgs
    pkgs.python313Packages.virtualenv
  ];

  shellHook = ''
    if [ ! -d .venv ]; then
      echo "Creating virtualenv and installing pynput..."
      python3 -m venv .venv
      source .venv/bin/activate
      pip install --upgrade pip
      pip install pynput
    else
      source .venv/bin/activate
    fi
  '';
}
