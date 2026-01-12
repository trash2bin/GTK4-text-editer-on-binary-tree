{
  description = "Dev shell for gtkmm4 C++ project (with LTO-friendly tools)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { nixpkgs, ... }:
  let
    system = "aarch64-linux"; # на linux x86-64 или aarch64-darwin тоже все работает
    pkgs = import nixpkgs { inherit system; };
  in
  {
    devShells.${system}.default = pkgs.mkShell {
      # Базовые инструменты для сборки
      nativeBuildInputs = with pkgs; [
        cmake
        gnumake      # make
        pkg-config
        fontconfig
      ];

      # Компилятор/линкер/архиватор и отладка
      buildInputs = with pkgs; [
        gcc
        binutils
        gdb
        clang-tools
        gtkmm4
        libsigcxx

        noto-fonts
        noto-fonts-color-emoji
        liberation_ttf
      ];
    };
  };
}
