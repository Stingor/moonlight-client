#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace ro::test_support {

// Fabrique de documents BSON.
//
// Même raison d'être que le constructeur d'archives : aucun asset du jeu n'est
// versionné, donc les cas que le lecteur doit tenir — une chaîne sans zéro
// final, une taille qui déborde, un type hors du sous-ensemble — se fabriquent
// ici plutôt que de se chercher dans de vraies données.
//
// Les octets produits sont ceux d'un document complet : sa taille en tête, ses
// éléments, son zéro final. Un document imbriqué s'écrit donc avec le même
// bâtisseur, et c'est ce que Document() attend.
class BsonBuilder {
 public:
  BsonBuilder& Number(std::string_view name, double value);
  BsonBuilder& Text(std::string_view name, std::string_view value);
  BsonBuilder& Document(std::string_view name, const BsonBuilder& nested);
  BsonBuilder& Array(std::string_view name, const BsonBuilder& nested);
  BsonBuilder& Flag(std::string_view name, bool value);
  BsonBuilder& Integer(std::string_view name, std::int32_t value);

  // Un élément écrit tel quel : c'est par lui qu'un test fabrique ce qu'aucune
  // méthode nommée ne doit produire — un type inconnu, une charge tronquée.
  BsonBuilder& RawElement(std::uint8_t type, std::string_view name,
                          std::span<const std::uint8_t> payload);

  // Le document complet.
  [[nodiscard]] std::vector<std::uint8_t> Build() const;

  // Le document complet avec une taille MENTIE de `delta` octets : de quoi
  // éprouver ce que fait le lecteur d'une taille qui ne tombe pas juste.
  [[nodiscard]] std::vector<std::uint8_t> BuildWithSizeOffBy(std::int32_t delta) const;

 private:
  BsonBuilder& Begin(std::uint8_t type, std::string_view name);

  std::vector<std::uint8_t> elements_;
};

}  // namespace ro::test_support
