#include "world/npc_talk_view.h"

#include <algorithm>

#include "ro/net/npc_dialog.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/npc_dialog.h"
#include "ro/sim/world.h"

#include "game/skin_image_of.h"
#include "options/setting_links.h"

namespace moonclient {
namespace {

// Ce que `sim` attend, traduit en ce que `ui` connaît. Deux couches qui ne se
// connaissent pas ne partagent pas un type, et la conversion est ce qui tient la
// frontière.
[[nodiscard]] ro::ui::NpcDialogPrompt PromptOf(ro::sim::NpcPrompt prompt) {
  switch (prompt) {
    case ro::sim::NpcPrompt::kNext:
      return ro::ui::NpcDialogPrompt::kNext;
    case ro::sim::NpcPrompt::kClose:
      return ro::ui::NpcDialogPrompt::kClose;
    case ro::sim::NpcPrompt::kMenu:
      return ro::ui::NpcDialogPrompt::kMenu;
    case ro::sim::NpcPrompt::kNumber:
      return ro::ui::NpcDialogPrompt::kNumber;
    case ro::sim::NpcPrompt::kText:
      return ro::ui::NpcDialogPrompt::kText;
    case ro::sim::NpcPrompt::kNone:
      break;
  }
  return ro::ui::NpcDialogPrompt::kNone;
}

// LE NOM VISIBLE D'UN PNJ.
//
// ⚠ CE QUI SUIT LE CROISILLON NE SE MONTRE PAS : un script nomme ses PNJ
// « Kafra#prontera » pour les distinguer entre eux, et cette partie-là est son
// affaire, pas celle du joueur. Le client officiel la coupe aussi.
[[nodiscard]] std::string ShownName(std::string_view name) {
  const std::size_t hidden = name.find('#');
  return std::string(hidden == std::string_view::npos ? name : name.substr(0, hidden));
}

// ── Les bornes d'un média ───────────────────────────────────────────────────
// Le contenu vient du serveur, donc du staff — mais un chiffre mal tapé dans un
// script ne doit pas produire une image plein écran dans une fenêtre de quatre
// cent soixante pixels.
constexpr float kMediaMaxHeightPixels = 220.0F;
// La hauteur d'un sprite de monstre posé au fil du texte, quand le script n'en
// impose pas.
constexpr float kInlineSpritePixels = 48.0F;
// Ce qu'on réserve à une image dont on n'a pas su lire les dimensions : assez
// pour porter son cartouche d'absence, pas plus.
constexpr float kNoticeWidthPixels = 300.0F;
constexpr float kNoticeHeightPixels = 22.0F;

// À l'échelle, sans jamais déformer ni dépasser.
[[nodiscard]] ro::ui::DialogMediaSize FitBox(float width, float height, float most_wide,
                                             float most_tall) {
  if (width <= 0.0F || height <= 0.0F) {
    return {};
  }
  float scale = 1.0F;
  if (width > most_wide) {
    scale = most_wide / width;
  }
  if (height * scale > most_tall) {
    scale = most_tall / height;
  }
  return ro::ui::DialogMediaSize{.width = width * scale, .height = height * scale};
}

}  // namespace

std::size_t NpcTalkView::VignetteHash::operator()(const VignetteKey& key) const noexcept {
  // Les trois champs se mêlent : deux tailles d'un même monstre ne doivent pas
  // tomber dans le même seau. ⚠ ON NE HACHE PAS POUR COMPARER — l'égalité, elle,
  // compare les champs.
  std::size_t mixed = std::hash<std::uint32_t>{}(key.monster_id);
  mixed ^= std::hash<int>{}(key.width) + 0x9E3779B9U + (mixed << 6U) + (mixed >> 2U);
  mixed ^= std::hash<int>{}(key.height) + 0x9E3779B9U + (mixed << 6U) + (mixed >> 2U);
  return mixed;
}

const ro::render::IconTexture* NpcTalkView::MonsterVignette(std::uint32_t monster_id, int width,
                                                            int height) {
  if (wardrobe_ == nullptr || monster_id == 0 || width <= 0 || height <= 0) {
    return nullptr;
  }
  const VignetteKey key{.monster_id = monster_id, .width = width, .height = height};
  auto found = monsters_.find(key);
  if (found == monsters_.end()) {
    found = monsters_.try_emplace(key, *wardrobe_).first;
  }
  ro::render::ActorVignette::Pose pose;
  // ⚠ NI JOUEUR NI OBJET : un monstre est un sprite UNIQUE, dont la classe tient
  // lieu de métier. Voir `render::ActorLook`.
  pose.look.player = false;
  pose.look.item = false;
  pose.look.job = monster_id;
  pose.animation = ro::render::kAnimationIdle;
  // De face : c'est ainsi qu'on montre un monstre qu'on nomme, et non de dos.
  pose.direction = 0;
  pose.animated = true;
  pose.elapsed_ms = static_cast<float>(now_ms_);
  pose.box.width = width;
  pose.box.height = height;
  const ro::render::IconTexture& drawn = found->second.Get(pose);
  return drawn.usable() ? &drawn : nullptr;
}

const ro::ui::NpcDialogView* NpcTalkView::Refresh(const ro::sim::World& world, std::uint32_t now_ms,
                                                  bool menu_search) {
  now_ms_ = now_ms;
  if (!world.npc_dialog().has_value()) {
    // ⚠ LES VIGNETTES MEURENT AVEC LA CONVERSATION : chacune tient une texture,
    // et une page de trente monstres en tiendrait trente jusqu'à la fin de la
    // partie.
    monsters_.clear();
    return nullptr;
  }
  const ro::sim::NpcDialog& dialog = *world.npc_dialog();

  name_.clear();
  if (const ro::sim::Actor* const npc = world.Find(dialog.npc_id); npc != nullptr) {
    name_ = ShownName(npc->name);
  }

  options_.clear();
  options_.reserve(dialog.options.size());
  for (const ro::sim::NpcMenuOption& option : dialog.options) {
    options_.push_back(option.text);
  }

  view_ = ro::ui::NpcDialogView{};
  view_.name = name_;
  view_.lines = dialog.lines;
  // ⚠ LE SPAN SE PREND MAINTENANT, le vecteur étant rempli.
  view_.options = options_;
  view_.prompt = PromptOf(dialog.prompt);
  view_.awaiting_reply = dialog.awaiting_reply;
  view_.page = dialog.page;
  view_.menu = dialog.menu;
  view_.text_limit = ro::net::kNpcTextAnswerMaxBytes;
  view_.menu_search = menu_search;
  return &view_;
}

ro::ui::NpcDialogArt NpcTalkView::art() {
  ro::ui::NpcDialogArt art;
  art.icon_owner = icons_;
  art.icon_aspect = [](std::uint32_t item_id, void* owner) -> float {
    auto* const icons = static_cast<ro::render::ItemIcons*>(owner);
    if (icons == nullptr) {
      return 0.0F;
    }
    const ro::render::IconTexture& icon = icons->Get(item_id);
    if (!icon.usable() || icon.height_px == 0) {
      return 0.0F;
    }
    return static_cast<float>(icon.width_px) / static_cast<float>(icon.height_px);
  };
  art.icon_image = [](std::uint32_t item_id, void* owner) -> ro::ui::SkinImage {
    auto* const icons = static_cast<ro::render::ItemIcons*>(owner);
    if (icons == nullptr) {
      return {};
    }
    return SkinImageOf(icons->Get(item_id));
  };

  art.media_owner = this;
  // 🔴 CETTE MESURE DOIT ÊTRE DÉTERMINISTE POUR UN ÉTAT DONNÉ : le dessin la
  // redemande et la compare au rectangle déjà placé. Deux réponses différentes
  // dans le même état remettraient la page en page à chaque image.
  art.media_measure = [](const ro::ui::DialogRun& media, float room,
                         void* owner) -> ro::ui::DialogMediaSize {
    auto* const self = static_cast<NpcTalkView*>(owner);
    const float most_wide = std::max(room, 8.0F);
    // Ce que le script impose fait foi — borné quand même : un zéro de trop ne
    // doit pas produire une image plein écran.
    if (media.wanted_width > 0.0F && media.wanted_height > 0.0F) {
      return FitBox(media.wanted_width, media.wanted_height, most_wide, kMediaMaxHeightPixels);
    }
    if (media.media == ro::ui::DialogMedia::kMonsterSprite) {
      // ⚠ UN CARRÉ, ET NON LA TAILLE DU SPRITE : la vignette est composée POUR
      // la boîte qu'on lui donne, et lui demander sa taille avant de la composer
      // serait circulaire.
      const float side = std::min(kInlineSpritePixels, most_wide);
      return ro::ui::DialogMediaSize{.width = side, .height = side};
    }
    if (self == nullptr || self->images_ == nullptr) {
      return {};
    }
    const ro::render::IconTexture& image = self->images_->Get(media.source);
    if (!image.usable()) {
      // Rien à montrer, mais quelque chose à DIRE : la place du cartouche.
      return ro::ui::DialogMediaSize{.width = std::min(kNoticeWidthPixels, most_wide),
                                     .height = kNoticeHeightPixels};
    }
    return FitBox(static_cast<float>(image.width_px), static_cast<float>(image.height_px),
                  most_wide, kMediaMaxHeightPixels);
  };

  art.draw_media = [](const ro::ui::DialogFragment& fragment, std::string_view source,
                      ImVec2 top_left, ImVec2 bottom_right, void* owner) -> bool {
    auto* const self = static_cast<NpcTalkView*>(owner);
    if (self == nullptr) {
      return false;
    }
    if (fragment.media == ro::ui::DialogMedia::kMonsterSprite) {
      const ro::render::IconTexture* const drawn =
          self->MonsterVignette(fragment.monster_id, static_cast<int>(bottom_right.x - top_left.x),
                                static_cast<int>(bottom_right.y - top_left.y));
      if (drawn == nullptr) {
        return false;
      }
      ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(drawn->texture.name()),
                                           top_left, bottom_right);
      return true;
    }
    if (self->images_ == nullptr) {
      return false;
    }
    const ro::render::IconTexture& image = self->images_->Get(source);
    if (!image.usable()) {
      return false;
    }
    ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(image.texture.name()), top_left,
                                         bottom_right);
    return true;
  };

  // ── CE QUE LE LECTEUR SAIT NOMMER ────────────────────────────────────────
  //
  // 🔴 C'EST CE QUI DÉCIDE QU'IL Y A UN LIEN. Une clé de réglage que ce client
  // ne connaît pas, un état absent de son installation : pas de lien, et le
  // libellé que le script transporte reste en texte simple.
  art.words.owner = this;
  art.words.setting_label = [](std::string_view key, void* owner) -> std::string {
    const auto* const self = static_cast<const NpcTalkView*>(owner);
    return SettingDestinationLabel(key, self != nullptr ? self->setting_reach_ : SettingReach{});
  };
  art.words.status_label = [](std::uint32_t effect_id, void* owner) -> std::string {
    auto* const self = static_cast<NpcTalkView*>(owner);
    if (self == nullptr || self->statuses_ == nullptr) {
      return {};
    }
    return std::string(self->statuses_->NameOf(effect_id));
  };

  art.draw_portrait = [](std::uint32_t monster_id, ImVec2 top_left, ImVec2 bottom_right,
                         void* owner) -> bool {
    auto* const self = static_cast<NpcTalkView*>(owner);
    if (self == nullptr) {
      return false;
    }
    const ro::render::IconTexture* const drawn =
        self->MonsterVignette(monster_id, static_cast<int>(bottom_right.x - top_left.x),
                              static_cast<int>(bottom_right.y - top_left.y));
    if (drawn == nullptr) {
      // ⚠ RIEN N'EST RÉSERVÉ QUAND LE SPRITE MANQUE : mieux vaut le texte pleine
      // largeur qu'une colonne vide.
      return false;
    }
    ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(drawn->texture.name()), top_left,
                                         bottom_right);
    return true;
  };
  return art;
}

}  // namespace moonclient
