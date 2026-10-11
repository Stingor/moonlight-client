#pragma once

#include <array>
#include <cstdint>
#include <string_view>

// Les tables que le client porte dans son exécutable plutôt que dans ses
// fichiers de données. Leur origine est dite dans client_tables.cpp, qui est
// engendré et ne se retouche pas à la main.
//
// Interne à ro_render : c'est sprite_catalog.cpp qui sait les employer, bornes
// comprises.
namespace ro::render::client_tables {

struct PlayerBodyName {
  std::uint32_t job;
  // Le nom du fichier de corps, sans le suffixe de sexe ni l'extension, en
  // UTF-8. Le GRF le porte en CP949 : c'est au VFS de transcoder.
  std::string_view name;
};

// Triés par métier, pour une recherche dichotomique. Un métier absent porte
// le corps du novice : c'est ce que le client fait, sa table étant remplie de
// ce nom avant d'être renseignée.
extern const std::array<PlayerBodyName, 321> kPlayerBodyNames;
inline constexpr std::string_view kDefaultPlayerBodyName = "초보자";

// Le dossier et le préfixe des sprites d'ARMES et de BOUCLIERS d'un métier :
// un chevalier runique tient les armes du chevalier, et un invocateur à
// charrette celles de l'invocateur. Triés par métier.
struct WeaponSpriteClass {
  std::uint32_t job;
  std::string_view folder;
  std::string_view prefix;
};
extern const std::array<WeaponSpriteClass, 320> kWeaponSpriteClasses;
inline constexpr std::string_view kDefaultWeaponSpriteClass = "초보자";

// La classe MONTÉE d'un métier joueur : celle dont il prend le corps sur une
// monture cash-shop. Triés par métier ; un métier absent garde le sien.
struct CashMountClass {
  std::uint32_t job;
  std::uint32_t mounted;
};
extern const std::array<CashMountClass, 150> kCashMountClasses;

// Le suffixe de fichier d'un bouclier, par forme. Zéro n'est pas une forme,
// et une forme au-delà de la table retombe sur ce même vide.
extern const std::array<std::string_view, 5> kShieldNames;

// Le numéro de fichier de tête, indexé par la coiffure que le serveur annonce.
// Ce n'est PAS l'identité : les douze premières coupes sont permutées, et pas
// de la même façon pour les deux sexes.
extern const std::array<std::string_view, 43> kMaleHairFiles;
extern const std::array<std::string_view, 43> kFemaleHairFiles;
extern const std::array<std::string_view, 11> kDoramMaleHairFiles;
extern const std::array<std::string_view, 11> kDoramFemaleHairFiles;

}  // namespace ro::render::client_tables
