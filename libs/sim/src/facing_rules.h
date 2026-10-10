#pragma once

// LES RÈGLES DU REGARD AU COMBAT : qui se tourne vers quoi quand un coup, une
// incantation, un soin ou une compétence posée sur une case arrive, et depuis
// où.
//
// Privé à la bibliothèque : ce sont les gestes que `World` fait sur ses propres
// acteurs, découpés hors de `world.cpp` parce qu'ils forment une famille — la
// même visée, cinq occasions de la poser — et non parce qu'ils seraient
// utiles à qui que ce soit d'autre.

#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>

#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/movement.h"
#include "ro/sim/walk_map.h"

namespace ro::sim::facing {

// Ce que les règles regardent du monde — en lecture seule, hors l'acteur
// qu'elles tournent.
struct Scene {
  const std::unordered_map<std::uint32_t, Actor>& actors;
  const std::map<std::uint32_t, SkillUnit>& units;
  const WalkMap* map;
  std::uint32_t player_id;
};

// Tourne `actor` vers un point, depuis là où il se trouve à `at_ms`, et fait
// que cela SE VOIE. Un point confondu avec lui ne change rien — pas même
// l'origine du regard.
void FaceTowards(const Scene& scene, Actor& actor, FractionalCell towards, std::uint32_t at_ms);

// Le regard de l'AUTEUR d'un coup, ordinaire ou de compétence.
// `target_at_strike` est où sa cible se trouvait au coup, avant que le coup ne
// l'arrête ; vide quand elle n'est pas en vue.
void FaceAttacker(const Scene& scene, Actor& attacker, const Strike& strike,
                  const std::optional<FractionalCell>& target_at_strike);

// Le regard du lanceur à l'ANNONCE d'une incantation.
void FaceCaster(const Scene& scene, Actor& caster, const Cast& cast);

// Le regard du lanceur d'une compétence posée SUR UNE CASE.
void FaceGroundSkillCaster(const Scene& scene, Actor& caster, const GroundSkillUse& use);

// Le regard du lanceur à l'EMPLOI d'une compétence sans dégâts, sur `target_id`.
void FaceSupporter(const Scene& scene, Actor& caster, std::uint32_t target_id,
                   const SupportHit& hit);

}  // namespace ro::sim::facing
