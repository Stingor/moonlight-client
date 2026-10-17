#include "ro/render/effect_node_world.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

#include "ro/render/effect_launch.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_scene.h"
#include "ro/render/map_effect_math.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

[[nodiscard]] std::uint8_t Clamped(float channel) {
  return static_cast<std::uint8_t>(std::clamp(channel, 0.0F, kEffectNodeChannelMax) + 0.5F);
}

// De combien l'abscisse de texture avance d'un segment au suivant.
constexpr float kTextureSpanPerSegment =
    1.0F / static_cast<float>(kEffectRingSegmentsPerTextureSpan);

// Un point du cercle, une fois orienté et posé.
//
// Le cercle s'étend en x et en z, la hauteur monte en y. L'orientation le place
// dans le monde, et sa quatrième colonne s'ajoute comme une translation.
//
// ⚠ L'ORIENTATION EST DÉJÀ DANS NOTRE REPÈRE, ET LE POINT DOIT L'ÊTRE AUSSI : la
// hauteur l'est, et la profondeur se retourne avec elle. N'en retourner qu'une
// ferait de l'anneau le miroir du sien. Le cylindre suit la même règle : son
// cercle, écrit dans le plan que le quart de tour replié redresse, retrouve
// après ce repli le plan de la bande.
[[nodiscard]] glm::vec3 OnCircle(const EffectRingOrientation& orientation, glm::vec3 anchor,
                                 float radius, float height, float degrees) {
  const float heading = DegreesToRadians(degrees);
  const glm::vec3 local{radius * std::cos(heading), height, -radius * std::sin(heading)};
  return anchor + orientation * glm::vec4{local, 1.0F};
}

// La BOUCLE COMMUNE aux deux dessinateurs d'anneau : deux cercles, et une
// bande de quads qui les joint.
//
// C'est celle du client, et elle est la même chez lui : `RingBand` et
// `RingCylinder` ne diffèrent que par les deux cercles qu'ils lui donnent. La
// dédoubler ferait diverger l'enroulement de la texture ou la fermeture du
// tour, qui sont une seule décision.
//
// Le premier cercle porte l'ordonnée de texture UN, le second ZÉRO.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeRingBetween(
    const EffectNode& node, glm::vec3 anchor, const EffectRingOrientation& orientation,
    float first_radius, float first_height, float second_radius, float second_height) {
  std::vector<EffectWorldQuad> quads;
  if (!(node.ring_step_degrees > 0.0F)) {
    // Un pas nul ou négatif ne ferait pas un anneau plat : il ferait un tour
    // qui n'avance jamais. Le client n'y survit que parce que son constructeur
    // pose toujours trente-six degrés.
    return quads;
  }

  const float step = node.ring_step_degrees;
  const std::array<std::uint8_t, 4> tint = {Clamped(node.color[0]), Clamped(node.color[1]),
                                            Clamped(node.color[2]), EffectNodeAlpha(node)};
  const auto texture = static_cast<std::size_t>(std::max(node.frame, 0));

  float texture_u = 0.0F;
  // Le dernier segment est celui qui COMMENCE avant le tour complet : c'est la
  // condition du client, et elle ferme l'anneau sans le faire se chevaucher.
  for (float degrees = 0.0F; degrees <= kEffectRingFullTurnDegrees - step; degrees += step) {
    const float next_degrees = degrees + step;
    const float next_u = texture_u + kTextureSpanPerSegment;

    EffectWorldQuad quad;
    // Les deux coins du premier cercle d'abord, puis les deux du second : c'est
    // l'ordre dans lequel le client émet ses deux triangles.
    quad.corners = {
        OnCircle(orientation, anchor, first_radius, first_height, degrees),
        OnCircle(orientation, anchor, first_radius, first_height, next_degrees),
        OnCircle(orientation, anchor, second_radius, second_height, next_degrees),
        OnCircle(orientation, anchor, second_radius, second_height, degrees),
    };
    quad.texture_uv = {glm::vec2{texture_u, 1.0F}, glm::vec2{next_u, 1.0F}, glm::vec2{next_u, 0.0F},
                       glm::vec2{texture_u, 0.0F}};
    quad.tint = tint;
    quad.blend_source = kEffectNodeBlendSource;
    quad.blend_destination = kEffectNodeBlendDestination;
    quad.texture = texture;
    quads.push_back(quad);

    // L'abscisse repart de zéro dès qu'elle ATTEINT un, et le segment qui la
    // franchit garde sa borne haute : sa portion de texture est donc entière,
    // et c'est le segment suivant qui recommence.
    texture_u = next_u >= 1.0F ? 0.0F : next_u;
  }
  return quads;
}

}  // namespace

