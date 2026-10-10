#pragma once

#include <cstdint>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// UNE FENÊTRE DE JEU, HABILLÉE EN ENTIER : le cadre, le fond, et la BARRE DE
// TITRE — puce, titre, bouton de fermeture, glisser.
//
// ⚠ LA BARRE EST REPRISE, PAS HABILLÉE PAR-DESSUS, et c'est la seule façon qui
// marche. ImGui peint le fond ET le titre pendant `Begin` : tout ce qu'on
// ajoute ensuite passe au-dessus, si bien qu'un habillage posé sur sa barre
// couvrirait le titre et le bouton de fermeture — qui resteraient cliquables et
// invisibles, pire que pas d'habillage. On ouvre donc SANS barre, et l'on peint
// la nôtre, ce qui oblige à refaire le glisser.
//
// ⚠ CE N'EST PAS POUR LES FENÊTRES D'ATELIER. Le journal et l'inspecteur de
// trames ne seront pas dans le jeu livré ; les habiller serait du travail jeté.
//
// S'emploie comme `Begin`/`End`, même règle : `EndSkinnedWindow` s'appelle
// TOUJOURS, même quand l'ouverture rend faux.
//
//   if (BeginSkinnedWindow("Titre###id", &open, art)) { ... }
//   EndSkinnedWindow();
//
// Sans habillage complet, la fenêtre s'ouvre telle qu'ImGui la fait — barre de
// titre comprise. Une installation sans art reste jouable.

// Ce qui sépare la puce du bord, et le titre de la puce, en pixels d'art.
inline constexpr float kBulletMarginPixels = 5.0F;
inline constexpr float kBulletTextGapPixels = 4.0F;
// Ce qui sépare deux boutons système, en pixels d'art.
inline constexpr float kSystemButtonGapPixels = 2.0F;
// Ce qui sépare le premier bouton système du bord DROIT. ⚠ CE N'EST PAS LA
// MARGE DE LA PUCE : le client laisse quatre pixels à droite et cinq à gauche,
// et les deux se relèvent séparément.
inline constexpr float kSystemEdgePixels = 4.0F;

// LA PUCE DE LA BARRE DE TITRE DEVIENT UN BOUTON, pour la prochaine fenêtre.
//
// ⚠ POSÉE AVANT `BeginSkinnedWindow`, consommée par lui — même mécanique que
// les `SetNextWindow…` d'ImGui, et pour la même raison : l'appelant n'a rien à
// garder entre deux images.
//
// ⚠ DÉCORATIVE PAR DÉFAUT, et il le faut : une puce qui se clique sur TOUTE
// fenêtre promettrait à chacune une action qu'elle n'a pas. Une fenêtre ne la
// réclame que lorsqu'elle a quelque chose à offrir derrière.
//
// 🔴 ET LA PUCE NE CHANGE PAS D'ASPECT quand elle devient cliquable : c'est le
// dessin du client, et il ne se corrige pas. Restent la main qui pointe et
// l'INFOBULLE, qui est donc le seul endroit où la fenêtre peut dire ce qu'il y a
// derrière. La laisser vide rendrait la puce cliquable indiscernable de l'autre.
void SetNextWindowBulletClickable(const char* tooltip);

// La puce de la fenêtre COURANTE vient-elle d'être cliquée ? À lire entre le
// `Begin` et le `End`, comme tout ce qu'une fenêtre rend.
[[nodiscard]] bool TitleBulletClicked();

// COMBIEN DE RACCOURCIS UNE BARRE DE TITRE PORTE. Trois : les fenêtres
// d'objets mènent chacune aux DEUX autres, et le rapport de bug s'y ajoute.
inline constexpr int kTitleActionCount = 3;

// UN BOUTON DE PLUS DANS LA BARRE DE TITRE, à gauche des boutons système.
//
// ⚠ MÊME MÉCANIQUE QUE LA PUCE : posé avant `BeginSkinnedWindow`, consommé par
// lui. Les deux textes sont DÉJÀ TRADUITS — cette couche ne traduit rien.
//
// ⚠ L'ORDRE DE POSE EST L'INVERSE DE L'ORDRE AFFICHÉ : chaque bouton devient la
// butée du suivant, qui se place à sa GAUCHE. Le premier posé est donc le plus à
// droite, contre les boutons système.
//
// ⚠ `enabled` À FAUX LE GRISE, il ne l'efface pas : « cette fenêtre n'a nulle
// part où aller » se dit mieux par un bouton éteint que par un bouton absent,
// qui passe pour un défaut. L'infobulle porte alors la raison.
//
// ⚠ IL S'EFFACE QUAND LA BARRE EST TROP ÉTROITE, plutôt que de recouvrir le
// titre. Une fenêtre d'objets se rétrécit librement, et le titre est ce qui dit
// où l'on est.
void SetNextWindowTitleAction(const char* label, const char* tooltip, bool enabled);

