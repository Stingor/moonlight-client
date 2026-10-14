#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/render/item_icons.h"
#include "ro/render/skill_icons.h"
#include "ro/sim/guild.h"
#include "ro/sim/shortcut_bar.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/vfs/item_table.h"

#include "character/skill_book_view.h"
#include "input/hotkeys.h"

namespace moonclient {

class HomunculusStage;
class MercenaryStage;

// LA BARRE DE RACCOURCIS, VUE DE LA PARTIE.
//
// ⚠ ELLE RASSEMBLE TROIS SOURCES QUE RIEN NE RELIE : la barre elle-même — des
// identifiants —, l'arbre du client — les noms et les icônes —, et le livre du
// serveur — ce qui est encore appris et ce qui est en attente. Une case ne se
// dessine qu'une fois les trois croisées, et c'est le travail de cet étage.
//
// ⚠ ET ELLE NE LANCE RIEN ELLE-MÊME. Lancer se fait en deux temps — choisir,
// puis désigner —, et cette mécanique vit déjà dans le grimoire de lancement. La
// barre ne fait que CHOISIR, comme un clic sur une de ses lignes.
class ShortcutBarView {
 public:
  ShortcutBarView(std::filesystem::path client_root, const ro::sim::SkillTree* tree,
                  ro::render::SkillIcons* icons);

  // CE QU'IL FAUT POUR MONTRER UN OBJET dans une case. Posé à part du
  // constructeur : la table et les icônes d'objets se chargent avec les
  // accessoires, plus tard que l'arbre des compétences.
  void UseItems(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons) {
    item_table_ = items;
    item_icons_ = icons;
  }

  // LES COMPÉTENCES DE GUILDE, qu'on glisse sur une case depuis l'onglet de la
  // fiche. ⚠ ELLES NE SONT PAS DANS LE LIVRE DU PERSONNAGE : c'est le carnet de
  // guilde qui dit leur rang, et elles se lancent toutes sur soi — par le
  // maître seul, que le carnet reconnaît à `char_id`.
  void UseGuild(const ro::sim::GuildBook* guild, std::uint32_t char_id) {
    guild_ = guild;
    guild_char_id_ = char_id;
  }

  // LES COMPÉTENCES DES COMPAGNONS — homoncule et mercenaire —, glissées depuis
  // leurs onglets. ⚠ ELLES NE SONT PAS DANS LE LIVRE DU PERSONNAGE : chaque
  // compagnon tient les siennes, et seule celle qui se lance sur soi part d'une
  // case — voir `ro::sim::CompanionCast`.
  void UseCompanions(HomunculusStage* homunculus, MercenaryStage* mercenary) {
    homunculus_ = homunculus;
    mercenary_ = mercenary;
  }

  [[nodiscard]] ro::sim::ShortcutBars& bar() noexcept { return bar_; }

  // Pose une compétence dans une case, et l'enregistre — le dépôt d'un glisser,
  // sans glisser. Voir `GrimoireKeys`.
  void PlaceSkill(std::size_t index, std::uint16_t skill_id, std::uint16_t level);
  // La compétence dont les cases s'éclairent au prochain `Refresh`, zéro pour
  // aucune.
  void Highlight(std::uint16_t skill_id) noexcept { highlighted_skill_ = skill_id; }

  // Relit la barre du personnage en cours, au changement de personnage. Pour
  // un compte staff, relit aussi celle du métier quand il change en jeu
  // (@jobchange) : voir `LoadJobShortcutBar`. À appeler à chaque image.
  void FollowCharacter(const ro::sim::World& world);

  // Refait les cases. À appeler une fois par image, avant le dessin : les temps
  // d'attente changent à chaque instant.
  void Refresh(const ro::sim::World& world, const Hotkeys& bindings, std::uint32_t now_ms);

