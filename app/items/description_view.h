#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/item_icons.h"
#include "ro/sim/item_facts.h"
#include "ro/sim/viewed_equipment.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/description_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/monster_list.h"
#include "ro/vfs/package_items.h"
#include "ro/vfs/pet_tables.h"
#include "ro/vfs/random_option_names.h"
#include "ro/vfs/server_db.h"

#include "game/bug_report_context.h"
#include "items/book_reader.h"
#include "items/creator_names.h"
#include "items/item_doll.h"
#include "items/item_facts_asker.h"

namespace moonclient {

// CE QUE LA BASE DE DONNÉES AJOUTE À LA FICHE D'UN OBJET (ADR 0116). Les vues
// pointent sur ce que l'appelant garde, le temps d'une image.
struct ItemSheetBase {
  // « Général » ; vide, l'onglet n'existe pas.
  std::span<const ro::ui::ItemSheetFact> facts;
  // LA BASE CONNAÎT L'OBJET : ses sources et ses scripts remplissent ce que le
  // serveur n'a pas encore dit.
  bool known = false;
  std::span<const ro::ui::DescriptionDropRow> ordinary;
  std::span<const ro::ui::DescriptionDropRow> boss;
  std::string_view script;
  std::string_view equip_script;
  std::string_view unequip_script;
  ro::ui::ItemSheetSellersPhase sellers_phase = ro::ui::ItemSheetSellersPhase::kAbsent;
  std::string_view sellers_status;
  std::span<const ro::ui::ItemSheetSellerRow> sellers;
  // La fiche est dans la base : son icône en en-tête, et le nom du serveur
  // pour un objet que la table du client ignore.
  bool in_database = false;
  ro::ui::SkinImage icon;
  std::string_view name;
};

// LA DESCRIPTION D'UN OBJET, VUE DE LA PARTIE.
//
// ⚠ ELLE CROISE TROIS SOURCES : la table de l'installation — le nom de base et
// les lignes de description —, les mots du client pour composer un nom décoré,
// et les illustrations. `ui` n'en connaît aucune.
//
// 🔴 SA CLÉ EST UN IDENTIFIANT, PAS UNE PLACE. On décrit aussi bien un objet du
// sac qu'une carte qu'on n'a jamais tenue, et une description ouverte doit
// survivre à la consommation de l'objet qui l'a ouverte. La place ne sert qu'à
// DÉCORER le nom — raffinement, cartes — quand l'objet est encore là.
class DescriptionView {
 public:
  DescriptionView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                  ro::render::ItemIcons* icons);

  // Refait la vue pour l'objet que la fenêtre montre. À appeler une fois par
  // image, avant le dessin. Ne coûte rien tant que rien n'est montré.
  //
  // 🔴 LA PLACE VIENT AVEC SON CONTENANT. La place 3 du sac et la place 3 du
  // coffre désignent deux objets différents : chercher l'une dans l'autre ne
  // rend pas une erreur, il rend un AUTRE objet — ou rien, et le nom perd alors
  // son raffinement et ses cartes sans que rien ne le signale.
  void Refresh(const ro::sim::World& world, std::uint32_t item_id, ro::ui::ItemHolder from,
               std::uint16_t decorate_from);

  // L'ÉQUIPEMENT MONTRÉ PAR UN AUTRE JOUEUR, où lire une pièce venue de
  // `ItemHolder::kViewedEquipment`. ⚠ CONFIÉ UNE FOIS : le livre vit aussi
  // longtemps que la partie, comme cette vue.
  void SeeViewedEquipment(const ro::sim::ViewedEquipmentBook* viewed) noexcept { viewed_ = viewed; }

  [[nodiscard]] const ro::ui::DescriptionView& view() const noexcept { return view_; }

  // Ce que le menu d'un item sait d'avance : vote shop, album. ⚠ À POSER APRÈS
  // `Refresh`, qui remet la vue à zéro.
  void NoteLinkFacts(ro::ui::ItemLinkFacts facts) noexcept { view_.link_facts = facts; }

