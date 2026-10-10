#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── L'ÉCHANGE ENTRE JOUEURS ─────────────────────────────────────────────────
//
// Deux colonnes, mon offre à
// gauche et celle du partenaire à droite, le zeny en dessous, puis « Verrouiller
// », « Échanger » et « Annuler ». Un item du sac lâché sur ma colonne s'offre ;
// une pile demande sa quantité.
//
// 🔴 LA FENÊTRE NE POSSÈDE AUCUNE OFFRE : elle vit dans l'échange, que `app`
// tient. La fenêtre DEMANDE un dépôt, un montant, un verrou, la validation ;
// `app` rejuge, émet, et la vue suivante montre ce que le serveur a accusé.
//
// 🔴 CE QUE LE PARTENAIRE PEUT ENCORE CHANGER SE VOIT. Tant qu'il n'a pas
// verrouillé, il peut ajouter un item ou RÉDUIRE son zeny, même après mon
// verrou : une ligne arrivée depuis porte la marque « nouveau », un zeny changé
// se signale, et la validation se confirme TOUJOURS par un récapitulatif de ce
// qui part et de ce qui arrive, zeny écrit en toutes lettres. Le récapitulatif
// porte le rang de l'offre qu'il montre : une offre changée depuis ne part pas.
//
// La fenêtre dit aussi les raisons d'un refus
// avant le clic (item porté, loué, pile déjà offerte, dix items), un dépôt en
// route grisé, le zeny proposé au partenaire avant le verrou, et « Ajouter
// depuis le sac », qui se passe de la fenêtre du sac.

inline constexpr const char* kDealWindowId = "deal";
inline constexpr const char* kDealProposalWindowId = "deal_proposal";

// UNE LIGNE D'OFFRE, de l'un ou de l'autre côté.
struct DealLineView {
  // Le nom COMPOSÉ — raffinement, cartes, emplacements.
  TradeItemView item;
  int refine_level = 0;
  // Ce qui est offert, pas la pile.
  int amount = 0;
  // Mon côté : le dépôt est parti, le serveur ne l'a pas encore accusé.
  bool in_flight = false;
  // Côté du partenaire : arrivée ou grossie après mon verrou.
  bool changed_after_my_lock = false;
  // Ce qui mérite d'être lu avant de valider — « non identifié », « cassé » —,
  // déjà traduit, ou vide.
  std::string_view note;
};

// UNE CASE DU SAC, telle que l'échange la juge.
struct DealBagView {
  std::uint16_t client_slot = 0;
  TradeItemView item;
  int refine_level = 0;
  // Ce que la case montre.
  int amount = 0;
  // Combien peut encore partir ; zéro, `refusal` dit pourquoi.
  int most = 0;
  std::string_view refusal;
};

struct DealView {
  // « Échange avec Untel (Niv. 99) », déjà composé.
  std::string_view title;
  std::string_view partner_name;
  // Le rang de l'ouverture : une neuve remet la fenêtre à zéro.
  std::uint32_t opening_seq = 0;

  std::span<const DealLineView> mine;
  std::span<const DealLineView> partner;
  std::span<const DealBagView> bag;

  // Le zeny que j'ai offert — envoyé — et celui du partenaire.
  std::int64_t my_zeny = 0;
  std::int64_t partner_zeny = 0;
  // Ce que je possède ; `kTradeNoPurse` tant que le serveur ne l'a pas dit.
  std::int64_t purse = kTradeNoPurse;
  bool partner_zeny_changed_after_my_lock = false;

  bool lock_sent = false;
  bool my_locked = false;
  bool partner_locked = false;
  bool execute_sent = false;
  // L'offre du partenaire a changé depuis mon verrou.
  bool partner_changed_after_my_lock = false;
  // Le rang de l'offre du partenaire : la validation confirmée le renvoie.
  std::uint32_t partner_revision = 0;

  // CE QUI RETIENT CHAQUE BOUTON, déjà traduit. Vide : il agit.
  std::span<const std::string_view> lock_blockers;
  std::span<const std::string_view> execute_blockers;
  // Le dernier refus d'un dépôt, avec les mots du jeu, ou vide.
  std::string_view last_refusal;
};

// UN DÉPÔT DEMANDÉ : la case du sac, et combien.
struct DealOffer {
  std::uint16_t client_slot = 0;
  int amount = 0;
};

// Une ligne désignée : de quel côté, et son rang dans la vue.
struct DealLineRef {
  bool partner = false;
  int index = -1;
  [[nodiscard]] bool valid() const noexcept { return index >= 0; }
};