  // LES QUATRE-VINGT-SEIZE CASES, prêtes à dessiner, dans l'ordre de leur index
  // global.
  //
  // ⚠ TOUTES D'UN COUP, et c'est le dessin qui les découpe par barre : une barre
  // est une FENÊTRE, et c'est lui qui les ouvre. Lui rendre douze cases à la
  // fois l'obligerait à nous rappeler huit fois pour une image.
  [[nodiscard]] std::span<const ro::ui::ShortcutSlotView> slots() const noexcept { return slots_; }

  // Fait ce que le joueur a demandé : lancer, vider, déposer. `book` est le
  // grimoire de lancement, qui seul sait armer.
  void Apply(const ro::ui::ShortcutBarsAction& asked, SkillBookPanel& book,
             ro::net::Connection& link, ro::sim::World& world, std::uint32_t now_ms);

  // LANCE CE QUE PORTE UNE CASE, désignée par son index global.
  //
  // ⚠ PUBLIQUE PARCE QUE LA TOUCHE Y VIENT DIRECTEMENT. Les cases sont
  // quatre-vingt-seize ; le clavier ne passe plus par une action nommée par
  // case, il donne l'index et cette méthode fait le reste.
  //
  // Une case vide, hors compte, ou dont la compétence n'est plus connue ne fait
  // rien — et le dit dans la conversation quand il y a quelque chose à dire.
  //
  // 🔴 `repeated` DIT QUE LA TOUCHE EST TENUE, et c'est un frein, pas un ajout.
  // La bibliothèque de fenêtrage nous livre l'auto-répétition du système ;
  // laissée libre, elle enverrait trente demandes par seconde.
  //
  // ⚠ UN OBJET NE SE RÉPÈTE PAS ICI : le jeu ignore la répétition du clavier
  // pour lui, et c'est Quick cast qui le réutilise, à SA cadence, quand le
  // joueur l'a demandé. Voir `RepeatItemSlot`.
  //
  // 🔴 UNE COMPÉTENCE SE RÉPÈTE, À UNE CONDITION, comme au client du joueur :
  // que la dernière demande de compétence sur une entité soit partie alors que
  // CETTE touche était la dernière enfoncée. La touche tenue relance alors la
  // compétence à la cadence de l'auto-répétition, sans autre frein que celui du
  // serveur. Une touche qui n'a encore rien lancé — une compétence au sol, un
  // ciblage jamais conclu — ne se répète pas.
  void Cast(std::size_t slot, SkillBookPanel& book, ro::net::Connection& link,
            ro::sim::World& world, std::uint32_t now_ms, bool repeated = false);

  // UNE TOUCHE VIENT D'ÊTRE ENFONCÉE — une case de barre, ou autre chose quand
  // `slot` est vide. Ni sur une répétition, ni pendant une saisie.
  //
  // ⚠ ELLE RETIENT D'ABORD ce qui s'est lancé sous la touche précédente : c'est
  // la touche enfoncée AU MOMENT DU LANCEMENT qui gagne le droit de répéter, pas
  // celle qu'on enfonce ensuite.
  void NotePress(std::optional<std::size_t> slot, const SkillBookPanel& book);

  // COMBIEN DE FOIS UNE CASE D'OBJET A SERVI, par une touche fraîche ou un
  // clic, et laquelle en dernier. C'est ce que Quick cast compare pour armer sa
  // répétition.
  [[nodiscard]] std::uint32_t item_uses() const noexcept { return item_uses_; }
  [[nodiscard]] std::size_t last_item_slot() const noexcept { return last_item_slot_; }
  [[nodiscard]] std::uint32_t last_item_id() const noexcept { return last_item_id_; }

  // RÉUTILISE LA CASE `slot`, pour la répétition de Quick cast. Faux quand il
  // n'y a plus rien à répéter : la case ne porte plus `item_id` — vidée ou
  // réarrangée —, ou l'Inventory n'en a plus.
  //
  // ⚠ LA VOIE EXACTE DE LA TOUCHE, et non une demande fabriquée à côté : la
  // place dans le sac se cherche au moment de partir, comme pour elle.
  bool RepeatItemSlot(std::size_t slot, std::uint32_t item_id, ro::net::Connection& link,
                      const ro::sim::World& world);