  // L'ÉTIQUETTE « CRAFT » de l'item montré, APRÈS `Refresh` — qui l'efface. Deux
  // textes vides : l'item n'a rien à voir avec la fabrication.
  void NoteCraftTag(std::string label, std::string tip);

  // LE BOUTON DU VOTE SHOP de l'item montré, APRÈS `Refresh` — qui l'efface. Un
  // texte vide : l'item n'est pas au catalogue, ou le catalogue n'est pas là.
  void NoteVoteShopTag(std::string label);

  // LA LIGNE « @item » DU STAFF, APRÈS `Refresh` — qui l'efface. ⚠ LA PIÈCE
  // PORTÉE d'une comparaison ne la reçoit jamais : on demande l'item décrit.
  // `stackable` et `free_bag_slots` bornent la quantité d'un item non
  // empilable ; vides, on ne les sait pas et rien n'est borné.
  void NoteStaff(bool staff_tools, std::optional<bool> stackable,
                 std::optional<int> free_bag_slots) noexcept {
    view_.staff_item = staff_tools;
    view_.stackable = stackable;
    view_.free_bag_slots = free_bag_slots;
  }

  // LA TABLE DE TIRAGE DES CONTENEURS, pour l'onglet « Probabilités ». ⚠ LA
  // FENÊTRE SEULEMENT : l'aperçu au survol n'a pas d'onglets.
  void SeePackages(const ro::vfs::PackageItems* packages) noexcept { packages_ = packages; }
  // Les monstres du serveur, qui nomment ceux qu'une branche invoque. Absents,
  // un monstre de la table s'écrit par son numéro seul.
  void SeeMonsters(const ro::vfs::MonsterList* monsters) noexcept { monsters_ = monsters; }
  // Les libellés des options aléatoires. Absents, une option s'écrit par son
  // numéro.
  void SeeRandomOptions(const ro::vfs::RandomOptionNames* names) noexcept { option_names_ = names; }
  // L'ITEM_DB DU SERVEUR, dont le bloc de faits suit les lignes de la table
  // (`AppendItemDbBlock`). ⚠ CONFIÉE UNE FOIS, ENCORE NON LUE : sa lecture est
  // paresseuse, et les lignes se refont le jour où elle est lue. La pièce
  // portée d'une comparaison la reçoit aussi.
  void SeeServerDb(const ro::vfs::ServerDb* db) noexcept;
  // LES TABLES DU FAMILIER, qui nomment ce qu'un appât capture, ce qui sort d'un
  // œuf et ce qu'il mange, dans le même bloc. La comparaison les reçoit aussi.
  void SeePets(const ro::vfs::PetTables* pets) noexcept;

  // L'OBJET D'UN LIEN DE LA CONVERSATION, qu'on décrit sous
  // `ItemHolder::kChatLink`. ⚠ RECOPIÉ : la table des liens se refait à chaque
  // image, et la description lui survit.
  void NoteLinked(const ro::vfs::ItemLink& link) { linked_ = link; }

  // LES FORGERONS SE DEMANDENT PAR UNE SEULE VUE : celle dont `AskFacts` tourne
  // à chaque image. L'aperçu lit donc les noms de la fenêtre, et y met ses
  // demandes.
  void ShareCreatorsWith(DescriptionView& owner) noexcept { creators_ = &owner.asker_; }
  // 🔴 LA FICHE DE LA BASE EMPRUNTE CELLE DE LA FENÊTRE : ses faits du serveur
  // — les réponses n'arrivent qu'à un seul endroit —, ses forgerons, ses tables
  // de tirage, d'options et de monstres, et sa garde-robe pour le pantin.
  void BorrowFrom(DescriptionView& owner, ro::render::ActorWardrobe* wardrobe);
  // CE QUE LA BASE AJOUTE, APRÈS `AskFacts` qui recompose les onglets du
  // serveur.
  void NoteDatabase(const ItemSheetBase& base);
  // LA POIGNÉE DES FORGERONS, pour les autres surfaces qui nomment un item.
  [[nodiscard]] CreatorNames creator_names() noexcept { return CreatorNames(creators_); }

