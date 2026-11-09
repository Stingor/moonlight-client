#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "ro/render/palette_ramps.h"
#include "ro/sim/style.h"

#include "character/style_store.h"

namespace moonclient {

// CE QUE LE SERVEUR A FAIT DE NOTRE DERNIÈRE DEMANDE, d'après son ÉCHO.
//
// 🔴 L'ÉCHO EST LE SEUL ACCUSÉ. Le serveur rediffuse à la zone, émetteur compris,
// ce qu'il retient ; et il JETTE sans un mot ce qu'il refuse — une version
// inconnue, des variables pas encore chargées, une clé nulle, un renvoi
// identique, une demande moins d'une seconde après la précédente —, comme la
// liaison avale un envoi sur une connexion fermée. « Envoyé » ne disait donc
// rien : seul le retour de notre propre style dit qu'il est partagé.
enum class StyleSendState : std::uint8_t {
  kNone,
  // Parti ; l'écho n'est pas encore revenu.
  kWaiting,
  // L'écho porte ce qu'on a demandé.
  kShared,
  // Aucun écho dans le délai, ou rien n'a pu partir.
  kRefused,
};

// Ce que la demande voulait, donc ce que l'écho doit montrer.
enum class StyleSendKind : std::uint8_t {
  // La recette du corps porté.
  kApply,
  // L'effacement du style de ce corps : il reprend le repli.
  kForgetBody,
  // L'effacement de tout.
  kDeleteAll,
};

// LA RECETTE QUE LE JOUEUR COMPOSE, ET SON SEUL ÉCRIVAIN.
//
// 🔴 UN SEUL ÉCRIVAIN. La fenêtre ne touche pas à la recette : elle reçoit une
// vue et rend des INTENTIONS, que `StyleView` traduit en appels ici. Chaque
// changement avance la RÉVISION, et c'est elle — pas une comparaison de la
// recette à chaque image — qui dit quand refaire les pastilles, le code, la
// palette du pantin et le brouillon.
//
// 🔴 UN TRAVAIL PAR CORPS. Une recette ne désigne ses pièces que par des index
// de palette, qui n'ont aucun sens commun d'un corps à l'autre : monter en selle
// met de côté le travail à pied et reprend celui de la monture — ou, s'il n'y en
// a pas, ce qu'elle porte, comme les autres joueurs la voient. Réappliquer la
// recette d'un corps à l'autre montrait des couleurs que personne ne voit, et
// « Valider » écrasait la variante de la monture.
//
// ⚠ TANT QU'AUCUN GESTE N'A EU LIEU SUR UN CORPS, SA RECETTE SUIT CE QU'IL
// PORTE : le lot du serveur peut arriver après l'ouverture, et un écho change ce
// qu'on porte. Le premier geste pose le VERROU D'AMORÇAGE de ce corps ; seuls
// « Revenir à mon style » et l'écho d'une demande le lèvent.
//
// ⚠ AUCUNE ENTRÉE-SORTIE : ni garde-robe, ni fichier, ni liaison. C'est ce qui
// la rend éprouvable seule ; `StyleView` lit et écrit les fichiers, et envoie.
// Elle vit avec la PARTIE : un autre personnage repart d'une session neuve, et
// rien de ce qu'on composait ne passe de l'un à l'autre.
class StyleSession {
 public:
  // Le brouillon rangé sur le disque, que l'appelant a lu.
  explicit StyleSession(StyleDrafts drafts = {});

  // ── CE QUE LA PARTIE DIT DE NOUS ────────────────────────────────────────

  struct Worn {
    // Le corps porté ; zéro quand aucun ne se charge.
    std::uint32_t body_key = 0;
    // Nos variantes telles que le serveur les a dites ; rien tant qu'il ne
    // nous a pas nommés, ou quand nous n'en avons plus.
    const ro::sim::StyleVariants* mine = nullptr;
    // `StyleBook::RevisionOf` pour nous : il avance à chaque lot qui nous
    // nomme, et c'est ce qui distingue un écho de ce qu'on savait déjà.
    std::uint32_t book_revision = 0;
  };

  // À chaque image, fenêtre ouverte ou non : l'écho peut revenir fenêtre
  // fermée, et le délai de l'annonce court quand même.
  void Observe(const Worn& worn, std::uint32_t now_ms);

  // ── LES GESTES, chacun sur le corps porté ───────────────────────────────

  void SetPiece(std::size_t rank, const ro::render::RampAdjust& adjust);
  void ChooseHairStyle(std::int16_t hair_style);
  void ChooseHairPalette(std::int16_t hair_palette);
  void ChooseBodyPalette(std::int16_t body_palette);
  // Un préréglage, un code : ils remplacent la recette entière.
  void Load(const ro::render::PaletteRecipe& recipe);
  // Reprend le brouillon de ce corps. Faux s'il n'y en a pas.
  bool LoadDraft();
  // La page blanche : les pièces reviennent à leur état d'origine.
  void BlankSlate();
  // 🔴 ANNULER LES RETOUCHES NE LES PERD PAS : elles partent d'abord au
  // brouillon, puis la recette revient à ce que le corps porte.
  void BackToWorn();

  // ── LES DEMANDES AU SERVEUR ─────────────────────────────────────────────

