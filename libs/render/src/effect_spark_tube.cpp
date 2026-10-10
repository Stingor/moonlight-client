#include "ro/render/effect_spark_tube.h"

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DEUX CASES DE QUEUE QUE CETTE FIGURE LIT : l'échelle de hauteur, puis le
// NOMBRE DE POINTS de la courbe — trois, quatre ou cinq selon le bâtisseur.
constexpr std::size_t kSlotOfHeightScale = 0;
constexpr std::size_t kSlotOfPointCount = 1;

// UN POINT DE LA COURBE, à partir de l'angle qu'une case porte.
//
// ⚠ LA DISTANCE EST PROPORTIONNELLE À L'ANGLE, LA HAUTEUR À SON SINUS. C'est
// ce qui fait l'arc : l'étincelle s'éloigne sans cesse et retombe. Prendre le
// sinus pour les deux la ramènerait au point de départ.
[[nodiscard]] glm::vec3 PointDeLaCourbe(const EffectTrack& track, float angle) {
  const float reach =
      angle > 0.0F ? (angle / kSparkTubeFullAngleDegrees) * track.radius_at_anchor : 0.0F;
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : là où il retranche, nous
  // ajoutons, et l'arc s'élève au lieu de s'enfoncer.
  const float height = track.radius_at_anchor * SinOfDegrees(static_cast<std::int32_t>(angle)) *
                       track.tail[kSlotOfHeightScale];
  // LE CAP DE LA PISTE EST CELUI DU CLIENT : sa profondeur se nie chez nous.
  return {reach * CosOfDegrees(track.tilt_degrees), height,
          -reach * SinOfDegrees(track.tilt_degrees)};
}

}  // namespace

std::array<std::uint8_t, 3> EffectSparkTubeColor(std::int16_t style, std::size_t point) {
  const auto rang = static_cast<std::int32_t>(point);
  switch (style) {
    case 0:
      return {255, 200, static_cast<std::uint8_t>(30 * rang)};
    case 1: {
      const auto canal = static_cast<std::uint8_t>(22 * rang + 10);
      return {canal, canal, 255};
    }
    case 2:
      return {255, 200, static_cast<std::uint8_t>(50 * rang)};
    default: {
      const auto canal = static_cast<std::uint8_t>(30 * rang + 105);
      return {canal, canal, canal};
    }
  }
}

std::vector<EffectWorldQuad> ComposeSparkTubeQuads(const EffectTrack& track) {
  std::vector<EffectWorldQuad> quads;
  // ⚠ TROIS GARDES, ET LA TROISIÈME EST LE RETARD : le bâtisseur sème un
  // compteur NÉGATIF, et la piste ne se montre qu'une fois qu'il a passé zéro.
  // L'oublier ferait paraître les quatre pistes ensemble.
  if (!track.active || track.alpha <= 0 || track.ticks <= 0) {
    return quads;
  }
  const auto count = static_cast<std::int32_t>(track.tail[kSlotOfPointCount]);
  if (count <= 0) {
    return quads;
  }

  for (std::int32_t point = 0; point < count; ++point) {
    const auto rang = static_cast<std::size_t>(point);
    if (rang + 1 >= track.value.size()) {
      break;
    }
    const std::int16_t alpha =
        static_cast<std::int16_t>(track.alpha - kSparkTubeAlphaFadePerPoint * point);
    const glm::vec3 head = PointDeLaCourbe(track, track.value[rang]);
    const glm::vec3 queue = PointDeLaCourbe(track, track.value[rang + 1]);

    // ⚠ LA SECTION SE POSE EN TRAVERS DU SEGMENT, et son cap se mesure sur la
    // seule projection AU SOL : le client ignore la hauteur pour l'orienter.
    // Ce cap, mesuré sur nos points, est l'opposé du sien : le quart de tour
    // qu'il y ajoute se RETRANCHE ici.
    const std::int32_t travers =
        HeadingOfVector(head.x - queue.x, head.z - queue.z) - kSparkTubeCrossQuarterTurn;
    const float vers_x = CosOfDegrees(travers);
    const float vers_z = SinOfDegrees(travers);

    // ⚠ LES DEUX TERMES VERTICAUX CHANGENT DE SIGNE, pas seulement le décalage :
    // le repère du client descend, si bien que la section RETOURNE aussi. Ne
    // retourner que le décalage donnerait un triangle pointé du mauvais côté.
    const auto arete = [&](float width, std::int32_t angle) {
      const float top = kSparkTubeSectionDrop - width * SinOfDegrees(angle);
      const float cote = width * CosOfDegrees(angle);
      return glm::vec3{cote * vers_x, top, cote * vers_z};
    };

    const auto place = [&](float width, std::int16_t alpha_du_tube,
                           std::array<std::uint8_t, 3> tint) {
      if (alpha_du_tube <= 0) {
        return;
      }
      for (std::size_t face = 0; face < kSparkTubeFaces; ++face) {
        const auto angle = static_cast<std::int32_t>(face) * kSparkTubeEdgeStepDegrees;
        const glm::vec3 before = arete(width, angle);
        const glm::vec3 after = arete(width, angle + kSparkTubeEdgeStepDegrees);
        EffectParticleQuadRequest request;
        // Une face : deux arêtes voisines, de la tête à la queue.
        request.corners = {track.anchor + head + after, track.anchor + queue + after,
                           track.anchor + queue + before, track.anchor + head + before};
        // ⚠ LES COINS SONT ABSOLUS : l'ancre de la piste est déjà un point du
        // monde, que le mouvement y recopie à chaque tic.
        request.absolute = true;
        request.alpha = static_cast<std::uint8_t>(alpha_du_tube);
        request.color = tint;
        request.v_at_a_and_b = 1.0F;
        request.v_at_c_and_d = 0.0F;
        quads.push_back(ComposeEffectParticleQuad(request, {}));
      }
    };

    place(track.lift_key, alpha, EffectSparkTubeColor(track.style, rang));
    // ⚠ ET SEUL LE STYLE TROIS MONTRE SON HALO. Le client ouvre sa boucle pour
    // tous les styles À PARTIR de trois, puis n'émet que pour le trois
    // EXACTEMENT : les styles au-delà calculent et ne montrent rien. Les deux
    // gardes disent donc la même chose, et une seule suffit.
    if (track.style == kSparkTubeHaloStyle) {
      place(track.lift_key * kSparkTubeHaloWidening,
            static_cast<std::int16_t>(alpha / kSparkTubeHaloAlphaDivisor), {0, 0, 255});
    }
  }
  return quads;
}

}  // namespace ro::render
