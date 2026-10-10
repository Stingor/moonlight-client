#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/description_window.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA BOUTIQUE DE VOTE ─────────────────────────────────────────────────────
//
// Le catalogue que le serveur envoie, en cartes, un panier, et l'achat en un
// clic.
//
// 🔴 LA FENÊTRE NE S'OUVRE PAS TOUTE SEULE. Elle suit la session du serveur :
// `app` recopie l'ouverture dans `open`, et une fenêtre fermée — la croix,
// Échap — devient une fermeture dite au serveur. Sans elle, le personnage
// resterait bloqué.
//
// Cette couche dessine, filtre et trie ; `app` nomme les items, charge leurs
// images, compose le pantin de l'aperçu, et parle au serveur.

inline constexpr const char* kCashShopWindowId = "cash_shop";

// Les onglets du serveur, dans son ordre. Voir `sim::kCashShopTabCount`.
inline constexpr std::size_t kCashShopTabCount = 9;

// Ce que le serveur accepte au plus d'un même item dans un achat. Voir
// `sim::kCashShopMostPerItem`.
inline constexpr int kCashShopMostPerLine = 99;

// L'ONGLET SE MONTRE-T-IL ? Ce serveur laisse toujours vides les limités, la
// location, les permanents, les parchemins et les soldes : ils sont masqués.
//
// ⚠ L'INDEX RESTE CELUI DU SERVEUR, masqué ou non : c'est lui qui voyage dans
// l'achat.
[[nodiscard]] bool CashShopTabShown(std::size_t tab) noexcept;

// L'EMPLACEMENT D'UN ITEM, déduit de son masque d'équipement, pour le filtre.
//
// Une clé d'ordre stable, et un libellé déjà traduit. Les costumes passent avant
// les pièces ordinaires : ce catalogue en est fait.
struct CashShopSlot {
  int key = 0;
  const char* label = nullptr;
};
[[nodiscard]] CashShopSlot CashShopSlotOf(std::uint32_t equip_location);
// La clé de ce qui ne s'équipe pas.
inline constexpr int kCashShopSlotOther = 99;
// Une entrée qu'aucun masque ne produit : les costumes qui se montrent par un
// effet de chapeau. ⚠ AU-DELÀ DE TOUTES LES AUTRES, pour se ranger en dernier.
inline constexpr int kCashShopSlotHatEffect = 100;
// Aucun filtre d'emplacement.
inline constexpr int kCashShopSlotAll = -1;

// UNE CARTE DU CATALOGUE.
struct CashShopCard {
  std::uint32_t item_id = 0;
  std::int32_t price = 0;
  std::uint16_t view_sprite = 0;
  std::uint32_t equip_location = 0;
  // Le nom montré, jamais vide. ⚠ SANS SON PRÉFIXE « Costume » : l'onglet et
  // la vignette disent déjà qu'on est dans les costumes.
  std::string_view name;
  // L'image de collection, ou l'icône quand l'art manque. Utilisable seulement
  // pour les cartes rapportées par `CashShopWindowAction::drawn` : voir là.
  SkinImage image;
  // L'item se montre par un effet de chapeau.
  bool hat_effect = false;
  // Le pantin sait le porter : son survol montre l'aperçu.
  bool wearable_preview = false;
};

// Ce que la dernière demande d'achat a rendu, depuis l'ouverture.
enum class CashShopPurchaseNote : std::uint8_t { kNone, kBought, kRefused };

struct CashShopView {
  // Les mots de la table du serveur : le nom de la boutique, ses deux monnaies,
  // et la case qui fait dépenser la seconde d'abord.
  std::string_view title;
  std::string_view currency;
  std::string_view event_points_label;
  std::string_view use_event_points_label;
  std::uint32_t vote_points = 0;
  std::uint32_t event_points = 0;
  std::array<std::span<const CashShopCard>, kCashShopTabCount> tabs{};
  CashShopPurchaseNote purchase = CashShopPurchaseNote::kNone;
  // Le message EXACT du client pour un refus.
  std::string_view refusal;
};

