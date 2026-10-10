#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace moonclient::web {

// UN LECTEUR JSON, POUR CE QUE LE SITE RÉPOND — et rien de plus.
//
// 🔴 ÉCRIT PLUTÔT QU'EMPRUNTÉ, et c'est une décision, pas un réflexe. Ce dépôt
// compte six dépendances, et la bibliothèque qu'on prendrait ici est un en-tête
// de vingt-cinq mille lignes pour lire trois réponses dont on connaît la forme
// par cœur. Ce qu'on y gagnerait — les cas que nous n'employons pas — est
// exactement ce qu'on ne veut pas exposer à des octets venus du réseau.
//
// ⚠ IL RESTE UN LECTEUR JSON COMPLET pour ce qu'il accepte : objets, tableaux,
// chaînes, nombres, booléens, nul. Le rendre partiel — « on ne lit que les
// clés qu'on connaît » — le casserait le jour où le site ajoute un champ, ce
// qu'un site fait.
//
// 🔴 ET IL DÉCODE `\uXXXX`, CE QUI N'EST PAS UN LUXE : l'API appelle
// `json_encode` SANS `JSON_UNESCAPED_UNICODE`, si bien que tout caractère
// accentué arrive échappé. « Réessaie dans 3 minutes » se lit
// `Réessaie...` sur le fil, et un lecteur qui l'ignorerait afficherait le
// message tel quel au joueur.

// LA PROFONDEUR MAXIMALE D'IMBRICATION.
//
// 🔴 UN LECTEUR RÉCURSIF SUR DES OCTETS DU RÉSEAU A BESOIN D'UNE BORNE : mille
// crochets ouvrants suffisent à faire déborder la pile, et rien dans le format
// ne l'interdit. Les réponses du site en emploient trois — objet, tableau,
// objet ; seize laisse toute la marge qu'on peut vouloir.
inline constexpr int kJsonMaxDepth = 16;

// UNE VALEUR JSON.
//
// ⚠ LES ACCESSEURS NE LANCENT RIEN ET NE SUPPOSENT RIEN : ils rendent un
// `optional`, ou la valeur par défaut demandée. Une réponse du réseau n'a pas
// la forme qu'on croit aussi souvent qu'on voudrait, et chaque `at()` qui
// suppose est un plantage à distance.
class JsonValue {
 public:
  enum class Kind : std::uint8_t { kNull, kBool, kNumber, kString, kArray, kObject };

  JsonValue() = default;
  explicit JsonValue(bool truth) : kind_(Kind::kBool), truth_(truth) {}
  explicit JsonValue(double number) : kind_(Kind::kNumber), number_(number) {}
  explicit JsonValue(std::string text) : kind_(Kind::kString), text_(std::move(text)) {}

  [[nodiscard]] static JsonValue Array(std::vector<JsonValue> items);
  [[nodiscard]] static JsonValue Object(std::map<std::string, JsonValue> fields);

  [[nodiscard]] Kind kind() const noexcept { return kind_; }
  [[nodiscard]] bool is_object() const noexcept { return kind_ == Kind::kObject; }
  [[nodiscard]] bool is_array() const noexcept { return kind_ == Kind::kArray; }

  // LE CHAMP `name` DE CET OBJET, ou nul — y compris quand la valeur n'est pas
  // un objet du tout.
  [[nodiscard]] const JsonValue* Field(std::string_view name) const;

  // Les éléments, vides si ce n'est pas un tableau.
  [[nodiscard]] const std::vector<JsonValue>& Items() const noexcept { return items_; }

  // ── CE QUE L'APPELANT LIT VRAIMENT ──────────────────────────────────────
  //
  // ⚠ CHACUN EXIGE LE BON GENRE : un nombre demandé sur une chaîne rend le
  // défaut, il ne la convertit pas. Le site répond en types, pas en texte, et
  // une conversion silencieuse masquerait un changement de sa part.

  [[nodiscard]] std::string_view Text() const noexcept;
  [[nodiscard]] bool Truth(bool fallback = false) const noexcept;
  // ⚠ REFUSE UN NOMBRE NON ENTIER : `account_id` est un entier, et 2000000.5
  // n'en est pas un. Tronquer en silence rendrait un identifiant voisin.
  [[nodiscard]] std::optional<std::int64_t> Whole() const noexcept;

  // Les mêmes, en une fois, sur un champ de cet objet.
  [[nodiscard]] std::string_view TextAt(std::string_view name) const;
  [[nodiscard]] bool TruthAt(std::string_view name, bool fallback = false) const;
  [[nodiscard]] std::int64_t WholeAt(std::string_view name, std::int64_t fallback = 0) const;

 private:
  Kind kind_ = Kind::kNull;
  bool truth_ = false;
  double number_ = 0.0;
  std::string text_;
  std::vector<JsonValue> items_;
  std::map<std::string, JsonValue> fields_;
};

// LIT UN DOCUMENT JSON ENTIER.
//
// ⚠ ENTIER : ce qui suit la valeur de tête, hors blancs, est une erreur. Un
// lecteur qui s'arrêterait à la première valeur accepterait une réponse
// tronquée au milieu, puis lirait la moitié d'une liste de comptes comme si
// c'était la liste.
[[nodiscard]] ro::core::Result<JsonValue> ParseJson(std::string_view text);

}  // namespace moonclient::web
