#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/actor_thumbnails.h"
#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/sim/actor.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/character_window.h"
#include "ro/ui/palette_editor.h"

#include "character/style_store.h"

namespace moonclient {

// L'ÉDITEUR DE STYLE, VU DE LA PARTIE.
//
// ⚠ IL CROISE CE QUE NI `ui` NI `render` NE PEUVENT CROISER : les pièces du
// corps porté viennent de la garde-robe, ce que le personnage PORTE vient du
// livre des styles, et la traduction d'une couleur en réglage demande la
// palette de BASE, qui n'appartient à aucune des deux.
//
// 🔴 RIEN NE TOUCHE AU PERSONNAGE AVANT LA VALIDATION. L'aperçu vit dans un
// STYLE À PART, posé sur la garde-robe sous son propre identifiant ; l'acteur
// en scène garde celui que le serveur lui a donné. C'est ce qui rend l'essai
// gratuit — tout dérégler puis fermer ne laisse aucune trace.
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
  // coûte une palette et une texture par pulsation.
  void Refresh(const ro::sim::World& world, ro::ui::PaletteEditorState& state, bool connected,
               std::uint32_t now_ms);

  [[nodiscard]] const ro::ui::PaletteEditorView& view() const noexcept { return view_; }

  // RANGE CE QUE LE SERVEUR DIT DE NOS COULEURS, pour la tablée — voir
  // `character_styles.h`. À appeler à chaque image : il ne fait rien tant que
  // le livre n'a rien appris de neuf sur le joueur.
  //
  // ⚠ SEULEMENT APRÈS UN LOT QUI NOUS NOMME : avant, un livre vide dit « pas
  // encore reçu », et l'écrire effacerait des couleurs bien réelles.
  void KeepForSelection(const ro::sim::World& world, std::uint32_t character_id);

  // Fait ce que le joueur a demandé. ⚠ ÉMET ET S'ARRÊTE LÀ, comme les verbes
  // du sac : le serveur décide, et avancer la couleur chez nous montrerait une
  // apparence que les autres ne partagent pas.
  void Apply(const ro::ui::PaletteEditorAction& asked, ro::net::Connection& link,
             ro::ui::PaletteEditorState& state, const ro::sim::World& world, std::uint32_t now_ms);

  // CHARGE UN CODE VENU D'AILLEURS : un collage, un lien du fil, un aperçu.
  //
  // 🔴 UN SEUL POINT D'ENTRÉE, et ce n'est pas de la cosmétique : trois
  // chemins mènent ici, et trois copies laisseraient l'une d'elles oublier le
  // verrou d'amorçage — la fenêtre reposerait alors les couleurs du serveur à
  // l'image suivante, effaçant le style qu'on vient de charger.
  //
  // ⚠ N'APPLIQUE RIEN AU PERSONNAGE : il faudra valider, comme pour tout le
  // reste. Un code vient d'un autre joueur, il ne doit pas pouvoir changer une
  // apparence d'un simple clic. Rend faux si le code est illisible.
  bool TryCode(std::string_view code, ro::ui::PaletteEditorState& state, std::uint32_t now_ms);

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

  // Rend l'identifiant d'aperçu à la garde-robe. ⚠ À LA FERMETURE DE LA
  // FENÊTRE : un style qu'on oublie détruit les tenues qui le portaient, et
  // une composition en cours les lirait mortes.
  void Close();

  // ÉCRIT LE BROUILLON S'IL A EU LE TEMPS DE SE POSER.
  //
  // 🔴 SUR DISQUE EN COURS DE ROUTE, ET PAS À LA FERMETURE DE LA FENÊTRE.
  // L'accident visé — plantage, coupure, client tué — ne passe justement PAS
  // par une fermeture propre, et n'attraper que celle-ci ne protégerait que
  // du cas où le joueur n'a rien perdu.
  //
  // ⚠ LE PRIX EST UNE ÉCRITURE DE FICHIER, donc pas à chaque image : on
  // attend que le joueur ait fini de bouger. Un glissement dans un nuancier
  // produit des dizaines d'états par seconde dont aucun ne mérite le disque.
  //
  // ⚠ À APPELER MÊME FENÊTRE FERMÉE : c'est ce qui laisse au délai le temps
  // d'expirer sur ce que le joueur venait de composer.
  void TickDraft(const ro::ui::PaletteEditorState& state, std::uint32_t now_ms);

 private:
  // L'apparence du pantin : le corps et la tête, rien d'autre.
  //
  // ⚠ NUE, SANS CE QUE LA RECETTE IMPOSE : c'est elle qui décide du CORPS,
  // donc de la variante de style à retrouver, et la variante ne doit pas
  // décider du corps. Ce que la recette impose se pose ensuite.
  [[nodiscard]] ro::render::ActorLook DollLook(const ro::sim::Actor& player) const;
  // Recharge les pièces quand le corps ou la teinte de base ont changé.
  void ReloadRamps(const ro::render::ActorLook& look);
  // Compose le pantin, pulsation comprise.
  void RefreshDoll(const ro::render::ActorLook& look, ro::ui::PaletteEditorState& state,
                   std::uint32_t now_ms);
  // LA PIPETTE : quelle pièce le curseur désigne sur le pantin.
  //
  // 🔴 ELLE NE COMPARE AUCUNE COULEUR. Deux pièces peuvent porter la même
  // teinte après réglage, et une comparaison les confondrait. On lit l'INDEX
  // de palette du pixel source — que le parseur conserve précisément pour ça —
  // et l'on cherche la rampe qui le contient : la réponse est exacte par
  // construction.
  //
  // ⚠ SUR LE POINT RELEVÉ À L'IMAGE PRÉCÉDENTE, `ui` ne pouvant pas répondre
  // elle-même : il lui faudrait le sprite et ses index par pixel. Le même
  // décalage d'une image que la pulsation, et il ne se voit pas davantage.
  void RefreshPipette(const ro::render::ActorLook& look, const ro::ui::PaletteEditorState& state);
  // Compose les vignettes de la grille ouverte, et rend les autres.
  //
  // ⚠ SEULEMENT CELLES QUE LA FENÊTRE A DIT VOIR, et avec un BUDGET : une
  // grille ouverte demande jusqu'à cent soixante cases, et composer les cent
  // soixante dans la même image se verrait. Voir `PalettePickerState`.
  void RefreshThumbnails(const ro::render::ActorLook& look,
                         const ro::ui::PaletteEditorState& state);