EffectRingOrientation EffectNodeOriented(float yaw_degrees, float pitch_degrees) {
  const float cap = yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  const float pitch = pitch_degrees * std::numbers::pi_v<float> / 180.0F;
  const float cos_cap = std::cos(cap);
  const float sin_cap = std::sin(cap);
  const float cos_pitch = std::cos(pitch);
  const float sin_pitch = std::sin(pitch);
  // Les colonnes sont les IMAGES des axes locaux, comme pour l'orientation face
  // à la caméra : abscisse, hauteur, profondeur, puis un décalage nul —
  // l'ancre s'ajoute à part.
  return EffectRingOrientation{glm::vec3{cos_cap, 0.0F, -sin_cap},
                               glm::vec3{sin_cap * sin_pitch, cos_pitch, cos_cap * sin_pitch},
                               glm::vec3{sin_cap * cos_pitch, -sin_pitch, cos_cap * cos_pitch},
                               glm::vec3{0.0F, 0.0F, 0.0F}};
}

EffectRingOrientation EffectNodeOriented(float yaw_degrees, float pitch_degrees,
                                         float roll_degrees) {
  const EffectRingOrientation unrolled = EffectNodeOriented(yaw_degrees, pitch_degrees);
  if (roll_degrees == 0.0F) {
    return unrolled;
  }
  // 🔴 LE ROULIS S'APPLIQUE EN PREMIER AUX COORDONNÉES LOCALES : le client pose
  // la rotation autour de Z — qui ÉCRASE la matrice —, puis multiplie
  // l'inclinaison, puis le cap. Le composer en
  // dernier donnerait une figure juste à plat et fausse dès qu'un cap la tourne.
  //
  // La rotation autour de l'axe local Z laisse la profondeur intacte et fait
  // tourner l'abscisse et la hauteur l'une dans l'autre : c'est exactement ce
  // qui couche un quad né debout.
  const float roll = roll_degrees * std::numbers::pi_v<float> / 180.0F;
  const float cos_roll = std::cos(roll);
  const float sin_roll = std::sin(roll);
  return EffectRingOrientation{cos_roll * unrolled[0] + sin_roll * unrolled[1],
                               cos_roll * unrolled[1] - sin_roll * unrolled[0], unrolled[2],
                               glm::vec3{0.0F, 0.0F, 0.0F}};
}

float EffectClientMatrixDegrees(float client_degrees) {
  // La table rend des radians à l'approximation de π du client ; les ramener en
  // degrés par la même constante rend l'entrée lue, au flottant près.
  constexpr float kClientPi = 3.141592F;
  constexpr float kHalfTurnDegrees = 180.0F;
  return NativeTableRadians(static_cast<std::int32_t>(client_degrees)) * kHalfTurnDegrees /
         kClientPi;
}

