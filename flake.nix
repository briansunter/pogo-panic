{
  description = "Pocket Pogo Panic — Game Boy ROM + browser runner dev environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    let
      gbdkVersion = "4.5.0";
      gbdkAssets = {
        "x86_64-linux" = {
          asset = "gbdk-linux64.tar.gz";
          sha256 = "d7857a5f6d135ee4c249043ca26aad9f2ec8ab5d4106d97720d404114f42605c";
        };
        "aarch64-linux" = {
          asset = "gbdk-linux-arm64.tar.gz";
          sha256 = "31eb2235f0fdb60163d0b1e9574a022098d6069cd56606a1daca4478a46e0439";
        };
        "x86_64-darwin" = {
          asset = "gbdk-macos.tar.gz";
          sha256 = "1aa549d12032d8f6509d11923bb28b1a453098f42597feb378e9a42541f8fd89";
        };
        "aarch64-darwin" = {
          asset = "gbdk-macos-arm64.tar.gz";
          sha256 = "289ee60e46c5a2785a21e35533f84a5131ed4a063b21b0dbdedc9a10af15bf78";
        };
      };
      supportedSystems = builtins.attrNames gbdkAssets;
    in
    flake-utils.lib.eachSystem supportedSystems (system:
      let
        pkgs = import nixpkgs { inherit system; };
        lib = pkgs.lib;
        assetInfo = gbdkAssets.${system};

        gbdk-2020 = pkgs.stdenv.mkDerivation {
          pname = "gbdk-2020";
          version = gbdkVersion;

          src = pkgs.fetchurl {
            url = "https://github.com/gbdk-2020/gbdk-2020/releases/download/${gbdkVersion}/${assetInfo.asset}";
            sha256 = assetInfo.sha256;
          };

          # Upstream tarball contains a top-level "gbdk/" directory.
          sourceRoot = "gbdk";

          # Prebuilt binaries — don't try to compile, strip, or rewrite them.
          dontBuild = true;
          dontConfigure = true;
          dontStrip = true;

          nativeBuildInputs = lib.optionals pkgs.stdenv.isLinux [
            pkgs.autoPatchelfHook
          ];

          buildInputs = lib.optionals pkgs.stdenv.isLinux [
            pkgs.stdenv.cc.cc.lib
            pkgs.zlib
          ];

          installPhase = ''
            runHook preInstall
            mkdir -p "$out"
            cp -R . "$out/"
            chmod -R u+w "$out"
            runHook postInstall
          '';

          # Smoke test — fails the build if lcc can't run on this host.
          doInstallCheck = true;
          installCheckPhase = ''
            "$out/bin/lcc" -v >/dev/null
          '';

          meta = with lib; {
            description = "GBDK-2020: Game Boy Development Kit (prebuilt toolchain)";
            homepage = "https://github.com/gbdk-2020/gbdk-2020";
            license = licenses.mit;
            platforms = supportedSystems;
            sourceProvenance = with sourceTypes; [ binaryNativeCode ];
          };
        };
      in {
        packages = {
          inherit gbdk-2020;
          default = gbdk-2020;
        };

        devShells.default = pkgs.mkShell {
          packages = [
            pkgs.bun
            pkgs.nodejs_22
            pkgs.gnumake
            pkgs.gcc       # provides cc + gcov for `make test-c` / `make levels` / coverage
            # NB: gbdk-2020 is intentionally NOT in `packages`. mkShell would
            # add its include/ to NIX_CFLAGS_COMPILE, causing the host cc to
            # pick up GBDK's <string.h> / <stdio.h> instead of glibc's. We
            # expose GBDK only via PATH + GBDK_HOME below; lcc finds its own
            # headers/libs through GBDK_HOME.
          ];

          shellHook = ''
            export GBDK_HOME="${gbdk-2020}"
            export PATH="${gbdk-2020}/bin:$PATH"
            echo "Pocket Pogo Panic dev shell"
            echo "  GBDK_HOME=$GBDK_HOME"
            echo "  bun $(bun --version), node $(node --version), make $(make --version | head -n1), $(cc --version | head -n1)"
            echo ""
            echo "Build the ROM:        make"
            echo "Run host C tests:     make test-c"
            echo "Regenerate levels:    make levels"
            echo "Install JS deps:      bun install"
            echo "Run all tests:        bun run test"
            echo "Browser dev server:   bun run dev"
          '';
        };
      });
}
