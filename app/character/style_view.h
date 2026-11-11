#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/player_style_layers.h"
#include "ro/render/actor_thumbnails.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/sim/style_layer_code.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/palette_editor.h"

#include "character/style_doll.h"
#include "character/style_layers.h"
#include "character/style_pieces.h"
#include "character/style_session.h"
#include "character/style_skin.h"
#include "character/style_store.h"

namespace moonclient {

// LA RECETTE TELLE QU'ELLE DOIT VOYAGER SANS SON PORTEUR : code copié,
// préréglage rangé.
//
// 🔴 CE QUE LA RECETTE LAISSE OUVERT CHANGE DE SENS EN QUITTANT SON
// PERSONNAGE. Sur un joueur EN VUE, moins un est exactement juste : son acteur
// porte déjà la couleur que le serveur lui a donnée. Dans un CODE, le même
// moins un devient « la tienne » chez celui qui le colle : le style arrive dans
// les cheveux du destinataire, qui se croit copié à tort. Un code voyage sans
// son porteur, il doit donc être COMPLET.
//
// ⛔ LA TEINTE DE BASE N'EST PAS COMBLÉE, elle. C'est la couleur de vêtement
// que le serveur assigne au destinataire : la lui imposer changerait la BASE
// sur laquelle ses rampes se détectent, donc le découpage de ses pièces.
[[nodiscard]] ro::render::PaletteRecipe Shareable(const ro::render::PaletteRecipe& recipe,
                                                  const ro::render::ActorLook& worn);

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
  //
  // ⚠ `character_id` RANGE LE BROUILLON : chaque personnage a le sien.
  StyleView(ro::render::ActorWardrobe* wardrobe, std::filesystem::path client_root,
            std::uint32_t character_id);

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

  // POSE NOS COULEURS DÈS L'ENTRÉE EN JEU, depuis ce que la tablée a gardé :
  // sans elles, le personnage s'affiche nu jusqu'au premier lot du serveur.
  //
  // ⚠ UNE FOIS, ET SEULEMENT TANT QUE LE LIVRE NE SAIT RIEN DE NOUS : le lot du
  // serveur, quand il arrive, redéfinit tout, et c'est lui qui fait foi.
  void SeedFromSelection(ro::sim::World& world, std::uint32_t character_id);

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
  // CE QUI ACCOMPAGNE LE CODE dans le rapport : le corps — sa clé, son sprite —,
  // ses pièces, et la part qu'elles couvrent. Des champs JSON, sans accolades.
  [[nodiscard]] std::string ReportFields() const;

  // Rend les styles d'aperçu et les vignettes à la garde-robe, au passage de la
  // fenêtre d'ouverte à fermée. ⚠ ENTRE DEUX IMAGES : un style qu'on oublie
  // détruit les tenues qui le portaient.
  void Close();

 private:
  // Ce qui se lit sur la recette et sur les pièces : pastilles, plafonds,
  // code. Refait à leur changement seulement.
  void RefreshPieces(const ro::render::ActorWardrobe::BodyRamps& body,
                     const ro::render::ActorLook& worn);
  // Compose le pantin, pulsation comprise, sous la recette `shown`.
  void RefreshDoll(const ro::render::ActorLook& look, const ro::render::PaletteRecipe& shown,
                   ro::ui::PaletteEditorState& state, std::uint32_t now_ms);
  // Les corps réglables d'ici, refaits quand le corps porté change.
  void RefreshBodies(const ro::render::ActorLook& worn_body, std::uint32_t worn_key);
  // CE QUE LE PANTIN MONTRE : ce qu'on compose, ce qu'on porte (avant/après),
  // ou le préréglage survolé.
  [[nodiscard]] const ro::render::PaletteRecipe& Shown(const ro::ui::PaletteEditorState& state);
  // Les pièces liées et gardées du corps réglé.
  [[nodiscard]] PieceMarks& Marks() { return marks_[session_.body_key()]; }
  // Les intentions qui touchent aux pièces.
  void ApplyPieces(const ro::ui::PaletteEditorAction& asked, std::uint32_t now_ms);
  // Charge un préréglage, en tout ou en partie.
  void LoadPreset(const ro::ui::PaletteEditorAction& asked, const ro::sim::World& world);
  // Les calques du personnage qu'un préréglage range : voir `StylePresets`.
  [[nodiscard]] std::vector<ro::sim::StyleLayerRecipe> PresetLayersNow() const;
  // Valide, et fait au besoin de ce corps le style principal.
  void Validate(ro::net::Connection& link, const ro::sim::World& world, std::uint32_t now_ms,
                bool make_main);
  // Compose les vignettes de la grille ouverte, et rend les autres.
  //
  // ⚠ SEULEMENT CELLES QUE LA FENÊTRE A DIT VOIR, et avec un BUDGET : une
  // grille ouverte demande jusqu'à cent soixante cases, et composer les cent
  // soixante dans la même image se verrait. Voir `PalettePickerState`.
  void RefreshThumbnails(const ro::render::ActorLook& look,
                         const ro::ui::PaletteEditorState& state);
  // LES TEINTURES DE CALQUES, en une trame, si leur cadence le permet. Vrai si
  // elle est partie ; le verdict dira si elle est retenue.
  bool SendLayers(ro::net::Connection& link, std::span<const ro::net::StyleLayerOrder> orders,
                  bool clear_character, std::uint32_t now_ms);
  // Les verbes de la fenêtre, une fois les réglages posés.
  void ApplyVerb(const ro::ui::PaletteEditorAction& asked, ro::net::Connection& link,
                 const ro::sim::World& world, std::uint32_t now_ms);

