// LE REGARD D'UN ACTEUR : vers ce qu'il vise, ou vers où le serveur le tourne.
//
// À part du reste du monde parce que ces règles vont ensemble : chacune dit la
// direction ET qui l'a donnée, et le client n'en tire pas le même angle.

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "facing_rules.h"

namespace ro::sim {
namespace {

// Le centre d'une case, là où se tient quiconque n'a pas dit autrement.
[[nodiscard]] FractionalCell CentreOf(CellPosition cell) noexcept {
  return FractionalCell{static_cast<float>(cell.x), static_cast<float>(cell.y)};
}

template <std::size_t N>
[[nodiscard]] bool Listed(const std::array<std::uint16_t, N>& skills, std::uint16_t skill_id) {
  return std::ranges::find(skills, skill_id) != skills.end();
}

// LES COMPÉTENCES DONT LE COUP NE TOURNE PAS SON AUTEUR : les douze classes du
// registre des actions qui laissent aussi la motion en cours — voir
// `kSkillsKeepingMotionOnHit`, côté rendu.
constexpr std::array<std::uint16_t, 12> kSkillHitsKeepingFacing = {
    5411, 5412, 5415, 5445, 5455, 5457, 5459, 5471, 5472, 5473, 5501, 6522};

// L'INCANTATION NE TOURNE SON LANCEUR QUE SI ELLE DURE — une annonce à durée
// nulle sort avant le geste —, sauf pour le poing d'Asura, qui y passe quand
// même, et Kyrie Eleison, qui tourne vers sa cible avant même cette sortie.
constexpr std::uint16_t kExtremityFistSkill = 271;
constexpr std::uint16_t kKyrieEleisonSkill = 73;

// LES COMPÉTENCES SANS DÉGÂTS QUI NE TOURNENT PAS LEUR LANCEUR : le retour d'un
// homoncule, et deux dont l'emploi pose un effet sur la cible à la place.
constexpr std::array<std::uint16_t, 3> kSupportSkillsKeepingFacing = {244, 777, 778};
// CELLES DONT LE CHAMP DE CIBLE PORTE UNE CASE, x en poids fort et y en poids
// faible : le lanceur se tourne vers elle.
constexpr std::array<std::uint16_t, 13> kSupportSkillsAimedAtCell = {12, 18,  21,  70,  79,  83, 85,
                                                                     89, 110, 478, 723, 733, 769};
constexpr unsigned kPackedCellShift = 16U;
constexpr std::uint32_t kPackedCellMask = 0xFFFFU;
// CELLES DONT LA CIBLE EST UNE UNITÉ AU SOL — retirer ou déclencher un piège.
constexpr std::array<std::uint16_t, 2> kSupportSkillsAimedAtUnit = {124, 131};

// LES COMPÉTENCES POSÉES SUR UNE CASE QUI NE TOURNENT PAS LEUR LANCEUR : le
// client leur impose la dernière image d'une action à part au lieu de la
// motion d'emploi, sans regard vers la case. Dix en dur, et une — 5455 — dont
// la classe du registre des actions le demande. Voir
// `GroundSkillKeepsCasterMotion`.
constexpr std::array<std::uint16_t, 11> kGroundSkillsKeepingFacing = {
    83, 769, 774, 5028, 5042, 5219, 5223, 5224, 5231, 5236, 5455};

}  // namespace

bool GroundSkillKeepsCasterMotion(std::uint16_t skill_id) noexcept {
  return Listed(kGroundSkillsKeepingFacing, skill_id);
}

namespace facing {

void FaceTowards(const Scene& scene, Actor& actor, FractionalCell towards, std::uint32_t at_ms) {
  // 🔴 ON VISE D'OÙ L'ON SE TROUVE, et non depuis la case qu'on annonce : la
  // visée s'INVERSE dès qu'on a dépassé son point de départ, ce qui arrive à
  // chaque approche d'un adversaire.
  const FractionalCell from = PositionAt(actor, scene.map, at_ms);
  const float aimed_dx_cells = towards.x - from.x;
  const float aimed_dy_cells = towards.y - from.y;
  // Viser où l'on se trouve n'écrit rien : ni angle, ni origine du regard.
  if (aimed_dx_cells == 0.0F && aimed_dy_cells == 0.0F) {
    return;
  }
  actor.position.direction =
      DirectionOfAim(aimed_dx_cells, aimed_dy_cells, actor.position.direction);
  actor.facing_origin = FacingOrigin::kAim;
  actor.aimed_dx_cells = aimed_dx_cells;
  actor.aimed_dy_cells = aimed_dy_cells;
  // 🔴 ET CE REGARD DOIT REPRENDRE LA MAIN SUR CELUI DU DERNIER PAS. Sans ce
  // drapeau, `Locate` garde la direction de la queue du chemin pour quiconque a
  // marché une fois : la direction était écrite et jamais dessinée, ce qui est
  // le pire des deux mondes — un champ qui a l'air mesuré et ne se voit pas.
  // Voir `Actor::turned_since_walk_began`.
  //
  // ⚠ SANS EFFET PENDANT LA MARCHE : `Locate` ne consulte le drapeau qu'une
  // fois le pas fini, si bien qu'un coup reçu en courant ne fait pas pivoter le
  // coureur sur place.
  actor.turned_since_walk_began = true;
}

void FaceAttacker(const Scene& scene, Actor& attacker, const Strike& strike,
                  const std::optional<FractionalCell>& target_at_strike) {
  if (strike.attacker == strike.target) {
    return;
  }
  const bool skill_hit = strike.skill_id != 0;
  if (target_at_strike.has_value()) {
    // ⚠ LA CIBLE SE VISE OÙ ELLE SE TROUVAIT AU COUP, et non où elle s'arrêtera :
    // une cible qui court est annoncée à son départ, et la viser là ferait
    // frapper derrière elle.
    if (skill_hit && Listed(kSkillHitsKeepingFacing, strike.skill_id)) {
      return;
    }
    FaceTowards(scene, attacker, *target_at_strike, strike.started_ms);
    return;
  }
  // UNE UNITÉ AU SOL SE FRAPPE AUSSI — un mur de glace, un piège — et son
  // auteur s'y tourne comme vers n'importe qui.
  if (const auto unit = scene.units.find(strike.target); unit != scene.units.end()) {
    FaceTowards(scene, attacker, CentreOf(unit->second.cell), strike.started_ms);
    return;
  }
  // 🔴 UNE CIBLE HORS DE VUE : le coup d'une compétence ne tourne alors
  // personne, mais l'attaque ordinaire tourne son auteur À L'OPPOSÉ DU JOUEUR —
  // vers le point symétrique du joueur par rapport à lui. Ce que l'on ne voit
  // pas est au-delà de la vue, donc plus loin que soi de ce côté-là. Le joueur
  // lui-même, qui vise alors sa propre place, ne bouge pas.
  if (skill_hit) {
    return;
  }
  const auto player = scene.actors.find(scene.player_id);
  if (player == scene.actors.end()) {
    return;
  }
  const FractionalCell own = PositionAt(attacker, scene.map, strike.started_ms);
  const FractionalCell viewer = PositionAt(player->second, scene.map, strike.started_ms);
  FaceTowards(scene, attacker,
              FractionalCell{own.x - (viewer.x - own.x), own.y - (viewer.y - own.y)},
              strike.started_ms);
}

void FaceCaster(const Scene& scene, Actor& caster, const Cast& cast) {
  const bool announced_only = cast.duration_ms == 0 && cast.skill_id != kExtremityFistSkill;
  // 🔴 DEUX BRANCHES, PAS UNE. Le client tourne le lanceur dans les DEUX : vers
  // l'acteur visé quand le paquet en nomme un, vers la CASE quand le sort part
  // au sol. N'en porter qu'une laisserait un mage lancer Storm Gust en tournant
  // le dos à sa zone.
  if (cast.target_id != 0) {
    const auto target = scene.actors.find(cast.target_id);
    if (target == scene.actors.end() || cast.target_id == caster.id) {
      return;
    }
    if (announced_only && cast.skill_id != kKyrieEleisonSkill) {
      return;
    }
    FaceTowards(scene, caster, PositionAt(target->second, scene.map, cast.started_ms),
                cast.started_ms);
    return;
  }
  if (announced_only) {
    return;
  }
  // ⚠ LA CASE ZÉRO N'EST PAS UNE CASE VISÉE mais un champ vide : s'y tourner
  // ferait pivoter le lanceur vers le coin de la carte.
  if (cast.target_cell.x != 0 || cast.target_cell.y != 0) {
    FaceTowards(scene, caster, CentreOf(cast.target_cell), cast.started_ms);
  }
}

void FaceGroundSkillCaster(const Scene& scene, Actor& caster, const GroundSkillUse& use) {
  // 🔴 VERS LA CASE, PAR L'ANGLE EXACT, et pour TOUT lanceur — joueur, monstre
  // ou PNJ : le client le tourne avant de poser la motion d'emploi, sans garde
  // sur la case ni sur le métier. Viser sa propre case ne change rien.
  if (GroundSkillKeepsCasterMotion(use.skill_id)) {
    return;
  }
  FaceTowards(scene, caster, CentreOf(use.cell), use.started_ms);
}

void FaceSupporter(const Scene& scene, Actor& caster, std::uint32_t target_id,
                   const SupportHit& hit) {
  if (Listed(kSupportSkillsKeepingFacing, hit.skill_id)) {
    return;
  }
  if (Listed(kSupportSkillsAimedAtCell, hit.skill_id)) {
    const CellPosition cell{static_cast<std::uint16_t>(target_id >> kPackedCellShift),
                            static_cast<std::uint16_t>(target_id & kPackedCellMask), 0};
    FaceTowards(scene, caster, CentreOf(cell), hit.started_ms);
    return;
  }
  if (Listed(kSupportSkillsAimedAtUnit, hit.skill_id)) {
    if (const auto unit = scene.units.find(target_id); unit != scene.units.end()) {
      FaceTowards(scene, caster, CentreOf(unit->second.cell), hit.started_ms);
    }
    return;
  }
  // LE LANCEUR SE TOURNE VERS QUI IL SOIGNE OU RENFORCE, quand il le voit et
  // que ce n'est pas lui-même.
  const auto target = scene.actors.find(target_id);
  if (target == scene.actors.end() || target_id == caster.id) {
    return;
  }
  FaceTowards(scene, caster, PositionAt(target->second, scene.map, hit.started_ms), hit.started_ms);
}

}  // namespace facing

bool World::Turn(std::uint32_t id, std::uint8_t direction, std::uint8_t head_direction) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.position.direction = direction;
  found->second.head_direction = head_direction;
  ++found->second.turns_received;
  found->second.turned_since_walk_began = true;
  found->second.facing_origin = FacingOrigin::kServer;
  return true;
}

