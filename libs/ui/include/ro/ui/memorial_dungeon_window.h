#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'INSTANCE (« MEMORIAL DUNGEON »), la fenêtre d'Alt+B : son nom, sa place en
// file ou son temps restant, et le bouton qui la détruit.
//
// Celle du jeu ne dit que le nom, la place en file et les deux échéances.
// Celle-ci y ajoute ce que le serveur de ce jeu sait seul : la difficulté (HM)
// et le mode turbo choisis à la création, le propriétaire, les joueurs
// présents, des jauges sur les durées totales, et POURQUOI le bouton de
// destruction est éteint quand il l'est.
//
// ⚠ ELLE NE POSSÈDE RIEN : l'étage de la partie compose la vue, et c'est lui
// qui envoie la destruction que le joueur a confirmée.

inline constexpr const char* kMemorialDungeonWindowId = "memorial_dungeon";
inline constexpr float kMemorialDungeonWindowWidth = 320.0F;

// À QUI APPARTIENT L'INSTANCE.
enum class MemorialOwnerView : std::uint8_t { kUnknown, kCharacter, kParty, kGuild, kClan };

// CE QUE MONTRE LE BOUTON DE DESTRUCTION, et pourquoi.
enum class MemorialDestroyGate : std::uint8_t {
  // L'instance ne se détruit pas depuis la fenêtre : pas de bouton.
  kHidden,
  kAllowed,
  // Le serveur n'a pas dit qui peut détruire : il jugera.
  kUnknownRights,
  // Seul le propriétaire, le chef du groupe ou le maître de la guilde le peut.
  kNotOwner,
  // Le serveur refuse toute destruction depuis une carte d'instance.
  kOnInstanceMap,
  // La demande est partie, la réponse n'est pas venue.
  kPending,
};

struct MemorialDungeonView {
  // Une instance existe, réservée ou ouverte.
  bool present = false;
  // Réservée, en file ; sinon ouverte.
  bool waiting = false;
  std::string_view name;
  int wait_position = 0;
  // Le détail maison est arrivé : difficulté, turbo, propriétaire, présents.
  bool detailed = false;
  int hard_mode = 1;
  bool turbo = false;
  MemorialOwnerView owner = MemorialOwnerView::kUnknown;
  std::string_view owner_name;
  int players_inside = 0;
  // Le temps restant, et la durée totale qui le rapporte à une jauge (zéro :
  // inconnue). Rien : sans limite.
  std::optional<std::int64_t> seconds_left;
  std::uint32_t duration_s = 0;
  // L'heure locale de la fin, « 21:47 » ; vide sans limite.
  std::string_view end_clock;
  // La fermeture faute de joueurs, de même. Rien : quelqu'un est dedans.
  std::optional<std::int64_t> idle_seconds_left;
  std::uint32_t idle_timeout_s = 0;
  MemorialDestroyGate destroy = MemorialDestroyGate::kHidden;
};

struct MemorialDungeonWindowState {
  bool open = false;
};

struct MemorialDungeonWindowAction {
  // Le joueur a confirmé la destruction.
  bool destroy = false;
};

// LE NOM DE LA DIFFICULTÉ, celui du menu où le chef de groupe la choisit ;
// nul pour un niveau que ce menu ne propose pas.
[[nodiscard]] const char* MemorialHardModeName(int hard_mode);

// « 1 h 02 min 03 s », « 4 min 05 s », « 12 s » : une durée lisible d'un coup
// d'œil, déjà traduite.
[[nodiscard]] std::string MemorialDuration(std::int64_t seconds);

[[nodiscard]] MemorialDungeonWindowAction DrawMemorialDungeonWindow(
    MemorialDungeonWindowState& state, const MemorialDungeonView& view, const SkinArt& art);

}  // namespace ro::ui
