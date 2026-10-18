#pragma once

#include <cstdint>
#include <span>

#include <imgui.h>

#include "ro/ui/inventory_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_art.h"

// LE CHROME D'UNE GRILLE D'OBJETS, partagé par les fenêtres qui en portent une.
//
// ⚠ LE SAC, LA CHARRETTE ET LE COFFRE SONT DES SŒURS, et c'est le client qui le
// dit : même cadre de fenêtre, même modèle d'objet, mêmes images d'onglets. Ce
// qui les sépare est ce qu'on peut FAIRE d'un objet — utiliser, jeter, envoyer
// ailleurs —, jamais la façon de le montrer.
//
// En-tête PRIVÉ : il vit dans `src/` et non dans `include/`, parce que personne
// hors de cette bibliothèque n'a à dessiner une demi-grille.
namespace ro::ui {

// Les cases sont JOINTIVES. Ce n'est pas un oubli d'espacement : c'est le
// dessin du client, dont le fond pavé ne laisserait voir aucun joint.
inline constexpr float kCellGapPixels = 0.0F;

// Le pied porte deux lignes, dans les trois fenêtres.
inline constexpr int kFooterLines = 2;
// Ce qui sépare le bord du bandeau de sa première icône, et l'icône de son
// texte.
inline constexpr float kFooterInsetPixels = 6.0F;
inline constexpr float kFooterIconGapPixels = 3.0F;

// LA PLACE QU'IL FAUT LAISSER A LA POIGNEE DE REDIMENSIONNEMENT, au coin bas
// droit. Le pied s'arrete avant elle : un montant aligne a droite passerait
// dessous, et la poignee le couvrirait a moitie.
inline constexpr float kResizeGripMarginPixels = 16.0F;

// CE QUI SÉPARE LE MARQUEUR D'AIDE DE CE QU'IL SUIT, à sa droite. Collé, il se
// lirait comme un morceau du bouton voisin.
inline constexpr float kFooterHelpGapPixels = 10.0F;

// LE POIDS ARRIVE EN DIXIÈMES D'UNITÉ, et les trois contenants le comptent
// pareil : `sd.weight` comme `sd.cart_weight` sont dans cette unité, et le
// serveur multiplie même son plafond de charrette par dix en le lisant
// (`battle.cpp`, `max_cart_weight *= 10`). Afficher le nombre brut donnerait un
// poids dix fois trop grand, et un plafond crédible à côté.
inline constexpr std::int64_t kWeightTenths = 10;

// Le bandeau en trois morceaux, ou un aplat quand l'installation ne les a pas.
void DrawFooterBar(ImDrawList* into, const SkinArt& art, ImVec2 top_left, ImVec2 bottom_right);

// Une icône du pied, à sa taille. Rend la largeur qu'elle a prise.
[[nodiscard]] float DrawFooterIcon(ImDrawList* into, const SkinImage& icon, float x,
                                   float center_y);

// UN INTERRUPTEUR DU PIED, dessiné par son image. Rend vrai au clic.
//
// ⚠ DEUX ÉTATS ET PAS DE SURVOL : c'est l'art du jeu qui le veut — le verrou de
// jet n'a qu'un « off » et un « on » —, et les images de survol de ceux qui en
// ont restent inemployées.
//
// `left_middle` est le bord GAUCHE et le milieu VERTICAL : le pied place ses
// boutons de proche en proche, et la hauteur d'un bitmap du jeu ne se devine
// pas. La largeur réellement occupée revient par `width`.
//
// ⚠ IL REND LE CURSEUR DE MISE EN PAGE INTACT, marque du contenu comprise. Le
// pied se dessine en coordonnées absolues, hors du flux : sans cette remise en
// place, un bouton posé plus bas que la grille ferait croire à la fenêtre
// qu'elle déborde, et lui donnerait un ascenseur.
[[nodiscard]] bool FooterToggle(const char* id, const SkinImage& on, const SkinImage& off,
                                bool active, ImVec2 left_middle, const char* tooltip, float* width);

// UN BOUTON DU PIED DESSINÉ PAR SON IMAGE, à trois états.
//
// ⚠ CE N'EST PAS UN INTERRUPTEUR, et c'est toute la différence avec son
// voisin : il ne retient rien, et ses trois images sont le REPOS, le SURVOL
// et l'ENFONCÉ — pas un « mis » et un « ôté ». Le jeu livre bien les trois
// pour ce bouton-là, alors que ses verrous n'en ont que deux.
//
// Mêmes précautions que `FooterToggle` : coordonnées absolues, curseur de
// mise en page rendu intact, et la largeur occupée revient par `width`.
//
// ⚠ `art_scale` REDUIT LE DESSIN, il ne le recadre pas : c'est pour l'art AJOUTÉ
// au jeu, qui n'a pas été taillé pour la ligne d'un pied et s'y trouve trop
// gros. L'art d'origine se pose à un, et un appelant qui le rapetisserait ne
// ferait que le rendre flou.
[[nodiscard]] bool FooterImageButton(const char* id, const SkinImage& out, const SkinImage& over,
                                     const SkinImage& down, ImVec2 left_middle, const char* tooltip,
                                     float* width, float art_scale = 1.0F);

// UN BOUTON DU PIED, habillé, posé à une place absolue.
//
// 🔴 MÊME REMISE EN PLACE DU CURSEUR QUE `FooterToggle`, et elle se paie de la
// même façon quand elle manque : un bouton posé plus bas que la fin de la zone
// de contenu fait croire à la fenêtre qu'elle déborde. Elle ouvre alors un
// ascenseur qui n'a RIEN à faire défiler, et cet ascenseur mange la poignée de
// redimensionnement.
//
// `left_middle` est son bord GAUCHE et son milieu VERTICAL, comme pour les
// interrupteurs. `width` est sa largeur, déjà mesurée par l'appelant : c'est
// elle qui lui dit où commence ce bord gauche.
//
// ⚠ SA HAUTEUR NE SE DONNE PAS : c'est son ART qui se centre sur `left_middle`,
// et lui seul sait de combien son item le déborde par le haut. Voir
// `SkinSmallButtonTopCenteredOn`.
[[nodiscard]] bool FooterButton(const char* label, const SkinArt& art, ImVec2 left_middle,
                                float width);

// LE MARQUEUR D'AIDE DU PIED : le « (?) » et la liste des gestes de la fenêtre.
//
// ⚠ MÊMES PRÉCAUTIONS QUE `FooterToggle` — coordonnées absolues, curseur de mise
// en page rendu intact — et pour la même raison : le pied se dessine hors du
// flux, et un élément laissé là ferait croire à la fenêtre qu'elle déborde.
//
// `left_middle` est son bord GAUCHE et son milieu VERTICAL. La largeur occupée
// revient par `width`.
void FooterHelp(const char* title, std::span<const char* const> lines, ImVec2 left_middle);

// La place que le marqueur prendra. ⚠ SE DEMANDE AVANT DE POSER, comme pour les
// interrupteurs : un pied se remplit depuis la DROITE, et il faut connaître la
// largeur d'un élément pour savoir où commence son bord gauche.
[[nodiscard]] float FooterHelpWidth();

// Les tuiles sous la teinte de la grille ; sans tuile, un aplat du corps.
void DrawTiledBackground(ImDrawList* into, const SkinImage& tile, ImVec2 origin, ImVec2 top_left,
                         ImVec2 bottom_right, const SkinLook& look);

void DrawBadge(ImDrawList* into, const InventoryCellView& cell, ImVec2 top_left,
               ImVec2 bottom_right);

// UNE CASE SE TIRE : c'est la source du glisser.
//
// ⚠ À POSER JUSTE APRÈS L'ÉLÉMENT DE LA CASE, et AVANT son infobulle : ImGui
// rattache la source au DERNIER objet soumis, et une infobulle est une fenêtre.
//
// 🔴 UNE SEULE SOURCE PAR CASE, et celle-ci sert à TOUT ce qu'on tire d'un
// objet : une case de raccourci, un onglet, un autre contenant, le mannequin, le
// sol. ImGui n'en honore qu'une par élément — en déclarer une seconde pour un
// nouveau geste ferait taire la première.
//
// ⚠ `padding` EST CELUI RELEVÉ AVANT LES POUSSÉES DE LA GRILLE : le fantôme est
// une fenêtre, et il hériterait sinon de la marge NULLE qui rend les tuiles
// jointives.
//
// ⚠ `refusal` EST DIT DANS LE FANTÔME, en rouge, sous le nom. C'est le SEUL
// moment où le joueur peut encore renoncer : une fois lâché, il ne reste qu'un
// refus muet. `nullptr` quand rien ne s'y oppose.
//
// Le rouge d'un refus annoncé, dans le fantôme comme dans un bandeau.
inline constexpr ImVec4 kDragRefusalColor(0.85F, 0.15F, 0.15F, 1.0F);
void DragItemFrom(const InventoryCellView& cell, ItemHolder mine, ImVec2 padding,
                  const char* refusal);

// LA SOURIS EST-ELLE SUR CETTE FENÊTRE-LÀ ?
//
// ⚠ POUR QUE LA SOURCE D'UN GLISSER SACHE OÙ IL ABOUTIRAIT. Une cible ne peut
// pas le dire : elle ne parle qu'au lâcher, et le fantôme doit prévenir AVANT.
//
// `window_id` est l'identité d'une fenêtre — ce qui suit `###` —, jamais son
// titre : un titre traduit désignerait une fenêtre différente d'une langue à
// l'autre.
[[nodiscard]] bool MouseOverWindow(const char* window_id);

// LA CIBLE D'UN LÂCHER D'OBJET, sur le DERNIER élément soumis — l'enfant qui
// porte la grille, ou la table du coffre, une fois refermés.
//
// 🔴 SUR LA ZONE ENTIÈRE ET NON SUR UNE CASE : on ne range pas un objet à une
// place choisie, le serveur décide où il retombe. Une cible posée sur toute la
// zone laisse aux cases leur survol, leurs clics et leurs menus — ce qu'un grand
// bouton invisible par-dessus leur prendrait.
//
// ⚠ ELLE NE REND QUELQUE CHOSE QUE SI LA CHARGE VIENT D'AILLEURS : un objet
// lâché dans le contenant d'où il sort ne va nulle part, et un paquet qui ne
// change rien se voit dans le journal du serveur.
//
// ⚠ SAUF SUR UNE CASE, placement libre en cours : là, un objet lâché chez soi
// veut dire « pose-le ICI », et c'est le seul geste qui ne parle pas au serveur.
// `same` ouvre cette porte, et il est faux partout ailleurs.
[[nodiscard]] ItemTransferAsk AcceptItemDrop(ItemHolder mine, bool same = false);

// L'INFOBULLE D'UNE CASE : son nom, et sa quantité s'il y en a plus d'un.
//
// 🔴 SA MARGE SE PASSE, elle ne se prend pas au style. Une infobulle est une
// FENÊTRE, et elle hérite du `WindowPadding` en vigueur au moment où elle
// s'ouvre — or la grille l'a mis à ZÉRO pour rendre ses tuiles jointives. Le
// texte se collerait donc au cadre.
//
// ⚠ `padding` EST DONC CELUI RELEVÉ AVANT LES POUSSÉES DE LA GRILLE, le même
// que le menu contextuel reçoit et pour la même raison.
void DrawCellTooltip(const InventoryCellView& cell, ImVec2 padding);

void DrawIcon(ImDrawList* into, const InventoryCellView& cell, ImVec2 top_left, float cell_side);

// COMMENT LA BANDE EST DISPOSÉE, et de quoi ses onglets sont faits.
//
// 🔴 LES DEUX SONT DES RÉGLAGES DU JOUEUR, et c'est pour cela qu'ils voyagent
// ensemble jusqu'ici : une fenêtre qui choisirait sa disposition en dur
// obligerait à écrire deux fois chaque geste — le clic, l'infobulle, le lâcher.
struct TabStripLook {
  // COUCHÉE : une rangée AU-DESSUS de ce qu'elle ouvre, avec le second jeu
  // d'images que l'installation livre (`tabh_*`). Debout : une colonne à gauche.
  bool lying = false;
  // LES TUILES DU JEU, ou le libellé écrit. ⚠ LE TEXTE RESTE DROIT dans les deux
  // dispositions : ImGui ne sait pas tourner un texte, et le client, lui, écrit
  // ses libellés verticaux à 90°. L'écart se voit, et il est préférable à un
  // atlas tourné entretenu pour six mots.
  bool images = true;
  // 🔴 LA BANDE ACCEPTE-T-ELLE QU'ON LÂCHE UN OBJET DESSUS ? Seul le sac sait
  // quoi en faire — ranger dans les favoris, ou les en sortir. Une bande qui
  // prend la charge sans savoir la ranger la VOLE à la zone qui, elle, le
  // sait : l'objet lâché près d'un onglet du coffre n'y entrerait jamais, et
  // rien ne le dirait.
  bool takes_drops = false;
  // 🔴 L'ONGLET DES FAVORIS PORTE-T-IL LE FOND DE SA GRILLE ?
  //
  // L'installation livre DEUX cœurs : `tab_fav`, posé sur le gris-bleu de la
  // grille des favoris, et `tab_favs`, sur du blanc. Le sac prend le premier —
  // sa grille change vraiment de fond sous cet onglet. Le coffre prend le
  // second : sa table ne change pas, et le gris-bleu y ferait une tache.
  //
  // ⚠ LE PONT DE L'ONGLET ACTIF SUIT : il prolonge le corps de la tuile, et un
  // pont gris-bleu sous un cœur blanc se verrait comme un défaut.
  bool favorite_tinted = true;
};

// L'ÉPAISSEUR DE LA BANDE : sa largeur si elle est debout, sa hauteur si elle
// est couchée. C'est l'axe IMPOSÉ — l'autre se déduit, onglet par onglet, du
// rapport de l'image ou de la longueur du libellé.
[[nodiscard]] float ItemTabsThickness(const SkinArt& art, std::span<const ItemTab> tabs,
                                      TabStripLook look);

// La bande d'onglets, ANCRÉE AU COIN HAUT-GAUCHE DE LA GRILLE. Rend l'onglet
// choisi, ou celui qui l'était.
//
// ⚠ LA LISTE DES ONGLETS EST DONNÉE, elle n'est pas déduite : la charrette en
// montre six là où le sac en montre sept, et c'est la seule différence entre
// leurs deux bandes.
// Ce que la bande rend : l'onglet choisi, et OÙ l'onglet actif a été dessiné.
//
// ⚠ LE RECTANGLE SERT AU PONT. L'onglet actif mange le bord qui le sépare de la
// grille, et pour cela il faut savoir où il est — une bande qui ne rendrait que
// le choix obligerait l'appelant à refaire sa disposition pour le deviner.
struct TabStripResult {
  ItemTab chosen = ItemTab::kAll;
  ImVec2 active_min{};
  ImVec2 active_max{};
  bool active_shown = false;

