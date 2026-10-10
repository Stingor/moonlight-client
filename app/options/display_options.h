#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/render/gl_context.h"

// CE QUE L'AFFICHAGE ÉCRIT DANS LE FICHIER, ET COMMENT ON LE RELIT.
//
// ⚠ UN EN-TÊTE PRIVÉ, dans `app/` : deux fichiers de la même classe s'en
// servent — celui qui POUSSE le réglage et celui qui le compose —, et rien
// d'autre n'a à connaître la façon dont une résolution s'écrit.
namespace moonclient {

// LA CLÉ DE L'ÉCHELLE DE L'INTERFACE. ⚠ ICI ET NON AVEC LES AUTRES CLÉS : les
// réglages du login la rétablissent seule, hors de la page qui la porte.
inline constexpr const char* kInterfaceScaleKey = "scale_interface";

// CE QU'ON REND SI PERSONNE NE CONFIRME — le filet de l'affichage.
//
// 🔴 UN MODE QUE LE MONITEUR REFUSE N'AFFICHE RIEN : la question est posée sous
// les yeux de personne, et c'est le silence qui doit annuler.
//
// ⚠ UNE ÉCHÉANCE À ZÉRO VEUT DIRE « RIEN N'ATTEND », et non « l'échéance est
// passée » : une horloge qui repart de zéro à chaque partie ferait sinon défaire
// un réglage que personne n'a touché.
//
// ⚠ NE SE PERSISTE PAS, et c'est ce qui le sépare des réglages qu'il protège :
// un filet tendu pour quelques secondes n'a aucun sens dans un fichier relu à la
// partie suivante.
struct DisplayRevert {
  std::string resolution;
  int display_mode = 0;
  // ⚠ L'ÉCHELLE EN FAIT PARTIE. Elle ne change pas de mode vidéo, mais elle
  // change TOUT ce qui se lit : à cinquante pour cent d'un petit écran, le
  // panneau où l'on vient de la poser n'est plus lisible — donc plus utilisable
  // pour la défaire.
  float interface_scale = 1.0F;
  std::uint32_t deadline_ms = 0;
};

// Une résolution écrite en toutes lettres, « 1280x720 ».
//
// ⚠ LE SÉPARATEUR EST UN « x » ASCII, jamais le « × » typographique : ce texte
// est une CLÉ de fichier de réglages, pas un libellé. Le libellé, lui, s'écrit
// joliment au moment de l'afficher.
[[nodiscard]] std::string ResolutionText(ro::render::Resolution size);

// La relit. Rend une taille nulle sur tout ce qui ne s'écrit pas ainsi — un
// fichier trafiqué, une version future, un champ vide.
[[nodiscard]] ro::render::Resolution ResolutionOf(std::string_view text);

// LA PLACE DE LA FENÊTRE, écrite « 100,50 ».
//
// ⚠ UNE VIRGULE ET NON UN « x » : ce ne sont pas deux longueurs mais deux
// coordonnées, et l'une comme l'autre peut être NÉGATIVE — un second écran posé
// à gauche du principal a des abscisses négatives.
[[nodiscard]] std::string PlaceText(ro::render::WindowPlace where);

// La relit. ⚠ ELLE REND UN « RIEN », et non une place à zéro : « jamais placée »
// doit se distinguer de « placée au coin », sinon toute première partie
// collerait la fenêtre en haut à gauche.
[[nodiscard]] std::optional<ro::render::WindowPlace> PlaceOf(std::string_view text);

// LES TROIS MODES, dans l'ordre où on les range dans le fichier. ⚠ CET ORDRE EST
// FIGÉ : c'est un nombre qui est écrit, et le réordonner changerait le réglage
// de tous ceux qui l'ont déjà posé.
[[nodiscard]] ro::render::DisplayMode DisplayModeOf(int rank);

// LES PLAFONDS D'IMAGES OFFERTS, en images par seconde. Zéro = aucun.
//
// ⚠ UNE LISTE ET NON UN CURSEUR : les valeurs qui comptent sont celles des
// écrans, et un curseur inviterait à s'arrêter sur 63 — un plafond qui ne
// correspond à rien donne un saccadement régulier, pire que pas de plafond.
//
// ⚠ ELLE VIVAIT AVEC LA CLASSE DES RÉGLAGES et n'y avait pas plus sa place que
// les paliers d'échelle juste en dessous : c'est un fait d'AFFICHAGE, et ce
// fichier les rassemble.
inline constexpr std::array<int, 6> kFrameLimits = {0, 30, 60, 120, 144, 240};

// ── L'ÉCHELLE DE L'INTERFACE, PAR PALIERS ───────────────────────────────────
//
// 🔴 DES PALIERS ET NON UN CURSEUR, et ce n'est pas une préférence d'aspect.
// Poser l'échelle redimensionne TOUTES les fenêtres, et la passe est à PERTE :
// chaque position et chaque taille est tronquée au pixel. Un curseur invite à
// balayer la plage, donc à empiler les troncatures ; il invite aussi à s'arrêter
// sur 1,07, une valeur que personne ne voulait. Une liste pose UNE valeur par
// clic, et chaque clic est un geste que l'on peut annoncer et défaire.
//
// ⚠ EN POUR CENT ENTIERS, parce que c'est ce que le joueur lit. Le fichier de
// réglages, lui, garde le FACTEUR : c'est ce que le moteur consomme, et le
// réécrire en pour cent périmerait tous les fichiers déjà posés.
//
// Les bornes sont celles du moteur — `ro::ui::kSmallestInterfaceScale` et
// `kLargestInterfaceScale`, soit 50 % et 200 %.
inline constexpr std::array<int, 7> kInterfaceScalePercents = {50, 75, 100, 125, 150, 175, 200};

// Le facteur que porte un pour cent : 125 -> 1,25.
[[nodiscard]] float InterfaceScaleOf(int percent);

// Le pour cent le plus proche d'un facteur, ARRONDI. ⚠ L'aller-retour n'est donc
// pas exact sous le pour cent : `/echelle 1.137` se lit « 114 % », et choisir
// cette entrée pose 1,14. C'est voulu — la liste ne promet que des pour cent.
[[nodiscard]] int InterfacePercentOf(float scale);

// Le libellé d'un pour cent, tel qu'il paraît dans la liste : « 125 % ».
//
// ⚠ ESPACE INSÉCABLE AVANT LE SIGNE, comme le veut le français. Un espace
// ordinaire laisserait le « % » passer seul à la ligne.
[[nodiscard]] std::string InterfaceScaleLabel(int percent);

}  // namespace moonclient
