#include "world/actor_sounds.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/audio/sound_placement.h"
#include "ro/core/log.h"
#include "ro/render/attack_sound.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_sound_table.h"
#include "ro/render/floating_number.h"
#include "ro/render/second_hand.h"
#include "ro/render/skill_damage_timing.h"
#include "ro/render/skill_lua_effect.h"
#include "ro/render/skill_sound_table.h"
#include "ro/render/skill_suite_table.h"
#include "ro/sim/inventory.h"
#include "ro/sim/movement.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/world.h"
#include "ro/vfs/encoding.h"

#include "effects/effect_param.h"
#include "effects/effect_view.h"
#include "world/actor_view.h"
#include "world/hit_popup_rules.h"
#include "world/struck_place.h"

namespace moonclient {
namespace {

// LA CLÉ D'UNE TRACE DÉJÀ DITE RANGE CÔTE À CÔTE TOUT CE QUI A DÉCIDÉ DU BRUIT :
// le métier en bas, l'arme au-dessus, la sorte du coup ensuite, la voie des
// compétences tout en haut. Les champs ne se recouvrent pas — deux décisions
// différentes qui partageraient une clé feraient taire la seconde trace, et
// c'est justement celle qu'on attend.
constexpr unsigned kWeaponShiftInBlowKey = 16;
constexpr std::uint64_t kWeaponMaskInBlowKey = 0xFF'FFFFULL;
constexpr unsigned kDamageTypeShiftInBlowKey = 40;
constexpr std::uint64_t kSkillInBlowKey = 1ULL << 63;

// L'image vue, en un seul nombre : l'action en haut, l'image en bas.
constexpr std::uint64_t kActionShift = 32;

std::uint64_t Witness(const ro::render::ComposedActor& composed) {
  return (static_cast<std::uint64_t>(composed.action_index) << kActionShift) |
         static_cast<std::uint64_t>(composed.frame_index);
}

std::array<float, 3> Coordinates(const glm::vec3& point) { return {point.x, point.y, point.z}; }

// UN PET QUI ATTEND : son animation est celle du repos, au premier rang des
// créatures. Un acteur en fondu de sortie n'a plus d'identifiant, ni de monde.
bool PetIdling(const SoundStage& scene, std::uint32_t id,
               const ro::render::ComposedActor& composed) {
  if (scene.world == nullptr || id == 0) {
    return false;
  }
  const ro::sim::Actor* const who = scene.world->Find(id);
  return who != nullptr && who->familiar == ro::sim::Familiar::kPet &&
         composed.action_index / ro::render::kDirectionCount == ro::render::kAnimationIdle;
}

// UN NOM VENU DU CLIENT, ramene a l'UTF-8 que tout le reste manipule.
//
// ⚠ PAS LA VARIANTE TOLÉRANTE : son point d'interrogation ferait une CLÉ qui
// ne désigne plus rien. Un nom intraduisible garde donc ses octets — il ne se
// trouvera pas davantage, mais il ne se transformera pas en un autre nom.
std::string FromClientBytes(std::string_view cp949) {
  auto utf8 = ro::vfs::Cp949ToUtf8(cp949);
  return utf8 ? std::move(*utf8) : std::string(cp949);
}

// La loi de `PlaceSound` : plein jusqu'à la proximité, puis proximité / distance.
float GainAtDistance(float distance) {
  return distance > ro::audio::kNearDistance ? ro::audio::kNearDistance / distance : 1.0F;
}

// UN BRUIT MONTÉ AU-DESSUS DE L'AUDITEUR : sa hauteur compte dans la distance,
// que `PlaceSound` mesure au sol. Rend ce que le gain au sol doit devenir, en
// facteur : un pour un bruit à terre.
float RaisedGain(const std::array<float, 3>& listener, const std::array<float, 3>& source,
                 std::uint8_t height_units) {
  if (height_units == 0) {
    return 1.0F;
  }
  // AU SOL, l'axe vertical ôté, comme le fait `PlaceSound`.
  const float dx = source[0] - listener[0];
  const float dz = source[2] - listener[2];
  const float au_sol = std::sqrt(dx * dx + dz * dz);
  const float height = static_cast<float>(height_units);
  return GainAtDistance(std::sqrt(au_sol * au_sol + height * height)) / GainAtDistance(au_sol);
}

}  // namespace

Listener ListenerOf(const SoundStage& scene) {
  Listener listener;
  // L'axe DROIT de l'auditeur : c'est lui qui décide de quelle oreille vient un
  // son, et il tourne avec la vue, au degré entier comme chez le client — voir
  // `ClientListenerRight`.
  listener.right = Coordinates(ro::render::ClientListenerRight(*scene.camera));

  const ro::sim::Actor* self = scene.world->player();
  if (self == nullptr) {
    listener.position = Coordinates(scene.camera->look_at);
    return listener;
  }
  // Sa position INTERPOLÉE, la même que celle où son sprite est peint : un
  // auditeur posé sur la case d'arrivée entendrait le monde d'un demi-pas en
  // avant de ce qu'il voit.
  const ro::sim::FractionalCell cell =
      ro::sim::InterpolatedCell(*self, scene.planner->PathFor(*self), scene.now_ms);
  listener.position = GroundPositionAt(*scene.stage, cell.x, cell.y);
  return listener;
}

bool ActorSounds::Known(ro::audio::AudioEngine& audio, const std::string& name) {
  const auto tried = tried_.find(name);
  if (tried != tried_.end()) {
    return tried->second;
  }
  // ⚠ LE NOM ARRIVE EN UTF-8, quelle que soit sa provenance, et c'est la
  // CONDITION de cet appel. Deux sources l'alimentent — nos tables engendrées,
  // qui sont en UTF-8, et les octets CP949 d'un `.act` ou d'une table Lua du
  // client — et `Known` ne peut pas deviner laquelle. C'est donc à la seconde
  // de transcoder avant d'appeler, comme le veut `ro/vfs/encoding.h` :
  // au-dessus de `vfs`, plus rien ne manipule de CP949.
  //
  // Le prendre pour du CP949 marchait sur tout nom ASCII et ne trouvait JAMAIS
  // un nom coréen — la table des sons d'effet en porte cinquante et un.
  auto bytes = vfs_->Read(std::string(ro::render::kSoundFolder) + name);
  bool loaded = false;
  if (!bytes) {
    // ⚠ UN AVERTISSEMENT, ET NON UNE NOTE. On ne passe ici QUE pour un son
    // qu'on s'apprête à jouer : son absence est donc du silence à l'oreille du
    // joueur, jamais un cas ordinaire. Voir la règle des niveaux dans
    // `effects/effect_view.h`.
    ro::core::LogWarning("son : {} absent", name);
  } else if (auto held = audio.LoadSound(name, *bytes); !held) {
    ro::core::LogWarning("son : {} illisible — {}", name, ro::core::Describe(held.error()));
  } else {
    loaded = audio.KnowsSound(name);
  }
  tried_.emplace(name, loaded);
  return loaded;
}

void ActorSounds::Hear(const SoundStage& scene, std::span<const ro::render::DrawnActor> drawn,
                       std::span<const std::uint32_t> ids) {
  if (!scene.audio->is_open()) {
    // Un coup porté pendant que le son est coupé ne sonne pas plus tard.
    if (scene.world != nullptr) {
      static_cast<void>(heard_strikes_.Take(scene.world->strikes()));
    }
    return;
  }
  ForgetDeparted(*scene.world);
  const Listener listener = ListenerOf(scene);
  PlayFrames(scene, listener, drawn, ids);
  PlaySkills(scene, listener);
}

void ActorSounds::ForgetDeparted(const ro::sim::World& world) {
  const auto departed = [&world](const auto& kept) { return !world.actors().contains(kept.first); };
  std::erase_if(seen_, departed);
  std::erase_if(heard_support_, departed);
}

void ActorSounds::PlayFrames(const SoundStage& scene, const Listener& listener,
                             std::span<const ro::render::DrawnActor> drawn,
                             std::span<const std::uint32_t> ids) {
  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    const ro::render::DrawnActor& actor = drawn[index];
    // 🔴 UN ACTEUR QUI S'EFFACE NE SONNE PLUS. Le monde l'a oublié : il se
    // dessine sous l'identifiant zéro, que tous les fondus partagent. Le témoin
    // d'image y passerait d'un acteur à l'autre, et chaque départ rejouerait le
    // son de l'image où il en était — sans que rien ne dise plus s'il est un
    // pet dont le joueur a tu l'attente.
    if (actor.actor == nullptr || ids[index] == 0) {
      continue;
    }
    const std::uint64_t witness = Witness(*actor.actor);
    const auto seen = seen_.find(ids[index]);
    const bool changed = seen == seen_.end() || seen->second != witness;
    seen_.insert_or_assign(ids[index], witness);
    if (!changed || actor.actor->sound.empty()) {
      continue;
    }

    const ro::audio::SoundPlacement placement = ro::audio::PlaceSound(
        listener.position, listener.right, Coordinates(actor.ground_position));
    // Un son qu'on n'entendrait pas ne se charge même pas : à l'entrée sur une
    // carte peuplée, cela évite de lire cent fichiers pour rien.
    if (!(placement.gain > 0.0F)) {
      continue;
    }
    // LE .act DONNE SES OCTETS BRUTS, en CP949 : ils passent en UTF-8 ici,
    // au seul endroit qui sache d'où ils viennent.
    const std::string name = FromClientBytes(actor.actor->sound);
    // CE QUE LE JOUEUR A TU se tait AVANT d'être chargé. ⚠ APRÈS la mise à
    // jour du témoin, plus haut : un son tu ne doit pas éclater à l'image
    // suivante parce qu'on aurait oublié qu'il était passé.
    if (SilencesFile(scene.mutes, name) ||
        (scene.mutes.pet_idle && PetIdling(scene, ids[index], *actor.actor))) {
      continue;
    }
    if (!Known(*scene.audio, name)) {
      continue;
    }
    scene.audio->PlaySound(name, placement.gain, placement.pan, ro::audio::SoundChannel::kSprites);
  }
}

