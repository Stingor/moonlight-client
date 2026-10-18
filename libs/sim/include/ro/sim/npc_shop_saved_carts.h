#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ro::sim {

// LES PANIERS D'ACHAT ENREGISTRÉS : des listes nommées d'objets et de
// quantités, propres au compte, qu'on recharge dans le panier de n'importe
// quelle boutique de PNJ.
//
// ⚠ LA CLÉ EST L'OBJET, JAMAIS LA BOUTIQUE : un panier « Potions » vaut chez
// tous les marchands. Le nom de l'objet voyage avec lui pour dire, hors de la
// boutique qui le vendait, ce qui manque.

struct SavedCartLine {
  std::uint32_t item_id = 0;
  std::int64_t amount = 0;
  // Le nom montré quand l'objet a été enregistré.
  std::string item_name;

  [[nodiscard]] bool operator==(const SavedCartLine&) const = default;
};

struct SavedCart {
  std::string name;
  std::vector<SavedCartLine> lines;

  [[nodiscard]] bool operator==(const SavedCart&) const = default;
};

// Le nom saisi, terminateur compris, et le nombre de paniers d'un compte.
inline constexpr std::size_t kSavedCartNameBytes = 32;
inline constexpr std::size_t kSavedCartLimit = 20;

enum class SavedCartRefusal : std::uint8_t {
  kNone,
  // Un nom vide, ou fait de blancs.
  kEmptyName,
  // Rien d'achetable dans le panier : il ne s'enregistre pas.
  kEmptyCart,
  // Un autre panier porte déjà ce nom.
  kNameTaken,
  // Le carnet est plein.
  kFull,
  // Le rang ne désigne aucun panier.
  kUnknown,
};

class SavedCartBook {
 public:
  SavedCartBook() = default;
  explicit SavedCartBook(std::vector<SavedCart> carts) : carts_(std::move(carts)) {}

  // ENREGISTRE CES LIGNES SOUS CE NOM. Un nom déjà porté REMPLACE son contenu,
  // à sa place dans la liste : c'est ainsi qu'on met un panier à jour.
  //
  // Une ligne sans objet ou sans quantité se saute, et deux lignes du même
  // objet n'en font qu'une. Le nom perd ses blancs de tête et de queue, et se
  // coupe à la taille de la saisie sans couper un caractère UTF-8 en deux.
  SavedCartRefusal Save(std::string_view name, std::span<const SavedCartLine> lines);
  SavedCartRefusal Rename(std::size_t index, std::string_view name);
  SavedCartRefusal Remove(std::size_t index);

  [[nodiscard]] std::span<const SavedCart> all() const noexcept { return carts_; }
  [[nodiscard]] const SavedCart* at(std::size_t index) const noexcept {
    return index < carts_.size() ? &carts_[index] : nullptr;
  }

 private:
  std::vector<SavedCart> carts_;
};

// Le nom tel que le carnet le range : sans blancs autour, borné à la saisie.
[[nodiscard]] std::string TidySavedCartName(std::string_view typed);

// Les lignes telles que le carnet les range : sans ligne vide, une par objet,
// chaque quantité bornée à une pile.
[[nodiscard]] std::vector<SavedCartLine> TidySavedCartLines(std::span<const SavedCartLine> lines);

}  // namespace ro::sim
