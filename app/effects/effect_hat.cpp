// LES EFFETS DE CHAPEAU EN JEU : ce qu'un costume, une monture ou une aura
// d'événement fait paraître autour de celui qui le porte.
//
// ⚠ CETTE FAMILLE NE DEMANDE AUCUN DESSIN NOUVEAU, et c'est la mesure qui l'a
// dit : sur les 259 fiches de l'installation, 149 nomment un `.str` et 111 un
// identifiant d'effet ordinaire — les deux familles que nous peignons déjà. Le
// plan l'annonçait comme un neuvième chemin de rendu, sur la foi d'un exemple.
// Voir `ro/render/hat_effect_table.h`.
//
// ⚠ ET LA TABLE VIT DANS LES DONNÉES, comme celle des effets NOMMÉS : deux
// fichiers Lua de l'installation, lus à la première demande. ADR 0027.

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_table.h"
#include "ro/render/hat_effect_anchor.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_actor_change.h"
#include "effects/effect_hat_seat.h"
#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

constexpr std::uint32_t kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);

// DONNE À UN `.str` DE CHAPEAU SA RECETTE D'ANCRE, et l'y pose une première
// fois. Le déplacement se recalcule ensuite à chaque image, dans le plan de la
// vue, autour des pieds que l'effet suit : voir `ReseatHatStr`.
void SeatHatStr(EffectMapMemory::Playing& playing, const ro::render::HatAnchorRecipe& recipe,
                const ro::sim::Actor& bearer, std::uint32_t now_ms, bool before_bodies) {
  playing.hat_anchor = recipe;
  ReseatHatStr(playing, bearer, now_ms);
  playing.behind_actors = playing.behind_actors || before_bodies;
}

using ActorChanges = std::vector<EffectsInPlay::ActorEffectToRecord>;

// LE CHANGEMENT D'ACTEUR QU'UN COSTUME POSE sur son porteur : une fois, puis
// retiré au bout de sa vie propre s'il en a une, sans être reposé tant que le
// costume reste.
void PoseHatActorChange(EffectMapMemory& memory, ActorChanges& queue, std::uint32_t owner_id,
                        std::uint32_t effect_id, std::uint32_t now_ms) {
  const auto key = std::pair{owner_id, effect_id};
  const auto posed = memory.hat_actor_changes.find(key);
  if (posed == memory.hat_actor_changes.end()) {
    memory.hat_actor_changes.emplace(key, now_ms);
    queue.push_back({.actor_id = owner_id, .effect_id = effect_id, .started_ms = now_ms});
    return;
  }
  const std::optional<std::uint64_t> life_ms = OwnLifeMs(effect_id);
  if (posed->second.has_value() && life_ms.has_value() &&
      static_cast<std::uint64_t>(now_ms - *posed->second) >= *life_ms) {
    posed->second.reset();
    queue.push_back(
        {.actor_id = owner_id, .effect_id = effect_id, .started_ms = now_ms, .withdrawn = true});
  }
}

// Et celui qu'ôter le costume retire, s'il est encore là.
void WithdrawHatActorChange(EffectMapMemory& memory, ActorChanges& queue, std::uint32_t owner_id,
                            std::uint32_t effect_id, std::uint32_t now_ms) {
  const auto posed = memory.hat_actor_changes.find({owner_id, effect_id});
  if (posed == memory.hat_actor_changes.end()) {
    return;
  }
  if (posed->second.has_value()) {
    queue.push_back(
        {.actor_id = owner_id, .effect_id = effect_id, .started_ms = now_ms, .withdrawn = true});
  }
  memory.hat_actor_changes.erase(posed);
}

}  // namespace

ro::render::HatEffectBearer HatBearerOf(const ro::sim::Actor& actor, std::uint32_t now_ms) {
  std::vector<ro::render::ActorEffectPlay> played;
  played.reserve(actor.effects.size());
  for (const ro::sim::PlayedEffect& effect : actor.effects) {
    played.push_back(ro::render::ActorEffectPlay{effect.id, (now_ms - effect.since_ms) / kTickMs});
  }
  return {.job = ShownJobOf(actor),
          .shrink_scale = ro::render::BodyShrinkAfterEffects(played),
          .cash_mount = actor.status_effects.contains(ro::sim::kStatusAllRiding)};
}

void ReseatHatStr(EffectMapMemory::Playing& playing, const ro::sim::Actor& bearer,
                  std::uint32_t now_ms) {
  if (!playing.hat_anchor.has_value()) {
    return;
  }
  const ro::render::HatEffectAnchor anchor =
      ro::render::HatAnchorFor(*playing.hat_anchor, HatBearerOf(bearer, now_ms));
  playing.lift_pixels = anchor.lift_pixels;
  playing.view_offset = anchor.view_offset;
  playing.scale = anchor.scale;
}

