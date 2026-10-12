#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::sim {

// ── LES MOTS INTERDITS, MASQUÉS PAR DES « * » ───────────────────────────────
//
// L'installation porte une liste de mots que le client ne montre pas tels
// quels : chacun est remplacé, là où il paraît, par autant d'étoiles qu'il a de
// caractères. Elle vient de `kBannedWordsPath`, un texte simple.
//
// ⚠ LE FICHIER PEUT ÊTRE VIDE, et la liste ne masque alors rien : une archive
// peut en livrer un vide pour éteindre le masquage.

// Le fichier de la liste, dans l'installation.
inline constexpr std::string_view kBannedWordsPath = "data\\manner.txt";

class BannedWords {
 public:
  BannedWords() = default;
  // Les mots en UTF-8, dans l'ordre où ils seront essayés. Ils sont abaissés
  // en minuscules ASCII ; un mot vide ne s'ajoute pas.
  explicit BannedWords(std::vector<std::string> words);

  // LA LISTE LUE DANS LE FICHIER, octets tels quels (1252 ou UTF-8).
  //
  // 🔴 UN MOT S'ARRÊTE À UN RETOUR CHARIOT OU À UN « # », JAMAIS À UN SAUT DE
  // LIGNE SEUL : un « \n » efface ce qui le précède sur la ligne. Une ligne
  // sans « \r » final — la dernière d'un fichier, ou toutes celles d'un fichier
  // aux fins de ligne Unix — ne donne donc aucun mot.
  //
  // ⚠ Un mot qui commence par « // » est un commentaire, jusqu'au retour
  // chariot, « # » compris. Un mot fait seulement d'espaces et de tabulations
  // ne compte pas ; un autre garde ses espaces, de tête comme de queue, et ne
  // se reconnaît qu'avec elles.
  [[nodiscard]] static BannedWords FromMannerText(std::string_view bytes);

  [[nodiscard]] bool empty() const noexcept { return words_.empty(); }
  [[nodiscard]] std::span<const std::string> words() const noexcept { return words_; }

  // LE TEXTE, CHAQUE MOT INTERDIT MASQUÉ, en UTF-8.
  //
  // Le texte se parcourt caractère par caractère. À chaque position, le
  // PREMIER mot de la liste qui y commence — sans égard à la casse ASCII,
  // dans un mot comme au milieu d'un autre — est remplacé par une étoile par
  // caractère, et la lecture reprend après lui : deux mots ne se chevauchent
  // jamais, et l'ordre de la liste départage, pas la longueur.
  //
  // ⚠ UN LIEN D'OBJET NE SE MASQUE PAS : un « <ITEML> » qui ouvre la position,
  // suivi plus loin de son « </ITEML> », se saute jusqu'après la balise
  // fermante.
  [[nodiscard]] std::string Mask(std::string_view utf8_text) const;

  // LE TEXTE PORTE-T-IL UN MOT INTERDIT ? Même lecture que `Mask` — mêmes
  // positions, même lien d'objet sauté. C'est l'épreuve d'une SAISIE : un nom
  // ou un titre qui en porte un ne se masque pas, il se refuse.
  [[nodiscard]] bool Contains(std::string_view utf8_text) const;

  // LE TEXTE MASQUÉ, ou tel quel sans liste : la forme du masque à un étage
  // qui peut ne pas en tenir.
  [[nodiscard]] static std::string MaskWith(const BannedWords* banned, std::string_view utf8_text) {
    return banned == nullptr ? std::string(utf8_text) : banned->Mask(utf8_text);
  }

 private:
  // Après le lien d'objet qui ouvre `at` dans le texte abaissé, ou `at`.
  [[nodiscard]] static std::size_t SkipItemLink(std::string_view lower, std::size_t at);
  // Le premier mot de la liste qui commence à `at`, ou nul.
  [[nodiscard]] const std::string* WordAt(std::string_view lower, std::size_t at) const;

  std::vector<std::string> words_;
};

}  // namespace ro::sim
