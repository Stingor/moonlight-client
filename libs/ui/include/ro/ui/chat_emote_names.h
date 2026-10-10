#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ro::ui {

// ── LES NOMS `:nom:` DES ÉMOTICONES DU JEU ──────────────────────────────────
//
// 🔴 UN CONTRAT AVEC DISCORD ET AVEC LE SITE : le nom qui voyage dans une ligne
// doit désigner la même émoticone chez tous les clients et dans les GIF du
// relai. La table est rangée par ACTION du sprite des émoticones, pas par
// numéro de protocole.

inline constexpr std::size_t kChatEmoteActions = 79;

// Vide : cette action n'a pas de nom.
[[nodiscard]] std::string_view ChatEmoteName(std::size_t action);
[[nodiscard]] std::optional<std::uint16_t> ChatEmoteAction(std::string_view name);

}  // namespace ro::ui
