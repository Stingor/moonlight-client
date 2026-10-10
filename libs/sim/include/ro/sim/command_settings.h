#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ro::sim {

// LES RÉGLAGES DE COMMANDE DU PERSONNAGE, tels que le serveur les tient : ce que
// `@showexp`, `@noks` ou `@autoloot` règlent, et que le joueur peut aussi régler
// d'un clic.
//
// 🔴 LE SERVEUR EN EST LA SOURCE. Il les garde dans les registres du personnage,
// les pousse à l'entrée en jeu, et les repousse chaque fois qu'une commande tapée
// dans le chat en change un. Ce miroir n'invente rien : tant que le serveur n'a
// pas parlé, il ne sait rien — voir `told`.
//
// ⚠ LES VALEURS SONT CELLES QUE LE JOUEUR LIT : un taux en pour-cent, un seuil en
// zeny entiers. Ce que le fil en fait — le taux en centièmes côté serveur, le
// seuil divisé par cent — est l'affaire de `net`.

// Un preset de la liste de ramassage automatique, rangé par le serveur.
struct AutolootPreset {
  // L'emplacement, de 1 à `kAutolootPresetSlots`. Zéro n'en désigne aucun.
  std::uint8_t slot = 0;
  // Tel que le joueur l'a tapé, en UTF-8. Vide pour un preset sans nom.
  std::string name;
  // Rechargé de lui-même à chaque connexion. Un seul preset l'est au plus.
  bool autoload = false;
};

// Le nombre d'emplacements de preset que le serveur tient par personnage.
inline constexpr std::uint8_t kAutolootPresetSlots = 10;

// La taille de la liste de ramassage automatique, côté serveur.
inline constexpr std::size_t kAutolootListCapacity = 50;

// Les quatre protections contre le vol de kill (`@noks`), dans l'ordre du fil.
enum class KillStealGuard : std::uint8_t { kOff = 0, kSelf = 1, kParty = 2, kGuild = 3 };

struct CommandSettings {
  // ⚠ FAUX TANT QUE LE SERVEUR N'A RIEN DIT : les valeurs ci-dessous ne sont
  // alors que des zéros, et un bouton qui les enverrait écraserait le vrai
  // réglage du joueur par un défaut.
  bool told = false;

  bool show_exp = false;
  bool show_zeny = false;
  bool show_mob_info = false;
  bool separate_kills = false;
  bool block_exp = false;
  bool show_skill_delay = false;
  bool show_speed = false;
  bool sell_stuff = false;
  bool sell_item = false;
  bool no_ask = false;
  bool wings = false;
  bool fly_wing_marker = false;
  KillStealGuard kill_steal_guard = KillStealGuard::kOff;

  int autoloot_rate_percent = 0;
  int autoloot_min_zeny = 0;
  // Un bit par type d'item, au rang du type : `1 << 6` pour les cartes.
  std::uint16_t autoloot_type_mask = 0;
  bool autoloot_rares = false;
  bool autoloot_mvp = false;
  bool autoloot_mvp_rewards = false;

  // ── LES PRESETS DE LA LISTE ───────────────────────────────────────────────
  std::vector<AutolootPreset> presets;
  // Le preset chargé en dernier, zéro pour aucun.
  std::uint8_t active_preset = 0;
  // LA LISTE TELLE QU'ELLE ÉTAIT À LA DERNIÈRE ANNONCE DES PRESETS. Le serveur
  // les annonce après chaque sauvegarde, chargement ou suppression : une liste
  // qui en diffère n'est donc pas sauvegardée.
  std::vector<std::uint32_t> saved_autoloot;
  // Compté à chaque annonce des presets. ⚠ C'EST CE QUI DIT « IL VIENT DE
  // PARLER » à qui en tient une copie — la liste elle-même peut revenir
  // identique.
  std::uint32_t presets_told_count = 0;
};

}  // namespace ro::sim
