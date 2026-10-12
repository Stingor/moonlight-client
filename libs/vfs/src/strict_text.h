#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace ro::vfs {

// ── LA FORME STRICTE DES GROSSES TABLES YAML ────────────────────────────────
//
// `item_info.yml` et `skill_info.yml` pèsent plusieurs méga-octets : un
// analyseur YAML général les lit en secondes, ce curseur-ci en une fraction. Il
// ne connaît que la forme que leurs scripts écrivent, qui reste du YAML valide :
//
//   · des commentaires `#` et des blancs, fins de ligne comprises ;
//   · des noms de champ en minuscules et soulignés ;
//   · des entiers sans signe, les mots `true` et `false` ;
//   · des textes entre guillemets doubles aux échappements de JSON (`\"`,
//     `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, `\uXXXX`), sur une seule ligne ;
//   · des suites `[…]` de textes ou d'entiers.
//
// Ce que la table assemble — accolades, deux-points, virgules —, c'est au
// lecteur de chaque table de le demander par `Take`.
//
// ⚠ PRIVÉ À `vfs` : cet en-tête vit dans `src/`. Ce n'est pas une bibliothèque
// de texte, c'est le peu que ces lecteurs partagent.
class StrictText {
 public:
  // `file_label` nomme la table dans les refus : « skill_info.yml ».
  StrictText(std::string_view text, std::string_view file_label)
      : text_(text), file_label_(file_label) {}

  // Le refus, avec la ligne où la lecture s'est arrêtée.
  [[nodiscard]] core::Error Refused(std::string_view what) const;

  // Saute les blancs, les fins de ligne et les commentaires.
  void SkipBlank();
  [[nodiscard]] bool AtEnd();

  // Le caractère attendu, après les blancs.
  [[nodiscard]] bool Take(char expected);

  // Un nom de champ : des minuscules et des soulignés.
  [[nodiscard]] bool ReadKey(std::string_view& key);

  // Un entier sans signe, au plus `most`.
  [[nodiscard]] bool ReadWhole(std::uint32_t most, std::uint32_t& value);

  // `true` ou `false`.
  [[nodiscard]] bool ReadBoolean(bool& value);

  // Un texte entre guillemets doubles, ses échappements résolus.
  [[nodiscard]] bool ReadQuoted(std::string& text);

  // Une suite `[…]` de textes, ajoutés à `lines`.
  [[nodiscard]] bool ReadQuotedList(std::vector<std::string>& lines);

  // Une suite `[…]` d'entiers sans signe, chacun au plus `most`, ajoutés à
  // `values`.
  [[nodiscard]] bool ReadWholeList(std::uint32_t most, std::vector<std::uint32_t>& values);

 private:
  [[nodiscard]] bool ReadEscape(std::string& text);
  // Le `XXXX` d'un `\uXXXX`.
  [[nodiscard]] bool ReadHex(char32_t& unit);
  // Un point de code, et le second d'une paire de substitution.
  [[nodiscard]] bool ReadCodePoint(std::string& text);

  std::string_view text_;
  std::string_view file_label_;
  std::size_t at_ = 0;
};

}  // namespace ro::vfs
