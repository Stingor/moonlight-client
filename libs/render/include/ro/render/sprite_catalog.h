#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"
#include "ro/formats/monster_skill_info.h"
#include "ro/render/actor_layers.h"
#include "ro/render/body_sprite_key.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// De l'APPARENCE d'un acteur aux FICHIERS qui la dessinent.
//
// Le serveur ne nomme aucun fichier : il annonce des identifiants — un métier,
// une coiffure, un accessoire, un objet tenu, une robe — et c'est le client qui
// les traduit, avec trois sources que ce module reproduit :
//
//   - ses tables de données, des .lub : le nom d'un monstre, d'un PNJ, d'un
//     accessoire, d'une arme, d'une cape ; et, pour la cape, de quel côté du
//     corps elle passe, image par image ;
//   - sa table des objets : la classe d'une arme, la forme d'un bouclier ;
//   - son exécutable : le nom de corps d'un métier joueur, le numéro de
//     fichier d'une coiffure, le dossier de ses armes. Voir client_tables.cpp.
//
// Cette couche ne lit ni le réseau ni la simulation : app lui remet des
// nombres et reçoit des chemins.

// Ce que le serveur dit d'un acteur, réduit à ce qui choisit ses fichiers.
struct ActorLook {
  // Un joueur se compose de pièces — corps, tête, coiffes, ce qu'il tient, sa
  // cape. Tout le reste est un sprite unique, dont le métier est une classe de
  // monstre ou de PNJ — ou, pour un objet au sol, un identifiant d'objet.
  bool player = false;
  bool item = false;
  std::uint32_t job = 0;
  // 0 = femme, 1 = homme, comme le serveur.
  std::uint8_t sex = 0;
  // La coiffure telle qu'annoncée. Ce n'est PAS un numéro de fichier : les
  // premières coupes sont permutées, et pas de la même façon selon le sexe.
  std::uint16_t hair = 0;
  // Le costume ; zéro est la tenue ordinaire.
  std::uint16_t body_style = 0;
  // Bas, milieu, haut : des identifiants d'accessoire, zéro pour rien.
  std::array<std::uint16_t, 3> headgears{};
  // Ce que chaque main tient : des identifiants d'OBJET — ou une vue, si la
  // base d'objets du serveur en déclare une. Zéro pour rien. Une arme à deux
  // mains ne se dit qu'à droite ; ce que la main gauche tient est un bouclier,
  // ou une seconde arme.
  std::uint32_t weapon = 0;
  std::uint32_t shield = 0;
  // La cape : un identifiant de robe, zéro pour rien.
  std::uint16_t robe = 0;
  // Monté sur une monture de trésorerie : le corps devient celui de la
  // monture, la cape le suit, et les mains se vident. C'est un ÉTAT que le
  // serveur pose et retire, pas un champ de l'apparence.
  bool cash_mount = false;
  // Zéro = la palette du .spr.
  std::int16_t hair_palette = 0;
  std::int16_t body_palette = 0;
  // LE STYLE : l'identifiant d'une recette de couleurs que la garde-robe
  // connaît, ou zéro pour l'apparence native. Voir `ActorWardrobe::SetStyle`.
  //
  // 🔴 IL FAIT PARTIE DE L'APPARENCE, ET C'EST OBLIGATOIRE. La garde-robe range
  // ses tenues par apparence : sans ce champ, deux joueurs de même classe et
  // même couleur de vêtement PARTAGERAIENT leur tenue, donc la palette de
  // l'un repeindrait l'autre.
  //
  // 🔴 UNE RECETTE MODIFIÉE PREND UN NOUVEL IDENTIFIANT. Voir `SetStyle` : ce
  // qui est déjà dessiné ne se mute pas.
  std::uint32_t style = 0;
  // LA TÊTE D'ORC : la tête du joueur cède la place au visage d'orc des effets,
  // sans palette de cheveux. Un ÉTAT, comme la monture, et pas un champ de
  // l'apparence que le joueur choisit.
  bool orc_head = false;
  // LA COMBINAISON DE MADOGEAR, et non la machine : sur un corps de Madogear
  // (`IsMadogearJob`), le corps et la lueur prennent l'armure (« 마도아머 »
  // au lieu de « 마도기어 », MEISTER_MADOGEAR2 au lieu de 1), et l'ombre
  // s'élargit. Un ÉTAT encore : la valeur d'EFST_MADOGEAR_TYPE.
  bool madogear_suit = false;
  // L'ACCESSOIRE D'UN PET : l'apparence que le serveur annonce, zéro pour
  // rien. Il ne pose pas de coiffe : il remplace le .act du corps — voir
  // `PetAccessoryAnimationPath`. Ignoré hors d'un pet.
  std::uint16_t pet_accessory = 0;
};

