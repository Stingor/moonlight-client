#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/skill_icons.h"
#include "ro/sim/grimoire.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/skill_window.h"

#include "character/skill_book_view.h"

namespace moonclient {

// DU GRIMOIRE À CE QUE LA FENÊTRE DESSINE.
//
// ⚠ C'est ici que l'arbre, le livre du serveur et les icônes deviennent des
// données pures. `ui` ne connaît ni `sim` ni OpenGL : elle reçoit des cases avec
// leur place, leur rang et un identifiant de texture, et rend ce que le joueur a
// demandé. Même frontière que la `FrameScene` du monde.
//
// ⚠ LES ARBRES SE FUSIONNENT COMME AU CLIENT : le premier palier et le deuxième
// dans le même onglet — une deuxième classe se lit toujours à la lumière de la
// première —, le troisième et le quatrième à part, parce que ce sont des arbres
// entiers que mélanger ne ferait qu'un mur d'icônes.
// LE RANG AUQUEL LE JOUEUR LANCE CHAQUE COMPÉTENCE.
//
// 🔴 LANCER PLUS BAS QU'ON N'A APPRIS A UN SENS : un Heal de rang 1 coûte dix
// points de magie là où le rang 10 en coûte quarante, et sur une égratignure
// c'est le rang 1 qu'on veut. Le client d'origine garde ce choix par compétence
// — `SetUseLevel` — et le menu de la grille le règle.
//
// ⚠ IL NE SE PERSISTE PAS, et le client non plus : il vit dans sa session et
// tombe à la déconnexion. Un rang gardé d'une partie à l'autre surprendrait plus
// qu'il ne rendrait service — on ne se souvient pas d'avoir réglé cela hier.
//
// ⚠ ZÉRO VEUT DIRE « CELUI QU'ON A APPRIS » : c'est le défaut, et il n'a pas à
// s'écrire pour chaque compétence du grimoire.
class SkillUseLevels {
 public:
  [[nodiscard]] int Of(std::uint16_t skill_id) const;
  // Un rang hors des bornes est écarté : c'est le dessin qui les connaît, et il
  // peut se tromper d'une unité sans que cela devienne un paquet refusé.
  void Set(std::uint16_t skill_id, int level, int learned);

 private:
  // ⚠ RANGÉE PAR NUMÉRO : rien ici n'en dépend, mais un parcours reproductible
  // vaut mieux qu'un ordre de table de hachage quand on vient lire au débogueur.
  std::map<std::uint16_t, int> chosen_;
};

class SkillView {
 public:
  // Refait les groupes. À appeler avant le dessin, une fois par image : le
  // grimoire change à chaque paquet et une réservation change les couleurs.
  //
  // ⚠ LE LIVRE DU SERVEUR ET L'HORLOGE VIENNENT AVEC : les temps d'attente
  // changent à chaque instant, et ce sont eux qui dessinent le voile. Les laisser
  // dehors ferait une grille qui ment sur ce qui est prêt.
  void Rebuild(const ro::sim::SkillTree& tree, const ro::sim::Grimoire& grimoire, std::uint32_t job,
               ro::render::SkillIcons& icons, const ro::sim::SkillBook& book,
               const SkillUseLevels& levels, std::uint32_t now_ms);

  [[nodiscard]] std::span<const ro::ui::SkillGroupView> groups() const noexcept { return groups_; }

 private:
  // ⚠ TOUT EST GARDÉ ICI, et les vues pointent dedans : une case porte une VUE
  // sur ses prérequis et sur son nom. Refaire ces tampons en cours de
  // remplissage ferait pendre les vues déjà posées — ils se remplissent donc
  // entièrement AVANT qu'on y pointe.
  std::vector<ro::ui::SkillCellView> cells_;
  std::vector<ro::ui::SkillNeedView> needs_;
  std::vector<ro::ui::SkillSplitView> splits_;
  std::vector<ro::ui::SkillGroupView> groups_;
  // Les libellés composés — « 1st / 2nd » —, qui n'existent dans aucune table.
  std::vector<std::string> labels_;
};

// Fait ce que le joueur a demandé au grimoire.
//
// ⚠ RÉSERVER NE PARLE À PERSONNE. Seul « Appliquer » envoie, et il envoie UNE
// MONTÉE PAR RANG : le paquet du jeu ne porte pas de niveau, et le serveur
// revalide chacune. C'est ce qui permet de préparer une branche entière, de la
// voir, et de l'abandonner sans avoir rien dépensé.
void ApplySkillAction(const ro::ui::SkillWindowAction& action, ro::sim::Grimoire& grimoire,
                      ro::net::Connection& link);

// LA FICHE DE PERSONNAGE, VUE DE LA BOUCLE : un objet, et quatre lignes.
//
// ⚠ ELLE TIENT DEBOUT SANS ARBRE. Une installation qui n'a pas les tables de
// compétences garde un jeu entier, moins ce grimoire : la fiche se tait alors,
// et rien d'autre ne change. C'est la même tolérance que pour l'habillage.
class SkillSheet {
 public:
  // Les deux pointeurs peuvent être nuls — voir ci-dessus. Ils doivent vivre
  // plus longtemps que la fiche, ce que la boucle assure en les déclarant avant.
  SkillSheet(const ro::sim::SkillTree* tree, ro::render::SkillIcons* icons);

  // Relit le livre du serveur, recompose les onglets, et rend les nombres de
  // l'en-tête.
  //
  // ⚠ NE TRAVAILLE QUE LA FENÊTRE OUVERTE. Relire tout l'arbre et tout le livre
  // à chaque image pour une fenêtre fermée serait du temps donné à rien.
  [[nodiscard]] ro::ui::SkillPointTally Refresh(bool open, const ro::sim::World& world,
                                                std::uint32_t now_ms);

  [[nodiscard]] std::span<const ro::ui::SkillGroupView> groups() const noexcept {
    return view_.groups();
  }

  // Fait ce que le joueur a demandé. Voir `ApplySkillAction`.
  //
  // ⚠ `book` EST LE GRIMOIRE DE LANCEMENT, et c'est lui qui arme : lancer se
  // fait en deux temps — choisir, puis désigner —, et cette mécanique n'a aucune
  // raison d'exister deux fois. Une case de la grille CHOISIT, exactement comme
  // une case de la barre de raccourcis.
  void Apply(const ro::ui::SkillWindowAction& action, SkillBookPanel& panel,
             ro::net::Connection& link, ro::sim::World& world, std::uint32_t now_ms);

 private:
  const ro::sim::SkillTree* tree_ = nullptr;
  ro::render::SkillIcons* icons_ = nullptr;
  std::optional<ro::sim::Grimoire> grimoire_;
  SkillUseLevels use_levels_;
  SkillView view_;
};

}  // namespace moonclient
