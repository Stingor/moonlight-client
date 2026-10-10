#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "ro/render/camera.h"

#include "effects/effect_map_memory.h"

namespace moonclient {

// CE QUE L'APPELANT FAIT DE L'IMAGE d'un `.str` qu'il sème : sa rotation à
// l'écran, son échelle, et le niveau qu'il lui passe.
struct StrLook {
  float spin_degrees = 0.0F;
  // La rotation suit la vue — voir `EffectMapMemory::Playing::turn_follows_view`.
  bool spin_follows_view = false;
  float scale = 1.0F;
  // Le niveau reçu à la naissance, ou aucun.
  std::int32_t level = kNoStrLevel;

  static constexpr std::int32_t kNoStrLevel = -1;
};

// Pose sur `playing` — l'instance que `effect_id` vient de semer sur
// `owner_id` — ce que le lecteur de `.str` fait de plus pour cet identifiant
// (`ro::render::StrEffectSetup`) : la file où l'image se peint, sa boucle, sa
// vie, son échelle fixe, ses relectures, et sa rotation vers le point visé,
// quand `toward_target_degrees` la donne. Rend les instants où l'écran doit
// trembler.
[[nodiscard]] std::vector<std::uint32_t> ShapeStrPlaying(
    EffectMapMemory::Playing& playing, std::uint32_t effect_id, std::uint32_t owner_id,
    std::optional<float> toward_target_degrees);

// Met en file les tremblements que `ShapeStrPlaying` a rendus : ils partent à
// leur instant, de la durée ordinaire.
void QueueQuakes(std::vector<EffectMapMemory::PendingQuake>& pending,
                 const std::vector<std::uint32_t>& due_ms);

// LES TREMBLEMENTS QUE L'ÂGE D'UNE INSTANCE DÉCLENCHE, quel que soit le chemin
// qui l'a fait naître — fichier ou nœuds — : Meteor Storm tremble au
// quarantième tic de sa boule. Voir `ro/render/effect_update_quake.h`. Les met
// en file dans `pending` ; rien pour un identifiant qui n'en demande aucun.
void QueueUpdateQuakes(std::vector<EffectMapMemory::PendingQuake>& pending, std::uint32_t effect_id,
                       std::uint32_t born_ms);

// Pose sur `playing` ce que l'appelant demande : sa rotation, quand il en donne
// une, et son échelle, fois celle que le lecteur tire du niveau.
void LookStrPlaying(EffectMapMemory::Playing& playing, std::uint32_t effect_id, const StrLook& look,
                    float view_yaw_degrees);

// L'ANGLE À L'ÉCRAN DE `from` VERS `to`, en degrés dans le sens des aiguilles
// d'une montre depuis la droite, ou rien quand les deux points se confondent à
// l'écran ou passent derrière la caméra.
[[nodiscard]] std::optional<float> ScreenBearingDegrees(const ro::render::Camera& camera,
                                                        const glm::vec3& from, const glm::vec3& to);

// La rotation que prend un `.str` qui se tourne vers `to` en naissant en
// `from`, ou rien pour un identifiant qui ne se tourne pas.
[[nodiscard]] std::optional<float> StrTowardTargetDegrees(std::uint32_t effect_id,
                                                          const ro::render::Camera* camera,
                                                          const glm::vec3& from,
                                                          const std::optional<glm::vec3>& to);

// Vrai quand `effect_id` ne vit qu'une fois par porteur et joue déjà sur
// `owner_id` : le redemander ne recommence rien.
[[nodiscard]] bool StrAlreadyOnOwner(std::span<const EffectMapMemory::Playing> playing,
                                     std::uint32_t effect_id, std::uint32_t owner_id);

// L'image que montre l'instance après `elapsed_ms`, ou rien quand elle a fini :
// son fichier une fois, rejoué, rebouclé, ou arrêté sur sa dernière image. Un
// effet qui boucle avec son porteur boucle ici sans condition : c'est à
// l'appelant de vérifier que le porteur est encore là.
[[nodiscard]] std::optional<std::int32_t> StrFrameAt(const EffectMapMemory::Playing& playing,
                                                     float elapsed_ms);

// La rotation de l'instance à cette image, en degrés dans [0, 360) quand elle
// suit la vue.
[[nodiscard]] float StrTurnDegrees(const EffectMapMemory::Playing& playing, float view_yaw_degrees);

}  // namespace moonclient
