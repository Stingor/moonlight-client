#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "ro/core/log.h"
#include "ro/render/auto_counter.h"
#include "ro/render/effect_frame.h"
#include "ro/render/floating_number.h"
#include "ro/render/second_hand.h"
#include "ro/render/skill_damage_timing.h"
#include "ro/render/skill_hit_effect.h"
#include "ro/render/skill_hit_extra.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/inventory.h"
#include "ro/sim/movement.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/world.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "world/actor_view.h"
#include "world/element_glow_of.h"
#include "world/hit_popups.h"
#include "world/struck_place.h"

// CE QU'UN COUP MONTRE SUR CE QU'IL TOUCHE, et ce qui vole jusqu'à lui.
//
// Les deux moissons que voici naissent du journal des coups
// (`World::strikes`) : l'impact que la cible pose sur elle, et le projectile
// qui part du geste. Elles partagent `per_map_.pending_hits` et la même
// horloge de vol, et rien d'autre du fichier voisin ne les regarde.
//
// ⚠ SORTIES DE `effect_view.cpp` LE JOUR OÙ IL A CRÉVÉ SON PLAFOND : la coupure
// suit une frontière réelle — la source du coup — et non la taille.

namespace moonclient {

void EffectsInPlay::NoticeHitEffects(const SoundStage& scene) {
  if (scene.wardrobe == nullptr || scene.world == nullptr || scene.stage == nullptr ||
      scene.planner == nullptr) {
    return;
  }
  // CEUX DONT L'HEURE EST VENUE D'ABORD, sur la cible telle qu'elle est ALORS.
  std::erase_if(per_map_.pending_hits, [&](const PendingHit& pending) {
    if (scene.now_ms < pending.due_ms) {
      return false;
    }
    // LE PORTEUR TEL QU'IL EST ALORS : un acteur, ou une unité au sol — celle
    // que le serveur a retirée entre-temps garde la case relevée au coup.
    std::optional<ro::sim::FractionalCell> bearer_cell =
        StruckCellAt(*scene.world, *scene.planner, pending.target_id, scene.now_ms);
    if (!bearer_cell.has_value() && pending.unit_cell.has_value()) {
      bearer_cell = CellOfUnit(*pending.unit_cell);
    }
    if (bearer_cell.has_value()) {
      const std::array<float, 3> ground =
          GroundPositionAt(*scene.stage, bearer_cell->x, bearer_cell->y);
      // LE POINT VISÉ, pris sur ce qui est visé tel qu'il est alors — l'unité
      // frappée comprise, à la case relevée au coup si elle n'est plus là.
      std::optional<glm::vec3> aim;
      std::optional<ro::sim::FractionalCell> aimed_cell;
      if (pending.aim_id != 0) {
        aimed_cell = StruckCellAt(*scene.world, *scene.planner, pending.aim_id, scene.now_ms);
        if (!aimed_cell.has_value() && pending.aim_id == pending.target_id &&
            pending.unit_cell.has_value()) {
          aimed_cell = CellOfUnit(*pending.unit_cell);
        }
      }
      if (aimed_cell.has_value()) {
        const std::array<float, 3> point =
            GroundPositionAt(*scene.stage, aimed_cell->x, aimed_cell->y);
        aim = glm::vec3(point[0], point[1], point[2]);
      }
      // ⚠ ET LE REFUS SE DIT, une fois par effet. `PlayById` rend FAUX quand
      // l'effet n'a ni fichier ni patron — il ne se dessine alors pas et ne
      // sonne pas davantage, puisque le bruit part avec l'image. Ignorer ce
      // retour laissait le dernier maillon du seul chemin de Bash sans témoin :
      // toutes les tables répondaient, et l'écran restait vide.
      if (!PlayById(pending.effect_id, glm::vec3(ground[0], ground[1], ground[2]), scene.now_ms,
                    pending.target_id, false, 0, true, 0, 0.0F, 0.0F, aim) &&
          reported_hit_effects_.insert(pending.effect_id).second) {
        ro::core::LogWarning("impact : effet {} ni fichier ni patron, rien montré ni entendu",
                             pending.effect_id);
      }
    }
    return true;
  });
  for (const ro::sim::RecordedStrike& taken : hit_strikes_.Take(scene.world->strikes())) {
    const ro::sim::Strike& strike = taken.strike;
    const std::uint32_t id = strike.target;
    // UNE UNITÉ AU SOL FRAPPÉE REÇOIT SON IMPACT comme un monstre, à sa case —
    // même retirée depuis : voir `world/struck_place.h`.
    const ro::sim::Actor* const struck = scene.world->Find(id);
    const std::optional<ro::sim::CellPosition>& unit_cell = strike.struck_unit_cell;
    if (struck == nullptr && !unit_cell.has_value()) {
      continue;
    }
    const ro::sim::Actor* attacker = scene.world->Find(strike.attacker);
    const std::uint16_t attacker_job = attacker != nullptr ? attacker->look.job : std::uint16_t{0};
    // LE BOUCLIER QUI REMPLACE LE « MISS » : un monstre rate le joueur sous
    // Kyrie Eleison ou Parrying, et l'effet naît sur le joueur à l'instant où
    // le mot aurait paru. Voir `MissShowsGuardShield`.
    if (MissShowsGuardShield(strike, SidesOfStrike(*scene.world, strike))) {
      per_map_.pending_hits.push_back(
          {kMissGuardShieldEffect, id,
           static_cast<std::uint32_t>(static_cast<float>(strike.started_ms) +
                                      HitLandsAtMs(*scene.wardrobe, *scene.world, strike))});
    }
    // 🔴 UN COUP SANS COMPÉTENCE POSE UN IMPACT, LUI AUSSI : c'est la gerbe
    // qu'on voit à chaque coup au corps à corps. Le client la choisit par le
    // traitement des dégâts ordinaires, où rien ne filtre la sorte du coup : la
    // garde ci-dessous appartient au répartiteur des compétences, et ne vaut
    // que pour lui.
    const bool has_no_skill = strike.skill_id == 0;
    if (!has_no_skill && !ro::render::DamageTypeShowsHitEffect(strike.type, strike.skill_id)) {
      continue;
    }
    // ⚠ NI SANS LANCEUR CONNU, sauf pour une unité au sol : l'impact d'une
    // compétence est un message que le client envoie à l'acteur du lanceur.
    if (!has_no_skill && !StrikeByGroundUnit(*scene.world, strike) && attacker == nullptr) {
      continue;
    }
    // ⚠ ET L'IMPACT D'UNE UNITÉ AU SOL A SA PROPRE TABLE : le client le choisit
    // dans un autre répartiteur, où Storm Gust pose la fumée du froid.
    // 🔴 UN COUP MANQUÉ NE POSE PAS CE QUE PORTE LE MESSAGE DE DÉGÂTS. La cible
    // le traite en écartant d'abord les dégâts nuls : elle écrit « Miss » et
    // saute le son comme l'effet. La gerbe d'un coup ordinaire paraissait sur
    // un raté. L'effet de frappe d'un coup ordinaire voyage dans ce message ;
    // celui d'une compétence, non, et sa table sait déjà ce qu'un raté montre.
    const bool missed = strike.damage == 0;
    const std::int32_t effect_id =
        has_no_skill ? (missed ? -1 : ro::render::PlainHitStrikeEffect(strike.type))
        : StrikeByGroundUnit(*scene.world, strike)
            ? ro::render::SkillUnitHitEffect(strike.skill_id)
            : ro::render::SkillHitEffect(strike.skill_id, strike.damage != 0,
                                         strike.attacker == scene.world->player_id());
    // 🔴 LE MESSAGE DE DÉGÂTS PORTE DEUX EFFETS, et le second est celui qui
    // SONNE pour la plupart des compétences classiques. Bash pose la gerbe
    // ordinaire par la règle par défaut ET l'effet 1 par la carte du message,
    // dont le bruit est `EF_hit2.wav` : n'en poser qu'un le laissait muet.
    //
    // ⚠ PAS POUR UNE UNITÉ AU SOL. Notre table d'impact d'unité vient d'un
    // AUTRE répartiteur du client, et Storm Gust y pose déjà la fumée du froid ;
    // y ajouter celle de la carte la poserait DEUX fois par battement et par
    // case.
    //
    // ⚠ POUR UN COUP ORDINAIRE, C'EST LA CLASSE DE L'ATTAQUANT QUI CHOISIT —
    // le coup de pied d'un Taekwon —, et c'est lui qui porte la gerbe : son
    // effet de frappe est vide sauf sur un critique.
    const std::int32_t message_effect_id =
        missed         ? -1
        : has_no_skill ? ro::render::PlainHitMsgEffect(attacker_job, strike.attacker == id)
        : StrikeByGroundUnit(*scene.world, strike)
            ? -1
            : ro::render::SkillDamageMsgEffect(strike.skill_id);
    // ⚠ L'EFFET D'EMPLOI N'EST PAS ICI : il naît sur le lanceur à l'arrivée du
    // paquet, sans attendre l'image de frappe — voir `NoticeSkills`. Cette
    // sortie ne pose que ce que la cible reçoit quand le coup touche.
    if (std::ranges::find(reported_hits_, strike.skill_id) == reported_hits_.end()) {
      reported_hits_.push_back(strike.skill_id);
      if (has_no_skill) {
        ro::core::LogDebug(
            "coup ordinaire à l'impact : effet {}, du message {}, classe de l'attaquant {}, "
            "type de dégâts {}",
            effect_id, message_effect_id, attacker_job, strike.type);
      } else {
        ro::core::LogDebug(
            "compétence {} à l'impact : effet de frappe {}, du message {}, type de dégâts {}",
            strike.skill_id, effect_id, message_effect_id, strike.type);
      }
    }
    // ⚠ ET LES CAS PARTICULIERS COMPTENT DANS CETTE SORTIE. Une compétence qui
    // n'aurait qu'eux — aucun impact, aucune carte — serait renvoyée d'ici sans
    // avoir rien montré.
    const std::span<const ro::render::SkillHitExtraEntry> extras =
        has_no_skill ? std::span<const ro::render::SkillHitExtraEntry>{}
                     : ro::render::SkillHitExtrasFor(strike.skill_id);
    // 🔴 L'IMPACT NOMMÉ PREND LA PLACE DE L'EFFET DU MESSAGE : le client
    // consulte le registre des actions avant de poser celui-ci, à chaque
    // frappe, et ne le pose que si la compétence n'y nomme rien. Pas pour une
    // unité au sol, dont le message va à un autre répartiteur.
    const bool named =
        !has_no_skill && !StrikeByGroundUnit(*scene.world, strike) &&
        !ro::render::SkillNamedEffects(strike.skill_id, ro::render::kSkillNamedOnHit).empty();
    if (effect_id < 0 && message_effect_id < 0 && extras.empty() && !named) {
      continue;
    }
    const auto pose_ms =
        static_cast<std::uint32_t>(static_cast<float>(strike.started_ms) +
                                   HitLandsAtMs(*scene.wardrobe, *scene.world, strike));
    // ET LA SECONDE MAIN, un message de plus qui change le pas des autres : voir
    // `ro/render/second_hand.h`.
    const bool has_second_hand = ro::render::ShowsSecondHand(strike.skill_id, strike.type,
                                                             strike.second_damage, attacker_job);
    const std::uint16_t first_hand_hits = ro::render::ShownHitCount(strike.type, strike.hits);
    const auto total_hits =
        static_cast<std::uint16_t>(first_hand_hits + (has_second_hand ? 1U : 0U));
    for (std::uint16_t hit = 0; hit < total_hits; ++hit) {
      const std::uint32_t offset =
          !has_second_hand        ? static_cast<std::uint32_t>(static_cast<float>(hit) *
                                                               ro::render::kMultiHitIntervalMs) +
                                        hit * ro::render::SkillHitStepExtraMs(strike.skill_id)
          : hit < first_hand_hits ? ro::render::TwoHandedHitOffsetMs(hit, ro::render::kHitStepMs)
                                  : ro::render::SecondHandOffsetMs(ro::render::kHitStepMs);
      // LES DEUX AU MÊME INSTANT : c'est un seul message, et le client remplit
      // ses deux champs avant de l'envoyer.
      if (effect_id >= 0) {
        per_map_.pending_hits.push_back(
            {static_cast<std::uint32_t>(effect_id), id, pose_ms + offset, 0U, unit_cell});
      }
      // Le RANG de la frappe va à la méthode : certaines se retournent une
      // frappe sur deux.
      if (named) {
        static_cast<void>(PlayNamedSkillOnHit(*scene.stage, *scene.planner, strike.skill_id,
                                              attacker, struck, hit, strike.attack_motion_ms,
                                              pose_ms + offset, unit_cell));
      } else if (message_effect_id >= 0 && (hit == 0 || has_no_skill)) {
        // ⚠ LA PREMIÈRE FRAPPE SEULE pour une compétence : son répartiteur
        // efface ce champ du message avant d'envoyer les suivantes. Celui du
        // coup ordinaire le laisse, et chaque frappe le porte.
        per_map_.pending_hits.push_back(
            {static_cast<std::uint32_t>(message_effect_id), id, pose_ms + offset, 0U, unit_cell});
      }
    }
    // LES CAS PARTICULIERS DU COUP — les enchaînements du moine et du champion,
    // les coups du Sura. Le client les traite dans le MÊME switch que l'effet
    // d'emploi, donc une fois par message et non par frappe.
    const auto already_sown = per_map_.hit_extras_sown.find(strike.attacker);
    const bool sown_again =
        already_sown != per_map_.hit_extras_sown.end() && already_sown->second == strike.started_ms;
    for (const ro::render::SkillHitExtraEntry& extra : extras) {
      // 🔴 RIEN SUR SOI-MÊME pour SR_RAMPAGEBLASTER, et rien du tout pour
      // SR_EARTHSHAKER qui vient de se jouer : sans ces deux gardes, une
      // compétence de zone rejouerait sa gerbe à chaque victime.
      if (extra.guard == ro::render::HitExtraGuard::kNotOnSelf && strike.attacker == id) {
        continue;
      }
      if (extra.guard == ro::render::HitExtraGuard::kNotIfAlreadyThere && sown_again) {
        continue;
      }
      const bool on_caster = extra.on == ro::render::SkillEffectOn::kCaster;
      per_map_.pending_hits.push_back({extra.effect_id, on_caster ? strike.attacker : id, pose_ms,
                                       extra.aims_at_target ? id : 0U,
                                       on_caster ? std::nullopt : unit_cell});
    }
    if (!extras.empty()) {
      per_map_.hit_extras_sown.insert_or_assign(strike.attacker, strike.started_ms);
    }
  }
  SpendAutoCounterGuards(scene);
}

// LA GARDE DU CHEVALIER, DÉPENSÉE SUR CELUI QUI FRAPPE.
//
// 🔴 CE CHEMIN NE RESSEMBLE À AUCUN AUTRE DE CE FICHIER : il ne consulte
// aucune table de compétence, ne regarde pas ce qui est touché, et naît sur
// l'ATTAQUANT. Aucune table d'effets de compétence ne le porte : il naît du
// traitement des dégâts, celui du coup ORDINAIRE.
//
// ⚠ SANS DÉLAI, à la différence des impacts. Le client le fait naître en
// traitant le message de dégâts, non en le rejouant à l'heure de la frappe :
// la garde éclate quand le chevalier riposte, pas quand son coup arrive.
void EffectsInPlay::SpendAutoCounterGuards(const SoundStage& scene) {
  std::erase_if(per_map_.auto_counter_armed, [&](const auto& garde) {
    const auto& [id, armed_ms] = garde;
    const ro::sim::Actor* attacker = scene.world->Find(id);
    if (attacker == nullptr) {
      return true;
    }
    // ⚠ UN COUP POSTÉRIEUR À L'ARMEMENT, et cette comparaison n'est pas un
    // ornement : `Actor::attack` garde le dernier coup porté longtemps après
    // qu'il a eu lieu, et une garde qui s'arme derrière lui le dépenserait
    // aussitôt — l'effet jaillirait à l'instant même où la posture se prend.
    if (!attacker->attack.has_value() || attacker->attack->started_ms < armed_ms ||
        !ro::render::AutoCounterSpentBy(attacker->attack->type)) {
      return false;
    }
    const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(
        *attacker, scene.planner->PathFor(*attacker), attacker->attack->started_ms);
    const std::array<float, 3> sol = GroundPositionAt(*scene.stage, cell.x, cell.y);
    PlayById(ro::render::kAutoCounterStrikeEffect, glm::vec3(sol[0], sol[1], sol[2]), scene.now_ms,
             id);
    return true;
  });
}

void EffectsInPlay::NoticeArrows(const SoundStage& scene) {
  if (scene.wardrobe == nullptr || scene.camera == nullptr || scene.world == nullptr ||
      scene.stage == nullptr || scene.planner == nullptr) {
    return;
  }
  const ro::render::SpriteCatalog& catalog = scene.wardrobe->catalog();
  // 🔴 UNE FRAPPE ATTEND SON GESTE, et le coup suivant ne l'efface pas : deux
  // flèches tirées dans la même trame en font partir deux. Chaque coup reste
  // ici jusqu'à son départ, ou jusqu'à ce qu'il n'y ait plus rien à tirer.
  for (const ro::sim::RecordedStrike& taken : arrow_strikes_.Take(scene.world->strikes())) {
    if (taken.strike.attacker == taken.strike.target) {
      continue;
    }
    // LA VOLÉE D'ARROW SHOWER sur chaque cible touchée : une flèche vers la
    // cible, puis huit vers les cases voisines de la sienne.
    // Une unité au sol frappée — un bloc d'Ice Wall — reçoit la sienne à sa case.
    if (taken.strike.skill_id == kArrowShowerSkill) {
      const std::optional<ro::sim::FractionalCell> caster_cell =
          StruckCellAt(*scene.world, *scene.planner, taken.strike.attacker, scene.now_ms);
      const std::optional<ro::sim::FractionalCell> aimed =
          StrikeTargetCellAt(*scene.world, *scene.planner, taken.strike, scene.now_ms);
      const auto ground_of = [&](ro::sim::FractionalCell cell) {
        const std::array<float, 3> ground = GroundPositionAt(*scene.stage, cell.x, cell.y);
        return glm::vec3(ground[0], ground[1], ground[2]);
      };
      if (scene.world->Find(taken.strike.attacker) != nullptr && caster_cell.has_value() &&
          aimed.has_value()) {
        QueueArrowShower(per_map_.shower_arrows, *scene.stage, taken.strike.attacker,
                         ground_of(*caster_cell), ground_of(*aimed), aimed->x, aimed->y,
                         scene.now_ms);
      }
    }
    // LE DRAPEAU D'UN IMPACT QUI DESSINE SES TRAITS se lève à l'arrivée du coup
    // et se dépense au premier geste de tir qui suit — le sien, d'ordinaire.
    if (ro::render::ImpactSuppressesNextArrow(taken.strike.skill_id)) {
      per_map_.arrow_suppressed.insert(taken.strike.attacker);
    }
    const ro::sim::Actor* const shooter = scene.world->Find(taken.strike.attacker);
    if (shooter != nullptr && per_map_.arrow_suppressed.contains(taken.strike.attacker) &&
        !ro::render::ArrowSpriteOf(catalog, LookOf(*shooter),
                                   shooter->status_effects.contains(ro::sim::kEfstSoulAttack))
             .empty()) {
      per_map_.arrow_suppressed.erase(taken.strike.attacker);
      continue;
    }
    per_map_.arrows_waiting.push_back(taken.strike);
  }
  // Rend vrai quand le coup a tiré, ou ne tirera jamais.
  const auto shoot = [&](const ro::sim::Strike& strike) {
    const ro::sim::Actor* const shooter = scene.world->Find(strike.attacker);
    if (shooter == nullptr) {
      return true;
    }
    const ro::sim::Actor& actor = *shooter;
    // Le projectile de CET acteur : la flèche d'un arc, le trait d'une
    // baguette, l'âme d'un Doram, ou celui de son espèce — et, faute de
    // projectile, le COUP que son espèce abat sur sa cible. Ni l'un ni
    // l'autre, il ne tire rien. Voir `ArrowSpriteOf` et `StrikeSpriteOf`.
    const ro::render::ActorLook look = LookOf(actor);
    const bool shoots_souls = actor.status_effects.contains(ro::sim::kEfstSoulAttack);
    const std::string_view projectile = ro::render::ArrowSpriteOf(catalog, look, shoots_souls);
    const std::optional<ro::render::CreatureStrikeSprite> hit =
        projectile.empty() ? ro::render::StrikeSpriteOf(look) : std::nullopt;
    if (projectile.empty() && !hit.has_value()) {
      return true;
    }
    // OÙ SE TIENT UN BOUT DU VOL : un acteur, ou l'unité au sol que le coup
    // frappe — voir `world/struck_place.h`.
    const auto ground_of =
        [&](const std::optional<ro::sim::FractionalCell>& cell) -> std::optional<glm::vec3> {
      if (!cell.has_value()) {
        return std::nullopt;
      }
      const std::array<float, 3> ground = GroundPositionAt(*scene.stage, cell->x, cell->y);
      return glm::vec3(ground[0], ground[1], ground[2]);
    };
    if (hit.has_value()) {
      // LE COUP S'ABAT AU GESTE, sans sommeil ni vol : l'effet d'attaque de
      // la mandragore se pose sur la cible avec un retard NUL, à l'instant où
      // le lanceur passe à l'attaque, et le sprite y joue son action jusqu'à
      // sa dernière image.
      if (const std::optional<glm::vec3> target_ground =
              ground_of(StrikeTargetCellAt(*scene.world, *scene.planner, strike, scene.now_ms));
          target_ground.has_value()) {
        nodes_->PlayActOnce(hit->sprite_base, *target_ground, hit->frame_period_units);
      }
      return true;
    }
    // 🔴 LA FLÈCHE PART AU GESTE, PAS AU COUP. Le client l'attend le temps de
    // l'animation — la flèche dort jusqu'à l'image où le geste porte — puis la
    // fait voler huit pas jusqu'à la cible. La blessure de la cible, elle,
    // attend ce que son tireur y ajoute : le vol entier pour un arc, davantage
    // pour quelques espèces, rien pour les autres. L'instant du coup moins ce
    // retard-là est donc l'instant du geste, et c'est là qu'elle naît.
    //
    // ⚠ CE RETARD N'EST PAS LE VOL. Retrancher le vol à tout tireur faisait
    // partir la balle d'une espèce sans retard cent quatre-vingt-douze
    // millisecondes AVANT que son geste ne l'ait lâchée. L'âme d'un Doram
    // compte dans le retard comme sa flèche : sans elle, la cible bronchait
    // huit pas avant que l'âme n'arrive.
    const float lands_at_ms = HitLandsAtMs(*scene.wardrobe, *scene.world, strike);
    const float departure_ms =
        lands_at_ms - ro::render::ProjectileDelayMs(catalog, look, shoots_souls);
    if (static_cast<float>(scene.now_ms - strike.started_ms) < departure_ms) {
      return false;
    }
    const std::optional<glm::vec3> target_ground =
        ground_of(StrikeTargetCellAt(*scene.world, *scene.planner, strike, scene.now_ms));
    const std::optional<glm::vec3> shooter_ground =
        ground_of(StruckCellAt(*scene.world, *scene.planner, strike.attacker, scene.now_ms));
    if (!target_ground.has_value() || !shooter_ground.has_value()) {
      return true;
    }
    // Les deux bouts, dix unités au-dessus du sol : le client retranche dix à
    // l'ordonnée de chacun, et son ordonnée descend.
    const glm::vec3 lift{0.0F, ro::render::kArrowFlightHeight, 0.0F};
    const glm::vec3 start = *shooter_ground + lift;
    const glm::vec3 arrival = *target_ground + lift;
    const glm::vec3 flight = arrival - start;
    const float distance = glm::length(flight);
    if (distance <= 0.0F) {
      return true;
    }
    // ⚠ SOUS QUARANTE UNITÉS LA FLÈCHE NE RALENTIT PAS : le client garde cinq
    // unités par pas et raccourcit le vol d'autant — huit pas pour quarante,
    // deux pour dix. En tics d'effet : douze pour le vol entier.
    const float share = std::min(distance / ro::render::kArrowFullFlightDistance, 1.0F);
    const auto flight_ticks = static_cast<std::int32_t>(
        std::lround(share * ro::render::kArrowFlightMs / ro::render::kEffectTickMs));
    // Et ses images défilent selon sa classe — flèche ou munition en sprite —,
    // à l'horloge du client, ou trois fois plus lentement pour le trait d'une
    // baguette, que le retard du coup attend d'autant : voir `ShootsWandBolts`.
    ro::render::ProjectileFlight drawn_flight = ro::render::ProjectileFlightOf(catalog, look);
    drawn_flight.glow_tint = ArrowGlowOf(actor, scene.element_glow);
    nodes_->ShootSprite(projectile, start, arrival, flight_ticks, drawn_flight);
    return true;
  };
  std::erase_if(per_map_.arrows_waiting, shoot);
  ShootDueShowerArrows(per_map_.shower_arrows, *nodes_, *scene.world, scene.element_glow,
                       scene.now_ms);
}

}  // namespace moonclient
