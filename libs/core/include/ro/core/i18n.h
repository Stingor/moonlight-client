#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace ro::core {

// LA LANGUE DE L'INTERFACE.
//
// ⚠ LE FRANÇAIS EST LA CLÉ. Pas de clés symboliques (`kBoutonFermer`) : le
// libellé français DU CODE est la clé du catalogue. Ce choix se paie et se
// gagne, et les deux valent d'être dits.
//
//   gagné — le code reste lisible en français, comme ses commentaires et sa
//           documentation ; et la migration d'un site d'appel est mécanique,
//           on enveloppe le littéral.
//   payé  — retoucher un texte français perd SILENCIEUSEMENT sa traduction.
//           C'est ce que `MissingTexts` existe pour rattraper : tout texte
//           demandé mais absent du catalogue est retenu, et se réexporte en
//           gabarit à traduire.
//
// ⚠ LE FRANÇAIS NE COÛTE RIEN. Sans catalogue, `Tr` rend son argument après un
// seul test : aucune allocation, aucune recherche. La langue par défaut est
// exactement aussi rapide que si ce fichier n'existait pas.

// Traduit un littéral français.
//
// ⚠ UN LITTÉRAL, JAMAIS UNE CHAÎNE CONSTRUITE. `Tr("Fermer")`, et non
// `Tr(nom_du_joueur)`. Une chaîne faite à l'exécution ne sera dans aucun
// catalogue, et elle polluerait la liste des textes manquants avec autant
// d'entrées qu'elle a de valeurs possibles.
//
// Pour un texte à trous, c'est le GABARIT qu'on traduit, et lui seul :
//
//     std::format(Tr("{} objets vendus"), count)
//
// ⚠ ET LES MESSAGES DU SERVEUR SE RELAIENT TELS QUELS. Les traduire les rendrait
// faux : le serveur dit ce qu'il dit, et le joueur le recopie dans un rapport.
//
// Le résultat n'est JAMAIS nul : catalogue absent, clé manquante ou traduction
// vide rendent le français. Une traduction incomplète dégrade, elle ne blanchit
// pas l'interface.
//
// ⚠ Le pointeur appartient au catalogue et meurt au prochain changement de
// langue. Le consommer dans l'image est sûr — tout appel ImGui copie ; le
// ranger dans un `static const char*` ne l'est pas.
//
// ⚠ LE FORMAT SE VÉRIFIE SUR LE LITTÉRAL FRANÇAIS. `Tr` rend un pointeur, que
// GCC et Clang refusent de prendre pour format d'`ImGui::Text` — le contrôle
// des arguments se perdrait. `format_arg` leur dit que le résultat EST le
// format passé en argument : `Text(Tr("%d / %d"), a, b)` se vérifie comme
// `Text("%d / %d", a, b)`. Une traduction qui change les spécificateurs n'est
// pas vue par là ; c'est `check_translations.py` qui la garde.
#if defined(__GNUC__) || defined(__clang__)
[[nodiscard]] __attribute__((format_arg(1))) const char* Tr(const char* french);
#else
[[nodiscard]] const char* Tr(const char* french);
#endif

// MARQUE UN LITTÉRAL POUR LE CATALOGUE, sans le traduire ici.
//
// ⚠ IL EXISTE PARCE QUE CERTAINS TEXTES SE DÉCLARENT LOIN D'OÙ ILS S'AFFICHENT.
// Le titre d'une fenêtre est une constante, et une constante ne peut pas
// appeler `Tr` : la traduction n'existe pas encore quand elle s'initialise, et
// elle devrait changer à chaque changement de langue.
//
// Sans marqueur, ces textes seraient INVISIBLES au relevé qui bâtit les
// catalogues — ils n'y entreraient jamais, et resteraient en français sans que
// personne ne sache pourquoi.
//
// Il ne fait rien à l'exécution : il rend son argument. C'est le relevé qui le
// lit, pas le programme.
[[nodiscard]] constexpr const char* TrText(const char* french) noexcept { return french; }

// La langue en cours, par son code — « fr » tant qu'aucun catalogue n'est
// chargé.
//
// ⚠ UNE COPIE, et non une vue : la chaîne vit sous le verrou du catalogue, et
// le prochain `LoadLanguage` la remplace. Une vue rendue pendrait dès lors.
[[nodiscard]] std::string CurrentLanguage();

// Combien de fois la langue a changé depuis le démarrage.
//
// Sert à qui GARDE un texte traduit d'une image à l'autre : ImGui compose le
// nom d'une fenêtre en une seule chaîne, et la refabriquer à chaque image
// coûterait une allocation par fenêtre et par image. Comparer ce compte dit
// quand il faut la refaire — et seulement alors.
[[nodiscard]] std::size_t LanguageSerial();

// Charge un catalogue et bascule dessus.
//
// LE FORMAT, tenu simple exprès : un traducteur l'ouvre dans n'importe quel
// éditeur, et un `diff` se lit.
//
//     # une ligne de commentaire
//     fr: Journal
//     tr: Log
//
// `fr:` porte le texte français, `tr:` sa traduction. Le reste de la ligne est
// pris tel quel, à DEUX échappements près : `\n` pour un saut de ligne, `\\`
// pour l'antislash. Rien d'autre — ni guillemet, ni signe égal, ni accolade de
// gabarit — pour garder le fichier lisible et corrigeable à la main.
//
// Une traduction VIDE est licite et veut dire « pas encore faite » : l'entrée
// reste dans le fichier, visible, et le texte s'affiche en français.
[[nodiscard]] Result<void> LoadLanguage(const std::filesystem::path& catalogue,
                                        std::string language);

// Revient au français, et oublie le catalogue.
void UseFrench();

// Les textes demandés à `Tr` qui n'étaient pas dans le catalogue.
//
// ⚠ C'EST LA CONTREPARTIE DU CHOIX « le français est la clé ». Un texte
// retouché dans le code perd sa traduction sans un mot ; cette liste est ce qui
// le rattrape, et elle ne se remplit qu'en jouant, donc elle dit ce qui manque
// VRAIMENT plutôt que ce qu'un relevé de sources croit deviner.
[[nodiscard]] std::vector<std::string> MissingTexts();

// Écrit les textes manquants en gabarit de catalogue, prêt à traduire.
[[nodiscard]] Result<void> ExportMissing(const std::filesystem::path& path);

}  // namespace ro::core
