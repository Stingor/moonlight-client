#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES WIDGETS DU CLIENT, habillés de ses propres pièces.
//
// Ce sont des BRIQUES : elles ne connaissent aucune fenêtre, et toute fenêtre
// les emploie. C'est ici que le portage des fenêtres de jeu viendra puiser.
//
// Sans art, chacune se rabat sur le widget ordinaire d'ImGui. Une installation
// à qui il manque des bitmaps reste jouable, partout et de la même façon.

// Ce qu'un bouton ajoute autour de son libellé, en pixels d'art.
//
// ⚠ C'EST DU CONFORT, ET IL NE SE DÉCOMPTE PAS DE LA PLACE DU TEXTE. L'ôter au
// libellé rapetisserait des mots qui tenaient très bien.
inline constexpr float kButtonPaddingPixels = 12.0F;

// Ce qui sépare le libellé de l'art, pour qu'il n'y colle pas.
inline constexpr float kButtonTextMarginPixels = 2.0F;

// De combien le libellé s'enfonce avec l'art qu'on presse.
inline constexpr float kButtonSinkPixels = 1.0F;

// Le décalage du FAUX GRAS : le même texte, repassé un pixel plus loin. La
// police n'a qu'une graisse chargée.
inline constexpr float kFakeBoldPixels = 1.0F;

// La largeur qu'occuperait un bouton en taille automatique, DANS LA LANGUE
// COURANTE. De quoi dimensionner un conteneur sur ses libellés traduits — le
// même bouton n'a pas la même largeur en français et en anglais.
//
// À appeler pendant une image : la mesure dépend de la police en cours.
[[nodiscard]] float SkinButtonWidth(const char* label, const SkinArt& art);
[[nodiscard]] float SkinSmallButtonWidth(const char* label, const SkinArt& art);

// La plus grande de plusieurs largeurs, DANS LA LANGUE COURANTE.
//
// ⚠ C'EST CE QUI ÉVITE UNE MISE EN PAGE CALÉE SUR LE FRANÇAIS. Une colonne
// dimensionnée sur « Panier » laisse sortir « Shopping cart » du cadre ; la
// mesurer sur les libellés traduits la fait suivre la langue.
[[nodiscard]] float SkinWidestButton(std::span<const char* const> labels, const SkinArt& art);

// LA HAUTEUR D'UN BOUTON HABILLÉ, celle de son ART — et non celle d'un cadre
// d'ImGui, qui est plus basse.
//
// ⚠ C'EST ELLE QU'IL FAUT RÉSERVER pour un pied de fenêtre. Réserver
// `GetFrameHeightWithSpacing()` laisse le bouton déborder de quelques pixels,
// ce qui suffit à faire apparaître un ascenseur — et l'ascenseur d'une fenêtre
// habillée passe PAR-DESSUS la barre de titre qu'on a peinte, donc par-dessus
// son bouton de fermeture.
[[nodiscard]] float SkinButtonHeight(const SkinArt& art);
[[nodiscard]] float SkinSmallButtonHeight(const SkinArt& art);

// L'ORDONNÉE OÙ POSER LE CURSEUR pour qu'un PETIT bouton ait son ART centré sur
// une ligne — celle d'une barre de titre, celle d'un pied.
//
// 🔴 CENTRER L'ITEM N'EST PAS CENTRER LE BOUTON. L'item d'un petit bouton est
// plus haut que son art de la CHUTE réservée au libellé qui dépasse par le haut
// (voir `SkinSmallButtonHeight`), et cette chute est du VIDE : centrer l'item
// descend donc l'art de la moitié d'une chute, et dans une barre de titre —
// haute de dix-sept pixels pour un art de quatorze — il en sort par le bas.
//
// ⚠ À APPELER PENDANT UNE IMAGE : la mesure dépend de l'échelle en cours.
[[nodiscard]] float SkinSmallButtonTopCenteredOn(const SkinArt& art, float middle_y);

// La place qu'un pied de fenêtre doit réserver : le bouton et son interligne.
[[nodiscard]] float SkinFooterHeight(const SkinArt& art);

