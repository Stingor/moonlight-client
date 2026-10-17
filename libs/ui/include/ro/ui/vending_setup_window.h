#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── LA PRÉPARATION DE L'ÉCHOPPE DE VENTE ────────────────────────────────────
//
// Le nom, le stock du cart en liste ou en grille, les lignes posées avec leur
// quantité et leur prix, les totaux, et le bouton qui ouvre. Poser : bouton,
// double-clic ou glisser ; retirer : « x » ou glisser en retour.
//
// 🔴 CE N'EST PAS LA FENÊTRE QUI OUVRE L'ÉCHOPPE. Elle s'ouvre quand le serveur
// annonce la préparation (la compétence), et sa réponse part vers `app`, qui
// REJUGE avant d'émettre. Fermée — croix, Échap, « Annuler » —, elle est un
// ABANDON que `app` dit au serveur : sans lui, le cart resterait figé.
//
// Quatre gardes de sûreté :
// - la QUANTITÉ d'une ligne se règle sur place, au lieu d'être figée à la pose ;
// - le PRIX accepte les séparateurs de milliers, et un prix illisible retient le
//   bouton au lieu de valoir zéro ;
// - le TITRE se montre coupé comme le fil le coupera, avec les octets qui
//   restent ;
// - un glisser porte la CASE du cart, pas un rang qui aurait pu bouger.

inline constexpr const char* kVendingSetupWindowId = "###vending_setup";

// Le texte d'un prix, NUL compris : douze chiffres et leurs séparateurs.
inline constexpr std::size_t kVendingPriceTextBytes = 24;
// Le titre tapé, NUL compris. ⚠ PLUS LONG QUE LE FIL : c'est `app` qui dit où
// il sera coupé, et le joueur doit voir ce qui tombe.
inline constexpr std::size_t kVendingTitleTextBytes = 256;

// LE SEUIL DU « SAFE CHECK » DU CLIENT D'ORIGINE : à partir de dix millions, un
// prix se confirme, montant écrit en toutes lettres.
inline constexpr std::int64_t kVendingHighPriceZeny = 10'000'000;

// UNE LIGNE DU CART, telle que le stock la montre.
struct VendingStockView {
  TradeItemView item;
  // La case du cart vue du client. ⚠ JAMAIS ZÉRO : le client décale les cases
  // du serveur de deux, et zéro veut dire « rien » dans les réponses.
  std::uint16_t cart_slot = 0;
  // Ce que la pile contient.
  int amount = 0;
  int refine_level = 0;
  // Pourquoi la pile ne peut pas se mettre en vente, déjà traduit — non
  // identifiée, abîmée, louée ; vide si elle le peut.
  std::string_view refusal;
};

// UNE LIGNE POSÉE, telle que le joueur la règle.
struct VendingSetupLineState {
  std::uint16_t cart_slot = 0;
  int amount = 1;
  std::array<char, kVendingPriceTextBytes> price_text{};
};

struct VendingSetupView {
  std::span<const VendingStockView> stock;
  // Les lignes que le serveur acceptera.
  std::size_t permitted_lines = 0;
  // Le titre TEL QU'IL PARTIRA, et ce qui reste d'octets sur le fil.
  std::string_view fitted_title;
  std::size_t title_bytes_left = 0;
  bool title_cut = false;
  // Ce que le joueur possède, pour les totaux ; `kTradeNoPurse` s'il n'est pas
  // dit.
  std::int64_t zeny = kTradeNoPurse;
  // La recette si tout se vend.
  std::int64_t takings = 0;
  // LE VERDICT : les raisons, déjà traduites, qui retiennent le bouton ; et
  // celle de chaque ligne posée, au même rang que `state.lines`, vide pour une
  // ligne saine.
  std::span<const std::string_view> blockers;
  std::span<const std::string_view> line_faults;
  // La demande est partie et attend sa réponse.
  bool awaiting = false;
  // LA DERNIÈRE ÉCHOPPE retenue pour ce personnage : vide s'il n'y en a pas.
  std::string_view resume_title;
};

