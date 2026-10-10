#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/sim/chat_line.h"
#include "ro/ui/chat_markup.h"
#include "ro/ui/chat_rich_art.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/map_display_names.h"

#include "chat/chat_item_link.h"
#include "items/creator_names.h"

namespace moonclient {

// ── UNE LIGNE DU FIL, DÉCORÉE JUSQU'AU TEXTE RICHE ──────────────────────────
//
// Les balises devenues des noms (`DecorateChatLine`), puis le balisage du texte,
// les adresses, l'auteur d'une ligne relayée de Discord, et les images qu'elle
// porte. `ui` reçoit le tout prêt à peindre.

// Les tables qui composent les noms des liens.
struct ChatDecorTables {
  const ro::vfs::ItemTable* items = nullptr;
  const ro::vfs::ItemWords* words = nullptr;
  const CreatorNames* creators = nullptr;
  const ro::vfs::MapDisplayNames* maps = nullptr;
  const MvpLinkClock* mvp_clock = nullptr;
  // Ce que le lecteur a, qui décide qu'un lien de réglage se forme.
  const SettingReach* settings = nullptr;
};

// L'icône d'un objet, ou zéro tant qu'elle n'est pas prête.
using ItemIconLookup = std::function<ImTextureID(std::uint32_t item_id)>;

// Ce que la décoration riche sait de l'image en cours, posé par la partie.
struct ChatRichContext {
  bool markup = true;
  bool item_icons = true;
  bool emote_images = true;
  bool pictures = false;
  // Ce qu'on sait d'une adresse écrite ; nul si on n'en sait rien.
  std::function<const ro::ui::ChatUrlFacts*(std::string_view)> url_facts;
  // L'état et la texture d'une vignette, par son adresse.
  std::function<void(std::string_view url, ro::ui::ChatInlineImage&)> picture;
  ItemIconLookup item_icon;
  // Les rangs des lignes qui nous nomment, croissants : elles se signalent.
  // Vide : rien ne se signale.
  std::span<const std::uint64_t> naming_seqs;
};

// UNE LIGNE PEUT-ELLE DEMANDER LA DÉCORATION RICHE ? Sans allocation : la
// question se pose à chaque ligne et à chaque image.
[[nodiscard]] bool MayNeedRichDecor(std::string_view raw, bool discord_line);
// Faux, et rien d'écrit : la ligne se montre telle quelle.
//
// Dans l'ordre : les balises deviennent des noms ; l'auteur d'une ligne relayée
// de Discord devient un lien `kDiscordName` ; le balisage s'applique, et ses
// adresses deviennent des liens `kUrl` ; une icône se pose devant chaque objet
// lié ; une adresse d'image du miroir devient une vignette. Les emotes de
// Discord et les vignettes ne restent que si `context.pictures`.
//
// ⚠ LES CLÉS DES VIGNETTES SONT LOCALES À LA LIGNE — leur rang dans
// `picture_urls` —, et les textures, les icônes et les faits d'adresse restent
// à poser : c'est à qui montre la ligne de les numéroter avec celles des autres.
[[nodiscard]] bool DecorateRichChatLine(std::string_view raw, bool discord_line,
                                        const ChatDecorTables& tables,
                                        const ChatRichContext& context, DecoratedChatLine* out);

// UNE PHRASE DE BULLE, décorée comme une ligne du fil : ses balises deviennent
// des noms, son balisage des styles, ses `:nom:` des emotes du jeu, et chaque
// objet lié porte son icône, texture posée par `item_icon` — vide : aucune
// icône. Faux, et rien d'écrit : la bulle montre la phrase telle quelle.
//
// ⚠ NI VIGNETTE NI EMOTE DE DISCORD : une bulle ne télécharge rien. Leur repli
// textuel reste dans la phrase.
[[nodiscard]] bool DecorateBubbleLine(std::string_view said, const ChatDecorTables& tables,
                                      const ItemIconLookup& item_icon, DecoratedChatLine* out);

// UNE PHRASE DÉCORÉE POSÉE DERRIÈRE UN EN-TÊTE — celui d'un chuchotement : son
// texte s'ajoute à celui de `head`, ses liens, ses styles et ses images se
// décalent de la longueur de l'en-tête, et ses vignettes se numérotent après
// celles que `head` porte déjà.
void AppendRichBody(DecoratedChatLine& head, DecoratedChatLine body);

// UNE LIGNE DONT LE SERVEUR A DÉCRIT L'AUTEUR : sa tête recopiée, le nom en
// pseudo cliquable `kPlayer` — sauf le sien, `own_name` —, puis la phrase
// décorée derrière. La coupe entre tête et phrase est EXACTE : le serveur l'a
// donnée. Faux, et rien d'écrit, si l'auteur ne s'accorde pas avec `raw`.
//
// ⚠ LE PSEUDO PORTE LE NOM, JAMAIS LES IDENTIFIANTS : sous un faux nom, ils
// valent zéro, et c'est par le nom affiché que tous ses gestes passent.
[[nodiscard]] bool DecorateAuthoredLine(std::string_view raw, const ro::sim::ChatAuthor& author,
                                        std::string_view own_name, const ChatDecorTables& tables,
                                        const ChatRichContext& context, DecoratedChatLine* out);

// L'AVATAR D'UN AUTEUR, en tête de la ligne et à la hauteur d'une rangée : une
// vignette de plus, que rien ne remplace. Elle passe AVANT toute autre image
// de la ligne, puisqu'elle s'insère au tout début du texte.
//
// ⚠ À L'APPELANT DE DÉCIDER : télécharger un avatar expose l'adresse du joueur,
// et il ne se pose que si les vignettes du chat sont permises.
void PlaceLineAvatar(DecoratedChatLine& line, std::string url);

// LES CLÉS DES VIGNETTES D'UNE LIGNE, rendues uniques dans l'image : chacune
// passe de son rang dans `picture_urls` à `first_key` plus ce rang. Une clé
// sans adresse met sa vignette en échec, et garde sa valeur.
void NumberPictureKeys(DecoratedChatLine& line, std::uint32_t first_key);

// OÙ COMMENCE CE QUE LA LIGNE DIT, après son auteur, en octets de `text` :
// derrière « [x][Pseudo] » pour une ligne relayée de Discord, derrière le
// premier « : » sinon ; zéro sans auteur. Sans allocation, comme la question
// d'au-dessus : une ligne ordinaire ne se décore pas, et se copie quand même.
//
// ⚠ UN PSEUDO QUI PORTE « : » TROMPE LA COUPE : la parole publique ne sépare
// pas l'auteur du texte. Seuls un chuchotement et une ligne de Discord la
// donnent exacte.
[[nodiscard]] std::uint32_t ChatBodyOffset(std::string_view text, bool discord_line);

}  // namespace moonclient
