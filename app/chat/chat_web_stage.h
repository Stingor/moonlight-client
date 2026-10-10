#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/web_preview_policy.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/chat_rich_art.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/chat_window.h"

#include "chat/chat_image_cache.h"
#include "chat/chat_tab_view.h"
#include "options/chat_web_options.h"

namespace moonclient {

// ── LES ADRESSES ET LES IMAGES DU CHAT, DANS LA BOUCLE ──────────────────────
//
// Ce que le fil sait d'une adresse à chaque image, les vignettes qu'il demande,
// et ce que le joueur fait d'un lien web : confirmer, ouvrir, admettre un site,
// agrandir, enregistrer. Le téléchargement est à `ChatImageCache`.
class ChatWebStage {
 public:
  // Les réglages du joueur, relus à chaque image.
  void Configure(const ChatWebSetup& setup);
  // Les émoticones du jeu, rangées par action pour le fil.
  void NoteEmotions(std::span<const ro::ui::ChatEmotion> emotions);
  // Pose ce que le fil peint en plus du texte, et vide les faits de l'image.
  void Compose(ro::ui::ChatWindows& chat, std::uint32_t now_ms);
  // Ce qu'on sait d'une adresse écrite, pour cette image ; nul sans adresse.
  [[nodiscard]] const ro::ui::ChatUrlFacts* FactsOf(std::string_view written);
  // L'état et la texture de la vignette de cette adresse.
  void FillPicture(std::string_view url, ro::ui::ChatInlineImage& image);
  // Demande les vignettes que le fil a posées sans les avoir.
  void RequestSeen(const ChatTabsView& tabs);
  // Une adresse survolée : son aperçu part après le délai de survol.
  void NoteHovered(std::string_view written, std::uint32_t now_ms);
  // Un geste sur une adresse : la confirmation, l'aperçu, la visionneuse.
  void Follow(const ro::ui::ChatLinkAsk& asked, std::string_view written,
              ro::ui::ChatWindows& chat);
  // Les réponses des confirmations et de la visionneuse.
  void Answer(ro::ui::ChatWindows& chat, const std::filesystem::path& client_root);
  // Un site que le joueur vient d'admettre, rendu une fois.
  [[nodiscard]] std::optional<std::string> TakeHostToAllow();
  [[nodiscard]] std::optional<std::string> TakeHostToOpenAlways();
  [[nodiscard]] std::optional<ChatSaveOutcome> TakeSaved();

 private:
  // CE QU'ON SAIT D'UNE ADRESSE ÉCRITE : sa forme ouvrable, lue une fois, et
  // l'hôte que la vue regarde. ⚠ L'HÔTE VIT DANS LE NŒUD DE LA TABLE, qui ne
  // bouge pas : la vue reste juste tant que l'entrée vit.
  struct KnownUrl {
    std::string openable;
    std::string host;
    ro::ui::ChatUrlFacts facts;
  };

  // L'entrée d'une adresse écrite, lue et jugée à sa première demande.
  [[nodiscard]] KnownUrl& Known(std::string_view written);
  // Rebâtit la liste blanche et la passe au cache : les sites du joueur, puis
  // ce qu'il a accordé pour une fois.
  void RebuildAllowList();
  // Accorde cette adresse pour une fois, puis rebâtit la liste.
  void AllowOnce(const std::string& openable);
  // L'état et les textures d'une image prête, dans les champs d'une vue.
  void FillFrom(std::string_view openable, ro::ui::ChatPictureState& state, ImTextureID& texture,
                float& width_px, float& height_px,
                std::span<const ro::ui::ChatPictureFrame>& frames) const;
  // Ouvrir une adresse : directement, ou par la confirmation.
  void Open(std::string_view written, ro::ui::ChatWindows& chat);
  // Un site que le joueur a dit d'ouvrir sans confirmation.
  [[nodiscard]] bool OpensWithoutAsking(std::string_view host) const;

  ChatImageCache cache_;
  // LES FAITS DES ADRESSES VUES, gardés d'une image à l'autre : les vues des
  // lignes pointent dedans. ⚠ OUBLIÉS AU `Compose` qui suit un changement —
  // du cache, de la liste, des réglages —, et seulement là : aucune vue de
  // l'image précédente ne vit encore.
  std::map<std::string, KnownUrl, std::less<>> facts_;
  // Ce sur quoi les faits ont été lus : la génération du cache, et celle de
  // l'étape, qui croît quand la liste ou un réglage qui les juge change.
  std::uint64_t facts_cache_generation_ = 0;
  std::uint64_t facts_generation_ = 0;
  std::uint64_t generation_ = 1;

  // Les réglages qui servent ici. ⚠ LES LISTES DE SITES N'Y SONT PAS : elles
  // se lisent par `*_hosts_seen_`, recopiées seulement quand elles changent.
  ChatWebSetup setup_;
  // Les deux listes du joueur telles que le réglage les écrit, pour ne les
  // redécouper que quand elles changent.
  std::string preview_hosts_seen_;
  std::string open_hosts_seen_;
  std::vector<std::string> open_hosts_;
  ro::core::PreviewAllowList allow_;
  // Les adresses accordées pour cette fois, rejouées sur chaque liste neuve.
  std::vector<std::string> allowed_once_;

  // Les émoticones du jeu, une case par action du sprite.
  std::vector<ro::ui::ChatEmotion> emotes_by_action_;
  // Les vignettes que le fil a posées sans les avoir, à cette image.
  std::vector<std::uint32_t> seen_pictures_;

  // L'ADRESSE SURVOLÉE, ouvrable, et depuis quand ; vide quand rien ne l'est.
  // ⚠ UN SURVOL QUI MANQUE UNE IMAGE RECOMMENCE : le délai compte un survol
  // continu, pas une adresse croisée au passage.
  std::string hovered_;
  std::uint32_t hovered_since_ms_ = 0;
  bool hovered_this_frame_ = false;

  std::optional<std::string> host_to_allow_;
  std::optional<std::string> host_to_open_always_;
};

}  // namespace moonclient
