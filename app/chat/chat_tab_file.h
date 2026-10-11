#pragma once

#include <filesystem>
#include <string_view>

#include "chat/chat_tabs.h"
#include "game/save_folder.h"

namespace moonclient {

// LES ONGLETS DE CONVERSATION, SUR LE DISQUE.
//
// ⚠ LEUR PROPRE FICHIER, comme les touches et la barre. Ce sont une LISTE
// D'ENTRÉES, pas des valeurs nommées : la forme du fichier de réglages leur va
// mal, et le client fait la même séparation — ses onglets vivent dans
// `ChatWndInfo_U.lua`, pas avec ses options.
//
// ⚠ CLÉS ANGLAISES (ADR 0093) : `tabs`, puis par onglet `name`, `filter` — les
// vingt-huit cases des types, vrai ou faux, dans l'ordre des numéros —,
// `window`, `locked`, `content` et `look`.
inline constexpr std::string_view kChatTabFileName = "chat_tabs.yml";

[[nodiscard]] std::filesystem::path ChatTabFilePath(const std::filesystem::path& client_root);

// Lit les onglets. Un fichier absent, illisible ou vide laisse ceux d'origine —
// une conversation sans onglet n'aurait nulle part où s'écrire.
//
// ⚠ UN FICHIER SANS `tabs` LES LAISSE AUSSI, et c'est le sort d'un fichier aux
// anciennes clés : ses filtres nommaient d'autres cases que les types, et les
// relire rangerait les messages là où le joueur ne les attend pas.
void LoadChatTabs(const std::filesystem::path& client_root, ChatTabs& into);

// Écrit les onglets. Rend faux si le fichier n'a pas pu être écrit.
[[nodiscard]] bool SaveChatTabs(const std::filesystem::path& client_root, const ChatTabs& from);

}  // namespace moonclient