// Ce bouton-là vient-il d'être cliqué ? `rank` est l'ordre de POSE, celui des
// appels. À lire entre le `Begin` et le `End`.
[[nodiscard]] bool TitleActionClicked(int rank);

// L'ÉPINGLE DE LA BARRE DE TITRE, pour la prochaine fenêtre.
//
// 🔴 CE QU'ELLE DIT : « Échap ne me ferme plus ». Elle ne fige ni la position ni
// la taille — c'est le verrou de taille qui fait cela. Une fenêtre épinglée sort
// de la pile d'Échap, donc elle n'avale pas non plus la touche : le jeu la
// reçoit et son menu s'ouvre, ce qui est bien ce que veut un joueur qui a
// délibérément cloué une fenêtre à l'écran.
//
// ⚠ L'ÉTAT APPARTIENT À L'APPELANT, comme `open` : `ui` ne possède rien, et
// c'est lui qui sait où le retenir d'une session à l'autre. Un pointeur nul
// n'épingle rien — la fenêtre n'a pas d'épingle du tout.
//
// ⚠ POSÉE AVANT `BeginSkinnedWindow`, consommée par lui, comme les autres
// `SetNextWindow…`.
//
// ⚠ ET ELLE NE CHANGE PAS D'ASPECT AU SURVOL : son image dit son ÉTAT. La
// montrer plantée au passage de la souris ferait croire la fenêtre épinglée.
void SetNextWindowPinnable(bool* pinned);

// L'ARRONDI DE LA PROCHAINE FENÊTRE HABILLÉE, en pixels avant l'échelle, à la
// place de celui de la palette. Zéro garde celui de la palette.
//
// ⚠ IL VAUT POUR TOUT CE QUE LA FENÊTRE OUVRE sans arrondi à soi — une
// infobulle ordinaire le prend aussi, et il entoure ainsi la fiche entière.
// Posé avant `BeginSkinnedWindow`, consommé par lui.
void SetNextWindowRounding(float pixels);

// Le titre AFFICHÉ d'un nom de fenêtre : ce qui précède `##`, comme ImGui le
// fait. Le reste est l'identité, et ne se montre pas.
[[nodiscard]] std::string_view ShownTitle(std::string_view name);

// Avec ou sans barre de titre.
//
// ⚠ LE DIALOGUE D'UN PNJ N'EN A PAS DANS LE CLIENT D'ORIGINE : c'est une boîte
// encadrée, qu'on ferme par son bouton et qu'on déplace en l'attrapant où il n'y
// a rien à cliquer.
//
// 🔴 CELUI DE CE CLIENT EN A UNE, ET ELLE PORTE LE NOM DU PNJ. Savoir à qui
// l'on parle vaut la barre — le client d'origine, lui, ne le dit nulle part.
// Voir l'ADR 0051.
// ⚠ UNE FENÊTRE DU JEU N'A PAS DE CADRE EN NEUF MORCEAUX. Son corps est un
// aplat presque blanc bordé d'un filet gris, que l'interface dessine à partir de
// la palette ; seule la BARRE DE TITRE est faite de pièces. Les huit morceaux de
// `sysbox` que porte l'installation appartiennent aux BOÎTES MODALES et aux
// panneaux de description, qui n'ont pas de barre de titre du tout.
// COMMENT UNE FENÊTRE PORTE SON TITRE.
//
// 🔴 `kDescription` N'EST PAS UNE VARIANTE DE STYLE MAIS UNE AUTRE FENÊTRE DU
// CLIENT. Il donne à ses descriptions une barre CLAIRE et basse — `skill_upbar`
// — sur un corps BLANC, là où toutes ses autres fenêtres ont la barre bleue sur
// un corps gris. C'est le seul endroit du jeu où le texte se lit sur du blanc,
// et la barre l'annonce avant qu'on ait lu une ligne.
//
// ⚠ ET ELLE N'A QUE SA CROIX : pas de puce, pas de minimiser, pas d'épingle. Le
// client n'en met pas, et ces trois-là occuperaient la place du nom d'objet, qui
// est long.
enum class SkinnedTitle : std::uint8_t { kBar, kNone, kDescription };

// L'OMBRE DU TITRE DE LA PROCHAINE FENÊTRE, décalée d'un pixel sous son texte.
//
// ⚠ POSÉE AVANT `BeginSkinnedWindow`, consommée par lui, comme les autres
// `SetNextWindow…`. Zéro = pas d'ombre.
//
// Le client s'en sert pour dire qu'un équipement est CASSÉ : le nom prend une
// ombre rouge, dans sa barre de titre comme dans son inventaire.
void SetNextWindowTitleShadow(ImU32 color);