  // LA COMPARAISON AVEC LA PIÈCE PORTÉE, pour la fenêtre seulement — le client
  // ne compare pas dans l'aperçu. ⚠ LA PIÈCE PORTÉE SE DÉCRIT PAR UNE SECONDE
  // VUE, qui ne compare rien elle-même.
  void EnableComparison();

  // LES LIVRES que cette description ouvre et fait réciter.
  [[nodiscard]] BookReader& books() noexcept { return books_; }

  // LE PANTIN DU « VIEWID » : la garde-robe où le composer, confiée une fois.
  void SeeWardrobe(ro::render::ActorWardrobe* wardrobe) { doll_.emplace(wardrobe); }
  // Compose le pantin quand la fenêtre le demande. À appeler après `Refresh`.
  // `dressing` : le registre des couleurs de la carte, voir `ItemDoll::Refresh`.
  void RefreshDoll(const ro::sim::World& world, ro::ui::DescriptionDollState& state,
                   bool with_effect, ActorStyles& dressing, std::uint32_t now_ms);
  // La pièce décrite a un effet de chapeau : son illustration montre le pantin.
  // ⚠ APRÈS `Refresh`, qui recompose la vue.
  void NoteHatEffect(bool worn) noexcept;
  // Le cadrage de ce pantin, que son effet de chapeau suit.
  [[nodiscard]] ro::render::VignetteFraming doll_framing() const noexcept;
  // Le corps de ce pantin à cette image, ou nul : voir `ItemDoll::body`.
  [[nodiscard]] const DollBody* doll_body() const noexcept;

  // L'objet d'où l'on vient, pour « < Retour » ; zéro sans retour.
  void NoteBack(std::uint32_t item_id);

  // Le libellé d'une ligne de la table de tirage, vide s'il n'y en a pas.
  [[nodiscard]] std::string_view OddsNameOf(std::uint32_t id) const;

  // ── CE QUE LE SERVEUR SEUL SAIT : les sources et le script ─────────────────
  //
  // Les réponses arrivent ici, et la vue les demande. ⚠ LES SOURCES VIEILLISSENT
  // — un taux bouge avec les événements, le VIP et l'équipement — : une réponse
  // vaut quinze secondes, et se redemande à chaque réouverture de la fenêtre.
  // Le script, lui, ne bouge pas de la session. Une demande sans réponse se
  // rejoue au bout de trois secondes.
  [[nodiscard]] ro::sim::ItemFacts& facts() noexcept { return facts_from_->facts(); }
  void AskFacts(ro::net::Connection& link, bool window_open, std::uint32_t item_id,
                std::uint32_t now_ms);
  // « Rafraichir » d'un onglet de sources : oublier la réponse, et redemander.
  void RefreshSources(std::uint32_t item_id, const ro::ui::DescriptionWindowAction& asked);
  // LE CONTEXTE D'UN RAPPORT DE BUG sur l'item montré : son nom décoré, et le
  // raffinement et les cartes de l'exemplaire tenu quand il y en a un.
  [[nodiscard]] BugReportContext BugReport(std::uint32_t item_id) const;
  // « +/- alootid » : l'item entre dans la liste de ramassage, ou en sort. Noté
  // d'avance — le serveur ne confirme pas un ajout, il ne dément qu'un refus.
  void ToggleAutoloot(ro::net::Connection& link, std::uint32_t item_id);
  // La même liste, dans un SENS voulu : rien ne part quand l'item y est déjà
  // tel qu'on le veut — le menu d'entité, ouvert avant, a pu la voir changer.
  void SetAutoloot(ro::net::Connection& link, std::uint32_t item_id, bool listed);
  // Le nom d'un monstre des sources, vide s'il n'y est pas.
  [[nodiscard]] std::string_view SourceNameOf(std::uint32_t monster_id) const;

