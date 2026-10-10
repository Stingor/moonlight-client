#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_sound_table.h"
#include "ro/render/skill_effect_table.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_sound_table.h"
#include "ro/render/skill_suite_table.h"

namespace ro::render {
namespace {

// Le marqueur qu'un nom à variantes porte à la place de son numéro, et le
// premier numéro : le client écrit `rand() % n + 1`.
constexpr std::string_view kSoundVariantMark = "%d";
constexpr std::uint32_t kFirstSoundVariant = 1;
// Le multiplicateur de Knuth, puis les bits hauts : des tirages voisins — un
// instant et le rang d'un bruit — s'y dispersent au lieu de se suivre.
constexpr std::uint32_t kDrawMixer = 2654435761U;
constexpr std::uint32_t kDrawMixedShift = 16U;

// LES SONS D'EFFET CHOISIS PAR LE JOUEUR, à l'écart du client d'origine, qui
// n'en joue aucun pour ces effets. Ils ne passent qu'à défaut du relevé : la
// bouteille d'Acid Terror éclate comme UNE flèche de Fire Bolt, l'une des trois
// variantes tirée au sort.
constexpr std::uint8_t kFireBoltVariants = 3;
constexpr std::array<EffectSoundEntry, 1> kPlayerChosenEffectSounds = {{
    {.effect_id = 298, .sound = "effect\\EF_FireArrow%d.wav", .variants = kFireBoltVariants},
}};

}  // namespace

std::string EffectSoundVariantFile(const EffectSoundEntry& entry, std::uint32_t variant) {
  const std::size_t mark = entry.sound.find(kSoundVariantMark);
  if (entry.variants == 0 || mark == std::string_view::npos) {
    return std::string(entry.sound);
  }
  std::string file(entry.sound.substr(0, mark));
  file += std::to_string(variant);
  file.append(entry.sound.substr(mark + kSoundVariantMark.size()));
  return file;
}

std::string EffectSoundFile(const EffectSoundEntry& entry, std::uint32_t draw) {
  if (entry.variants == 0) {
    return std::string(entry.sound);
  }
  const std::uint32_t mixed = (draw * kDrawMixer) >> kDrawMixedShift;
  return EffectSoundVariantFile(entry, kFirstSoundVariant + mixed % entry.variants);
}

std::span<const EffectSoundEntry> EffectSounds(std::uint32_t effect_id) {
  const auto among = [effect_id](std::span<const EffectSoundEntry> table) {
    const auto first = std::ranges::lower_bound(table, effect_id, {}, &EffectSoundEntry::effect_id);
    const auto last = std::ranges::upper_bound(table, effect_id, {}, &EffectSoundEntry::effect_id);
    return table.subspan(static_cast<std::size_t>(first - table.begin()),
                         static_cast<std::size_t>(last - first));
  };
  if (const std::span<const EffectSoundEntry> measured = among(EffectSoundTable());
      !measured.empty()) {
    return measured;
  }
  return among(kPlayerChosenEffectSounds);
}

std::string_view EffectSound(std::uint32_t effect_id) {
  const std::span<const EffectSoundEntry> sounds = EffectSounds(effect_id);
  return sounds.empty() ? std::string_view{} : sounds.front().sound;
}

std::vector<EffectSoundEntry> SkillSounds(std::uint32_t skill_id) {
  std::vector<EffectSoundEntry> files;
  if (skill_id == 0) {
    return files;
  }
  // LE SON PROPRE DE LA COMPÉTENCE D'ABORD, et il REMPLACE celui de l'effet :
  // le client le joue dans le cas de son switch, avant d'en arriver au
  // répartiteur d'effets.
  if (const std::string_view own = SkillSound(skill_id); !own.empty()) {
    EffectSoundEntry table_entry;
    table_entry.sound = own;
    files.push_back(table_entry);
    return files;
  }
  // QUEL EFFET SONNE : celui que la compétence MONTRE.
  //
  // ⚠ LA FAMILLE PROCÉDURALE N'A PAS D'EFFET DE CE CÔTÉ : `SkillImpl` tient le
  // sien de `SkillEffectTable`, qui ne garde que les effets à fichier. Son
  // identifiant est dans le patron de nœuds, et c'est lui qui porte le son. Un
  // bruit pris sur l'autre ferait chercher un effet là où il n'y en a pas.
  std::uint32_t skill_effect_id = SkillImplFor(skill_id).EffectId();
  if (EffectSounds(skill_effect_id).empty()) {
    if (const ProceduralSkillEffect* seed =
            DrawableProceduralEffectOfSkill(static_cast<std::uint16_t>(skill_id));
        seed != nullptr) {
      skill_effect_id = seed->effect_id;
    }
  }
  if (EffectSoundPlaysAtSowing(skill_effect_id)) {
    return files;
  }
  const std::span<const EffectSoundEntry> own_entries = EffectSounds(skill_effect_id);
  files.assign(own_entries.begin(), own_entries.end());
  return files;
}

bool SkillPathShowsEffect(std::uint16_t skill_id, bool on_hit) {
  if (on_hit ? SkillDamageEffectOf(skill_id) != nullptr
             : SkillAnnouncedEffectOf(skill_id) != nullptr) {
    return true;
  }
  return std::ranges::any_of(
      SkillSuiteOf(skill_id, on_hit ? SkillSuiteMoment::kImpact : SkillSuiteMoment::kAnnounced),
      [](const SkillSuiteStep& step) { return step.kind == SkillSuiteKind::kEffect; });
}

std::vector<EffectSoundEntry> SkillSoundsOnPath(std::uint16_t skill_id, bool on_hit) {
  if (SkillSound(skill_id).empty() && !SkillPathShowsEffect(skill_id, on_hit)) {
    return {};
  }
  return SkillSounds(skill_id);
}

}  // namespace ro::render
