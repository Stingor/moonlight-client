// LES EFFETS QU'UN NŒUD FAIT NAÎTRE, satisfaits ici.
//
// La troupe ne voit ni le monde ni les acteurs : ses nœuds EXPRIMENT une
// demande — quel effet, sur quel acteur ou à quel point —, et c'est ici qu'elle
// devient un effet joué. Voir `ro/render/effect_spawn_request.h`.

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_spawn_request.h"
#include "ro/render/effect_table.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"

namespace moonclient {

void EffectsInPlay::SowRequestedEffects(const ro::sim::World& world, const MapStage& stage,
                                        ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  for (const ro::render::EffectSpawnRequest& request : nodes_->TakeSpawnRequests()) {
    glm::vec3 place = request.position;
    std::uint32_t bearer = 0;
    if (request.place == ro::render::EffectSpawnPlace::kOnDesignatedActor) {
      // LE JOUEUR D'ABORD, par une comparaison EN FLOTTANT ; les autres par leur
      // identifiant relu en entier. Personne sous ce nom : rien ne naît.
      bearer = ro::render::DesignatesPlayer(request.designated_actor, world.player_id())
                   ? world.player_id()
                   : ro::render::DesignatedActorId(request.designated_actor);
      const ro::sim::Actor* actor = world.Find(bearer);
      if (actor == nullptr) {
        continue;
      }
      const ro::sim::FractionalCell cellule =
          ro::sim::InterpolatedCell(*actor, planner.PathFor(*actor), now_ms);
      const std::array<float, 3> sol = GroundPositionAt(stage, cellule.x, cellule.y);
      place += glm::vec3(sol[0], sol[1], sol[2]);
    }
    // UN EFFET QUI CHANGE L'ACTEUR s'inscrit sur lui, comme ceux qu'une suite
    // pose. ⚠ IL NE S'EMPILE PAS : réinscrit à chaque demande, il REPART de son
    // premier tic. Le corps garde donc la teinte de ce tic tant que les demandes
    // durent, et ne clignote qu'après la dernière.
    // ⚠ ET IL SE TAIT, pour la raison que dit le cas suivant : marqué vu,
    // `Notice` ne le fait pas sonner à chaque réinscription.
    if (bearer != 0 && !EffectNodesInPlay::KnowsProcedural(request.effect_id) &&
        ro::render::ActorEffectChangeFor(request.effect_id) != nullptr) {
      actor_effects_to_record_.push_back({bearer, request.effect_id, now_ms});
      per_map_.noticed.insert_or_assign(std::pair{bearer, request.effect_id}, now_ms);
      continue;
    }
    // ⚠ PAR LA TROUPE ET SANS SON BRUIT. Chaque demande est une instance neuve
    // chez le client, qui sonnerait à chacune : ce chemin ne fait pas entendre
    // soixante fois par seconde le son de l'enfant. Et il ne passe pas par
    // `AskedEffectFile` : ce n'est pas le serveur qui demande.
    if (EffectNodesInPlay::KnowsProcedural(request.effect_id)) {
      static_cast<void>(
          nodes_->PlayProcedural(request.effect_id, place, std::nullopt, 0.0F, bearer));
      continue;
    }
    if (const std::string file = ro::render::EffectFile(request.effect_id, now_ms); !file.empty()) {
      static_cast<void>(Play(std::string(ro::render::kEffectFolder) + file, place, now_ms, bearer,
                             0, request.effect_id));
      continue;
    }
    if (std::ranges::find(reported_, request.effect_id) == reported_.end()) {
      reported_.push_back(request.effect_id);
      ro::core::LogWarning("effet {} demandé par un nœud : ni fichier ni patron, rien ne se verra",
                           request.effect_id);
    }
  }
  // ET CE QU'UNE CLASSE DEMANDE SUR SON PORTEUR en s'en allant — l'ombre de
  // Shadow Exceed lui laisse son aura —, sauf s'il le porte déjà. Un effet qui
  // change l'acteur s'inscrit sur lui, comme ci-dessus ; les autres naissent à
  // sa place, attachés à lui.
  for (const EffectClassesInPlay::OwnerSpawn& asked : classes_.TakeOwnerSpawns()) {
    if (Carries(asked.effect_id, asked.owner_id)) {
      continue;
    }
    if (!EffectNodesInPlay::KnowsProcedural(asked.effect_id) &&
        ro::render::ActorEffectChangeFor(asked.effect_id) != nullptr) {
      const ro::sim::Actor* bearer = world.Find(asked.owner_id);
      if (bearer != nullptr &&
          std::ranges::find(bearer->effects, asked.effect_id, &ro::sim::PlayedEffect::id) ==
              bearer->effects.end()) {
        actor_effects_to_record_.push_back({asked.owner_id, asked.effect_id, now_ms});
      }
      continue;
    }
    static_cast<void>(PlayById(asked.effect_id, asked.ground_position, now_ms, asked.owner_id));
  }
}

}  // namespace moonclient