EffectRingOrientation EffectNodeOrientedAsClient(float yaw_degrees, float pitch_degrees,
                                                 float roll_degrees, float pitch_fold_degrees) {
  // 🔴 LA MATRICE DU CLIENT SE CONJUGUE PAR `S` : le cap et le roulis changent
  // de signe, l'inclinaison non. Ses trois rotations sont celles d'un repère
  // direct, et `S·Rz(r)·S = Rz(−r)` comme `S·Ry(c)·S = Ry(−c)`, alors que
  // `S·Rx(p)·S = Rx(p)`. Le roulis gardé tel quel couchait un quad du mauvais
  // côté de son axe, sa texture retournée.
  const float cap = EffectClientYawInWorld(EffectClientMatrixDegrees(-yaw_degrees));
  return EffectNodeOriented(cap, EffectClientMatrixDegrees(pitch_degrees) - pitch_fold_degrees,
                            -EffectClientMatrixDegrees(roll_degrees));
}

EffectRingOrientation EffectNodeFacingCamera(const Camera& camera, float pitch_degrees) {
  // Les colonnes sont les IMAGES des axes locaux : l'abscisse locale est la
  // normale du quad, l'ordonnée sa hauteur, la troisième sa largeur. La
  // quatrième colonne ne décale rien — l'ancre s'ajoute à part.
  //
  // ⚠ C'EST LA MATRICE QUE LE CLIENT BÂTIT DE SA CAMÉRA, ramenée dans notre
  // repère par `S` comme celle qu'il bâtit des angles du nœud : la normale
  // regarde VERS l'œil et la largeur locale va vers la GAUCHE de l'écran. Les
  // points locaux du quad passent donc par `S` sur les deux branches, et la
  // base reste directe.
  //
  // Le lacet et l'altitude du client sont les opposés des nôtres ; la somme du
  // lacet et du quart de tour se fait en flottant AVANT la troncature.
  const float client_yaw = -camera.yaw_degrees + kEffectFacingCameraYawQuarterTurnDegrees;
  const float client_altitude = -camera.pitch_degrees;
  // Rotation autour de z de l'altitude, puis autour de y du lacet : c'est le
  // roulis puis le cap d'`EffectNodeOriented`, qui les compose dans cet ordre.
  // `S` nie les deux angles.
  const glm::mat3 camera_turn{
      EffectNodeOriented(EffectClientYawInWorld(EffectClientMatrixDegrees(client_yaw)), 0.0F,
                         -EffectClientMatrixDegrees(client_altitude))};
  // L'inclinaison tourne les axes LOCAUX avant la caméra : elle multiplie donc
  // à droite. C'est la rotation autour de l'abscisse qu'`EffectNodeOriented`
  // bâtit à cap nul, et la conjugaison par `S` ne la change pas.
  const glm::mat3 node_tilt{EffectNodeOriented(0.0F, EffectClientMatrixDegrees(pitch_degrees))};
  const glm::mat3 turned = camera_turn * node_tilt;
  return EffectRingOrientation{turned[0], turned[1], turned[2], glm::vec3{0.0F}};
}

float EffectOrientedQuadYawDegrees(std::uint32_t flags, float node_yaw_degrees, float body_degrees,
                                   float node_yaw_step_degrees) {
  if ((flags & kEffectOrientedQuadYawFollowsBody) == 0U) {
    return node_yaw_degrees;
  }
  // L'angle du corps est un angle du client, que `S` nie ; le pas est déjà
  // dans notre repère.
  return EffectClientYawInWorld(body_degrees) + node_yaw_step_degrees;
}

EffectRingOrientation EffectNodeLeaningByCameraAltitude(const Camera& camera, float body_degrees) {
  // L'altitude du client est l'opposé de notre tangage, et `S` nie la rotation
  // autour de z : il reste notre tangage, PRIS POUR DES RADIANS, que l'on rend
  // en degrés pour le roulis d'`EffectNodeOriented`.
  constexpr float kDegreesPerRadian = 180.0F / std::numbers::pi_v<float>;
  const float leaning_roll_degrees = camera.pitch_degrees * kDegreesPerRadian;
  // L'angle du corps est un angle du CLIENT, lu dans sa table ; `S` le nie.
  const float cap = EffectClientYawInWorld(EffectClientMatrixDegrees(body_degrees));
  return EffectNodeOriented(cap, 0.0F, leaning_roll_degrees);
}

