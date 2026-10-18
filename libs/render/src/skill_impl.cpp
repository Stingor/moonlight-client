#include "ro/render/skill_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "ro/core/sorted_table.h"
#include "ro/render/cast_bar.h"

namespace ro::render {
namespace {

// TOUT ce que les tables engendrées disent d'une compétence, réuni.
//
// Les deux tables ne portent pas les mêmes compétences — l'une vient du chemin
// des dégâts, l'autre de celui de l'incantation —, et une compétence qui n'est
// que dans l'une garde les défauts de l'autre.
struct SkillFacts {
  std::uint16_t skill_id = 0;
  std::uint16_t effect_id = 0;
  SkillEffectOn on = SkillEffectOn::kTarget;
  std::int16_t cast_effect_id = kCastEffectDefault;
  std::int8_t cast_state = kCastStateDefault;
};

// Le cas GÉNÉRAL : ce que les tables engendrées disent, et rien de plus.
class TableSkill : public SkillImpl {
 public:
  explicit TableSkill(const SkillFacts& facts) : SkillImpl(facts.skill_id), facts_(facts) {}

  [[nodiscard]] std::uint16_t EffectId() const override { return facts_.effect_id; }
  [[nodiscard]] SkillEffectOn EffectOn() const override { return facts_.on; }
  [[nodiscard]] std::int16_t CastEffectId() const override { return facts_.cast_effect_id; }
  [[nodiscard]] std::int8_t CastState() const override { return facts_.cast_state; }

 private:
  SkillFacts facts_;
};

// Une compétence dont le client ne montre JAMAIS la barre d'incantation, bien
// que le serveur en annonce une.
//
// Le serveur donne une durée à Brandish Spear et à Bowling Bash, et le client
// ne montre rien : sa barre ne naît pas de l'annonce mais d'un appel que ces
// deux compétences ne font pas. Montrer la nôtre laisserait une barre pleine
// sous un personnage qui a déjà frappé.
//
// 🔴 NI D'EFFET D'INCANTATION : la table donne 144, la lance, à Brandish
// Spear, mais le client refuse de faire naître l'effet d'incantation de ces
// deux compétences. La lance ne naît qu'au coup, sur le lanceur, par l'effet
// d'emploi ; la poser aussi à l'incantation la montrait deux fois.
class SilentCastSkill : public TableSkill {
 public:
  using TableSkill::TableSkill;

  [[nodiscard]] bool ShowsCastBar(std::uint32_t /*duration_ms*/) const override { return false; }
  [[nodiscard]] bool CastSpawnsEffect() const override { return false; }
};

// Ce que rend un numéro dont la table ne sait rien.
const SkillImpl& Unknown() {
  static const SkillImpl unknown{0};
  return unknown;
}

// Les compétences qui ont une classe à elles.
[[nodiscard]] std::unique_ptr<const SkillImpl> Special(const SkillFacts& facts) {
  if (facts.skill_id == kBrandishSpearSkill || facts.skill_id == kBowlingBashSkill) {
    return std::make_unique<SilentCastSkill>(facts);
  }
  return nullptr;
}

// Les deux tables engendrées, fusionnées par identifiant de compétence.
//
// Toutes deux sont triées, et c'est ce qui permet de les parcourir de front :
// une compétence connue d'une seule garde les défauts de l'autre.
[[nodiscard]] std::vector<SkillFacts> MergedFacts() {
  const std::span<const SkillEffectEntry> effects = SkillEffectTable();
  const std::span<const SkillCastEntry> casts = SkillCastTable();
  std::vector<SkillFacts> facts;
  facts.reserve(effects.size() + casts.size());

  std::size_t effect_index = 0;
  std::size_t cast_index = 0;
  while (effect_index < effects.size() || cast_index < casts.size()) {
    const bool effects_left = effect_index < effects.size();
    const bool casts_left = cast_index < casts.size();
    const std::uint16_t skill_id =
        !casts_left     ? effects[effect_index].skill_id
        : !effects_left ? casts[cast_index].skill_id
                        : std::min(effects[effect_index].skill_id, casts[cast_index].skill_id);

    SkillFacts merged;
    merged.skill_id = skill_id;
    if (effects_left && effects[effect_index].skill_id == skill_id) {
      merged.effect_id = effects[effect_index].effect_id;
      merged.on = effects[effect_index].on;
      ++effect_index;
    }
    if (casts_left && casts[cast_index].skill_id == skill_id) {
      merged.cast_effect_id = casts[cast_index].effect_id;
      merged.cast_state = casts[cast_index].state;
      ++cast_index;
    }
    facts.push_back(merged);
  }
  return facts;
}

// LA FABRIQUE : une instance par compétence connue, rangée par identifiant.
//
// Construite UNE fois, jamais modifiée, et rendue par référence constante. Ce
// n'est pas le singleton que ce dépôt refuse — il n'y a ni état de jeu, ni
// service à joindre de partout : c'est la table engendrée, sous une autre
// forme, et deux exemplaires en donneraient les mêmes réponses.
class SkillRegistry {
 public:
  SkillRegistry() {
    const std::vector<SkillFacts> facts = MergedFacts();
    skills_.reserve(facts.size() + kSilentCastCount);
    for (const SkillFacts& entry : facts) {
      std::unique_ptr<const SkillImpl> special = Special(entry);
      skills_.push_back(special != nullptr ? std::move(special)
                                           : std::make_unique<TableSkill>(entry));
    }
    // Les compétences à comportement PROPRE que les tables ne portent pas :
    // elles n'ont rien à montrer, mais elles ont quelque chose à dire.
    for (const std::uint16_t skill_id : {kBrandishSpearSkill, kBowlingBashSkill}) {
      if (Find(skill_id) == nullptr) {
        SkillFacts silent;
        silent.skill_id = skill_id;
        Insert(std::make_unique<SilentCastSkill>(silent));
      }
    }
  }