// UNE LIGNE DU PANIER.
struct CashShopCartLine {
  std::uint32_t item_id = 0;
  std::size_t tab = 0;
  int amount = 1;
  std::int32_t price = 0;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct CashShopWindowState {
  // Recopié par `app` depuis la session du serveur ; la croix et Échap le
  // baissent.
  bool open = false;
  // Le rang de l'ouverture que la fenêtre montre : voir `sim::CashShop`.
  std::uint32_t opening_seen = 0;
  // Le rang de l'achat déjà connu à l'ouverture : les résultats d'avant ne se
  // montrent pas.
  std::uint32_t purchase_seen = 0;
  // Poser la place à la première image d'une ouverture.
  bool placed = false;

  std::size_t tab = 0;
  // L'onglet à SÉLECTIONNER de force à la prochaine image. ⚠ ÉCRIRE `tab` NE
  // SUFFIT PAS : la barre d'onglets tient sa propre sélection et la
  // réimposerait aussitôt.
  std::size_t forced_tab = kCashShopTabCount;
  int slot = kCashShopSlotAll;
  // Tri : 0 = nom, 1 = identifiant, 2 = coût.
  int sort = 0;
  bool ascending = true;
  // Dépenser d'abord les points d'événement.
  bool use_event_points = false;
  ImGuiTextFilter filter;
  std::vector<CashShopCartLine> cart;

  // UN APERÇU ÉTAIT SURVOLÉ À L'IMAGE PRÉCÉDENTE : la grille ne défile plus à
  // la molette, qui tourne le pantin.
  bool preview_active = false;
  // Le pantin de l'aperçu, et la pièce qu'il doit porter — posés par la
  // fenêtre, composés par `app`.
  DescriptionDollState doll;
  // L'item lui-même : c'est lui qui dit son effet de chapeau.
  std::uint32_t doll_item = 0;
  std::uint16_t doll_view = 0;
  std::uint32_t doll_location = 0;

  // ── LE REDIMENSIONNEMENT PAR PALIERS ──────────────────────────────────────
  // La fenêtre ne prend que des tailles qui tiennent un nombre ENTIER de
  // colonnes et de lignes de cartes. Mesuré à l'image précédente.
  float card_width = 0.0F;
  float card_height = 0.0F;
  float card_gap = 0.0F;
  float chrome_width = 0.0F;
  float chrome_height = 0.0F;
  bool snap_known = false;

  // Le menu qu'un clic droit sur une carte a armé.
  ItemLinkMenu item_menu;
};

// Ajoute une unité au panier, ou une de plus à sa ligne. ⚠ PLAFONNÉE À
// `kCashShopMostPerLine` : au-delà, le serveur refuserait l'achat entier.
void AddToCashShopCart(CashShopWindowState& state, std::uint32_t item_id, std::size_t tab,
                       std::int32_t price);

// Amène la vue sur un item : son onglet, le filtre posé sur son nom, et une
// unité au panier. `app` s'assure que l'item est au catalogue.
void RevealCashShopItem(CashShopWindowState& state, std::uint32_t item_id, std::size_t tab,
                        std::int32_t price, std::string_view name);

// Remet la fenêtre à l'état d'une ouverture neuve, sur cet onglet — s'il se
// montre : atterrir sur une page masquée sans un mot serait pire que d'ignorer
// la préférence du serveur.
void OpenCashShopOn(CashShopWindowState& state, std::uint32_t opening_seq,
                    std::uint32_t purchase_seq, std::uint32_t server_tab);

// CE QUE L'ACHAT DÉPENSE EN POINTS D'ÉVÉNEMENT : rien sans la case ; avec elle,
// au plus le total — le serveur prélève le reste en points de vote SANS borner
// ce montant au prix, et envoyer tout le solde le viderait.
[[nodiscard]] std::uint32_t CashShopEventPointsToSpend(long long total, std::uint32_t balance,
                                                       bool use_event_points) noexcept;

// Ce que l'achat peut dépenser : les points de vote, plus ceux d'événement
// quand la case est cochée.
[[nodiscard]] long long CashShopSpendable(std::uint32_t vote_points, std::uint32_t event_points,
                                          bool use_event_points) noexcept;

struct CashShopWindowAction {
  // La croix, ou Échap. ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  // Les lignes à acheter, vides pour rien. ⚠ L'ACHAT EN UN CLIC FERME LA
  // BOUTIQUE APRÈS : `close_after_purchase`, et le paquet d'achat part avant la
  // fermeture.
  std::vector<CashShopCartLine> purchase;
  std::uint32_t event_points_spent = 0;
  bool close_after_purchase = false;
  // LES GESTES D'UNE CARTE, ceux d'un item de commerce : le survol
  // (`hovered_id`) pour l'aperçu de sa description, Maj + clic (`kRepost`)
  // pour son lien dans la saisie, et ce qu'on a choisi au menu des objets
  // qu'ouvre le clic droit — la description (`kDescribe`), la base de données,
  // les commandes du serveur.
  ChatLinkAsk item;
  // 🔴 LES ITEMS DONT UNE CARTE A ÉTÉ DESSINÉE, pour leurs images : un onglet
  // en compte des milliers, et `app` ne charge que celles qu'on a vues. ⚠ UNE
  // IMAGE DE RETARD.
  std::vector<std::uint32_t> drawn;
};

// Dessine la fenêtre si elle est ouverte.
[[nodiscard]] CashShopWindowAction DrawCashShopWindow(CashShopWindowState& state,
                                                      const CashShopView& view, const SkinArt& art);

}  // namespace ro::ui