// Un bouton. `size` à zéro sur un axe prend la taille automatique : le libellé
// plus ses marges en largeur, la hauteur de l'art en hauteur.
//
// ⚠ `size.y` EST LA PLACE PRISE, celle que rendent les deux mesures ci-dessus,
// et non la hauteur de l'art : sur un PETIT bouton, l'art occupe ce qui reste
// une fois la chute retranchée.
//
// 🔴 UNE LARGEUR IMPOSÉE TROP COURTE RAPETISSE LE LIBELLÉ, puis le coupe — il
// ne déborde jamais. Un bouton du jeu écrit son texte à la main dans la liste
// d'affichage, qu'ImGui ne clippe pas : un libellé trop long baverait sur ce
// qui se trouve à côté. C'est ce que fait le client, et le plancher est bas —
// un mot rapetissé dit encore ce que fait le bouton, « Tout dép… » non.
//
// ⚠ CELA NE DISPENSE PAS DE MESURER : là où le conteneur peut respirer,
// `SkinButtonWidth` vaut mieux que d'imposer une largeur et de compter sur le
// rapetissement.
[[nodiscard]] bool SkinButton(const char* label, const SkinArt& art, ImVec2 size = ImVec2(0, 0));

// LE MÊME BOUTON, EN PETIT. Pour une ligne qui doit partager sa place : deux
// boutons pleine taille mangent une largeur que la fenêtre n'a pas toujours.
[[nodiscard]] bool SkinSmallButton(const char* label, const SkinArt& art,
                                   ImVec2 size = ImVec2(0, 0));

// UN BOUTON D'ÉTAT : l'outil courant, l'onglet ouvert, l'option retenue.
//
// ⚠ IL S'ENFONCE ET SON LIBELLÉ PASSE EN GRAS, les deux ensemble. L'art
// « pressé » seul se confond avec le survol : un onglet actif paraîtrait
// simplement survolé, et le joueur ne saurait plus lequel il regarde.
//
// L'ÉTAT APPARTIENT À L'APPELANT — la fonction rend vrai au clic, comme un
// bouton ordinaire, et ne retient rien. C'est la même règle que partout dans
// cette couche : une brique qui garderait l'état le garderait pour tout le monde.
[[nodiscard]] bool SkinSmallToggleButton(const char* label, bool active, const SkinArt& art,
                                         ImVec2 size = ImVec2(0, 0));

[[nodiscard]] bool SkinToggleButton(const char* label, bool active, const SkinArt& art,
                                    ImVec2 size = ImVec2(0, 0));

// UNE CASE À COCHER. Rend vrai quand elle vient de changer, et écrit alors dans
// `ticked` — la convention d'ImGui, pour qu'un appelant puisse enregistrer au
// moment où cela bouge.
[[nodiscard]] bool SkinCheckBox(const char* label, bool* ticked, const SkinArt& art);

// La largeur qu'occupe cette case DANS LA LANGUE COURANTE — même raison que
// pour les boutons : une contrainte de taille calée sur le français laisse
// sortir un libellé anglais du cadre.
[[nodiscard]] float SkinCheckBoxWidth(const char* label, const SkinArt& art);

// LE BOUTON RADIO du jeu. Dessine l'image et son libellé, tous deux cliquables,
// et rend vrai au clic : c'est à l'appelant de poser la valeur — un radio ne
// sait pas ce que ses frères valent.
[[nodiscard]] bool SkinRadio(const char* label, bool chosen, const SkinArt& art);

// LE CURSEUR DE RÉGLAGE — QUI EST L'ASCENSEUR HORIZONTAL DU JEU.
//
// ⚠ RAGNAROK N'A PAS DE « SLIDER ». Son équivalent visuel est la barre de
// défilement horizontale, flèches comprises, et l'on reprend ses pièces telles
// quelles plutôt que d'inventer une forme que le jeu n'a pas.
//
// Le geste reste celui d'ImGui : glisser la poignée, et Ctrl+clic pour taper la
// valeur — ce qui rend atteignables des valeurs qu'aucun pixel de piste ne
// désigne. Les FLÈCHES, elles, sont à nous : un clic vaut un pas, le maintien
// répète, Majuscule multiplie par dix.
//
// `arrow_step` à zéro prend l'unité : un pour un entier, un centième pour un
// flottant.
// L'INFOBULLE DU DERNIER WIDGET SOUMIS.
//
// ⚠ LE SURVOL SE DEMANDE À IMGUI, pas à un rectangle qu'on retiendrait :
// `IsItemHovered` connaît les fenêtres qui recouvrent, l'item actif ailleurs,
// et le délai que le style impose. Un test maison redirait tout cela, moins
// bien, à chaque endroit qui montre une infobulle.
//
// ⚠ UN WIDGET GRISÉ PARLE AUSSI : c'est souvent son infobulle qui dit POURQUOI
// il est grisé, et ImGui ne le tient pour survolé que si on le lui demande.
//
// Le texte est déjà traduit quand il arrive ici : il est POSÉ tel quel, sans
// passer par un gabarit, pour qu'un pourcent dans une traduction ne devienne
// pas une conversion.
void SkinTooltip(const char* text);

