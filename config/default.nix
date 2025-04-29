{
  pkgs ? import <nixpkgs> { },
  firmware ? import ../src { },
}:

let
  config = ./.;

  left = firmware.zmk.override {
    board = "glove80_lh";
    keymap = "${config}/glove80.keymap";
    kconfig = "${config}/glove80.conf";
    snippets = [
      ../custom_behavior
    ];
  };
  right = firmware.zmk.override {
    board = "glove80_rh";
    keymap = "${config}/glove80.keymap";
    kconfig = "${config}/glove80.conf";
    snippets = [
      ../custom_behavior
    ];
  };

in
{
  glove80_left = left;
  glove80_right = right;
  glove80_combined = firmware.combine_uf2 left right;
}