// L'ÉPAISSEUR DU BORD DU CADRE EN BOÎTE (`sysbox`), en pixels d'écran.
//
// ⚠ ELLE VIENT DE L'ART, jamais d'une constante : un habillage de joueur peut la
// changer. Elle sert à donner sa marge à ce qu'on pose dans le cadre.
[[nodiscard]] float DescPanelEdge(const SkinArt& art);

// LE CADRE « PANNEAU DE DESCRIPTION » — fond clair et cadre `sysbox` en neuf
// morceaux — dans un rectangle quelconque, sur un ImDrawList QUELCONQUE.
//
// ⚠ C'EST CE QUI PERMET DE L'HABILLER SANS FENÊTRE : une infobulle n'a pas de
// barre de titre où accrocher un cadre, et elle ne connaît sa taille qu'APRÈS
// son contenu. On peint donc à la main, sur la liste qu'on veut.
//
// ⚠ `fill_bg` PEINT LE CORPS ENTRE LES PIÈCES, jamais dessous : l'extérieur
// arrondi des coins reste transparent, comme chez le client. À faux quand
// l'appelant a déjà peint son propre fond.
void DrawDescPanelFrame(ImDrawList* into, const SkinArt& art, ImVec2 top_left, ImVec2 bottom_right,
                        bool fill_bg);

[[nodiscard]] bool BeginSkinnedWindow(const char* name, bool* open, const SkinArt& art,
                                      SkinnedTitle title = SkinnedTitle::kBar,
                                      ImGuiWindowFlags flags = 0);
void EndSkinnedWindow();

// CE QU'ÉCHAP FAIT À UNE MODALE.
enum class ModalEscape : std::uint8_t {
  // Échap la referme.
  kCloses,
  // ELLE NE SE CONGÉDIE PAS : seuls ses boutons en sortent. Pour une décision
  // qui ne doit pas disparaître d'une touche — une invitation reçue.
  kIgnored,
};

// UNE BOÎTE MODALE DU JEU À CONTENU LIBRE : la barre de titre des fenêtres, le
// corps de la palette, et le voile d'ImGui derrière.
//
// ⚠ CE N'EST PAS `DrawModalBox`, qui pose une question et deux boutons. Celle-
// ci porte ce que l'appelant y met — un champ à retaper, une liste de fichiers,
// un éditeur entier — et se ferme par ses propres boutons.
//
// ⚠ ELLE S'OUVRE PAR `AskModal`, comme l'autre, et `EndSkinnedModal` NE
// S'APPELLE QUE SI L'OUVERTURE A RENDU VRAI. Échap la referme, sauf si
// l'appelant demande `ModalEscape::kIgnored` : une boîte qu'on ne peut pas
// congédier au clavier retient le jeu entier, et ne se justifie que pour une
// décision qui ne doit pas disparaître d'une touche.
//
// ⚠ SA PUCE EST DÉCORATIVE et elle n'a aucun bouton système : un dialogue n'a
// ni réglages où mener, ni raison d'être replié.
[[nodiscard]] bool BeginSkinnedModal(const char* name, const SkinArt& art,
                                     ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize,
                                     ModalEscape escape = ModalEscape::kCloses);
void EndSkinnedModal();

// UNE INFOBULLE HABILLÉE COMME UN PANNEAU DE DESCRIPTION : fond clair arrondi,
// cadre `sysbox`, texte sombre.
//
// 🔴 UNE INFOBULLE, ET PAS UNE FENÊTRE. ImGui ordonne les fenêtres par focus :
// une fenêtre ordinaire, même soumise après celle qu'on survole, passerait
// DERRIÈRE elle. Les infobulles vivent dans la couche avant, et ImGui les
// recadre lui-même près des bords.
//
// ⚠ LE CADRE SE PEINT APRÈS COUP, SUR UN SECOND CANAL, et c'est le seul moyen :
// il doit être DERRIÈRE le contenu, mais on ne connaît la taille de l'infobulle
// qu'une fois le contenu écrit. `EndSkinnedTooltip` fait la fusion.
//
// ⚠ `EndSkinnedTooltip` NE S'APPELLE QUE SI L'OUVERTURE A RENDU VRAI : elle
// dépile ce que l'ouverture a posé.
//
// `widest` borne la largeur ; zéro prend celle du panneau de description.
[[nodiscard]] bool BeginSkinnedTooltip(const SkinArt& art, float widest = 0.0F);
void EndSkinnedTooltip(const SkinArt& art);

// Les coins d'une infobulle habillée ; son corps est dans `skin_look.h`, avec
// les couleurs que l'habillage règle.
inline constexpr float kSkinnedTooltipRounding = 4.0F;
// Au-delà, le texte se replie : une bulle qui traverse l'écran se lit moins bien
// que quatre lignes de plus.
inline constexpr float kSkinnedTooltipWidest = 330.0F;
// Et elle ne prend jamais tout l'écran en hauteur.
inline constexpr float kSkinnedTooltipScreenShare = 0.8F;

}  // namespace ro::ui
