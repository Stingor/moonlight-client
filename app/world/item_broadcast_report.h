#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ro/sim/item_news.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// CE QUE LE SERVEUR ANNONCE À TOUS SUR DES OBJETS : un objet rare obtenu, un
// raffinage, un grade.
//
// Chaque annonce s'écrit dans la conversation ET se pose en bulle en haut de
// l'écran, comme chez le client. L'objet s'y nomme par un LIEN : le fil et la
// bulle le décorent en nom composé, et le fil le rend cliquable.

// Les couleurs du client, en `0xRRGGBB` : l'orange des annonces d'objet, et le
// jaune de la ligne d'un objet rare dans la conversation.
inline constexpr std::uint32_t kItemBroadcastColor = 0xFFB062;
inline constexpr std::uint32_t kRareObtainChatColor = 0xFFFF00;

// LA LETTRE D'UN GRADE, du rang du fil : D, C, B, A, R, S, SS. Vide hors de
// cette échelle, comme chez le client.
[[nodiscard]] std::string_view GradeLetter(int grade);

// Les phrases, telles que le client les écrit.
[[nodiscard]] std::string RareObtainLine(const ro::sim::RareItemObtain& obtained);
[[nodiscard]] std::string RefineAnnounceLine(const ro::sim::RefineAnnounce& refined);
[[nodiscard]] std::string GradeAnnounceLine(const ro::sim::GradeAnnounce& graded);

// Prend les annonces que le réseau a déposées, et les écrit : une ligne dans la
// conversation, une bulle en haut de l'écran. ⚠ DANS LE POMPAGE RÉSEAU, comme
// les autres nouvelles d'objets.
void ReportItemBroadcasts(ro::sim::ItemNews& news, ro::sim::World& world);

}  // namespace moonclient