void ActorSounds::PlayBlow(const SoundStage& scene, std::uint32_t target_id,
                           const ro::sim::Strike& strike) {
  // Un coup MANQUÉ ne sonne pas : le client écarte les dégâts nuls avant même
  // d'arriver au choix du son. ⚠ UN MONTANT NÉGATIF, LUI, SONNE : c'est le coup
  // factice qui précède les vrais — le client n'en montre pas le nombre, mais
  // il suit tout le reste du chemin des dégâts.
  if (scene.catalog == nullptr || strike.damage == 0) {
    return;
  }
  // ⚠ ET UNE COMPÉTENCE QUE L'IMPACT NE TRAITE PAS NE SONNE PAS : sans message
  // de dégâts, rien ne choisit de bruit. Voir `StrikeReachesDamageMessages`.
  const StrikeSides sides{.attacker_is_known = scene.world->Find(strike.attacker) != nullptr,
                          .attacker_is_ground_unit = StrikeByGroundUnit(*scene.world, strike)};
  if (!StrikeReachesDamageMessages(strike, sides)) {
    return;
  }
  // 🔴 UNE SOURCE INTROUVABLE NE FAIT PAS TAIRE LE COUP. Le client garde sa
  // classe d'arme à MOINS UN et sonne des quatre bruits sourds —
  // et c'est le cas de TOUTE unité posée au sol, qui ne vit pas dans sa liste
  // d'acteurs. Abandonner ici rendait chaque coup de Storm Gust muet.
  const ro::sim::Actor* attacker = scene.world->Find(strike.attacker);
  const std::int32_t weapon_class =
      attacker != nullptr ? static_cast<std::int32_t>(attacker->look.weapon) : -1;
  // SUR LA CIBLE : c'est là que le coup porte, et c'est de là que le client le
  // fait entendre — une unité au sol frappée à sa case, et comme un monstre.
  const std::optional<ro::sim::FractionalCell> cell =
      StrikeTargetCellAt(*scene.world, *scene.planner, strike, strike.started_ms);
  if (!cell.has_value()) {
    return;
  }
  const ro::sim::Actor* target = scene.world->Find(target_id);
  const std::array<float, 3> ground = GroundPositionAt(*scene.stage, cell->x, cell->y);
  // QUAND LE COUP TOUCHE, et non quand il part : le client fait sonner chaque
  // MESSAGE DE DÉGÂTS, différé du geste et du vol — l'instant même où le
  // chiffre paraît. Un coup à plusieurs frappes sonne autant de fois, au pas
  // des chiffres.
  std::uint32_t lands_ms = strike.started_ms;
  if (scene.wardrobe != nullptr) {
    lands_ms += static_cast<std::uint32_t>(HitLandsAtMs(*scene.wardrobe, *scene.world, strike));
  }
  // ET LA SECONDE MAIN, un message de plus, qui change le pas des autres : voir
  // `ro/render/second_hand.h`.
  const std::uint16_t first_hand = ro::render::ShownHitCount(strike.type, strike.hits);
  const bool second_hand =
      ro::render::ShowsSecondHand(strike.skill_id, strike.type, strike.second_damage,
                                  attacker != nullptr ? attacker->look.job : 0U);
  const auto blows = static_cast<std::uint16_t>(first_hand + (second_hand ? 1U : 0U));
  const auto offset_ms = [&](std::uint16_t blow) -> std::uint32_t {
    if (second_hand) {
      return blow < first_hand ? ro::render::TwoHandedHitOffsetMs(blow, ro::render::kHitStepMs)
                               : ro::render::SecondHandOffsetMs(ro::render::kHitStepMs);
    }
    return static_cast<std::uint32_t>(static_cast<float>(blow) * ro::render::kMultiHitIntervalMs) +
           blow * ro::render::SkillHitStepExtraMs(strike.skill_id);
  };
  for (std::uint16_t blow = 0; blow < blows; ++blow) {
    // Le TIRAGE parmi les quatre bruits d'un même jeu : le client tire au
    // hasard, et l'instant du coup en tient lieu — deux coups du même instant
    // sont le même coup, deux frappes du même coup tirent chacune le leur.
    const std::uint32_t draw = strike.started_ms + blow;
    // ⚠ UNE COMPÉTENCE NE SONNE PAS DE L'ARME : le client demande la classe
    // d'arme MOINS UN pour ses messages de dégâts (96 et 97), ce
    // qui rend l'un des quatre bruits sourds, quoi que tienne le lanceur. Le
    // coup ordinaire garde celui de l'arme — MÊME PROVENANCE QUE LE SON D'UNE
    // IMAGE : ce nom sort d'une table Lua du client, donc en octets CP949.
    // ⚠ ET UN COUP D'UNITÉ N'EST PAS UN COUP DE COMPÉTENCE. Le client range le
    // coup d'une unité posée au sol dans son message 26, qui rejoint le corps
    // des messages de dégâts SANS prendre leur branche « compétence » : il
    // sonne donc comme un coup ordinaire, dont la source est introuvable.
    // 🔴 LE MÉTIER DE LA CIBLE, parce que c'est LUI qui nomme le bruit d'un
    // joueur frappé — `player_clothes.wav` pour presque toutes les classes.
    // Sans lui, un joueur sonnait des quatre bruits sourds d'un monstre.
    const auto target_job =
        target != nullptr ? static_cast<std::int32_t>(target->look.job) : std::int32_t{0};
    const bool on_a_player = target != nullptr && target->kind == ro::sim::ActorKind::kPlayer;
    // 🔴 LA SORTE DU COUP ET SON COMPTE DÉCIDENT, ET NON LE NUMÉRO DE LA
    // COMPÉTENCE. Le client n'écarte le métier de la cible que pour DEUX de ses
    // messages de dégâts — 96 et 97 —, et deux choses disent lequel il
    // emploie : la sorte, puis le CUMUL d'un coup multiple, qui requalifie le
    // message en 111. Un critique de compétence, comme toute compétence à
    // coups multiples, sonne donc de la CLASSE qu'il frappe.
    const bool by_skill = strike.skill_id != 0 && attacker != nullptr &&
                          ro::render::SkillBlowUsesEnemySounds(strike.type, strike.hits);
    const std::string name =
        by_skill ? ro::render::SkillBlowSound(draw)
                 : FromClientBytes(ro::render::AttackSound(*scene.catalog, on_a_player, target_job,
                                                           weapon_class, draw));
    // ⚠ ET CE QUE LA CHAÎNE A CHOISI SE DIT, une fois par couple. « Je n'entends
    // rien » ne distingue pas un nom vide d'un fichier introuvable, et les deux
    // se corrigent à des endroits opposés.
    //
    // 🔴 ELLE NOMME CE QUI A DÉCIDÉ, ET RIEN D'AUTRE — trois voies, trois
    // phrases. Une trace qui annonce « métier 4010 » pour un son choisi par la
    // branche des compétences fait chercher le défaut dans la table des
    // classes ; c'est arrivé, et cela a coûté plusieurs essais au joueur pour
    // un port qui était juste.
    const std::uint64_t couple =
        ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(weapon_class)) &
          kWeaponMaskInBlowKey)
         << kWeaponShiftInBlowKey) |
        (static_cast<std::uint64_t>(strike.type) << kDamageTypeShiftInBlowKey) |
        (by_skill ? kSkillInBlowKey : std::uint64_t{0}) |
        (on_a_player && !by_skill ? static_cast<std::uint64_t>(target_job) : std::uint64_t{0});
    if (reported_blows_.insert(couple).second) {
      // AU DÉTAIL : « ce qui décide → fichier de bruit » est ce qu'on relit en
      // portant le son, pas ce qu'on relit en reconstituant une partie.
      const std::string heard_name = name.empty() ? "(aucun)" : name;
      // ⚠ LA SORTE FIGURE PARTOUT, parce que c'est elle qui aiguille : sans
      // elle, « compétence 14 » et « métier 4010 » se ressemblent trop pour
      // qu'on voie laquelle des cinq voies du client a été prise.
      const auto hit_type = static_cast<int>(strike.type);
      if (by_skill) {
        ro::core::LogDebug("coup : COMPÉTENCE {} sorte {} — le métier ne décide pas → bruit « {} »",
                           strike.skill_id, hit_type, heard_name);
      } else if (on_a_player) {
        ro::core::LogDebug("coup : métier {} d'un joueur, sorte {} → bruit « {} »", target_job,
                           hit_type, heard_name);
      } else {
        ro::core::LogDebug("coup : arme {} sur autre chose, sorte {} → bruit « {} »", weapon_class,
                           hit_type, heard_name);
      }
    }
    if (name.empty()) {
      continue;
    }
    // L'IMPACT SONNE AVEC LES SPRITES : il accompagne chaque coup automatique,
    // et baisser les effets pour alléger une bataille doit le laisser entendre.
    pending_sounds_.push_back(
        {name, ground, lands_ms + offset_ms(blow), ro::audio::SoundChannel::kSprites});
  }
}

