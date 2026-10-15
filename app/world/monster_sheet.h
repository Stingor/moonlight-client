#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/item_icons.h"
#include "ro/sim/monster_sheet.h"
#include "ro/sim/player_status.h"
#include "ro/sim/skill_tree.h"
#include "ro/ui/monster_sheet_window.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/map_display_names.h"
#include "ro/vfs/skill_words.h"

#include "world/monster_sprite.h"

namespace moonclient {

// ── LA FICHE D'UN MONSTRE, VUE DE `app` ─────────────────────────────────────
//
// Ce que le serveur a répondu, ce qu'on lui a demandé et quand, le dernier
// relevé de Sense, le sprite qu'on titille — et la vue que la fenêtre dessine.
// Voir `ro/sim/monster_sheet.h` pour ce que la fiche porte, et
// `ro/ui/monster_sheet_window.h` pour ce qu'elle montre.
class MonsterSheet {
 public:
  // Les accessoires peuvent manquer : la fiche se dessine alors sans les mots
  // de l'installation, sans icônes ou sans sprite, plutôt que pas du tout.
  // `models` lit les acteurs 3D de la fenêtre ; l'aperçu ne les montre pas.
  MonsterSheet(ro::render::ActorWardrobe* wardrobe, const ro::vfs::ItemWords* item_words,
               ro::render::ItemIcons* item_icons, const ro::sim::SkillTree* skill_tree,
               const ro::vfs::SkillWords* skill_words, ro::vfs::Vfs* models = nullptr);

  [[nodiscard]] ro::sim::MonsterSheetBook& book() noexcept { return book_; }

  // Les noms lisibles des cartes, pour l'onglet des spawns. Nul : les lignes
  // n'écrivent que le nom interne.
  void ReadsMaps(const ro::vfs::MapDisplayNames* names) noexcept { map_names_ = names; }
  [[nodiscard]] MonsterSprite* sprite() noexcept {
    return sprite_.has_value() ? &*sprite_ : nullptr;
  }

  // Montre la fiche de ce monstre. `by_view` : l'identifiant est une classe de
  // SPRITE, et non un identifiant de la base — voir `RequestMonsterSheet`.
  void Open(ro::ui::MonsterSheetWindowState& state, std::uint32_t monster_id, bool by_view);
  // La même, sans toucher la fenêtre : la base de données l'héberge (ADR 0116).
  void Show(std::uint32_t monster_id);
  // La fiche montrée a été demandée par une classe de sprite (Sense) : la base
  // de données, qui range par identifiant, ne la reprend pas.
  [[nodiscard]] bool shown_by_view() const;
  // LE MONSTRE MONTRÉ A ÉTÉ VU EN JEU, sur cette carte : Sense, le menu d'un
  // acteur. À poser APRÈS `Open`. Un autre monstre montré ou un changement de
  // carte l'oublient.
  void NoteSeenOn(std::string map_name) { seen_map_ = std::move(map_name); }
  [[nodiscard]] const std::string& seen_map() const noexcept { return seen_map_; }
  // La vue de la dernière image dessinée.
  [[nodiscard]] const ro::ui::MonsterSheetView& view() const noexcept { return view_; }

  // LA FICHE QUE LA BASE DE DONNÉES TIRE D'ELLE-MÊME, montrée tant que le
  // serveur n'a pas répondu, ou quand il ne répond pas. La réponse du serveur
  // la remplace dès qu'elle arrive : elle seule porte les résistances, les
  // compétences et le gain d'EXP du personnage.
  void NoteBase(std::uint32_t monster_id, ro::sim::MonsterSheet sheet);
  [[nodiscard]] bool HasBase(std::uint32_t monster_id) const { return base_.contains(monster_id); }
  // « Base de données » dans la fiche, APRÈS `Refresh`, qui remet la vue à zéro.
  void NoteDatabaseLink(bool shown) noexcept { view_.database_link = shown; }

  // CE QU'IL FAUT DEMANDER AU SERVEUR À CETTE IMAGE, et c'est noté comme parti.
  //
  // 🔴 UNE FICHE EST STATIQUE, SAUF SON EXP ESTIMÉE : un instantané du
  // personnage, qui ne vaut plus rien après un niveau. On redemande alors la
  // fiche entière — le paquet est unique. Un buff d'EXP pris entre-temps n'est
  // PAS guetté : un chiffre qui bougerait seul sous les yeux poserait plus de
  // questions qu'il n'en résout.
  //
  // ⚠ DEUX MONSTRES PEUVENT ATTENDRE : celui de la fenêtre, si elle est
  // ouverte, puis celui de l'aperçu. Une demande par image, dans cet ordre.
  struct Ask {
    std::uint32_t monster_id = 0;
    bool by_view = false;
  };
  [[nodiscard]] std::optional<Ask> NextAsk(const ro::sim::PlayerStatus& status, bool sheet_open,
                                           std::uint32_t now_ms);

  // UN LIEN DE MONSTRE EST SURVOLÉ À CETTE IMAGE. Voir `Preview`. `hint` : une
  // ligne de plus en pied de l'aperçu, déjà traduite.
  void NoteHovered(std::uint32_t monster_id, bool by_view, std::string_view hint = {});
  // UNE FICHE QUE L'IA D'UN COMPAGNON VEUT LIRE, par la classe de sprite de
  // l'acteur : elle se demande après la fenêtre et l'aperçu.
  void NoteWanted(std::uint32_t sprite_class) noexcept;

