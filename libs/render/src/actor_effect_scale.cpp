#include <cstdint>

#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_scale.h"

namespace ro::render {
namespace {

// LES DEUX BITS DU MOT DE RENDU QUI CHANGENT LA TAILLE DU CORPS.
//
// Le client les lit tous les deux et multiplie l'échelle du corps : le premier
// l'emmène vers un demi, le second vers un et demi. Les deux sens partagent le
// même compteur et la même courbe — il n'y a donc qu'une règle, pas deux.
inline constexpr std::uint32_t kShrinkingBody = 0x10;
inline constexpr std::uint32_t kGrowingBody = 0x400;

// Le compteur que chacun lit. Un bit sans son champ ne montre rien (ADR 0024) :
// le bit dit VERS QUOI le corps va, le compteur dit OÙ IL EN EST.
inline constexpr std::uint16_t kShrinkingField = 0xF0;
inline constexpr std::uint16_t kGrowingField = 0xF1;

// Le pas que lit le bit de taille, ou rien quand aucun effet ne le lève.
//
// ⚠ LE PREMIER EFFET VIVANT QUI LÈVE LE BIT L'EMPORTE, et le compteur avec lui.
// Les cinq effets de taille testent le bit avant tout : déjà levé à ce tic, ils
// sortent sans rien écrire. Le mot de rendu se refaisant à chaque image, c'est
// le plus ANCIEN des effets vivants qui le lève en premier. Le retour à la
// taille normale ne passe donc pas par-dessus la réduction : le client retire
// la réduction avant de jouer le retour, qui ramène son compteur de
// quarante-cinq à zéro en quinze pas, puis s'éteint avec son bit.
struct BitStep {
  std::int32_t value = 0;
  bool raised = false;
};

[[nodiscard]] BitStep StepOn(std::span<const ActorEffectPlay> played, std::uint32_t bit,
                             std::uint16_t field) {
  for (const ActorEffectPlay& play : played) {
    const ActorEffectChange* change = ActorEffectChangeFor(play.effect_id);
    if (change == nullptr || (RenderBitsAt(*change, play.age_ticks) & bit) == 0) {
      continue;
    }
    for (const ActorFieldWrite& write : change->writes) {
      if (write.field == field) {
        return BitStep{ValueAt(write, play.age_ticks, change->life_ticks), true};
      }
    }
    // Un bit de taille levé sans compteur n'existe pas dans la table : les
    // cinq effets de taille écrivent tous le leur.
    return BitStep{};
  }
  return BitStep{};
}

}  // namespace

float BodyScaleAfterEffects(std::span<const ActorEffectPlay> played) {
  float scale = 1.0F;
  if (const BitStep shrink = StepOn(played, kShrinkingBody, kShrinkingField); shrink.raised) {
    scale *= SizeChangeScale(kSmallBodyScale, shrink.value);
  }
  if (const BitStep grow = StepOn(played, kGrowingBody, kGrowingField); grow.raised) {
    scale *= SizeChangeScale(kBigBodyScale, grow.value);
  }
  return scale;
}

float BodyShrinkAfterEffects(std::span<const ActorEffectPlay> played) {
  const BitStep shrink = StepOn(played, kShrinkingBody, kShrinkingField);
  return shrink.raised ? SizeChangeScale(kSmallBodyScale, shrink.value) : 1.0F;
}

}  // namespace ro::render