void ActorSounds::PlaySkills(const SoundStage& scene, const Listener& listener) {
  for (const auto& [id, actor] : scene.world->actors()) {
    // Une compétence SANS DÉGÂTS s'entend aussi : le soin, le renfort, l'état
    // posé. Elle n'arrive par aucun paquet de coup, et rester sur les seuls
    // coups la rend muette.
    if (actor.support.has_value()) {
      const ro::sim::SupportHit& given = *actor.support;
      const auto heard = heard_support_.find(id);
      if (heard == heard_support_.end() || heard->second != given.started_ms) {
        heard_support_.insert_or_assign(id, given.started_ms);
        const ro::sim::FractionalCell cell =
            ro::sim::InterpolatedCell(actor, scene.planner->PathFor(actor), given.started_ms);
        PlaySkillSound(scene, listener, id, GroundPositionAt(*scene.stage, cell.x, cell.y),
                       scene.world->Find(given.source_id), given.skill_id, given.started_ms,
                       /*on_hit=*/false, given.amount);
      }
    }
  }
  for (const ro::sim::RecordedStrike& taken : heard_strikes_.Take(scene.world->strikes())) {
    const ro::sim::Strike& strike = taken.strike;
    // UNE UNITÉ AU SOL FRAPPÉE SONNE COMME UN MONSTRE, à sa case : voir
    // `world/struck_place.h`.
    const std::optional<ro::sim::FractionalCell> struck_cell =
        StrikeTargetCellAt(*scene.world, *scene.planner, strike, strike.started_ms);
    if (!struck_cell.has_value()) {
      continue;
    }
    PlayBlow(scene, strike.target, strike);

    // 🔴 UNE UNITÉ AU SOL SONNE À SA POSE, JAMAIS À SES COUPS. Le client range
    // le bruit d'une unité dans le message de son APPARITION (cas 2) ; le message du coup, lui, ne
    // joue aucun son. Sans cette porte, le vent de Storm Gust repartirait de zéro à chaque
    // battement de chacune de ses unités.
    if (StrikeByGroundUnit(*scene.world, strike)) {
      continue;
    }
    // ⚠ NI SANS LANCEUR CONNU : l'impact est un message que le client envoie à
    // l'acteur du lanceur, et il n'a personne à qui l'envoyer.
    if (strike.skill_id != 0 && scene.world->Find(strike.attacker) == nullptr) {
      continue;
    }
    PlaySkillSound(scene, listener, strike.target,
                   GroundPositionAt(*scene.stage, struck_cell->x, struck_cell->y),
                   scene.world->Find(strike.attacker), strike.skill_id, strike.started_ms,
                   /*on_hit=*/true, EffectParamOfStrike(strike), strike.type);
  }
}

