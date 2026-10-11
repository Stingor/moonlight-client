#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── SIGNALER UN JOUEUR ──────────────────────────────────────────────────────
//
// La fenêtre « Report a User » du client d'origine : deux motifs au choix, un
// texte libre de cent caractères, et l'avertissement qu'un abus se sanctionne.
// « Envoyer » et « Annuler » au pied. La fenêtre ne connaît ni le joueur ni le
// réseau : `app` y pose le nom visé et lit, à l'envoi, le motif et le texte.

inline constexpr const char* kPlayerReportWindowId = "signalement_joueur";

// Le texte libre, en CARACTÈRES : chacun pèse un octet sur le fil. Le tampon
// d'ImGui, lui, est en UTF-8, où un caractère pèse jusqu'à quatre octets.
inline constexpr std::size_t kPlayerReportMessageMostChars = 100;
inline constexpr std::size_t kUtf8MostBytesPerChar = 4;
inline constexpr std::size_t kPlayerReportMessageBufferBytes =
    (kPlayerReportMessageMostChars * kUtf8MostBytesPerChar) + 1;

// Les deux motifs, dans l'ordre du client d'origine : c'est le numéro du fil.
enum class PlayerReportMotive : std::uint8_t {
  kIllegalProgram = 0,
  kBugAbuse = 1,
};

struct PlayerReportWindowState {
  bool open = false;
  // Mise au premier plan à la prochaine image : un nouveau joueur s'y montre.
  bool raise = false;
  // Le joueur visé, tel que le menu l'a relevé, en UTF-8.
  std::string target_name;
  PlayerReportMotive motive = PlayerReportMotive::kIllegalProgram;
  std::array<char, kPlayerReportMessageBufferBytes> message{};
};

struct PlayerReportAnswer {
  // Le signalement part : motif et texte sont dans l'état.
  bool send = false;
};

// Combien de caractères compte un texte UTF-8.
[[nodiscard]] std::size_t Utf8CharCount(std::string_view utf8) noexcept;

[[nodiscard]] PlayerReportAnswer DrawPlayerReportWindow(PlayerReportWindowState& state,
                                                        const SkinArt& art);

}  // namespace ro::ui
