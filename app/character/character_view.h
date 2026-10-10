#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/item_icons.h"
#include "ro/render/skill_icons.h"
#include "ro/sim/companion.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/character_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/job_names.h"
#include "ro/vfs/status_table.h"

#include "items/creator_names.h"

namespace moonclient {

// LA FICHE DE PERSONNAGE, VUE DE LA PARTIE.
//
// ⚠ ELLE CROISE CE QUE RIEN NE RELIE : les pièces portées vivent dans le SAC,
// les nombres dans le PERSONNAGE, la vie sur l'ACTEUR, et les noms et icônes
// dans l'installation. `ui` ne connaît aucune de ces quatre sources — elle
// reçoit onze cases déjà remplies et un jeu de nombres déjà dérivés.
//
// ⚠ LE RANGEMENT PAR EMPLACEMENT EST ICI, PAS DANS LA FENÊTRE. Savoir qu'un
// chapeau d'apparat ne répond pas au bit d'un chapeau ordinaire, et qu'une arme
// à deux mains ne doit pas se montrer dans la case du bouclier, demande de
// connaître les drapeaux du serveur.
class CharacterView {
 public:
  // LES NOMS DES FORGERONS, confiés une fois la description ouverte.
  void SeeCreators(CreatorNames creators) noexcept { creators_ = creators; }

  CharacterView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                ro::render::ItemIcons* icons, const ro::vfs::JobNames* jobs,
                const ro::sim::SkillTree* skills, const ro::vfs::StatusTable* statuses,
                ro::render::SkillIcons* skill_icons, ro::render::ActorWardrobe* wardrobe);

  // Refait les onze cases et les nombres. À appeler une fois par image, avant le
  // dessin. `costume` change les emplacements cherchés, pas leur place.
  //
  // ⚠ LES NOMS COMPOSÉS SONT GARDÉS D'UNE IMAGE À L'AUTRE : les vues rendues
  // POINTENT dessus.
  //
  // ⚠ NE TRAVAILLE QUE LA FENÊTRE OUVERTE, comme le grimoire : les cases, les
  // nombres, les bonus et les compagnons ne sont lus que par elle.
  void Refresh(bool open, const ro::sim::World& world, bool costume);

  [[nodiscard]] std::span<const ro::ui::EquipSlotView> slots() const noexcept { return slots_; }
  [[nodiscard]] const ro::ui::CharacterHeaderView& header() const noexcept { return header_; }
  [[nodiscard]] const ro::ui::CharacterStatsView& stats() const noexcept { return stats_; }
  // LES COMPAGNONS QU'ON PEUT APPELER — le chariot, la monture, le faucon.
  //
  // ⚠ SEULS CEUX DONT LA COMPÉTENCE EST APPRISE : c'est ici qu'on écarte, pas
  // dans la fenêtre. Vide pour l'immense majorité des métiers.
  [[nodiscard]] std::span<const ro::ui::CompanionCaseView> companions() const noexcept {
    return companions_;
  }

  // LA SILHOUETTE DE L'AVATAR, composée à la taille que la fenêtre a mesurée
  // à l'image précédente.
  //
  // ⚠ À PART DE `Refresh`, parce qu'elle lit ce que la FENÊTRE a écrit — sa
  // boîte, sa pose, sa direction — là où tout le reste ne lit que le monde. Les
  // mêler ferait dessiner la silhouette avant de savoir où elle tombe.
  //
  // ⚠ ELLE MONTRE CE QUE L'APPELANT A HABILLÉ : ce que la carte montre, sauf
  // sur l'onglet Équipement quand « Voir les costumes » est décochée — les
  // coiffes et la cape y sont alors celles que le personnage PORTE.
  //
  // `dressed` est l'apparence du joueur DÉJÀ HABILLÉE de ses couleurs —
  // identifiant de style et les trois numéros que sa recette impose, voir
  // `world/actor_styles.h`.
  //
  // ⚠ ELLE VIENT DE L'APPELANT plutôt que d'être calculée ici : le registre
  // qui distribue les identifiants est le même que celui de la carte, et deux
  // registres donneraient deux identifiants pour la même recette, donc deux
  // tenues et deux textures pour un seul personnage.
  void RefreshAvatar(const ro::sim::World& world, const ro::ui::CharacterAvatarState& avatar,
                     ro::ui::SkinImage& image, std::uint32_t now_ms,
                     const ro::render::ActorLook& dressed);

  // Fait ce que le joueur a demandé : retirer une pièce, monter une
  // caractéristique.
  //
  // ⚠ ÉMET ET S'ARRÊTE LÀ, comme les verbes du sac. Le serveur décide, et il
  // refuse une montée pour trois raisons distinctes ; avancer la valeur chez
  // nous montrerait une force que le monde ne partage pas.
  void Apply(const ro::ui::CharacterWindowAction& asked, ro::net::Connection& link,
             const ro::sim::World& world);

 private:
  // Les trois cases, refaites à chaque image par `Refresh`.
  void RefreshCompanions(const ro::sim::World& world);

  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  CreatorNames creators_;
  ro::render::ItemIcons* icons_;
  const ro::vfs::JobNames* jobs_ = nullptr;
  // ⚠ IL PEUT MANQUER : une installation sans tables de compétences fait une
  // fiche qui les nomme par leur numéro, pas une fiche qui plante.
  const ro::sim::SkillTree* skills_ = nullptr;
  const ro::vfs::StatusTable* statuses_ = nullptr;
  // ⚠ ELLES PEUVENT MANQUER, comme l'arbre : une case sans icône garde son
  // cadre et son infobulle.
  ro::render::SkillIcons* skill_icons_ = nullptr;
  // ⚠ ELLE PEUT MANQUER AUSSI : une installation sans sprites donne une fiche
  // dont la place de l'avatar reste vide, et le dit — pas une fiche qui plante.
  std::optional<ro::render::ActorVignette> avatar_;

  // Un nom par case, gardé le temps que la fenêtre le lise.
  std::array<std::string, ro::ui::kEquipSlotCount> names_;
  std::vector<ro::ui::EquipSlotView> slots_;
  // ⚠ LES NOMS DES TROIS CASES VIVENT ICI, et les vues pointent dedans : le
  // nom vient de l'arbre quand il est là, et d'un repli composé sinon — lequel
  // n'aurait nulle part où vivre.
  std::array<std::string, ro::ui::kCompanionCaseMost> companion_names_;
  std::vector<ro::ui::CompanionCaseView> companions_;
  ro::ui::CharacterHeaderView header_;
  ro::ui::CharacterStatsView stats_;
  // Les apports non nuls, refaits à chaque image. ⚠ LA VUE POINTE DEDANS, comme
  // pour les noms des onze cases.
  std::vector<ro::ui::CharacterBonusRow> bonus_rows_;
  // ⚠ LES LIBELLÉS COMPOSÉS VIVENT ICI, et les rangées conditionnelles pointent
  // dedans : elles ne portent que des vues.
  std::vector<std::string> conditional_labels_;
  std::vector<ro::ui::CharacterBonusRow> conditional_rows_;
  std::vector<std::string> skill_labels_;
  std::vector<ro::ui::CharacterBonusRow> skill_rows_;
  std::vector<std::string> item_labels_;
  std::vector<ro::ui::CharacterBonusRow> item_rows_;
  std::string character_name_;
};

}  // namespace moonclient