void ActorSounds::PlaySkillSound(const SoundStage& scene, const Listener& listener,
                                 std::uint32_t target_id, const std::array<float, 3>& target_ground,
                                 const ro::sim::Actor* caster, std::uint16_t skill_id,
                                 std::uint32_t started_ms, bool on_hit, std::int32_t argument,
                                 std::uint8_t damage_type) {
  // OÙ UN SON SE PLACE : sur un acteur, à la case où il est à l'instant du coup.
  const auto ground_of = [&](const ro::sim::Actor& sounding_actor) {
    const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(
        sounding_actor, scene.planner->PathFor(sounding_actor), started_ms);
    return GroundPositionAt(*scene.stage, cell.x, cell.y);
  };
  // UN SON, À SA PLACE ET À SON TIC.
  //
  // 🔴 À SON TIC, et non à la naissance de la compétence. Le client compte les
  // tics de l'instance, seize millisecondes chacun, et un effet sonne souvent
  // PLUSIEURS fois — Provoke deux fois, aux tics 5 et 30. ⚠ LE TIC SE COMPTE
  // DEPUIS LE COUP, non depuis l'image courante : un paquet traité en retard doit
  // sonner en retard d'autant, pas repartir de zéro.
  const auto play = [&](std::string_view sound, const std::array<float, 3>& ground,
                        std::uint16_t delay_ticks) {
    std::string name(sound);
    if (delay_ticks > 0) {
      pending_sounds_.push_back(
          {std::move(name), ground,
           started_ms + static_cast<std::uint32_t>(delay_ticks) *
                            static_cast<std::uint32_t>(ro::render::kEffectTickMs),
           ro::audio::SoundChannel::kEffects});
      return;
    }
    const ro::audio::SoundPlacement placement =
        ro::audio::PlaceSound(listener.position, listener.right, ground);
    if (placement.gain > 0.0F && Known(*scene.audio, name)) {
      scene.audio->PlaySound(name, placement.gain, placement.pan,
                             ro::audio::SoundChannel::kEffects);
    }
  };
  // 🔴 LE SON DU LUA D'ABORD, et il REMPLACE celui des tables du client :
  // aucune des soixante-six compétences de `skilleffectinfolist.lub` ne figure
  // dans sa table des sons de compétence, et le client joue le sien dans le
  // chemin même qui lit ce Lua. Deux sons, un par chemin — voir `on_hit`.
  const ro::render::SkillLuaSoundEntry from_lua = ro::render::SkillLuaSoundFor(skill_id);
  const std::string_view own_lua_wave = on_hit ? from_lua.target_wave : from_lua.caster_wave;
  if (!own_lua_wave.empty()) {
    play(own_lua_wave, target_ground, 0);
    return;
  }
  // PUIS LA SUITE DE LA COMPÉTENCE : chacun de ses sons, à la place que le
  // client lui donne.
  bool later = false;
  for (const ro::render::SkillSuiteStep& step :
       ro::render::SkillSuiteOf(skill_id, on_hit ? ro::render::SkillSuiteMoment::kImpact
                                                 : ro::render::SkillSuiteMoment::kAnnounced)) {
    if (step.kind != ro::render::SkillSuiteKind::kSound ||
        !ro::render::SkillSuiteStepPlays(step, argument) ||
        !ro::render::SkillSuiteStepPlaysForCaster(step,
                                                  caster != nullptr && scene.world != nullptr &&
                                                      caster->id == scene.world->player_id()) ||
        !ro::render::SkillSuiteStepFitsDamageType(step, damage_type) ||
        !ro::render::SkillSuiteStepShows(
            step, scene.effects != nullptr && scene.effects->MinimalEffects()) ||
        !SkillSuiteConditionHolds(scene.effects, step, caster != nullptr ? caster->id : 0,
                                  target_id)) {
      continue;
    }
    later = true;
    std::array<float, 3> ground =
        step.carrier == ro::render::SkillSuiteCarrier::kCaster && caster != nullptr
            ? ground_of(*caster)
            : target_ground;
    // SUR L'AUDITEUR : plein et centré, quel que soit le lanceur.
    if (step.carrier == ro::render::SkillSuiteCarrier::kLocalPlayer && scene.world != nullptr) {
      if (const ro::sim::Actor* self = scene.world->Find(scene.world->player_id());
          self != nullptr) {
        ground = ground_of(*self);
      }
    }
    // DANS LE MONDE : le décalage relevé est dans le repère du client, dont la
    // profondeur est l'opposée de la nôtre.
    if (step.carrier == ro::render::SkillSuiteCarrier::kWorldNearTarget) {
      ground[0] += step.offset_x;
      ground[2] -= step.offset_z;
    }
    play(step.sound, ground, step.delay_ticks);
  }
  if (later) {
    return;
  }
  // À DÉFAUT, LES SONS DE L'EFFET QUE LA COMPÉTENCE MONTRE, sur la cible — et
  // seulement si CE chemin en montre un : voir `ro::render::SkillSoundsOnPath`.
  std::uint32_t rank = 0;
  for (const ro::render::EffectSoundEntry& entry :
       ro::render::SkillSoundsOnPath(skill_id, on_hit)) {
    // UN NOM À VARIANTES TIRE À CHAQUE BRUIT : voir `EffectSoundFile`.
    play(ro::render::EffectSoundFile(entry, started_ms + rank++), target_ground, entry.delay_ticks);
  }
}

