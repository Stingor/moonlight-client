#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace ro::sim {

// LES BARRES DE RACCOURCIS : ce que le joueur a mis sous ses touches.
//
// ⚠ ELLES NE SAVENT PAS CE QU'ELLES PORTENT. Une case retient un identifiant et
// un rang, rien de plus : ni le nom, ni l'icône, ni si la compétence est encore
// apprise. C'est ce qui lui permet de survivre à tout — un changement de métier,
// un oubli de compétence, une carte où le serveur n'a pas encore parlé. Une case
// qui pointe sur ce qu'on ne sait plus lancer se montre grisée et ne part pas ;
// elle ne s'efface pas d'elle-même, parce qu'on peut réapprendre.
//
// ⚠ ET LE SERVEUR N'EN SAIT RIEN — il ne le PEUT pas. Sa table en tient
// soixante-seize au mieux (deux onglets de trente-huit), nous en avons
// quatre-vingt-seize : voir la fiche 0038. Tout reste chez le joueur, dans
// `app/shortcut_file.h`.

// Une case : la compétence, et le rang auquel elle part.
//
// Le rang est retenu À PART de ce que le joueur sait : poser Cold Bolt au rang
// trois et monter la compétence ensuite ne doit pas changer ce qui part.
struct ShortcutSlot {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;

  // UN OBJET, et c'est l'autre chose qu'une case peut porter.
  //
  // ⚠ LES DEUX NE COHABITENT JAMAIS. Poser l'un efface l'autre — `Assign` s'en
  // charge —, et rien dans le reste du client n'a à se demander lequel gagne.
  // Deux champs plutôt qu'un identifiant et un genre : les appelants qui ne
  // connaissent que les compétences continuent de lire `skill_id`, et ceux-là
  // sont nombreux.
  //
  // ⚠ ET C'EST L'IDENTIFIANT D'OBJET, PAS SA PLACE DANS LE SAC. Une place se
  // renumérote dès qu'on consomme ; l'identifiant, lui, désigne encore la bonne
  // potion après qu'on a bu la précédente.
  std::uint32_t item_id = 0;

  [[nodiscard]] bool filled() const noexcept { return skill_id != 0 || item_id != 0; }
  [[nodiscard]] bool holds_item() const noexcept { return item_id != 0; }
};

// ⚠ HUIT BARRES DE DOUZE CASES, ET LES DOUZE APPARTIENNENT À LA BARRE. Réduire
// une barre à trois cases est un réglage d'AFFICHAGE : il cache les neuf autres,
// il ne les renumérote pas.
//
// 🔴 C'EST CE QUI REND L'INDEX D'UNE CASE STABLE, et c'est la raison d'être de
// la disposition à plat. Si les barres se partageaient une réserve commune,
// baisser le nombre de cases montrées par la barre 2 décalerait toutes celles
// des barres 3 à 8 — donc leurs raccourcis, leurs entrées de fichier, et ce que
// le joueur avait rangé. Un réglage d'affichage qui déplace des touches est un
// piège qu'on ne voit qu'après.
inline constexpr std::size_t kShortcutBarCount = 8;
inline constexpr std::size_t kShortcutSlotsPerBar = 12;
inline constexpr std::size_t kShortcutSlotCount = kShortcutBarCount * kShortcutSlotsPerBar;

// L'INDEX D'UNE CASE, ET LES DEUX NOMBRES QU'IL PORTE.
//
// ⚠ UNE SEULE FAÇON DE DÉSIGNER UNE CASE, et c'est tout l'intérêt : le
// raccourci, le fichier, le glisser et le dessin parlent tous de la même chose.
// Un couple (barre, rang) promené partout aurait donné quatre conversions, donc
// quatre endroits où se tromper d'un.
[[nodiscard]] constexpr std::size_t ShortcutIndex(std::size_t bar, std::size_t rank) noexcept {
  return bar * kShortcutSlotsPerBar + rank;
}
[[nodiscard]] constexpr std::size_t ShortcutBarOfIndex(std::size_t index) noexcept {
  return index / kShortcutSlotsPerBar;
}
[[nodiscard]] constexpr std::size_t ShortcutRankOfIndex(std::size_t index) noexcept {
  return index % kShortcutSlotsPerBar;
}

class ShortcutBars {
 public:
  // Pose ce qu'une case porte. ⚠ CE QUI ENTRE DEPUIS LE GRIMOIRE OU LE SAC, pas
  // un déplacement : celui-ci passe par `Swap`, qui ne peut rien dupliquer.
  //
  // ⚠ RETIRE LA MÊME ENTRÉE DE SA BARRE, ET DE CELLE-LÀ SEULEMENT. Deux cases
  // voisines pour la même compétence ne servent qu'à se tromper de touche ;
  // mais poser `Heal` sur la barre de soin ET sur celle de mêlée est la chose
  // même pour quoi l'on fait huit barres. Voir la fiche 0038.
  void Assign(std::size_t index, ShortcutSlot what);
  void Clear(std::size_t index);

  // ÉCHANGE DEUX CASES — le glisser de l'une sur l'autre.
  //
  // 🔴 UN ÉCHANGE ET NON UN DÉPLACEMENT. Lâcher sur une case occupée doit rendre
  // son contenu à la place qu'on quitte : l'écraser en silence ferait perdre une
  // compétence pour un geste d'un pixel trop à droite. C'est ce que fait le jeu.
  //
  // ⚠ IL TRAVERSE LES BARRES : les index sont globaux, et une case de la barre 1
  // s'échange avec une case de la barre 6 comme avec sa voisine.
  void Swap(std::size_t first, std::size_t second);

  [[nodiscard]] ShortcutSlot At(std::size_t index) const;
  [[nodiscard]] std::span<const ShortcutSlot> slots() const noexcept { return slots_; }

  // Les douze cases d'une barre, dans l'ordre. Une barre hors compte rend une
  // vue VIDE plutôt que de sortir du tableau : les numéros de barre viennent
  // d'un réglage, et un réglage peut dériver.
  [[nodiscard]] std::span<const ShortcutSlot> BarSlots(std::size_t bar) const;

  // LA DERNIÈRE CASE REMPLIE D'UNE BARRE, plus un ; zéro quand elle est vide.
  //
  // ⚠ POUR L'ÉCRITURE DU FICHIER, et c'est sa seule raison d'être : écrire les
  // douze cases de chaque barre rendrait illisible un fichier fait pour se
  // corriger à la main, et une barre vide n'aurait pas à s'y montrer du tout.
  // L'index d'une case restant ce qu'il est, tronquer la queue ne déplace rien.
  //
  // Une barre hors compte rend zéro, comme une barre vide.
  [[nodiscard]] std::size_t used_span(std::size_t bar) const;

  // COMBIEN DE FOIS LES BARRES ONT CHANGÉ. Même rôle que pour les raccourcis
  // clavier : qui enregistre compare ce compte au sien, plutôt que de lever un
  // drapeau qu'un troisième appelant oublierait.
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

 private:
  std::array<ShortcutSlot, kShortcutSlotCount> slots_{};
  std::uint32_t revision_ = 0;
};

}  // namespace ro::sim
