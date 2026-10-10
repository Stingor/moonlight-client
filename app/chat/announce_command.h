#pragma once

#include <string>
#include <string_view>

namespace moonclient {

// ── `/where`, `/b`, `/nb`, `/lb`, `/nlb` : LE LIEU ET LES ANNONCES ──────────
//
//     /where              écrit la carte et la case du joueur, chez lui seul
//     /b <message>        annonce à tout le serveur, précédée du nom
//     /nb <message>       de même, sans le nom
//     /lb <message>       annonce à la carte seule, précédée du nom
//     /nlb <message>      de même, sans le nom
//
// ⚠ LES ANNONCES SONT AU STAFF, et le serveur en décide : il les rejoue en
// `@kami` / `@lkami` sous les droits du groupe, et refuse sans un mot. Le nom,
// lui, c'est au client de l'écrire — le serveur n'en ajoute pas.

enum class AnnounceCommandKind { kNone, kWhere, kBroadcast, kLocalBroadcast };

struct AnnounceCommand {
  AnnounceCommandKind kind = AnnounceCommandKind::kNone;
  // L'annonce porte le nom de qui la fait : `/b` et `/lb`, pas `/nb` ni `/nlb`.
  bool named = false;
  // Ce qui suit la commande, sans l'espace qui l'en sépare. Vide pour `/where`.
  std::string message;
};

// Le mot de commande se compare sans tenir compte de la casse, comme le client.
// `/bonjour` n'est pas `/b` : la commande s'arrête à l'espace.
[[nodiscard]] AnnounceCommand ReadAnnounceCommand(std::string_view line);

// Le texte qui part : « Nom : message », ou le message seul.
[[nodiscard]] std::string AnnouncedText(const AnnounceCommand& command, std::string_view speaker);

// La ligne de `/where` : « prontera 150 180 ».
[[nodiscard]] std::string WhereLine(std::string_view map_name, int x, int y);

}  // namespace moonclient
