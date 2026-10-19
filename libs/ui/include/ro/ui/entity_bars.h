#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <imgui.h>

namespace ro::ui {

// ── LES BARRES DE VIE ET DE MAGIE AU-DESSUS DES ENTITÉS ─────────────────────
//
// Ce que le client d'origine peint au-dessus — ou plutôt SOUS — chaque entité :
// la jauge d'un joueur et celle d'un monstre (alimentée par `ZC_HP_INFO
// 0x0977`). Deux jauges, une seule peinture : soixante pixels sur cinq, posée
// en `(x - 30, y + échelle(12))` — c'est-à-dire SOUS LES PIEDS.
//
// 🔴 CE QUE LA PEINTURE DU CLIENT DIT, ET QUI SE REPREND ICI :
//
//   · le cadre est la surface effacée, d'un pixel, puis le creux, puis la part
//     pleine — soit trois rectangles et non quatre bandes de bordure ;
//   · SOUS VINGT-CINQ POUR CENT, LA BARRE CHANGE DE COULEUR. C'est un `if` du
//     client (« au moins 25 % »), pas une invention : vert vif au-dessus,
//     rouge en dessous ;
//   · le mode DEUX BARRES existe déjà dans le client — la seconde, bleue, est
//     celle de la magie. Le client ne l'alimente jamais : le rendu était prêt,
//     le transport manquait.
//
// Les couleurs sont celles de la jauge du client, en COLORREF — donc
// `0x00BBGGRR`, et non l'inverse : vert `0x25EA16` plein, rouge `0x1616EA` en
// alerte, bleu `0xDB651D` pour la barre basse, creux `0x444444`, cadre
// `0x9B1914`.
//
// 🔴 `ui` NE SAIT RIEN DU MONDE. Elle reçoit un point d'écran, deux jauges et
// une ESPÈCE — des données pures, comme les bulles de parole. C'est `app` qui
// projette une tête en pixels et qui sait ce qu'est un membre du groupe.
//
// ⚠ SUR LA LISTE DE FOND, SOUS LES BULLES : une phrase ne doit jamais passer
// derrière une barre, et une barre jamais devant une fenêtre.

// L'ESPÈCE DONT ON RÈGLE LES BARRES — un onglet chacune.
//
// 🔴 CE DÉCOUPAGE EST CELUI DES DONNÉES, PAS UN GOÛT. Ce que le serveur nous
// dit de la vie d'autrui dépend exactement de qui c'est : un monstre l'annonce
// à tout le monde une fois entamé, un compagnon de groupe par un paquet à lui,
// un joueur quelconque pas du tout. Régler les quatre ensemble ferait promettre
// au joueur un affichage que le protocole ne peut pas tenir.
//
// ⚠ L'ORDRE PART DANS LE FICHIER DE RÉGLAGES : ne pas intercaler.
enum class EntityBarKind : std::uint8_t {
  // MOI. ⚠ LE CLIENT NE LA MONTRE PAS — on a ses jauges libres et sa fiche —,
  // donc elle est éteinte par défaut. Elle existe parce qu'un joueur qui pose
  // ses jauges en bas d'écran ne les regarde pas pendant qu'il vise.
  kOwn = 0,
  // MON GROUPE. Leur vie arrive par `ZC_NOTIFY_HP_TO_GROUPM 0x080E`, et c'est
  // la seule source : un allié hors groupe reste inconnu.
  kParty = 1,
  // TOUT AUTRE JOUEUR. ⚠ SA VIE N'EST CONNUE QUE S'IL EST LA CIBLE, par
  // `ZC 0x0F2A`, et la porte PVP du serveur peut encore la taire — sauf chez
  // le staff, à qui le serveur envoie la vie de toute la zone par le paquet du
  // groupe.
  kPlayer = 2,
  // LES MONSTRES. `ZC_HP_INFO 0x0977` une fois entamés, et le paquet
  // d'apparition lui-même quand le serveur le veut bien.
  kMonster = 3,
  // LES BOSS ET LES MVP. ⚠ UNE ESPÈCE À PART PARCE QU'ON LES REGARDE
  // AUTREMENT : une barre qu'on suit pendant dix minutes n'a pas la taille de
  // celle d'un mouton. Le paquet d'apparition porte le drapeau.
  kBoss = 4,
  // PNJ, familiers, homoncules, mercenaires, élémentaires — tout le reste.
  // Leur vie n'est connue que par le dossier de cible, quand le serveur la
  // donne.
  kOther = 5,
};
inline constexpr std::size_t kEntityBarKindCount = 6;

// QUAND UNE BARRE PARAÎT.
//
// ⚠ MÊME RÈGLE D'ORDRE : ces rangs partent dans le fichier de réglages.
// 🔴 SON TYPE SOUS-JACENT EST `int`, ET CE N'EST PAS UN DÉTAIL. Ce rang se
// déclare au fichier de réglages par un `reinterpret_cast<int*>` — c'est la
// seule forme entière que le fichier connaisse —, et la lecture écrit alors
// QUATRE octets. Sur un `std::uint8_t`, elle écraserait les trois champs
// suivants de la structure, en silence.
enum class EntityBarWhen : int {
  // Dès que la valeur est connue.
  kAlways = 0,
  // Seulement en dessous du maximum. ⚠ C'EST LE COMPORTEMENT DU CLIENT, et il
  // vient du serveur autant que de lui : rAthena n'envoie la vie d'un monstre
  // QU'UNE FOIS ENTAMÉ (`status_get_hp < status_get_max_hp`, dans
  // `clif_set_unit_idle`). Une barre pleine au-dessus de chaque mouton d'une
  // carte est ce que ce réglage évite.
  kDamaged = 1,
  // Seulement ce que la souris désigne.
  kHovered = 2,
  // Seulement la cible.
  kTarget = 3,
  // L'un ou l'autre : on vise du regard, ou on a verrouillé.
  kHoveredOrTarget = 4,
  kCount,
};

// CE QUE LE TEXTE D'UNE BARRE DIT.
//
// 🔴 UNE ÉNUMÉRATION À ELLE, bien que `StatBarText` porte les mêmes quatre
// valeurs. Les deux partent dans le MÊME fichier de réglages sous des clés
// différentes, et le jour où l'une gagne une entrée, la partager déplacerait
// les choix déjà faits de l'autre. C'est le raisonnement de `GaugeText`, dans
// le HUD de cible, et il a déjà servi.
// ⚠ EN `int` POUR LA MÊME RAISON QUE `EntityBarWhen` : il se persiste.
enum class EntityBarText : int {
  kNothing = 0,
  kPercent = 1,
  kValues = 2,
  kBoth = 3,
  kCount,
};

// Les rangs lus du fichier de réglages, bornés. ⚠ LA SOURCE N'EST PAS SOUS
// NOTRE CONTRÔLE — le joueur ouvre ce fichier dans un éditeur de texte — et un
// `static_cast` hors bornes sur une énumération est un comportement indéfini,
// pas une valeur bizarre.
[[nodiscard]] EntityBarWhen EntityBarWhenOf(int stored);
[[nodiscard]] EntityBarText EntityBarTextOf(int stored);

// UNE JAUGE D'ENTITÉ : ce que le serveur en a dit.
//
// ⚠ `known` FAUX N'EST PAS ZÉRO, et c'est tout le sujet de ces barres : une
// barre vide se lit « il est mort », alors que le silence du serveur est le cas
// ORDINAIRE. Voir `sim::kUnknownHealth`, qui dit la même chose côté monde.
struct EntityGauge {
  bool known = false;
  std::int32_t current = 0;
  std::int32_t most = 0;
  // `current` est un pourcentage et `most` cent : la part est juste, mais le
  // texte ne s'écrit qu'en pour cent.
  bool in_percent = false;
};

// LES COULEURS DU CLIENT, converties de COLORREF en `IM_COL32`.
//
// ⚠ REPRISES ET NON CHOISIES : ce sont celles de la jauge du client. Les
// reprendre fait qu'un joueur qui
// vient du client d'origine reconnaît son écran.
inline constexpr ImU32 kClientBarFill = IM_COL32(0x16, 0xEA, 0x25, 255);
inline constexpr ImU32 kClientBarLowFill = IM_COL32(0xEA, 0x16, 0x16, 255);
inline constexpr ImU32 kClientBarMagicFill = IM_COL32(0x1D, 0x65, 0xDB, 255);
inline constexpr ImU32 kClientBarBackground = IM_COL32(0x44, 0x44, 0x44, 255);
inline constexpr ImU32 kClientBarBorder = IM_COL32(0x14, 0x19, 0x9B, 255);

// LA PART SOUS LAQUELLE LA BARRE PASSE EN ALERTE. Un quart, et c'est le seuil
// du client — pas un arrondi de notre part.
inline constexpr float kClientLowShare = 0.25F;

// LES MESURES DU CLIENT, en pixels : soixante sur cinq, posée douze pixels sous
// les pieds — douze pixels d'un écran de 480 de haut, mis à l'échelle de la
// fenêtre.
inline constexpr float kClientBarWidthPixels = 60.0F;
inline constexpr float kClientBarHeightPixels = 5.0F;
inline constexpr float kClientBarDropPixels = 12.0F;

// Les bornes des curseurs, en pixels. Une barre plus étroite que son propre
// cadre ne montrerait plus rien.
inline constexpr float kSmallestEntityBarWidth = 16.0F;
inline constexpr float kLargestEntityBarWidth = 240.0F;
inline constexpr float kSmallestEntityBarHeight = 2.0F;
inline constexpr float kLargestEntityBarHeight = 24.0F;
inline constexpr float kLargestEntityBarOffset = 80.0F;
inline constexpr float kLargestEntityBarGap = 8.0F;
// ⚠ CINQ PIXELS EN BAS, PARCE QU'UNE BARRE EN FAIT SIX. Sept forçait un texte
// plus haut que la barre qui le porte, donc débordant de son cadre quoi qu'on
// fasse du centrage. Une vraie police rend encore des chiffres de quatre pixels
// à cette taille — petits, mais c'est un choix qui revient au joueur.
inline constexpr float kSmallestEntityBarFont = 5.0F;
inline constexpr float kLargestEntityBarFont = 24.0F;

// Le plancher de la taille une fois le zoom appliqué. ⚠ TECHNIQUE, ET NON
// LISIBLE : sept pixels dézoomés au tiers donnent deux, et c'est bien ce qu'on
// veut — mais zéro ferait diviser par zéro le calcul d'échelle d'ImGui.
inline constexpr float kSmallestBakedFontPixels = 1.0F;

// L'épaisseur du cadre, en pixels. ⚠ CELLE DU CLIENT, ET ELLE NE SE RÈGLE PAS :
// un cadre de trois pixels sur une barre qui en fait cinq ne laisse plus de
// place à ce qu'il borde.
inline constexpr float kEntityBarBorderPixels = 1.0F;

// CE QUE LE JOUEUR A FAIT DES BARRES D'UNE ESPÈCE.
//
// 🔴 TOUT EST PAR ESPÈCE, sauf ce qui ne peut pas différer. La tentation d'une
// allure commune se paie tout de suite : on veut une barre fine et muette sur
// les moutons, et une barre large qui écrit ses nombres sur un MVP. Un réglage
// commun n'est pas plus simple, il est plus pauvre — c'est la leçon des six
// jauges libres.
struct EntityBarLook {
  // L'INTERRUPTEUR DE L'ESPÈCE. ⚠ MAÎTRE DES DEUX BARRES : éteint, il grise le
  // reste de l'onglet plutôt que de le faire disparaître.
  bool shown = true;
  bool health = true;
  // LA MAGIE. ⚠ ÉTEINTE PAR DÉFAUT PARTOUT, et ce n'est pas de la timidité :
  // hors de soi et de la cible, AUCUN paquet du jeu ne la porte. Allumée, elle
  // ne paraît que là où la donnée existe — jamais en barre vide.
  bool magic = false;