// Les tables du client, dans l'état où il les laisse après les avoir évaluées
// dans SON ordre : jobName lit les identifiants que NPCIdentity a nommés.
struct SpriteCatalog {
  formats::LuaTable globals;
  // LA TABLE DES OBJETS, entière — voir `ro/vfs/item_table.h`. Le renderer n'en
  // lit que deux nombres, l'apparence et le nom de ressource ; l'interface, qui
  // la reçoit par le même chemin, en lit le texte. Treize méga-octets ne se
  // lisent pas deux fois.
  //
  // Vide si le fichier manque — plus rien ne se tient alors en main.
  vfs::ItemTable items;
  // LE GESTE D'UNE CRÉATURE POUR UNE COMPÉTENCE — voir
  // `ro/formats/monster_skill_info.h`. Vide si le fichier manque : aucune
  // créature ne bouge alors en employant une compétence.
  formats::MonsterSkillInfo monster_skill_actions;
};

// Le fichier des gestes de créature, là où l'installation le range.
inline constexpr std::string_view kMonsterSkillInfoFile = "data\\monsterskillinfo.xml";

// LE NOM DE RESSOURCE d'une classe qui n'est pas un métier joueur — celui qui
// nomme son sprite, et sous lequel la table des gestes la range.
[[nodiscard]] std::string NonPlayerResourceName(const SpriteCatalog& catalog, std::uint32_t job);

// Les fichiers que le client évalue, dans son ordre : ceux qui nomment des sprites, et la liste
// d'héritage des métiers parce qu'elle nomme JOBID, dont les tables de couches des capes se
// servent. Le client évalue entre eux des fichiers de fonctions (_f) et ses
// anciennes tables de couches, que sa passe actuelle ne lit plus. Un fichier
// absent se saute, comme chez lui ; un fichier illisible refuse le catalogue :
// mieux vaut le savoir qu'habiller tout le monde en Poring.
inline constexpr std::array<std::string_view, 18> kSpriteCatalogFiles = {
    "data\\luafiles514\\lua files\\datainfo\\npcidentity.lub",
    "data\\luafiles514\\lua files\\datainfo\\jobidentity.lub",
    "data\\luafiles514\\lua files\\datainfo\\npclocationradius.lub",
    "data\\luafiles514\\lua files\\datainfo\\shadowtable.lub",
    "data\\luafiles514\\lua files\\datainfo\\jobname.lub",
    "data\\luafiles514\\lua files\\datainfo\\accessoryid.lub",
    "data\\luafiles514\\lua files\\datainfo\\accname.lub",
    "data\\luafiles514\\lua files\\skillinfoz\\jobinheritlist.lub",
    "data\\luafiles514\\lua files\\datainfo\\spriterobeid.lub",
    "data\\luafiles514\\lua files\\datainfo\\spriterobename.lub",
    "data\\luafiles514\\lua files\\spreditinfo\\2dlayerdir_f.lub",
    "data\\luafiles514\\lua files\\spreditinfo\\_new_2dlayerdir_f.lub",
    "data\\luafiles514\\lua files\\spreditinfo\\_new_biglayerdir_male.lub",
    "data\\luafiles514\\lua files\\spreditinfo\\_new_biglayerdir_female.lub",
    "data\\luafiles514\\lua files\\spreditinfo\\_new_smalllayerdir_male.lub",
    "data\\luafiles514\\lua files\\spreditinfo\\_new_smalllayerdir_female.lub",
    "data\\luafiles514\\lua files\\datainfo\\weapontable.lub",
    "data\\luafiles514\\lua files\\datainfo\\petinfo.lub",
};
inline constexpr std::string_view kJobNameTableGlobal = "JobNameTable";
// Le rayon qu'une classe occupe au sol, en cellules. Huit classes seulement y
// figurent — les grands monstres —, et c'est ce rayon qui grandit leur ombre.
inline constexpr std::string_view kNpcRadiusTableGlobal = "NpcLocationRadius";

