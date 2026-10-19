{
  description = "Uplink: the Logos node referral programme (QML view + C++ backend)";

  nixConfig = {
    extra-substituters = [ "https://cache.nix.logos.co/public" ];
    extra-trusted-public-keys = [ "public:l4HrXgL4nw246+LBh2SOJyhz64BoGegOYLheT/iIAPU=" ];
  };

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder";
    # The referral programme (logos-blockchain/uplink-module).
    uplink_module = {
      url = "github:logos-blockchain/uplink-module";
      inputs.logos-module-builder.follows = "logos-module-builder";
      inputs.lez_core.follows = "lez_core";
    };
    # send_generic_private_transaction_with_accounts (logos-execution-zone-module#69); use main once merged.
    lez_core.url = "github:logos-blockchain/logos-execution-zone-module?rev=dde621ab4dbd873d8a2b314932a9f2180d03813a";
    blockchain_module.url = "github:logos-blockchain/logos-blockchain-module";
  };

  outputs = inputs@{ logos-module-builder, ... }:
    let
      shared = {
        src = ./.;
        configFile = ./metadata.json;
        flakeInputs = inputs;
      };

      module = logos-module-builder.lib.mkLogosQmlModule shared;

      # mkLogosQmlModule only runs tests/*.mjs; the mock's rules are C++ unit tests.
      unitTests = logos-module-builder.lib.mkLogosModuleTests
        (shared // { testDir = ./tests; });
    in
    module // {
      checks = builtins.mapAttrs
        (system: tests: ((module.checks or {}).${system} or {}) // tests)
        unitTests;
    };
}