// LE MARQUEUR D'AIDE : un « (?) » grisé, et sa bulle au survol.
//
// 🔴 IL PREND SES LIGNES SÉPARÉMENT, ET C'EST LE CATALOGUE QUI L'EXIGE : une
// entrée de traduction est UNE ligne, et un pavé de dix gestes écrit d'un bloc
// n'entrerait dans aucun catalogue. Chaque ligne se déclare donc avec
// `core::TrText` là où on la lit, et se traduit ici.
//
// ⚠ ELLES NE SE RECOLLENT QU'AU SURVOL : le pavé se compose à la demande, et
// une fenêtre qu'on ne survole pas ne paie rien.
//
// ⚠ ET LE TEXTE S'ENROULE : la liste des gestes est longue, et une bulle qui
// traverse l'écran se lit moins bien que trois lignes de plus.
//
// ⚠ LE TITRE EST À PART, et c'est ce qui pose la ligne VIDE qui le sépare de la
// liste : une entrée de catalogue ne peut pas porter de saut de ligne, donc le
// blanc ne peut pas vivre dans le texte.
void SkinHelpMarker(const char* title, std::span<const char* const> lines);

// LA MOLETTE SUR UN CURSEUR. Par défaut il la prend au survol, et un cran vaut
// un cran de flèche. `kLeaves` la laisse à la fenêtre : c'est le choix d'un
// curseur dont chaque changement part au serveur, où un cran effleuré en
// faisant défiler la page enverrait une commande.
enum class SkinSliderWheel : std::uint8_t { kTakes, kLeaves };

[[nodiscard]] bool SkinSliderFloat(const char* label, float* value, float lowest, float highest,
                                   const SkinArt& art, const char* format = "%.2f",
                                   float arrow_step = 0.0F);
[[nodiscard]] bool SkinSliderInt(const char* label, int* value, int lowest, int highest,
                                 const SkinArt& art, const char* format = "%d", int arrow_step = 0,
                                 SkinSliderWheel wheel = SkinSliderWheel::kTakes);

// LE CHAMP À PAS : un « - » habillé, le champ où l'on tape, un « + » habillé,
// puis le libellé à droite. Ctrl tenu pendant le clic avance de `fast_step`
// (zéro : pas de pas rapide), et un bouton tenu répète.
//
// ⚠ LES FLÈCHES DU CHAMP À PAS D'IMGUI NE S'HABILLENT PAS : deux carrés bleus
// au milieu d'une fenêtre du jeu. C'est pourquoi le champ est ici sans pas et
// les deux boutons sont les nôtres.
//
// ⚠ LA LARGEUR DE L'ITEM COURANT EST CELLE DE L'ENSEMBLE, boutons compris,
// comme pour un curseur : leurs libellés s'alignent dans une même liste.
//
// La valeur est bornée à [`lowest`, `highest`], et le retour dit qu'elle a
// effectivement changé.
[[nodiscard]] bool SkinInputInt(const char* label, int* value, int lowest, int highest, int step,
                                int fast_step, const SkinArt& art);
[[nodiscard]] bool SkinInputFloat(const char* label, float* value, float lowest, float highest,
                                  float step, float fast_step, const SkinArt& art,
                                  const char* format = "%.1f");

// LA LISTE DÉROULANTE : un champ bordé et le bouton flèche du client.
//
// S'emploie comme celle d'ImGui — l'appelant pose ses `Selectable` entre les
// deux. ⚠ `EndSkinCombo` NE S'APPELLE QUE SI L'OUVERTURE A RENDU VRAI : elle
// dépile ce que l'ouverture a posé.
//
// 🔴 CE QU'ELLE PREND AUTOUR DE SON TEXTE SE LIT ICI, ET NULLE PART
// AILLEURS. Une disposition qui veut réserver sa place AVANT de la dessiner
// doit poser le même nombre que `BeginSkinCombo` — deux règles qui se
// répondent de loin finissent par se contredire, et le libellé se fait
// découper.
//
// LA MOLETTE AU-DESSUS DE LA LISTE FERMÉE en fait défiler les entrées, une par
// cran : `wheel_step` reçoit −1 pour l'entrée d'avant (molette vers le haut),
// +1 pour celle d'après, zéro sinon — c'est à l'appelant de choisir l'entrée,
// lui seul connaît sa liste. Les gardes de `WheelOverItem` : parcourir une page
// à la molette ne règle rien au passage. Sans `wheel_step`, la liste laisse la
// molette à la fenêtre : c'est le choix d'une liste où un cran de trop COÛTE.
[[nodiscard]] float SkinComboChromeWidth(const SkinArt& art);
[[nodiscard]] bool BeginSkinCombo(const char* label, const char* shown, const SkinArt& art,
                                  int* wheel_step = nullptr);