// Le facteur d'ombre d'une classe, quand le rayon ne dit rien : mille deux cent
// quatre-vingt-huit classes y figurent, de zéro — un portail n'a pas d'ombre —
// à deux pour les plus grosses créatures. C'est CETTE table qui donne à chaque
// monstre son ombre, et l'ignorer les fait toutes de la même taille.
inline constexpr std::string_view kShadowFactorTableGlobal = "ShadowFactorTable";
inline constexpr std::string_view kAccessoryNameTableGlobal = "AccNameTable";
inline constexpr std::string_view kWeaponNameTableGlobal = "WeaponNameTable";
inline constexpr std::string_view kExpansionWeaponsGlobal = "Expansion_Weapon_IDs";
inline constexpr std::string_view kRobeNameTableGlobal = "RobeNameTable";

// Les tables de couches des capes : par robe, sa taille — la grande quand rien
// n'est dit — ; par métier, celui dont il prend la table ; puis, par taille et
// par sexe, la table elle-même, qui liste par métier et par action les images
// où la cape passe derrière le corps.
inline constexpr std::string_view kLayerSizeTypeListGlobal = "LayerSizeTypeList";
inline constexpr double kSmallLayerSizeType = 1;
inline constexpr std::string_view kLayerJobInheritanceGlobal = "LAYER_ALL_TABLE_LIST";
struct LayerTableGlobals {
  std::string_view female;
  std::string_view male;
};
inline constexpr LayerTableGlobals kBigLayerTableGlobals = {"_New_BigLayerDir_F",
                                                            "_New_BigLayerDir_M"};
inline constexpr LayerTableGlobals kSmallLayerTableGlobals = {"_New_SmallLayerDir_F",
                                                              "_New_SmallLayerDir_M"};

[[nodiscard]] core::Result<SpriteCatalog> LoadSpriteCatalog(vfs::Vfs& vfs);

// Une pièce à charger.
struct ActorPieceFiles {
  ActorPart part = ActorPart::kBody;
  // Les chemins à essayer dans l'ordre, SANS extension, en UTF-8 : le premier
  // qui existe l'emporte. Une coiffe a une variante Doram que la plupart des
  // chapeaux n'ont pas ; le client l'essaie, puis retombe sur le chemin humain.
  std::vector<std::string> candidates;
  // La palette de remplacement, ou vide pour celle du .spr.
  std::string palette_path;
  // Vrai quand l'absence de TOUS les chemins est un cas ordinaire et non une
  // anomalie : le client sonde ses armes, ses traînées, ses boucliers et ses
  // capes, et n'en dessine aucun sans un mot quand le fichier manque.
  bool optional = false;
  // Le .act, quand il ne porte pas le nom du .spr retenu ; vide sinon. Deux
  // pièces seulement ont leurs deux fichiers à des chemins différents : la cape,
  // et la tête d'un Doram sans coupe.
  std::string animation_path;
};

// Le sprite d'OMBRE, le même pour tout le monde. Le client le charge à part de
// l'acteur, par son nom nu.
inline constexpr std::string_view kShadowSpritePath = "data\\sprite\\shadow";

// De combien l'ombre d'un acteur est grandie, ou ZÉRO s'il n'en porte pas.
//
// DEUX SOURCES, dans cet ordre, comme le client.
// D'abord le RAYON au sol : quand la classe y figure, l'échelle vaut deux fois
// le rayon plus un. Sinon le FACTEUR d'ombre, que le client relève classe par
// classe au démarrage en appelant ReqshadowFactor de zéro à trente-deux mille,
// et qu'il garde en cache. Un pour ce qui ne figure ni dans l'une
// ni dans l'autre.
//
// Deux acteurs n'en ont aucune : la tombe d'un monstre, que le client écarte
// nommément, et un objet au sol, qui n'est pas un être. Un portail non plus,
// mais c'est la table qui le dit, par un facteur nul. Et la combinaison de
// Madogear passe outre les deux tables : une fois et demie, 1,12 pour le bébé.
[[nodiscard]] float ShadowScaleFor(const SpriteCatalog& catalog, const ActorLook& look);

// Le sprite de corps. Pour un joueur, d'après son métier, son sexe et sa
// race ; pour un objet au sol, d'après la ressource que la table des objets
// lui donne ; pour les autres, d'après la table des noms et la plage de la
// classe, qui choisit le dossier. nullopt quand la classe désigne un modèle
// 3D, que rien ici ne dessine, ou un objet que la table ne connaît pas.
//
// ⚠ Le style de corps ne désigne un COSTUME que dans la plage des classes de
// tenue alternative ; partout ailleurs c'est le corps ordinaire. Le serveur met
// dans ce champ la classe du personnage lui-même quand il ne porte aucune tenue
// alternative, et le lire comme un numéro de costume fait chercher un fichier
// qui n'existe pas.
[[nodiscard]] std::optional<std::string> BodySpritePath(const SpriteCatalog& catalog,
                                                        const ActorLook& look);