EffectRingOrientation EffectOrientedQuadFacing(std::uint32_t flags, const Camera& camera,
                                               float yaw_degrees, float pitch_degrees,
                                               float roll_degrees, float body_degrees,
                                               float yaw_step_degrees) {
  if (EffectOrientedQuadLeansByCameraAltitude(flags)) {
    return EffectNodeLeaningByCameraAltitude(camera, body_degrees);
  }
  return EffectNodeFacesCamera(flags)
             ? EffectNodeFacingCamera(camera, pitch_degrees)
             : EffectNodeOriented(
                   EffectOrientedQuadYawDegrees(flags, yaw_degrees, body_degrees, yaw_step_degrees),
                   pitch_degrees, roll_degrees);
}

glm::vec3 EffectWorldQuadSortPoint(const EffectWorldQuad& quad) {
  const std::array<glm::vec3, 4>& corners = quad.corners;
  switch (quad.sort_point) {
    case EffectQuadSortPoint::kFirstEdge:
      return (corners[0] + corners[1]) / 2.0F;
    case EffectQuadSortPoint::kFirstCorner:
      return corners[0];
    case EffectQuadSortPoint::kCentre:
      break;
  }
  return (corners[0] + corners[1] + corners[2] + corners[3]) / 4.0F;
}

std::vector<EffectWorldQuad> ComposeEffectNodeRingBand(const EffectNode& node, glm::vec3 anchor,
                                                       const EffectRingOrientation& orientation) {
  std::vector<EffectWorldQuad> quads =
      ComposeRingBetween(node, anchor, orientation, node.radius_at_anchor.half_size, 0.0F,
                         node.radius_at_height.half_size, node.half_height.half_size);
  // LA BANDE SE RANGE PAR SON CERCLE DE L'ANCRE : ses deux triangles ont pour
  // premier sommet l'un des deux coins de ce cercle. Voir `EffectQuadSortPoint`.
  for (EffectWorldQuad& quad : quads) {
    quad.sort_point = EffectQuadSortPoint::kFirstEdge;
  }
  return quads;
}

std::vector<EffectWorldQuad> ComposeEffectNodeRingCylinder(
    const EffectNode& node, glm::vec3 anchor, const EffectRingOrientation& orientation) {
  const float outer = node.ring_radius.half_size;
  // LA LARGEUR SE DÉDUIT : le client écrit à chaque tic le plus petit du rayon
  // à l'ancre et du rayon de la couronne. Tant qu'une seule trace est posée,
  // la stocker comme il le fait ou la recalculer ici revient au même — et la
  // recalculer évite un champ que l'horloge devrait entretenir pour un seul
  // dessinateur.
  // 🔴 UN BIT LA REMPLIT, ET C'EST UN DISQUE. Les drapeaux du nœud portent un
  // bit 0x1 qui, sur CE dessinateur, veut dire « pleine » : la
  // couronne va alors du centre au bord au lieu d'être creusée jusqu'au rayon à
  // l'ancre. Le socle d'un point de téléportation est ainsi — il n'a pas de
  // rayon intérieur, et sans ce bit sa largeur vaudrait zéro : une bande
  // d'épaisseur nulle, qui ne se voit pas.
  //
  // ⚠ UN DRAPEAU NE VEUT DIRE QUELQUE CHOSE QUE POUR SON DESSINATEUR : le même
  // bit sert ailleurs à pendre un quad par son bord haut.
  const float width =
      EffectNodeFillsRing(node.flags) ? outer : std::min(node.radius_at_anchor.half_size, outer);
  // ⚠ UN RAYON À L'ANCRE NÉGATIF N'EST PAS UNE ERREUR : la largeur l'est alors
  // aussi, et le bord « intérieur » passe AU-DELÀ du rayon. La bande s'ouvre vers
  // l'extérieur — l'onde jaune de Sight Trasher, dix unités hors de son rayon.
  // LES DEUX CERCLES SONT AU MÊME PLAN, et c'est ce qui fait la figure plate :
  // le client multiplie par zéro les colonnes qui porteraient la hauteur.
  return ComposeRingBetween(node, anchor, orientation, outer - width, 0.0F, outer, 0.0F);
}