void EndSkinCombo();

// LE RANG QUE VISE UN CRAN dans une liste de `count` entrées : le voisin, borné
// aux deux bouts — la liste ne boucle pas. Sans rang choisi (négatif), le
// premier cran, dans un sens comme dans l'autre, vise la première entrée.
[[nodiscard]] int SkinComboStepped(int rank, int step, int count);

// LA LISTE TOUTE FAITE : des libellés et l'index choisi. La molette la fait
// défiler.
//
// ⚠ REND VRAI SUR UN CHANGEMENT EFFECTIF, et une seule image — pas tant que la
// liste est ouverte, et pas si l'on reclique l'entrée déjà choisie. C'est ce qui
// permet d'écrire `if (SkinCombo(...)) { Enregistrer(); }` sans réécrire son
// fichier de réglages soixante fois par seconde.
//
// ⚠ LES LIBELLÉS SE MARQUENT PAR `core::TrText`, ET SE PASSENT EN FRANÇAIS. La
// traduction se fait à la LECTURE, ici : une table de libellés est presque
// toujours statique, et la traduire à sa définition l'évaluerait avant le
// catalogue — elle resterait française pour toujours. Le marqueur est ce qui la
// fait entrer au catalogue malgré tout.
//
// `closed_text`, s'il est donné, remplace l'entrée choisie dans la liste
// FERMÉE — déjà traduit ; les entrées de la liste ouverte restent les libellés.
[[nodiscard]] bool SkinCombo(const char* label, int* chosen, std::span<const char* const> items,
                             const SkinArt& art, const char* closed_text = nullptr);

// OÙ POSER LE HAUT D'UN TEXTE dans une rangée, pour qu'il y soit centré.
//
// 🔴 `row_height` EST CELLE DE LA RANGÉE, PAS CELLE DE LA PIÈCE D'À
// CÔTÉ, et c'est tout l'objet de cette fonction. Une case du client fait onze
// pixels, moins qu'une ligne de texte : centrer le libellé sur LA CASE le pose
// au-dessus du haut de rangée, et tout ce qu'un `SameLine` ajoute ensuite — un
// marqueur d'aide, par exemple — paraît décalé SOUS lui. La rangée fait la
// hauteur du PLUS GRAND des deux, et c'est elle la référence.
//
// Arrondi à l'entier : un demi-pixel fait flotter le texte entre deux lignes.
[[nodiscard]] float RowTextTop(float row_top, float row_height, float text_height);

// LE STYLE RESSERRÉ d'un panneau dense en réglages : marges et écarts
// VERTICAUX réduits, les horizontaux laissés tels quels.
//
// ⚠ TOUJOURS PAR PAIRE, et la paire ne saute pas une sortie anticipée : une
// variable de style restée empilée ne se voit pas sur le panneau fautif, elle
// déteint sur tout ce que l'image dessine ENSUITE — donc loin de sa cause.
void PushCompactStyle();
void PopCompactStyle();

// LE MARQUEUR D'AIDE : un « (?) » grisé, et sa bulle au survol.
//
// ⚠ IL SE VOIT, ET C'EST TOUT SON OBJET. Une aide accrochée au libellé seul
// n'existe que pour qui pense à y passer le curseur : personne ne survole un
// mot pour voir s'il cache quelque chose.
//
// ⚠ SA BULLE RÉPOND AUSSI SOUS `BeginDisabled` : sur une rangée grisée, c'est
// souvent elle qui dit pourquoi.
void SkinHelpMarker(const char* help);
// LE MÊME, DONT LA BULLE MET EN GARDE : `warning` y vient en dernier, derrière
// le signe peint — voir `SkinTooltipWarned`. Nul, c'est le marqueur ordinaire.
// Sans aide, la mise en garde a son marqueur seule ; sans l'une ni l'autre,
// rien ne se dessine.
void SkinHelpMarker(const char* help, const char* warning);

// Le côté de la pastille, en pixels d'art.
inline constexpr float kSwatchSidePixels = 20.0F;

