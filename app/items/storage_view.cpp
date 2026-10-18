#include "items/storage_view.h"

#include <algorithm>
#include <cstdint>
#include <ranges>

#include "ro/core/log.h"
#include "ro/net/chat_target.h"
#include "ro/net/inventory.h"
#include "ro/net/storage_list.h"
#include "ro/sim/storage_facts.h"
#include "ro/sim/storage_shelf.h"
#include "ro/sim/world.h"

#include "game/skin_image_of.h"
#include "items/inventory_view.h"

namespace moonclient {
namespace {

// ⚠ PAS TRADUITE, ET SURTOUT PAS. C'est une commande que le SERVEUR lit, pas un
// texte que le joueur relit : la traduire la rendrait inconnue de lui.
constexpr const char* kOpenStorageCommand = "@storage";

// LE COFFRE QU'UNE BASCULE OUVRE : le PRINCIPAL quand le personnage y a droit,
// le premier de sa liste sinon.
//
// ⚠ LE PRINCIPAL N'EST PAS TOUJOURS LÀ. La liste est filtrée par les droits, et
// un compte à qui « @storage » est refusé peut n'avoir que des coffres
// alternatifs. Demander zéro d'office ne ferait alors rien du tout — le serveur
// ignore un numéro auquel on n'a pas droit, en silence.
[[nodiscard]] std::uint8_t MainShelfOf(const ro::sim::World& world) {
  for (const ro::sim::StorageShelf& shelf : world.storage_shelves()) {
    if (shelf.id == ro::sim::kMainStorageId) {
      return ro::sim::kMainStorageId;
    }
  }
  return world.storage_shelves().front().id;
}

}  // namespace

StorageView::StorageView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                         ro::render::ItemIcons* icons)
    : items_(items), words_(words), icons_(icons) {}

