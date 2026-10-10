#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── LA PRÉPARATION D'UNE ÉCHOPPE D'ACHAT ────────────────────────────────────
//
// Un nom, les items qu'on peut poser, les lignes posées — quantité voulue et
// prix —, la limite de zeny à dépenser, le coût total.
//
// 🔴 LE SERVEUR OUVRE LA FENÊTRE, ET LE SERVEUR OUVRE L'ÉCHOPPE. La fenêtre
// naît de sa « préparation » ; « Ouvrir » ne fait que demander, et la fenêtre
// attend sa réponse. Annuler, la croix et Échap renoncent : la préparation est
// perdue, il faut relancer le skill.
//
// Ni grille d'icônes ni glisser (la liste suffit à cinq
// lignes), ni import d'une échoppe passée (un service web que ce client n'a
// pas) ; la longueur du nom se compte en OCTETS DU FIL, comme le serveur la
// coupe ; chaque défaut se dit avant le clic, ligne désignée ; un prix élevé
// se confirme.

inline constexpr const char* kBuyingStoreSetupWindowName = "###buying_store_setup";

// Le nom se tape ici ; le fil n'en garde qu'une partie, que `app` mesure.
inline constexpr std::size_t kBuyingStoreTitleBufferBytes = 256;

// UN PRIX QUI SE CONFIRME : le seuil de la case « prix élevés » du client
// d'origine.
inline constexpr std::int64_t kBuyingStoreDearPrice = 10'000'000;

// Un avertissement qui ne retient rien : le joueur doit le lire, pas le subir.
inline constexpr ImVec4 kTradeWarningText = ImVec4(0.80F, 0.45F, 0.05F, 1.0F);

// UN ITEM DU SAC QU'ON PEUT POSER.
struct BuyingStoreCandidateView {
  TradeItemView item;
  // La pile que le serveur complétera.
  int owned = 0;
};

// UNE LIGNE POSÉE. ⚠ L'ITEM FAIT L'IDENTITÉ : le serveur refuse le même item
// sur deux lignes.
struct BuyingStoreSetupLine {
  std::uint32_t item_id = 0;
  int amount = 1;
  int price = 0;
};

struct BuyingStoreSetupView {
  // Le titre de la fenêtre, avec les mots du jeu quand l'installation les a.
  std::string_view window_title;
  // Combien de lignes le skill permet.
  std::size_t slots = 0;
  // Le plus qu'une ligne puisse vouloir.
  int most_amount = 0;
  std::span<const BuyingStoreCandidateView> candidates;
  // LE NOM TEL QU'IL PARTIRA : ce que les autres liront, et les octets qui
  // restent — négatifs quand le fil le coupe.
  std::string_view title_on_wire;
  int title_bytes_left = 0;
  // Ce que le joueur possède ; `kTradeNoPurse` s'il n'est pas dit.
  std::int64_t zeny = kTradeNoPurse;
  std::int64_t total_cost = 0;
  // LE VERDICT, DÉJÀ TRADUIT : ce qui retient le bouton, ce qui avertit
  // seulement, et, par ligne posée, ce qui cloche (vide si rien).
  std::span<const std::string_view> blockers;
  std::span<const std::string_view> warnings;
  std::span<const std::string_view> line_notes;
  // La demande est partie : rien ne repart avant la réponse.
  bool awaiting = false;
};

struct BuyingStoreSetupState {
  // Recopié par `app` depuis la préparation ; la croix, Échap et « Annuler »
  // le baissent.
  bool open = false;
  // Le rang de la préparation montrée : une préparation neuve remet les lignes
  // à zéro. ⚠ LE NOM, LUI, RESTE d'une échoppe à l'autre.
  std::uint32_t preparation_seen = 0;
  std::array<char, kBuyingStoreTitleBufferBytes> title{};
  std::vector<BuyingStoreSetupLine> lines;
  std::int64_t zeny_limit = 0;
  ImGuiTextFilter filter;
  // La hauteur du pied, mesurée à l'image précédente : la liste des lignes
  // prend ce qui reste.
  float footer_pixels = 0.0F;
  // Le menu qu'un clic droit sur un item a armé.
  ItemLinkMenu item_menu;
};

struct BuyingStoreSetupAction {
  // Annuler, la croix ou Échap. ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool cancel = false;
  // « Ouvrir », confirmé s'il le fallait. ⚠ `app` REJUGE AVANT D'ÉMETTRE.
  bool open_store = false;
  // L'item survolé, pour l'aperçu, et celui qu'un geste vise.
  std::uint32_t hovered_id = 0;
  std::uint32_t describe_id = 0;
  // Ce que le geste demande de l'item visé : sa description, son lien dans la
  // saisie (Maj + clic), ou une autre entrée du menu des objets.
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// Remet la fenêtre à l'état d'une préparation neuve.
void OpenBuyingStoreSetupOn(BuyingStoreSetupState& state, std::uint32_t preparation_seq);

// Pose un item. Faux si les lignes sont pleines ou l'item déjà posé.
bool AddBuyingStoreSetupLine(BuyingStoreSetupState& state, std::uint32_t item_id,
                             std::size_t slots);

// Vrai si une ligne demande un prix qui se confirme.
[[nodiscard]] bool BuyingStoreSetupHasDearPrice(const BuyingStoreSetupState& state);

// `vote_shop` : le catalogue du vote shop, pour le menu d'un item.
[[nodiscard]] BuyingStoreSetupAction DrawBuyingStoreSetupWindow(
    BuyingStoreSetupState& state, const BuyingStoreSetupView& view, const SkinArt& art,
    std::span<const VoteShopOffer> vote_shop = {});

}  // namespace ro::ui