  // LA RECETTE À VALIDER, ou rien quand ce corps porte déjà exactement celle-là
  // en propre : le serveur jetterait le renvoi sans un mot, et l'éditeur
  // l'annoncerait refusé. Elle est alors annoncée partagée sur-le-champ.
  //
  // 🔴 LA COUPE PORTÉE SE FIGE ICI quand la recette n'en impose aucune : le
  // serveur range le moins un mais renvoie dans l'écho la coupe RÉELLEMENT
  // portée, si bien que la recette ne coïnciderait jamais avec ce qu'on porte
  // et resterait « modifiée » pour de bon.
  [[nodiscard]] std::optional<ro::render::PaletteRecipe> Validate(std::int16_t worn_hair_style,
                                                                  std::uint32_t now_ms);
  // Ce qui vient d'être demandé pour le corps porté, et si c'est parti. Rien
  // ne change avant l'écho : un refus laisse le travail en place.
  void Sent(StyleSendKind kind, bool left, std::uint32_t now_ms);

  // ── CE QU'ON EN LIT ─────────────────────────────────────────────────────

  [[nodiscard]] const ro::render::PaletteRecipe& recipe() const;
  // Avance à chaque changement de la recette montrée, changement de corps
  // compris. Jamais zéro.
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }
  [[nodiscard]] std::uint32_t body_key() const noexcept { return body_key_; }
  // Ce que ce corps porte : sa variante, sinon le repli, sinon rien.
  [[nodiscard]] const ro::render::PaletteRecipe& applied() const noexcept { return applied_; }
  // La recette diffère-t-elle de ce que le corps porte ?
  [[nodiscard]] bool has_edits() const;
  // Ce corps a-t-il sa propre variante, et combien le joueur en a-t-il ?
  [[nodiscard]] bool own_style() const noexcept { return own_style_; }
  [[nodiscard]] int style_count() const noexcept { return style_count_; }
  [[nodiscard]] StyleSendState send_state() const noexcept { return send_state_; }
  // Un brouillon de ce corps, autre que ce qu'on voit, attend d'être repris.
  [[nodiscard]] bool OffersDraft() const;

  // ── LE BROUILLON ────────────────────────────────────────────────────────

  // Range la recette au brouillon une fois qu'elle a cessé de bouger.
  //
  // ⚠ L'IMMOBILITÉ SE COMPTE SUR LA RÉVISION : aucun encodage à chaque image.
  // 🔴 ET RIEN NE L'EFFACE ICI : une recette revenue à ce qu'on porte ne
  // retire pas le brouillon. Seuls l'écho d'une validation ou d'un effacement
  // l'emportent.
  void TickDraft(std::uint32_t now_ms);
  [[nodiscard]] const StyleDrafts& drafts() const noexcept { return drafts_; }
  // Vrai une fois après chaque changement du brouillon : il faut l'écrire.
  [[nodiscard]] bool TakeDraftsChanged();

 private:
  // Le travail sur un corps.
  struct Slot {
    ro::render::PaletteRecipe recipe;
    // Le verrou d'amorçage : un geste a eu lieu, ce que le corps porte ne
    // remplace plus la recette.
    bool touched = false;
    // La révision de son dernier changement : un écho ne lève pas le verrou
    // d'un corps retouché APRÈS la demande.
    std::uint32_t changed_at = 0;
  };

  // Une demande partie, en attente de son écho.
  struct Pending {
    StyleSendKind kind = StyleSendKind::kApply;
    std::uint32_t body_key = 0;
    std::uint32_t at_revision = 0;
    // Le livre au départ : l'écho est un lot qui vient APRÈS.
    std::uint32_t book_revision = 0;
    // Ce qui est parti, pour reconnaître l'écho d'une validation.
    ro::render::PaletteRecipe sent;
    // Les corps habillés au départ : « tout supprimer » emporte leurs
    // brouillons.
    std::vector<std::uint32_t> bodies;
    std::uint32_t sent_ms = 0;
  };

  [[nodiscard]] Slot& Current();
  // Remplace la recette du corps porté et pose le verrou.
  void Put(const ro::render::PaletteRecipe& recipe);
  void Changed(Slot& slot);
  // Le corps sans verrou reprend ce qu'il porte.
  void Follow();
  void KeepDraft();
  void HearEcho(const Worn& worn, std::uint32_t now_ms);
  void Settle(const Pending& asked);
  void Announce(StyleSendState state, std::uint32_t now_ms);

  std::map<std::uint32_t, Slot> slots_;
  std::uint32_t body_key_ = 0;
  bool observed_ = false;
  std::uint32_t revision_ = 1;

  ro::render::PaletteRecipe applied_;
  bool own_style_ = false;
  int style_count_ = 0;
  std::vector<std::uint32_t> worn_bodies_;
  std::uint32_t book_revision_ = 0;

  std::optional<Pending> pending_;
  StyleSendState send_state_ = StyleSendState::kNone;
  std::uint32_t announced_ms_ = 0;

  StyleDrafts drafts_;
  bool drafts_changed_ = false;
  std::uint32_t draft_revision_ = 0;
  // L'instant où la révision s'est arrêtée ; zéro : rien en attente.
  std::uint32_t draft_since_ms_ = 0;
};

}  // namespace moonclient
