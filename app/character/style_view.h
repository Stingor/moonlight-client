#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/actor_thumbnails.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/palette_editor.h"

#include "character/style_doll.h"
#include "character/style_session.h"
#include "character/style_store.h"

namespace moonclient {

// L'ÉDITEUR DE STYLE, VU DE LA PARTIE.
//
// ⚠ IL CROISE CE QUE NI `ui` NI `render` NE PEUVENT CROISER : les pièces du
// corps porté viennent de la garde-robe, ce que le personnage PORTE vient du
// livre des styles, et la traduction d'une couleur en réglage demande la
// palette de BASE, qui n'appartient à aucune des deux.
//
// 🔴 LA RECETTE N'A QU'UN ÉCRIVAIN, la `StyleSession` : la fenêtre rend des
// intentions, et c'est ici qu'elles deviennent des appels. Ce qui se lit sur la
// recette — pastilles, code, palette du pantin — ne se refait qu'au changement
// de sa révision.
//
// 🔴 RIEN NE TOUCHE AU PERSONNAGE AVANT LA VALIDATION : l'aperçu vit dans le
// pantin, sous un style à part — voir `StyleDoll`.
class StyleView {
 public:
  // La garde-robe peut manquer : une installation sans sprites n'a pas de
  // pièces à régler, et la fenêtre le dit plutôt que de mentir.
  //
  // `client_root` désigne l'installation, sous laquelle vivent les préréglages
  // et le brouillon — voir `character/style_store.h`. Ils se LISENT ici, à la
  // construction : ce sont deux petits fichiers, et les relire à l'ouverture
  // de la fenêtre ferait un accès disque par ouverture.
  StyleView(ro::render::ActorWardrobe* wardrobe, std::filesystem::path client_root);

  // Refait l'aperçu et la vue. À appeler une fois par image, AVANT le dessin,
  // et seulement fenêtre ouverte : composer un pantin que personne ne regarde
  // coûte une palette et une texture par palier de pulsation.
  void Refresh(const ro::sim::World& world, ro::ui::PaletteEditorState& state, bool connected,
               std::uint32_t now_ms);

  [[nodiscard]] const ro::ui::PaletteEditorView& view() const noexcept { return view_; }

  // À CHAQUE IMAGE, FENÊTRE OUVERTE OU NON : l'écho d'une demande peut revenir
  // fenêtre fermée, et le brouillon s'écrit en cours de route.
  //
  // 🔴 SUR DISQUE EN COURS DE ROUTE, ET PAS À LA FERMETURE DE LA FENÊTRE.
  // L'accident visé — plantage, coupure, client tué — ne passe justement PAS
  // par une fermeture propre.
  void Tick(const ro::sim::World& world, std::uint32_t now_ms);

  // RANGE CE QUE LE SERVEUR DIT DE NOS COULEURS, pour la tablée — voir
  // `character_styles.h`. À appeler à chaque image : il ne fait rien tant que
  // le livre n'a rien appris de neuf sur le joueur.
  //
  // ⚠ SEULEMENT APRÈS UN LOT QUI NOUS NOMME : avant, un livre vide dit « pas
  // encore reçu », et l'écrire effacerait des couleurs bien réelles.
  void KeepForSelection(const ro::sim::World& world, std::uint32_t character_id);

  // Fait ce que le joueur a demandé. ⚠ ÉMET ET S'ARRÊTE LÀ, comme les verbes
  // du sac : le serveur décide, et son écho dit ce qu'il a retenu.
  void Apply(const ro::ui::PaletteEditorAction& asked, ro::net::Connection& link,
             const ro::sim::World& world, std::uint32_t now_ms);

  // CHARGE UN CODE VENU D'AILLEURS : un collage, un lien du fil, un aperçu.
  //
  // 🔴 UN SEUL POINT D'ENTRÉE, et c'est un geste EXPLICITE : il pose le verrou
  // d'amorçage, sans quoi la fenêtre reposerait les couleurs du serveur à
  // l'image suivante, effaçant le style qu'on vient de charger.
  //
  // ⚠ N'APPLIQUE RIEN AU PERSONNAGE : il faudra valider, comme pour tout le
  // reste. Rend faux si le code est illisible.
  bool TryCode(std::string_view code, std::uint32_t now_ms);

