#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/style.h"

namespace ro::sim {

// LA TEINTURE D'UN CALQUE ÉCRITE EN CLAIR : ce qu'un joueur copie et colle pour
// partager la couleur d'un chapeau, d'une cape, de ses cheveux ou de sa peau.
//
// La forme est `L1:<calque>:<profil>:<masque>:` suivie de dix chiffres
// hexadécimaux par réglage non neutre, dans l'ordre des rangs : la teinte sur
// deux octets, la saturation, la luminosité et « absolu » sur un octet chacun.
// Le masque tient sur quatre chiffres, un bit par rang.
//
// 🔴 LE CODE NE PORTE PAS DE CLÉ. Une teinture d'objet appartient à
// l'exemplaire, et un code est une teinture SANS objet : coller le code d'un
// chapeau l'applique au chapeau qu'on porte. Au profil des classes de teinte,
// il garde son sens sur n'importe quel sprite ; au profil des rampes, il ne vaut
// sûrement que pour le même.
//
// ⚠ À CÔTÉ DU CODE V7, ET JAMAIS À SA PLACE : le corps garde son code, que les
// autres clients du serveur lisent. Le préfixe `L` ne peut pas commencer un
// code v7, dont le premier champ est un nombre : les deux ne se confondent pas.
//
// LE PROFIL MIXTE S'ÉCRIT `L2:`, et seulement quand le calque a un reste ou des
// retouches : `L2:<calque>:<profil>:<clé>:<masque>:<réglages>:<masque du
// reste>:<réglages du reste>:<nombre>:<retouches>`. La clé tient sur huit
// chiffres, le nombre sur deux, une retouche sur huit (l'index, puis rouge,
// vert, bleu).
//
// 🔴 CE CODE-LÀ PORTE LA CLÉ, parce qu'une retouche désigne un index : elle ne
// vaut que pour le sprite de cette clé. Collé ailleurs, le code n'apporte que
// ses pièces et son reste, et l'éditeur le dit.

// Ce qu'un code de calque porte.
struct StyleLayerCode {
  StyleLayerKind layer = StyleLayerKind::kHead;
  std::uint8_t profile = 0;
  std::array<StyleAdjust, kStyleLayerAdjustCount> adjusts{};
  // Le profil mixte. La clé vaut zéro dans un code `L1:`.
  std::uint32_t key = 0;
  std::array<StyleAdjust, kStyleLayerAdjustCount> rest{};
  std::vector<StyleRetouch> retouches;
};

// Les profils qu'un code sait écrire, et les rangs que chacun emploie.
inline constexpr std::uint8_t kStyleLayerCodeRampProfile = 1;
inline constexpr std::uint8_t kStyleLayerCodeHueClassProfile = 2;
inline constexpr std::size_t kStyleLayerCodeRampRanks = 8;

[[nodiscard]] std::string EncodeStyleLayerCode(const StyleLayerCode& code);

// Lit un code de calque, ou rien s'il est illisible, d'une autre version, ou
// s'il porte ce que le serveur refuserait : un calque inconnu, un profil que ce
// calque n'emploie pas, un rang au-delà de son profil, un réglage hors bornes.
//
// ⚠ LES ESPACES AUTOUR SONT TOLÉRÉS, comme pour le code v7 : un code collé
// depuis un chat en traîne presque toujours.
[[nodiscard]] std::optional<StyleLayerCode> DecodeStyleLayerCode(std::string_view text);

// Le nombre de rangs que ce calque emploie à ce profil : huit pour les rampes,
// treize pour les classes de teinte, un seul pour la peau ; zéro pour un couple
// que le serveur refuse.
[[nodiscard]] std::size_t StyleLayerRanks(StyleLayerKind layer, std::uint8_t profile) noexcept;

}  // namespace ro::sim