  // L'OBJET D'UNE CASE, POUR LE DÉCRIRE — son identifiant, et une PLACE du sac
  // où il se trouve encore.
  //
  // ⚠ LA PLACE SE CHERCHE MAINTENANT, elle n'est pas retenue : une case garde un
  // identifiant précisément parce qu'une place se renumérote dès qu'on consomme.
  // Elle ne sert qu'à décorer le nom — raffinement, cartes, forge —, et zéro
  // veut dire « plus aucun exemplaire », auquel cas le nom de base fait
  // l'affaire.
  //
  // Rend zéro pour une case vide, hors compte, ou qui porte une compétence.
  [[nodiscard]] std::uint32_t DescribedItem(std::size_t slot, const ro::sim::World& world,
                                            std::uint16_t* found_at) const;
  // LA COMPÉTENCE D'UNE CASE, POUR LA DÉCRIRE ; zéro pour une case vide, hors
  // compte, ou qui porte un objet.
  [[nodiscard]] std::uint16_t DescribedSkill(std::size_t slot) const;

 private:
  void FollowJob(const ro::sim::World& world);
  void SaveIfNeeded();
  // Si une compétence est partie sur une entité depuis la dernière fois, la
  // touche enfoncée à ce moment-là devient celle qui répète.
  void LatchCasting(const SkillBookPanel& book);
  // La compétence de guilde de ce numéro, APPRISE ET QUE LE JOUEUR PEUT LANCER,
  // ou nulle. `held` dit, quand elle est nulle, si elle est apprise quand même.
  [[nodiscard]] const ro::sim::GuildSkill* GuildSkillOf(std::uint16_t skill_id,
                                                        bool* held = nullptr) const;
  // Lance la compétence d'un compagnon qui la tient. Faux quand aucun ne la
  // tient : elle n'est alors pas à eux.
  bool CastCompanionSkill(std::uint16_t skill_id, int level, ro::net::Connection& link,
                          ro::sim::World& world, std::uint32_t now_ms);

  std::filesystem::path client_root_;
  const ro::sim::SkillTree* tree_ = nullptr;
  ro::render::SkillIcons* icons_ = nullptr;
  const ro::sim::GuildBook* guild_ = nullptr;
  std::uint32_t guild_char_id_ = 0;
  // Posés plus tard que les précédents : la table des objets et leurs icônes se
  // chargent avec les accessoires, après l'arbre des compétences.
  const ro::vfs::ItemTable* item_table_ = nullptr;
  ro::render::ItemIcons* item_icons_ = nullptr;
  ro::sim::ShortcutBars bar_;
  // Le personnage dont la barre est chargée. Vide tant que le serveur n'a pas
  // nommé le nôtre.
  std::string character_;
  // Le métier dont le jeu de barres est chargé, pour un compte staff ; vide
  // pour un joueur, dont la barre ne suit que le personnage.
  std::optional<std::uint16_t> job_;
  std::uint16_t highlighted_skill_ = 0;
  // Les libellés de touche composés, et les vues qui pointent dedans.
  std::vector<std::string> keys_;
  std::vector<ro::ui::ShortcutSlotView> slots_;
  std::uint32_t saved_revision_ = 0;
  std::uint32_t item_uses_ = 0;
  std::size_t last_item_slot_ = 0;
  std::uint32_t last_item_id_ = 0;
  // La case de la dernière touche enfoncée, vide si ce n'en était pas une.
  std::optional<std::size_t> pressed_slot_;
  // La case dont la touche tenue relance sa compétence — voir `Cast`.
  std::optional<std::size_t> repeating_slot_;
  // Les lancements sur une entité déjà comptés : ceux du grimoire, et les
  // compétences de guilde que la barre envoie elle-même.
  std::uint32_t seen_entity_casts_ = 0;
  std::uint32_t guild_casts_ = 0;
  // Et celles des compagnons.
  std::uint32_t companion_casts_ = 0;
  HomunculusStage* homunculus_ = nullptr;
  MercenaryStage* mercenary_ = nullptr;
};

}  // namespace moonclient