bool World::Reposition(std::uint32_t id, std::uint16_t x, std::uint16_t y, std::uint32_t now_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  Actor& actor = found->second;
  if (IsWalking(actor, now_ms)) {
    return true;
  }
  // LE REGARD MONTRÉ EST GARDÉ, et non `position.direction` : une marche
  // achevée le porte encore sur son dernier pas.
  const CellPosition where{x, y, FacingAt(actor, now_ms)};
  if (actor.movement.has_value()) {
    // La marche achevée reste l'événement que le repos quitte : on la ramène
    // sur place, sa distance reportée pour que le cycle de marche ne reparte
    // pas de sa première image au pas suivant.
    actor.travelled_before_cells =
        TravelledCells(actor, StraightWalkPath(actor.movement->from, actor.movement->to), now_ms);
    actor.movement->from = where;
    actor.movement->to = where;
    actor.turned_since_walk_began = true;
  }
  actor.position = where;
  return true;
}

void World::TrackCastTargets(std::uint32_t now_ms) {
  // 🔴 UN LANCEUR QUI INCANTE SUR QUELQU'UN LE SUIT DES YEUX : tant que dure
  // l'incantation, le client le retourne vers là où sa cible se trouve — une
  // cible qui contourne son lanceur le voit pivoter. Une incantation instantanée
  // n'a pas le temps de suivre, un sort au sol ou sur soi n'a personne à suivre,
  // et un coup qui fait broncher le lanceur lui fait quitter la pose qui suit.
  const facing::Scene scene{actors_, skill_units_, walk_map_, player_id_};
  for (auto& [id, actor] : actors_) {
    if (!actor.cast.has_value()) {
      continue;
    }
    const Cast& cast = *actor.cast;
    if (cast.duration_ms == 0 || cast.target_id == 0 || cast.target_id == id) {
      continue;
    }
    if (now_ms < cast.started_ms || now_ms - cast.started_ms >= cast.duration_ms) {
      continue;
    }
    if (actor.hit.has_value() && actor.hit->flinches && actor.hit->started_ms > cast.started_ms) {
      continue;
    }
    // ⚠ UNE COMPÉTENCE POSÉE SUR UNE CASE LUI FAIT OUBLIER SA CIBLE : le client
    // efface la cible suivie avant de le tourner vers la case, et seule une
    // incantation nouvelle la lui rend.
    if (actor.ground_skill.has_value() && actor.ground_skill->started_ms >= cast.started_ms) {
      continue;
    }
    const auto target = actors_.find(cast.target_id);
    if (target == actors_.end()) {
      continue;
    }
    facing::FaceTowards(scene, actor, PositionAt(target->second, walk_map_, now_ms), now_ms);
  }
}

}  // namespace ro::sim