  // ── LES CALQUES — voir `style_view_layers.cpp` ──────────────────────────

  // UN ONGLET DE CALQUE, et ce qu'il faut pour le régler.
  struct LayerTab {
    ro::ui::PaletteTab shown;
    LayerAt at;
    // La partie de l'acteur qui peint ce calque, et le profil qui le découpe.
    ro::render::ActorPart part = ro::render::ActorPart::kBody;
    std::uint8_t profile = ro::render::kLayerProfileNumber;
    // Un objet : l'emplacement qu'on désigne au serveur, et l'objet attendu.
    std::optional<ro::net::StyleSlot> slot;
    std::uint32_t item_id = 0;
    // La base et les pièces : celles du sprite au profil 1, une couleur
    // représentative par classe au profil 2, l'ensemble de peau pour la peau.
    ro::formats::Palette base;
    std::vector<ro::render::PaletteRamp> ramps;
    // La palette du sprite lui-même : c'est elle que la pipette lit.
    ro::formats::Palette sprite_base;
    // LE RESTE DU PROFIL MIXTE : les classes de teinte présentes hors des
    // pièces, dans l'ordre des classes, et leur couleur moyenne. Vides quand il
    // ne s'offre pas : la peau, le profil 2, un serveur qui ne le connaît pas.
    std::vector<std::size_t> rest_classes;
    std::vector<std::array<std::uint8_t, 3>> rest_means;
    // Les retouches exactes s'y permettent : un sprite palettisé, hors de la
    // peau, et un serveur qui les accepte.
    bool retouchable = false;
    // La race du porteur : la peau d'une tête se tient hors de son reste.
    ro::render::SkinRace race = ro::render::SkinRace::kHuman;
  };
  // ── LE PROFIL MIXTE : voir style_view_rest.cpp ───────────────────────────
  // Le reste d'un onglet, tiré de sa mesure.
  void FillRest(LayerTab& tab, const ro::render::RestMeasure& rest) const;
  // Les rangées d'un onglet : ses pièces, puis les classes de son reste.
  [[nodiscard]] static std::size_t RowCount(const LayerTab& tab);
  // Le rang de réglage d'une rangée : une pièce, ou `kRestRankFirst` + classe.
  [[nodiscard]] static std::size_t EditRank(const LayerTab& tab, std::size_t row);
  // La couleur d'origine d'une rangée.
  [[nodiscard]] static std::array<std::uint8_t, 3> RowBaseColor(const LayerTab& tab,
                                                                std::size_t row);
  // LES RANGÉES D'UN ONGLET, pastille par pastille : pièces, puis reste.
  struct Rows {
    std::vector<ro::render::PaletteRamp>* ramps;
    std::vector<ro::render::RampAdjust>* adjusts;
    std::vector<ImU32>* colors;
    std::vector<ImU32>* base_colors;
    std::vector<int>* ceilings;
    std::vector<std::array<std::uint8_t, 3>>* painted;
  };
  static void AppendRows(const LayerTab& tab, const ro::render::LayerDye& dye, const Rows& rows);
  // La palette du calque, telle que le joueur la verra : pièces, reste,
  // retouches.
  [[nodiscard]] static ro::formats::Palette PaintedPalette(const LayerTab& tab,
                                                           const ro::render::LayerDye& dye);
  // Les retouches de l'onglet ouvert, et la palette dont elles partent.
  void RefreshRetouchView(const LayerTab& tab, const ro::render::LayerDye& dye);
  // Une retouche posée ou retirée.
  void ApplyRetouch(const ro::ui::PaletteEditorAction& asked, const LayerTab& tab);
  // La suite et le reste du corps, sous les huit pièces v7 de l'onglet Corps.
  void RefreshBodyMore(const ro::render::ActorWardrobe::BodyRamps& body);
  // Les gestes de l'onglet Corps qui visent ses rangées au-delà des huit.
  void ApplyBodyMore(const ro::ui::PaletteEditorAction& asked, Harmony harmony,
                     std::uint32_t now_ms);
  // La rangée que la pipette désigne dans les rangées d'un onglet, à partir de
  // `first`, ou -1.
  [[nodiscard]] static int RowAt(const LayerTab& tab, std::uint8_t index, std::size_t first);
  // Les onglets des calques que `look` porte, refaits quand il change.
  void RefreshTabs(const ro::render::ActorLook& look, const ro::sim::World& world);
  // L'onglet ouvert, ou nul sur le corps.
  [[nodiscard]] const LayerTab* OpenLayer() const;
  [[nodiscard]] const LayerTab* TabOf(ro::ui::PaletteTabKind kind) const;
  // Ce que la vue montre d'un onglet de calque : pièces, pastilles, code.
  void RefreshLayerPieces(const LayerTab& tab);
  // Ce que le serveur dit de nos calques, et ce que la vue en tire.
  void RefreshLayerState(const ro::sim::World& world, std::uint32_t now_ms);
  // L'apparence du pantin pour l'onglet ouvert.
  [[nodiscard]] ro::render::ActorLook DollLook(const ro::render::ActorLook& body_look,
                                               const ro::render::ActorLook& full_look) const;
  // La pipette, tous calques confondus.
  void ReadPipette(const ro::ui::PaletteEditorState& state);
  // Les intentions sur les pièces d'un calque.
  void ApplyLayerPieces(const ro::ui::PaletteEditorAction& asked, const LayerTab& tab,
                        Harmony harmony, std::uint32_t now_ms);
  // La raison du dernier refus des calques, déjà traduite, ou nulle.
  [[nodiscard]] const char* LayersRefusal() const;