// Vrai quand la classe se dessine en modèle 3D : elle n'a alors aucun sprite.
[[nodiscard]] bool DrawnAsModel(const SpriteCatalog& catalog, const ActorLook& look);

// Le corps, puis ce sur quoi se rabattre s'il manque : une tenue alternative
// absente de l'installation laisse le corps ordinaire, plutôt qu'un acteur
// invisible — rien ne s'ancre sans corps.
[[nodiscard]] std::vector<std::string> BodySpritePaths(const SpriteCatalog& catalog,
                                                       const ActorLook& look);

// La tête d'un joueur.
[[nodiscard]] std::string HeadSpritePath(const ActorLook& look);

// Une coiffe, ou rien si la table ne connaît pas l'accessoire.
[[nodiscard]] std::vector<std::string> HeadgearSpritePaths(const SpriteCatalog& catalog,
                                                           const ActorLook& look,
                                                           std::uint16_t accessory);

// La table qui nomme, par accessoire de pet, le .act de son corps paré.
inline constexpr std::string_view kPetAccessoryActTableGlobal = "PetAccActNameTable";

// LE .ACT DU CORPS D'UN PET PARÉ, sans extension, à côté de `body_path` : celui
// que nomme `kPetAccessoryActTableGlobal`. Le .spr reste celui de l'espèce, qui
// porte déjà les images de l'accessoire.
//
// Vide quand l'acteur n'est pas un pet paré, ou que la table ne connaît pas
// l'accessoire : le corps garde alors son propre .act.
[[nodiscard]] std::string PetAccessoryAnimationPath(const SpriteCatalog& catalog,
                                                    const ActorLook& look,
                                                    std::string_view body_path);

// L'arme tenue, dans l'ordre où le client la cherche : par le nom propre de sa
// classe d'abord, par le nom de sa famille ensuite. Deux armes en main se
// dessinent en une seule pièce, nommée par le couple.
[[nodiscard]] std::vector<std::string> WeaponSpritePaths(const SpriteCatalog& catalog,
                                                         const ActorLook& look);

// La traînée de l'arme — le trait de lumière qui suit une lame —, quand son
// fichier existe.
[[nodiscard]] std::vector<std::string> WeaponTrailPaths(const SpriteCatalog& catalog,
                                                        const ActorLook& look);

// LES DEUX ARMES D'UN AMBIDEXTRE, CHACUNE SOUS SON PROPRE SPRITE.
//
// Le client d'origine dessine deux armes en main d'une seule pièce, nommée par
// leur couple : deux dagues différentes deviennent la même paire générique.
// Ici, chaque main garde le sprite de son objet — celui que WeaponSpritePaths
// donnerait si elle le tenait seule —, la droite au rang de l'arme et la gauche
// au rang de la traînée, qu'elle remplace.
//
// nullopt quand la main gauche ne tient pas d'arme — un bouclier, ou rien — ou
// que rien ne se dessine en main. `main_hand` est vide quand seule la gauche
// est armée. C'est la garde-robe qui juge, fichiers en main, si la séparation
// tient : voir ActorWardrobe.
struct DualWieldFiles {
  std::vector<std::string> main_hand;
  std::vector<std::string> off_hand;
};
[[nodiscard]] std::optional<DualWieldFiles> DualWieldSpritePaths(const SpriteCatalog& catalog,
                                                                 const ActorLook& look);

// Le bouclier : la variante propre à l'objet d'abord, la forme générique
// ensuite — et rien pour les métiers auxquels le client n'en dessine pas.
[[nodiscard]] std::vector<std::string> ShieldSpritePaths(const SpriteCatalog& catalog,
                                                         const ActorLook& look);

// La cape. Le client cherche son .spr d'abord À PLAT, sous le nom de la robe —
// suffixé pour un Doram —, puis dans la disposition par métier ; son .act, lui,
// toujours par métier. nullopt sans cape, pour une robe que la table ne nomme
// pas, et pour la mécanicienne en Madogear, à qui le client n'en dessine pas.
[[nodiscard]] std::optional<ActorPieceFiles> GarmentFiles(const SpriteCatalog& catalog,
                                                          const ActorLook& look);

// Où la cape se dessine, image par image, d'après les tables de couches du
// client. Vide — devant, toujours — sans cape ou sans table.
[[nodiscard]] GarmentPlacement GarmentPlacementFor(const SpriteCatalog& catalog,
                                                   const ActorLook& look);

