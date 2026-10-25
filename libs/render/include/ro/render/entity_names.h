#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ro::render {

// ── LES ÉTIQUETTES AU-DESSUS — OU PLUTÔT SOUS — LES ENTITÉS ─────────────────
//
// Ce que le client natif écrit aux pieds de ce que la souris désigne : il
// replace une étiquette UNIQUE à chaque survol, et c'est tout. Une carte peuplée reste donc lisible
// parce qu'un seul nom s'écrit à la fois.
//
// 🔴 CE FICHIER NE DÉCRIT QUE DES RÉGLAGES, et c'est ce qui rend « quand un nom
// paraît » vérifiable sans fenêtre ni carte : `ShowsEntityName` ne monte ni
// monde ni GPU. Voir `ro/ui/entity_bars.h`, dont ce découpage reprend la forme
// — jusqu'aux raisons.
//
// ⚠ ET IL NE CONNAÎT PAS `sim`. Il reçoit une ESPÈCE, que `app` a traduite du
// monde, exactement comme les barres d'entité reçoivent la leur.

// L'ESPÈCE DONT ON RÈGLE L'ÉTIQUETTE — un onglet chacune.
//
// 🔴 CE DÉCOUPAGE EST CELUI DES DONNÉES, PAS UN GOÛT, et il n'est pas celui des
// barres de vie. Ce qui distingue une étiquette d'une autre, c'est ce que le
// serveur met dedans :
//
//   · ce qui n'est PAS un joueur se coupe au croisillon — voir `NameCutAtHash` ;
//   · un PNJ reçoit son titre EN TOUTES LETTRES, un joueur un NUMÉRO que la
//     table de l'installation traduit ;
//   · un MONSTRE porte un niveau, et c'est la seule espèce sur laquelle le
//     client applique sa couleur d'écart de niveau ;
//   · un BOSS porte un insigne, et rien d'autre n'en porte ;
//   · un OBJET AU SOL n'a pas de nom du tout — le client le COMPOSE depuis la
//     table des objets, et `clif_name` ne répond pas sur un `BL_ITEM`.
//
// ⚠ PAS D'ESPÈCE « MON GROUPE » ICI, contre les barres de vie : le nom d'un
// compagnon arrive comme celui de n'importe quel joueur, et rien dans
// l'étiquette ne les sépare. Un onglet de plus promettrait une distinction que
// le protocole ne fait pas.
//
// ⚠ L'ORDRE PART DANS LE FICHIER DE RÉGLAGES : ne pas intercaler.
enum class NamePlateKind : std::uint8_t {
  // MOI. ⚠ LE CLIENT N'ÉCRIT PAS MON NOM sous mes pieds, et c'est pourquoi
  // cette espèce est éteinte par défaut.
  kOwn = 0,
  kPlayer = 1,
  // LES PNJ. 🔴 UN PORTAIL N'EN EST PAS, ET CE N'EST PAS UN DÉTAIL : le serveur
  // NOMME ses warps — « ggprowarp » —, et le client sort avant de construire
  // leur étiquette (métier 45). Aucun réglage de
  // cet onglet ne doit pouvoir les faire paraître ; voir `HasNamePlate`.
  kNonPlayer = 2,
  kMonster = 3,
  // MINI-BOSS ET MVP, la seule espèce qui porte un insigne.
  kBoss = 4,
  // LES OBJETS AU SOL. ⚠ LEUR NOM EST COMPOSÉ PAR NOUS, pas reçu : voir
  // `GroundItemName`. Le serveur ne nomme jamais un butin.
  kGroundItem = 5,
};
inline constexpr std::size_t kNamePlateKindCount = 6;

// QUAND UNE ÉTIQUETTE PARAÎT.
//
// 🔴 SON TYPE SOUS-JACENT EST `int`, ET CE N'EST PAS UN DÉTAIL : ce rang se
// déclare au fichier de réglages par un `reinterpret_cast<int*>`, et la lecture
// écrit alors QUATRE octets. Sur un `std::uint8_t`, elle écraserait les champs
// suivants de la structure, en silence. Même piège que `EntityBarWhen`.
enum class NamePlateWhen : int {
  // TOUS LES NOMS, TOUT LE TEMPS. ⚠ CE N'EST PAS CE QUE FAIT LE CLIENT : sur
  // une carte de chasse, c'est trente étiquettes par-dessus les sprites.
  kAlways = 0,
  // CE QUE LA SOURIS DÉSIGNE — le comportement du client, et le défaut.
  kHovered = 1,
  kTarget = 2,
  kHoveredOrTarget = 3,
  kCount,
};

// Le rang lu du fichier de réglages, borné. ⚠ LA SOURCE N'EST PAS SOUS NOTRE
// CONTRÔLE — le joueur ouvre ce fichier dans un éditeur de texte — et un
// `static_cast` hors bornes sur une énumération est un comportement indéfini.
[[nodiscard]] NamePlateWhen NamePlateWhenOf(int stored);