std::vector<EffectWorldQuad> ComposeEffectNodeCrossedPlanes(
    const EffectNode& node, glm::vec3 anchor, const EffectRingOrientation& orientation) {
  // ⚠ CROISÉES, comme les huit points du client : sa demi-hauteur porte
  // l'extension du premier axe, la demi-largeur celle
  // de l'axe partagé.
  const float along_first = node.half_height.half_size;
  const float along_shared = node.half_width.half_size;
  const std::array<std::uint8_t, 4> tint = {Clamped(node.color[0]), Clamped(node.color[1]),
                                            Clamped(node.color[2]), EffectNodeAlpha(node)};
  const auto texture = static_cast<std::size_t>(std::max(node.frame, 0));

  // Les deux plans ne diffèrent que par l'axe qu'ils écartent : le premier
  // s'étend en abscisse, le second en ordonnée, et tous deux le long du
  // troisième. Les écrire deux fois ferait diverger leurs coins.
  //
  // ⚠ CES POINTS SONT CEUX DU CLIENT, et l'orientation est déjà ramenée dans
  // notre repère : ils y passent donc aussi, ordonnée et profondeur retournées.
  // Les poser tels quels mettait la texture à l'envers sur les deux plans.
  const auto plan = [&](bool along_x) {
    const auto place = [&](float first, float shared) {
      const glm::vec3 local =
          along_x ? glm::vec3{first, 0.0F, -shared} : glm::vec3{0.0F, -first, -shared};
      return anchor + orientation * glm::vec4{local, 1.0F};
    };
    EffectWorldQuad quad;
    // L'ordre du client, et ses coordonnées de texture : la première extension
    // porte l'ordonnée de texture, l'axe partagé porte l'abscisse.
    quad.corners = {place(-along_first, along_shared), place(-along_first, -along_shared),
                    place(along_first, -along_shared), place(along_first, along_shared)};
    quad.texture_uv = {glm::vec2{0.0F, 1.0F}, glm::vec2{1.0F, 1.0F}, glm::vec2{1.0F, 0.0F},
                       glm::vec2{0.0F, 0.0F}};
    quad.tint = tint;
    quad.blend_source = kEffectNodeBlendSource;
    quad.blend_destination = kEffectNodeBlendDestination;
    quad.texture = texture;
    return quad;
  };

  return {plan(true), plan(false)};
}

