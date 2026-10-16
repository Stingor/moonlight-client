#pragma once

// LE CLAVIER D'UNE LISTE : flèches haut et bas pour l'entrée d'avant ou
// d'après, Page précédente et Page suivante pour une page visible, Début et Fin
// pour la première et la dernière. La liste DÉFILE pour garder l'entrée choisie
// en vue. Les touches se répètent, tenues.
//
// 🔴 LES FLÈCHES SEULES QUAND UNE SAISIE A LA MAIN : Début, Fin et les pages y
// déplacent le curseur du texte ; haut et bas n'y font rien, et l'usage du
// client — le menu des PNJ, la liste des portails — est qu'elles parcourent la
// liste depuis son champ de recherche.

#include <cstdint>
#include <functional>

namespace ro::ui {

enum class ListKeyMove : std::uint8_t {
  kNone,
  kPrevious,
  kNext,
  kPageUp,
  kPageDown,
  kFirst,
  kLast,
};

// Aucune entrée choisie.
inline constexpr int kListNoEntry = -1;

// LE RANG OÙ LA TOUCHE MÈNE, borné à la liste — il ne boucle pas. Sans entrée
// choisie, la flèche du bas et la page suivante mènent à la première, celles
// du haut à la dernière. Une liste vide rend `kListNoEntry`. `page_rows` vaut au
// moins un.
[[nodiscard]] int ListKeyTarget(int current, int count, int page_rows, ListKeyMove move);

// Le même rang, parmi les seules entrées que `selectable` accepte — vide : toutes.
// Le rang visé qui ne se choisit pas cède la place à la plus proche qui se
// choisit, d'abord dans le sens de la touche (vers le bas pour la suivante, la
// page suivante et le début), puis dans l'autre. Aucune : `kListNoEntry`.
[[nodiscard]] int ListKeyTarget(int current, int count, int page_rows, ListKeyMove move,
                                const std::function<bool(int)>& selectable);

// La touche de liste pressée à cette image, ou `kNone`. `arrows_only` ne lit
// que haut et bas.
[[nodiscard]] ListKeyMove PressedListKey(bool arrows_only);

// Combien d'entrées de `row_pitch` pixels la fenêtre courante montre en
// entier, au moins une.
[[nodiscard]] int VisibleListRows(float row_pitch);

// Fait défiler la fenêtre courante juste assez pour montrer le dernier élément
// posé.
void KeepLastItemInView();

// Quand le clavier prend une entrée d'une liste déroulante.
enum class ComboKeyChoice : std::uint8_t {
  // Le choix suit la touche, comme sous la molette ; Entrée referme.
  kFollowsKey,
  // La touche déplace un curseur, Entrée prend l'entrée qu'il marque et
  // referme. Pour une liste où chaque pas COÛTE — un ordre au serveur.
  kOnEnter,
};

// LE CLAVIER D'UNE LISTE DÉROULANTE OUVERTE que l'appelant remplit lui-même :
// les touches de `PressedListKey` — les flèches seules quand une saisie de la
// liste a la main —, la liste qui défile pour garder l'entrée en vue, et
// Entrée qui referme.
//
// Se construit juste après l'ouverture, avant les entrées — dans la fenêtre
// fille qui les porte, s'il y en a une —, avec le rang de l'entrée choisie
// (`kListNoEntry` sinon) et le nombre d'entrées. Les rangs sont ceux des
// ENTRÉES posées, dans l'ordre : un intitulé de groupe n'en est pas une, et une
// liste filtrée compte ses entrées visibles. Une entrée que `selectable`
// refuse — désactivée — se saute.
//
// ⚠ `Took` S'APPELLE APRÈS CHAQUE ENTRÉE, juste derrière son `Selectable`, et
// rend vrai quand elle vient d'être prise, au clic ou au clavier : l'appelant y
// fait ce qu'il fait d'un clic, et le clavier ne peut pas émettre autre chose.
class ComboKeys {
 public:
  ComboKeys(int chosen, int count, const std::function<bool(int)>& selectable = {},
            ComboKeyChoice choice = ComboKeyChoice::kFollowsKey);

  // `clicked` : ce que le `Selectable` de l'entrée `rank` vient de rendre, garde
  // de l'appelant comprise.
  [[nodiscard]] bool Took(int rank, bool clicked);

  // L'entrée que la liste montre choisie : celle que la touche vient de prendre,
  // le curseur sous `kOnEnter`, l'entrée choisie sinon.
  [[nodiscard]] int marked() const { return marked_; }
  // L'entrée que la touche vient de viser, ou `kListNoEntry` : une liste qui ne
  // pose que ses lignes visibles doit la poser aussi.
  [[nodiscard]] int keyed() const { return keyed_; }

 private:
  int marked_ = kListNoEntry;
  int keyed_ = kListNoEntry;
  int taken_ = kListNoEntry;
};

}  // namespace ro::ui
