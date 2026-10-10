#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"

namespace ro::ui {

// LA FENÊTRE DE JOURNAL : ce que le programme a écrit, dans le programme.
//
// POURQUOI ELLE COMPTE PLUS QU'ELLE N'EN A L'AIR. Le journal partait sur la
// console et dans un fichier ; or une console se ferme avec la fenêtre, défile,
// ou n'existe pas selon la façon dont le client a été lancé, et le fichier ne
// se lit qu'une fois la partie finie — c'est-à-dire trop tard pour regarder.
// Entre les deux, il n'y avait rien : le moment où l'on voudrait lire est
// exactement celui où le jeu tourne.
//
// C'est aussi la sortie des COMMANDES DE DÉVELOPPEMENT. Elles écrivaient dans
// le journal de conversation, qui n'en tient que huit lignes et qui appartient
// au jeu : y mêler nos relevés les tronquait ET salissait le chat.

// LE NOM DE LA FENÊTRE, et pourquoi il porte trois dièses.
//
// ⚠ IMGUI TIRE L'IDENTITÉ D'UNE FENÊTRE DE SON TITRE, ce qui la lierait à la
// LANGUE. Le jour où l'interface se traduit, « Journal » devient « Log », et
// c'est une autre fenêtre : elle rouvre au milieu de l'écran, sans sa taille ni
// sa place, et le réglage qui retient son ouverture ne la retrouve plus.
//
// Ce qui suit `###` est l'IDENTITÉ et ne se traduit jamais ; ce qui précède est
// le titre montré. Le poser maintenant coûte une ligne ; le poser quand cinquante
// fenêtres existent coûte cinquante réglages de joueurs perdus.
inline constexpr const char* kLogWindowTitle = core::TrText("Journal");

// L'identité seule, pour qui retient l'ouverture de cette fenêtre.
inline constexpr const char* kLogWindowId = "log";

// Ce qu'un champ de filtre peut porter, terminaison comprise.
//
// Une longueur FIXE plutôt qu'une chaîne qui grandit, et ce n'est pas de
// l'avarice : ImGui écrit la saisie directement dans le tampon qu'on lui donne,
// et l'état de la fenêtre reste ainsi une donnée pure, sans allocation ni
// dépendance à ImGui dans cet en-tête. C'est ce qui laisse `SelectLogLines` se
// vérifier sans lui.
inline constexpr std::size_t kLogFilterSize = 128;

// Ce que le joueur a réglé, et qui doit survivre d'une image à la suivante.
struct LogWindowState {
  bool open = false;
  // Quelles gravités montrer, dans l'ordre de `core::LogLevel`.
  //
  // ⚠ LE DÉTAIL EST ÉTEINT AU DÉPART. Il est écrit pour qui cherche une panne
  // précise ; allumé par défaut, il noie les trois autres et rend la fenêtre
  // inutile au moment où elle sert.
  std::array<bool, 4> shown = {false, true, true, true};
  // Coller au bas de la liste quand une ligne arrive.
  //
  // ⚠ ET LÂCHER PRISE DÈS QU'ON REMONTE : une fenêtre qui redescend de force
  // pendant qu'on lit est une fenêtre qu'on ferme. Le suivi se reprend tout
  // seul quand on revient en bas.
  bool follow = true;
  // Le texte à chercher dans les lignes. Vide, il ne retient rien.
  //
  // ⚠ LA CASSE EST IGNORÉE SUR L'ASCII SEUL. « ERREUR » trouve « erreur », mais
  // « ÉTEINT » ne trouve pas « éteint » : abaisser les octets au-delà de 0x7F
  // réécrirait de l'UTF-8 à deux octets et casserait les accents. C'est la règle
  // de `core::AsciiLower`, et elle est préférable ici à une correspondance plus
  // savante qui abîmerait ce qu'elle cherche.
  std::array<char, kLogFilterSize> filter = {};

  // LES LIGNES CHOISIES, par leur numéro d'écriture.
  //
  // ⚠ PAR NUMÉRO ET NON PAR RANG : le journal grandit pendant qu'on lit, et un
  // rang désignerait une autre ligne à chaque écriture. On copierait alors autre
  // chose que ce qu'on a cliqué, sans que rien ne le signale.
  std::set<std::uint64_t> selected;
  // La dernière ligne cliquée, d'où une sélection au clavier s'étend. Zéro tant
  // que rien n'a été cliqué.
  std::uint64_t anchor = 0;
};

// Le texte d'un groupe de lignes, tel qu'on le colle ailleurs.
//
// ⚠ CE QUI EST COPIÉ EST CE QUI EST MONTRÉ, horodatage compris. Copier le seul
// message donnerait un texte plus propre et moins UTILE : c'est l'écart entre
// deux instants qui dit ce qui s'est passé, et il disparaîtrait.
[[nodiscard]] std::string JoinLogLines(std::span<const core::LoggedLine> lines);

// Les lignes retenues qui sont AUSSI choisies, dans l'ordre d'écriture.
//
// ⚠ L'INTERSECTION, ET NON LA SÉLECTION SEULE : une ligne choisie puis cachée
// par un filtre ne doit pas revenir dans le presse-papiers. Ce qu'on colle est
// ce qu'on voit.
[[nodiscard]] std::vector<core::LoggedLine> SelectedLogLines(
    std::span<const core::LoggedLine> shown, const LogWindowState& state);

// Vrai quand cette ligne passe le filtre. Un filtre vide laisse tout passer.
//
// Exposée parce que c'est la seule règle discutable de la fenêtre, et qu'une
// règle qui se vérifie seule vaut mieux qu'une règle enfouie dans du dessin.
[[nodiscard]] bool LinePassesFilter(std::string_view text, std::string_view filter);

// Les lignes à montrer, dans l'ordre où elles ont été écrites.
//
// Fonction pure, et séparée du dessin pour cette raison : c'est elle qui porte
// la seule décision de la fenêtre, et elle se vérifie sans ImGui.
[[nodiscard]] std::vector<core::LoggedLine> SelectLogLines(std::span<const core::LoggedLine> lines,
                                                           const LogWindowState& state);

// Dessine la fenêtre si elle est ouverte, et rend le nombre de lignes montrées.
//
// Le compte rendu n'est pas décoratif : sans lui, un test ne pourrait dire que
// « ça n'a pas planté ». Il vaut zéro quand la fenêtre est fermée.
std::size_t DrawLogWindow(LogWindowState& state, std::span<const core::LoggedLine> lines);

}  // namespace ro::ui