void ActorSounds::PlayOnCell(const SoundStage& scene, const Listener& listener,
                             std::string_view sound, std::uint32_t one_in,
                             const ro::sim::CellPosition& cell) {
  // LE TIRAGE AU SORT, quand le client en fait un : le son ne s'entend qu'une
  // fois sur autant, tiré à chaque appel. Voir `ro::render::SoundChance`.
  if (!chance_.Heard(one_in)) {
    return;
  }
  const std::array<float, 3> ground =
      GroundPositionAt(*scene.stage, static_cast<float>(cell.x), static_cast<float>(cell.y));
  const ro::audio::SoundPlacement placement =
      ro::audio::PlaceSound(listener.position, listener.right, ground);
  if (!(placement.gain > 0.0F)) {
    return;
  }
  const std::string name(sound);
  if (!Known(*scene.audio, name)) {
    return;
  }
  scene.audio->PlaySound(name, placement.gain, placement.pan, ro::audio::SoundChannel::kEffects);
}

void ActorSounds::HearEffects(const SoundStage& scene, std::span<const LaunchedEffect> launched) {
  if (scene.audio == nullptr) {
    return;
  }
  const Listener listener = ListenerOf(scene);
  // LES SONS QUI ATTENDAIENT LEUR TIC passent d'abord : ils sont dus quel que
  // soit ce qui vient d'être lancé. Voir `PendingSound`.
  std::erase_if(pending_sounds_, [&](const PendingSound& pending) {
    if (scene.now_ms < pending.due_ms) {
      return false;
    }
    // UN BRUIT QUI SUIT SON PORTEUR sonne là où celui-ci se tient à cet
    // instant ; un porteur parti laisse le point où il était.
    std::array<float, 3> ground = pending.ground;
    const bool follows = pending.carrier_id != 0 && scene.world != nullptr &&
                         scene.planner != nullptr && scene.stage != nullptr;
    if (const ro::sim::Actor* carrier = follows ? scene.world->Find(pending.carrier_id) : nullptr) {
      const ro::sim::FractionalCell cell =
          ro::sim::InterpolatedCell(*carrier, scene.planner->PathFor(*carrier), scene.now_ms);
      ground = GroundPositionAt(*scene.stage, cell.x, cell.y);
    }
    if (pending.at_listener) {
      ground = listener.position;
    }
    ro::audio::SoundPlacement placement =
        ro::audio::PlaceSound(listener.position, listener.right, ground);
    placement.gain *= RaisedGain(listener.position, ground, pending.listener_height);
    if (placement.gain > 0.0F && Known(*scene.audio, pending.sound)) {
      scene.audio->PlaySound(pending.sound, placement.gain, placement.pan, pending.channel);
    }
    return true;
  });
  // LES BOURDONS ENSUITE, et pour la même raison : ils sont dus quoi qu'il
  // arrive. Chacun se repose la question à chaque image — c'est ce qui le fait
  // TAIRE quand plus personne ne resème son effet.
  for (HummingEffect& hum : humming_) {
    if (scene.now_ms < hum.next_due_ms || scene.now_ms >= hum.until_ms) {
      continue;
    }
    const ro::audio::SoundPlacement placement =
        ro::audio::PlaceSound(listener.position, listener.right, hum.ground);
    if (placement.gain > 0.0F && Known(*scene.audio, hum.sound)) {
      scene.audio->PlaySound(hum.sound, placement.gain, placement.pan,
                             ro::audio::SoundChannel::kEffects);
    }
    // ⚠ AVANCER D'UN PAS, ET NON DEPUIS MAINTENANT : une image longue décalerait
    // le carillon d'autant, et les décalages s'accumuleraient. Mais un retard
    // de plus d'une période — une image qui a duré, un client au ralenti — ne
    // se rattrape pas en jouant dix fois de suite : on se recale.
    hum.next_due_ms += hum.period_ms;
    if (hum.next_due_ms <= scene.now_ms) {
      hum.next_due_ms = scene.now_ms + hum.period_ms;
    }
  }
  std::erase_if(humming_,
                [&scene](const HummingEffect& hum) { return scene.now_ms >= hum.until_ms; });
  for (const LaunchedEffect& effect : launched) {
    // UN EFFET MUET EST UN CAS, pas un manque : la table ne porte que ceux dont
    // le client déclenche un son, et la plupart n'en font aucun. Et un effet
    // peut en jouer PLUSIEURS d'un coup — voir `EffectSounds`.
    const std::span<const ro::render::EffectSoundEntry> files =
        ro::render::EffectSounds(effect.effect_id);
    if (files.empty()) {
      continue;
    }
    // ⚠ SAUF QUAND C'EST LA TROUPE QUI SONNE : le bâtisseur de Soul Strike joue
    // son son À CHAQUE BOULE, dans la garde du semis, et rien au départ. Le
    // jouer ici en plus ferait entendre six coups pour cinq. Voir
    // `HearNodeSounds`.
    if (ro::render::EffectSoundPlaysAtSowing(effect.effect_id)) {
      continue;
    }
    const ro::audio::SoundPlacement effect_placement =
        ro::audio::PlaceSound(listener.position, listener.right, effect.ground);
    // UN PORTEUR ORDINAIRE : présent, sans état du corps ni option.
    const ro::sim::Actor* bearer = effect.bearer_id != 0 && scene.world != nullptr
                                       ? scene.world->Find(effect.bearer_id)
                                       : nullptr;
    const bool plain_bearer = bearer != nullptr && bearer->body_state == 0 && bearer->option == 0;
    std::uint32_t rank = 0;
    for (const ro::render::EffectSoundEntry& entry : files) {
      // Sur un porteur ordinaire seulement, quand le bruit l'exige. Et UNE FOIS
      // SUR AUTANT, tiré pour CE lancement : vingt-cinq cellules d'un bourbier
      // nées dans la même image tirent chacune. Voir `EffectSoundEntry::one_in`.
      if ((entry.needs_plain_bearer && !plain_bearer) || !chance_.Heard(entry.one_in)) {
        ++rank;
        continue;
      }
      // SUR L'AUDITEUR, le bruit sonne plein où que soit l'effet : voir
      // `EffectSoundEntry::at_listener`.
      ro::audio::SoundPlacement placement =
          entry.at_listener
              ? ro::audio::PlaceSound(listener.position, listener.right, listener.position)
              : effect_placement;
      placement.gain *=
          RaisedGain(listener.position, entry.at_listener ? listener.position : effect.ground,
                     entry.listener_height);
      if (!(placement.gain > 0.0F)) {
        continue;
      }
      // UN NOM À VARIANTES TIRE À CHAQUE BRUIT : les quatre des pétales ne
      // sonnent pas forcément pareil. Voir `EffectSoundFile`.
      std::string name = ro::render::EffectSoundFile(entry, scene.now_ms + rank++);
      // UN BOURDON SE POSE, IL NE SE PROGRAMME PAS : le client rejoue l'appel
      // tant que l'instance vit, et le portail prêt carillonne ainsi toutes les
      // deux cent vingt-quatre millisecondes. Un semis de plus sur le même point
      // ne l'empile pas, il en repousse l'échéance — c'est ce qui l'entretient,
      // et c'est ce qui l'éteint quand l'unité s'en va.
      if (entry.period_ticks > 0) {
        Hum(effect, entry, scene.now_ms);
        continue;
      }
      // À SON TIC, et non au départ : le client compte les tics de l'instance,
      // seize millisecondes chacun. Le son attend, et sonnera d'où l'on sera.
      if (entry.delay_ticks > 0) {
        pending_sounds_.push_back(
            {std::move(name), effect.ground,
             scene.now_ms + static_cast<std::uint32_t>(entry.delay_ticks) *
                                static_cast<std::uint32_t>(ro::render::kEffectTickMs),
             ro::audio::SoundChannel::kEffects, 0, entry.at_listener, entry.listener_height});
        continue;
      }
      if (Known(*scene.audio, name)) {
        scene.audio->PlaySound(name, placement.gain, placement.pan,
                               ro::audio::SoundChannel::kEffects);
      }
    }
  }
}