struct DealWindowState {
  // Recopié par `app` depuis l'échange ; la croix, Échap et « Annuler » le
  // baissent — c'est une ANNULATION, que `app` dit au serveur.
  bool open = false;
  std::uint32_t opening_seen = 0;
  // Le montant tapé, tel quel.
  std::string zeny_text;
  // LA QUANTITÉ DEMANDÉE pour une pile lâchée ou choisie.
  bool asking_amount = false;
  std::uint16_t asking_slot = 0;
  int asking_value = 1;
  // LA PILE A ÉTÉ LÂCHÉE depuis le sac : la quantité se demande par la modale
  // du sac et du storage, sous le curseur. Choisie dans « Ajouter depuis
  // l'inventory », elle se demande sur place, sous la liste.
  bool asking_by_drop = false;
  // « Ajouter depuis le sac » est déplié.
  bool picking = false;
  ImGuiTextFilter filter;
  // Un refus dit par la fenêtre elle-même — un lâcher venu d'ailleurs que le
  // sac, une case que l'échange refuse —, déjà traduit.
  std::string flash;
  // LA VALIDATION QUI ATTEND SA CONFIRMATION, et le rang qu'elle a montré.
  bool confirming = false;
  std::uint32_t confirming_revision = 0;
};

struct DealWindowAction {
  // La croix, Échap ou « Annuler ». ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool cancel = false;
  std::optional<DealOffer> offer;
  // Un montant à proposer, absolu. Peut accompagner `lock` : il part d'abord.
  std::optional<std::int64_t> zeny;
  bool lock = false;
  // LA VALIDATION CONFIRMÉE, pour l'offre du partenaire à ce rang.
  //
  // ⚠ `app` REJUGE AVANT D'ÉMETTRE : le verdict dessiné a une image de retard.
  bool execute = false;
  std::uint32_t execute_revision = 0;
  // La ligne survolée, pour l'aperçu, et celle qu'on veut décrire.
  DealLineRef hovered;
  DealLineRef described;
};

// Le texte du récapitulatif : ce que je donne, ce que je reçois, zeny en
// chiffres ET en toutes lettres, et la mise en garde quand l'offre du
// partenaire a changé après mon verrou. Déjà traduit.
[[nodiscard]] std::string DealConfirmationText(const DealView& view);

// La phrase d'état : ce qu'on attend, et de qui. Toujours une, déjà traduite.
[[nodiscard]] std::string DealStatusText(const DealView& view);

// LE MONTANT TAPÉ, jugé avant le clic.
struct DealZenyEntry {
  // Ce qui se proposerait ; vide quand rien n'est tapé ou que la saisie se refuse.
  std::optional<std::int64_t> amount;
  // Pourquoi la saisie se refuse, déjà traduit ; vide sinon.
  std::string_view refusal;
  [[nodiscard]] bool blank() const noexcept { return !amount.has_value() && refusal.empty(); }
};

// ⚠ UN MONTANT ILLISIBLE N'EST PAS ZÉRO : il retient « Proposer » ET le verrou,
// sans quoi le verrou partirait sur le zeny d'avant. `purse` borne la saisie
// quand le serveur l'a dite ; `kTradeNoPurse` ne borne rien.
[[nodiscard]] DealZenyEntry ReadDealZeny(std::string_view typed, std::int64_t purse);

// UNE CASE DU SAC LÂCHÉE OU CHOISIE : refusée, son refus s'écrit ; seule de son
// espèce, elle s'offre tout de suite ; une pile demande d'abord sa quantité.
void ChooseDealBagSlot(DealWindowState& state, const DealBagView& bag, DealWindowAction& action);

// Remet la fenêtre à l'état d'une ouverture neuve.
void OpenDealOn(DealWindowState& state, std::uint32_t opening_seq);

// Dessine la fenêtre si elle est ouverte.
[[nodiscard]] DealWindowAction DrawDealWindow(DealWindowState& state, const DealView& view,
                                              const SkinArt& art);

// ── LA PROPOSITION ──────────────────────────────────────────────────────────
//
// Reçue, elle demande « Accepter » ou « Refuser » ; Échap et la croix
// refusent. Envoyée, elle dit qu'on attend, et « Annuler » la retire — ce que
// le client d'origine ne permet pas.

struct DealProposalView {
  // Déjà composé : « Untel (Niv. 99) vous propose un échange. »
  std::string_view text;
  // Reçue (vrai) ou envoyée (faux).
  bool incoming = false;
  // Ma réponse est partie : les boutons se retiennent.
  bool answered = false;
  // Le rang de la proposition : une neuve rouvre la boîte.
  std::uint32_t seq = 0;
};

struct DealProposalState {
  bool open = false;
  std::uint32_t seen = 0;
};

struct DealProposalAction {
  bool accept = false;
  // Refuser une proposition reçue, ou retirer la mienne.
  bool refuse = false;
};

[[nodiscard]] DealProposalAction DrawDealProposal(DealProposalState& state,
                                                  const DealProposalView& view, const SkinArt& art);

}  // namespace ro::ui
