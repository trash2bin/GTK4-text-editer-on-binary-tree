{
  description = "Dev shell for gtkmm4 C++ project (with LTO-friendly tools)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { nixpkgs, ... }:
  let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
  in
  {
    devShells.${system}.default = pkgs.mkShell {
      # Базовые инструменты для сборки
      nativeBuildInputs = with pkgs; [
        cmake
        ninja
        gnumake      # make
        pkg-config
      ];

      # Компилятор/линкер/архиватор и отладка
      buildInputs = with pkgs; [
        gcc                  # GNU toolchain (gcc, gcc-ar, gcc-ranlib)
        binutils             # ar/ranlib/ld/strip (на случай fallback)
        gdb
        clang-tools          # опционально: clang-tidy/clang-format
        gtkmm4
        libsigcxx
      ];
    };
  };
}
