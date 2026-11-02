#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LES JOUEURS BLOQUÉS : CHUCHOTEMENTS ET VOIX, EN UNE SEULE LISTE ─────────
//
// Une ligne par joueur, une case par canal : décochée, le canal est rétabli.
// « Débloquer » rétablit les deux. Le champ du haut bloque un nom tapé.
//
// 🔴 LES DEUX CANAUX N'ONT PAS LA MÊME CLÉ. Un chuchotement se bloque par NOM,
// connu ou non, en ligne ou non. Une voix se bloque par PERSONNAGE, et la voix
// ne connaît que ceux qu'elle entend : un nom tapé ne coupe sa voix que s'il
// est à portée. La fenêtre le dit plutôt que de griser sans raison.
//
// ⚠ LA FENÊTRE NE CHANGE RIEN ELLE-MÊME. Elle rend un geste ; la liste ne bouge
// qu'à la réponse du serveur, et la vue suivante le montre.

inline constexpr const char* kBlockListWindowId = "joueurs_bloques";

// Le plus long nom qu'on tape, terminateur compris : un nom de personnage
// tient en 24 octets en 1252, davantage en UTF-8.
inline constexpr std::size_t kBlockNameBytes = 64;

struct BlockedPlayerView {
  std::string name;
  bool whispers = false;  // ses chuchotements sont jetés
  bool voice = false;     // sa voix est coupée
  // La voix le connaît, et peut donc le couper. Toujours vrai d'une voix déjà
  // coupée : on la rétablit sans l'entendre.
  bool voice_reachable = false;
};

struct BlockListView {
  // Triés par nom, sans doublon.
  std::vector<BlockedPlayerView> players;
  // « Bloquer tout le monde » est en vigueur : aucun chuchotement n'arrive.
  bool whispers_from_everyone = false;
  // Le serveur de carte écoute. Sans lui, aucun geste ne part.
  bool connected = false;
  // La liaison voix est ouverte : sans elle, la voix ne dit pas qui elle coupe.
  bool voice_online = false;
  // La liste des chuchotements est rangée au service web, et survit donc à la
  // déconnexion.
  bool whispers_kept = false;
};

enum class BlockListVerb : std::uint8_t {
  kNothing,
  // Le nom tapé : ses chuchotements, et sa voix s'il est à portée.
  kBlock,
  kBlockWhispers,
  kAllowWhispers,
  kBlockVoice,
  kAllowVoice,
  // Les deux canaux à la fois.
  kAllowBoth,
  kBlockEveryone,
  kAllowEveryone,
};

struct BlockListWindowState {
  bool open = false;
  // Mise au premier plan à la prochaine image.
  bool raise = false;
  std::array<char, kBlockNameBytes> name{};
  std::array<char, kBlockNameBytes> filter{};
};

struct BlockListAnswer {
  BlockListVerb verb = BlockListVerb::kNothing;
  // Vide pour les deux gestes « tout le monde ».
  std::string name;
};

// Ouvre la fenêtre, ou la ramène devant.
void RaiseBlockListWindow(BlockListWindowState& state) noexcept;

// LES RANGS DES JOUEURS QUE LE FILTRE LAISSE PASSER : ceux dont le nom contient
// le texte, à la casse près. Un filtre vide les laisse tous.
[[nodiscard]] std::vector<std::size_t> BlockListShown(std::span<const BlockedPlayerView> players,
                                                      std::string_view filter);

[[nodiscard]] BlockListAnswer DrawBlockListWindow(BlockListWindowState& state,
                                                  const BlockListView& view, const SkinArt& art);

}  // namespace ro::ui