// Le rang d'animation d'ATTAQUE : dix ou onze pour un joueur, selon son métier
// et ce qu'il tient ; deux pour une créature. Les taekwons tirent au sort à chaque coup ;
// chance_tenth, de zéro à neuf, en tient lieu. Un HOMONCULE tire aussi au sort,
// une fois sur deux entre son rang deux et son rang six — un dixième pair
// donne le six —, sauf Eleanor, qui frappe toujours au deux.
[[nodiscard]] std::size_t AttackAnimation(const SpriteCatalog& catalog, const ActorLook& look,
                                          std::uint8_t chance_tenth);

// LE RANG D'ATTAQUE QUE L'ARME SEULE DÉCIDE, sans le tirage du taekwon : celui
// de l'état d'action neuf, la variante d'attaque des compétences de barde et de
// danseuse. Deux pour une créature.
[[nodiscard]] std::size_t AttackVariantAnimation(const SpriteCatalog& catalog,
                                                 const ActorLook& look);

// L'image de l'attaque où le coup d'un JOUEUR porte — c'est là que la cible
// bronche —, comptée depuis le début de l'animation, d'après une table par
// métier et par arme. Une créature n'y passe pas : son image est celle du
// marqueur du coup de son .act, voir `ActorOutfit::AttackSoundFrame`.
[[nodiscard]] float DamageFrame(const ActorLook& look, std::size_t attack_animation);

// Le facteur de cadence de l'attaque : la durée que le serveur lui donne,
// rapportée aux 432 ms d'une attaque à vitesse un — plafonnée à un, sauf pour
// un arc. Il multiplie la durée de chaque image.
[[nodiscard]] float AttackMotionFactor(const SpriteCatalog& catalog, const ActorLook& look,
                                       std::uint32_t attack_motion_ms);

// LA DURÉE D'IMAGE QUE L'IMPACT D'UNE COMPÉTENCE IMPOSE à son lanceur, en pas
// de vingt-quatre millisecondes, par-dessus celle de l'attaque — ou rien. Fatal
// Menace, sur un coup qui éclabousse (sorte 5), pose quatre fois le quotient de
// sa durée d'attaque par 432 ms, sans plafond ; une durée nulle ne pose rien.
[[nodiscard]] std::optional<float> ImpactFramePeriodSteps(std::uint16_t skill_id,
                                                          std::uint8_t damage_type,
                                                          std::uint32_t attack_motion_ms);

// Le facteur de cadence de la BLESSURE : la durée que le serveur donne au coup
// pour sa CIBLE, rapportée aux 288 ms d'une blessure à vitesse un. Il multiplie lui aussi la
// durée de chaque image, et il n'est PAS plafonné.
//
// ⚠ Ce n'est ni la même unité ni le même plafond que l'attaque, et surtout ce
// n'est pas une DURÉE : le serveur annonce 288 pour la plupart des monstres,
// et la prendre pour une durée fait broncher tout le monde pendant exactement
// 288 ms, quel que soit ce que son .act déclare.
//
// La SORTE du coup entre ici parce que le défaut d'une durée nulle en dépend :
// le client y substitue 1440 ms, sauf pour un coup ordinaire ou encaissé, qu'il
// laisse à zéro — et zéro veut alors dire « la cadence que le
// fichier déclare », donc un.
[[nodiscard]] float HurtMotionFactor(std::uint32_t hurt_motion_ms, std::uint8_t damage_type);

// LA CADENCE QU'UNE BLESSURE LAISSE, en pas de vingt-quatre millisecondes par
// image — celle qu'une méthode nommée relit ensuite sur l'acteur.
//
// La posture de blessure part à la cadence du fichier (`file_steps`, déjà
// relevée à un pas), sans facteur : le facteur de cadence ne sert qu'à
// l'attaque, et le client le remet à un après chaque action. C'est ENSUITE que
// le quotient de la blessure (la durée du serveur sur 288, zéro pour un coup
// ordinaire sans durée) la retouche, et pas de la même façon selon la cible :
//   - un JOUEUR voit sa cadence MULTIPLIÉE par le quotient quand il ne dépasse
//     pas un ;
//   - une CRÉATURE la voit REMPLACÉE par le quotient lui-même, sans plancher ;
//   - au-delà de un, la cadence reste celle du fichier des deux côtés — le
//     quotient va alors à un autre réglage, que cette cadence ne lit pas.
[[nodiscard]] float HurtCadenceSteps(float file_steps, std::uint32_t hurt_motion_ms,
                                     std::uint8_t damage_type, bool player_class);

