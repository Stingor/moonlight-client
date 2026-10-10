#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/chat_strip.h"

namespace ro::ui {

// ── CE QU'UNE LIGNE DU FIL PEINT EN PLUS DE SON TEXTE ───────────────────────
//
// Les images posées dans une ligne — une emote du jeu, l'icône d'un objet lié,
// la vignette d'une image relayée — et ce qu'on sait d'une adresse web. `ui` ne
// télécharge rien et ne décode rien : `app` lui donne des textures prêtes, ou
// l'état d'une image qui ne l'est pas encore.
//
// ⚠ TOUT CE QUI EST ICI NE VIT QUE LE TEMPS DE L'IMAGE, comme le texte des
// lignes : les vues et les textures appartiennent à `app`.

enum class ChatPictureState : std::uint8_t { kUnknown, kPending, kReady, kFailed };

// Une image d'une animation, et le temps qu'elle reste à l'écran.
struct ChatPictureFrame {
  ImTextureID texture = 0;
  std::uint32_t delay_ms = 0;
};

enum class ChatInlineKind : std::uint8_t { kGameEmote, kItemIcon, kPicture };

// UNE IMAGE POSÉE DANS LE TEXTE D'UNE LIGNE.
struct ChatInlineImage {
  // Le repli textuel qu'elle remplace ; une longueur nulle veut dire que
  // l'image s'insère devant `offset`, sans rien remplacer.
  std::uint32_t offset = 0;
  std::uint32_t length = 0;
  ChatInlineKind kind = ChatInlineKind::kGameEmote;
  // `kGameEmote` : l'action du sprite des émoticones.
  std::uint16_t emote_action = 0;
  // `kItemIcon` : l'objet dont on peint l'icône.
  std::uint32_t item_id = 0;
  // `kPicture` : son rang dans la table de l'image, qui donne son adresse.
  std::uint32_t picture_key = 0;
  // `kPicture` : vrai, elle tient dans la rangée comme une icône d'objet — un
  // avatar devant un pseudo ; faux, elle prend la hauteur des vignettes.
  bool line_high = false;
  ChatPictureState state = ChatPictureState::kUnknown;
  ImTextureID texture = 0;
  float width_px = 0.0F;
  float height_px = 0.0F;
  // Vide pour une image fixe.
  std::span<const ChatPictureFrame> frames;
};

// CE QU'ON SAIT D'UNE ADRESSE WEB MONTRÉE, pour son infobulle et son menu.
struct ChatUrlFacts {
  std::string_view host;
  // L'aperçu est permis : hôte admis, ou accord pour cette fois.
  bool allowed = false;
  // L'infobulle dit l'état de l'aperçu : aperçu au survol activé, ou accord
  // explicite. Sinon elle ne montre que l'adresse.
  bool statuses = false;
  ChatPictureState preview = ChatPictureState::kUnknown;
  ImTextureID texture = 0;
  float width_px = 0.0F;
  float height_px = 0.0F;
  std::span<const ChatPictureFrame> frames;
};

// CE QUE LE FIL A POUR PEINDRE LE TEXTE RICHE, posé à chaque image.
struct ChatRichArt {
  // Les polices des styles, posées par l'étage d'interface. Nulles, OU ÉGALES À
  // LA POLICE DU FIL — un étage qui n'a pas trouvé de fichier gras ou italique
  // y repose la police ordinaire —, le fil simule le gras et l'italique.
  ImFont* bold = nullptr;
  ImFont* italic = nullptr;
  ImFont* code = nullptr;
  // Une case par action du sprite ; des images vides veulent dire absente.
  std::span<const ChatEmotion> emotes_by_action;
  bool emote_images = true;
  bool item_icons = true;
  // La hauteur des vignettes, en pixels ; zéro les éteint.
  float picture_pixels = 0.0F;
  // SORTIE : les clés des vignettes posées cette image et pas encore prêtes.
  std::vector<std::uint32_t>* seen_pictures = nullptr;
};

// L'image d'une animation à montrer après `elapsed_ms`, en boucle. Un délai
// sous 20 ms compte pour 100 ms, comme dans les navigateurs.
[[nodiscard]] std::size_t ChatPictureFrameAt(std::span<const ChatPictureFrame> frames,
                                             std::uint64_t elapsed_ms);

}  // namespace ro::ui