  // ── CE QU'ON VIENT DE LÂCHER SUR UN ONGLET ────────────────────────────────
  //
  // 🔴 LÂCHER SUR UN ONGLET EST UN RANGEMENT, pas une sélection. C'est le geste
  // du client : on tire un objet sur « Favoris » pour l'y mettre, et on l'en
  // tire pour l'en sortir. La bande ne sait pas ce qu'est un favori — elle dit
  // la place et l'onglet, et l'appelant décide.
  //
  // ⚠ ET L'ONGLET N'EST PAS CHOISI POUR AUTANT : on range, on ne navigue pas.
  std::uint16_t dropped_slot = 0;
  ItemTab dropped_on = ItemTab::kAll;

  // ⚠ LE PONT A BESOIN DE SAVOIR QUEL BORD MANGER, et la bande est seule à le
  // savoir. Le lui redemander à l'appelant ferait deux vérités à tenir
  // d'accord, et la seconde se lirait « l'onglet actif paraît fermé ».
  bool lying = false;
  // ⚠ ET QUELLE COULEUR LUI DONNER : voir `TabStripLook::favorite_tinted`. Le
  // pont se dessine hors de la bande, qui seule sait de quelle tuile il part.
  bool favorite_tinted = true;
};

// ⚠ `length` NE VAUT QUE POUR LA BANDE DEBOUT : c'est la hauteur de son enfant,
// celle de la grille qu'elle longe. La rangée couchée, elle, tient sur une ligne
// du flux et se fabrique sa hauteur à partir de `thickness`.
[[nodiscard]] TabStripResult DrawItemTabs(std::span<const ItemTab> tabs, ItemTab current,
                                          const SkinArt& art, TabStripLook look, float thickness,
                                          float length);

// LE PONT DE L'ONGLET ACTIF : il mange le bord qui le sépare de ce qu'il ouvre.
//
// ⚠ IL NE SE POSE QUE SUR CE QUI A UNE BORDURE — la grille du sac et celle de la
// charrette. La table du coffre n'en a pas de ce côté-là : un pont y laisserait
// un rectangle clair posé sur l'en-tête.
//
// 🔴 SANS LUI, L'ONGLET ACTIF EST FERMÉ COMME LES AUTRES. Son art a le bord
// ouvert du côté du contenu, mais la bordure de la grille repasse par-dessus :
// un trait d'un pixel suffit à rendre les sept onglets identiques, et le joueur
// ne voit plus lequel il a ouvert.
//
// ⚠ LA COULEUR SE DÉDUIT DE L'ONGLET, et non de l'appelant : c'est un fait de
// son IMAGE, et deux fenêtres qui la choisiraient chacune finiraient par ne plus
// choisir pareil. La teinte des onglets s'y pose comme sur l'image.
void DrawActiveTabBridge(ImDrawList* into, const TabStripResult& strip, const SkinLook& look);

// CE QUE LE SERVEUR REFUSE PENDANT QU'UN STORAGE EST OUVERT, posé sur l'entrée
// grisée qui précède.
//
// 🔴 UN LIBELLÉ GRISÉ NE DIT PAS POURQUOI. La règle est invisible — le serveur
// jette le paquet sans un mot — donc elle se lit ici, avec la MANŒUVRE qui la
// contourne : faire transiter l'objet par le storage. Un « impossible » sans
// issue vaut à peine mieux qu'un clic sans effet.
//
// ⚠ UNE FONCTION ET NON UNE CONSTANTE, pour que le texte reste un littéral
// passé à `Tr` : le relevé des traductions ne lit que ceux-là, et il a raison de
// n'en lire que ceux-là. Le mot vit donc à UN endroit, avec la règle qu'il dit.
void StorageBlocksTransferTooltip();

// Ce que le joueur a répondu. `slot` nul tant qu'il n'a pas validé.
struct QuantityAnswer {
  std::uint16_t slot = 0;
  int amount = 0;
};

// L'INVITE DE QUANTITÉ, partagée par les fenêtres qui déplacent des piles.
//
// ⚠ LE VERBE EST DONNÉ PAR L'APPELANT — « Jeter » pour le sac, « Retirer » pour
// la charrette — parce que c'est la seule chose qui change. Un bouton nommé
// « Valider » ne dirait pas ce qu'on s'apprête à faire d'irréversible.
//
// ⚠ ET LA PLACE SE REVÉRIFIE À CHAQUE IMAGE : le temps de répondre, l'objet a pu
// partir. La question se referme alors d'elle-même plutôt que de porter sur une
// case que rien n'occupe.
[[nodiscard]] QuantityAnswer DrawQuantityPrompt(std::span<const InventoryCellView> cells,
                                                ItemQuantityAsk& ask, const char* confirm,
                                                const SkinArt& art);

// LA MÊME INVITE, SANS PLACE : une borne et un nombre. C'est elle que l'invite
// d'une case appelle une fois la case retrouvée, et c'est elle que prend une
// fenêtre dont les piles ne sont pas des places de sac — l'album de cartes.
//
// ⚠ `amount` EST L'ÉTAT DE LA SAISIE, gardé par l'appelant d'une image à
// l'autre ; le prompt le pose à `most` à son apparition.
struct AmountAnswer {
  // Le joueur a validé : `amount` est sa réponse.
  bool chosen = false;
  int amount = 0;
  // La question est close — validée ou abandonnée.
  bool closed = false;
};
[[nodiscard]] AmountAnswer DrawCountPrompt(int& amount, int most, const char* confirm,
                                           const SkinArt& art);

// CE QUE L'ASCENSEUR PREND À LA GRILLE, en pixels, zéro quand il ne paraît pas.
//
// 🔴 SA GOUTTIÈRE FAIT PARTIE DU CHROME, au même titre que la bande d'onglets
// ou le pied : c'est de la largeur que les cases n'auront pas. Comptée dans la
// grille, le calage taille la fenêtre sur des cases dont l'ascenseur reprend
// ensuite quatorze pixels — et il reste une bande de fond pavé trop étroite
// pour une case, contre laquelle rien ne peut jamais être posé.
//
// ⚠ À APPELER DEDANS, entre `BeginChild` et `EndChild`.
[[nodiscard]] float GridScrollbarGutter();

// Les drapeaux de l'enfant « grille ».
//
// 🔴 TAILLE VERROUILLÉE, LA GOUTTIÈRE EST TOUJOURS RÉSERVÉE. Le calage court à
// chaque image, verrou ou non — `NoResize` ne retire que la poignée —, et une
// gouttière qui paraît au premier objet qui déborde élargirait la fenêtre de sa largeur : elle
// quitterait le bord d'écran où le joueur l'a collée. Réservée, elle laisse le
// chrome constant, et la fenêtre ne bouge qu'une fois, au verrouillage.
//
// ⚠ DÉVERROUILLÉE, ELLE NE PARAÎT QUE SI ELLE SERT : le joueur retaille alors
// lui-même, et une gouttière vide lui volerait une colonne pour rien.
[[nodiscard]] ImGuiWindowFlags GridScrollFlags(bool size_locked);

// Cale la taille demandée sur un nombre ENTIER de cases, dans les deux sens.
//
// ⚠ SON `UserData` EST UN `const ImVec2*` : le chrome de la fenêtre, mesuré à
// l'image précédente. Chaque fenêtre garde le sien.
void SnapToCells(ImGuiSizeCallbackData* asked);

}  // namespace ro::ui