// LA CADENCE D'UNE ACTION QUI COMMENCE, en pas de vingt-quatre millisecondes
// par image : celle que son fichier déclare, relevée à un pas au moins.
[[nodiscard]] float FileCadenceSteps(float frame_interval_ms);

// LA BLESSURE TELLE QU'ELLE SE JOUE : sa cadence (voir `HurtCadenceSteps`) et
// le nombre de PASSAGES qu'elle tient. Au-delà de un, le quotient ne touche pas
// la cadence : il allonge la tenue, la dernière image restant affichée jusqu'à
// ce que ce nombre de passages soit écoulé.
struct HurtPlay {
  float cadence_steps = 1.0F;
  float passes = 1.0F;
};
[[nodiscard]] HurtPlay HurtPlayFor(float file_steps, std::uint32_t hurt_motion_ms,
                                   std::uint8_t damage_type, bool player_class);

// L'IMAGE DE LA BLESSURE `since_ms` après qu'elle a porté, ou RIEN quand elle
// n'a pas commencé ou qu'elle est finie. Elle défile à sa cadence, se TIENT sur
// sa dernière image, et finit quand les passages sont écoulés — un au moins.
[[nodiscard]] std::optional<std::size_t> HurtFrameAt(const HurtPlay& play, std::size_t frame_count,
                                                     float since_ms);

// CE QUE L'IMAGE DE FRAPPE D'UN JOUEUR RECULE POUR SON PROJECTILE, en
// millisecondes : huit pas pour la flèche d'un arc, vingt-quatre pour le trait
// d'une baguette (voir `ShootsWandBolts`), huit pour l'âme qu'un Doram tire
// sous l'état qui la donne — l'arme et l'âme s'ajoutent. Rien pour une
// créature, dont le geste ne recule pas. C'est la part du retard que l'attaque
// elle-même pose, et que le coup d'une compétence d'état deux pose aussi.
[[nodiscard]] float ArrowDelayMs(const SpriteCatalog& catalog, const ActorLook& look,
                                 bool shoots_souls);

// Le retard du coup ORDINAIRE sur la cible, en millisecondes, quand un
// projectile lent le porte : la flèche d'un archer, le trait d'une baguette,
// l'âme d'un Doram, et celui de quelques monstres — un rattrapage par espèce,
// ajouté au coup et non au geste.
[[nodiscard]] float ProjectileDelayMs(const SpriteCatalog& catalog, const ActorLook& look,
                                      bool shoots_souls = false);

// Le rattrapage par espèce du coup d'une COMPÉTENCE : le client n'en garde
// que l'Archer Squelette et sa variante, à huit pas. Rien pour un joueur, dont
// la flèche est déjà dans son image de frappe.
[[nodiscard]] float SkillHitProjectileDelayMs(const ActorLook& look);

// LE VOL D'UNE FLÈCHE D'ARC : huit pas de vingt-quatre millisecondes, et c'est aussi de cela que la
// blessure de la cible est retardée.
inline constexpr float kArrowFlightMs = 192.0F;
// LE SPRITE DE CETTE FLÈCHE, celui que le client donne à tout tireur qui n'a pas le sien — un
// joueur, quel qu'il soit —, sans dossier ni extension : « monstre\flèche d'archer squelette ».
inline constexpr std::string_view kArrowSpriteBase = "data\\sprite\\몬스터\\skel_archer_arrow";
// Combien de haut la flèche vole, au départ comme à l'arrivée : le client
// retranche dix à l'ordonnée des deux bouts, et son ordonnée descend.
inline constexpr float kArrowFlightHeight = 10.0F;
// SOUS QUELLE DISTANCE LA FLÈCHE NE RALENTIT PLUS : en deçà de quarante unités,
// le client garde cinq unités par pas et raccourcit le vol d'autant.
inline constexpr float kArrowFullFlightDistance = 40.0F;

// VRAI QUAND CET ACTEUR TIRE DES FLÈCHES en frappant : son arme est de la
// classe des arcs, celle que la liste `BowTypeList` du Lua du client nomme,
// interrogée à chaque attaque. C'est ce qui fait naître le projectile ; rien d'autre.
[[nodiscard]] bool ShootsArrows(const SpriteCatalog& catalog, const ActorLook& look);

