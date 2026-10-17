#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "ro/ui/options_window.h"

namespace moonclient {

// ── LES BASCULES « / » QUI COCHENT UNE CASE DES OPTIONS ─────────────────────
//
// Chacune inverse un réglage qui a déjà sa case, et passe par la MÊME demande
// que la case : `GameOptions::Change` l'écrit, l'applique et l'enregistre. La
// commande ne fait qu'en écrire l'écho, celui que le client du joueur écrit.
//
// ⚠ UNE BASCULE AVEUGLE : seul le premier mot compte, sans la casse latine, et
// ce qui suit est ignoré — « /quake off » bascule comme « /quake », chez le
// client du joueur comme ici.
enum class SettingToggle : std::uint8_t {
  // `/monsterhp` : les barres de vie des monstres et des boss.
  kMonsterHp,
  // `/emblem` : l'emblème de guilde devant l'étiquette.
  kGuildEmblem,
  // `/window`, `/wi` : l'aimant qui pose une fenêtre qu'on traîne.
  kWindowSnap,
  // `/miss` : le mot « Miss » d'un coup raté.
  kMissShown,
  // `/skillfail`, `/sf` : les messages d'échec de compétence.
  kSkillFailShown,
  // `/quake` : les tremblements d'écran.
  kScreenQuake,
};

// La couleur de l'écho, le jaune pâle que le client du joueur donne à ses
// réglages, en `0xRRGGBB`.
inline constexpr std::uint32_t kSettingToggleEchoColor = 0xFFFF64;

// La bascule que la ligne demande, rien si elle n'en demande aucune.
[[nodiscard]] std::optional<SettingToggle> ReadSettingToggle(std::string_view line);

// LES DEMANDES QUE LA CASE ENVERRAIT pour poser le réglage à `on`, dans
// l'ordre. ⚠ PLUSIEURS POUR UNE COMMANDE : `/monsterhp` règle deux espèces.
[[nodiscard]] std::vector<ro::ui::OptionsWindowAction> SettingToggleChanges(SettingToggle toggle,
                                                                            bool on);

// L'écho à écrire une fois le réglage posé à `on`, traduit.
[[nodiscard]] const char* SettingToggleEcho(SettingToggle toggle, bool on);

}  // namespace moonclient