// CE QU'UN NUANCIER REND : la couleur a-t-elle bougé, et est-il déroulé ?
//
// 🔴 LES DEUX COMPTENT, ET LE SECOND EST UN PIÈGE RÉEL. Le sélecteur
// s'ouvre SOUS la pastille et recouvre la suite de la fenêtre ; l'appelant
// qui pose des curseurs en dessous doit les neutraliser pendant ce temps,
// sans quoi viser le nuancier revient à attraper une poignée restée dessous.
struct SkinColorPick {
  bool changed = false;
  bool open = false;
};

// LE NUANCIER : une pastille, son libellé, et le sélecteur qui s'ouvre dessous.
//
// ⚠ REND VRAI À CHAQUE IMAGE D'UN GLISSEMENT dans le sélecteur, comme tout
// widget continu d'ImGui : l'appelant qui écrit un fichier passe par une
// attente, il n'en sauve pas soixante par seconde.
//
// ⚠ `with_alpha` À FAUX RETIRE LA BARRE D'OPACITÉ ET LA COMPOSANTE A. À
// employer dès que la couleur choisie n'a pas de transparence à régler — un
// curseur qui ne commande rien est pire qu'absent, il invite à le bouger.
//
// ⚠ `numbers` À FAUX NE LAISSE QUE LE NUANCIER : ni R/V/B, ni T/S/V, ni
// hexadécimal. C'est le bon défaut dès que le nombre ne veut rien dire pour
// celui qui choisit ; un fond de chat, lui, se recopie d'un thème à l'autre,
// et là l'hexadécimal sert.
//
// ⚠ LA COULEUR EST UN `ImU32` et non quatre flottants : c'est la forme que
// tout le dépôt emploie, fichiers compris. La convertir à chaque appelant
// donnerait autant d'occasions d'intervertir deux canaux.
[[nodiscard]] SkinColorPick SkinColorSwatch(const char* label, ImU32* color, const SkinArt& art,
                                            bool with_alpha = true, bool numbers = true);

// DE COMBIEN LA POIGNÉE RENTRE DANS LE COIN.
//
// 🔴 ELLE NE SE POSE PAS AU RAS DU BORD. Le cadre d'une fenêtre du jeu est un
// neuf-pièces : ses bords ont une épaisseur, et une poignée collée au coin se
// pose PAR-DESSUS eux — elle déborde du cadre au lieu d'y entrer, et le coin
// arrondi passe derrière elle.
//
// ⚠ DEUX MESURES, PARCE QU'IL Y A DEUX CADRES. Celui d'une fenêtre ordinaire est
// mince ; celui d'une description est une boîte `sysbox`, plus épaisse, et deux
// pixels n'y suffisent pas. Ce sont les deux valeurs du client.
inline constexpr float kResizeGripInsetPixels = 2.0F;
inline constexpr float kDescriptionResizeGripInsetPixels = 4.0F;

// LA POIGNÉE DE REDIMENSIONNEMENT de la fenêtre courante, habillée.
//
// Même règle que l'ascenseur : c'est l'IMAGE qu'on remplace, ImGui garde le
// geste. À appeler entre le `Begin` et le `End`.
//
// ⚠ L'IMAGE RENTRE, LE GESTE NON : ImGui garde sa zone de saisie au ras du coin.
// C'est ce que fait le client, et c'est le bon compromis — la poignée se voit à
// sa place, et le geste reste là où la souris va le chercher.
void PaintWindowResizeGrip(const SkinArt& art, float inset_pixels = kResizeGripInsetPixels);

// LA BARRE D'ONGLETS DU JEU.
//
// ⚠ L'ART SE PEINT PAR-DESSUS CELUI D'IMGUI, il ne le remplace pas : on garde
// la mécanique entière — choix, survol, défilement, ordre — et l'on efface
// seulement ses fonds. Ce qui se voit est alors le nôtre, ce qui répond au clic
// est le sien.
//
// ⚠ `EndSkinTabBar` NE S'APPELLE QUE SI CELUI-CI A RENDU VRAI : il dépile les
// couleurs que l'ouverture a posées, et les dépiler deux fois — ou pas du tout —
// dérègle le style de tout ce qui suit dans l'image.
[[nodiscard]] bool BeginSkinTabBar(const char* id, const SkinArt& art, ImGuiTabBarFlags flags = 0);
void EndSkinTabBar();

