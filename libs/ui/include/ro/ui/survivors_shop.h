#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <imgui.h>

namespace ro::ui {

// LES DEUX BOUTIQUES DE LA MÉTA SURVIVORS — l'Église de Prontera (les
// Bénédictions) et la Kafra (les charges) — partagent leur forme (Q2, Q3) :
// une ligne par rang à acheter, des pastilles de rang, l'effet actuel et le
// suivant, le prix, un bouton grisé avec sa raison, et en pied un seul « Tout
// rembourser », confirmé sur place.
//
// 🔴 ELLES NE DÉCIDENT RIEN : un bouton grisé ne rend aucune intention, mais
// l'achat se juge dans `app`, sur le carnet relu. La vue dit ce qui est ; la
// fenêtre rend ce que le joueur demande.
//
// Touches, fenêtre au premier plan : ↑ et ↓ choisissent la ligne, Entrée
// achète. Échap ferme par la pile d'Échap d'`app`, qui tient `open`.

// Une ligne, déjà résolue par `app`.
struct SurvivorsShopRow {
  // La clé stable du catalogue (`blessing`, `magnifier`…) : elle choisit le
  // libellé de l'effet et, pour la Kafra, la description.
  const char* key = "";
  ImTextureID icon = 0;          // l'icône du skill ou de l'objet RO
  const char* name = "";         // tiré des tables du jeu
  const char* description = "";  // ⚠ EN FRANÇAIS, marqué par `TrText` ; vide : celle de la clé
  int rank = 0;
  int most_rank = 0;
  int effect_now = 0;   // le total au rang actuel
  int effect_next = 0;  // le total au rang suivant ; lu sous le rang maximal
  std::uint64_t next_price = 0;
};

struct SurvivorsShopView {
  std::span<const SurvivorsShopRow> rows;
  std::uint64_t zeny = 0;             // le solde
  std::uint64_t refundable_zeny = 0;  // ce que « Tout rembourser » rendrait
};

struct SurvivorsShopState {
  bool open = false;
  std::size_t selected_row = 0;
  bool confirming_refund = false;

  // Ouvrir le lieu : une confirmation de « Tout rembourser » laissée en
  // suspens à la fermeture ne revient jamais avec lui.
  void Open() noexcept {
    open = true;
    confirming_refund = false;
  }
};

struct SurvivorsShopAsk {
  std::optional<std::size_t> buy_row;
  bool refund_all = false;
  [[nodiscard]] bool operator==(const SurvivorsShopAsk&) const = default;
};

// POURQUOI UNE LIGNE NE S'ACHÈTE PAS ; rien quand elle s'achète.
enum class SurvivorsShopRefusal : std::uint8_t { kMostRank, kNotEnoughZeny };
[[nodiscard]] std::optional<SurvivorsShopRefusal> RefusalOf(const SurvivorsShopRow& row,
                                                            std::uint64_t zeny) noexcept;

}  // namespace ro::ui
