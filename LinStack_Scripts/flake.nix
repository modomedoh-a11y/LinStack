{
  description = "Linstack Extras Script";
  inputs = {
    systems.url = "github:nix-systems/default";
  };
  outputs = { self, nixpkgs, systems }:
  let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);
    mkApp = program: { type = "app"; inherit program; };
  in {
    packages = eachSystem (system: rec {
      linstack_script = nixpkgs.legacyPackages."${system}".callPackage ./package.nix { };
      default = linstack_script;
    });
    apps = eachSystem (system: rec {
      linstack_script = mkApp "${self.outputs.packages.${system}.linstack_script}/bin/linstack_script";
      default = linstack_script;
    });
  };
}