  [[nodiscard]] const SkillImpl* Find(std::uint32_t skill_id) const {
    const auto found = std::ranges::lower_bound(skills_, skill_id, {},
                                                [](const std::unique_ptr<const SkillImpl>& skill) {
                                                  return std::uint32_t{skill->skill_id()};
                                                });
    if (found == skills_.end() || (*found)->skill_id() != skill_id) {
      return nullptr;
    }
    return found->get();
  }

 private:
  // Combien de compétences peuvent s'ajouter à la table sans y figurer.
  static constexpr std::size_t kSilentCastCount = 2;

  void Insert(std::unique_ptr<const SkillImpl> skill) {
    const std::uint16_t skill_id = skill->skill_id();
    const auto place = std::ranges::lower_bound(skills_, std::uint32_t{skill_id}, {},
                                                [](const std::unique_ptr<const SkillImpl>& other) {
                                                  return std::uint32_t{other->skill_id()};
                                                });
    skills_.insert(place, std::move(skill));
  }

  std::vector<std::unique_ptr<const SkillImpl>> skills_;
};

}  // namespace

const SkillImpl& SkillImplFor(std::uint32_t skill_id) {
  static const SkillRegistry registry;
  const SkillImpl* found = registry.Find(skill_id);
  return found != nullptr ? *found : Unknown();
}

std::int16_t CastEffectForElement(std::int16_t cast_effect_id, std::uint32_t element) {
  if (cast_effect_id != kCastEffectElemental) {
    return cast_effect_id;
  }
  const std::span<const std::int16_t> auras = CastElementalEffects();
  if (element >= auras.size()) {
    return cast_effect_id;
  }
  return auras[element];
}

std::int16_t CastEffectForJob(std::uint16_t skill_id, std::int16_t cast_effect_id,
                              std::uint32_t job) {
  for (const CastEffectJobOverride& override_entry : CastEffectJobOverrideTable()) {
    if (override_entry.skill_id == skill_id && job >= override_entry.first_job) {
      return override_entry.effect_id;
    }
  }
  return cast_effect_id;
}

std::optional<std::size_t> ActionAnimationFor(std::int8_t action_state, SpriteLayout layout,
                                              std::uint32_t job_id) {
  if (action_state == kCastStateNone) {
    return std::nullopt;
  }
  if (layout != SpriteLayout::kPlayer) {
    return std::nullopt;
  }
  // ⚠ LE MÉTIER PASSE AVANT LE RANG COMMUN. Trois états seulement en dépendent,
  // mais l'un d'eux est celui que prend par défaut la quasi-totalité des
  // compétences au chemin de l'emploi.
  const std::span<const CastStateJobRank> by_job = CastStateJobRankTable();
  const auto exception = std::ranges::lower_bound(
      by_job, std::pair{action_state, static_cast<std::uint16_t>(job_id)}, {},
      [](const CastStateJobRank& entry) { return std::pair{entry.state, entry.job}; });
  if (exception != by_job.end() && exception->state == action_state && exception->job == job_id) {
    return std::size_t{exception->animation};
  }
  const CastStateEntry* found =
      core::RowWithKey(CastStateTable(), action_state, &CastStateEntry::state);
  if (found == nullptr) {
    // Un état que la table ne nomme pas ne joue rien chez le client : ses deux
    // tables rendent alors le rang ZÉRO, et SetActionAnim n'a pas de cas pour
    // lui. Mieux vaut laisser la posture en place que planter l'acteur debout.
    return std::nullopt;
  }
  return std::size_t{found->animation};
}

}  // namespace ro::render
