#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/skin_palette.h"

namespace ro::ui {

// LA SAISIE DE TALKIE BOX — la fenêtre où l'on écrit le message que la boîte
// dira quand elle se déclenchera.
//
// 🔴 SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : c'est la pose de la compétence
// qui l'ouvre, au moment où elle partirait. La compétence ne part qu'avec
// « OK », et le texte avec elle ; « Annuler » ne pose rien.
//
// ⚠ ELLE NE SAIT RIEN DE LA COMPÉTENCE : elle rend un texte ou un refus, et
// c'est l'appelant qui garde la case et le rang.

inline constexpr const char* kTalkboxWindowId = "talkbox_input";

// ── LA GÉOMÉTRIE DU CLIENT, en pixels d'art, depuis le coin de la fenêtre ───
inline constexpr float kTalkboxWindowWidth = 280.0F;
inline constexpr float kTalkboxWindowHeight = 120.0F;
// Sa place à l'ouverture : un X pensé pour un écran de 640 de large, recentré,
// et une hauteur rapportée à 480, remontée de soixante pixels.
inline constexpr float kTalkboxWindowLeft = 185.0F;
inline constexpr float kTalkboxWindowTopOn480 = 300.0F;
inline constexpr float kTalkboxWindowLift = 60.0F;
// La ligne qui demande le message, et son ombre d'un pixel.
inline constexpr ImVec2 kTalkboxPromptOrigin{16.0F, 22.0F};
// Le champ de saisie.
inline constexpr ImVec2 kTalkboxFieldOrigin{22.0F, 50.0F};
inline constexpr ImVec2 kTalkboxFieldSize{240.0F, 16.0F};
// Le fond du champ : celui de toute saisie de ce client. Un gris presque blanc
// se perdait sur le corps clair de la fenêtre.
inline constexpr ImU32 kTalkboxFieldColor = kSkinInputColor;
// Ce que le champ accepte, en caractères. ⚠ PLUS QUE CE QUI PART : le paquet
// n'en porte que vingt octets, et c'est lui qui coupe, comme au client.
inline constexpr std::size_t kTalkboxFieldChars = 78;
// La même fenêtre sert à Graffiti, dont le champ s'arrête à vingt.
inline constexpr std::size_t kGraffitiFieldChars = 20;
// Les deux boutons-images du client, « OK » puis « Annuler », sur la même ligne.
inline constexpr ImVec2 kTalkboxOkOrigin{189.0F, 96.0F};
inline constexpr ImVec2 kTalkboxCancelOrigin{234.0F, 96.0F};
inline constexpr ImVec2 kTalkboxButtonSize{42.0F, 20.0F};

// CE QUE LA FENÊTRE MONTRE, recomposé par l'appelant à chaque image.
//
// Les textes arrivent déjà dans la langue du joueur.
struct TalkboxView {
  // Vide pour Graffiti : sa fenêtre n'a pas de titre.
  std::string_view title;
  std::string_view prompt;
  // Ce que le champ accepte, en octets, plafonné à `kTalkboxFieldChars`.
  std::size_t field_chars = kTalkboxFieldChars;
};

struct TalkboxWindowState {
  // ⚠ CE N'EST PAS LE JOUEUR QUI LE POSE : il recopie la pose en attente.
  bool open = false;
  // Le front montant : la fenêtre se place, passe au premier plan et donne le
  // clavier à son champ à l'ouverture.
  bool was_open = false;
  std::array<char, kTalkboxFieldChars + 1> text{};
};

struct TalkboxWindowAction {
  // Le message, en UTF-8 : la pose part avec lui. La fenêtre se ferme.
  std::optional<std::string> confirmed;
  // « Annuler » : rien ne part.
  bool cancelled = false;
};

[[nodiscard]] TalkboxWindowAction DrawTalkboxWindow(TalkboxWindowState& state,
                                                    const TalkboxView& view, const SkinArt& art);

}  // namespace ro::ui