  // Les réglages que la session des calques reprend d'une couleur désignée.
  [[nodiscard]] std::vector<LayerEdit> Reach(const LayerTab& tab,
                                             std::span<const std::size_t> ranks,
                                             std::array<std::uint8_t, 3> target) const;
  // « Adapter à ma nouvelle coupe ».
  void AdaptCut(const LayerTab& tab);
  // Les teintures à valider, en ordres du fil. Les objets n'y entrent qu'à
  // l'atelier.
  [[nodiscard]] std::vector<ro::net::StyleLayerOrder> LayerOrders(bool atelier_open) const;
  // Valide les calques, s'il y a lieu.
  void ValidateLayers(ro::net::Connection& link, std::uint32_t now_ms);
  // Retire la teinture de l'objet de l'onglet.
  void ClearItemDye(const LayerTab& tab, ro::net::Connection& link, std::uint32_t now_ms);
  // Lit un code de calque sur l'onglet ouvert. Faux s'il n'y va pas.
  bool TryLayerCode(std::string_view code);

  // ── LES DEUX PEAUX — voir `style_view_skin.cpp` ─────────────────────────

  // Ce qui décide si les peaux se règlent, relu au changement d'apparence.
  void RefreshSkins(const ro::render::ActorLook& look);
  // Pourquoi cette ligne ne se règle pas, ou rien.
  [[nodiscard]] SkinRefusal SkinRefusalOf(SkinLine line) const;
  [[nodiscard]] bool SkinAdjustable(SkinLine line) const;
  // La ligne de peau de l'onglet de la vue : le visage sous la Tête, le corps
  // sous le Corps ; rien ailleurs, ni pour un Doram.
  [[nodiscard]] std::optional<SkinLine> OpenSkinLine() const;
  // Le calque de la ligne, sous la clé du corps qu'on habille.
  [[nodiscard]] LayerAt SkinAt(SkinLine line) const;
  // Le réglage composé de la ligne, neutre sans teinture.
  [[nodiscard]] ro::render::RampAdjust SkinAdjust(SkinLine line) const;
  // Ce que la vue montre de la ligne de l'onglet ouvert, et les teints.
  void RefreshSkinLine();
  // Les gestes de cette ligne.
  void ApplySkinLine(const ro::ui::PaletteEditorAction& asked);
  // La peau a-t-elle une retouche sur l'onglet de cette ligne ?
  [[nodiscard]] bool SkinEdited(SkinLine line) const;
  // Lit un code de peau sur l'onglet Tête : il teint le visage.
  bool TrySkinCode(const ro::sim::StyleLayerCode& code);

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
  std::vector<ImU32> base_colors_;
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

