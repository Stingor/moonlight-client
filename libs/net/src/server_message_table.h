#pragma once

#include <cstdint>

// LA TABLE DES MESSAGES PAR NUMÉRO, vue du décodeur. Voir `ro/net/server_message.h`.
//
// En-tête PRIVÉ : seul le décodeur des messages du serveur la lit.
namespace ro::net {

// Le texte FRANÇAIS du message, clé de sa traduction, ou nul quand la table ne
// porte pas ce numéro. ⚠ À traduire par `core::Tr` au moment d'écrire.
[[nodiscard]] const char* ServerMessageFrench(std::uint16_t message_id);

// Vrai pour un refus qui s'écrit en rouge, quelle que soit la couleur que le
// paquet demande.
[[nodiscard]] bool ServerMessageIsRed(std::uint16_t message_id);

}  // namespace ro::net
