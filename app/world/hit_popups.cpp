#include "world/hit_popups.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include "ro/render/second_hand.h"
#include "ro/render/skill_damage_timing.h"
#include "ro/render/sprite_billboard.h"
#include "ro/sim/inventory.h"
#include "ro/sim/movement.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/world.h"

#include "world/actor_view.h"
#include "world/number_paths.h"
#include "world/struck_place.h"

namespace moonclient {

bool HitPopups::Alive(const Popup& popup, std::uint32_t now_ms, const HitPopupLook& look) const {
  if (now_ms < popup.born_ms) {
    return true;
  }
  const auto elapsed_ms = static_cast<float>(now_ms - popup.born_ms);
  // Un mot garde la vie du client, que son animation tient.
  if (popup.mark.has_value()) {
    return mark_animation_ != nullptr &&
           elapsed_ms < ro::render::DamageMarkLifeMs(*mark_animation_, *popup.mark);
  }
  return !ro::render::FloatingNumberExpired(
      popup.number.style, elapsed_ms,
      OnPath(look.motion, PathOf(look, popup.path), PathLifeOf(look, popup.path), popup.scatter));
}

void HitPopups::NoticeSupport(const ro::sim::World& world, const HitPopupLook& look) {
  for (const auto& [id, actor] : world.actors()) {
    if (!actor.support.has_value()) {
      continue;
    }
    const ro::sim::SupportHit& support = *actor.support;
    const auto seen = noticed_support_.find(id);
    if (seen != noticed_support_.end() && seen->second == support.started_ms) {
      continue;
    }
    noticed_support_.insert_or_assign(id, support.started_ms);
    SpawnSupport(id, support.started_ms, support.skill_id, support.amount, look);
  }
}

void HitPopups::SpawnSupport(std::uint32_t carrier_id, std::uint32_t born_ms,
                             std::uint16_t skill_id, std::int32_t amount,
                             const HitPopupLook& look) {
  const std::optional<ro::render::FloatingNumber> number = SupportNumber(skill_id, look);
  // Rien à montrer : la compétence n'a pas de nombre, le réglage le tait, ou
  // elle n'a rien rendu. Un montant nul n'est pas un « zéro » à écrire — la
  // plupart des compétences sans dégâts n'emploient même pas ce champ.
  if (!number.has_value() || amount <= 0) {
    return;
  }
  Popup popup;
  popup.carrier_id = carrier_id;
  popup.born_ms = born_ms;
  popup.number = *number;
  popup.number.value = static_cast<std::uint32_t>(amount);
  popup.path = DamagePath::kHeal;
  popup.scatter = ScatterOf(PathOf(look, popup.path), ScatterSeed(carrier_id, born_ms, 0));
  numbers_.push_back(popup);
}

namespace {

// LE MESSAGE QUI FAIT PARAÎTRE UN CHIFFRE au-dessus d'un acteur.
//
// Trois effets du lot de l'acteur ne dessinent rien et envoient cet ordre à leur
// cible — `EF_DAMAGE1` et ses deux variantes —, avec une valeur de UN et une
// couleur écrite en clair. C'est le même chemin que les chiffres d'un coup, et
// c'est pourquoi il vit ici.
constexpr std::uint16_t kDamageNumberCommand = 0x58;

// L'allure que le client donne à ce nombre : le type cent quatorze, celui d'un
// montant rendu.
constexpr std::int32_t kLeastVisibleAmount = 1;

// LA CIBLE FACTICE DU CADRE D'ESSAI. Il n'a pas de carte, donc aucun acteur ne
// la porte : elle n'a qu'à n'être pas zéro.
constexpr std::uint32_t kSampleCarrierId = 1;

// Le chemin que la dérive de côté a parcouru à cet instant, en unités du monde.
// ⚠ ELLE SUIT LA VIE DU NOMBRE, rapportée à celle du client : une vie deux fois
// plus longue la parcourt deux fois plus lentement, et elle finit au même
// point.
[[nodiscard]] float DriftUnitsAt(float elapsed_ms, ro::render::NumberStyle style,
                                 const std::optional<ro::render::NumberLife>& life) {
  const ro::render::NumberLife client = ro::render::ClientLifeOf(style);
  const float stretch =
      life.has_value() ? ro::render::SaneLifeMs(life->life_ms) / client.life_ms : 1.0F;
  return ro::render::BurstDriftUnitsAt(elapsed_ms / (ro::render::kNumberTickMs * stretch));
}

// Vrai si ce nombre dérive de côté : la trajectoire de son type le dit, et un
// mot ne dérive jamais.
[[nodiscard]] bool Drifts(bool marked, DamagePath path, const HitPopupLook& look) {
  return !marked && PathOf(look, path).facing_drift;
}

}  // namespace

// Repère les effets d'ACTEUR qui commandent un chiffre, et le fait paraître.
//
// ⚠ LA COULEUR VIENT DE LA TABLE, PAS D'UNE RÈGLE. Deux de ces trois effets
// écrivent un chiffre ROUGE et le troisième un MAGENTA ; rien dans leur numéro
// ne le dit. Le réglage ne choisit donc que l'allure — celle du soin, comme le client — et s'il se
// montre.
void HitPopups::NoticeCommandedNumbers(const ro::sim::World& world, const HitPopupLook& look) {
  for (const auto& [id, actor] : world.actors()) {
    for (const ro::sim::PlayedEffect& played : actor.effects) {
      const ro::render::ActorEffectChange* change = ro::render::ActorEffectChangeFor(played.id);
      if (change == nullptr || change->command != kDamageNumberCommand ||
          change->command_value < kLeastVisibleAmount) {
        continue;
      }
      const auto key = std::pair{id, played.id};
      const auto seen = noticed_effect_.find(key);
      if (seen != noticed_effect_.end() && seen->second == played.since_ms) {
        continue;
      }
      noticed_effect_.insert_or_assign(key, played.since_ms);
      if (!look.shown || !look.effect_numbers) {
        continue;
      }

      Popup popup;
      popup.carrier_id = id;
      popup.born_ms = played.since_ms;
      popup.number.value = static_cast<std::uint32_t>(change->command_value);
      // La couleur est en alpha-rouge-vert-bleu, en clair : le dispatcher de
      // messages de l'acteur la passe telle quelle.
      popup.number.color = {static_cast<std::uint8_t>((change->command_color >> 16U) & 0xFFU),
                            static_cast<std::uint8_t>((change->command_color >> 8U) & 0xFFU),
                            static_cast<std::uint8_t>(change->command_color & 0xFFU)};
      popup.number.style = StyleOfPath(DamagePath::kHeal);
      popup.path = DamagePath::kHeal;
      popup.scatter = ScatterOf(PathOf(look, popup.path), ScatterSeed(id, popup.born_ms, 0));
      numbers_.push_back(popup);
    }
  }
}

void HitPopups::NoticeEffectNumbers(const ro::sim::World& world, const HitPopupLook& look) {
  for (const auto& [id, actor] : world.actors()) {
    for (const ro::sim::PlayedEffect& played : actor.effects) {
      if (!played.value.has_value()) {
        continue;
      }
      const auto key = std::pair{id, played.id};
      const auto seen = noticed_effect_.find(key);
      if (seen != noticed_effect_.end() && seen->second == played.since_ms) {
        continue;
      }
      noticed_effect_.insert_or_assign(key, played.since_ms);

      const std::optional<ro::render::FloatingNumber> number = EffectNumber(played.id, look);
      if (!number.has_value()) {
        continue;
      }
      Popup popup;
      popup.carrier_id = id;
      popup.born_ms = played.since_ms;
      popup.number = *number;
      popup.number.value = ShownEffectAmount(*played.value);
      popup.path = DamagePath::kHeal;
      popup.scatter = ScatterOf(PathOf(look, popup.path), ScatterSeed(id, popup.born_ms, 0));
      numbers_.push_back(popup);
    }
  }
}

void HitPopups::Spawn(std::uint32_t target_id, std::uint32_t lands_ms,
                      const ro::sim::Strike& strike, const StrikeSides& sides,
                      const HitPopupLook& look) {
  const StrikePlan plan = PlanStrike(strike, sides, look);
  if (!plan.shown) {
    return;
  }
  const bool marks_loaded = mark_sprite_ != nullptr && mark_animation_ != nullptr;
  // Un MOT ne se répète pas : le client n'en pose qu'un, quand bien même le
  // coup annoncerait plusieurs frappes.
  if (plan.mark.has_value()) {
    if (!marks_loaded) {
      return;
    }
    // ⚠ LE « MISS » NE VA PAS SUR LA CIBLE : il naît sur celui qui a raté. La
    // règle et sa raison sont dans `StrikePlan::mark_on_attacker`.
    const std::uint32_t carrier_id = plan.mark_on_attacker ? strike.attacker : target_id;
    Popup popup;
    popup.carrier_id = carrier_id;
    popup.born_ms = lands_ms;
    popup.mark = plan.mark;
    popup.number = plan.number;
    popup.path = plan.path;
    popup.scatter = ScatterOf(PathOf(look, popup.path), ScatterSeed(carrier_id, popup.born_ms, 0));
    numbers_.push_back(popup);
    return;
  }
  // Le pas choisi par le joueur, et ce qu'une compétence y ajoute.
  const float hit_interval_ms =
      std::max(look.hit_interval_ms, 0.0F) +
      static_cast<float>(ro::render::SkillHitStepExtraMs(strike.skill_id));
  // CE QUE LA CIBLE PORTE DÉJÀ, quand le total continu est allumé : le second
  // nombre repart de cette somme au lieu de zéro, et un sort de zone dit enfin
  // ce qu'il a fait en tout. Éteint, chaque annonce recommence — le client.
  const bool carries = plan.total.has_value() && look.total_carries;
  const std::uint32_t carried =
      carries ? CarriedTotal(tallies_[target_id], lands_ms, look.total_forget_ms) : 0U;
  const auto step_ms = static_cast<std::uint32_t>(hit_interval_ms);
  const NumberPath& part_path = PathOf(look, plan.path);
  const NumberPath& total_path = PathOf(look, DamagePath::kTotal);
  for (std::uint16_t hit = 0; hit < plan.count; ++hit) {
    const std::uint32_t born_ms = lands_ms + StrikeHitOffsetMs(plan, hit, step_ms);
    Popup popup;
    popup.carrier_id = target_id;
    popup.born_ms = born_ms;
    popup.number = plan.number;
    popup.number.value = StrikeValueAt(strike, plan, hit);
    // L'ÉTOILE d'impact PORTE le chiffre au lieu de le remplacer : c'est la
    // seule marque qui paraisse avec un nombre, et le client en pose une par
    // coup. Elle vit dans le même cadre, donc derrière lui et à son rythme.
    popup.star = plan.star && marks_loaded;
    // L'étoile d'un coup de pied ne va qu'au PREMIER nombre : les frappes
    // suivantes passent par un autre message, qui ne la pose pas.
    popup.rising_star = plan.rising_star && hit == 0 && marks_loaded;
    // Chaque nombre d'un coup multiple tire son propre écart : c'est pour eux
    // que la dispersion existe.
    popup.path = plan.path;
    popup.scatter = ScatterOf(part_path, ScatterSeed(target_id, lands_ms, hit));
    numbers_.push_back(popup);
    // LE TOTAL, né avec sa part et tiré à part : les rangs au-delà du compte
    // sont à lui.
    if (plan.total.has_value()) {
      Popup total;
      total.carrier_id = target_id;
      total.born_ms = born_ms;
      total.number = *plan.total;
      total.path = DamagePath::kTotal;
      total.number.value = carried + StrikeTotalAt(strike, plan, hit);
      total.scatter =
          ScatterOf(total_path,
                    ScatterSeed(target_id, lands_ms, static_cast<std::uint16_t>(plan.count + hit)));
      numbers_.push_back(total);
      // ⚠ LA SOMME SE RETIENT À L'INSTANT DU NOMBRE, pas à celui du paquet :
      // c'est cet instant-là que l'oubli mesure, et les frappes d'un coup
      // multiple s'étalent.
      if (carries) {
        tallies_[target_id] = TargetTally{.total = total.number.value, .last_ms = born_ms};
      }
    }
  }
  // LA SECONDE MAIN, après toutes les frappes de la première : ses dégâts à
  // elle, entiers, et le total qu'elle achève. Voir `ro/render/second_hand.h`.
  if (!plan.second_hand) {
    return;
  }
  const std::uint32_t second_ms = lands_ms + ro::render::SecondHandOffsetMs(step_ms);
  const auto after_first = static_cast<std::uint16_t>(plan.count * 2U);
  Popup second;
  second.carrier_id = target_id;
  second.born_ms = second_ms;
  second.number = plan.number;
  second.number.value = static_cast<std::uint32_t>(std::max(strike.second_damage, 0));
  second.star = plan.star && marks_loaded;
  second.path = plan.path;
  second.scatter = ScatterOf(part_path, ScatterSeed(target_id, lands_ms, after_first));
  numbers_.push_back(second);
  if (plan.total.has_value()) {
    Popup total;
    total.carrier_id = target_id;
    total.born_ms = second_ms;
    total.number = *plan.total;
    total.path = DamagePath::kTotal;
    total.number.value = carried +
                         StrikeTotalAt(strike, plan, static_cast<std::uint16_t>(plan.count - 1)) +
                         second.number.value;
    total.scatter = ScatterOf(
        total_path, ScatterSeed(target_id, lands_ms, static_cast<std::uint16_t>(after_first + 1U)));
    numbers_.push_back(total);
    if (carries) {
      tallies_[target_id] = TargetTally{.total = total.number.value, .last_ms = second_ms};
    }
  }
}

void HitPopups::Notice(const ro::sim::World& world, ro::render::ActorWardrobe* wardrobe,
                       const HitPopupLook& look) {
  NoticeSupport(world, look);
  NoticeEffectNumbers(world, look);
  NoticeCommandedNumbers(world, look);
  // ⚠ LE CURSEUR AVANCE AVANT QU'ON DÉCIDE : un coup que le réglage tait n'est
  // pas un coup à rejouer quand on le rallume.
  for (const ro::sim::RecordedStrike& taken : noticed_strikes_.Take(world.strikes())) {
    const ro::sim::Strike& strike = taken.strike;
    const std::uint32_t id = strike.target;
    // UNE UNITÉ AU SOL FRAPPÉE PORTE SON CHIFFRE comme un monstre, à sa case —
    // même retirée depuis : voir `world/struck_place.h`.
    if (world.Find(id) == nullptr && !strike.struck_unit_cell.has_value()) {
      continue;
    }
    const StrikeSides sides = SidesOfStrike(world, strike);
    std::uint32_t lands_ms = strike.started_ms;
    if (wardrobe != nullptr) {
      lands_ms += static_cast<std::uint32_t>(HitLandsAtMs(*wardrobe, world, strike));
    }
    const std::size_t spawned_from = numbers_.size();
    Spawn(id, lands_ms, strike, sides, look);
    for (std::size_t index = spawned_from; index < numbers_.size(); ++index) {
      if (numbers_[index].carrier_id == id) {
        numbers_[index].kept_unit_cell = strike.struck_unit_cell;
      }
    }
  }
}

void HitPopups::Rehearse(std::uint32_t now_ms, const ro::sim::World& world,
                         const HitPopupLook& look) {
  if (!look.preview) {
    rehearsal_due_ms_ = 0;
    rehearsal_index_ = 0;
    return;
  }
  if (rehearsal_due_ms_ != 0 && now_ms < rehearsal_due_ms_) {
    return;
  }
  rehearsal_due_ms_ = now_ms + RehearsalStepMs(look);
  // Sur le personnage, et sur personne d'autre : c'est lui qu'on a sous les
  // yeux quand on règle. Tant que le serveur ne l'a pas posé, rien.
  const std::uint32_t self_id = world.player_id();
  if (self_id == 0 || world.Find(self_id) == nullptr) {
    return;
  }
  Rehearsal shown = RehearsalAt(rehearsal_index_++);
  // L'APERÇU N'A QU'UN ACTEUR : un coup factice que le personnage PORTE part
  // donc de lui, et son « Miss » y naît — là où il naît en jeu.
  if (shown.sides.attacker_is_self) {
    shown.strike.attacker = self_id;
  }
  if (shown.support_skill_id != 0) {
    SpawnSupport(self_id, now_ms, shown.support_skill_id, shown.support_amount, look);
    return;
  }
  Spawn(self_id, now_ms, shown.strike, shown.sides, look);
}

void HitPopups::SpawnSample(DamagePath path, std::uint32_t now_ms, const HitPopupLook& look) {
  Rehearsal shown = PathRehearsal(path);
  // UNE SEULE CIBLE, qui porte aussi le « Miss » de ce qu'elle rate.
  shown.strike.attacker = kSampleCarrierId;
  if (shown.support_skill_id != 0) {
    SpawnSupport(kSampleCarrierId, now_ms, shown.support_skill_id, shown.support_amount, look);
    return;
  }
  Spawn(kSampleCarrierId, now_ms, shown.strike, shown.sides, look);
}

std::span<const HitPopups::SampledNumber> HitPopups::Sampled(std::uint32_t now_ms,
                                                             const HitPopupLook& look,
                                                             const SampleGround& ground) {
  std::erase_if(numbers_,
                [this, now_ms, &look](const Popup& popup) { return !Alive(popup, now_ms, look); });
  OrderByBirth(numbers_);
  sampled_.clear();
  for (Popup& number : numbers_) {
    if (now_ms < number.born_ms) {
      continue;
    }
    const auto elapsed_ms = static_cast<float>(now_ms - number.born_ms);
    const bool drifts = Drifts(number.mark.has_value(), number.path, look);
    if (drifts && !number.drift_chosen) {
      number.drift_direction = BurstDriftInWorld(ground.body_degrees);
      number.drift_chosen = true;
    }
    SampledNumber shown;
    shown.text = ComposeOne(number, elapsed_ms, look);
    shown.frame = &number.frame;
    shown.path = number.path;
    shown.born_ms = number.born_ms;
    if (drifts) {
      const float units =
          DriftUnitsAt(elapsed_ms, number.number.style, PathLifeOf(look, number.path));
      shown.drift_pixels = units * (number.drift_direction[0] * ground.per_world_x +
                                    number.drift_direction[1] * ground.per_world_z);
    }
    sampled_.push_back(shown);
  }
  return sampled_;
}

std::optional<ro::render::NumberText> HitPopups::ComposeOne(Popup& number, float elapsed_ms,
                                                            const HitPopupLook& look) {
  // CE QUI EST PROPRE À CE NOMBRE, plié dans le réglage commun : la trajectoire
  // de son type, la transparence de son nuancier, et son écart au hasard sur le
  // point d'arrivée — il décrit donc son propre arc. Une marque n'a ni opacité
  // ni arrivée à elle : tout passe par là.
  ro::render::NumberLook popup_look =
      OnPath(look.motion, PathOf(look, number.path), PathLifeOf(look, number.path), number.scatter);
  popup_look.opacity *= number.number.opacity;
  // EN POLICE, le cadre ne garde que l'étoile, et le nombre part à l'écran.
  // ⚠ UN MOT RESTE UNE IMAGE : ce n'est pas un chiffre.
  std::optional<ro::render::NumberText> text;
  if (look.font_text && digit_box_.has_value() && !number.mark.has_value()) {
    text = ro::render::ComposeFloatingNumberText(number.number, elapsed_ms, popup_look);
    number.frame = {};
    if (number.star || number.rising_star) {
      number.frame = ro::render::ComposeNumberStar(
          number.number, *mark_sprite_, *mark_animation_,
          number.star ? ro::render::DamageMark::kCritical : ro::render::DamageMark::kRisingStar,
          elapsed_ms, popup_look);
    }
  } else if (number.mark.has_value()) {
    number.frame = ro::render::ComposeDamageMark(*mark_sprite_, *mark_animation_, *number.mark,
                                                 number.number.color, elapsed_ms, popup_look);
  } else if (number.star) {
    number.frame = ro::render::ComposeCriticalNumber(digits_, number.number, *mark_sprite_,
                                                     *mark_animation_, elapsed_ms, popup_look);
  } else if (number.rising_star) {
    number.frame = ro::render::ComposeRisingStarNumber(digits_, number.number, *mark_sprite_,
                                                       *mark_animation_, elapsed_ms, popup_look);
  } else {
    number.frame =
        ro::render::ComposeFloatingNumber(digits_, number.number, elapsed_ms, popup_look);
  }
  return text;
}

std::span<const ro::render::DrawnActor> HitPopups::Drawn(
    std::uint32_t now_ms, const MapStage& stage, const ro::sim::World& world,
    ro::sim::WalkPlanner& planner, const HitPopupLook& look, const NumberScreen& screen) {
  std::erase_if(numbers_,
                [this, now_ms, &look](const Popup& popup) { return !Alive(popup, now_ms, look); });
  // Une cible qu'on a oubliée ne se retient plus : sans cela, la table garderait
  // une entrée par monstre rencontré depuis l'entrée en jeu.
  std::erase_if(tallies_, [now_ms, &look](const auto& kept) {
    return CarriedTotal(kept.second, now_ms, look.total_forget_ms) == 0;
  });
  drawn_.clear();
  texts_.clear();
  // ⚠ ÉTEINT, RIEN NE SE DESSINE MAIS TOUT CONTINUE DE VIVRE : rallumer en
  // pleine rafale montre ce qui reste, et rien ne s'accumule puisque les
  // nombres s'éteignent d'eux-mêmes.
  if (!look.shown) {
    return drawn_;
  }
  // Dans l'ordre de naissance, le plus récent par-dessus : voir `OrderByBirth`.
  OrderByBirth(numbers_);
  for (Popup& number : numbers_) {
    // Pas encore né : le coup n'a pas touché.
    if (now_ms < number.born_ms) {
      continue;
    }
    const bool drifts = Drifts(number.mark.has_value(), number.path, look);
    if (const std::optional<PopupCarrierPlace> carrier =
            PopupCarrierAt(world, planner, number.carrier_id, now_ms, number.kept_unit_cell);
        carrier.has_value()) {
      const std::array<float, 3> ground = GroundPositionAt(stage, carrier->cell.x, carrier->cell.y);
      number.last_ground = glm::vec3(ground[0], ground[1], ground[2]);
      // La diagonale de dérive se choisit à la PREMIÈRE image où le nombre se
      // voit, sur la direction de la cible à cet instant — comme le client, qui
      // la fixe dès qu'elle est encore nulle.
      if (drifts && !number.drift_chosen) {
        number.drift_direction = BurstDriftInWorld(carrier->client_body_degrees);
        number.drift_chosen = true;
      }
    }
    const auto elapsed_ms = static_cast<float>(now_ms - number.born_ms);
    const std::optional<ro::render::NumberText> text = ComposeOne(number, elapsed_ms, look);
    if (number.frame.quads.empty() && !text.has_value()) {
      continue;
    }
    // LA DÉRIVE EST UN DÉPLACEMENT DANS LE MONDE, depuis la position de la cible
    // : c'est là que le client la met, et non dans le repère du sprite. Elle
    // suit la vie du nombre.
    glm::vec3 placed = number.last_ground;
    if (drifts) {
      const float units =
          DriftUnitsAt(elapsed_ms, number.number.style, PathLifeOf(look, number.path));
      placed +=
          glm::vec3(number.drift_direction[0] * units, 0.0F, number.drift_direction[1] * units);
    }
    // Le nombre est posé aux pieds de sa cible, dérive comprise : la hauteur
    // qu'il a prise est déjà dans ses quads.
    if (!number.frame.quads.empty()) {
      drawn_.push_back(ro::render::DrawnActor{&number.frame, nullptr, placed});
    }
    if (text.has_value()) {
      if (const std::optional<ro::ui::DamageTextView> written =
              DamageTextOnScreen(screen, placed, *text, *digit_box_, look.font_outline_pixels);
          written.has_value()) {
        texts_.push_back(*written);
      }
    }
  }
  return drawn_;
}

}  // namespace moonclient