std::vector<EffectWorldQuad> ComposeEffectNodeOrientedQuad(
    const EffectNode& node, glm::vec3 anchor, const EffectRingOrientation& orientation) {
  const float half_height = node.half_height.half_size;
  const float half_width = node.half_width.half_size;

  // L'ANCRAGE : centré sur son point, posé au-dessus, ou pendu en doublant sa
  // demi-hauteur. Le client obtient les deux derniers en ramenant une borne à
  // zéro plutôt qu'en décalant le centre.
  //
  // ⚠ SON REPÈRE DESCEND ET LE NÔTRE MONTE : ce qu'il pose en ordonnée négative
  // se pose chez nous en positive. Un quad centré est symétrique et ne s'en
  // aperçoit pas ; les deux autres, si.
  float bottom = -half_height;
  float top = half_height;
  if ((node.flags & kEffectNodeQuadHangsFromAnchor) != 0) {
    bottom = 0.0F;
    top = 2.0F * half_height;
  } else if ((node.flags & kEffectNodeQuadSitsAboveAnchor) != 0) {
    bottom = 0.0F;
    top = half_height;
  }

  // 🔴 UN QUAD COUCHÉ SE DÉGAGE DU TERRAIN, et un quad debout ne bouge pas. La
  // mesure est la NORMALE du quad — l'image de son abscisse locale : verticale
  // quand la figure est à plat, horizontale quand elle est dressée. Le dégagement
  // suit donc la pose, sans seuil ni cas particulier : l'anneau du classement PVP
  // le prend en entier, les quatre murs d'EF_GLASSWALL rien du tout.
  //
  // Voir `kEffectFlatQuadClearance` : c'est notre épreuve de profondeur qui
  // l'exige, pas le client.
  const glm::vec3 placed =
      anchor + glm::vec3{0.0F, kEffectFlatQuadClearance * std::abs(orientation[0].y), 0.0F};
  const auto place = [&](float up, float across) {
    return placed + orientation * glm::vec4{glm::vec3{0.0F, up, across}, 1.0F};
  };

  // ⚠ LES COINS SONT CEUX DU CLIENT PASSÉS PAR `S`, comme pour les plans
  // croisés : l'orientation est déjà la conjuguée de sa matrice, que celle-ci
  // vienne des angles du nœud ou de la caméra. Le coin d'abscisse de texture
  // nulle, en haut, a chez lui la largeur POSITIVE : il a donc chez nous la
  // négative. Ne retourner que la hauteur faisait du quad le miroir du sien.
  EffectWorldQuad quad;
  quad.corners = {place(top, -half_width), place(top, half_width), place(bottom, half_width),
                  place(bottom, -half_width)};
  quad.texture_uv = {glm::vec2{0.0F, node.texture_v_min}, glm::vec2{1.0F, node.texture_v_min},
                     glm::vec2{1.0F, node.texture_v_max}, glm::vec2{0.0F, node.texture_v_max}};
  quad.tint = {Clamped(node.color[0]), Clamped(node.color[1]), Clamped(node.color[2]),
               EffectNodeAlpha(node)};
  quad.blend_source = kEffectNodeBlendSource;
  quad.blend_destination = kEffectNodeBlendDestination;
  quad.texture = static_cast<std::size_t>(std::max(node.frame, 0));
  return {quad};
}

std::vector<std::size_t> EffectWorldQuadsBackToFront(std::span<const EffectWorldQuad> quads,
                                                     glm::vec3 eye) {
  std::vector<std::size_t> rangs;
  std::vector<float> eloignement;
  EffectWorldQuadsBackToFront(quads, eye, rangs, eloignement);
  return rangs;
}

