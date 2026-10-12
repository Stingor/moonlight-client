#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/render/sprite_catalog.h"

// Aides partagées par les fichiers du catalogue — le corps, la tête et les
// mains dans sprite_catalog.cpp, la cape dans garment_catalog.cpp. Elles
// portent ce que tous les gabarits du client ont en commun : la racine des
// sprites, la race, le jeton de sexe, le nom de corps d'un métier, la lecture
// d'une table.
//
// Interne à ro_render : rien ici n'est une promesse d'API.
namespace ro::render::catalog_internal {

// Les dossiers du client, tels que ses gabarits les nomment. Ils y sont en
// CP949 ; ici en UTF-8, et c'est le VFS qui transcode.
inline constexpr std::string_view kSpriteRoot = "data\\sprite\\";
inline constexpr std::string_view kSeparator = "\\";
inline constexpr std::string_view kHumanRace = "인간족";
inline constexpr std::string_view kDoramRace = "도람족";
inline constexpr std::string_view kMaleToken = "남";
inline constexpr std::string_view kFemaleToken = "여";
// La tête, et les coiffes — que le client range sous « accessoires ».
inline constexpr std::string_view kHeadFolder = "머리통";
inline constexpr std::string_view kHeadgearFolder = "악세사리";

// Les classes d'arme du client, par leur rang dans WeaponNameTable
// (weapontable.lub) ; au-delà de vingt-quatre, les couples de deux armes.
inline constexpr std::uint32_t kFistsClass = 0;
inline constexpr std::uint32_t kDaggerClass = 1;
inline constexpr std::uint32_t kSwordClass = 2;
inline constexpr std::uint32_t kTwoHandSwordClass = 3;
inline constexpr std::uint32_t kSpearClass = 4;
inline constexpr std::uint32_t kTwoHandSpearClass = 5;
inline constexpr std::uint32_t kAxeClass = 6;
inline constexpr std::uint32_t kTwoHandAxeClass = 7;
inline constexpr std::uint32_t kMaceClass = 8;
inline constexpr std::uint32_t kRodClass = 10;
inline constexpr std::uint32_t kBowClass = 11;
inline constexpr std::uint32_t kKnuckleClass = 12;
inline constexpr std::uint32_t kBookClass = 15;
inline constexpr std::uint32_t kKatarClass = 16;
inline constexpr std::uint32_t kRifleClass = 18;
inline constexpr std::uint32_t kGrenadeLauncherClass = 21;
inline constexpr std::uint32_t kShurikenClass = 22;
inline constexpr std::uint32_t kTwoHandRodClass = 23;
inline constexpr std::uint32_t kDaggerPairClass = 25;
inline constexpr std::uint32_t kSwordPairClass = 26;
inline constexpr std::uint32_t kAxePairClass = 27;
inline constexpr std::uint32_t kDaggerSwordClass = 28;
inline constexpr std::uint32_t kDaggerAxeClass = 29;
inline constexpr std::uint32_t kSwordAxeClass = 30;
inline constexpr std::uint32_t kFirstWeaponPairClass = kDaggerPairClass;
inline constexpr std::uint32_t kLastWeaponPairClass = kSwordAxeClass;

[[nodiscard]] bool IsDoram(std::uint32_t job);

// Les coiffures. Au-delà de la table humaine, le numéro s'écrit tel quel, pour
// l'humain comme pour le Doram — c'est le patch Allow65kHairs du client de ce
// serveur, dont le GRF porte des têtes jusqu'à 97. En deçà, un Doram hors de
// 1..10 porte la dixième coupe. ⚠ De 11 au bout de la table humaine, le client
// du joueur lit HORS de la table Doram, ce qui ne se reproduit pas : on garde
// la dixième.
inline constexpr std::uint16_t kDoramHairFirst = 1;
inline constexpr std::uint16_t kDoramHairLast = 10;

// Les fichiers de la tête d'un joueur — voir le cas du Doram sans coupe.
[[nodiscard]] ActorPieceFiles HeadPieceFiles(const ActorLook& look);

// LES MERCENAIRES HUMAINS, que le client habille comme un joueur : voir
// sprite_catalog_mercenary.cpp.
[[nodiscard]] bool IsHumanMercenary(std::uint32_t job);
// Ajoute leurs pièces après le corps : la tête, l'arme et sa lueur, la coiffe
// de la classe, les ornements des meilleurs. Rien pour une autre classe.
void AppendMercenaryPieces(std::uint32_t job, std::vector<ActorPieceFiles>& files);

// Les métiers qui tiennent une arme dans chaque main.
[[nodiscard]] bool IsDualWielder(std::uint32_t job);

// La classe d'une arme tenue : la vue de l'objet, -1 pour un bouclier reconnu à
// sa plage, zéro sans table.
[[nodiscard]] int WeaponClassOf(const SpriteCatalog& catalog, std::uint32_t item);

// Le couple de deux armes en main, ou l'arme seule.
[[nodiscard]] std::uint32_t DualWieldClass(std::uint32_t right, std::uint32_t left);

// La famille d'une classe d'arme (GetRealWeaponId) : elle-même, sauf pour une
// classe étendue, que Expansion_Weapon_IDs ramène à la sienne.
[[nodiscard]] std::uint32_t WeaponFamilyClass(const SpriteCatalog& catalog,
                                              std::uint32_t weapon_class);

// Le jeton d'un sexe : 0 = femme, 1 = homme, comme le serveur.
[[nodiscard]] std::string_view SexToken(std::uint8_t sex);

[[nodiscard]] std::string Join(std::initializer_list<std::string_view> parts);

// Le nom de corps d'un métier joueur, tel que le client le connaît ; celui du
// novice pour un métier qu'il ne nomme pas.
[[nodiscard]] std::string_view PlayerBodyName(std::uint32_t job);

// VRAI QUAND LE STYLE DE CORPS EST UNE TENUE ALTERNATIVE : le corps se cherche
// alors dans le dossier des costumes.
[[nodiscard]] bool WearsAlternateOutfit(const ActorLook& look);

// LE MÉTIER DONT UN JOUEUR EMPRUNTE LE CORPS, hors monture cash-shop : celui de
// son style de corps, que le styliste du serveur peut poser sur n'importe quel
// métier. Une tenue alternative y prête le métier qu'elle habille, et la
// monture du métier montré passe au corps emprunté. Le corps, la cape, l'arme
// et le bouclier se lisent sur lui ; la tête et le numéro de palette, non.
[[nodiscard]] std::uint32_t BodyClass(const ActorLook& look);

// Le métier dont un joueur prend le CORPS : `BodyClass`, ou sa classe montée
// sur une monture cash-shop.
[[nodiscard]] std::uint32_t BodyJob(const ActorLook& look);

// LE NOM D'UN FICHIER DE MADOGEAR SOUS LA COMBINAISON : l'armure au lieu de la
// machine, pour le corps comme pour sa lueur. Tout autre nom passe tel quel,
// et tout nom passe tel quel hors de la combinaison.
[[nodiscard]] std::string_view MadogearSuitName(std::string_view machine_name, bool suit);

// Le nom de corps qu'un joueur MONTRE : celui de `BodyJob`, sous la
// combinaison s'il la porte.
[[nodiscard]] std::string_view ShownBodyName(const ActorLook& look);

// Ce qu'une table du client donne à un identifiant : un nom, en UTF-8, ou un
// nombre. nullopt si la table ou l'entrée manque, ou n'est pas de ce type.
[[nodiscard]] std::optional<std::string> TableName(const SpriteCatalog& catalog,
                                                   std::string_view table_name, std::uint32_t id);
[[nodiscard]] std::optional<double> TableNumber(const SpriteCatalog& catalog,
                                                std::string_view table_name, std::uint32_t id);

}  // namespace ro::render::catalog_internal