  EntityBarWhen when = EntityBarWhen::kDamaged;
  // Un mort n'a plus de vie à montrer, et le serveur ne la reprend pas.
  bool hide_dead = true;

  // ── LA PLACE ──────────────────────────────────────────────────────────────
  //
  // 🔴 SOUS LES PIEDS EST LE CLIENT, et c'est le défaut. Au-dessus de la tête
  // est ce que beaucoup de clients tiers font, et cela se règle : la barre suit
  // alors la TAILLE du sprite, donc elle monte au-dessus d'un MVP.
  bool under_feet = true;
  // De combien elle s'écarte du point d'ancrage. Douze pixels sous les pieds,
  // comme le client ; au-dessus de la tête, six suffisent.
  float offset_pixels = kClientBarDropPixels;
  float width_pixels = kClientBarWidthPixels;
  float height_pixels = kClientBarHeightPixels;
  // La hauteur de la barre de magie. ⚠ À PART : le client donne à ses deux
  // barres la moitié de la hauteur chacune, ce qui les rend illisibles toutes
  // les deux. Ici la magie est plus fine que la vie, et cela se règle.
  float magic_height_pixels = 3.0F;
  float gap_pixels = 1.0F;

  // ── L'ALLURE ──────────────────────────────────────────────────────────────
  ImU32 fill = kClientBarFill;
  // LA COULEUR SOUS LE SEUIL. Voir `low_share`.
  ImU32 low_fill = kClientBarLowFill;
  float low_share = kClientLowShare;
  // LE DÉGRADÉ CONTINU, du plein vers l'alerte, au lieu d'une bascule au seuil.
  // ⚠ IL S'ÉCARTE DU CLIENT, et c'est pour cela qu'il est éteint : le client
  // bascule d'un coup, et cette bascule est un SIGNAL — on la voit du coin de
  // l'œil, ce qu'un dégradé continu ne donne pas.
  bool gradient = false;
  ImU32 magic_fill = kClientBarMagicFill;
  ImU32 background = kClientBarBackground;
  bool border = true;
  ImU32 border_color = kClientBarBorder;
  float rounding = 0.0F;
  // De zéro à un, appliquée à TOUT — cadre, creux, remplissage et texte.
  float opacity = 1.0F;