void ActorSounds::Hum(const LaunchedEffect& effect, const ro::render::EffectSoundEntry& entry,
                      std::uint32_t now_ms) {
  // ⚠ L'ÉCHÉANCE EST LA VIE DE L'EFFET, pas celle de l'unité — nous ne savons
  // rien d'elle ici. Et c'est suffisant : une unité au sol resème son effet plus
  // souvent qu'il ne dure, donc chaque semis repousse l'échéance et le silence
  // tombe peu après le dernier. Une durée inconnue rend ZÉRO, et un bourdon qui
  // expire à l'instant où il naît ne sonne pas : l'essai tient l'invariant
  // qu'aucun bruit périodique n'est dans ce cas.
  const std::int32_t life_ticks = ro::render::EffectDurationTicks(effect.effect_id);
  if (life_ticks <= 0) {
    return;
  }
  const auto to_ms = [](std::int32_t ticks) {
    return static_cast<std::uint32_t>(ticks) *
           static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  };
  const std::uint32_t until_ms = now_ms + to_ms(life_ticks);
  const auto same = [&effect, &entry](const HummingEffect& hum) {
    return hum.effect_id == effect.effect_id && hum.ground == effect.ground &&
           hum.sound == entry.sound;
  };
  if (const auto found = std::ranges::find_if(humming_, same); found != humming_.end()) {
    // ⚠ ON NE REMET PAS L'ÉCHÉANCE DU PROCHAIN COUP. Le client fait naître une
    // instance neuve qui carillonne depuis son propre tic zéro, mais les deux se
    // chevauchent : recaler à chaque semis ferait battre le bourdon au rythme de
    // la RELANCE, treize fois trop lent, et c'est exactement le défaut qu'on
    // corrige.
    found->until_ms = until_ms;
    return;
  }
  humming_.push_back({effect.effect_id, std::string(entry.sound), effect.ground,
                      to_ms(entry.period_ticks), now_ms + to_ms(entry.delay_ticks), until_ms});
}

