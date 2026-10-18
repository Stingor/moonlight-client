#include "ro/ui/inventory_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include <imgui_internal.h>

#include "ro/ui/skin_popup.h"

// ⚠ POUR LA SEULE IDENTITÉ DES FENÊTRES DE CHARRETTE ET DE COFFRE : le fantôme
// d'un glisser doit savoir s'il les survole. Aucun cycle — c'est la source qui
// les inclut, et leurs en-têtes ne connaissent que celui du sac.
#include "ro/ui/card_album_window.h"
#include "ro/ui/cart_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/storage_window.h"
#include "ro/ui/window_name.h"

#include "inventory_cell_menu.h"
#include "inventory_footer.h"
#include "item_grid.h"
#include "transfer_freeze.h"

namespace ro::ui {
namespace {

using render::SkinPiece;

// La taille d'ouverture : six colonnes et sept rangées, la fenêtre que le client
// montre au premier lancement.
constexpr float kFirstWidthPixels = 260.0F;
constexpr float kFirstHeightPixels = 330.0F;

// LES SEPT ONGLETS DU SAC, dans l'ordre où le client les empile.
//
// ⚠ LA CHARRETTE N'EN MONTRE QUE SIX — elle n'a pas de favoris —, et c'est la
// seule différence entre leurs deux bandes. La liste se DONNE donc au dessin,
// plutôt que de se déduire d'un compte que les deux ne partagent pas.
constexpr std::array<ItemTab, 7> kInventoryTabs = {
    ItemTab::kAll, ItemTab::kConsumable, ItemTab::kEquipment, ItemTab::kAmmo,
    ItemTab::kEtc, ItemTab::kCard,       ItemTab::kFavorite};

// LES RANGS DE POSE DES RACCOURCIS DE LA BARRE DE TITRE, ceux que relit
// `TitleActionClicked`.
constexpr int kCartRank = 0;
constexpr int kStorageRank = 1;

// Le groupement des montants : trois chiffres, séparés par une virgule.
constexpr std::size_t kThousandsGroup = 3;
constexpr char kThousandsSeparator = ',';

// COMBIEN EN JETER. Une boîte modale, parce qu'un jet ne se rattrape pas.
//
// ⚠ LE DÉFAUT EST UN, JAMAIS LA PILE. Proposer le total ferait tout jeter d'une
// touche à qui valide sans lire — et c'est exactement ce qu'on ne peut pas
// défaire.
// L'invite du sac : la question partagée, avec le verbe qu'elle confirme.
//
// 🔴 LE LIBELLÉ DU BOUTON EST CELUI DU GESTE, jamais « Valider ». Jeter une pile,
// la ranger dans la charrette et la ranger dans le storage se demandent pareil
// et ne se rattrapent pas pareil : le mot est la dernière chose qui les
// distingue avant le clic.
// CE QU'UNE CASE FAIT D'UN LÂCHER, placement libre en cours.
//
// ⚠ ELLE PREND AUSSI CE QUI VIENT D'AILLEURS. Une cible posée sur une case est
// plus PETITE que celle de la grille entière : ImGui lui donne donc le lâcher, et
// refuser ici ne le rendrait pas à la grande — l'objet venu de la charrette
// serait tombé dans le vide.
void TakeCellDrop(int rank, InventoryWindowAction& action) {
  const ItemTransferAsk asked = AcceptItemDrop(ItemHolder::kInventory, /*same=*/true);
  if (asked.client_slot == 0) {
    return;
  }
  if (asked.from == ItemHolder::kInventory) {
    action.placed_slot = asked.client_slot;
    action.placed_cell = rank;
  } else if (asked.from == ItemHolder::kEquipment) {
    action.verb = ItemVerb::kTakeOff;
    action.slot = asked.client_slot;
  } else {
    action.received = asked;
  }
}

// UNE CASE VIDE : UNE PLACE, ET RIEN D'AUTRE. Ni icône, ni badge, ni infobulle,
// ni menu — le fond pavé la dessine déjà. Mais elle REÇOIT, et c'est tout son
// objet.
//
// ⚠ SON IDENTITÉ VIENT DE SON RANG, EN NÉGATIF : une case vide n'a pas de place
// serveur, donc elles porteraient toutes le même zéro et ImGui n'en
// distinguerait plus aucune. Un rang négatif ne peut pas rencontrer une place
// serveur, qui ne l'est jamais.
void DrawEmptyCell(int rank, float cell_side, InventoryWindowAction& action) {
  ImGui::PushID(-rank - 1);
  ImGui::InvisibleButton("case", ImVec2(cell_side, cell_side));
  TakeCellDrop(rank, action);
  ImGui::PopID();
}

void DrawAmountPrompt(std::span<const InventoryCellView> cells, InventoryWindowState& state,
                      InventoryWindowAction& action, const SkinArt& art) {
  const char* confirm = core::Tr("Jeter");
  if (state.asking_verb == ItemVerb::kToCart) {
    confirm = core::Tr("Vers le cart");
  } else if (state.asking_verb == ItemVerb::kToStorage) {
    confirm = core::Tr("Vers le storage");
  } else if (state.asking_verb == ItemVerb::kToMail) {
    confirm = core::Tr("Joindre au courrier");
  }
  ItemQuantityAsk ask{.slot = state.asking_slot, .amount = state.asked_amount};
  const QuantityAnswer answer = DrawQuantityPrompt(cells, ask, confirm, art);
  state.asking_slot = ask.slot;
  state.asked_amount = ask.amount;
  if (answer.slot != 0) {
    action.verb = state.asking_verb;
    action.slot = answer.slot;
    action.amount = answer.amount;
  }
}

}  // namespace

// ⚠ UN LITTÉRAL PAR CAS, ET C'EST LA CONTRAINTE DU RELEVÉ : le catalogue se
// bâtit en lisant les sources, et un libellé rangé dans une table ne s'y lit
// pas. Le contrôle le refuse, et il a raison — un texte qui n'entre dans aucun
// catalogue reste en français dans toutes les langues sans que rien ne le dise.
//
// ⚠ ET LES MOTS SONT FIXÉS À LA LETTRE : le joueur doit retrouver ceux qu'il
// connaît. « Conso » et « Equip » sont abrégés, et le restent.
const char* ItemTabLabel(ItemTab tab) {
  switch (tab) {
    case ItemTab::kAll:
      return core::Tr("Tout");
    case ItemTab::kConsumable:
      return core::Tr("Conso");
    case ItemTab::kEquipment:
      return core::Tr("Equip");
    case ItemTab::kAmmo:
      return core::Tr("Ammo");
    case ItemTab::kEtc:
      return core::Tr("Etc");
    case ItemTab::kCard:
      return core::Tr("Cartes");
    case ItemTab::kFavorite:
      return core::Tr("Favoris");
    // ── LES QUATRE DU COFFRE ──────────────────────────────────────────────
    //
    // ⚠ CE SONT SES MOTS À LUI, et ils ne sont pas abrégés comme « Conso » et
    // « Equip » : sa bande a la place, celle du sac ne l'a pas.
    case ItemTab::kWeapon:
      return core::Tr("Armes");
    case ItemTab::kArmor:
      return core::Tr("Armures");
    case ItemTab::kCostume:
      return core::Tr("Costumes");
    case ItemTab::kCash:
      return core::Tr("Cash");
  }
  return "";
}

std::string GroupThousands(std::int64_t amount) {
  const bool negative = amount < 0;
  // ⚠ LE PLUS PETIT ENTIER N'A PAS D'OPPOSÉ : le nier déborde, et déborder un
  // entier signé est un comportement indéfini. On passe donc par le non signé,
  // où sa valeur absolue tient sans peine.
  std::uint64_t left =
      negative ? static_cast<std::uint64_t>(-(amount + 1)) + 1 : static_cast<std::uint64_t>(amount);

  std::string digits;
  // ⚠ ON COMPTE LES CHIFFRES, PAS LES CARACTÈRES ÉCRITS. Compter la longueur de
  // la chaîne inclurait les séparateurs déjà posés, et les groupes glisseraient
  // d'un cran à chaque virgule.
  std::size_t written = 0;
  do {
    if (written != 0 && written % kThousandsGroup == 0) {
      digits.push_back(kThousandsSeparator);
    }
    digits.push_back(static_cast<char>('0' + left % 10));
    ++written;
    left /= 10;
  } while (left != 0);
  if (negative) {
    digits.push_back('-');
  }
  std::reverse(digits.begin(), digits.end());
  return digits;
}

int ItemGridColumns(float available_width, float cell, float gap) {
  const float step = cell + gap;
  if (step <= 0.0F) {
    return 1;
  }
  const int columns = static_cast<int>((available_width + gap) / step);
  return columns < 1 ? 1 : columns;
}

float ItemBadgeFontSize(float base_size, float text_width, float available_width) {
  if (text_width <= available_width || text_width <= 0.0F || available_width <= 0.0F) {
    return base_size;
  }
  const float floor_size = std::max(1.0F, base_size * kItemBadgeMinimumScale);
  return std::max(floor_size, std::floor(base_size * available_width / text_width));
}

InventoryWindowAction DrawInventoryWindow(InventoryWindowState& state,
                                          const InventoryWindowView& view,
                                          std::span<const InventoryCellView> cells,
                                          InventoryTally tally, const SkinArt& art) {
  InventoryWindowAction action;
  if (!state.open) {
    return action;
  }

  // ⚠ POSÉ AVANT LE `Begin`, comme les `SetNextWindow…` d'ImGui : la barre de
  // titre se peint DANS l'ouverture, et rien ne doit rester à garder entre deux
  // images.
  // 🔴 L'ORDRE DE POSE EST L'INVERSE DE L'ORDRE AFFICHÉ, et l'ordre affiché est
  // celui du client : [Storage][Cart]. Le premier posé va le plus à droite.
  //
  // ⚠ LE RACCOURCI VERS LA CHARRETTE SE GRISE PLUTÔT QUE DE DISPARAÎTRE quand
  // le personnage n'en pousse pas : un bouton absent passe pour un défaut, un
  // bouton éteint dit qu'il manque quelque chose — et son infobulle dit quoi.
  if (state.storage_button) {
    SetNextWindowTitleAction(core::Tr("Cart"),
                             !view.cart_available
                                 ? core::Tr("Vous ne possédez pas de Cart")
                                 : (view.cart_open ? core::Tr("Refermer cette fenêtre")
                                                   : core::Tr("Ouvrir cette fenêtre")),
                             view.cart_available);
    SetNextWindowTitleAction(
        core::Tr("Storage"),
        view.storage_open ? core::Tr("Refermer le storage") : core::Tr("Ouvrir le storage"), true);
  }
  // LE BOUTON DU STAFF, POSÉ EN DERNIER : il se range à gauche des raccourcis
  // sans en déplacer aucun, et c'est lui qui s'efface le premier quand la barre
  // rétrécit. Son rang est celui qui suit les raccourcis présents.
  //
  // 🔴 AUCUNE CONFIRMATION, et c'est voulu : le staff vide son sac entre deux
  // essais. L'infobulle dit ce qui part et ce qui reste.
  const int wipe_rank = state.storage_button ? kStorageRank + 1 : kCartRank;
  if (view.staff_tools) {
    SetNextWindowTitleAction(
        core::Tr("Vider"),
        core::Tr("Vider l'inventory (@itemreset)\nSupprime TOUS les items, sauf l'équipement "
                 "porté, les favoris et l'œuf du familier sorti. Irréversible, et sans "
                 "confirmation."),
        true);
  }
  // ⚠ L'INFOBULLE EST LE SEUL ANNONCEUR : la puce ne change pas d'aspect en
  // devenant cliquable, et c'est le dessin du client.
  SetNextWindowBulletClickable(core::Tr("Réglages de cette fenêtre"));
  SetNextWindowPinnable(&state.pinned);
  static WindowName name(kInventoryWindowTitle, kInventoryWindowId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels, kFirstHeightPixels), ImGuiCond_FirstUseEver);
  // 🔴 LA FENÊTRE SAUTE D'UNE CASE ENTIÈRE, elle n'en coupe jamais une en deux.
  // C'est le dessin du client, et c'est aussi ce qui garde le fond pavé aligné :
  // une largeur quelconque laisserait une bande de tuile tronquée au bord.
  ImGui::SetNextWindowSizeConstraints(ImVec2(0.0F, 0.0F), ImVec2(FLT_MAX, FLT_MAX), SnapToCells,
                                      &state.chrome);
  const ImGuiWindowFlags flags =
      state.size_locked ? ImGuiWindowFlags_NoResize : ImGuiWindowFlags_None;
  const bool body_shown =
      BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar, flags);
  // 🔴 LA PUCE SE LIT AVANT LE DEMI-TOUR, et c'est la seule façon qu'elle
  // fonctionne REPLIÉE. `BeginSkinnedWindow` rend FAUX sur une fenêtre repliée —
  // son corps ne se dessine pas —, et l'on repartirait alors sans jamais
  // demander ce que la barre de titre a répondu. Or elle, elle répond : ses
  // boutons lisent la souris à la main, précisément pour survivre au repli.
  if (TitleBulletClicked()) {
    action.settings_asked = true;
  }
  // ⚠ LES RACCOURCIS, EUX, N'EXISTENT PAS REPLIÉS : ce sont de vrais éléments
  // ImGui, et une fenêtre repliée n'en accepte aucun. Les lire ici ne coûte
  // rien et garde toutes les réponses de la barre au même endroit.
  //
  // ⚠ UN RANG NE VAUT QUE POUR UN BOUTON POSÉ : sans les raccourcis, le rang
  // zéro est celui du staff, et le lire comme « Cart » ouvrirait la charrette à
  // chaque sac vidé.
  if (state.storage_button && TitleActionClicked(kCartRank)) {
    action.toggle_cart = true;
  }
  if (state.storage_button && TitleActionClicked(kStorageRank)) {
    action.toggle_storage = true;
  }
  if (view.staff_tools && TitleActionClicked(wipe_rank)) {
    action.staff_wipe = true;
  }
  if (!body_shown) {
    EndSkinnedWindow();
    return action;
  }

  const float footer_height =
      static_cast<float>(kFooterLines) * ImGui::GetTextLineHeightWithSpacing();

  // 🔴 LE FILTRE EST AU-DESSUS DE TOUT, PLEINE LARGEUR. Rangé à droite des
  // onglets, il repousserait la grille vers le bas SANS repousser la bande :
  // les deux ne partageraient plus de bord.
  if (state.filter_shown) {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##filtre", core::Tr("Filtrer..."), state.filter,
                             sizeof(state.filter));
  }
  DrawTransfersFrozenBanner(view.transfers_frozen);

  // ⚠ SEULE DES TROIS À PRENDRE UN LÂCHER SUR SES ONGLETS : elle est seule à
  // savoir ce que cela veut dire — ranger dans les favoris, ou les en sortir.
  const TabStripLook tabs_look{.lying = !state.tabs_vertical, .images = true, .takes_drops = true};
  const float tabs_thickness = ItemTabsThickness(art, kInventoryTabs, tabs_look);
  const TabStripResult strip =
      DrawItemTabs(kInventoryTabs, state.tab, art, tabs_look, tabs_thickness, -footer_height);
  const ItemTab chosen = strip.chosen;
  // 🔴 LÂCHER SUR UN ONGLET RANGE, et le sens se lit sur les DEUX : l'onglet où
  // l'on lâche, et ce que l'objet est déjà. Tirer un favori sur « Favoris » ne
  // fait rien, et tirer un objet ordinaire sur « Etc » non plus — le client
  // n'envoie rien dans ces cas-là, et un paquet qui ne change rien se voit dans
  // le journal du serveur.
  if (strip.dropped_slot != 0) {
    const auto found =
        std::ranges::find(cells, strip.dropped_slot, &InventoryCellView::client_slot);
    if (found != cells.end()) {
      const bool wanted = strip.dropped_on == ItemTab::kFavorite;
      if (found->favorite != wanted) {
        action.verb = ItemVerb::kToggleFavorite;
        action.slot = strip.dropped_slot;
      }
    }
  }
  // ⚠ LA GRILLE NE REJOINT LA BANDE SUR SA LIGNE QUE SI ELLE EST DEBOUT. Couchée,
  // la rangée a pris sa ligne à elle et la grille lui succède dans le flux.
  if (state.tabs_vertical) {
    ImGui::SameLine(0.0F, 0.0F);
  }

  // ⚠ RELEVÉ AVANT LES POUSSÉES DE LA GRILLE : un menu ouvert dedans hériterait
  // de la marge nulle et de l'espacement jointif qui collent les cases.
  const ImVec2 menu_padding = ImGui::GetStyle().WindowPadding;
  const ImVec2 menu_spacing = ImGui::GetStyle().ItemSpacing;
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kCellGapPixels, kCellGapPixels));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  // 🔴 L'ASCENSEUR NE PARAÎT QUE S'IL SERT, SAUF TAILLE VERROUILLÉE : voir
  // `GridScrollFlags`.
  // 🔴 LA GRILLE PORTE UNE BORDURE, et c'est le dessin du client : un liseré d'un
  // pixel autour des cases, qui sépare le fond pavé du corps de la fenêtre. Sans
  // elle la grille flotte, et surtout le pont de l'onglet actif n'a plus rien à
  // manger — les sept onglets paraissent alors tous fermés.
  const bool grid_open =
      ImGui::BeginChild("grille", ImVec2(0.0F, -footer_height), ImGuiChildFlags_Borders,
                        GridScrollFlags(state.size_locked));
  // ⚠ DEDANS, ET AVANT LE DEMI-TOUR : une grille entièrement rognée ne dessine
  // rien mais garde sa gouttière, et un chrome mesuré sans elle ferait sauter la
  // fenêtre d'une case à l'image suivante.
  const float scrollbar_gutter = GridScrollbarGutter();
  if (grid_open) {
    ImDrawList* const into = ImGui::GetWindowDrawList();
    const float cell_side = kItemCellPixels;
    const int columns =
        ItemGridColumns(ImGui::GetContentRegionAvail().x, cell_side, kCellGapPixels);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    // ⚠ RELEVÉE AVANT LA PREMIÈRE CASE : après, le curseur a descendu et la
    // place restante ne dit plus la hauteur de la grille.
    const float grid_height = ImGui::GetContentRegionAvail().y;
    // ⚠ JUSQU'À LA ZONE DE CONTENU, PAS JUSQU'AU BORD DE L'ENFANT. Pavé sur
    // toute la largeur, le fond passerait SOUS la gouttière de l'ascenseur : on
    // y verrait des cases que rien ne pourrait atteindre.
    const ImVec2 corner(origin.x + ImGui::GetContentRegionAvail().x,
                        ImGui::GetWindowPos().y + ImGui::GetWindowSize().y);
    // 🔴 LE FOND CHANGE SUR LES FAVORIS, VERROU DE VENTE MIS. C'est le seul
    // écart de dessin entre la grille du sac et celle de la charrette, et c'est
    // ce qui rend le verrou VISIBLE sans rien écrire : on voit d'un coup d'œil
    // que cette liste-là est protégée.
    //
    // ⚠ REPLI SUR LA TUILE ORDINAIRE si l'installation n'a pas la verrouillée :
    // une grille sans fond serait pire que le mauvais fond.
    const SkinImage& locked_tile = art.Get(SkinPiece::kItemTileLocked);
    const bool tiles_locked = chosen == ItemTab::kFavorite && state.sell_locked;
    const SkinImage& tile =
        tiles_locked && locked_tile.usable() ? locked_tile : art.Get(SkinPiece::kItemTile);
    DrawTiledBackground(into, tile, origin,
                        ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y), corner,
                        art.look());

    int column = 0;
    int cell_rank = -1;
    for (const InventoryCellView& cell : cells) {
      cell_rank = cell_rank + 1;
      if (column > 0) {
        ImGui::SameLine(0.0F, kCellGapPixels);
      }
      // ⚠ L'IDENTITÉ VIENT DE LA PLACE SERVEUR, pas du rang d'affichage : un tri
      // ou un objet consommé renumérote l'affichage, et un état ImGui accroché
      // au rang sauterait d'un objet à l'autre.
      //
      // ⚠ SAUF POUR UNE CASE VIDE, qui n'a pas de place serveur : les vides
      // porteraient toutes le même zéro, et ImGui n'en distinguerait plus
      // aucune. Leur rang NÉGATIF ne peut pas rencontrer une place serveur, qui
      // ne l'est jamais.
      ImGui::PushID(cell.client_slot != 0 ? static_cast<int>(cell.client_slot) : -cell_rank - 1);
      const ImVec2 top_left = ImGui::GetCursorScreenPos();
      // ⚠ LES DEUX BOUTONS, et il faut le demander : un bouton invisible n'écoute
      // que le gauche par défaut, et le menu contextuel n'aurait alors jamais de
      // clic à recevoir.
      ImGui::InvisibleButton("case", ImVec2(cell_side, cell_side),
                             ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
      const ImVec2 bottom_right(top_left.x + cell_side, top_left.y + cell_side);
      // UNE CARTE LÂCHÉE SUR UNE PIÈCE LA SERTIT, et passe avant le placement
      // libre : sur une pièce, c'est le geste qu'on attend d'une carte.
      const CardDropTaken card_drop =
          cell.takes_cards ? TakeCardDrop(cell.client_slot, view.card_drop, action.card_drop)
                           : CardDropTaken{};
      // UNE CASE PLEINE REÇOIT AUSSI, et c'est ce qui fait l'ÉCHANGE : l'objet
      // qui s'y trouvait prend la case que l'autre quitte.
      if (state.free_layout && !card_drop.taken) {
        TakeCellDrop(cell_rank, action);
      }
      if (cell.client_slot == 0) {
        // Un trou laissé par le placement libre : rien à dessiner de plus.
        ImGui::PopID();
        column = column + 1 >= columns ? 0 : column + 1;
        continue;
      }
      if (ImGui::IsItemHovered()) {
        action.hovered_slot = cell.client_slot;
        action.hovered_id = cell.item_id;
        into->AddRectFilled(top_left, bottom_right, kItemHoverVeil, 0.0F);
      }
      // LE GLISSER D'UN OBJET.
      //
      // ⚠ AVANT LE DESSIN, et il faut que ce soit avant : ImGui rattache la
      // source au DERNIER objet posé, et la déclarer plus bas l'accrocherait à
      // l'infobulle.
      //
      // 🔴 LE COFFRE OUVERT MANGE L'ÉCHANGE SAC <-> CHARRETTE, et le serveur le
      // refuse SANS RIEN DIRE. Le fantôme le dit donc AVANT le lâcher — le seul
      // moment où l'on peut encore renoncer.
      //
      // ⚠ C'EST LA SOURCE QUI TRANCHE, pas la cible : une cible ne parle qu'au
      // lâcher, et il serait alors trop tard.
      // Le gel passe devant, seulement vers la charrette ou le coffre : le sac
      // garde ses autres destinations.
      const bool over_container =
          MouseOverWindow(kCartWindowId) || MouseOverWindow(kStorageWindowId);
      const bool card_refused = view.card_drop.look == CardDropLook::kRefused &&
                                view.card_drop.card_slot == cell.client_slot;
      const char* refusal = view.transfers_frozen && over_container ? TransfersFrozenText()
                            : view.storage_open && MouseOverWindow(kCartWindowId)
                                ? core::Tr("Storage ouvert : vers le cart impossible")
                            : card_refused ? view.card_drop.refusal
                                           : nullptr;
      DragItemFrom(cell, ItemHolder::kInventory, menu_padding, refusal);
      DrawIcon(into, cell, top_left, cell_side);
      DrawBadge(into, cell, top_left, bottom_right);
      if (card_drop.taken) {
        PaintCardDropFrame(into, top_left, bottom_right, card_drop.look);
      }
      // ⚠ L'APERÇU REMPLACE CETTE INFOBULLE-CI, il ne s'y ajoute pas : c'est
      // l'étage qui le dessine, après toutes les fenêtres, parce qu'une
      // description n'appartient à aucune d'elles.
      if (!state.hover_preview && ImGui::IsItemHovered()) {
        DrawCellTooltip(cell, menu_padding);
      }
      // LE DOUBLE-CLIC : consommer ou porter, et RETIRER si l'on porte déjà.
      // C'est le même geste dans le client, et c'est le type de l'objet qui
      // décide — la fenêtre ne fait que rapporter lequel.
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (cell.worn) {
          action.verb = ItemVerb::kTakeOff;
          action.slot = cell.client_slot;
        } else if (cell.usable || cell.wearable || cell.pet_accessory) {
          action.verb = ItemVerb::kUseOrWear;
          action.slot = cell.client_slot;
          // CTRL : dans l'AUTRE MAIN. C'est le geste du client, et il ne se
          // distingue du premier que par cette touche.
          action.alternate = ImGui::GetIO().KeyCtrl;
        } else if (cell.card) {
          action.verb = ItemVerb::kInsertCard;
          action.slot = cell.client_slot;
        }
      }
      // MAJ + CLIC GAUCHE : le lien de l'objet dans la saisie du chat.
      //
      // ⚠ À L'ENFONCEMENT, contrairement aux gestes du bouton droit, et c'est
      // celui du client : rien ne s'ouvre ici, donc il n'y a pas de fenêtre à
      // faire naître derrière celle-ci.
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyShift) {
        action.link_slot = cell.client_slot;
      }
      // LES DEUX GESTES QUI COURT-CIRCUITENT LE MENU.
      //
      // ⚠ AVANT LUI, et il faut que ce soit avant : le menu s'ouvre sur le même
      // bouton, et il prendrait le clic pour lui.
      //
      // 🔴 AU RELÂCHEMENT, JAMAIS À L'ENFONCEMENT. La fenêtre de description naît
      // à l'image suivante : partie de l'enfoncement, elle naîtrait pendant que
      // le bouton est encore tenu sur CETTE fenêtre-ci, qui reprendrait le
      // premier plan — et la description s'ouvrirait DERRIÈRE elle.
      //
      // ⚠ ET C'EST LE BORD DU MENU. `BeginPopupContextItem` s'ouvre lui aussi au
      // relâchement : sur l'enfoncement, les deux gestes ne se départageraient
      // plus par leur touche mais par leur instant.
      const ImGuiIO& keys = ImGui::GetIO();
      const bool right_clicked =
          ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right);
      if (right_clicked && keys.KeyCtrl) {
        // CTRL : la description, sans passer par le menu. C'est le geste du
        // client, et le plus employé des trois.
        action.describe_id = cell.item_id;
        action.describe_slot = cell.client_slot;
      } else if (right_clicked && keys.KeyShift) {
        // MAJ : bascule le favori, comme le client.
        action.verb = ItemVerb::kToggleFavorite;
        action.slot = cell.client_slot;
      } else if (right_clicked && keys.KeyAlt) {
        // 🔴 ALT : LE TRANSFERT RAPIDE, pile entière et sans question. Il vise le
        // storage dès qu'il est ouvert, la charrette sinon — c'est l'ordre du
        // client, et il a sa raison : une session de coffre est rare et courte,
        // et c'est elle qu'on vient servir. Rien d'ouvert, rien ne part : le
        // geste n'a pas de libellé à lire, il ne doit donc jamais aboutir
        // ailleurs qu'où le joueur regarde.
        if (view.storage_open) {
          action.verb = ItemVerb::kToStorage;
          action.slot = cell.client_slot;
          action.amount = cell.amount;
        } else if (view.cart_open && view.cart_available) {
          action.verb = ItemVerb::kToCart;
          action.slot = cell.client_slot;
          action.amount = cell.amount;
        }
      } else {
        DrawInventoryCellMenu(cell, menu_padding, menu_spacing, action, state, view);
      }
      ImGui::PopID();
      column = column + 1 >= columns ? 0 : column + 1;
    }

    // 🔴 LA GRILLE SE COMPLÈTE EN PLACEMENT LIBRE, ET SANS CELA LE RÉGLAGE NE
    // SERT QU'À MOITIÉ. Les cases que l'étage donne s'arrêtent au dernier objet
    // — c'est sa règle, et elle est juste : il ne connaît ni la largeur ni la
    // hauteur de la grille. Mais alors il n'y a NULLE PART OÙ POSER : le seul
    // geste possible serait d'échanger deux objets.
    //
    // ⚠ JUSQU'AU BAS DE CE QUI SE VOIT, jamais plus loin. La taille est
    // verrouillée — le placement libre l'exige —, donc la zone visible EST la
    // grille : des cases au-delà ne s'atteindraient jamais, et leur hauteur
    // ouvrirait un ascenseur qui n'aurait rien à faire défiler.
    //
    // ⚠ ET LA DERNIÈRE LIGNE SE TERMINE, même quand les objets débordent déjà :
    // une ligne coupée en deux laisserait un bout de fond pavé où l'on croirait
    // pouvoir poser.
    if (state.free_layout) {
      const int shown = static_cast<int>(cells.size());
      const int rows_visible = std::max(1, static_cast<int>(grid_height / cell_side));
      const int rows_used = (shown + columns - 1) / columns;
      const int wanted = std::max(rows_visible, rows_used) * columns;
      for (int rank = shown; rank < wanted; ++rank) {
        if (column > 0) {
          ImGui::SameLine(0.0F, kCellGapPixels);
        }
        DrawEmptyCell(rank, cell_side, action);
        column = column + 1 >= columns ? 0 : column + 1;
      }
    }
    PaintWindowScrollbar(art);
  }
  ImGui::EndChild();
  // 🔴 CE QU'ON RAMÈNE DANS LE SAC. Deux gestes tombent ici : une pièce PORTÉE
  // qu'on ôte, et un objet d'un autre contenant qu'on rapatrie. Le premier se
  // lit sur l'origine de la charge, le second aussi.
  //
  // ⚠ APRÈS `EndChild`, et il faut que ce soit après : la cible se pose sur le
  // DERNIER élément soumis, et l'enfant n'en est un qu'une fois refermé.
  if (const ItemTransferAsk asked = AcceptItemDrop(ItemHolder::kInventory);
      asked.client_slot != 0) {
    if (asked.from == ItemHolder::kEquipment) {
      action.verb = ItemVerb::kTakeOff;
      action.slot = asked.client_slot;
    } else {
      action.received = asked;
    }
  }
  // UNE POCHETTE DE L'ALBUM LÂCHÉE SUR LE SAC : la reprendre. ⚠ SA CHARGE A SON
  // PROPRE NOM — une pochette n'est pas une place de sac —, et c'est l'album qui
  // demandera combien.
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kCardAlbumDragPayload);
        dropped != nullptr && dropped->DataSize == static_cast<int>(sizeof(std::uint32_t))) {
      action.album_withdraw = *static_cast<const std::uint32_t*>(dropped->Data);
    }
    ImGui::EndDragDropTarget();
  }
  // La mesure que le calage relira : tout ce qui n'est pas des CASES.
  //
  // 🔴 LA GOUTTIÈRE DE L'ASCENSEUR EN FAIT PARTIE : le rectangle de l'enfant la
  // contient, si bien que sans elle une fenêtre calée sur sept cases n'en
  // logerait que six, et garderait une bande de fond pavé de dix-huit pixels
  // contre l'ascenseur — trop étroite pour une case, donc morte.
  //
  // ⚠ APRÈS LA FERMETURE, jamais avant : l'enfant n'est un ITEM de la fenêtre
  // qu'une fois refermé, et mesurer plus tôt rendrait la taille du dernier objet
  // dessiné à l'intérieur.
  state.chrome = ImVec2(ImGui::GetWindowSize().x - ImGui::GetItemRectSize().x + scrollbar_gutter,
                        ImGui::GetWindowSize().y - ImGui::GetItemRectSize().y);
  ImGui::PopStyleVar(2);
  // ⚠ APRÈS L'ENFANT, jamais dedans : c'est la bordure QU'IL VIENT DE PEINDRE
  // qu'il s'agit de manger, et un pont posé avant passerait dessous.
  DrawActiveTabBridge(ImGui::GetWindowDrawList(), strip, art.look());

  // ⚠ LE BANDEAU S'ARRÊTE DANS LA MARGE DE LA FENÊTRE, il ne touche pas son
  // bord. Étalé d'un bord à l'autre, il passerait sous le filet du cadre et la
  // fenêtre paraîtrait fendue en bas.
  const float padding = ImGui::GetStyle().WindowPadding.x;
  const ImVec2 footer_top(ImGui::GetWindowPos().x + padding, ImGui::GetCursorScreenPos().y);
  const ImVec2 footer_bottom(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x - padding,
                             footer_top.y + footer_height);
  DrawInventoryFooter(art, chosen, tally, footer_top, footer_bottom, state, action);

  // EN DERNIER : c'est la seule chose de cette fenêtre à laquelle le joueur DOIT
  // répondre, et elle doit passer devant la grille qui l'a ouverte.
  DrawAmountPrompt(cells, state, action, art);

  EndSkinnedWindow();
  state.tab = chosen;
  if (!state.filter_shown) {
    state.filter[0] = '\0';
  }
  return action;
}

}  // namespace ro::ui