void StorageView::Refresh(const ro::sim::World& world, ro::ui::ItemTab tab, std::string_view filter,
                          const StorageFavorites* favorites, int sub_category,
                          const StorageTabNames* tab_names) {
  favorites_ = favorites;
  const ro::sim::Inventory& storage = world.storage();
  // 🔴 SES FAVORIS SONT À NOUS. Le serveur n'en marque aucun dans un coffre —
  // sa structure n'a pas le champ —, alors qu'il retient ceux du sac. Sans cette
  // liste-ci, l'onglet ne pourrait rien montrer et le drapeau venu du fil
  // ferait disparaître des objets vers un onglet vide.
  BuildItemCells(storage,
                 ItemCellsAsk{.tab = tab,
                              .filter = filter,
                              .favorites_tab = true,
                              .fine_tabs = true,
                              .client_favorites = favorites_,
                              .creators = creators_},
                 items_, words_, icons_, names_, cells_);

  // ⚠ DES VUES SUR LE MONDE, refaites à chaque image : la liste du monde change
  // à chaque ouverture, et une vue gardée d'une image à l'autre pendrait.
  shelves_.clear();
  shelves_.reserve(world.storage_shelves().size());
  for (const ro::sim::StorageShelf& shelf : world.storage_shelves()) {
    // ⚠ LE NOM DU JOUEUR L'EMPORTE, et un nom vide rend celui du serveur : c'est
    // ainsi qu'on annule un renommage, sans bouton de plus.
    const StorageTabName* const told = tab_names == nullptr ? nullptr : tab_names->Of(shelf.id);
    // ⚠ LES DEUX NOMS PARTENT : l'onglet écrit celui du JOUEUR — ou le numéro —,
    // l'infobulle celui du SERVEUR. Un seul champ aurait obligé la fenêtre à
    // choisir, et elle ne peut pas : « Storage Alt 3 » ne tient pas dans un
    // cadre de la taille d'une icône.
    ro::ui::StorageShelfView view{
        .id = shelf.id,
        .name = shelf.name,
        .own_name = told != nullptr ? std::string_view(told->name) : std::string_view()};
    // ⚠ L'IDENTIFIANT PART MÊME QUAND L'ICÔNE MANQUE : le menu distingue « rien
    // n'a été demandé » de « demandé, mais l'installation ne l'a pas », et les
    // deux silences ne se disent pas du même mot.
    if (told != nullptr) {
      view.icon_item_id = told->icon_item_id;
    }
    if (told != nullptr && told->icon_item_id != 0 && icons_ != nullptr) {
      const ro::render::IconTexture& icon = icons_->Get(told->icon_item_id);
      view.icon = SizedSkinImageOf(icon);
    }
    shelves_.push_back(view);
  }

  // ── LES SOUS-TYPES, PUIS LE FILTRE QU'ILS OFFRENT ──────────────────────────
  //
  // 🔴 LA LISTE SE BÂTIT AVANT LE FILTRAGE, jamais après : bâtie sur les lignes
  // déjà filtrées, elle ne porterait plus que le sous-type choisi — et le
  // joueur ne pourrait plus en changer sans repasser par « Tous ».
  //
  // ⚠ ELLE NE PORTE QUE CE QUI EST PRÉSENT, et elle vient des faits du SERVEUR :
  // tant qu'il n'a rien dit, elle reste vide et le combo ne paraît pas.
  const SubCategoryKind kind = SubCategoryKindOf(tab);
  sub_views_.clear();
  if (kind != SubCategoryKind::kNone) {
    for (const ro::ui::InventoryCellView& cell : cells_) {
      const SubCategory sub = SubCategoryOf(kind, world.storage_facts().Of(cell.item_id));
      if (!sub.told()) {
        continue;
      }
      const auto place =
          std::ranges::lower_bound(sub_views_, sub.key, {}, &ro::ui::StorageSubCategoryView::key);
      if (place != sub_views_.end() && place->key == sub.key) {
        continue;
      }
      sub_views_.insert(place, ro::ui::StorageSubCategoryView{.key = sub.key, .label = sub.label});
    }
  }
  if (kind != SubCategoryKind::kNone && sub_category != ro::ui::kAnySubCategory) {
    const auto elsewhere = [&](const ro::ui::InventoryCellView& cell) {
      return SubCategoryOf(kind, world.storage_facts().Of(cell.item_id)).key != sub_category;
    };
    // ⚠ LES NOMS NE BOUGENT PAS : chaque case garde une vue sur `names_`, que ce
    // retrait NE TOUCHE PAS. Retirer aussi les noms apparierait chaque case au
    // nom de sa voisine.
    const auto removed = std::ranges::remove_if(cells_, elsewhere);
    cells_.erase(removed.begin(), removed.end());
  }

  // CE QUE CHAQUE PILE SE REVEND, et ce que tout ce qui se voit vaut.
  //
  // 🔴 LE SERVEUR EST SEUL À LE SAVOIR — `value_sell` vit dans sa base, et la
  // table de l'installation n'en porte rien. Sa table arrive APRÈS la liste du
  // coffre : une image durant, les prix manquent, et la fenêtre doit le dire
  // plutôt qu'écrire zéro.
  //
  // ⚠ LE TOTAL SUIT L'ONGLET ET LE FILTRE : il additionne ce qui SE VOIT. Un
  // total qui compterait tout le coffre répondrait à une question que personne
  // n'a posée devant une liste filtrée.
  std::int64_t shown_value = 0;
  for (ro::ui::InventoryCellView& cell : cells_) {
    const ro::sim::StorageItemFacts* const facts = world.storage_facts().Of(cell.item_id);
    cell.value = facts == nullptr ? 0 : static_cast<std::int64_t>(facts->sell_price) * cell.amount;
    shown_value += cell.value;
  }

  name_ = storage.name();
  tally_ = ro::ui::StorageTally{
      .item_count = static_cast<int>(storage.size()),
      .capacity = storage.capacity(),
      // ⚠ CE QUE L'ONGLET ET LE FILTRE LAISSENT VOIR, qui n'est pas le compte du
      // coffre : un coffre plein dont l'onglet ne rend rien se lit autrement
      // qu'un coffre vide.
      .shown_count = static_cast<int>(cells_.size()),
      .shown_value = shown_value,
      // ⚠ « LE SERVEUR A-T-IL PARLÉ ? » et non « le total est-il non nul » : un
      // coffre qui ne contiendrait que des objets sans valeur vaut zéro, et
      // c'est une réponse.
      .value_known = !world.storage_facts().empty(),
  };
}