  // ── LES CORPS RÉGLABLES D'ICI ───────────────────────────────────────────
  struct BodyChoice {
    // Une clé du catalogue, en français.
    const char* name = nullptr;
    ro::render::ActorLook look;
    std::uint32_t key = 0;
  };
  std::vector<BodyChoice> bodies_;
  std::vector<const char*> body_names_;
  // Le corps porté pour lequel la liste a été faite.
  std::uint32_t bodies_for_ = 0;
  // Le rang du corps réglé ; zéro : le corps porté.
  int edited_body_ = 0;

  // ── CE QUE LE PANTIN MONTRE ─────────────────────────────────────────────
  //
  // 🔴 SON TÉMOIN N'EST PAS LA RÉVISION DE LA RECETTE quand il montre autre
  // chose qu'elle : il avance à chaque changement de ce qui est montré, et le
  // pantin se repeint alors.
  struct ShownKey {
    int source = 0;
    std::uint32_t revision = 0;
    // La révision des calques composés : leurs teintures se peignent aussi.
    std::uint32_t layers_revision = 0;
    int preset = -1;

    [[nodiscard]] bool operator==(const ShownKey&) const = default;
  };
  ShownKey shown_key_;
  std::uint32_t shown_serial_ = 1;
  ro::render::PaletteRecipe preview_;

  // Les pièces liées et gardées, par corps.
  std::map<std::uint32_t, PieceMarks> marks_;
  // Le nombre de tirages : chaque inspiration en tire un autre.
  std::uint32_t inspirations_ = 0;

  // ── CE QUI EST RANGÉ SUR LE DISQUE ──────────────────────────────────────
  std::filesystem::path client_root_;
  std::uint32_t character_id_ = 0;
  // La révision du livre pour le joueur déjà rangée ; zéro : rien encore.
  std::uint32_t kept_revision_ = 0;
  // Nos couleurs gardées ont-elles été posées, ou n'y avait-il rien à poser ?
  bool seeded_ = false;
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

  // ── LES CALQUES ─────────────────────────────────────────────────────────
  StyleLayers layers_;
  std::vector<LayerTab> tabs_;
  std::vector<ro::ui::PaletteTab> tab_views_;
  // L'apparence pour laquelle les onglets ont été faits, et la mesure du corps.
  std::optional<ro::render::ActorLook> tabs_look_;
  // Le profil mixte permis au moment de la mesure des onglets.
  bool tabs_mixed_ = false;
  std::uint32_t tabs_generation_ = 0;
  // L'onglet de la vue, et ce qui s'y lit.
  ro::ui::PaletteTabKind view_tab_ = ro::ui::PaletteTabKind::kBody;
  std::vector<ro::render::RampAdjust> layer_adjusts_;
  std::vector<ImU32> layer_colors_;
  std::vector<ImU32> layer_base_colors_;
  std::vector<int> layer_ceilings_;
  std::string layer_code_;

  std::vector<std::array<std::uint8_t, 3>> layer_painted_;
  std::vector<ro::render::LayerDye> composed_;
  std::vector<ImU32> skin_tones_;
  // LES PEAUX : un Doram n'en a pas ; l'habit du GM et les chemins des corps
  // décident si elles se règlent ; le serveur, s'il range celle du corps.
  bool skins_shown_ = false;
  bool skins_gm_outfit_ = false;
  std::vector<std::string> skin_body_paths_;
  int server_layer_last_ = 0;
  // Les pièces liées et gardées, par calque.
  std::map<LayerAt, PieceMarks> layer_marks_;
  // Une teinture d'objet peut-elle se valider, à cette image ? Vrai quand le
  // serveur n'exige pas l'atelier du styliste, ou quand celui-ci est ouvert.
  bool atelier_open_ = false;
  // 🔴 LE PROFIL MIXTE N'EXISTE QUE SI LE SERVEUR L'ANNONCE (règle 24) : sans
  // elle, ni reste ni retouche ne s'offrent, ni ne partent.
  bool mixed_allowed_ = false;
  int retouches_most_ = 0;
  // La suite et le reste du corps, comme un onglet de calque caché sous Corps.
  LayerTab body_more_;
  // Les rangées de la vue : pièces puis reste, pour un calque comme pour le
  // corps.
  std::vector<ro::render::PaletteRamp> layer_rows_;
  std::vector<ro::render::PaletteRamp> body_rows_;
  std::vector<ro::render::RampAdjust> body_adjusts_;
  std::vector<ImU32> body_colors_;
  std::vector<ImU32> body_base_colors_;
  std::vector<int> body_ceilings_;
  std::vector<std::array<std::uint8_t, 3>> body_painted_;
  std::vector<ImU32> palette_colors_;
  std::vector<ro::render::PaletteRetouch> view_retouches_;
  // Ce que le dernier code de calque collé n'a pas apporté, ou rien.
  const char* code_note_ = nullptr;
};

}  // namespace moonclient