void EffectWorldQuadsBackToFront(std::span<const EffectWorldQuad> quads, glm::vec3 eye,
                                 std::vector<std::size_t>& order, std::vector<float>& distances) {
  // La profondeur se calcule UNE FOIS par quad, et non à chaque comparaison :
  // un tri en fait beaucoup plus que d'éléments.
  distances.clear();
  distances.reserve(quads.size());
  for (const EffectWorldQuad& quad : quads) {
    const glm::vec3 offset = EffectWorldQuadSortPoint(quad) - eye;
    distances.push_back(glm::dot(offset, offset));
  }

  order.resize(quads.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  // ⚠ STABLE, ET DÉCROISSANT : le plus LOIN d'abord, et à égalité l'ordre de la
  // figure. Voir l'en-tête.
  std::ranges::stable_sort(order, [&distances](std::size_t left, std::size_t right) {
    return distances[left] > distances[right];
  });
}

void OrderEffectPaintedQuads(std::vector<EffectPaintedQuad>& painted, glm::vec3 eye,
                             glm::vec3 view_forward) {
  for (EffectPaintedQuad& entry : painted) {
    entry.view_depth = glm::dot(EffectWorldQuadSortPoint(*entry.quad) - eye, view_forward);
  }
  std::ranges::stable_sort(painted,
                           [](const EffectPaintedQuad& left, const EffectPaintedQuad& right) {
                             if (left.bucket != right.bucket) {
                               return left.bucket < right.bucket;
                             }
                             return left.view_depth > right.view_depth;
                           });
}

namespace {

// Le tour complet, en degrés : l'angle d'un exemplaire s'y tire.
constexpr std::uint32_t kFullTurnDegrees = 360;

}  // namespace

glm::vec3 EffectNodeInstanceOffset(EffectNodeInstancePlacement placement, std::uint16_t count,
                                   std::uint16_t index, float spacing, float spacing_high,
                                   float yaw_degrees, std::uint32_t& generator,
                                   std::int32_t spacing_values) {
  switch (placement) {
    case EffectNodeInstancePlacement::kMirroredPair: {
      // ⚠ LE LONG DE LA NORMALE DU NŒUD, et non d'un axe du monde : c'est
      // l'abscisse locale de `EffectNodeOriented`, celle que le cap fait
      // tourner. Le client multiplie une colonne de sa matrice de lacet par
      // plus ou moins l'écart ; deux murs tournés d'un quart de tour se posent
      // donc sur deux axes perpendiculaires, et la boîte se ferme.
      //
      // Le rang pair passe d'un côté, l'impair de l'autre. Rien ne se tire.
      const float cap = yaw_degrees * std::numbers::pi_v<float> / 180.0F;
      const float sens = index % 2 == 0 ? 1.0F : -1.0F;
      return glm::vec3{std::cos(cap), 0.0F, -std::sin(cap)} * (sens * spacing);
    }
    case EffectNodeInstancePlacement::kSquareGrid: {
      // ⚠ LE CÔTÉ EST LA RACINE DU COMPTE, et la grille est CENTRÉE. Le client
      // boucle de moins dix à dix par pas de cinq, soit cinq rangs de cinq :
      // vingt-cinq. Compter les rangs à partir de zéro la poserait tout entière
      // d'un côté du lanceur.
      const auto cote =
          static_cast<std::uint16_t>(std::lround(std::sqrt(static_cast<double>(count))));
      if (cote == 0) {
        return {};
      }
      const auto column = static_cast<float>(index % cote);
      const auto rang = static_cast<float>(index / cote);
      const float start = -0.5F * static_cast<float>(cote - 1) * spacing;
      // ⚠ LA BOUCLE EXTÉRIEURE DU CLIENT PARCOURT SA PROFONDEUR, l'intérieure son
      // abscisse, de moins à plus. Sa profondeur est la nôtre au signe près :
      // sans ce retournement, le premier rang de tirages tomberait sur le
      // dernier rang de cases.
      return EffectClientOffsetInWorld(start + column * spacing, 0.0F, start + rang * spacing);
    }
    case EffectNodeInstancePlacement::kRing: {
      // ⚠ L'ANGLE SE TIRE, il ne se répartit pas. Le client tire le sien sur le
      // tour entier pour chaque exemplaire : six pointes ne sont donc jamais
      // également écartées, et les poser à soixante degrés d'intervalle ferait
      // une couronne trop régulière pour être la sienne.
      const auto cap = static_cast<float>(NextEffectNodeDraw(generator) % kFullTurnDegrees);
      const float radians = cap * std::numbers::pi_v<float> / 180.0F;
      // ⚠ ET LE RAYON SE TIRE AUSSI, quand le patron porte une borne haute : le
      // client écrit « trois plus un tirage sur vingt-cinq » pour les cristaux
      // de Storm Gust, un DISQUE et non un cercle. Sans borne, rien ne se tire
      // et la suite du générateur reste celle des couronnes à rayon fixe.
      // ⚠ AVEC SON NOMBRE DE VALEURS : voir `EffectNodeRange::values`.
      const float ring_radius =
          DrawEffectNodeRange(EffectNodeRange{spacing, spacing_high, spacing_values}, generator);
      // 🔴 L'ABSCISSE PORTE LE SINUS ET LA PROFONDEUR LE COSINUS RETRANCHÉ, dans
      // le repère du client — `(sin, 0, −cos) · rayon` : ses deux tables de
      // degrés ont leurs noms échangés, celle qu'on croirait du cosinus rend le
      // sinus. Dans le nôtre, la profondeur change de signe.
      return {std::sin(radians) * ring_radius, 0.0F, std::cos(radians) * ring_radius};
    }
    case EffectNodeInstancePlacement::kDrawnDepthLine: {
      // L'ANGLE SE TIRE POUR CHAQUE EXEMPLAIRE, celui du milieu compris, et seul
      // son cosinus sert : le client écrit `−rang × table(a)` en profondeur, et
      // la table qu'il nomme sinus rend le cosinus. Dans notre repère, la
      // profondeur change de signe.
      const auto cap = static_cast<float>(NextEffectNodeDraw(generator) % kFullTurnDegrees);
      const float rang = static_cast<float>(index) - 0.5F * static_cast<float>(count - 1);
      return {0.0F, 0.0F, rang * spacing * std::cos(cap * std::numbers::pi_v<float> / 180.0F)};
    }
    case EffectNodeInstancePlacement::kSingle:
      break;
  }
  return {};
}

glm::vec3 EffectBirthSweepOffset(float radius, float degrees_per_tick, std::int32_t tick) {
  if (radius <= 0.0F) {
    return {};
  }
  // ⚠ L'ANGLE SE TRONQUE AVANT LE SINUS : le client indexe une table de
  // trigonométrie par degré ENTIER, par troncature. L'abscisse porte
  // le SINUS, la profondeur le COSINUS RETRANCHÉ, dans son repère — d'où le
  // changement de repère à la fin.
  //
  // 🔴 LE BÂTISSEUR DE SIGHT pose ainsi l'écart `(sin(θ) · 15, −cos(θ) · 15)` :
  // échanger sinus et cosinus parcourrait le cercle dans l'autre sens, et d'un
  // quart de tour décalé.
  //
  // ⚠ ET UN ANGLE NÉGATIF S'Y LIT UN DEGRÉ DE TRAVERS — moins cinq comme trois
  // cent cinquante-six : voir `NativeTableRadians`.
  const auto angle_degrees =
      static_cast<std::int32_t>(std::trunc(static_cast<float>(tick) * degrees_per_tick));
  const float radians = NativeTableRadians(angle_degrees);
  return EffectClientOffsetInWorld(std::sin(radians) * radius, 0.0F, -std::cos(radians) * radius);
}

glm::vec3 EffectNodeLeanAxis(float lean_degrees, float yaw_degrees) {
  const float penchant = lean_degrees * std::numbers::pi_v<float> / 180.0F;
  const float lacet = yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  return glm::vec3{-std::cos(lacet) * std::sin(penchant), std::cos(penchant),
                   std::sin(lacet) * std::sin(penchant)};
}

EffectRingOrientation EffectNodeLeaningOrientation(float lean_degrees, float yaw_degrees) {
  const float penchant = lean_degrees * std::numbers::pi_v<float> / 180.0F;
  const float lacet = yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  const float cp = std::cos(penchant);
  const float sp = std::sin(penchant);
  const float cl = std::cos(lacet);
  const float sl = std::sin(lacet);
  // Le penchant fait basculer la verticale vers l'abscisse ; le lacet tourne le
  // tout autour de la verticale. Les colonnes sont les images des axes locaux.
  const glm::vec3 x_local{cl * cp, sp, -sl * cp};
  const glm::vec3 z_local{sl, 0.0F, cl};
  // ⚠ LA SECONDE COLONNE VIENT DE `EffectNodeLeanAxis`, elle n'est pas réécrite
  // ici. C'est le même axe, et l'avoir en deux exemplaires le laisserait dériver
  // le jour où l'un des deux serait corrigé : la pointe monterait alors ailleurs
  // qu'elle ne penche.
  return EffectRingOrientation{x_local, EffectNodeLeanAxis(lean_degrees, yaw_degrees), z_local,
                               glm::vec3{0.0F, 0.0F, 0.0F}};
}

}  // namespace ro::render
