#include "options/sprite_sound_options.h"

#include <array>
#include <cstdio>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. ⚠ GRAVÉES : les changer rendrait la parole aux sons
// que le joueur avait tus, sans que rien ne le dise.
constexpr const char* kPetIdleMutedKey = "sound_idle_pets_muted";
constexpr std::string_view kDeathMutedKeyPrefix = "sound_death_muted_";

[[nodiscard]] int IdOf(SpriteSoundRow row) { return kSpriteSoundRowBase + static_cast<int>(row); }

[[nodiscard]] int IdOfDeath(std::size_t rank) {
  return IdOf(SpriteSoundRow::kFirstDeath) + static_cast<int>(rank);
}

// La clé d'un son de mort : son nom de fichier, sans l'extension.
[[nodiscard]] std::string DeathMutedKey(std::string_view file) {
  const std::size_t dot = file.rfind('.');
  return std::string(kDeathMutedKeyPrefix) + std::string(file.substr(0, dot));
}

}  // namespace

void SpriteSoundOptions::Declare(SettingsFile& into) {
  into.Declare(kPetIdleMutedKey, &pet_idle_muted_);
  for (std::size_t rank = 0; rank < kDeathSounds.size(); ++rank) {
    into.Declare(DeathMutedKey(kDeathSounds[rank].file), &death_muted_[rank]);
  }
}

void SpriteSoundOptions::ComposeInto(std::vector<OptionRowView>& rows) {
  labels_.clear();
  labels_.reserve(kDeathSounds.size());
  for (const DeathSoundCandidate& sound : kDeathSounds) {
    std::array<char, 160> label{};
    std::snprintf(label.data(), label.size(), Tr("%s   %s — %d sprite(s)"),
                  std::string(sound.file).c_str(), std::string(sound.monsters).c_str(),
                  sound.sprites);
    labels_.emplace_back(label.data());
  }

  rows.push_back(OptionRowView{
      .id = IdOf(SpriteSoundRow::kPetIdle),
      .label = Tr("Couper le bruit d'attente des pets"),
      .help = Tr("Le son que l'animation d'attente d'un pet rejoue à chaque tour de boucle, "
                 "tant qu'il ne bouge pas."),
      .kind = OptionKind::kToggle,
      .on = pet_idle_muted_});

  rows.push_back(OptionRowView{
      .id = IdOf(SpriteSoundRow::kDeathIntro),
      .label = Tr("Un son de trop ? Coche-le pour le taire, partout dans le jeu.\nLe client "
                  "d'origine ne faisait jamais entendre ces sons de mort : il sautait la "
                  "première image d'une animation. Celui-ci les joue tous."),
      .kind = OptionKind::kNote});
  rows.back().fold = Tr("Sons de mort");
  for (std::size_t rank = 0; rank < kDeathSounds.size(); ++rank) {
    rows.push_back(OptionRowView{.id = IdOfDeath(rank),
                                 .label = labels_[rank].c_str(),
                                 .kind = OptionKind::kToggle,
                                 .on = death_muted_[rank]});
  }
}

bool SpriteSoundOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (!IsSpriteSoundRow(asked.changed)) {
    return false;
  }
  if (asked.changed == IdOf(SpriteSoundRow::kPetIdle)) {
    pet_idle_muted_ = asked.on;
  } else if (asked.changed >= IdOf(SpriteSoundRow::kFirstDeath)) {
    death_muted_[static_cast<std::size_t>(asked.changed - IdOf(SpriteSoundRow::kFirstDeath))] =
        asked.on;
  }
  return true;
}

SpriteSoundMutes SpriteSoundOptions::mutes() {
  muted_files_.clear();
  for (std::size_t rank = 0; rank < kDeathSounds.size(); ++rank) {
    if (death_muted_[rank]) {
      muted_files_.push_back(SoundFileKey(kDeathSounds[rank].file));
    }
  }
  return SpriteSoundMutes{.pet_idle = pet_idle_muted_, .files = muted_files_};
}

}  // namespace moonclient
