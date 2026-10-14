// L'ombre d'une classe : de combien le disque sous ses pieds s'étale.
//
// À part de sprite_catalog.cpp, qui touchait au plafond. Le sujet se tient tout
// seul : il ne lit qu'une table du client et ne compose aucun chemin.

#include <cstdint>

#include "ro/render/shown_job.h"
#include "ro/render/sprite_catalog.h"

namespace ro::render {

float ShadowScaleFor(const SpriteCatalog& catalog, const ActorLook& look) {
  // UN OBJET AU SOL porte une petite ombre, sans regarder aucune table : son
  // numéro est celui d'un objet, pas d'une classe.
  constexpr float kItemShadow = 0.4F;
  if (look.item) {
    return kItemShadow;
  }
  // La tombe d'un monstre n'a pas d'ombre : le client l'écarte par sa classe.
  constexpr std::uint32_t kMobTombClass = 565;
  if (look.job == kMobTombClass) {
    return 0.0F;
  }
  // LA COMBINAISON DE MADOGEAR élargit l'ombre, et ce qu'en disent les tables
  // ne compte plus. Moins pour le bébé.
  constexpr std::uint32_t kBabyMadogearJob = 4112;
  constexpr float kMadogearSuitShadow = 1.5F;
  constexpr float kBabyMadogearSuitShadow = 1.12F;
  if (look.madogear_suit && IsMadogearJob(look.job)) {
    return look.job == kBabyMadogearJob ? kBabyMadogearSuitShadow : kMadogearSuitShadow;
  }
  // Le rayon d'abord : il ne concerne que les plus grandes créatures, et il
  // l'emporte quand il est là.
  if (const double* radius = FindSpriteEntry(catalog.tables.class_radii, look.job);
      radius != nullptr && *radius != 0.0) {
    return 2.0F * static_cast<float>(*radius) + 1.0F;
  }
  if (const double* factor = FindSpriteEntry(catalog.tables.class_shadows, look.job);
      factor != nullptr) {
    return static_cast<float>(*factor);
  }
  return 1.0F;
}

}  // namespace ro::render
