#pragma once

#include <array>
#include <string_view>

namespace moonclient {

// LES QUATRE NOMS DES COMMANDES QUI TAISENT LA PAROLE, à part de leur lecteur
// pour se lire sans les options : la liste des commandes du chat et son essai
// les comparent à la table `slash_commands.cpp`.
//
// Par paires : le nom long, puis son alias court, qui porte la même bascule.
inline constexpr std::string_view kTalkHiddenCommand = "/notalkmsg";
inline constexpr std::string_view kTalkHiddenShortCommand = "/nm";
inline constexpr std::string_view kTalkAndGuildHiddenCommand = "/notalkmsg2";
inline constexpr std::string_view kTalkAndGuildHiddenShortCommand = "/nm2";

inline constexpr std::array<std::string_view, 4> kTalkFilterWords = {
    kTalkHiddenCommand, kTalkHiddenShortCommand, kTalkAndGuildHiddenCommand,
    kTalkAndGuildHiddenShortCommand};

}  // namespace moonclient
