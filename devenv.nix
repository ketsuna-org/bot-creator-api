{ pkgs, lib, config, inputs, ... }:

{
    dotenv.enable = true;
  env = {
    CXXFLAGS = "-std=c++20";
    PKG_CONFIG_PATH = "${pkgs.openssl.dev}/lib/pkgconfig"; # Critical pour trouver OpenSSL
  };

  packages = with pkgs; [
    git
    gcc
    cmake
    clang-tools
    openssl.dev  # Inclure les headers de développement
    zlib.dev
    libopus
    libsodium
    pkg-config
    ninja
    gh
    (stdenv.mkDerivation {
        name = "dpp";
        version = "latest";
        src = fetchFromGitHub {
          owner = "brainboxdotcc";
          repo = "DPP";
          rev = "c6bcab5b4632fe35e32e63e3bc813e9e2cd2893e";
          sha256 = "sha256-IMESnvvjATgsKCDfrRY8bPxUYpiULIPFf6SToO7GbVM=";
        };
        nativeBuildInputs = [ cmake pkg-config ];
        buildInputs = [ openssl zlib libopus ];
        cmakeFlags = [
          "-DCMAKE_CXX_STANDARD=20"
          "-DBUILD_TEST=OFF"
          "-DBUILD_EXAMPLES=OFF"
        ];
      })  # Nécessaire pour détecter OpenSSL
  ];

  languages.go.enable = true;

  scripts.build.exec = ''
    mkdir -p bot/build
    cd bot/build
    cmake ..
    make -j$NIX_BUILD_CORES
  '';

  scripts.start.exec = ''
    build
    mv bot/build/discord-bot ./app
    cd app
    go run cmd/main.go
  '';

  # Supprimer la configuration brew inutile dans Nix
}