  // ── LE TEXTE ──────────────────────────────────────────────────────────────
  //
  // ⚠ MUET PAR DÉFAUT : le client n'écrit rien sur ces barres (il ne pose que
  // des rectangles), et vingt barres chiffrées
  // sur une carte peuplée ne se lisent plus.
  EntityBarText text = EntityBarText::kNothing;
  float font_pixels = 11.0F;
  ImU32 text_color = IM_COL32(240, 240, 245, 255);
};

// LES DÉFAUTS, ESPÈCE PAR ESPÈCE.
//
// ⚠ UNE FONCTION ET NON UN AGRÉGAT VIDE : trois espèces s'écartent du défaut
// commun, et c'est ici qu'on lit lesquelles — la mienne éteinte, le boss plus
// large et bavard, le groupe montré même intact.
[[nodiscard]] std::array<EntityBarLook, kEntityBarKindCount> DefaultEntityBars();

// CE QUI VAUT POUR TOUTES LES ESPÈCES.
struct EntityBarsLook {
  // L'interrupteur maître. ⚠ ALLUMÉ : la barre des monstres entamés est ce que
  // le client montre, et l'éteindre par défaut serait s'écarter de lui.
  bool shown = true;

  // LE MODE RÉGLAGE : les jauges ondulent, du vide au plein, dans le cadre de
  // la page comme sur les entités à l'écran, pour voir ce qu'un réglage fait à
  // une barre qui bouge. ⚠ PAS DANS LE FICHIER DE RÉGLAGES, et sans effet
  // panneau fermé : une vie factice affichée en combat serait un mensonge.
  bool tuning = false;

