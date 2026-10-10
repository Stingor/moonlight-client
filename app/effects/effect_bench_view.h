#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/font_atlas.h"
#include "ro/render/screen_overlay.h"
#include "ro/render/skill_panel.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

#include "effects/effect_view.h"
#include "world/map_stage.h"

// LE BANC D'ESSAI DES EFFETS : la liste de ce que nous savons montrer, et un
// clic pour le jouer sur soi.
//
// POURQUOI IL EXISTE. Une figure procédurale ne se juge qu'en jeu, contre le
// client natif — c'est écrit dans le plan, et c'est ce qui a fait trouver le
// fond noir des éclairs et le plan de travers des quads. Mais la juger passait
// par « /noeud 93 » tapé à la main, un identifiant à la fois, relu dans un
// fichier engendré. Un essai pénible est un essai qu'on ne fait pas, ou qu'on
// fait mal.
//
// Il ne DÉCORE rien et ne prétend rien de l'interface du jeu : il emprunte le
// panneau du grimoire, qui ne sait déjà pas ce qu'il montre.
//
// ⚠ IL MONTRE LES DEUX FAMILLES, et il l'a appris à ses dépens : n'y mettre que
// les effets PROCÉDURAUX faisait chercher en vain le mur de feu, dont l'effet a
// un fichier et n'y paraissait donc pas. Un banc qui ne montre qu'une moitié
// fait conclure à tort qu'une chose manque.
//
// Il montre aussi les UNITÉS POSÉES, sous leur numéro d'unité, parce que c'est
// par lui qu'on les cherche quand on vient du protocole ou du serveur. Et il
// garde celles que nous ne savons PAS peindre, marquées comme telles : une
// absence nommée se corrige, une absence muette se redécouvre.
namespace moonclient {

// La liste des effets qu'un banc montre, et celui qu'on vient de jouer.
class EffectBenchPanel {
 public:
  [[nodiscard]] bool open() const noexcept { return open_; }

  // Ouvre ou ferme le banc. La liste se bâtit à la première ouverture : elle ne
  // dépend que de la table engendrée, qui ne bouge pas de la partie.
  void Toggle();

  // Fait défiler quand le pointeur est SUR le panneau. Faux s'il est ailleurs,
  // et la molette retourne alors à la caméra.
  [[nodiscard]] bool Scroll(glm::vec2 mouse_px, float wheel_steps, float viewport_width,
                            float viewport_height);

  // Joue l'effet de la ligne désignée, sur le joueur. Vrai dès que le clic
  // TOMBE DANS le panneau, qu'il ait joué quelque chose ou non : c'est ce qui
  // empêche le clic de descendre jusqu'au sol et de faire marcher.
  [[nodiscard]] bool Click(EffectsInPlay& effects, EffectNodesInPlay& nodes, ro::sim::World& world,
                           const MapStage& stage, ro::sim::WalkPlanner& planner, glm::vec2 mouse_px,
                           float viewport_width, float viewport_height, std::uint32_t now_ms);

  [[nodiscard]] std::vector<ro::render::ScreenQuad> Drawn(const ro::render::FontAtlas& font,
                                                          float viewport_width,
                                                          float viewport_height);

 private:
  // Une ligne : ce qu'on lit, et l'effet qu'elle joue.
  struct Entry {
    std::uint32_t effect_id = 0;
    // Les deux moitiés du libellé, gardées ICI parce que le panneau ne prend
    // que des vues : une chaîne temporaire lui laisserait un pointeur mort.
    std::string label;
    std::string detail;
    // Vrai quand une des deux tables du client lui donne un fichier. Le clic
    // n'en joue qu'un des deux : un effet qui a les deux est un effet dont le
    // patron double le fichier, et les semer ensemble le montrerait double.
    bool has_file = false;
    // Vrai pour un effet de la NEUVIÈME famille : il ne dessine rien et change
    // l'acteur. Le clic le pose alors sur le joueur au lieu de semer une figure.
    bool on_actor = false;
  };

  void Rebuild();
  [[nodiscard]] ro::render::SkillPanelLayout Placement(float viewport_width,
                                                       float viewport_height) const;

  bool open_ = false;
  std::vector<Entry> entries_;
  std::size_t first_row_ = 0;
  // Le dernier joué, pour le montrer en surbrillance — on voit ainsi ce qu'on
  // regarde, et l'on retrouve sa ligne après avoir tourné la caméra.
  std::size_t played_ = 0;
  bool has_played_ = false;
};

}  // namespace moonclient
