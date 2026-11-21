{ lib,
  stdenvNoCC,
  lzip,
  python3,
  makeWrapper }:
let
  wrappedPath = lib.makeBinPath [ lzip ];
in stdenvNoCC.mkDerivation {
  name = "linstack_script";

  buildInputs = [
    (python3.withPackages(ps: with ps; [ tqdm requests inquirerpy ]))
  ];

  nativeBuildInputs = [
    makeWrapper
  ];

  src = ./.;

  postPatch = ''
    patchShebangs main.py
  '';

  installPhase = ''
    mkdir -p $out/libexec
    cp -r . $out/libexec/linstack_script
    mkdir -p $out/bin
    makeShellWrapper $out/libexec/linstack_script/main.py $out/bin/linstack_script \
      --prefix PATH : "${wrappedPath}"
  '';
}