// OÙ SE TIENT LA POIGNÉE d'un ascenseur vertical, dans une piste donnée.
//
// ⚠ CETTE FORMULE EST CELLE D'IMGUI, RECOPIÉE, et c'est ce qu'elle a de
// fragile : l'ascenseur qu'on PEINT doit tomber exactement là où celui qu'ImGui
// ÉCOUTE se trouve, sans quoi la poignée se prend à côté de là où on la voit.
// Elle est isolée ici, nommée, et éprouvée — pas dispersée dans le dessin —
// pour qu'une montée de version ait UN endroit à revérifier.
//
// `visible` est ce que la fenêtre montre, `content` tout ce qu'elle contient,
// `scroll` où elle en est. Rend un couple {début, fin} dans la piste, en
// pixels depuis son bord haut. Une piste entièrement visible rend la piste.
struct ScrollGrab {
  float start = 0.0F;
  float end = 0.0F;
};
[[nodiscard]] ScrollGrab VerticalScrollGrab(float track_height, float visible, float content,
                                            float scroll, float smallest_grab);

// L'ASCENSEUR VERTICAL de la fenêtre courante, habillé.
//
// ⚠ IL SE PEINT PAR-DESSUS CELUI D'IMGUI, qui garde tout le geste — clic,
// glisser, roulette, clic dans la piste. On ne remplace que l'IMAGE : refaire
// l'interaction rouvrirait une demi-douzaine de cas que la bibliothèque traite
// déjà, et qu'on ne redécouvrirait qu'en jouant.
//
// À appeler entre le `Begin` et le `End` de la fenêtre, après son contenu : la
// taille de ce qu'elle contient n'est connue qu'à ce moment.
void PaintWindowScrollbar(const SkinArt& art);

// CE QUE LA FENÊTRE COURANTE DONNE À `VerticalScrollGrab`, et c'est la moitié
// de la formule qu'ImGui ne laisse pas lire toute faite.
//
// 🔴 `content` N'EST PAS `ContentSize.y` : ImGui y ajoute DEUX marges de
// fenêtre, et son défilement maximal en découle. Y mettre autre chose — la
// hauteur visible, par exemple — donne une poignée plus courte que la vraie et
// une course qui n'atteint jamais le bas : on peint alors un ascenseur à côté
// de celui qui écoute le clic. L'invariant qui le garde est
// `content - visible == window->ScrollMax.y`, et il est éprouvé.
struct ScrollExtent {
  float visible = 0.0F;
  float content = 0.0F;
  float scroll = 0.0F;
};
[[nodiscard]] ScrollExtent WindowScrollExtent();

// LA PASTILLE ET SON NUANCIER : un carré de couleur, et le sélecteur qui
// s'ouvre dessous.
//
// 🔴 LE NUANCIER RECOUVRE CE QUI SUIT DANS LA FENÊTRE, et c'est inévitable :
// un popup s'ouvre sous l'item qui l'appelle. Le déplacer ailleurs
// supprimerait le recouvrement, mais un sélecteur qui surgit loin de sa
// pastille est déroutant. C'est donc à l'APPELANT de neutraliser ce qui passe
// dessous pendant ce temps — d'où `opened`, qui le lui dit. Sans cela, viser
// le nuancier revient à attraper ce qui est resté en dessous.
//
// ⚠ LE LIBELLÉ PEUT PORTER UN IDENTIFIANT CACHÉ (`##`) : seul ce qui précède
// s'affiche, comme le fait ImGui pour tous ses widgets.
//
// Rend vrai sur un changement de couleur.
[[nodiscard]] bool SkinColorSwatch(const char* label, std::array<float, 3>& rgb, const SkinArt& art,
                                   bool* opened = nullptr);

// UN LIEN : du texte bleu, souligné au survol, qui rend vrai quand on le clique.
//
// 🔴 IL N'OUVRE RIEN LUI-MÊME. `ui` ne parle pas au système : ouvrir un
// navigateur est un geste d'`app`, exactement comme pour une adresse trouvée
// dans la description d'un objet — et c'est là, pas ici, que l'on vérifie que
// l'adresse est bien du `http(s)` avant de la tendre au système.
//
// ⚠ SOULIGNÉ AU SURVOL SEULEMENT : un trait permanent accompagne le regard
// pendant toute la lecture, alors qu'on ne cherche à savoir « est-ce que ça se
// clique ? » qu'une fois le curseur dessus.
//
// ⚠ IL N'A PAS D'ART À LUI : c'est du texte, et le client d'origine n'a rien
// de tel à porter.
[[nodiscard]] bool SkinHyperlink(const char* label);

}  // namespace ro::ui
