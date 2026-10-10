#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace ro::sim {

// OÙ UNE ILLUSTRATION DE PNJ SE MONTRE : le type que le script donne à `cutin`.
enum class IllustrationPlace : std::uint8_t {
  kBottomLeft = 0,
  kBottomMiddle = 1,
  kBottomRight = 2,
  // Au milieu de l'écran, dans une fenêtre titrée qu'on déplace et qu'on ferme.
  kCenteredWindow = 3,
  // Au milieu de l'écran, sans cadre, avec un bouton pour la fermer.
  kCenteredFloating = 4,
};

// L'ILLUSTRATION QU'UN PNJ MONTRE : un portrait, une carte, un décor, posé par
// le script (`cutin`) et retiré par lui.
struct NpcIllustration {
  // Le nom du fichier sous `유저인터페이스\illust\`, tel que le serveur l'écrit :
  // en CP949, avec ou sans extension.
  std::string image;
  IllustrationPlace place = IllustrationPlace::kBottomMiddle;

  [[nodiscard]] friend bool operator==(const NpcIllustration&, const NpcIllustration&) = default;
};

// CE QUE LE TYPE REÇU DEMANDE : une place, ou l'effacement (rien).
//
// 🔴 255 EFFACE, et un nom vide aussi — c'est la forme `cutin "",255` que les
// scripts emploient par milliers.
//
// ⚠ LES TYPES 5 À 254 N'EXISTENT PAS dans le protocole. Le client d'origine
// ouvre quand même sa fenêtre, collée en haut de l'écran, à un endroit que rien
// ne documente ; on les montre plutôt en bas au milieu, là où l'on regarde une
// illustration.
[[nodiscard]] std::optional<IllustrationPlace> IllustrationPlaceOf(std::uint8_t type) noexcept;

// L'ILLUSTRATION MONTRÉE, s'il y en a une. Une seule à la fois : la suivante
// remplace la précédente.
//
// ⚠ ELLE SURVIT AU DIALOGUE. Le script l'efface lui-même, d'ordinaire en
// fermant sa conversation ; celle qu'il oublie reste, comme chez le client
// d'origine. Seul un changement de carte la retire d'office — à celui qui la
// tient de l'y oublier.
//
// ⚠ HORS DU MONDE, comme la liste des charrettes : c'est ce qu'un script montre
// au joueur, pas un état de la carte.
class NpcIllustrationSlot {
 public:
  [[nodiscard]] const std::optional<NpcIllustration>& shown() const noexcept { return shown_; }
  void Show(NpcIllustration illustration) { shown_ = std::move(illustration); }
  void Clear() noexcept { shown_.reset(); }

 private:
  std::optional<NpcIllustration> shown_;
};

}  // namespace ro::sim