 private:
  const ro::vfs::ItemTable* items_ = nullptr;
  const ro::vfs::ItemWords* words_ = nullptr;
  ro::render::ItemIcons* icons_ = nullptr;
  const ro::sim::ViewedEquipmentBook* viewed_ = nullptr;
  const ro::vfs::RandomOptionNames* option_names_ = nullptr;
  ro::vfs::ItemLink linked_;
  ro::sim::InventoryItem linked_item_;

  // CE QUE PORTE L'OBJET TENU : cartes, options, familier.
  void ComposeInstance(const ro::sim::InventoryItem& held);
  // Le nom décoré, et ses morceaux quand le forgeron s'y colore.
  void ComposeName(const ro::sim::InventoryItem& held, bool from_link);
  std::vector<ro::vfs::ItemNameSegment> segments_;
  std::vector<ro::ui::DescriptionNamePiece> name_pieces_;
  std::vector<ro::ui::DescriptionCardRow> cards_;
  std::vector<std::string> option_texts_;
  std::vector<ro::ui::DescriptionOptionRow> options_;

  // ⚠ LES TEXTES SE GARDENT ICI : la vue rendue POINTE dessus, et elle est lue
  // après le retour de cette fonction.
  std::string name_;
  std::vector<std::string> lines_;
  // Les lignes portent le bloc de l'item_db une fois la base lue, et dans la
  // langue où elles ont été composées.
  const ro::vfs::ServerDb* server_db_ = nullptr;
  const ro::vfs::PetTables* pets_ = nullptr;
  const ro::vfs::PetTables* lines_pets_ = nullptr;
  bool lines_with_base_ = false;
  std::size_t lines_language_ = 0;
  std::string craft_label_;
  std::string craft_tip_;
  std::string vote_shop_label_;
  const ro::vfs::PackageItems* packages_ = nullptr;
  const ro::vfs::MonsterList* monsters_ = nullptr;
  void RefreshOdds(std::uint32_t item_id);
  // Le nom d'une entrée de la table : l'objet dans la table des items, le
  // monstre dans celle du serveur. Vide quand ni l'une ni l'autre ne le sait.
  [[nodiscard]] std::string PackageEntryName(const ro::vfs::PackageEntry& entry) const;
  // Les lignes de la table et leurs libellés, refaits au changement d'objet
  // seulement. ⚠ LES LIGNES DÉSIGNENT LES LIBELLÉS : ceux-ci ne bougent plus
  // une fois les lignes posées.
  std::vector<std::string> odds_names_;
  std::vector<ro::ui::DescriptionOddsRow> odds_;
  bool odds_many_groups_ = false;
  std::string back_name_;

  void ComposeFacts(std::uint32_t item_id, bool online);
  ItemFactsAsker asker_;
  ItemFactsAsker* creators_ = &asker_;
  // Les faits du serveur : les siens, ou ceux de la vue empruntée.
  ItemFactsAsker* facts_from_ = &asker_;
  bool facts_window_was_open_ = false;
  std::unique_ptr<DescriptionView> compare_;
  // L'exemplaire tenu, pour le rapport de bug : moins un et rien sans lui.
  int held_refine_ = -1;
  std::uint16_t held_view_ = 0;
  std::uint32_t held_wearable_ = 0;
  std::optional<ItemDoll> doll_;
  BookReader books_;
  std::array<std::uint32_t, 4> held_cards_{};
  std::vector<ro::ui::DescriptionDropRow> ordinary_rows_;
  std::vector<ro::ui::DescriptionDropRow> boss_rows_;
  std::vector<std::vector<ro::ui::DescriptionComboMember>> combo_members_;
  std::vector<ro::ui::DescriptionComboView> combos_;
  std::uint32_t shown_ = 0;
  ro::ui::DescriptionView view_;
};

}  // namespace moonclient