void ActorSounds::HearSkillUnits(const SoundStage& scene, std::span<const std::uint32_t> sown) {
  const Listener listener = ListenerOf(scene);

  // UNE UNITÉ RETIRÉE S'OUBLIE : sans quoi celle qui reprendra son identifiant
  // arriverait en silence.
  std::erase_if(placed_units_, [&scene](const auto& kept) {
    return !scene.world->skill_units().contains(kept.first);
  });

  for (const auto& [id, unit] : scene.world->skill_units()) {
    if (unit.unit_id == ro::sim::kSilentSkillUnit) {
      continue;
    }
    const auto heard = placed_units_.find(id);
    if (heard != placed_units_.end() && heard->second == unit.placed_ms) {
      continue;
    }
    placed_units_.insert_or_assign(id, unit.placed_ms);
    if (const ro::render::SkillUnitSoundEntry* entry =
            ro::render::SkillUnitPlacementSound(unit.unit_id);
        entry != nullptr) {
      PlayOnCell(scene, listener, entry->sound, entry->one_in, unit.cell);
    }
  }

  // Et le son de l'ENTRETIEN, sur les seules unités dont l'effet vient d'être
  // resemé : le client les joue ensemble, et un son qui battrait à part se
  // décalerait de l'image en quelques secondes.
  for (const std::uint32_t id : sown) {
    const auto found = scene.world->skill_units().find(id);
    if (found == scene.world->skill_units().end()) {
      continue;
    }
    if (const ro::render::SkillUnitSoundEntry* entry =
            ro::render::SkillUnitSustainSound(found->second.unit_id);
        entry != nullptr) {
      PlayOnCell(scene, listener, entry->sound, entry->one_in, found->second.cell);
    }
  }
}