  ro::render::ActorWardrobe* wardrobe_ = nullptr;
  std::optional<ro::render::ActorVignette> doll_;
  std::optional<ro::render::ActorThumbnails> thumbnails_;

  // Les pièces du corps porté, et la base sur laquelle elles se règlent.
  ro::render::ActorWardrobe::BodyRamps body_;
  // CE POUR QUOI `body_` A ÉTÉ MESURÉ, et ce qui dit quand recommencer.
  //
  // 🔴 DEUX TÉMOINS, PAS UN. Le corps décide du sprite, la TEINTE DE BASE
  // décide de la palette sur laquelle il est fusionné — donc du découpage des
  // rampes. N'en surveiller qu'un laissait le joueur changer de teinte et
  // continuer de régler les pièces de l'ancienne.
  bool measured_valid_ = false;
  std::uint32_t body_key_ = 0;
  std::int16_t measured_palette_ = -1;

  // ── L'AMORÇAGE : CE QUE LE PERSONNAGE PORTE DÉJÀ ────────────────────────
  //
  // 🔴 SANS LUI, OUVRIR LA FENÊTRE PROPOSE LE VIDE À UN JOUEUR DÉJÀ TEINT, et
  // « Valider » effacerait ses couleurs — un geste qui a l'air anodin et qui
  // détruit son travail.
  //
  // ⚠ RETENTÉ TANT QU'IL N'A PAS ABOUTI : le lot du serveur peut arriver après
  // l'ouverture, et une seule tentative laisserait la fenêtre vide pour toute
  // la session.
  bool seeded_ = false;
  // Le joueur a-t-il EXPLICITEMENT demandé le vide ? C'est le verrou qui
  // empêche le rattrapage ci-dessus de défaire une page blanche.
  bool touched_ = false;

  // Ce que la fenêtre montre, refait à chaque image.
  std::vector<ImU32> colors_;
  std::vector<int> ceilings_;
  std::vector<ro::ui::SkinImage> thumbs_;
  // La grille composée à l'image précédente : c'est le passage à « aucune »
  // qui rend les textures, et non une remise à zéro à chaque image.
  ro::ui::PaletteEditorPicker shown_picker_ = ro::ui::PaletteEditorPicker::kNone;
  ro::ui::PaletteEditorView view_;

  // L'identifiant du style d'APERÇU, et ce qu'il montre. ⚠ UN IDENTIFIANT NEUF
  // À CHAQUE CHANGEMENT : la garde-robe refuse de réécrire une palette en
  // place, parce qu'une tenue déjà composée en retient l'adresse.
  std::uint32_t preview_style_ = 0;
  ro::render::PaletteRecipe shown_recipe_;
  int shown_highlight_ = -1;
  int shown_step_ = -1;
  bool shown_valid_ = false;

  // L'instant du dernier envoi, pour l'accusé local de quelques secondes.
  std::uint32_t sent_ms_ = 0;

  // ── CE QUI EST RANGÉ SUR LE DISQUE ──────────────────────────────────────
  std::filesystem::path client_root_;
  // La révision du livre pour le joueur déjà rangée ; zéro : rien encore.
  std::uint32_t kept_revision_ = 0;
  StylePresets presets_;
  StyleDrafts drafts_;
  // Les noms, à plat, pour que la fenêtre les parcoure sans connaître le
  // magasin.
  std::vector<std::string> preset_names_;
  // Le code du style courant, refait à chaque image et prêt à copier.
  std::string share_code_;
  // Celui que le joueur vient de demander à poser dans le chat, en attente
  // d'être consommé.
  StyleToShare to_share_;
  // L'instant du dernier collage REFUSÉ : le message ne reste que quelques
  // secondes.
  std::uint32_t refused_ms_ = 0;

  // LE STYLE TEL QU'IL EST POSÉ SUR LE PERSONNAGE.
  //
  // 🔴 LE BROUILLON N'EXISTE QUE PAR SON ÉCART AVEC LUI : tant que les deux
  // coïncident, il n'y a rien à récupérer, et proposer de recharger
  // l'identique serait un bouton qui ment.
  ro::render::PaletteRecipe applied_;

  // ── LE BROUILLON EN ATTENTE D'ÊTRE ÉCRIT ────────────────────────────────
  //
  // Le style vu à l'image précédente, sous sa forme de code, et l'instant où
  // il a changé. Un instant nul veut dire « rien en attente ».
  std::string draft_seen_;
  std::uint32_t draft_since_ms_ = 0;
  bool draft_seen_valid_ = false;
};

}  // namespace moonclient