void StorageView::Apply(const ro::ui::StorageWindowAction& asked, ro::net::Connection& link,
                        const ro::sim::World& world) {
  // ⚠ LA FERMETURE NE PARLE D'AUCUNE CASE : elle part telle quelle, et c'est le
  // serveur qui refermera — la fenêtre ne disparaît qu'à sa réponse.
  if (asked.verb == ro::ui::StorageVerb::kClose) {
    const ro::core::Result<void> sent = ro::net::RequestCloseStorage(link);
    if (!sent) {
      ro::core::LogWarning("coffre : fermeture non émise — {}", ro::core::Describe(sent.error()));
    }
    return;
  }
  // ⚠ CHANGER DE COFFRE N'EST PAS UN VERBE D'OBJET, et c'est pourquoi il se lit
  // AVANT la garde de case : il ne désigne rien de ce que la fenêtre montre.
  if (asked.chose_shelf >= 0) {
    const ro::core::Result<void> sent =
        ro::net::RequestOpenStorage(link, static_cast<std::uint8_t>(asked.chose_shelf));
    if (!sent) {
      ro::core::LogWarning("coffre : ouverture non émise — {}", ro::core::Describe(sent.error()));
    }
    return;
  }
  if (asked.verb == ro::ui::StorageVerb::kNothing || asked.slot == 0) {
    return;
  }
  // 🔴 LA PLACE SE REVÉRIFIE, on ne se fie pas au clic. Une invite de quantité
  // laisse tout le temps au coffre de se vider autrement — ou de se fermer.
  const ro::sim::InventoryItem* item = world.storage().Find(asked.slot);
  if (item == nullptr) {
    return;
  }
  const int amount = std::clamp(asked.amount, 1, static_cast<int>(item->amount));
  // ⚠ LA CASE EST CELLE DU COFFRE, et le serveur ne lui retranche qu'un là où il
  // en retranche deux à celle du sac. C'est le contenant d'où l'objet PART qui
  // la donne.
  const ro::core::Result<void> sent =
      asked.verb == ro::ui::StorageVerb::kToCart
          ? ro::net::RequestStorageToCart(link, item->client_slot, amount)
          : ro::net::RequestStorageToInventory(link, item->client_slot, amount);
  if (!sent) {
    ro::core::LogWarning("coffre : demande non émise — {}", ro::core::Describe(sent.error()));
  }
}

void StorageView::ToggleSession(ro::net::Connection& link, const ro::sim::World& world) {
  // ⚠ RIEN N'EST SUPPOSÉ D'AVANCE. Le serveur peut refuser l'ouverture — droits
  // du compte, échange en cours, coffre de guilde déjà ouvert — et son refus
  // arrive dans la conversation, comme pour la commande tapée à la main. La
  // fenêtre ne s'ouvre qu'à sa liste.
  // ⚠ ET LE PAQUET DÈS QUE LE SERVEUR A DIT SES COFFRES : il BASCULE, là où la
  // commande referme quand un coffre est déjà ouvert. La commande reste le
  // repli — un serveur sans l'extension, ou une liste qui n'est pas arrivée.
  const ro::core::Result<void> sent =
      world.storage_open() ? ro::net::RequestCloseStorage(link)
      : !world.storage_shelves().empty()
          ? ro::net::RequestOpenStorage(link, MainShelfOf(world))
          : ro::net::SendServerCommand(link, world.player_name(), kOpenStorageCommand);
  if (!sent) {
    ro::core::LogWarning("coffre : bascule non émise — {}", ro::core::Describe(sent.error()));
  }
}

}  // namespace moonclient
