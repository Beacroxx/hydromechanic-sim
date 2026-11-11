{
  description = "CPP Dev Shell Flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        buildType = "Debug";
        projectName = "hd-sim";
        version = "0.1.0";
        pkgs = import nixpkgs {
          inherit system;
        };
        lib = pkgs.lib;
        nativeBuildInputs = with pkgs; [
          nix-direnv
          clang
          clang-tools
          gdb
          cmake
          ninja
          shader-slang
          pkg-config
        ];
        buildInputs = with pkgs; [
          fmt
          gtest
          glfw
          glm
          rapidyaml
          stb
          vulkan-headers
          vulkan-loader
          vulkan-validation-layers
          gbenchmark
          cli11
        ];
        stdenv = (pkgs.llvmPackages_21 or pkgs.llvmPackages).libcxxStdenv;
      in
      {
        devShells.default = import ./shell.nix {
          inherit
            buildInputs
            nativeBuildInputs
            pkgs
            stdenv
            ;
        };
        packages =
          let
            fs = lib.fileset;
            sourceFiles = fs.unions [
              ./CMakeLists.txt
              (fs.fileFilter (file: file.hasExt "cpp") ./src)
              (fs.fileFilter (file: file.hasExt "hpp") ./include)
              (fs.fileFilter (file: file.hasExt "cmake") ./cmake)
              (fs.fileFilter (file: file.hasExt "slang") ./shaders)
              (fs.fileFilter (
                file:
                file.hasExt "png"
                || file.hasExt "jpg"
                || file.hasExt "jpeg"
                || file.hasExt "bmp"
                || file.hasExt "tga"
                || file.hasExt "gif"
                || file.hasExt "psd"
                || file.hasExt "hdr"
                || file.hasExt "pic"
                || file.hasExt "pnm"
              ) ./textures)
            ];
          in
          rec {
            app = stdenv.mkDerivation {
              name = projectName;
              version = version;
              outputs = [ "out" ];
              src = fs.toSource {
                root = ./.;
                fileset = sourceFiles;
              };
              buildInputs = buildInputs;
              nativeBuildInputs = nativeBuildInputs ++ [ pkgs.makeWrapper ];
              enableDebugging = true;
              dontStrip = true;
              donotUnpack = true;
              configurePhase = ''
                cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=${buildType} -DCMAKE_CXX_COMPILER=clang++
              '';
              buildPhase = ''
                cmake --build cmake-build --verbose -j "$NIX_BUILD_CORES"
              '';
              installPhase = ''
                cmake --install cmake-build --prefix $out
              '';
              postFixup = ''
                wrapProgram $out/bin/${projectName} \
                  --prefix VK_LAYER_PATH : "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
              '';
              meta = {
                description = "Core C++ library";
                license = lib.licenses.mit;
              };
            };
            default = app;
          };
        apps = rec {
          app = {
            type = "app";
            program = "${self.packages.${system}.app}/bin/${projectName}";
          };
          default = app;
        };
      }

    );
}
