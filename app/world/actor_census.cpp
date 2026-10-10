#include <algorithm>
#include <cstddef>

#include "ro/core/log.h"

#include "world/actor_view.h"

namespace moonclient {

void ReportMissingSprites(ro::render::ActorWardrobe* wardrobe, std::size_t& already_reported) {
  if (wardrobe == nullptr) {
    return;
  }
  for (; already_reported < wardrobe->missing().size(); ++already_reported) {
    ro::core::LogWarning("sprite : {} absent", wardrobe->missing()[already_reported]);
  }
}

void ReportCensus(const SceneCensus& census, SceneCensus& last_reported) {
  // 🔴 `without_image` NE SE COMPARE PAS COMME LE RESTE, et c'est ce qui faisait
  // repartir ce recensement à chaque image. Il compte les acteurs dont la
  // composition n'a rendu aucun calque — quantité qui suit l'IMAGE
  // D'ANIMATION, non la population : une seule trame vide dans un sprite la
  // fait monter puis redescendre soixante fois par seconde, et chacune de ces
  // lignes est vidée sur le disque.
  //
  // On n'en retient donc que le PLUS HAUT ATTEINT. « Il est arrivé que {} n'aient
  // rien à montrer » est le fait qu'on cherche en lisant ce recensement, et ce
  // fait-là ne se dédit pas.
  SceneCensus population = census;
  population.without_image = std::max(census.without_image, last_reported.without_image);
  if (population == last_reported) {
    return;
  }
  last_reported = population;
  // Au journal de MISE AU POINT, et non sur la sortie d'erreur : ce recensement
  // sert à comprendre pourquoi un acteur ne paraît pas, et il change à chaque
  // arrivée comme à chaque départ. Sur une carte fréquentée, il noierait ce
  // qu'on a vraiment besoin de lire.
  ro::core::LogDebug(
      "scène : {} acteur(s), {} dessiné(s) ; {} sans sprite, {} caché(s), {} sans corps, "
      "{} sans image (au plus)",
      population.actors, population.drawn, population.without_sprite, population.hidden,
      population.without_body, population.without_image);
}

}  // namespace moonclient