void ActorSounds::HearNamedEffects(const SoundStage& scene, std::span<const SownNamedSound> sown) {
  if (sown.empty() || scene.audio == nullptr) {
    return;
  }
  const Listener listener = ListenerOf(scene);
  std::uint32_t rank = 0;
  for (const SownNamedSound& heard : sown) {
    // UN NOM À VARIANTES TIRE À CHAQUE BRUIT : les coups empilés d'une même
    // méthode ne sonnent pas forcément pareil.
    ro::render::EffectSoundEntry pattern;
    pattern.sound = heard.sound;
    pattern.variants = heard.variants;
    std::string name = ro::render::EffectSoundFile(pattern, scene.now_ms + rank++);
    // UN POINT DU MONDE, à sa portée et à son volume : voir `SownNamedSound::point`.
    if (heard.point.has_value()) {
      ro::audio::SoundPlacement placement =
          ro::audio::PlaceSound(listener.position, listener.right, *heard.point,
                                heard.hearing_range, heard.near_distance);
      placement.gain *= heard.volume;
      if (placement.gain > 0.0F && Known(*scene.audio, name)) {
        scene.audio->PlaySound(name, placement.gain, placement.pan,
                               ro::audio::SoundChannel::kEffects);
      }
      continue;
    }
    // UN BRUIT EMPILÉ ATTEND SON TIC, et suivra son porteur jusque-là.
    if (heard.due_ms > scene.now_ms && scene.stage != nullptr) {
      pending_sounds_.push_back(
          {std::move(name), GroundPositionAt(*scene.stage, heard.cell.x, heard.cell.y),
           heard.due_ms, ro::audio::SoundChannel::kEffects, heard.carrier_id});
      continue;
    }
    // Aucun tirage : ces fiches n'en portent pas, contrairement aux sons
    // d'unité dont un sur huit s'entend.
    constexpr std::uint32_t kAlways = 1;
    PlayOnCell(scene, listener, name, kAlways, heard.cell);
  }
}

void ActorSounds::HearNodeSounds(const SoundStage& scene,
                                 std::span<const ro::render::EffectNodeSound> sounds) {
  if (sounds.empty() || scene.audio == nullptr) {
    return;
  }
  const Listener listener = ListenerOf(scene);
  for (const ro::render::EffectNodeSound& bounce : sounds) {
    // ⚠ LE POINT EST DÉJÀ CELUI DU MONDE, et non une case : c'est là que la
    // particule a touché, et deux pièces d'une même gerbe ne touchent pas au
    // même endroit. Les ramener à la case de l'effet leur donnerait à toutes
    // le même placement, ce qui s'entend comme un seul son plus fort.
    const std::array<float, 3> point = {bounce.position.x, bounce.position.y, bounce.position.z};
    const ro::audio::SoundPlacement placement =
        ro::audio::PlaceSound(listener.position, listener.right, point);
    if (!(placement.gain > 0.0F)) {
      continue;
    }
    if (!Known(*scene.audio, bounce.sound)) {
      continue;
    }
    scene.audio->PlaySound(bounce.sound, placement.gain, placement.pan,
                           ro::audio::SoundChannel::kEffects);
  }
}

}  // namespace moonclient
