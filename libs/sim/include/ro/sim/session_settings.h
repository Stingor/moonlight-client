#pragma once

#include <cstdint>

namespace ro::sim {

// CE QUE LE SERVEUR DIT DE LA SESSION dans ses réglages (`ZC_MOONLIGHT_SETTINGS`),
// et seulement à un client qu'il a reconnu.
//
// ⚠ RIEN NE S'Y DEVINE NI NE S'Y GARDE : chaque champ vaut son défaut tant que
// le serveur n'a rien dit, et ce qu'il dit ensuite fait foi, y compris à la
// baisse. Ce qui en dépend le relit à chaque image plutôt que de retenir un
// verdict.
struct SessionSettings {
  // LE NIVEAU DE GROUPE DU COMPTE. Zéro : un joueur, ou un serveur muet.
  std::uint8_t group_level = 0;

  // LA VITESSE DE MARCHE, en millisecondes par case.
  //
  // ⚠ ZÉRO VEUT DIRE « IL N'A RIEN DIT », et ce n'est pas la même chose que la
  // vitesse normale : tant qu'elle vaut zéro, le curseur du volet du staff garde
  // son défaut au lieu de sauter sur un chiffre inventé.
  std::uint16_t walk_speed = 0;

  // `@killer` : le personnage attaque les joueurs hors des cartes PvP.
  bool killer = false;
  // `@killable` : les joueurs peuvent l'attaquer hors des cartes PvP.
  bool killable = false;
  // `@playertest` : les plafonds GM d'ASPD et de delay de skill sont ignorés,
  // pour jouer à la cadence d'un joueur.
  bool player_test = false;
  // `@nodelay` : les skills n'ont ni cooldown, ni after-cast delay, ni incantation.
  bool no_delay = false;
  // LE RELAI DU CANAL DISCORD est-il ouvert pour ce personnage ? Le serveur
  // retient ce réglage par personnage, et le croit vrai tant qu'il n'a rien dit.
  bool discord_chat = true;
  // ⚠ PAS UNE PAROLE DU SERVEUR, LA MARQUE DU CLIENT : la demande de rallumer
  // ce relai est déjà partie depuis l'entrée en jeu, et elle ne part qu'une
  // fois (voir `ro::net::ReviveDiscordRelay`).
  bool discord_chat_revived = false;

  // ── LES RÈGLES DU SERVEUR (`ZC_MOONLIGHT_SERVER_RULES`) ────────────────────
  //
  // Les défauts sont ceux d'un serveur rAthena pre-renewal d'usine, sauf le
  // délai après téléportation, propre au serveur Moonlight : ils valent tant que
  // le serveur n'a rien dit, et un serveur qui ne connaît pas ce paquet ne le
  // dira jamais.
  //
  // LE RAYON DE VUE, en cases autour du personnage : le serveur ne fait
  // connaître au client que les acteurs à cette distance. Au-delà, un compagnon
  // disparaît du monde connu, et une cible n'y figure pas.
  int view_cells = kDefaultViewCells;
  // LA PORTÉE DU JEU, en cases : ce que le serveur tolère hors de la vue — un
  // compagnon plus loin du maître est ramené, un PNJ ou une échoppe plus loin
  // refuse de s'ouvrir. Un client d'origine voit exactement à cette portée ;
  // moonclient voit plus loin. Zéro : le serveur n'a rien dit, et elle se
  // confond avec la vue.
  int game_area_cells = 0;
  // LE PLUS LONG CHEMIN qu'une marche peut demander, en pas : au-delà, le
  // serveur la refuse sans rien dire.
  int max_walk_path_steps = kDefaultMaxWalkPathSteps;
  // Les niveaux maximum d'un homoncule, et d'une forme mutée (« S »).
  int homunculus_max_level = kDefaultHomunculusMaxLevel;
  int homunculus_s_max_level = kDefaultHomunculusSMaxLevel;
  // Le délai pendant lequel le serveur refuse l'attaque d'un homoncule après sa
  // téléportation avec le maître, en millisecondes. ⚠ UNE OPTION DU SERVEUR
  // MOONLIGHT (`homunc_teleport_acd`), qu'un rAthena d'usine n'a pas : le défaut
  // est la valeur de sa configuration.
  std::uint32_t homunculus_teleport_hold_ms = kDefaultHomunculusTeleportHoldMs;
  // LA TAXE DES ÉCHOPPES DE VENTE, en dix-millièmes du total d'une demande
  // d'achat : l'acheteur paie le total, le vendeur en touche le reste, tronqué.
  // Elle ne s'applique qu'à un total d'au moins `vending_tax_min_zeny`. Une
  // taxe nulle ne retient rien ; l'échoppe d'achat n'est jamais taxée.
  int vending_tax_permyriad = kDefaultVendingTaxPermyriad;
  std::int64_t vending_tax_min_zeny = kDefaultVendingTaxMinZeny;

  static constexpr int kDefaultViewCells = 14;
  static constexpr int kDefaultMaxWalkPathSteps = 17;
  static constexpr int kDefaultHomunculusMaxLevel = 99;
  static constexpr int kDefaultHomunculusSMaxLevel = 275;
  static constexpr std::uint32_t kDefaultHomunculusTeleportHoldMs = 5000;
  static constexpr int kDefaultVendingTaxPermyriad = 500;
  static constexpr std::int64_t kDefaultVendingTaxMinZeny = 100'000'000;
  // La taxe entière : le vendeur ne touche rien.
  static constexpr int kMaxVendingTaxPermyriad = 10'000;

  // La portée du jeu, ou la vue tant que le serveur ne l'a pas dite.
  [[nodiscard]] int GameAreaCells() const noexcept {
    return game_area_cells > 0 ? game_area_cells : view_cells;
  }
};

}  // namespace ro::sim