// ── LES COULEURS, EN UN SEUL ENTIER ─────────────────────────────────────────
//
// 🔴 EMPAQUETÉES PARCE QU'ELLES SE PERSISTENT. Le fichier de réglages ne sait
// déclarer qu'un `std::uint32_t` ; un tableau de quatre octets demanderait une
// surcharge de plus pour ne rien gagner.
//
// ⚠ L'ORDRE EST CELUI D'`IM_COL32` — le ROUGE en poids faible —, et c'est
// voulu : le nuancier du panneau de réglages travaille dans ce format, et une
// conversion de plus entre lui et nous serait une occasion de plus d'inverser
// le rouge et le bleu. Voir `name_plate.h`, qui raconte ce que cette inversion
// a coûté une fois.
[[nodiscard]] constexpr std::uint32_t PackedColor(const std::array<std::uint8_t, 4>& rgba) {
  return static_cast<std::uint32_t>(rgba[0]) | static_cast<std::uint32_t>(rgba[1]) << 8U |
         static_cast<std::uint32_t>(rgba[2]) << 16U | static_cast<std::uint32_t>(rgba[3]) << 24U;
}

[[nodiscard]] constexpr std::array<std::uint8_t, 4> UnpackedColor(std::uint32_t packed) {
  return {static_cast<std::uint8_t>(packed & 0xFFU),
          static_cast<std::uint8_t>(packed >> 8U & 0xFFU),
          static_cast<std::uint8_t>(packed >> 16U & 0xFFU),
          static_cast<std::uint8_t>(packed >> 24U & 0xFFU)};
}

// Les bornes des curseurs. ⚠ L'ÉCART PEUT ÊTRE NÉGATIF : le nom remonte alors
// vers le sprite, et c'est ce que demande une étiquette posée sur une monture.
inline constexpr float kSmallestNamePlateOffset = -40.0F;
inline constexpr float kLargestNamePlateOffset = 80.0F;

// La taille de la police des noms, en pixels. ⚠ COMMUNE AUX SIX ESPÈCES, et
// c'est le seul réglage qui le soit sans que ce soit un choix : notre atlas est
// GRAVÉ à une taille, et six atlas coûteraient six gravures.
inline constexpr float kSmallestNamePlateFont = 7.0F;
inline constexpr float kLargestNamePlateFont = 32.0F;
inline constexpr float kDefaultNamePlateFont = 12.0F;

// CE QUE LE JOUEUR A FAIT DES ÉTIQUETTES D'UNE ESPÈCE.
//
// 🔴 TOUT EST PAR ESPÈCE, sauf ce qui ne peut pas différer. C'est la leçon des
// six jauges libres et des six barres d'entité : un réglage commun n'est pas
// plus simple, il est plus pauvre — on veut le nom des monstres au survol et
// celui des objets au sol en permanence, parce qu'un butin se cherche des yeux.
//
// ⚠ TOUS N'EXISTENT PAS SUR TOUTES LES ESPÈCES. Un objet au sol n'a pas de
// titre, un PNJ pas de seconde ligne, et seul un boss porte un insigne : le
// champ reste dans la structure — une seule forme pour six espèces —, mais le
// panneau ne le PROPOSE que là où il peut faire quelque chose. Le tableau est
// dans `app/options/entity_name_fields.h`, avec le paquet qui le justifie.
struct EntityNameLook {
  // L'INTERRUPTEUR DE L'ESPÈCE. ⚠ MAÎTRE DU RESTE DE L'ONGLET : éteint, il
  // grise ce qui suit plutôt que de le faire disparaître.
  bool shown = true;
  NamePlateWhen when = NamePlateWhen::kHovered;

  // ── CE QUE L'ÉTIQUETTE ÉCRIT ──────────────────────────────────────────────
  //
  // ⚠ CES TROIS-LÀ NE SONT PAS TOUJOURS REMPLIS : le serveur choisit ce qu'il
  // met dans la carte de nom, et ce qu'il tait reste vide quoi qu'on coche.
  // Voir `sim::NameCard`.

  // Le TITRE, devant le nom et entre crochets — « [Master] Stingor ».
  bool title = true;
  // CE QUI SUIT LE NOM, entre parenthèses. ⚠ SUR UN MONSTRE, CE SERVEUR-CI Y
  // MET LE NIVEAU ET LA VIE (`show_mob_info`) ; sur un objet au sol, c'est la
  // QUANTITÉ ramassable, que nous y composons.
  bool aside = true;
  // LA SECONDE LIGNE — le groupe, la guilde, le clan — et son insigne entre
  // crochets.
  bool banner = true;

