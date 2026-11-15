// LES ÉMOTICONES ET LES EMOJI QU'ON PEUT ENVOYER, posés à l'ouverture d'une
// partie : voir `Game::OpenChatEmotions`.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/core/log.h"
#include "ro/render/emotion.h"
#include "ro/render/emotion_icons.h"
#include "ro/ui/chat_emote_names.h"
#include "ro/ui/system_fonts.h"

#include "chat/chat_web_stage.h"
#include "chat/emoji_catalog.h"
#include "chat/emotion_catalog.h"
#include "chat/emotion_commands.h"
#include "game/game.h"

namespace moonclient {

void Game::OpenChatEmotions() {
  // LES ÉMOTICONES QU'ON PEUT ENVOYER, une fois pour toutes : la grille les
  // montre TOUTES dès qu'elle s'ouvre, donc les étaler n'étalerait rien.
  //
  // 🔴 L'INTERSECTION DE DEUX LISTES, ET IL EN FAUT DEUX. Le CATALOGUE dit
  // ce que le joueur a le droit d'envoyer — une partie des numéros appartient au
  // jeu et aux PNJ. Le SPRITE dit ce que cette installation-ci sait dessiner.
  // Proposer ce qui manque à l'une des deux, c'est promettre une émoticone que
  // personne ne verra — pas même celui qui l'envoie.
  emotion_layers_.clear();
  emotion_frames_.clear();
  emotions_.clear();
  if (pieces_.emotion_icons->has_value()) {
    // ⚠ LA PLACE SE RÉSERVE D'ABORD, et il faut donc compter avant de remplir :
    // les vues pointent DANS ces tableaux, et une réallocation en cours de route
    // les laisserait pendantes. Le premier passage téléverse déjà tout ; le
    // second ne fait que relire ce qui est en cache.
    std::size_t layers = 0;
    std::size_t frames = 0;
    for (std::uint16_t emotion = 0; emotion < kKnownEmotions; ++emotion) {
      if (!EmotionIsOffered(emotion)) {
        continue;
      }
      const std::uint16_t count = (*pieces_.emotion_icons)->FrameCount(emotion);
      for (std::uint16_t frame = 0; frame < count; ++frame) {
        layers += (*pieces_.emotion_icons)->Layers(emotion, frame).size();
      }
      frames += count;
    }
    emotion_layers_.reserve(layers);
    emotion_frames_.reserve(frames);

    for (std::uint16_t emotion = 0; emotion < kKnownEmotions; ++emotion) {
      const std::uint16_t count = (*pieces_.emotion_icons)->FrameCount(emotion);
      const ro::render::EmotionBounds box = (*pieces_.emotion_icons)->ActionBounds(emotion);
      if (!EmotionIsOffered(emotion) || count == 0 || !box.usable()) {
        continue;
      }
      const std::size_t first_frame = emotion_frames_.size();
      for (std::uint16_t frame = 0; frame < count; ++frame) {
        const std::size_t first_layer = emotion_layers_.size();
        for (const ro::render::EmotionLayer& layer :
             (*pieces_.emotion_icons)->Layers(emotion, frame)) {
          ro::ui::ChatEmotionLayer posed{
              .id = static_cast<ImTextureID>(layer.texture),
              .tint = IM_COL32(layer.tint[0], layer.tint[1], layer.tint[2], layer.tint[3])};
          for (std::size_t point = 0; point < posed.corners.size(); ++point) {
            posed.corners[point] = ImVec2(layer.corners[point].x, layer.corners[point].y);
          }
          emotion_layers_.push_back(posed);
        }
        emotion_frames_.push_back(ro::ui::ChatEmotionFrame{
            .layers = std::span<const ro::ui::ChatEmotionLayer>(emotion_layers_)
                          .subspan(first_layer, emotion_layers_.size() - first_layer)});
      }
      // SON NOM `:nom:` SE LIT PAR L'ACTION DU SPRITE, et non par le numéro :
      // c'est ainsi que Discord et les autres clients le rangent.
      const std::optional<std::size_t> action = ro::render::EmotionAction(emotion);
      emotions_.push_back(ro::ui::ChatEmotion{
          .id = emotion,
          .name = EmotionName(emotion),
          // ⚠ LA COMMANDE DU JEU, ET NON LE NOM COURT : le
          // menu des macros écrit ce qui joue la bulle chez
          // TOUS les clients, pas un jeton de cette chatbox.
          .command = EmotionCommandOf(emotion),
          .frames = std::span<const ro::ui::ChatEmotionFrame>(emotion_frames_)
                        .subspan(first_frame, count),
          .frame_ms = (*pieces_.emotion_icons)->FrameMs(emotion),
          .box_min = ImVec2(box.min.x, box.min.y),
          .box_max = ImVec2(box.max.x, box.max.y),
          .chat_token = action.has_value() ? ro::ui::ChatEmoteName(*action) : std::string_view()});
    }
    ro::core::LogInfo("émoticônes : {} proposée(s), {} image(s), {} couche(s)", emotions_.size(),
                      emotion_frames_.size(), emotion_layers_.size());
  }
  // LE FIL LES PEINT AUSSI, rangées par action : le `:nom:` d'une ligne.
  pieces_.chat_web->NoteEmotions(emotions_);
  // LES EMOJI. ⚠ SEULEMENT SI LA POLICE EST LÀ : sans elle, la grille ne
  // rendrait que des losanges « glyphe absent », et l'on chercherait le défaut
  // du côté de la table alors qu'il est dans l'atlas. Un onglet absent se
  // comprend ; un onglet plein de carrés, non.
  emojis_.clear();
  if (!ro::ui::EmojiFontSource().empty()) {
    for (const EmojiCategory& group : EmojiCategories()) {
      // L'intertitre arrive déjà traduit : c'est le catalogue qui le fait, là
      // où les libellés sont des littéraux.
      emojis_.push_back(ro::ui::ChatEmojiGroup{.label = group.label, .items = group.items});
    }
  }
}

}  // namespace moonclient
