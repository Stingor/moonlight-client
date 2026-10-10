#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace ro::render {

// LES EFFETS D'UNE SEULE PARTICULE EN SPRITE : un couple `.spr`/`.act` posé sur
// un acteur ou à ses pieds, qui joue une action et la boucle — ou la joue une
// fois — tant que son instance vit. Les chemins sont écrits dans le client, pas
// dans une table de données : ils sont recopiés ici, un par identifiant.
//
// Trente-sept identifiants partagent la même classe ; un trente-huitième, le
// décor de Digital Space (1240), n'est pas une classe mais un cas du
// répartiteur procédural dont la figure est la même : un sprite qui suit son
// porteur et boucle. Voir `docs/effets-par-sprite.md`.
//
// ⚠ UNE AURA DE COSTUME EST DE CEUX-LÀ : les ailes de Valkyrie, l'ombre du
// serpent, l'aura d'un sujet… Aucun n'a de fichier `.str` ni de patron de
// nœuds, et c'est ce qui les laissait muets.

// Ce que l'image fait au bout de l'action.
enum class SpriteEffectPlay : std::uint8_t {
  // Elle repart de la première.
  kLoop,
  // Le sprite s'éteint après la dernière, et son instance avec lui.
  kOnce,
};

// CE QUI FAIT DURER L'INSTANCE.
enum class SpriteEffectLife : std::uint8_t {
  // Tant que son porteur est là : un costume, un état.
  kWhileBearer,
  // Tant que le sprite joué une fois n'a pas fini.
  kUntilPlayed,
  // Un nombre fixe de tics : voir `kSpriteEffectFixedLifeTicks`.
  kFixedTicks,
};

// La vie d'une instance à vie fixe, en tics.
inline constexpr std::int32_t kSpriteEffectFixedLifeTicks = 100;

// LA PLACE DU SPRITE : ses pieds sont ceux du porteur, relevés de `lift_units`
// — ou de `doram_lift_units` pour un porteur Doram —, plus, quand
// `adds_job_lift` est vrai, le relèvement que le métier du porteur impose
// (`JobEffectLift`). En unités du monde, vers le haut.
struct SpriteEffectRecipe {
  std::uint32_t effect_id = 0;
  // Relatif au dossier des sprites, sans extension.
  std::string_view sprite;
  std::int32_t action = 0;
  // Combien de tics chaque image tient.
  float frame_period_ticks = 0.0F;
  SpriteEffectPlay play = SpriteEffectPlay::kLoop;
  SpriteEffectLife life = SpriteEffectLife::kWhileBearer;
  // Le mot que la particule passe à la file de rendu : voir
  // `ro/render/effect_render_word.h`.
  std::uint32_t render_word = 0;
  // Voir `EffectClassSpriteDraw::depth_bias`.
  float depth_bias = 0.0F;
  float scale = 1.0F;
  float lift_units = 0.0F;
  float doram_lift_units = 0.0F;
  bool adds_job_lift = false;
};

// La recette d'un identifiant, ou rien.
[[nodiscard]] const SpriteEffectRecipe* SpriteEffectRecipeFor(std::uint32_t effect_id);

// Toutes les recettes, triées par identifiant.
[[nodiscard]] std::span<const SpriteEffectRecipe> SpriteEffectRecipes();

// DE COMBIEN LE MÉTIER RELÈVE CE QUI SE POSE AU-DESSUS D'UN PORTEUR, en unités :
// six pour un corps monté (Peco, dragon, griffon…), quatre pour un bébé monté,
// moins un pour un bébé à pied, moins deux pour un Doram, zéro sinon.
//
// ⚠ C'EST LA RÈGLE D'UN PORTEUR SANS L'ÉTAT QUE LE CLIENT TIENT À PART : avec
// lui, le bébé passe à quatre, le Doram à moins trois et tout autre joueur à
// six. Cet état n'est pas identifié ; voir `docs/effets-par-sprite.md`.
[[nodiscard]] float JobEffectLift(std::uint32_t job);

// LA HAUTEUR DU SPRITE au-dessus des pieds de son porteur, selon son métier ;
// sans porteur connu, celle d'un porteur ordinaire.
[[nodiscard]] float SpriteEffectLiftUnits(const SpriteEffectRecipe& recipe,
                                          std::optional<std::uint32_t> bearer_job);

}  // namespace ro::render
