#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/style_outfit.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES TENUES : le code court et la garde-robe du compte (`CZ 0x0F61`,
// `ZC 0x0F62`). Elles voyagent sous le bit des calques, et ne partent qu'une
// fois la cadence des tenues dite par le serveur (règle 22).
//
// UNE TENUE :
//   [version:1][corps:1][repli:4]
//   puis corps × [clé:4][palette:2][palette_cheveux:2][coupe:2][réglages:8×5]
//   puis [nombre:1] et nombre × entrée de calque à la forme du ZC 0x0F5F.
//
// 🔴 CE QUE LE SERVEUR REND N'EST QU'UNE PROPOSITION : rien ici n'écrit sur le
// personnage. L'éditeur la montre, et le joueur la valide par les envois
// ordinaires.

inline constexpr std::uint8_t kOutfitVersion = 1;
inline constexpr std::size_t kOutfitHead = 6;
inline constexpr std::size_t kOutfitBodyBytes = 50;
inline constexpr std::size_t kOutfitEntriesMost = 12;
inline constexpr std::size_t kOutfitBytesMost = 1024;
inline constexpr std::size_t kOutfitNameMost = 24;
inline constexpr std::size_t kWardrobeSlotsMost = 16;

// La tenue en octets, ou la raison de la refuser : trop de corps ou
// d'entrées, une clé nulle ou répétée, un repli qui n'est pas l'un des corps,
// une entrée hors de son calque, plus de `kOutfitBytesMost` octets.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeOutfit(const sim::StyleOutfit& outfit);
// 🔴 TOUT OU RIEN : une structure fausse rend une erreur. Une entrée d'un calque
// ou d'un profil inconnus, elle, est seulement enjambée.
[[nodiscard]] core::Result<sim::StyleOutfit> DecodeOutfit(std::span<const std::uint8_t> raw);

// UN CODE TEL QUE LE JOUEUR LE COLLE : espaces et tirets retirés, minuscules
// relevées. Rien s'il ne fait pas huit signes de l'alphabet des codes.
[[nodiscard]] std::optional<std::string> NormalizeOutfitCode(std::string_view typed);
// Le code à montrer : deux groupes de quatre, « K7QM-3XPA ».
[[nodiscard]] std::string ShowOutfitCode(std::string_view code);

// LA DEMANDE, ou la raison de la refuser avant qu'elle ne parte.
struct OutfitRequest {
  sim::OutfitAction action = sim::OutfitAction::kWardrobeList;
  std::uint8_t slot = 0;
  // Normalisé : voir `NormalizeOutfitCode`.
  std::string code;
  std::string name;
  std::optional<sim::StyleOutfit> outfit;
};
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeOutfitRequest(
    const OutfitRequest& request);
[[nodiscard]] core::Result<void> SendOutfitRequest(Connection& link, const OutfitRequest& request);

// `ZC 0x0F62` : la réponse, rangée au guichet du monde. Une liste remplace la
// garde-robe connue ; une tenue illisible fait ignorer la trame.
[[nodiscard]] FrameOutcome ApplyOutfitFrame(const Frame& frame, sim::World& world);

}  // namespace ro::net
