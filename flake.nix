{
  description = "Dev shell for gtkmm4 C++ project (with LTO-friendly tools)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { nixpkgs, ... }:
  let
    system = "aarch64-darwin";
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
        fontconfig
      ];

      # Компилятор/линкер/архиватор и отладка
      buildInputs = with pkgs; [
        gcc                  # GNU toolchain (gcc, gcc-ar, gcc-ranlib)
        binutils             # ar/ranlib/ld/strip (на случай fallback)
        gdb
        clang-tools          # опционально: clang-tidy/clang-format
        gtkmm4
        libsigcxx
        noto-fonts
        noto-fonts-color-emoji
        liberation_ttf
      ];

    # Настройка Fontconfig для поиска системных шрифтов macOS
    shellHook = ''
      export PANGOCAIRO_BACKEND=fc
      export FONTCONFIG_PATH=${pkgs.fontconfig.out}/etc/fonts
      export FONTCONFIG_FILE=${pkgs.fontconfig.out}/etc/fonts/fonts.conf
      
      # Добавляем пути к системным шрифтам macOS
      export FONTCONFIG_PATH=$FONTCONFIG_PATH:/System/Library/Fonts:/Library/Fonts:~/.fonts
    '';

    };
  };
}
