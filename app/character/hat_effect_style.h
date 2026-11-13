#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "ro/render/effect_dye.h"
#include "ro/render/hat_effect_table.h"
#include "ro/sim/hat_effect_items.h"
#include "ro/sim/style.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/palette_effects.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/vfs.h"

#include "effects/hat_effect_dyes.h"

namespace ro::net {
class Connection;
}

namespace moonclient {

// LE RÉGLAGE D'UN EFFET DE CHAPEAU, tel que le fil le porte : le calque 8 sous
// le numéro de l'effet, au profil 4, son seul réglage au rang 0 et « Assombrir »
// dans l'octet de l'absolu.
[[nodiscard]] ro::sim::StyleLayerRecipe HatEffectRecipeOf(
    std::uint16_t number, const ro::ui::PaletteEffectSetting& setting);
// Et l'inverse : ce que le livre dit qu'un effet porte.
[[nodiscard]] ro::ui::PaletteEffectSetting HatEffectSettingOf(
    const ro::sim::StyleLayerRecipe& recipe);
// La teinture que ce réglage donne au dessin.
[[nodiscard]] ro::render::EffectDye HatEffectDyeOf(const ro::ui::PaletteEffectSetting& setting);
// Le nom lisible d'une constante `HAT_EF_` : sans préfixe, les soulignés en
// espaces.
[[nodiscard]] std::string HatEffectDisplayName(std::string_view constant);

// L'ONGLET « EFFETS » DE L'ÉDITEUR DE STYLE, côté jeu : les effets que le joueur
// affiche, ce que le serveur dit qu'ils portent, l'aperçu des réglages en cours
// sur le personnage, et l'envoi.
//
// ⚠ L'APERÇU NE VAUT QUE FENÊTRE OUVERTE : une teinture qu'on règle sans la
// valider ne doit pas rester sur le personnage une fois l'éditeur fermé.
class HatEffectStyle {
 public:
  // L'installation, pour la table des effets et leurs fichiers ; les objets qui
  // posent un effet, que le serveur annonce.
  void SetSources(ro::vfs::Vfs* vfs, const ro::sim::HatEffectItems* items) {
    vfs_ = vfs;
    items_ = items;
  }

  void Refresh(const ro::sim::World& world, ro::ui::PaletteEffectsState& state, bool connected,
               bool served, bool atelier_open, const ro::vfs::ItemTable* names,
               std::uint32_t now_ms);
  [[nodiscard]] const ro::ui::PaletteEffectsView& view() const noexcept { return view_; }

  // Vrai quand cet objet porté pose un effet affiché.
  [[nodiscard]] bool PosesEffect(std::uint32_t item_id) const;

  // ⚠ APRÈS LE DESSIN, sur les réglages que la vue montrait : ce qui part se
  // pose dans l'état de la fenêtre au prochain `Refresh`.
  void Apply(const ro::ui::PaletteEffectsAction& asked, ro::net::Connection& link,
             std::uint32_t now_ms);

  // LA TEINTURE QUE LE PANTIN MONTRE, par numéro : ce qu'on règle, sinon ce
  // qu'on porte.
  [[nodiscard]] const std::map<std::uint16_t, ro::render::EffectDye>& doll_dyes() const noexcept {
    return doll_dyes_;
  }
  // Les réglages en cours, posés sur le joueur ; rien fenêtre fermée.
  [[nodiscard]] std::span<const WornHatEffectDye> previews() const noexcept { return previews_; }
  void Close() { previews_.clear(); }

 private:
  // Vrai quand l'effet s'ajoute au fond, d'après les images clés de son fichier.
  [[nodiscard]] bool Additive(const ro::render::HatEffect& record);

  ro::vfs::Vfs* vfs_ = nullptr;
  const ro::sim::HatEffectItems* items_ = nullptr;
  std::optional<ro::render::HatEffectTable> table_;
  bool table_read_ = false;
  std::map<std::uint16_t, bool> additive_;
  std::vector<ro::ui::PaletteEffectRow> rows_;
  std::vector<std::uint32_t> posing_items_;
  std::vector<WornHatEffectDye> previews_;
  std::map<std::uint16_t, ro::render::EffectDye> doll_dyes_;
  // Les réglages que la vue montrait, et ceux qu'un envoi impose à la fenêtre.
  std::map<std::uint16_t, ro::ui::PaletteEffectSetting> drafts_;
  std::map<std::uint16_t, ro::ui::PaletteEffectSetting> sent_drafts_;
  ro::ui::PaletteEffectsView view_;
  // Le dernier envoi : quel effet, quand, et l'état qu'on en montre.
  std::uint16_t sent_number_ = 0;
  std::uint32_t sent_ms_ = 0;
  ro::ui::PaletteEffectSend send_ = ro::ui::PaletteEffectSend::kNone;
};

}  // namespace moonclient
