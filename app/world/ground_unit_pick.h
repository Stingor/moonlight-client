#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include <glm/glm.hpp>

#include "ro/render/actor_pick.h"
#include "ro/render/camera.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world_fwd.h"

#include "world/armed_cursor.h"

// CE QU'UNE UNITÉ AU SOL OFFRE À LA SOURIS : une emprise de clic, l'épée du
// curseur, et le clic qui la frappe.
namespace ro::sim {
struct Actor;
}  // namespace ro::sim

namespace moonclient {

// Une unité qui porte une emprise de clic, posée au point du sol de sa case.
// Son rectangle d'écran se calcule au moment de désigner : voir
// `ro::render::GroundUnitScreenRect`.
struct GroundUnitPick {
  std::uint32_t id = 0;
  std::uint32_t unit_id = 0;
  glm::vec3 ground{0.0F};
};

// L'UNITÉ PORTE-T-ELLE UNE EMPRISE DE CLIC ? Le client ne la donne qu'à
// quelques unités — les blocs de glace, les pièges posés, quelques murs et
// plantes — et toute autre unité laisse passer la souris jusqu'au sol.
[[nodiscard]] bool GroundUnitTakesPick(std::uint32_t unit_id);

// LE CLIC SUR ELLE ATTAQUE-T-IL, et le curseur y montre-t-il l'épée ? Hors
// siège, seules les unités qu'on détruit à coups : un bloc d'Ice Wall en est
// une. En siège, l'épée paraît aussi sur les pièges.
[[nodiscard]] bool GroundUnitClickAttacks(std::uint32_t unit_id);
[[nodiscard]] bool GroundUnitShowsSword(std::uint32_t unit_id, bool siege_map);

// LES UNITÉS QUE LA SOURIS PEUT DÉSIGNER à cette image : celles qui portent
// une emprise ET qui se montrent. Une unité cachée n'en dépose aucune, modèle
// ou pas — le client ne pose son rectangle qu'au dessin de ce qu'on voit.
[[nodiscard]] bool GroundUnitOffersPick(const ro::sim::World& world, std::uint32_t id);

// L'unité que désigne un pixel, par son identifiant ; rien quand aucune
// emprise ne le contient. Quand plusieurs se recouvrent, la plus proche de
// l'œil gagne. `floor_pixels` est le plancher de la famille : voir
// `ro::render::PickFloorPixels`.
[[nodiscard]] std::optional<std::uint32_t> PickGroundUnit(const ro::render::Camera& camera,
                                                          float aspect_ratio,
                                                          std::span<const GroundUnitPick> picks,
                                                          float screen_x, float screen_y,
                                                          float floor_pixels, float viewport_width,
                                                          float viewport_height);

// L'UNITÉ PASSE-T-ELLE DEVANT L'ACTEUR que la souris désigne aussi ?
//
// Le client range tous les rectangles cliquables dans un même arbre et rend,
// sous le pixel, celui dont la profondeur PLUS sa marge est la plus grande. La
// marge d'une unité vaut 350 ; celle d'un joueur 100, d'un PNJ 200, d'un
// familier 50, d'un objet au sol 300, d'un monstre 400. La profondeur n'est
// qu'un inverse de distance, bien plus petit qu'un écart de marges : seul un
// monstre passe donc devant une unité.
[[nodiscard]] bool GroundUnitOutranks(const ro::sim::Actor* hovered_actor);

// CE QUE FAIT UN CLIC GAUCHE qui tombe sur l'emprise d'une unité.
enum class GroundUnitClick : std::uint8_t {
  // L'unité ne prend pas le clic : aucune sous le pixel, un acteur passe devant,
  // ou une compétence armée le garde pour elle.
  kNotTaken,
  // Le personnage s'approche puis frappe l'unité : un bloc d'Ice Wall.
  kAttack,
  // Le clic S'ARRÊTE sur l'emprise, sans rien faire — ni marche, ni acteur : un
  // piège hors siège. Le client ne frappe que les unités qu'on détruit.
  kSwallowed,
  // La compétence armée PART sur l'unité : Remove Trap, Spring Trap. Le client
  // l'inscrit comme une visée d'acteur — approcher à portée, puis lancer —, avec
  // l'unité pour cible, quelle qu'elle soit.
  kCastSkill,
};
// `aim` est ce que la compétence armée attend : seule la visée d'un piège prend
// une unité, les autres gardent le clic pour elles.
[[nodiscard]] GroundUnitClick GroundUnitClickOf(std::optional<std::uint32_t> picked_unit_id,
                                                const ro::sim::Actor* hovered_actor, SkillAim aim);

// CE QUI SE PEINT SUR LE SOL, après le terrain et le décor, dans l'ordre même
// où l'image le peint : le dessin du monde parcourt cette table.
//
// ⚠ LE CADRE DE LA CASE PASSE AVANT LES UNITÉS AU SOL : le client le range avec
// le sol, et le modèle d'un piège posé dessus le recouvre, comme un personnage.
//
// ⚠ LA TRACE DU GUIDAGE PASSE ENTRE LES ZONES DE SORT ET LE CADRE : elle est
// une marque de sol comme eux, sous les acteurs ; le cadre, qui dit où mènerait
// un clic, doit rester lisible par-dessus une flèche.
//
// 🔴 L'EAU N'EN EST PAS. Elle se peint après les corps et passe par-dessus ces
// marques comme par-dessus des pieds : posées au fond, elles en prennent la
// teinte. Le jeu les trie avec ses cellules d'eau, et une cellule plus haute que
// la marque passe après elle.
//
// ⚠ LE MESSAGE D'UN GRAFFITI PASSE EN PREMIER : il est tracé SUR le sol, et
// une zone de sort ou le cadre posés là doivent rester lisibles par-dessus.
//
// ⚠ SAUF LE QUADRILLAGE DE GREYWORLD, qui le précède : il habille le sol
// lui-même, et toute autre marque doit se lire par-dessus.
//
// ⚠ LES ZONES DE L'IA DU COMPAGNON PASSENT AVANT CELLES DES SORTS : elles
// entourent le maître en permanence quand on les garde, et une zone de sort,
// brève, doit se lire par-dessus.
enum class GroundPaint : std::uint8_t {
  kSkillAreas,
  kNaviTrail,
  kCellCursor,
  kGroundUnits,
  kGraffiti,
  kGreyWorld,
  kCompanionZones,
};
inline constexpr std::array<GroundPaint, 7> kGroundPaintOrder = {
    GroundPaint::kGreyWorld,  GroundPaint::kGraffiti,  GroundPaint::kCompanionZones,
    GroundPaint::kSkillAreas, GroundPaint::kNaviTrail, GroundPaint::kCellCursor,
    GroundPaint::kGroundUnits};

// Vrai quand `first` se peint avant `then` dans `kGroundPaintOrder`.
[[nodiscard]] constexpr bool PaintsBefore(GroundPaint first, GroundPaint then) {
  for (const GroundPaint pass : kGroundPaintOrder) {
    if (pass == first) {
      return true;
    }
    if (pass == then) {
      return false;
    }
  }
  return false;
}

// LA CASE QUE LE CADRE AU SOL MARQUE.
//
// ⚠ SOUS UNE EMPRISE D'UNITÉ, LE CADRE NE SUIT PLUS LA SOURIS : le client ne
// relève la case désignée que lorsque rien de cliquable n'a pris le pointeur,
// et laisse sinon le cadre sur la dernière case relevée. `held` est cette
// dernière case, que l'appel tient à jour.
[[nodiscard]] std::optional<ro::sim::CellPosition> CellCursorOf(
    const std::optional<ro::sim::CellPosition>& hovered_cell, bool over_unit,
    std::optional<ro::sim::CellPosition>& held);

}  // namespace moonclient