// ── Le trait de la baguette ─────────────────────────────────────────────────
//
// ⚠ ÉCART VOULU AU CLIENT D'ORIGINE : chez lui, un bâton frappe sans rien
// tirer. Ici, un JOUEUR dont l'arme est de la famille des
// bâtons — le bâton et le bâton à deux mains, sous-types repliés sur eux —
// tire un trait à chaque attaque ordinaire, comme l'arc tire sa flèche : même
// sommeil jusqu'à l'image de frappe, même hauteur, même ligne droite, même
// raccourci sous quarante unités, mêmes images que la flèche. Mais son
// horloge va TROIS FOIS moins vite : le vol, et le défilement des images avec
// lui, durent trois fois plus, et la blessure de la cible attend d'autant.
[[nodiscard]] bool ShootsWandBolts(const SpriteCatalog& catalog, const ActorLook& look);
// Son sprite, sans extension, dans le dossier des monstres.
inline constexpr std::string_view kWandBoltSpriteBase = "data\\sprite\\몬스터\\baguette_shot";
// De combien son horloge est divisée par rapport à celle de la flèche.
inline constexpr std::int32_t kWandBoltSlowdown = 3;
// Son vol plein, et ce dont la blessure de la cible est retardée.
inline constexpr float kWandBoltFlightMs = kArrowFlightMs * static_cast<float>(kWandBoltSlowdown);
// CE QUE SON IMAGE TOURNE EN PLUS DE L'ANGLE D'ÉCRAN D'UNE FLÈCHE, en degrés :
// son `.act` n'est pas dessiné sur l'axe de la flèche.
inline constexpr std::int32_t kWandBoltTurnDegrees = 270;

// LES MÉTIERS DORAM — l'invocateur et les siens : les seuls dont un coup ordinaire tire une âme.
[[nodiscard]] bool IsDoramJob(std::uint32_t job);

// VRAI POUR UN HOMONCULE : les seize premiers et les cinq mutants, hors de la
// plage des mercenaires humains qui les sépare.
[[nodiscard]] bool IsHomunculusJob(std::uint32_t job);

// VRAI QUAND CETTE CLASSE EST UN MÉTIER JOUEUR, donc un corps que l'on COMPOSE
// — tête, coiffes, armes — plutôt qu'un sprite unique.
//
// ⚠ UN PNJ PEUT EN PORTER UN. Le serveur annonce alors un PNJ ordinaire dont
// la classe est un métier : le lire comme une classe de PNJ ne trouve aucun
// nom, et dessine un Poring à la place.
[[nodiscard]] bool IsPlayerClass(std::uint32_t job);

// L'ÂME QU'UN DORAM TIRE, sans dossier ni extension : le sprite que le client
// donne aux métiers 4217 à 4221, 4308 et 4315.
inline constexpr std::string_view kSoulAttackSpriteBase = "data\\sprite\\몬스터\\su_soulattack";

// LE PROJECTILE QUE CET ACTEUR TIRE EN FRAPPANT, sans dossier ni extension —
// ou VIDE quand il n'en tire pas. Il vole comme la flèche : il dort le temps
// du geste, puis huit pas jusqu'à la cible.
//
// Un joueur tire par son ARME — la flèche générique, voir `ShootsArrows`, ou
// le trait de la baguette, voir `ShootsWandBolts` — ou par son ÂME : un Doram
// sous `EFST_SOULATTACK` tire `su_soulattack` à chaque coup, et `shoots_souls`
// dit si l'état est posé. Le client teste l'arme puis l'âme, l'une après
// l'autre.
//
// Une créature tire par son ESPÈCE : l'aiguillage du client sur l'identifiant
// de monstre mène à DEUX sortes de projectile qui volent pareil — la flèche de
// l'archer, et l'effet 2404, la balle de monstre habillée selon l'espèce. Même
// sommeil jusqu'à l'image de frappe, même vol de huit pas des pieds du tireur
// aux pieds de la cible, dix unités au-dessus et en ligne droite, même
// raccourci sous quarante unités, même face du vol. Ce qui les sépare —
// l'avance des images, la troncature de l'angle — est dit par
// `ProjectileImageRule`.
//
// ⚠ LA TROISIÈME CLASSE NE VOLE PAS : voir `StrikeSpriteOf`. Ses espèces
// rendent VIDE ici.
[[nodiscard]] std::string_view ArrowSpriteOf(const SpriteCatalog& catalog, const ActorLook& look,
                                             bool shoots_souls);