struct VendingSetupWindowState {
  // Recopié par `app` ; la croix, Échap et « Annuler » le baissent.
  bool open = false;
  std::uint32_t preparation_seen = 0;
  // ⚠ LE NOM SURVIT D'UNE PRÉPARATION À L'AUTRE : un
  // marchand rouvre souvent la même échoppe.
  std::array<char, kVendingTitleTextBytes> title{};
  std::vector<VendingSetupLineState> lines;
  // RÉGLAGES DU JOUEUR, que `app` garde sur le disque : le stock en grille, et
  // la confirmation des prix élevés.
  bool grid = false;
  bool confirm_high_prices = true;
  // Combien poser, par case du cart — en liste.
  std::vector<std::pair<std::uint16_t, int>> stock_amounts;
  // La case du cart dont la grille demande combien poser, zéro pour aucune.
  std::uint16_t ask_slot = 0;
  int ask_amount = 1;
  // Une ouverture attend la confirmation de ses prix élevés.
  bool confirming = false;
  // LA DISPOSITION MESURÉE à l'image précédente : la hauteur du pied, et celle
  // qu'occupent les lignes posées. Le surplus va au stock.
  float footer_pixels = 0.0F;
  float lines_content_pixels = 0.0F;
  // Le menu qu'un clic droit sur un item a armé ; son jeton est la case du cart.
  ItemLinkMenu item_menu;
};

struct VendingSetupAction {
  // Abandon : la croix, Échap ou « Annuler ». ⚠ AUSSI ÉCRIT DANS `state.open`.
  bool cancel = false;
  // Le joueur ouvre, prix élevés confirmés. ⚠ `app` REJUGE.
  bool open_shop = false;
  // « Reprendre la dernière échoppe ».
  bool resume = false;
  // Un réglage a bougé : `app` l'écrit sur le disque.
  bool settings_changed = false;
  // La case du cart survolée, et celle qu'un geste vise ; zéro pour aucune.
  std::uint16_t hovered_slot = 0;
  std::uint16_t describe_slot = 0;
  // Ce que le geste demande de l'item visé : sa description, son lien dans la
  // saisie (Maj + clic), ou une autre entrée du menu des objets.
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// Remet la fenêtre à l'état d'une préparation neuve. Le nom et les réglages
// restent.
void OpenVendingSetupOn(VendingSetupWindowState& state, std::uint32_t preparation_seq);

// Pose `amount` de la pile de cette case, ou l'ajoute à sa ligne, borné à
// `stack`. Rend faux si rien n'a été posé : toutes les lignes sont prises, ou
// la pile l'est déjà entière.
bool PlaceOnVendingSetup(VendingSetupWindowState& state, std::uint16_t cart_slot, int amount,
                         int stack, std::size_t permitted_lines);

// Retire la ligne de cette case ; son prix part avec elle.
void TakeFromVendingSetup(VendingSetupWindowState& state, std::uint16_t cart_slot);

// Combien de la pile sont déjà posés.
[[nodiscard]] int PlacedOnVendingSetup(const VendingSetupWindowState& state,
                                       std::uint16_t cart_slot);

// Le prix d'une ligne, lu dans son texte ; rien s'il ne se lit pas.
[[nodiscard]] std::optional<std::int64_t> VendingLinePrice(const VendingSetupLineState& line);

// Les rangs des lignes dont le prix se confirme.
[[nodiscard]] std::vector<std::size_t> VendingHighPricedLines(const VendingSetupWindowState& state);

// Dessine la fenêtre si elle est ouverte. `link_facts` : ce que le
// menu d'un item sait d'avance — vote shop, album.
[[nodiscard]] VendingSetupAction DrawVendingSetupWindow(VendingSetupWindowState& state,
                                                        const VendingSetupView& view,
                                                        const SkinArt& art,
                                                        ItemLinkFacts link_facts = {});

}  // namespace ro::ui
