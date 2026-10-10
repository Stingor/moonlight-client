#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/call_options.h"
#include "world/sprite_sound_mutes.h"

namespace moonclient {

// ── LES SONS DES MONSTRES ET DES PETS, DANS « SOUND » ───────────────────────
//
// Une case pour le bruit d'attente des pets, puis un bloc repliable : les sons
// de mort que le client d'origine ne faisait jamais entendre. Ce client les
// joue — voir `SpriteSoundMutes` —, et chacun se tait d'une case.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`, comme
// celle de l'appel d'urgence, dont la plage précède la sienne.
inline constexpr int kSpriteSoundRowBase = 7600;

// UN SON DE MORT À TAIRE : le fichier tel que les .act l'écrivent — fautes de
// Gravity comprises, « doppleganger » —, les monstres qui le portent, et le
// nombre de sprites de monstres du data.grf qui le jouent sur l'image 0 de
// leur mort.
//
// ⚠ UN RELEVÉ ET NON UN BALAYAGE : lire les deux mille .act de monstres à
// chaque ouverture du panneau coûterait plus que la liste ne vaut. Trois
// d'entre eux sont des sons de DÉGÂT que Gravity a posés sur la mort.
struct DeathSoundCandidate {
  std::string_view file;
  std::string_view monsters;
  int sprites = 0;
};

inline constexpr auto kDeathSounds = std::to_array<DeathSoundCandidate>({
    {.file = "doppleganger_die.wav", .monsters = "Bio Lab, Doppelganger", .sprites = 14},
    {.file = "baphomet_die.wav", .monsters = "Baphomet", .sprites = 4},
    {.file = "ork_warrior_die1.wav", .monsters = "Orc Warrior, Orc Archer", .sprites = 4},
    {.file = "osiris_die.wav", .monsters = "Osiris", .sprites = 3},
    {.file = "worm_tail_die.wav", .monsters = "Wormtail, Stem Worm", .sprites = 3},
    {.file = "baphomet__die.wav", .monsters = "Event Baphomet", .sprites = 2},
    {.file = "obeaune_die.wav", .monsters = "Iara", .sprites = 2},
    {.file = "orc_baby_damage.wav", .monsters = "Orc Baby", .sprites = 2},
    {.file = "pupa_die.wav", .monsters = "Pupa, Thief Bug Egg", .sprites = 2},
    {.file = "wolf_die.wav", .monsters = "Vagabond Wolf", .sprites = 2},
    {.file = "creamy_die.wav", .monsters = "Creamy Fear", .sprites = 1},
    {.file = "elder_wilow_die.wav", .monsters = "Elder Willow", .sprites = 1},
    {.file = "obeaune_damage.wav", .monsters = "Obeaune", .sprites = 1},
    {.file = "thanatos_damage.wav", .monsters = "Memory of Thanatos", .sprites = 1},
});

enum class SpriteSoundRow : int {
  kPetIdle = 0,
  kDeathIntro,
  kFirstDeath,
  kCount = kFirstDeath + static_cast<int>(kDeathSounds.size()),
};

// Cette ligne du panneau est-elle l'une des siennes ?
[[nodiscard]] constexpr bool IsSpriteSoundRow(int row_id) noexcept {
  return row_id >= kSpriteSoundRowBase &&
         row_id < kSpriteSoundRowBase + static_cast<int>(SpriteSoundRow::kCount);
}

static_assert(kCallRowBase + static_cast<int>(CallRow::kCount) <= kSpriteSoundRowBase,
              "la plage des sons de sprite chevauche celle de l'appel d'urgence");

class SpriteSoundOptions {
 public:
  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Ajoute les lignes à la section « Sound ». ⚠ AVANT QUE LA VUE DE LA
  // SECTION NE SE PRENNE : elle n'en garde qu'un span. ET EN DERNIER : le bloc
  // repliable court jusqu'à la fin de la section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows);

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // Ce que le joueur a tu, relu des cases à chaque appel.
  [[nodiscard]] SpriteSoundMutes mutes();

 private:
  bool pet_idle_muted_ = false;
  // ⚠ TOUT S'ENTEND PAR DÉFAUT : se taire est un choix du joueur.
  std::array<bool, kDeathSounds.size()> death_muted_{};
  std::vector<std::string> muted_files_;
  // Les libellés des lignes, remplis EN ENTIER avant que les vues ne les
  // désignent : un tampon réalloué sous elles les ferait pendre.
  std::vector<std::string> labels_;
};

}  // namespace moonclient