// COMMENT UN PROJECTILE FAIT DÉFILER SES IMAGES EN VOL, ET COMMENT IL TOURNE.
// Les deux classes de projectile d'acteur du client volent pareil, mais chacune
// dessine à sa façon ce qu'elle porte.
enum class ProjectileImageRule : std::uint8_t {
  // La flèche d'un arc, et les munitions que sa table habille : la première
  // image tenue huit mises à jour, puis une image par mise à jour, figée sur la
  // dernière ; l'angle d'écran tronqué au degré.
  kArrow,
  // La munition en sprite d'un monstre — l'effet 2404 — : une image par mise à
  // jour dès la première, en boucle ; l'angle d'écran en degrés réels.
  kSpriteBullet,
};

// Combien de mises à jour la flèche tient sa première image.
inline constexpr std::int32_t kArrowFirstImageHoldTicks = 8;

// La règle du projectile de cet acteur : celle de la munition en sprite pour
// les espèces que l'aiguillage du client envoie à cette classe, celle de la
// flèche pour tout autre tireur — un joueur, ou une espèce à flèche propre.
[[nodiscard]] ProjectileImageRule ProjectileImageRuleOf(const ActorLook& look);

// COMMENT LE PROJECTILE D'UN ACTEUR VOLE ET SE DESSINE, au-delà de son trajet.
struct ProjectileFlight {
  ProjectileImageRule image_rule = ProjectileImageRule::kArrow;
  // De combien son horloge est divisée : le vol ET la tenue de chaque image
  // durent autant de fois plus. Un pour tout projectile du client.
  std::int32_t clock_slowdown = 1;
  // Ce que son image tourne en plus de la face de son vol, en degrés d'écran,
  // sur [0, 360).
  std::int32_t turn_degrees = 0;
};

// Le vol du projectile que cet acteur tire en frappant : celui du trait pour
// qui tient une baguette, la règle de `ProjectileImageRuleOf` à l'horloge du
// client sinon.
[[nodiscard]] ProjectileFlight ProjectileFlightOf(const SpriteCatalog& catalog,
                                                  const ActorLook& look);

// L'IMPACT DE CES COMPÉTENCES LÈVE SUR LEUR LANCEUR UN DRAPEAU que le PROCHAIN
// tir de flèche dépense sans partir : elles dessinent leurs propres traits.
// Triangle Shot et deux tirs de l'Abyss Chaser en dur, trois autres par leur
// action enregistrée. Le drapeau survit à un coup qui ne tire rien.
[[nodiscard]] bool ImpactSuppressesNextArrow(std::uint16_t skill_id);

// LE COUP QUI S'ABAT SUR LA CIBLE : un sprite d'acteur joué UNE fois à ses
// pieds, à l'instant du geste et sans vol — la morsure de la Mandragore, la
// tête de l'Hydre. Le client le pose sur la cible avec un retard NUL, et il
// meurt au bout de son action zéro, à la cadence que le `.act` déclare — sauf
// l'Hydre, forcée à deux tics par image.
struct CreatureStrikeSprite {
  std::string_view sprite_base;
  // La cadence FORCÉE, en tics de vingt-quatre millisecondes par image ; zéro
  // pour celle du fichier.
  float frame_period_units = 0.0F;
};

// Le coup de cette espèce, ou rien : les trente et une espèces que l'aiguillage
// du client mène à ce coup.
[[nodiscard]] std::optional<CreatureStrikeSprite> StrikeSpriteOf(const ActorLook& look);

// Les palettes de remplacement. Vides pour zéro, qui veut dire « celle du
// .spr » : le client n'en charge alors aucune.
[[nodiscard]] std::string BodyPalettePath(const ActorLook& look);
[[nodiscard]] std::string HairPalettePath(const ActorLook& look);

// Toutes les pièces d'un acteur, dans l'ordre des DÉPENDANCES d'ancrage : le
// corps, puis la tête, puis les coiffes qui s'y accrochent, puis ce que les
// mains tiennent, puis la cape. Vide quand l'acteur n'a pas de sprite.
// UN MERCENAIRE HUMAIN, que le client habille comme un joueur — tête, arme,
// coiffes — à partir de sa seule classe.
//
// 🔴 ET QU'IL FIGE AU REPOS : dans l'attente, corps, tête et coiffes se lisent
// sur leur image zéro, jamais déroulée. Les images de repos d'une tête sont les
// orientations du regard ; les dérouler la fait tourner sans fin.
[[nodiscard]] bool IsHumanMercenaryClass(std::uint32_t job);

[[nodiscard]] std::vector<ActorPieceFiles> ActorFiles(const SpriteCatalog& catalog,
                                                      const ActorLook& look);

}  // namespace ro::render
