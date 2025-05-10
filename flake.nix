{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs = {flake-parts, ...} @ inputs:
    flake-parts.lib.mkFlake {inherit inputs;} {
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      perSystem = {system, ...}: let
        pkgs = import inputs.nixpkgs {
          inherit system;
        };
      in {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            ninja
            (python3.withPackages (ps: [
              # From https://github.com/zmkfirmware/zephyr/blob/HEAD/scripts/requirements-base.txt
              ps.west
              ps.pyelftools
              ps.pyyaml
              ps.pykwalify
              ps.canopen
              ps.packaging
              ps.progress
              ps.psutil
              ps.pylink-square
              ps.pyserial
              ps.requests
              ps.anytree
              ps.intelhex
              # For ZMK Studio builds
              ps.protobuf
              ps.grpcio-tools
            ]))
            gcc-arm-embedded
            protobuf
          ];

          env = {
            ZEPHYR_TOOLCHAIN_VARIANT = "gnuarmemb";
            GNUARMEMB_TOOLCHAIN_PATH = pkgs.gcc-arm-embedded;
          };
          shellHook = ''
            export ZEPHYR_BASE=$PWD/zephyr
          '';
        };
      };
    };
}