  // L'INSIGNE DE BOSS, la tête de mort argent ou dorée posée à gauche du nom.
  // ⚠ SANS EFFET HORS DE L'ESPÈCE « BOSS » : le rang vient du paquet, et les
  // autres espèces n'en ont pas.
  bool boss_badge = true;

  // ── LES COULEURS ──────────────────────────────────────────────────────────
  //
  // ⚠ LE DÉFAUT EST CELUI DU CLIENT, converti de COLORREF : voir
  // `name_plate.h`, qui dit pourquoi ces octets se lisent dans l'ordre inverse
  // d'un COLORREF.
  std::uint32_t color = 0;

  // L'ÉCART DE NIVEAU, qui ÉCRASE la couleur ci-dessus : rouge quand la cible
  // nous dépasse de plus de dix niveaux, gris quand on la dépasse de plus de
  // quinze.
  //
  // ⚠ IL NE VAUT QUE SUR UN MONSTRE, et pas par timidité : le client le garde
  // derrière un « ce n'est pas un métier de joueur », et le niveau des autres
  // joueurs n'est pas montré par ce chemin. Ailleurs, il ne se propose pas ET
  // ne s'applique pas — c'est `NameColorOf` qui le refuse, et non le panneau,
  // parce que le fichier de réglages s'ouvre dans un éditeur de texte.
  bool level_gap = false;

  // L'OMBRE portée sous le texte. ⚠ CE QUI REND UN NOM LISIBLE SUR UN SOL
  // CLAIR : le client en pose une sous chacun des siens, en dessinant le texte
  // quatre fois autour de lui.
  bool shadow = true;
  std::uint32_t shadow_color = 0;

  // ── LA PLACE ──────────────────────────────────────────────────────────────
  //
  // De combien l'étiquette s'écarte des pieds, vers le bas. VINGT, LA PLACE DU
  // CLIENT : sous sa jauge, qu'il pose à douze, sans la toucher.
  float offset_pixels = 20.0F;
  // De zéro à un, appliquée à TOUT — texte, ombre et insigne.
  float opacity = 1.0F;
};

// LES DÉFAUTS, ESPÈCE PAR ESPÈCE.
//
// ⚠ UNE FONCTION ET NON UN AGRÉGAT VIDE : quatre espèces s'écartent du défaut
// commun, et c'est ici qu'on lit lesquelles — la mienne éteinte, les monstres
// et les boss avec leur écart de niveau, les objets au sol montrés en
// permanence parce qu'un butin par terre est ce qu'on cherche des yeux.
[[nodiscard]] std::array<EntityNameLook, kNamePlateKindCount> DefaultEntityNames();

// CE QUI VAUT POUR TOUTES LES ESPÈCES.
struct EntityNamesLook {
  // L'interrupteur maître. ⚠ ALLUMÉ : le nom de ce que la souris désigne est ce
  // que le client montre, et l'éteindre par défaut serait s'écarter de lui.
  bool shown = true;

  // LA TAILLE DE LA POLICE DES NOMS, en pixels. ⚠ COMMUNE, parce que l'atlas
  // est gravé : voir `kDefaultNamePlateFont`.
  float font_pixels = kDefaultNamePlateFont;

  // L'EMBLÈME DE GUILDE à gauche de l'étiquette — le « /emblem » du client.
  // ⚠ UN SEUL INTERRUPTEUR POUR TOUTES LES ESPÈCES, comme chez lui, et allumé :
  // c'est le défaut de sa table de réglages.
  bool guild_emblem = true;

  // L'EMBLÈME DE CLAN, à la même place, chez qui n'a PAS de guilde. ⚠ UNE
  // GUILDE LE FAIT TAIRE, même sans emblème ou sa case décochée : voir
  // `ChoosePlateEmblem`. « /emblem » ne touche pas cette case-ci.
  bool clan_emblem = true;

  std::array<EntityNameLook, kNamePlateKindCount> kinds = DefaultEntityNames();
};

// CE QU'`app` A TROUVÉ SUR UNE ENTITÉ. ⚠ TOUT CE QUI SERT À DÉCIDER EST DÉJÀ
// LÀ : le filtre ne regarde rien d'autre, et c'est ce qui le rend vérifiable.
struct EntityNameView {
  NamePlateKind kind = NamePlateKind::kMonster;
  bool hovered = false;
  bool targeted = false;
};

// UNE ÉTIQUETTE DE CETTE ESPÈCE PARAÎT-ELLE, dans cet état-là ?
[[nodiscard]] bool ShowsEntityName(const EntityNamesLook& look, const EntityNameView& what);

// Le libellé d'un onglet. ⚠ DÉJÀ TRADUIT — ces noms paraissent dans le panneau.
[[nodiscard]] const char* EntityNameKindName(NamePlateKind kind);

}  // namespace ro::render
