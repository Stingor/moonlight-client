#include "skill_chain.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace ro::ui {
namespace {

// ── LES FLÈCHES DE CHAÎNE ───────────────────────────────────────────────────
//
// Les mêmes teintes que les cadres de survol, et pour cause : c'est la même
// chose dite deux fois — l'ambre est ce qu'il faut AVANT, le bleu ce que ça
// OUVRE.
//
// ⚠ L'OPACITÉ ET L'ÉPAISSEUR TOMBENT AVEC LA DISTANCE, et c'est ce qui rend
// l'ordre de la chaîne lisible d'un coup d'œil : sans cela, six traits de même
// poids ne disent pas lequel vient en premier. Les planchers empêchent que le
// dernier maillon disparaisse.
constexpr int kUpstreamRed = 255;
constexpr int kUpstreamGreen = 205;
constexpr int kUpstreamBlue = 105;
constexpr int kUpstreamAlpha = 235;
constexpr float kUpstreamThickness = 2.5F;
constexpr int kDownstreamRed = 120;
constexpr int kDownstreamGreen = 160;
constexpr int kDownstreamBlue = 255;
constexpr int kDownstreamAlpha = 200;
constexpr float kDownstreamThickness = 2.0F;
constexpr int kChainAlphaFade = 35;
constexpr int kChainAlphaFloor = 85;
constexpr float kChainThicknessFade = 0.25F;
constexpr float kChainThicknessFloor = 1.2F;
// 🔴 GARDE-FOU : aucun arbre de métier n'est si profond, et une donnée qui
// boucle ne doit pas faire tourner le dessin en rond.
constexpr int kChainDepthMost = 6;
// En deçà, les deux cases se touchent presque : il n'y a rien à tracer.
constexpr float kChainLeastGapPixels = 8.0F;
constexpr float kChainHeadPixels = 8.0F;

// Cette case réclame-t-elle celle-là ?
[[nodiscard]] bool RequiresSkill(const SkillCellView& cell, std::uint16_t skill_id) {
  for (const SkillNeedView& need : cell.needs) {
    if (need.skill_id == skill_id) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] const SkillCellView* CellOf(std::span<const SkillCellView> cells,
                                          std::uint16_t skill_id) {
  for (const SkillCellView& cell : cells) {
    if (cell.skill_id == skill_id) {
      return &cell;
    }
  }
  return nullptr;
}

// LA RÉDUCTION TRANSITIVE d'un lien « prérequis -> ce qu'il débloque ».
//
// 🔴 LA LISTE DU CLIENT PORTE DES RACCOURCIS : Bowling Bash réclame Bash ET
// Magnum Break, alors que Magnum Break réclame déjà Bash. Cent trente-six arêtes
// redondantes ont été comptées dans `skillinfolist`. Tracées telles quelles,
// elles doubleraient le même chemin. On saute donc le trait DIRECT quand un
// AUTRE prérequis de la même case réclame déjà celui-ci.
//
// ⚠ CES RACCOURCIS NE FONT PAS DE LA LISTE UNE FERMETURE TRANSITIVE, et c'est le
// contraire qu'on pourrait croire : elle est aussi INCOMPLÈTE — d'où la
// réservation qui remonte la chaîne elle-même, dans `sim`.
}  // namespace

bool SkillEdgeIsRedundant(std::span<const SkillCellView> cells, const SkillCellView& dependent,
                          std::uint16_t prereq_id) {
  for (const SkillNeedView& need : dependent.needs) {
    if (need.skill_id == prereq_id) {
      continue;
    }
    const SkillCellView* other = CellOf(cells, need.skill_id);
    if (other != nullptr && RequiresSkill(*other, prereq_id)) {
      return true;
    }
  }
  return false;
}

namespace {

// UNE FLÈCHE, du bord d'une case au bord d'une autre.
void DrawChainArrow(ImDrawList* into, ImVec2 from, ImVec2 to, ImU32 color, float thickness,
                    float trim) {
  ImVec2 way(to.x - from.x, to.y - from.y);
  const float span = std::sqrt(way.x * way.x + way.y * way.y);
  if (span <= trim * 2.0F + kChainLeastGapPixels) {
    return;
  }
  way.x /= span;
  way.y /= span;
  const ImVec2 start(from.x + way.x * trim, from.y + way.y * trim);
  const ImVec2 end(to.x - way.x * trim, to.y - way.y * trim);
  into->AddLine(start, end, color, thickness);
  // La normale écarte les deux ailes de la pointe.
  const ImVec2 side(-way.y, way.x);
  const float head = kChainHeadPixels;
  into->AddTriangleFilled(end,
                          ImVec2(end.x - way.x * head + side.x * head * 0.5F,
                                 end.y - way.y * head + side.y * head * 0.5F),
                          ImVec2(end.x - way.x * head - side.x * head * 0.5F,
                                 end.y - way.y * head - side.y * head * 0.5F),
                          color);
}

// LA BRANCHE ENTIÈRE, EN LARGEUR, depuis la case survolée.
//
// 🔴 TOUT LE CHEMIN, PAS LE RANG VOISIN. C'est le chemin ENTIER qu'on cherche en
// survolant une compétence : « d'où vient-elle, et qu'ouvre-t-elle ? ». S'arrêter
// au premier rang n'en montrait qu'un bout.
//
// ⚠ CHAQUE CASE N'EST DÉVELOPPÉE QU'UNE FOIS : le graphe a des raccourcis, et
// sans marquage une même case serait reprise à chaque profondeur.
//
}  // namespace

// 🔴 LA FLÈCHE VA TOUJOURS DU PRÉREQUIS VERS CE QU'IL DÉBLOQUE, quel que soit le
// sens du parcours : c'est le sens de lecture de l'arbre, pas celui du survol.
void WalkChain(ImDrawList* into, const SkillGroupView& group, std::span<const ImVec2> centers,
               std::span<const char> shown, std::size_t focus, bool upstream, float trim) {
  const std::span<const SkillCellView> cells = group.cells;
  const int red = upstream ? kUpstreamRed : kDownstreamRed;
  const int green = upstream ? kUpstreamGreen : kDownstreamGreen;
  const int blue = upstream ? kUpstreamBlue : kDownstreamBlue;
  const int alpha = upstream ? kUpstreamAlpha : kDownstreamAlpha;
  const float thickness = upstream ? kUpstreamThickness : kDownstreamThickness;

  // ⚠ DES OCTETS, PAS DES BOOLÉENS : `std::vector<bool>` est un champ de bits,
  // sans stockage contigu — il ne se prête à aucune vue.
  std::vector<char> seen(cells.size(), 0);
  std::vector<std::pair<std::size_t, int>> queue;
  queue.reserve(cells.size());
  queue.emplace_back(focus, 0);
  seen[focus] = 1;
  for (std::size_t at = 0; at < queue.size(); ++at) {
    const auto [current, depth] = queue[at];
    if (depth >= kChainDepthMost) {
      continue;
    }
    for (std::size_t rank = 0; rank < cells.size(); ++rank) {
      if (rank == current) {
        continue;
      }
      // Amont : `rank` est un prérequis de `current`. Aval : `rank` réclame
      // `current`.
      const std::size_t dependent = upstream ? current : rank;
      const std::size_t required = upstream ? rank : current;
      if (!RequiresSkill(cells[dependent], cells[required].skill_id)) {
        continue;
      }
      if (SkillEdgeIsRedundant(cells, cells[dependent], cells[required].skill_id)) {
        continue;
      }
      if (shown[dependent] != 0 && shown[required] != 0) {
        DrawChainArrow(
            into, centers[required], centers[dependent],
            IM_COL32(red, green, blue, std::max(kChainAlphaFloor, alpha - depth * kChainAlphaFade)),
            std::max(kChainThicknessFloor,
                     thickness - static_cast<float>(depth) * kChainThicknessFade),
            trim);
      }
      if (seen[rank] == 0) {
        seen[rank] = 1;
        queue.emplace_back(rank, depth + 1);
      }
    }
  }
}

}  // namespace ro::ui