  // LES MARCHANDS SANS BARRE : qui tient une échoppe, de vente ou d'achat, ne
  // porte plus ni vie ni magie. ⚠ ALLUMÉ PAR DÉFAUT : un marchand ne combat
  // pas, et là où le serveur donne la vie de tous les joueurs, une rue de
  // boutiques devient une rangée de barres pleines qui couvrent les enseignes.
  // ⚠ COMMUN AUX ESPÈCES, parce qu'il dit QUI et non COMMENT : un marchand est
  // tantôt du groupe, tantôt un inconnu, et doit se taire dans les deux cas.
  bool hide_vendors = true;

  std::array<EntityBarLook, kEntityBarKindCount> kinds = DefaultEntityBars();
};

// CE QU'`app` A TROUVÉ AU-DESSUS D'UNE ENTITÉ, en pixels d'écran.
//
// ⚠ TOUT CE QUI SERT À DÉCIDER EST DÉJÀ LÀ. Le filtre — l'espèce, le survol, la
// cible, la mort — se joue dans `ShowsEntityBar`, qui ne monte ni monde ni
// ImGui : c'est ce qui rend « quand une barre paraît » vérifiable dans un essai.
struct EntityBarView {
  // L'ACTEUR QUI LA PORTE : c'est par lui que son nom retrouve sa barre, pour
  // ne pas s'y poser dessus. Zéro pour une barre qui n'en a pas.
  std::uint32_t actor_id = 0;
  // LE POINT D'ANCRAGE. `feet` sont les pieds projetés, `head` le haut de ce que
  // le sprite occupe : le réglage choisit lequel sert, et l'appelant n'a donc
  // pas à connaître ce choix.
  ImVec2 feet{0.0F, 0.0F};
  float head_y = 0.0F;
  EntityBarKind kind = EntityBarKind::kMonster;
  EntityGauge health;
  EntityGauge magic;
  bool hovered = false;
  bool targeted = false;
  bool dead = false;

