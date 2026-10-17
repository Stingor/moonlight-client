#include "world/screen_overlays.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/render/pvp_hud.h"
#include "ro/render/screen_overlay.h"
#include "ro/render/screen_text.h"
#include "ro/sim/actor.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world.h"

#include "chat/chat_view.h"
#include "world/armed_cursor.h"
#include "world/battlefield_view.h"
#include "world/cast_bar_view.h"
#include "world/entity_bar_view.h"
#include "world/overlay_view.h"
#include "world/pick_box_view.h"
#include "world/talkbox_bubbles.h"

namespace moonclient {
namespace {

constexpr double kMsPerSecond = 1000.0;

// LE HUD DE JEU, déposé pour la couche au-dessus du voile quand elle en tient
// un, peint tout de suite sinon.
void PaintGameHud(const ScreenOverlays& what, std::span<const ro::render::ScreenQuad> quads,
                  float viewport_width, float viewport_height) {
  if (what.game_hud != nullptr) {
    what.game_hud->insert(what.game_hud->end(), quads.begin(), quads.end());
    return;
  }
  what.pieces->overlay.Draw(quads, viewport_width, viewport_height);
}

// LA BARRE QUE CET ACTEUR PORTE VRAIMENT SOUS SES PIEDS À CETTE IMAGE, ou rien.
// ⚠ PEINTE, ET NON SEULEMENT RÉGLÉE : un monstre intact, une barre « au
// survol » qu'on ne survole pas n'écartent pas son nom.
[[nodiscard]] std::optional<ro::ui::ScreenBand> BarsUnderFeetOf(const ScreenOverlays& what,
                                                                std::uint32_t actor_id) {
  if (what.entity_bars_on_screen == nullptr || !what.entity_bars.shown) {
    return std::nullopt;
  }
  for (const ro::ui::EntityBarView& view : *what.entity_bars_on_screen) {
    if (view.actor_id != actor_id) {
      continue;
    }
    const auto rank = static_cast<std::size_t>(view.kind);
    if (rank >= what.entity_bars.kinds.size() ||
        !ro::ui::ShowsEntityBar(what.entity_bars.kinds[rank], view)) {
      return std::nullopt;
    }
    return ro::ui::EntityBarsBandUnderFeet(what.entity_bars.kinds[rank], view);
  }
  return std::nullopt;
}

// LE CHAMP DE BATAILLE : le score, puis la marque au-dessus des ennemis.
//
// 🔴 LA MARQUE EST DE L'INTERFACE, PAS DU MONDE : chaque marque est une petite
// fenêtre, peinte avec les fenêtres, donc par-dessus le monde et le voile de
// ténèbres, et APRÈS le score, qui se peint avant elles. Toutes deux passent
// par le HUD de jeu, sous les fenêtres de ce client.
void PaintBattlefield(const ScreenOverlays& what, float aspect_ratio, float viewport_width,
                      float viewport_height) {
  GamePieces& pieces = *what.pieces;
  const ro::sim::World& world = *what.world;
  if (what.battleground.score_shown && pieces.pvp_rank_font.sprite != nullptr &&
      pieces.pvp_rank_font.animation != nullptr) {
    if (const std::optional<ro::render::BattleScoreFigures> figures =
            BattleScoreOf(world, what.battleground.own_side_highlighted);
        figures.has_value()) {
      PaintGameHud(what,
                   ro::render::ComposeBattleScore(
                       *pieces.pvp_rank_font.sprite, *pieces.pvp_rank_font.animation,
                       pieces.battlefield_art, *figures, {viewport_width, viewport_height}),
                   viewport_width, viewport_height);
    }
  }
  if (what.battleground.enemy_marker) {
    // ⚠ L'IMAGE EST CELLE DES PIÈCES, en place pour toute la partie : les quads
    // la désignent par son adresse.
    PaintGameHud(
        what,
        EnemyMarkersOnScreen(*what.camera, aspect_ratio, what.actors->drawn, what.actors->ids,
                             what.actors->concealed, what.sees_concealed_enemies, world,
                             pieces.battlefield_art, viewport_width, viewport_height),
        viewport_width, viewport_height);
  }
}

}  // namespace

HoveredActor DrawScreenOverlays(const ScreenOverlays& what, float aspect_ratio, glm::vec2 mouse_px,
                                float viewport_width, float viewport_height, std::uint32_t now_ms) {
  GamePieces& pieces = *what.pieces;
  const ro::sim::World& world = *what.world;
  const ro::sim::PartyRoster& party = *what.party;
  const ro::render::Camera& camera = *what.camera;
  const ComposedScene& actors = *what.actors;
  const ro::render::FontAtlas* font = pieces.font.has_value() ? &*pieces.font : nullptr;

  // L'ENSEIGNE qu'un acteur porte au-dessus de lui. ⚠ SES JAUGES ET SON
  // INCANTATION NE SONT PAS ICI : elles écrivent du texte, et se peignent en
  // ImGui — récoltées plus bas.
  //
  // ⚠ SON CADRE SE RELÈVE EN PEIGNANT, pour le clic qui ouvre l'échoppe : une
  // enseigne qu'on ne voit pas ne se clique pas.
  pieces.trade.signboards().clear();
  const ro::render::FontAtlas* board_font = EnsureSignboardFont(pieces, what.signboards);
  if (what.connected && !actors.drawn.empty() && board_font != nullptr) {
    // ⚠ LES COULEURS D'UNE ÉCHOPPE SONT PEINTES DANS SA SURFACE : repeintes
    // quand le réglage change, et la texture qui y était attachée oubliée.
    if (pieces.signboards.shop_colors != what.signboards.look.shop) {
      ForgetShopSignboards(pieces);
      ro::render::RecolorShopSurfaces(pieces.signboards, what.signboards.look.shop);
    }
    const std::vector<ro::render::ScreenQuad> boards =
        DrawnSignboards(camera, aspect_ratio, actors.drawn, actors.ids, world, party, *board_font,
                        pieces.signboards, what.entity_bars, what.signboards.look,
                        pieces.trade.store_search().spotlit_owner_aid(), viewport_width,
                        viewport_height, &pieces.banned_words, &pieces.trade.signboards());
    pieces.overlay.Draw(boards, viewport_width, viewport_height);
  }

  // Ce que la souris DÉSIGNE décide de deux choses : la forme du curseur et le
  // nom qui s'écrit. On ne le cherche donc qu'une fois.
  HoveredActor hovered;
  hovered.drawn_rank = ro::render::PickActor(camera, aspect_ratio, actors.drawn, mouse_px.x,
                                             mouse_px.y, viewport_width, viewport_height);
  if (hovered.drawn_rank.has_value() && *hovered.drawn_rank < actors.ids.size()) {
    hovered.actor = world.Find(actors.ids[*hovered.drawn_rank]);
  }
  // LES BARRES D'ENTITÉ : RÉCOLTÉES ICI, PEINTES PAR L'ÉTAGE D'INTERFACE.
  //
  // ⚠ APRÈS LE SURVOL : un réglage « au survol » a besoin de savoir qui la
  // souris désigne, et cette recherche n'a lieu qu'une fois. ⚠ ET AVANT LES
  // NOMS : un nom s'écarte de la barre qu'il chevaucherait, et il doit savoir
  // laquelle est vraiment peinte. L'ordre de PEINTURE, lui, ne s'en trouve pas
  // changé : rien n'est dessiné ici.
  if (what.connected && what.entity_bars.shown && what.entity_bars_on_screen != nullptr) {
    *what.entity_bars_on_screen = EntityBarsOnScreen(
        camera, aspect_ratio, actors.drawn, actors.ids, world, party,
        hovered.actor != nullptr ? hovered.actor->id : 0, viewport_width, viewport_height);
    // ⚠ RETIRÉES DE LA LISTE ET NON SEULEMENT DU DESSIN : le nom d'un marchand
    // s'écarte de la barre qu'il chevaucherait, et une barre tue ne doit pas
    // le déplacer.
    std::erase_if(*what.entity_bars_on_screen, [&](const ro::ui::EntityBarView& view) {
      const ro::sim::Actor* actor = world.Find(view.actor_id);
      return actor != nullptr && HidesEntityBarsOf(*actor, world, what.entity_bars);
    });
    // LE MODE RÉGLAGE : des jauges factices qui ondulent, panneau ouvert
    // seulement — une vie inventée ne doit pas s'afficher en combat.
    if (what.entity_bars.tuning && what.options_open) {
      const double seconds = static_cast<double>(now_ms) / kMsPerSecond;
      for (std::size_t rank = 0; rank < what.entity_bars_on_screen->size(); ++rank) {
        ro::ui::EntityBarView& view = (*what.entity_bars_on_screen)[rank];
        view.health = ro::ui::TuningEntityGauge(seconds, rank, false);
        view.magic = ro::ui::TuningEntityGauge(seconds, rank, true);
      }
    }
  } else if (what.entity_bars_on_screen != nullptr) {
    what.entity_bars_on_screen->clear();
  }

  // ── LES ÉTIQUETTES ────────────────────────────────────────────────────────
  //
  // 🔴 TOUS LES ACTEURS SONT PARCOURUS, ET NON LE SEUL SURVOLÉ. Le client
  // n'écrit que le nom de ce que la souris désigne — un widget unique replacé à
  // chaque survol —, mais c'est un RÉGLAGE ici : « toujours » veut dire toutes
  // les étiquettes, et rien ne sait lesquelles avant d'avoir demandé à
  // `ShowsEntityName`, espèce par espèce.
  //
  // ⚠ LE FILTRE EST CHEZ `render`, ET C'EST VOULU : il se vérifie alors sans
  // monde ni fenêtre. Ici on ne fait que dire ce qu'on a trouvé — l'espèce, le
  // survol, la visée.
  // ⚠ LEUR PROPRE POLICE, gravée à la taille réglée : `font` porte aussi les
  // enseignes et les panneaux d'atelier, que ce réglage-là ne doit pas emporter.
  const ro::render::FontAtlas* name_font = EnsureNameFont(pieces, what.entity_names.font_pixels);
  if (name_font != nullptr) {
    const ro::render::TitleTable* titles = pieces.titles.has_value() ? &*pieces.titles : nullptr;
    // LE NOM D'UN OBJET AU SOL SE COMPOSE ICI, et nulle part ailleurs : le
    // serveur ne le donne pas, et c'est la table de l'installation qui le
    // porte. Vide pour tout autre acteur, dont le nom vient du fil.
    const ro::vfs::ItemTable* items =
        pieces.wardrobe.has_value() ? &pieces.wardrobe->catalog().items : nullptr;
    const std::uint32_t hovered_id = hovered.actor != nullptr ? hovered.actor->id : 0;
    const std::uint32_t targeted_id = world.selected_id();

    std::vector<ro::render::ScreenQuad> names;
    for (std::size_t rank = 0; rank < actors.drawn.size() && rank < actors.ids.size(); ++rank) {
      const ro::sim::Actor* actor = world.Find(actors.ids[rank]);
      // ⚠ LA GARDE DU PORTAIL PASSE DEVANT LE RÉGLAGE, comme chez le client :
      // le serveur nomme ses warps, et ces noms ne sont pas faits pour être lus.
      if (actor == nullptr || !HasNamePlate(*actor)) {
        continue;
      }
      const ro::render::EntityNameView view{.kind = NamePlateKindOf(*actor, world),
                                            .hovered = actor->id == hovered_id,
                                            .targeted = actor->id == targeted_id};
      if (!ro::render::ShowsEntityName(what.entity_names, view)) {
        continue;
      }
      const std::string item_name = GroundItemName(*actor, items, pieces.item_words);
      const ro::formats::Image* emblem =
          what.entity_names.guild_emblem && what.guild != nullptr && what.emblems != nullptr
              ? EmblemOnPlate(*actor, world, *what.guild, *what.emblems)
              : nullptr;
      // ⚠ LES INSIGNES ET LES EMBLÈMES SE PASSENT PAR RÉFÉRENCE, sans copie : la
      // couche d'écran retient leur ADRESSE pour ne les téléverser qu'une fois.
      const std::vector<ro::render::ScreenQuad> one =
          DrawnName(*name_font, camera, aspect_ratio, actors.drawn[rank], *actor, titles, world,
                    BarsUnderFeetOf(what, actor->id), what.entity_names, pieces.boss_badges, emblem,
                    viewport_width, viewport_height, &pieces.banned_words, item_name);
      names.insert(names.end(), one.begin(), one.end());
    }
    pieces.overlay.Draw(names, viewport_width, viewport_height);
  }

  // LA PAROLE : RÉCOLTÉE ICI, PEINTE PAR L'ÉTAGE D'INTERFACE.
  //
  // 🔴 ELLE PASSE PAR LE MÊME TEXTE QUE LA CONVERSATION, et c'est ce
  // qui lui rend les emoji : notre atlas de rendu est gravé en niveaux de gris
  // et borné au latin-1. La liste de FOND d'ImGui garde l'ordre voulu —
  // au-dessus des jauges qu'on vient de peindre, sous toutes les fenêtres.
  if (what.connected && !actors.drawn.empty() && what.bubbles_on_screen != nullptr) {
    *what.bubbles_on_screen = ChatBubblesOnScreen(
        camera, aspect_ratio, actors.drawn, actors.ids, world, now_ms, viewport_width,
        viewport_height, what.bubbles, what.items, what.words, what.bubble_texts,
        pieces.skill_tree.has_value() ? &*pieces.skill_tree : nullptr, &what.creators,
        pieces.map_display_names.has_value() ? &*pieces.map_display_names : nullptr, what.item_icon,
        what.setting_reach);
    const std::vector<ro::ui::ChatBubbleView> talkbox =
        TalkboxBubblesOnScreen(camera, aspect_ratio, what.talkbox_speakers, world, now_ms,
                               viewport_width, viewport_height, what.bubbles);
    what.bubbles_on_screen->insert(what.bubbles_on_screen->end(), talkbox.begin(), talkbox.end());
  }
  // LES INCANTATIONS : RÉCOLTÉES ICI, PEINTES PAR L'ÉTAGE D'INTERFACE.
  //
  // ⚠ APRÈS LA PAROLE : une barre dont l'acteur parle pose son étiquette à côté
  // plutôt qu'au-dessus, et c'est la liste des bulles qui le lui dit.
  if (what.cast_bars_on_screen != nullptr) {
    what.cast_bars_on_screen->clear();
    if (what.connected) {
      *what.cast_bars_on_screen =
          CastBarsOnScreen(camera, aspect_ratio, actors.drawn, actors.ids, world, party,
                           pieces.skill_tree.has_value() ? &*pieces.skill_tree : nullptr,
                           what.bubbles_on_screen != nullptr
                               ? std::span<const ro::ui::ChatBubbleView>(*what.bubbles_on_screen)
                               : std::span<const ro::ui::ChatBubbleView>(),
                           now_ms, viewport_width, viewport_height);
    }
  }
  // LE CLASSEMENT PVP, sous les panneaux : c'est du HUD de jeu, et un panneau
  // ouvert doit le couvrir.
  //
  // 🔴 UN SEUL PAQUET ALLUME LES DEUX, mais ils ne s'allument pas ensemble : le
  // compteur paraît dès qu'on a un rang, le chronomètre seulement au PREMIER.
  // Les lier ferait disparaître le compteur dès qu'on perd la tête.
  //
  // ⚠ ET LE DRAPEAU DE CARTE NE LES COMMANDE PAS. Le client d'origine met
  // COUNT_PK en disjonction avec sa zone PVP, si bien qu'un rang reçu suffit —
  // et le serveur n'envoie ce paquet QUE sur une carte qui compte les rangs.
  // Exiger le drapeau tairait le compteur là où lui le montre.
  if (what.connected) {
    const ro::sim::PvpRank& rank = world.pvp_rank();
    if (pieces.pvp_rank_font.sprite != nullptr && pieces.pvp_rank_font.animation != nullptr) {
      PaintGameHud(
          what,
          ro::render::ComposePvpRank(
              *pieces.pvp_rank_font.sprite, *pieces.pvp_rank_font.animation, rank.rank, rank.total,
              ro::ui::PvpHudPlacementOf(ro::ui::PvpHudPart::kRank, what.pvp_hud,
                                        {viewport_width, viewport_height})),
          viewport_width, viewport_height);
    }
    // ⚠ LE CHRONOMÈTRE NE SE DESSINE QUE PREMIER, et son instant de départ vaut
    // zéro autrement. Le peindre sans cette garde ferait courir un compteur
    // depuis l'origine de l'horloge de l'appelant.
    if (rank.leader_since_ms != 0 && pieces.pvp_stopwatch_font.sprite != nullptr &&
        pieces.pvp_stopwatch_font.animation != nullptr) {
      PaintGameHud(what,
                   ro::render::ComposePvpStopwatch(
                       *pieces.pvp_stopwatch_font.sprite, *pieces.pvp_stopwatch_font.animation,
                       now_ms - rank.leader_since_ms,
                       ro::ui::PvpHudPlacementOf(ro::ui::PvpHudPart::kTimer, what.pvp_hud,
                                                 {viewport_width, viewport_height})),
                   viewport_width, viewport_height);
    }
    PaintBattlefield(what, aspect_ratio, viewport_width, viewport_height);
  }

  if (font != nullptr) {
    // L'ÉTIQUETTE DE CE QUI EST ARMÉ : la compétence qui attend sa cible. Le
    // RANG, lui, se lit au CURSEUR — c'est là que le client le met, et c'est le
    // seul endroit où la molette se voit agir. Voir `DrawnArmedLevel`.
    const ro::sim::KnownSkill* armed = what.book->armed();
    if (armed != nullptr && what.armed_skill.banner_shown) {
      const ro::sim::SkillTree* skills =
          pieces.skill_tree.has_value() ? &*pieces.skill_tree : nullptr;
      // ⚠ NOMMÉ, et non composé dans l'appel : la vue que prend le composeur
      // désigne cette chaîne, et elle doit lui survivre.
      const std::string banner =
          ArmedBannerText(skills, armed->id, armed->level, what.armed_skill.banner_level);
      PaintGameHud(
          what,
          ro::render::ComposeCenteredLabel(
              *font, banner,
              ro::render::ArmedBannerBaseline(what.armed_skill, viewport_width, viewport_height),
              ro::render::ArmedBannerColorBytes(what.armed_skill.banner_color)),
          viewport_width, viewport_height);
    }
    // Le grimoire d'atelier par-dessus tout le reste : c'est un panneau, il
    // masque ce qui passe derrière lui, et son clic ne descend pas jusqu'au sol.
    const std::vector<ro::render::ScreenQuad> book =
        what.book->Drawn(*font, world, viewport_width, viewport_height, now_ms);
    PaintGameHud(what, book, viewport_width, viewport_height);
    // Le banc par-dessus lui : les deux s'accrochent au même coin, et celui
    // qu'on vient d'ouvrir est celui qu'on regarde.
    PaintGameHud(what, what.bench->Drawn(*font, viewport_width, viewport_height), viewport_width,
                 viewport_height);
  }

  // LES ZONES CLIQUABLES, quand l'établi les montre : récoltées ici, où l'on
  // sait ce que la souris désigne, et peintes par l'étage d'interface.
  if (what.pick_boxes_on_screen != nullptr) {
    *what.pick_boxes_on_screen =
        PickBoxesOnScreen(camera, aspect_ratio,
                          PickScene{.drawn = actors.drawn,
                                    .ids = actors.ids,
                                    .ground_units = actors.ground_units,
                                    .ground_unit_floor_pixels = actors.ground_unit_floor_pixels},
                          world, hovered.drawn_rank, mouse_px, viewport_width, viewport_height);
  }
  return hovered;
}

}  // namespace moonclient