  // LE STYLE QUE LE JOUEUR VEUT POSER DANS LE CHAT, ou rien.
  //
  // 🔴 IL SE CONSOMME, comme la demande du styliste : laissé en place, il
  // reposerait le même code à chaque image et la saisie se remplirait toute
  // seule. ⚠ ET C'EST `Game` QUI LE POSE : la barre de chat ne nous
  // appartient pas.
  struct StyleToShare {
    std::string code;
    // L'ÉTIQUETTE QUI L'ACCOMPAGNE : le nom du préréglage que le joueur a
    // désigné, ou vide pour le style en cours — c'est alors `Game` qui y met
    // notre pseudo, seul à le connaître.
    std::string label;

    [[nodiscard]] bool empty() const noexcept { return code.empty(); }
  };
  [[nodiscard]] StyleToShare TakeStyleToShare();

  // LE CODE DE LA RECETTE EN COURS, tel quel, pour un rapport de bug.
  [[nodiscard]] std::string ReportCode() const;

  // Rend les styles d'aperçu et les vignettes à la garde-robe, au passage de la
  // fenêtre d'ouverte à fermée. ⚠ ENTRE DEUX IMAGES : un style qu'on oublie
  // détruit les tenues qui le portaient.
  void Close();

 private:
  // Ce qui se lit sur la recette et sur les pièces : pastilles, plafonds,
  // code. Refait à leur changement seulement.
  void RefreshPieces(const ro::render::ActorWardrobe::BodyRamps& body,
                     const ro::render::ActorLook& worn);
  // Compose le pantin, pulsation comprise.
  void RefreshDoll(const ro::render::ActorLook& look, ro::ui::PaletteEditorState& state,
                   std::uint32_t now_ms);
  // Compose les vignettes de la grille ouverte, et rend les autres.
  //
  // ⚠ SEULEMENT CELLES QUE LA FENÊTRE A DIT VOIR, et avec un BUDGET : une
  // grille ouverte demande jusqu'à cent soixante cases, et composer les cent
  // soixante dans la même image se verrait. Voir `PalettePickerState`.
  void RefreshThumbnails(const ro::render::ActorLook& look,
                         const ro::ui::PaletteEditorState& state);
  // Les verbes de la fenêtre, une fois les réglages posés.
  void ApplyVerb(const ro::ui::PaletteEditorAction& asked, ro::net::Connection& link,
                 const ro::sim::World& world, std::uint32_t now_ms);

  ro::render::ActorWardrobe* wardrobe_ = nullptr;
  std::optional<StyleDoll> doll_;
  std::optional<ro::render::ActorThumbnails> thumbnails_;
  StyleSession session_;

  // ── CE QUI SE LIT SUR LA RECETTE, gardé d'une image à l'autre ───────────
  //
  // 🔴 LE TÉMOIN EST LA RÉVISION, pas la recette : la comparer à chaque image
  // revenait à l'encoder à chaque image, fenêtre fermée comprise. Le code
  // dépend en plus de la coupe et de la couleur PORTÉES, qu'il comble.
  struct PiecesKey {
    std::uint32_t revision = 0;
    std::uint32_t generation = 0;
    std::uint16_t worn_hair = 0;
    std::int16_t worn_hair_palette = 0;

    [[nodiscard]] bool operator==(const PiecesKey&) const = default;
  };
  PiecesKey pieces_key_;
  std::vector<ImU32> colors_;
  std::vector<int> ceilings_;
  // Le code du style courant, prêt à copier.
  std::string share_code_;

  std::vector<ro::ui::SkinImage> thumbs_;
  // La grille composée à l'image précédente : c'est le passage à « aucune »
  // qui rend les textures, et non une remise à zéro à chaque image.
  ro::ui::PaletteEditorPicker shown_picker_ = ro::ui::PaletteEditorPicker::kNone;
  ro::ui::PaletteEditorView view_;
  // La fenêtre a-t-elle été montrée depuis la dernière fermeture ?
  bool shown_ = false;

  // ── CE QUI EST RANGÉ SUR LE DISQUE ──────────────────────────────────────
  std::filesystem::path client_root_;
  // La révision du livre pour le joueur déjà rangée ; zéro : rien encore.
  std::uint32_t kept_revision_ = 0;
  StylePresets presets_;
  // Les noms, à plat, pour que la fenêtre les parcoure sans connaître le
  // magasin.
  std::vector<std::string> preset_names_;
  // Celui que le joueur vient de demander à poser dans le chat, en attente
  // d'être consommé.
  StyleToShare to_share_;
  // L'instant du dernier collage REFUSÉ : le message ne reste que quelques
  // secondes.
  std::uint32_t refused_ms_ = 0;
};

}  // namespace moonclient