const ro::render::HatEffect* EffectsInPlay::HatEffectFor(std::uint16_t hat_effect_id) {
  if (!hat_effects_read_) {
    hat_effects_read_ = true;
    auto table = ro::render::HatEffectTable::Load(*vfs_);
    if (!table) {
      ro::core::LogWarning(
          "effets de chapeau : {} — aucune aura de costume ni de monture ne "
          "paraîtra",
          ro::core::Describe(table.error()));
      return nullptr;
    }
    hat_effects_ = std::move(*table);
  }
  if (!hat_effects_.has_value()) {
    return nullptr;
  }
  return hat_effects_->Find(hat_effect_id);
}

// LE FICHIER QU'UNE FICHE MONTRE VRAIMENT, ou vide.
//
// ⚠ UN SEUL ENDROIT EN DÉCIDE, et c'est ce qui évite que la période et le semis
// divergent : la première se tire de la longueur du fichier, le second le joue,
// et les laisser le résoudre chacun de leur côté ferait resemer à un rythme qui
// n'est pas celui de ce qui joue.
//
// Deux chemins, et le second reproduit un DÉFAUT DU CLIENT : une fiche nomme son
// `.str`, ou elle nomme un identifiant d'effet que le client résout — au-delà de
// 2442 — comme l'ORDINAL `identifiant - 2442`, c'est-à-dire sur une AUTRE fiche.
// Voir `HatEffectTable::FindByConcreteId`.
std::string EffectsInPlay::HatFileOf(const ro::render::HatEffect& record) {
  if (!record.file.empty()) {
    return record.file;
  }
  if (record.effect_id != 0 && hat_effects_.has_value()) {
    const ro::render::HatEffect* vu = hat_effects_->FindByConcreteId(record.effect_id);
    if (vu != nullptr) {
      return vu->file;
    }
  }
  return {};
}

// Combien de temps une fiche tient avant d'être resemée, en millisecondes.
//
// ⚠ UN EFFET DE CHAPEAU DURE TANT QUE LE COSTUME EST PORTÉ, et aucune des deux
// familles ne dure aussi longtemps d'elle-même : un `.str` a un nombre d'images
// fini, une troupe procédurale une vie en tics. C'est donc à l'entretien de les
// rejouer, comme pour les effets d'option et les unités posées.
std::uint32_t EffectsInPlay::HatPeriodMs(const ro::render::HatEffect& record) {
  // UNE CLASSE NE SE RESÈME PAS : elle dure d'elle-même tant que son porteur
  // est là, et c'est `NoticeHatEffects` qui la fait naître et la retire.
  if (record.file.empty() && ro::render::IsPortedEffectClass(record.effect_id)) {
    return 0;
  }
  if (const std::string file = HatFileOf(record); !file.empty()) {
    const ro::render::LoadedEffect* effect =
        catalog_->Load(std::string(ro::render::kEffectFolder) + file);
    if (effect != nullptr) {
      return static_cast<std::uint32_t>(effect->prepared.last_frame + 1) *
             static_cast<std::uint32_t>(ro::render::kEffectTickMs);
    }
    // Le fichier est nommé et l'installation ne l'a pas : rien à rejouer, et
    // rien à réessayer à chaque image. Onze fiches sur 259 sont dans ce cas.
    return 0;
  }
  if (record.effect_id != 0 && EffectNodesInPlay::KnowsProcedural(record.effect_id)) {
    return PeriodMs(EffectNodesInPlay::ProceduralLifeTicks(record.effect_id), 0);
  }
  // ⚠ RIEN À MONTRER N'EST PAS UNE PANNE. Soixante-dix des identifiants que ces
  // fiches nomment n'ont ni cas du répartiteur ni fichier, et sont en deçà du
  // seuil de la branche Lua : le client natif ne montre rien non plus.
  return 0;
}