  // COMBIEN LE MONDE PARAÎT PLUS GRAND QU'AU RECUL DE RÉFÉRENCE. Voir
  // `ro::render::ApparentScale` : c'est `app` qui le calcule, une fois par
  // image, parce que `ui` ne connaît pas la caméra.
  //
  // ⚠ BORNÉ AU DESSIN et non ici : la valeur brute est juste, et c'est
  // l'affichage qui décide de ce qu'il est raisonnable d'en faire.
  float scale = 1.0F;
};

// ⛔ UNE ÉCHELLE DE ZOOM A VÉCU ICI, bornée entre 0,35 et 3, et l'ADR 0053 la
// défendait : « une barre de taille fixe sur un monde qui change de taille rend
// le réglage d'écart faux à tous les reculs sauf un ».
//
// 🔴 ELLE EST PARTIE PARCE QU'ELLE NE TENAIT PAS À L'ŒIL. Le client ne fait pas
// cela : sa jauge est un widget de soixante sur cinq PIXELS que rien
// n'agrandit, et seule son ORDONNÉE passe par une échelle — douze lignes d'un
// écran de 480, fois l'échelle de dessin du sprite. La taille
// ne suit donc ni le zoom ni la fenêtre ; l'écart suit la fenêtre et le corps.
// Voir `ro::render::ScreenAnchorScale`, qui porte la mesure.

// ── UN SEUL ARRONDI, ET LE CADRE LE SUIT ────────────────────────────────────
//
// 🔴 LE CADRE EST UN TRAIT POSÉ DESSUS, PAS UN RECTANGLE PLEIN DESSOUS — et
// c'est le joueur qui l'a fait changer : « le cadre cache l'arrondi ».
//
// La mesure dit pourquoi. `ImDrawList::PathRect` borne tout rayon à
// `hauteur / 2 - 1` et abandonne l'arrondi sous un demi-pixel. Le creux rentré
// d'un pixel de chaque côté perd donc TOUJOURS un pixel d'arrondi sur le cadre,
// et sur la barre du client — cinq pixels de haut — il tombe à 0,5, c'est-à-dire
// à un rectangle droit :
//
//     h= 5 : cadre 1,5 px · creux 0,5 px   (le contenu est CARRÉ)
//     h=12 : cadre 5,0 px · creux 4,0 px
//     h=24 : cadre 11 px  · creux 10 px
//
// Or ce que l'œil voit d'une barre, c'est sa PART PLEINE, pas son liseré sombre.
// Le contenu occupe donc toute la surface et garde l'arrondi entier ; le cadre
// se pose par-dessus en le suivant.
//
// ⚠ C'EST UN ÉCART ASSUMÉ AVEC LE CLIENT, qui peint bien un rectangle plein puis
// le creux dedans. Il ne coûte rien en fidélité : le client
// n'arrondit RIEN, cet arrondi est à nous.

// UNE BARRE, EXPORTÉE POUR ÊTRE MESURÉE.
//
// 🔴 C'EST CE QUI DISTINGUE CETTE PREUVE D'UN RAISONNEMENT, comme pour l'orbe
// des jauges libres : comparer deux rayons ne dit rien si le dessin passe le
// mauvais au creux. Un banc peint donc une barre dans une liste nue et relève
// ses SOMMETS — aucun de ceux du creux ne doit sortir du coin du cadre.
//
// `ink` est la couleur du remplissage, déjà choisie par l'appelant : voir
// `EntityBarFillColor`, qui sait si l'on est sous le seuil d'alerte.
void PaintEntityBar(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, float fill, ImU32 ink,
                    const EntityBarLook& look);

// LA PILE DE BARRES D'UNE ENTITÉ, telle que le jeu la peint — du point
// d'ancrage vers le bas, ou vers le haut. ⚠ SANS FILTRE : c'est
// `ShowsEntityBar` qui décide si elle paraît.
void PaintEntityBarStack(ImDrawList& into, const EntityBarView& what, const EntityBarLook& look);

// ── LE NOM ET LES BARRES SOUS LES PIEDS ─────────────────────────────────────
//
// 🔴 DEUX PLACES INDÉPENDANTES, COMME CHEZ LE CLIENT : la jauge et le nom se
// mesurent chacun depuis les pieds, de leur propre écart, et aucun ne pousse
// l'autre. Le nom ne se déplace QUE s'il chevauche une barre réellement peinte
// sur cette entité à cet instant — il se pose alors juste dessous. Une réserve
// tirée du seul réglage le faisait sauter pour toutes les entités d'une espèce
// dès qu'on allumait ses barres, et laissait un trou sous un monstre intact.

// Une bande horizontale de l'écran, de son haut à son bas, en pixels.
struct ScreenBand {
  float top = 0.0F;
  float bottom = 0.0F;
};

// L'écart laissé entre une barre et le nom qu'elle repousse.
inline constexpr float kNameBarGapPixels = 2.0F;

// LA BANDE QUE CES BARRES PRENNENT SOUS LES PIEDS, ou rien quand elles se posent
// au-dessus de la tête. ⚠ SANS FILTRE : c'est à l'appelant de dire si elles
// paraissent — `ShowsEntityBar` en jeu, l'espèce allumée dans le cadre d'essai.
[[nodiscard]] std::optional<ScreenBand> EntityBarsBandUnderFeet(const EntityBarLook& look,
                                                                const EntityBarView& what);

// LE HAUT DU NOM, écarté des barres qu'il chevaucherait : juste sous elles,
// `kNameBarGapPixels` plus bas. Inchangé quand rien ne se chevauche.
//
// ⚠ TOUJOURS VERS LE BAS : un nom remonté vers les pieds passerait sous le
// sprite, là où on ne le lit plus.
[[nodiscard]] float NameTopClearOfBars(float name_top, float name_height,
                                       const std::optional<ScreenBand>& bars);

// UNE JAUGE DU MODE RÉGLAGE : elle ondule du vide au plein, à une période
// propre à `rank` et à la barre, pour que deux barres ne battent pas ensemble.
[[nodiscard]] EntityGauge TuningEntityGauge(double seconds, std::size_t rank, bool magic);

// VRAI SI CETTE BARRE-LÀ DOIT PARAÎTRE, réglages en main.
//
// 🔴 SÉPARÉ DU DESSIN POUR ÊTRE ÉPROUVÉ. Cinq conditions se croisent — l'espèce
// éteinte, le mort, la valeur inconnue, le seuil d'entame, le survol — et un
// « et » de trop se voit à l'écran comme une barre qui ne vient jamais, ce
// qu'on met une soirée à distinguer d'un serveur qui se tait.
[[nodiscard]] bool ShowsEntityBar(const EntityBarLook& look, const EntityBarView& what);

// La part remplie d'une jauge, entre zéro et un. Zéro quand rien n'est connu.
[[nodiscard]] float EntityGaugeFill(const EntityGauge& gauge);

// LA COULEUR DU REMPLISSAGE pour cette part-là : le plein, l'alerte, ou le
// dégradé entre les deux quand on l'a demandé.
[[nodiscard]] ImU32 EntityBarFillColor(const EntityBarLook& look, float fill);

// LE TEXTE D'UNE BARRE dans le mode demandé. Vide en mode muet, et vide aussi
// quand la valeur est inconnue — un « 0 % » inventé serait pire que rien. Une
// jauge `in_percent` s'écrit en pour cent dans tout mode chiffré.
[[nodiscard]] std::string EntityBarCaption(const EntityGauge& gauge, EntityBarText mode);

// LA HAUTEUR TOTALE que cette entité occupera, cadre compris : de quoi savoir
// où poser ce qui vient au-dessus — une bulle de parole, un nom.
[[nodiscard]] float EntityBarsHeight(const EntityBarLook& look, const EntityBarView& what);

// LA TAILLE DE POLICE DEMANDÉE AU DESSIN, EN PIXELS ENTIERS.
//
// 🔴 ENTIÈRE, PARCE QU'IMGUI CUIT SES GLYPHES À `IM_ROUND(taille)` PUIS LES
// ÉTIRE DE `taille / cuite`. Une taille fractionnaire fait donc rééchantillonner
// chaque lettre : sur la rampe de zoom, l'étirement va jusqu'à 10 %, et il SAUTE
// quinze fois — de 1,08 à 0,92 d'un coup, à chaque demi-pixel franchi. Le texte
// s'épaissit, maigrit et ressaute pendant qu'on tourne la molette.
//
// Demander un entier rend l'étirement égal à un : les lettres se posent alors
// texel pour texel, et la taille monte par paliers d'un pixel.
//
// ⚠ AU MOINS UN PIXEL : zéro ferait une division par zéro dans le calcul
// d'échelle d'ImGui.
// ⚠ PLUS D'ÉCHELLE ICI : la taille réglée est celle qu'on grave, et le zoom n'y
// touche plus. Ce qui suit reste vrai d'un réglage fractionnaire.
[[nodiscard]] float EntityBarFontPixels(const EntityBarLook& look);

// LE POINT DE DESSIN D'UN TEXTE CENTRÉ DANS UNE BOÎTE, SON ENCRE AU MILIEU.
//
// 🔴 CENTRER SUR LA BOÎTE DE LIGNE NE CENTRE PAS LE TEXTE. `CalcTextSizeA` rend
// la hauteur de LIGNE — la taille de police, tout simplement —, et les lettres
// n'y sont pas au milieu : elles pendent sous l'ascendante, et des chiffres, qui
// n'ont pas de descendante, laissent tout leur creux EN HAUT. Mesuré sur
// malgun.ttf à sept pixels, pour « 99% » : deux pixels de creux au-dessus de
// l'encre, ZÉRO en dessous. Le bas des chiffres touchait donc le cadre pendant
// que le haut respirait.
//
// ⚠ LA BOÎTE D'ENCRE SE DEMANDE AUX GLYPHES, ET ELLE DÉPEND DU TEXTE : « 99% »
// et « 1234 / 5678 » ne pendent pas pareil, et le creux change encore avec la
// police et la taille — de 0,5 à 2,5 pixels sur la seule gamme du curseur. Une
// constante de recentrage serait juste pour un cas et fausse pour les autres.
// Voir `tests/ui/font_box_bench.cpp`, qui reprend la mesure.
//
// ⚠ ALIGNÉ SUR LE PIXEL, comme tout ce qu'on pose dans une barre.
[[nodiscard]] ImVec2 EntityBarTextOrigin(ImFont& font, float pixels, ImVec2 top_left,
                                         ImVec2 bottom_right, const char* text);

// Dessine tout ce qui a été récolté, sur la liste de FOND.
//
// ⚠ LES VUES SONT DES DONNÉES, et ce qui les filtre est le réglage : passer une
// liste non filtrée est la conduite normale, `ShowsEntityBar` tranche ici.
void DrawEntityBars(std::span<const EntityBarView> bars, const EntityBarsLook& look);

// LE NOM D'UNE ESPÈCE, celui de l'onglet. ⚠ DÉJÀ TRADUIT.
[[nodiscard]] const char* EntityBarKindName(EntityBarKind kind);

}  // namespace ro::ui
