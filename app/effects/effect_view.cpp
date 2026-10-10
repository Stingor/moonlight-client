#include "effects/effect_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/auto_counter.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_concentric_square_seed.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_table.h"
#include "ro/render/effect_visibility.h"
#include "ro/render/floating_number.h"
#include "ro/render/map_effect_table.h"
#include "ro/render/option_effect_table.h"
#include "ro/render/pvp_rank_tint.h"
#include "ro/render/skill_cast_extra.h"
#include "ro/render/skill_effect_table.h"
#include "ro/render/skill_hit_effect.h"
#include "ro/render/skill_lua_effect.h"
#include "ro/render/sprite_billboard.h"
#include "ro/render/str_effect_setup.h"
#include "ro/sim/movement.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"
#include "ro/sim/world_bearers.h"

#include "effects/effect_node_view.h"
#include "effects/effect_str_look.h"
#include "world/actor_view.h"
#include "world/hit_popup_rules.h"

namespace moonclient {
namespace {

// L'ANGLE DE BASE QUE PORTE UN ACTEUR, pour les effets qui naissent sur lui.
//
// ⚠ LE CLIENT NE LIT QU'UN DE SES DEUX ANGLES. L'instance d'effet garde un
// pointeur vers son porteur et relit son angle à chaque semis : c'est
// l'angle du CORPS, celui qu'écrivent le tour et la marche. Le regard de la
// tête, qui ne dépasse jamais un huitième de tour, n'y entre pas — un
// personnage qui suit quelqu'un des yeux ne fait pas pivoter sa gerbe.
// ⚠ ET C'EST LA DIRECTION DESSINÉE, non celle qu'il a annoncée. Le client
// réécrit l'angle du corps à chaque pas d'une marche : un effet qui naît au milieu d'une
// course s'ouvre devant le coureur, pas derrière. C'est le chemin qui sépare
// les deux, et c'est pourquoi on le demande.
float OwnerFacingDegrees(const ro::sim::Actor& actor, const ro::sim::WalkPath& path,
                         std::uint32_t at_ms) {
  return ro::render::EffectOwnerFacingDegrees(ro::sim::FacingAt(actor, path, at_ms));
}

}  // namespace

void EffectsInPlay::Notice(const ro::sim::World& world, const MapStage& stage,
                           ro::sim::WalkPlanner& planner) {
  for (const auto& [id, actor] : world.actors()) {
    for (const ro::sim::PlayedEffect& played : actor.effects) {
      const auto key = std::pair{id, played.id};
      const auto seen = per_map_.noticed.find(key);
      if (seen != per_map_.noticed.end() && seen->second == played.since_ms) {
        continue;
      }
      per_map_.noticed[key] = played.since_ms;

      // LA GARDE DU CHEVALIER S'ARME ICI, et rien ne se montre encore : le
      // client met l'acteur en posture et lève un drapeau que le coup SUIVANT
      // dépensera. Voir `ro/render/auto_counter.h` — c'est le seul effet de
      // notre chantier qui ne dépende ni d'une compétence ni d'une cible.
      if (played.id == ro::render::kAutoCounterGuardEffect) {
        per_map_.auto_counter_armed.insert_or_assign(id, played.since_ms);
      }

      // Le tirage vient de l'instant : un effet à plusieurs fichiers n'en
      // montre pas toujours le même, et deux lancers rapprochés ne se
      // ressemblent pas.
      const std::string file = AskedEffectFile(played.id, played.since_ms);
      const ro::sim::FractionalCell cell =
          ro::sim::InterpolatedCell(actor, planner.PathFor(actor), played.since_ms);
      const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
      // 🔴 UNE CLASSE INSCRITE AU GESTIONNAIRE D'EFFETS PASSE DEVANT LE
      // PRIMITIF. Pour un effet annoncé sur un acteur, le client demande d'abord
      // la classe que son gestionnaire tient pour ce numéro, et ne sème son
      // fichier ou ses nœuds que s'il n'en a aucune. La fumée 44 en est une : la
      // cheminée des cartes, posée sur l'acteur, qui le SUIT, fume sans fin et
      // meurt quand il n'est plus là — sans bruit, la classe n'en jouant aucun.
      //
      // ⚠ C'EST LA SEULE DE CES CLASSES QUE CE CHEMIN PORTE : les autres numéros
      // inscrits jouent encore leur fichier ou leurs nœuds.
      if (played.id == static_cast<std::uint32_t>(ro::render::kMapChimneySmokeEffect)) {
        per_map_.chimneys.emplace_back(glm::vec3(ground[0], ground[1], ground[2]), id);
        continue;
      }
      // LES SPHÈRES DES QUATRIÈMES MÉTIERS ET LES SPHÈRES ÉLÉMENTAIRES naissent
      // par leur classe, qui tient son nombre de la valeur ; chaque nouvelle
      // valeur le lui redit.
      if (ro::render::EffectClassCountsFromValue(played.id) || played.born_of_class) {
        static_cast<void>(
            classes_.Spawn({.effect_id = played.id,
                            .owner_id = id,
                            .ground_position = glm::vec3(ground[0], ground[1], ground[2]),
                            .level = played.value.value_or(0)},
                           played.since_ms));
        continue;
      }
      QueueUpdateQuakes(per_map_.pending_quakes, played.id, played.since_ms);
      // 🔴 UN FICHIER N'EXCLUT PAS LES NŒUDS. EF_GLASSWALL joue SafetyWall.str
      // ET dresse quatre murs bleus : son bâtisseur fait défiler le fichier
      // lui-même, puis bâtit ses nœuds. Ne jouer que le fichier laissait la
      // boîte absente, et rien ne le disait — l'effet montrait quelque chose.
      // Deux effets sont dans ce cas, et leurs deux bâtisseurs font les deux.
      //
      // Et pas de fichier ne veut pas dire pas d'image : c'est par ici que
      // passent les impacts, qu'aucune compétence ne nomme.
      const bool played_by_nodes = EffectNodesInPlay::KnowsProcedural(played.id);
      if (played_by_nodes) {
        // ⚠ ATTACHÉ À CELUI QUI LE PORTE, et c'est ce qui manquait. Le serveur
        // annonce ces effets-là SUR UN ACTEUR ; l'effet lui appartient, meurt
        // avec lui, et SUIT sa place quand son identifiant le veut — l'anneau du
        // classement PVP, qui vit dix mille tics, restait planté là où le joueur
        // se tenait. ⚠ Mais pas tous : un impact ou une bouffée de poison
        // restent où ils sont nés, accrochés quand même. Voir
        // `EffectBearerFollow`.
        //
        // ⚠ ET LA VALEUR DE L'EFFET N'EST SON PARAMÈTRE QUE POUR CEUX QUI EN
        // LISENT UN. Le client range le nombre de coups et le rang du classement
        // dans le MÊME champ d'instance ; une valeur de soin — le paquet 0x0284
        // en porte de plusieurs centaines — versée là ferait semer des centaines
        // de nœuds aux patrons qui suivent le paramètre.
        // 🔴 ET TOUTE GERBE DE SPHÈRES Y LIT SON NOMBRE : celles du Monk, de
        // l'enfant et les pièces comme les quatre éléments.
        const bool reads_value = ro::render::EffectReadsValueAsParam(played.id) ||
                                 !ro::render::EffectConcentricSquarePatternsFor(played.id).empty();
        const std::int32_t param =
            reads_value ? played.value.value_or(kRawEffectParam) : kRawEffectParam;
        nodes_->PlayProcedural(played.id, glm::vec3(ground[0], ground[1], ground[2]), std::nullopt,
                               OwnerFacingDegrees(actor, planner.PathFor(actor), played.since_ms),
                               id, param);
      }
      if (file.empty()) {
        // UN EFFET QUI NE FAIT QUE CHANGER SON PORTEUR n'a rien à peindre, mais son
        // instance SONNE — Shrink au cinquième tic — quand le serveur l'annonce.
        // Celles que nous inscrivons nous-mêmes se sont fait entendre en naissant
        // et sont marquées vues : une seule demande de bruit par instance.
        if (played_by_nodes || ro::render::ActorEffectChangeFor(played.id) != nullptr) {
          NoteLaunched(played.id, ground, id);
        } else if (std::ranges::find(reported_, played.id) == reported_.end()) {
          reported_.push_back(played.id);
          // ⚠ ELLE PARTAIT SUR `stderr`, donc hors du journal : ni filtrable par
          // niveau, ni relisible une fois la partie finie.
          ro::core::LogWarning(
              "effet {} : ni fichier ni patron, semis procédural sans figure — rien ne se verra",
              played.id);
        }
        continue;
      }
      const ro::render::LoadedEffect* effect =
          catalog_->Load(std::string(ro::render::kEffectFolder) + file);
      if (effect == nullptr || StrAlreadyOnOwner(per_map_.playing, played.id, id)) {
        if (effect == nullptr && played_by_nodes) {
          NoteLaunched(played.id, ground, id);
        }
        continue;
      }
      // Marqué de son identifiant : un retrait le désigne ainsi.
      Playing announced{effect, id, glm::vec3(0.0F), played.since_ms};
      announced.from_effect_id = played.id;
      per_map_.playing.push_back(announced);
      QueueQuakes(per_map_.pending_quakes,
                  ShapeStrPlaying(per_map_.playing.back(), played.id, id, std::nullopt));
      // LES DEUX FAMILLES SONNENT PAR LE MÊME CHEMIN : le bruit est rangé sous
      // l'identifiant de l'effet, et celui-ci ne dit pas s'il a un fichier.
      NoteLaunched(played.id, ground, id);
    }
  }
}

// LES CHEMINÉES QUE LES EFFETS ANNONCÉS ONT DEMANDÉES depuis le dernier passage,
// chacune avec l'acteur qui la porte, que l'appelant EMPORTE : c'est la carte
// qui les fait vivre, avec celles de son RSW.
std::vector<std::pair<glm::vec3, std::uint32_t>> EffectsInPlay::TakeChimneys() {
  return std::exchange(per_map_.chimneys, {});
}

bool EffectsInPlay::PlayById(std::uint32_t effect_id, glm::vec3 ground_position,
                             std::uint32_t now_ms, std::uint32_t owner_id, bool endless,
                             std::int32_t life_floor_ticks, bool announce_sound,
                             std::int32_t life_exact_ticks, float instance_scale,
                             float instance_second, const std::optional<glm::vec3>& target_position,
                             std::int32_t instance_param) {
  // UNE CLASSE INSCRITE PASSE DEVANT LE FICHIER ET LE PATRON : voir
  // `EffectClassesInPlay`.
  if (ro::render::IsPortedEffectClass(effect_id)) {
    return classes_.Spawn({.effect_id = effect_id,
                           .owner_id = owner_id,
                           .ground_position = ground_position,
                           .bound_ground_position = target_position},
                          now_ms);
  }
  // 🔴 SOUS `/mineffect`, CERTAINS BÂTISSEURS SORTENT ENTIERS : ni nœud, ni
  // bruit. Voir `ro::render::EffectVanishesWithMinimalEffects`.
  if (minimal_effects_ && ro::render::EffectVanishesWithMinimalEffects(effect_id)) {
    return true;
  }
  // UNE SEULE INSTANCE PAR PORTEUR pour certains : la redemander ne recommence rien.
  if (StrAlreadyOnOwner(per_map_.playing, effect_id, owner_id)) {
    return true;
  }
  const std::string file = AskedEffectFile(effect_id, now_ms);
  // Pas de fichier ne veut pas dire pas d'image, et un fichier n'exclut pas les
  // nœuds — voir `Notice`. Les deux se jouent, et l'un des deux suffit.
  //
  // ⚠ LA BOUCLE NE VAUT QUE POUR LE FICHIER : un effet PROCÉDURAL se sème une
  // fois, et son patron de nœuds n'a pas de prise par identifiant pour le
  // rejouer ni le couper. Aucune aura d'état n'est dans ce cas.
  const bool played_by_nodes =
      EffectNodesInPlay::KnowsProcedural(effect_id) &&
      nodes_->PlayProcedural(effect_id, ground_position, target_position, 0.0F, owner_id,
                             instance_param, life_floor_ticks, life_exact_ticks, instance_scale, 0,
                             instance_second);
  const bool by_file =
      !file.empty() && Play(std::string(ro::render::kEffectFolder) + file, ground_position, now_ms,
                            owner_id, endless ? owner_id : 0, effect_id);
  if (!played_by_nodes && !by_file) {
    return false;
  }
  if (by_file) {
    QueueQuakes(per_map_.pending_quakes,
                ShapeStrPlaying(per_map_.playing.back(), effect_id, owner_id,
                                StrTowardTargetDegrees(effect_id, camera_ ? &*camera_ : nullptr,
                                                       ground_position, target_position)));
  }
  QueueUpdateQuakes(per_map_.pending_quakes, effect_id, now_ms);
  if (announce_sound) {
    NoteLaunched(effect_id, {ground_position.x, ground_position.y, ground_position.z}, owner_id);
  }
  return true;
}

void EffectsInPlay::NoteLaunched(std::uint32_t effect_id, std::array<float, 3> ground,
                                 std::uint32_t bearer_id) {
  per_map_.just_launched.push_back({effect_id, ground, bearer_id});
}

bool EffectsInPlay::Play(std::string_view path, glm::vec3 ground_position, std::uint32_t now_ms,
                         std::uint32_t owner_id, std::uint32_t loops_with,
                         std::uint32_t from_effect_id) {
  const ro::render::LoadedEffect* effect = catalog_->Load(path);
  if (effect == nullptr) {
    return false;
  }
  // ⚠ UNE AURA VEUT LES DEUX CHAMPS : `owner_id` la fait suivre son porteur,
  // `loops_with_actor` la fait rejouer son fichier et s'éteindre avec lui.
  Playing sown{effect, owner_id, ground_position, now_ms};
  sown.loops_with_actor = loops_with;
  sown.from_effect_id = from_effect_id;
  per_map_.playing.push_back(sown);
  return true;
}

std::span<const ro::render::DrawnEffect> EffectsInPlay::Drawn(std::uint32_t now_ms,
                                                              const MapStage& stage,
                                                              const ro::sim::World& world,
                                                              ro::sim::WalkPlanner& planner) {
  drawn_.clear();
  behind_.clear();
  over_.clear();
  laid_quads_.clear();
  laid_lots_.clear();
  // LES TREMBLEMENTS MIS EN FILE partent à leur tic, chacun de sa durée.
  std::erase_if(per_map_.pending_quakes, [&](const EffectMapMemory::PendingQuake& quake) {
    if (static_cast<std::int32_t>(now_ms - quake.due_ms) < 0) {
      return false;
    }
    screen_quake_until_ms_ =
        std::max(screen_quake_until_ms_.value_or(0U), quake.due_ms + quake.lasts_ms);
    return true;
  });
  std::size_t kept = 0;
  for (Playing& playing : per_map_.playing) {
    // UN EFFET QUI ATTEND SON DÉPART vit sans se montrer.
    if (static_cast<std::int32_t>(now_ms - playing.started_ms) < 0) {
      per_map_.playing[kept] = playing;
      ++kept;
      continue;
    }
    // Il rejoue son fichier tant que ce qu'il marque est là. Disparu, il s'en va
    // avec — un butin ramassé n'a plus de colonne.
    if (playing.loops_with_actor != 0 && world.Find(playing.loops_with_actor) == nullptr &&
        !world.skill_units().contains(playing.loops_with_actor)) {
      continue;
    }
    const std::optional<std::int32_t> frame =
        StrFrameAt(playing, static_cast<float>(now_ms - playing.started_ms));
    if (!frame.has_value() || playing.LostItsBearerAt(now_ms)) {
      continue;
    }
    // Un effet porté suit son acteur, qui peut marcher. Un porteur disparu
    // l'emporte ou le laisse finir où il était : voir `dies_with_actor`.
    if (playing.actor_id != 0) {
      if (const ro::sim::Actor* carrier = world.Find(playing.actor_id); carrier != nullptr) {
        const ro::sim::FractionalCell cell =
            ro::sim::InterpolatedCell(*carrier, planner.PathFor(*carrier), now_ms);
        const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
        playing.ground_position =
            glm::vec3(ground[0], ground[1], ground[2]) + ViewOffsetInWorld(playing.view_offset);
      } else if (playing.holds_last_frame ||
                 (playing.dies_with_actor && !ro::sim::WorldHolds(world, playing.actor_id))) {
        continue;
      }
    }
    // 🔴 COUPÉ N'EST PAS MORT, et c'est la forme du client : son portier
    // d'affichage saute le DESSIN et laisse l'instance vivre. Sortir d'ici
    // perdrait l'effet pour de bon, là où le client le remontre dès qu'on
    // rebascule.
    // ⚠ ET LA PORTE DE SON PORTEUR, de la même façon : voir `EffectDrawGate`.
    if (!ShowsEffect(playing.from_effect_id) ||
        !draw_gate_.DrawsEffect(playing.from_effect_id, playing.actor_id, playing.from_hat)) {
      per_map_.playing[kept] = playing;
      ++kept;
      continue;
    }
    if (playing.laid_on_ground) {
      LayOnGround(playing, *frame, stage);
    } else {
      ro::render::DrawnEffect shown{playing.effect, playing.ground_position, *frame,
                                    playing.lift_pixels, playing.behind_actors};
      shown.spin_degrees = StrTurnDegrees(playing, camera_ ? camera_->yaw_degrees : 0.0F);
      shown.scale = playing.scale;
      shown.flip_x = playing.flip_x;
      shown.flip_y = playing.flip_y;
      (playing.behind_actors ? behind_ : playing.over_everything ? over_ : drawn_).push_back(shown);
    }
    per_map_.playing[kept] = playing;
    ++kept;
  }
  per_map_.playing.resize(kept);
  BindLaidLots();
  // LES CLASSES BATTENT ICI, et ce qu'elles montrent rejoint les effets en
  // fichier : leurs `.str` et leurs quads du monde passent par le même renderer.
  classes_.Advance(world, stage, planner, now_ms);
  std::ranges::copy(classes_.Drawn(), std::back_inserter(drawn_));
  std::ranges::copy(classes_.DrawnBehindActors(), std::back_inserter(behind_));
  std::ranges::copy(classes_.WorldQuads(), std::back_inserter(laid_lots_));
  return drawn_;
}

void EffectsInPlay::Forget() {
  per_map_ = {};
  drawn_.clear();
  behind_.clear();
  over_.clear();
  just_sown_units_.clear();
  classes_.Forget();
}

void EffectsInPlay::NoticeOptionEffects(const ro::sim::World& world, const MapStage& stage,
                                        ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  // UN PORTEUR PARTI OU SANS OPTION EMPORTE SON COMPTE : voir `per_map_.sown_states`.
  std::erase_if(per_map_.sown_states, [&world](const auto& entry) {
    const ro::sim::Actor* bearer = world.Find(entry.first);
    return bearer == nullptr || bearer->option == 0;
  });
  // 🔴 ET CE QU'IL PORTAIT S'EN VA AVEC LUI. Un nœud accroché ne meurt que de sa
  // vie, et celle de l'anneau du classement PVP vaut dix mille tics : le joueur
  // qui quittait la carte laissait son anneau tourner sur une case vide jusqu'au
  // changement de carte suivant. Vu en jeu.
  //
  // 🔴 ET UNE UNITÉ AU SOL EST UN PORTEUR, elle aussi : `World::Holds` répond
  // pour les DEUX tables. Ne demander qu'aux acteurs faisait deux fautes
  // opposées — un mur accroché à son unité tombait dès son premier semis, et un
  // mur sans ancre restait à l'écran dix mille tics après sa levée.
  const auto is_present = [&world](std::uint32_t id) { return ro::sim::WorldHolds(world, id); };
  nodes_->ForgetAbsentBearers(is_present);
  // ⚠ ET LA MÉMOIRE DE CE QU'ON A DÉJÀ VU AUSSI, sans quoi un acteur qui revient
  // sous le même identifiant — le serveur les recycle — ne rejouerait jamais ce
  // qu'il portait : `Notice` le croirait déjà posé.
  std::erase_if(per_map_.noticed, [&world](const auto& entry) {
    return !world.actors().contains(entry.first.first);
  });
  std::erase_if(per_map_.seen_options,
                [&world](const auto& entry) { return !world.actors().contains(entry.first); });

  for (const auto& [id, actor] : world.actors()) {
    // 🔴 LE FRONT MONTANT SE LIT SUR LE PAQUET, PAS SUR L'IMAGE. Le client
    // tranche dans `clif_changeoption_recv` sur les deux mots du paquet reçu ;
    // un bit qui tombe puis remonte entre deux images ne laisse rien dans le
    // mot. `World::SetStates` compte les changements, et l'acteur porte les
    // bits du dernier. Voir `Actor::option_risen`.
    //
    // ⚠ UN ACTEUR JAMAIS VU NE SONNE PAS : on retient son compte tel quel. Le
    // serveur donne l'état qui TIENT, pas celui qui vient d'être posé, et
    // partir de zéro ferait sonner tous ceux qui entrent dans le champ de
    // vision avec leur Sight déjà allumé — et à chaque changement de carte.
    const auto vu = per_map_.seen_options.find(id);
    const bool rise = vu != per_map_.seen_options.end() && vu->second != actor.option_rise_count;
    per_map_.seen_options.insert_or_assign(id, actor.option_rise_count);
    // L'EFFET SUIT SON PORTEUR — place, face et angle du corps : voir `FollowActor`.
    if (nodes_->HasClocksAttachedTo(id)) {
      const ro::sim::FractionalCell cell =
          ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
      const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
      nodes_->FollowActor(
          id, glm::vec3(ground[0], ground[1], ground[2]),
          OwnerFacingDegrees(actor, planner.PathFor(actor), now_ms),
          ClientBodyDegrees(actor, ro::sim::FacingAt(actor, planner.PathFor(actor), now_ms)));
    }
    for (const ro::render::OptionEffect& state : ro::render::OptionEffectTable()) {
      if ((actor.option & state.option_mask) == 0) {
        continue;
      }
      // 🔴 LE BRUIT PART DU FRONT MONTANT, PAS DU RESEMIS. Le client le joue
      // dans `clif_changeoption_recv`, gardé par un test de changement puis de
      // présence du bit : une fois, à l'instant où le bit s'allume. Le lier au resemis ferait
      // sonner Sight toutes les deux cent soixante-dix tics, tant qu'il dure.
      if (rise && (actor.option_risen & state.option_mask) != 0 && !state.rise_sound.empty()) {
        per_map_.just_sown_named_sounds.push_back(
            SownNamedSound{state.rise_sound, ro::sim::StandingCell(actor)});
      }
    }
    const auto sown = per_map_.sown_states.find(id);
    const ro::render::OptionEffect* due = ro::render::OptionEffectDue(
        actor.option, sown != per_map_.sown_states.end()
                          ? std::optional<std::uint32_t>(now_ms - sown->second)
                          : std::nullopt);
    if (due == nullptr) {
      continue;
    }
    per_map_.sown_states.insert_or_assign(id, now_ms);
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    // ⚠ LE MÊME CHEMIN QUE `@effect`, attaché à l'acteur : c'est l'effet lui-même.
    PlayById(due->effect_id, glm::vec3(ground[0], ground[1], ground[2]), now_ms, id);
  }
}

void EffectsInPlay::NoticeSkillUnitVisuals(const ro::sim::World& world, const MapStage& stage,
                                           std::uint32_t now_ms) {
  just_sown_units_.clear();
  // ⚠ LA LEVÉE D'UNE UNITÉ EST UNE SCÈNE : sa fabrique y joue ses effets de
  // fin. Elle s'oublie ensuite, sans quoi son compte retiendrait le premier
  // semis de celle qui reprendra son identifiant.
  for (auto kept = per_map_.sown_units.begin(); kept != per_map_.sown_units.end();) {
    if (world.skill_units().contains(kept->first)) {
      ++kept;
      continue;
    }
    const std::uint32_t instance_id = kept->first;
    const SownUnit gone = kept->second;
    kept = per_map_.sown_units.erase(kept);
    // Ce que la pose avait inscrit cesse : la levée réinscrit ce qu'elle veut.
    std::erase_if(per_map_.pending_named, [instance_id](const PendingNamed& waiting) {
      return waiting.owner_id == instance_id && waiting.from_unit && !waiting.after_vanish;
    });
    PlayNamedUnit(instance_id, gone.unit_id, gone.cell, ro::render::SkillUnitNamedMoment::kOnVanish,
                  now_ms);
  }

  for (const auto& [id, unit] : world.skill_units()) {
    // L'unité muette cache ce que le serveur ne montre pas : une case, rien de plus.
    if (unit.unit_id == ro::sim::kSilentSkillUnit) {
      continue;
    }
    // 🔴 UNE UNITÉ DÉJÀ POSÉE QUI REPARAÎT AILLEURS SE DÉPLACE, elle ne renaît
    // pas : le serveur renvoie la même unité à sa nouvelle case quand son groupe
    // suit son poseur — les chansons et les danses à chaque pas du barde. Ce
    // qu'elle porte la suit alors selon la table de suivi de chaque effet,
    // comme le porteur d'un effet d'acteur.
    if (const auto already_sown = per_map_.sown_units.find(id);
        already_sown != per_map_.sown_units.end() && (already_sown->second.cell.x != unit.cell.x ||
                                                      already_sown->second.cell.y != unit.cell.y)) {
      already_sown->second.cell = unit.cell;
      const std::array<float, 3> moved = GroundPositionAt(stage, unit.cell.x, unit.cell.y);
      nodes_->FollowActor(id, glm::vec3(moved[0], moved[1], moved[2]), 0.0F);
    }
    // LA FABRIQUE D'ABORD, et elle exclut les deux autres chemins : une unité
    // qu'elle sert n'a ni effet d'apparition ni effet d'entretien.
    const std::span<const ro::render::SkillUnitNamedEffectEntry> named =
        ro::render::SkillUnitNamedEffects(unit.unit_id);
    if (!named.empty()) {
      if (!per_map_.sown_units.contains(id)) {
        // Une attente NULLE : les semis nommés portent leur propre horloge.
        per_map_.sown_units.insert_or_assign(
            id, SownUnit{now_ms, 0, static_cast<std::uint16_t>(unit.unit_id), unit.cell});
        PlayNamedUnit(id, static_cast<std::uint16_t>(unit.unit_id), unit.cell,
                      ro::render::SkillUnitNamedMoment::kOnPlace, now_ms, unit.skill_level, &stage);
      }
      continue;
    }
    const ro::render::SkillUnitVisualEntry* visual = ro::render::SkillUnitVisual(unit.unit_id);
    // LE VISUEL QUI DÉPEND DU NIVEAU n'est pas dans le cas du tic : l'apparition
    // l'a choisi et rangé dans un champ de l'acteur, que le tic consomme. Ne le
    // chercher que dans la table ordinaire fait conclure que l'unité n'a rien.
    ro::render::SkillUnitVisualEntry by_level;
    if (visual == nullptr) {
      const std::uint16_t effect_id =
          ro::render::SkillUnitLevelVisual(unit.unit_id, unit.skill_level);
      if (effect_id == 0) {
        continue;
      }
      by_level.unit_id = static_cast<std::uint16_t>(unit.unit_id);
      by_level.effect_id = effect_id;
      by_level.shape = ro::render::SkillUnitVisualShape::kPermanent;
      visual = &by_level;
    }
    const bool first = !per_map_.sown_units.contains(id);
    if (const auto sown = per_map_.sown_units.find(id); sown != per_map_.sown_units.end()) {
      if (sown->second.period_ms == 0 || now_ms - sown->second.at_ms < sown->second.period_ms) {
        continue;
      }
      // 🔴 UN VISUEL PERMANENT NE SE RELANCE PAS : chaque tic de l'unité repousse
      // la fin de son instance. Il ne se repose que si tout ce qu'il a semé est mort.
      // Sauf s'il dure autant que son unité : il se relance avant de s'effacer.
      if (visual->shape == ro::render::SkillUnitVisualShape::kPermanent &&
          !ro::render::SkillUnitVisualLastsWithUnit(unit.unit_id) &&
          nodes_->CarriesEffect(visual->effect_id, id)) {
        sown->second.at_ms = now_ms;
        continue;
      }
    }
    const std::array<float, 3> ground = GroundPositionAt(stage, unit.cell.x, unit.cell.y);
    const std::uint32_t period_ms =
        SowUnitVisual(*visual, glm::vec3(ground[0], ground[1], ground[2]), now_ms, first, id);
    per_map_.sown_units.insert_or_assign(id, SownUnit{now_ms, period_ms, 0, unit.cell});
    // Ce qui n'a rien pu semer ne fait rien entendre non plus : le client joue
    // le son dans le même souffle que l'image, et un bruit sans image ferait
    // chercher un effet là où il n'y en a pas.
    if (period_ms != 0) {
      // L'identifiant de l'INSTANCE, pas celui de son type : le son se place
      // sur la case, et c'est le monde qui la donne.
      just_sown_units_.push_back(id);
    }
  }
  SowDueNamed(stage, now_ms);
}

std::uint32_t EffectsInPlay::SowUnitVisual(const ro::render::SkillUnitVisualEntry& visual,
                                           glm::vec3 ground_position, std::uint32_t now_ms,
                                           bool first_sow, std::uint32_t unit_instance_id) {
  // 🔴 LA PÉRIODE EST CELLE DE L'EFFET, QUOI QUE L'EFFET CONTIENNE — fichier
  // `.str`, nœuds, ou les deux. Le client ne regarde jamais ce qu'il y a dedans :
  //
  //     if (compteur >= durée(effet) − retrait) { compteur = 0; poser l'effet; }
  //
  // C'est le même code pour les quatre-vingt-six unités — Safety Wall pose
  // l'effet 315 et retranche SEPT.
  //
  // ⚠ ET LA DURÉE N'EST PAS LA VIE D'UN NŒUD. `EffectDurationTicks` lit la table
  // que le client garnit au démarrage ; `EffectNodeLifeFor` répond à une autre
  // question — « quelle vie un semis donne à son nœud » — et ne couvre qu'une
  // espèce de bâtisseur sur trois. Les deux coïncident pour Safety Wall et
  // divergent ailleurs.
  const std::uint32_t period = ro::render::SkillUnitVisualPeriodMs(visual);
  if (period == 0U && std::ranges::find(reported_, visual.effect_id) == reported_.end()) {
    reported_.push_back(visual.effect_id);
    // ⚠ ZÉRO N'EST PAS UNE PÉRIODE COURTE. Sans durée relevée on ne relance PAS,
    // plutôt que soixante fois par seconde : l'unité montre son effet une fois
    // et le garde, ce qui se voit et se corrige, là où un semis à chaque image
    // empile des centaines de nœuds et ressemble à une panne de rendu.
    ro::core::LogWarning(
        "unité au sol : la durée de l'effet {} n'est pas relevée — il sera posé UNE fois, sans "
        "relance",
        visual.effect_id);
  }
  const std::string file = AskedEffectFile(visual.effect_id, now_ms);
  const ro::render::LoadedEffect* effect =
      file.empty() ? nullptr : catalog_->Load(std::string(ro::render::kEffectFolder) + file);
  // 🔴 UN FICHIER N'EXCLUT PAS LES NŒUDS — voir `Notice`. ⚠ MAIS ILS NE SE
  // RELANCENT PAS AVEC LUI. Le client ne relance rien : son instance vit tant
  // que l'unité tient sa case, et les nœuds de son tic zéro ne renaissent pas.
  // La relance du fichier est la nôtre, pour un `.str` qui finit ; y accrocher
  // les nœuds EMPILERAIT à chaque tour des murs faits pour durer dix mille
  // tics. Ils naissent donc avec l'unité, une fois.
  // 🔴 ET ILS S'ACCROCHENT À L'UNITÉ. Sans ancre, un nœud ne meurt que de sa
  // vie — dix mille tics pour un mur —, et le mur de Safety Wall restait à
  // l'écran longtemps après que le serveur l'avait levé. Le client natif
  // l'efface dès que l'unité quitte sa case, que ce soit sa durée qui s'achève
  // ou son compte de coups qui s'épuise ; c'est `ForgetAbsentBearers` qui le
  // fait ici, et l'ancre est ce qui lui donne prise.
  const bool nodes_due = effect == nullptr || first_sow;
  const bool played_by_nodes = nodes_due && EffectNodesInPlay::KnowsProcedural(visual.effect_id) &&
                               nodes_->PlayProcedural(visual.effect_id, ground_position,
                                                      std::nullopt, 0.0F, unit_instance_id);
  // 🔴 ET LA LONGUEUR DU FICHIER N'EST PAS UNE PÉRIODE. Nous rejouions le `.str`
  // à sa propre longueur : `SafetyWall.str` fait soixante et une images, donc le
  // mur repartait toutes les neuf cent soixante millisecondes, indéfiniment.
  // Le client, lui, compare à la durée de l'effet — neuf mille neuf cent
  // quatre-vingt-dix-neuf tics pour 315 —, et pose donc le mur UNE FOIS. Ce que
  // le joueur voyait se relancer en boucle était ce fichier-là, et la faute
  // portait sur VINGT ET UN des quatre-vingt-six visuels, tous ceux qui ont un
  // fichier.
  // 🔴 ET L'EFFET SONNE EN NAISSANT, comme chez le client : c'est le BÂTISSEUR
  // qui joue le son à son tic zéro — celui des anneaux de Safety Wall joue
  // `EF_GlassWall.wav` dans le même souffle qu'il pose son nœud. Le son de l'UNITÉ, lui, part d'un
  // autre chemin
  // (`ActorSounds::HearSkillUnits`) et vient d'une autre table.
  //
  // ⚠ LES DEUX TABLES SONT DISJOINTES sur les quatre-vingt-six visuels :
  // ONZE ont un son d'effet sans son d'unité — Safety Wall, Benedictio, Fire
  // Pillar, les trois Ice Wall, Quagmire, Earth Spike, Dimension, Enchant
  // Poison, Ready Portal — et AUCUN n'a les deux. Rien ne peut donc sonner
  // deux fois, et ces onze-là n'ont que le son de leur effet.
  const auto play_sound = [this, &visual, ground_position] {
    NoteLaunched(visual.effect_id, {ground_position.x, ground_position.y, ground_position.z});
  };
  if (effect != nullptr) {
    per_map_.playing.push_back(Playing{effect, 0, ground_position, now_ms});
    play_sound();
    return period;
  }
  // LA FAMILLE PROCÉDURALE N'A PAS DE FICHIER, et c'est le seul moyen de le
  // savoir. La plupart des unités au sol en relèvent : le client les dessine en
  // code, et leur image est celle d'une troupe de nœuds.
  if (played_by_nodes) {
    play_sound();
    return period;
  }
  if (std::ranges::find(reported_, visual.effect_id) == reported_.end()) {
    reported_.push_back(visual.effect_id);
    // ⚠ « SANS FIGURE » ET NON « NON RELEVÉ » : cette condition vaut aussi faux
    // quand le patron EXISTE et que le semis n'a rien posé — `PlayProcedural`
    // rend faux dès qu'aucun nœud ne naît au tic zéro, ce qui arrive pour des
    // effets parfaitement relevés. Les deux se corrigent à des endroits opposés.
    ro::core::LogWarning(
        "unité au sol : effet {} ne montre rien — ni fichier ni patron, et le semis procédural "
        "n'a rien posé",
        visual.effect_id);
  }
  return 0;
}

}  // namespace moonclient