void EffectsInPlay::NoticeHatEffects(const ro::sim::World& world, const MapStage& stage,
                                     ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  // UN PORTEUR PARTI EMPORTE SES SEMIS. Sans cela, le compte d'un costume ancien
  // retiendrait le premier semis de celui qui reprendrait son identifiant.
  std::erase_if(per_map_.sown_hats, [&world](const auto& entry) {
    return !world.actors().contains(entry.first.first);
  });
  std::erase_if(per_map_.hat_actor_changes, [&world](const auto& entry) {
    return !world.actors().contains(entry.first.first);
  });

  std::set<std::pair<std::uint32_t, std::uint32_t>> still_worn;
  for (const auto& [id, actor] : world.actors()) {
    for (const ro::sim::HatEffectId worn : actor.hat_effects) {
      const ro::render::HatEffect* record = HatEffectFor(worn);
      // ⚠ UN NUMÉRO SANS FICHE N'EST PAS UN DÉFAUT : `HatEFID` en nomme 292 et
      // la table n'en décrit que 259. Le client natif ne montre rien non plus.
      if (record == nullptr) {
        continue;
      }
      // UNE CLASSE — un sprite qui boucle sur son porteur, par exemple — NAÎT
      // UNE FOIS et vit tant que le costume est porté : la redemander à chaque
      // période en empilerait des copies. Elle suit son porteur d'elle-même.
      // Une fiche qui nomme un fichier ne la joue que si elle se dit en paire.
      const bool plays_id = record->effect_id != 0 && (record->file.empty() || record->effect_pair);
      if (plays_id) {
        still_worn.emplace(id, record->effect_id);
      }
      if (plays_id && ro::render::IsPortedEffectClass(record->effect_id)) {
        if (!classes_.Carries(record->effect_id, id)) {
          const ro::sim::FractionalCell cell =
              ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
          const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
          static_cast<void>(
              classes_.Spawn({.effect_id = record->effect_id,
                              .owner_id = id,
                              .ground_position = glm::vec3(ground[0], ground[1], ground[2]),
                              .from_hat = true},
                             now_ms));
        }
      }
      // UN EFFET QUI CHANGE L'ACTEUR — rapetisser, blanchir, dédoubler en
      // ombres — se POSE sur le porteur, une fois : c'est son instance que la
      // table des effets d'acteur lit à chaque image. Il s'éteint au bout de sa
      // vie propre, s'il en a une, comme l'instance du client que rien ne
      // recrée tant que le costume reste porté.
      if (plays_id && ChangesOnlyItsBearer(record->effect_id)) {
        PoseHatActorChange(per_map_, actor_effects_to_record_, actor.id, record->effect_id, now_ms);
        continue;
      }
      const std::uint32_t period_ms = HatPeriodMs(*record);
      if (period_ms == 0) {
        continue;
      }
      const std::string file = HatFileOf(*record);
      if (!file.empty()) {
        still_worn.emplace(id, ro::render::kHatEffectIdBase + worn);
      }
      const auto key = std::pair{id, worn};
      const auto sown = per_map_.sown_hats.find(key);
      if (sown != per_map_.sown_hats.end() && now_ms - sown->second < period_ms) {
        continue;
      }
      per_map_.sown_hats.insert_or_assign(key, now_ms);

      const ro::sim::FractionalCell cell =
          ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
      const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
      const glm::vec3 feet(ground[0], ground[1], ground[2]);
      if (!file.empty()) {
        // ⚠ ATTACHÉ À SON PORTEUR, et c'est ce qui distingue cette famille d'un
        // effet posé : un costume suit celui qui le porte, et le laisser à la
        // case où il est né le détacherait au premier pas. Marqué du rang plus
        // 2442, le numéro sous lequel le client le crée : ôter le costume le
        // désigne ainsi.
        if (Play(std::string(ro::render::kEffectFolder) + file, feet, now_ms, id, 0,
                 ro::render::kHatEffectIdBase + worn)) {
          // L'ANCRE DU CHAPEAU : pas de relèvement, un déplacement dans le plan
          // de la vue que la fiche corrige — celle dont le fichier joue.
          const ro::render::HatEffect* shown =
              record->file.empty() ? hat_effects_->FindByConcreteId(record->effect_id) : record;
          const ro::render::HatEffect& read = shown != nullptr ? *shown : *record;
          per_map_.playing.back().from_hat = true;
          SeatHatStr(per_map_.playing.back(), ro::render::HatStrAnchorRecipe(read, file), actor,
                     now_ms, read.render_before_character);
        }
        continue;
      }
      // ⚠ ATTACHÉ AUSSI : sa troupe appartient au porteur, et le suit aux tics
      // où son identifiant le veut — voir `EffectBearerFollow`.
      //
      // ⚠ SANS LE DÉCALAGE DE LA FICHE : un identifiant concret se place comme
      // il se place ailleurs. Seul le lecteur de `.str` prend l'ancre du
      // chapeau, sans rien lire de la fiche.
      //
      // ET TOUT CE QUI EN NAÎT EST MARQUÉ « CHAPEAU », jusqu'aux nœuds que son
      // horloge sèmera : voir `EffectDrawGate::Draws`.
      const std::size_t before = per_map_.playing.size();
      nodes_->SowAsHat(true);
      PlayById(record->effect_id, feet, now_ms, id);
      nodes_->SowAsHat(false);
      for (std::size_t rang = before; rang < per_map_.playing.size(); ++rang) {
        per_map_.playing[rang].from_hat = true;
      }
      if (ro::render::HatIdIsReadAsStr(record->effect_id) && per_map_.playing.size() > before) {
        SeatHatStr(per_map_.playing.back(),
                   ro::render::HatConcreteStrAnchorRecipe(
                       ro::render::EffectFile(record->effect_id, now_ms)),
                   actor, now_ms, false);
      }
    }
  }
  // LE COSTUME ÔTÉ EMPORTE CE QU'IL A FAIT NAÎTRE — sa classe, ses nœuds, son
  // fichier, son changement d'acteur. Un porteur parti emporte les siens de
  // lui-même.
  for (const auto& [owner_id, effect_id] : per_map_.worn_hat_effects) {
    if (!still_worn.contains({owner_id, effect_id})) {
      StopById(effect_id, owner_id);
      WithdrawHatActorChange(per_map_, actor_effects_to_record_, owner_id, effect_id, now_ms);
    }
  }
  per_map_.worn_hat_effects = std::move(still_worn);
}

}  // namespace moonclient