  // L'APERÇU DU MONSTRE SURVOLÉ, ou nul quand rien ne l'est. Le survol noté est
  // consommé : il se redit à chaque image où la souris reste.
  //
  // ⚠ SON PROPRE SPRITE, jamais celui de la fenêtre : survoler un lien pendant
  // qu'une autre fiche est ouverte ne doit pas lui arracher le sien.
  [[nodiscard]] const ro::ui::MonsterSheetPreview* Preview(
      const ro::ui::MonsterSheetPreviewState& state, bool animate, std::uint32_t now_ms);

  // UN RELEVÉ DE SENSE NEUF, et la classe dont il faut ouvrir la fiche.
  //
  // ⚠ LE JEU REFUSE DE MONTRER CINQ GARDIENS DE FORTERESSE : leurs PV
  // resteraient cachés pendant la Guerre d'Emperium. C'est une règle de jeu,
  // pas un défaut d'interface — on la garde, et `show_guardians` la lève.
  [[nodiscard]] std::optional<std::uint32_t> TakeSense(bool show_guardians);

  // La vue de cette image.
  [[nodiscard]] const ro::ui::MonsterSheetView& Refresh(
      const ro::ui::MonsterSheetWindowState& state, const ro::sim::PlayerStatus& status,
      bool animate, bool can_post_link, std::uint32_t now_ms);

  // Le catalogue du vote shop, pour l'entrée du menu d'un item. ⚠ À POSER APRÈS
  // `Refresh`, qui remet la vue à zéro.
  void NoteVoteShop(std::span<const ro::ui::VoteShopOffer> offers) noexcept {
    view_.vote_shop = offers;
  }

  [[nodiscard]] std::uint32_t shown_id() const noexcept { return shown_id_; }
  [[nodiscard]] const ro::sim::MonsterSheet* shown() const { return book_.Find(shown_id_); }

  // Tout oublier : changement de carte. Les ressources du sprite, le cache et
  // le relevé ne valent plus.
  void Forget();

  // Vrai à la première image d'une NOUVELLE entrée sur carte, et la fiche a
  // alors tout oublié. `map_entries` est le compte du monde.
  [[nodiscard]] bool ForgetOnMapEntry(std::uint32_t map_entries);

 private:
  // Ce qu'on a demandé d'un identifiant.
  struct Asked {
    std::uint32_t at_ms = 0;
    bool sent = false;
    // ⚠ UN RAFRAÎCHISSEMENT DOIT REPOSER LA MÊME QUESTION : une classe de
    // sprite redemandée comme identifiant de base désignerait un autre monstre.
    bool by_view = false;
    // La réponse dont on a noté les niveaux, et ces niveaux : c'est ce qui
    // périme l'EXP estimée, pas le temps qui passe.
    std::uint32_t arrival = 0;
    std::int64_t base_level = 0;
    std::int64_t job_level = 0;
  };

  // Un monstre désigné, et ce que son identifiant est.
  struct Aimed {
    std::uint32_t monster_id = 0;
    bool by_view = false;
  };

  void ComposeLists(const ro::sim::MonsterSheet& sheet);
  [[nodiscard]] std::optional<Ask> AskFor(const Aimed& aimed, const ro::sim::PlayerStatus& status,
                                          std::uint32_t now_ms);

  const ro::vfs::ItemWords* item_words_;
  ro::render::ItemIcons* item_icons_;
  const ro::sim::SkillTree* skill_tree_;
  const ro::vfs::SkillWords* skill_words_;
  const ro::vfs::MapDisplayNames* map_names_ = nullptr;
  std::optional<MonsterSprite> sprite_;
  std::optional<MonsterSprite> preview_sprite_;

  ro::sim::MonsterSheetBook book_;
  // Les fiches tirées de la base, par identifiant. Elles survivent au
  // changement de carte : la base ne change pas avec lui.
  std::map<std::uint32_t, ro::sim::MonsterSheet> base_;
  std::map<std::uint32_t, Asked> asked_;
  std::uint32_t shown_id_ = 0;
  std::string seen_map_;
  std::uint32_t sense_seen_ = 0;
  std::uint32_t map_entries_ = 0;
  std::optional<ro::sim::SenseReading> sense_;

  // Les listes de la vue, refaites à chaque réponse ou changement de monstre.
  // ⚠ LA VUE POINTE DEDANS : elles ne bougent pas entre deux compositions.
  std::uint32_t lists_for_id_ = 0;
  std::uint32_t lists_for_arrival_ = 0;
  std::vector<ro::ui::MonsterSheetDropView> drops_;
  std::vector<ro::ui::MonsterSheetSpawnView> spawns_;
  std::vector<ro::ui::MonsterSheetSkillView> skills_;
  ro::ui::MonsterSheetView view_;

  // Le survol noté pour cette image, et celui que l'aperçu montre.
  std::optional<Aimed> hovered_;
  // La ligne de pied du survol noté : elle vit et meurt avec lui.
  std::string hover_hint_;
  std::optional<Aimed> previewed_;
  // La fiche que l'IA d'un compagnon attend.
  std::optional<Aimed> wanted_;
  ro::ui::MonsterSheetPreview preview_;
};

}  // namespace moonclient
