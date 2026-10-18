// La table des commandes « / » que la liste du chat propose. Voir l'en-tête.
//
// ⚠ LES GRAPHIES EN FRAPPE CORÉENNE (« dkswrl » pour « 앉기 ») SONT DES ALIAS :
// elles se filtrent, mais la rangée montre le nom que le joueur lit.

#include "chat/slash_commands.h"

#include <array>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::TrText;

// Une commande sans argument, une qui en attend, une du staff.
[[nodiscard]] constexpr SlashCommand Plain(std::string_view name, std::string_view aliases,
                                           const char* help) {
  return SlashCommand{.name = name, .aliases = aliases, .help = help};
}

[[nodiscard]] constexpr SlashCommand Taking(std::string_view name, std::string_view aliases,
                                            std::string_view params, const char* help) {
  return SlashCommand{
      .name = name, .aliases = aliases, .params = params, .help = help, .takes_argument = true};
}

[[nodiscard]] constexpr SlashCommand Staff(std::string_view name, const char* help) {
  return SlashCommand{
      .name = name, .params = "<texte>", .help = help, .takes_argument = true, .staff = true};
}

constexpr auto kSlashCommands = std::to_array<SlashCommand>({
    // ── PAROLES ADRESSÉES ────────────────────────────────────────────────────
    Taking("w", "whisper", "<nom> <texte>",
           TrText("Chuchote à un joueur ; seule, compte les joueurs connectés.")),
    Taking("bc", "", "<texte>", TrText("Parle à ton équipe de champ de bataille.")),
    Taking("gc", "guildchat", "<texte>", TrText("Parle à ta guilde.")),
    Taking("cl", "clanchat", "<texte>", TrText("Parle à ton clan.")),
    Plain("battlechat", "",
          TrText("Bascule le chat de champ de bataille : ce que tu tapes part à l'équipe.")),
    // ── LE JOUEUR, LE GROUPE, LA GUILDE ─────────────────────────────────────
    Plain("sit", "stand dkswrl tjrl", TrText("S'assied, ou se relève.")),
    Taking("deal", "rjfo ryghks", "<nom>", TrText("Propose un échange à ce joueur.")),
    Taking("organize", "ruftjd", "<nom du groupe>", TrText("Crée un groupe.")),
    Taking("invite", "cheo", "<nom>", TrText("Invite ce joueur dans ton groupe.")),
    Plain("leave", "xkfxhl", TrText("Quitte ton groupe.")),
    Taking("expel", "wpaud", "<nom>", TrText("Exclut ce joueur de ton groupe.")),
    Taking("guild", "rlfem", "<nom de la guilde>", TrText("Crée une guilde.")),
    Taking("guildinvite", "rlfemcheo", "<nom>", TrText("Invite ce joueur dans ta guilde.")),
    Taking("breakguild", "gocp", "<nom de la guilde>", TrText("Dissout ta guilde.")),
    Plain("accept", "", TrText("Accepte les invitations de groupe.")),
    Plain("refuse", "", TrText("Refuse les invitations de groupe.")),
    Plain("notrade", "nt", TrText("Refuse ou accepte de nouveau les demandes d'échange.")),
    Plain("who", "wjqthrwk snrn", TrText("Compte les joueurs connectés.")),
    Plain("help", "h", TrText("Affiche l'aide des commandes du jeu.")),
    Plain("emotion", "", TrText("Liste les commandes d'émoticône.")),
    Taking("v", "", "[0-127]", TrText("Règle le volume des bruits ; seule, le dit.")),
    Taking("bv", "", "[0-127]", TrText("Règle le volume de la musique ; seule, le dit.")),
    Plain("where", "", TrText("Dit ta position.")),
    Taking("follow", "", "[<nom>]", TrText("Suit un joueur en vue ; seule, cesse de suivre.")),
    Taking("ex", "", "[<nom>]",
           TrText("Bloque les chuchotements de ce joueur ; seule, liste les bloqués.")),
    Taking("in", "", "<nom>", TrText("Autorise de nouveau ses chuchotements.")),
    Plain("exall", "", TrText("Bloque les chuchotements de tout le monde.")),
    Plain("inall", "", TrText("Autorise de nouveau tous les chuchotements.")),
    Plain("eqopen", "wkdqlrhdro 장비공개", TrText("Montre ou cache ton équipement aux autres.")),
    Plain("memo", "rldjr 기억", TrText("Mémorise ce point de warp.")),
    Plain("call", "", TrText("Accepte ou refuse l'appel d'urgence.")),
    // ── LES CARACTÉRISTIQUES ────────────────────────────────────────────────
    Taking("str+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("agi+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("vit+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("int+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("dex+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("luk+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("pow+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("sta+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("wis+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("spl+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("con+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    Taking("crt+", "", "<points>", TrText("Monte cette caractéristique de ce nombre de points.")),
    // ── LES SALONS ───────────────────────────────────────────────────────────
    Plain("chat", "coxldqkd", TrText("Crée un salon de discussion.")),
    Plain("q", "", TrText("Quitte le salon de discussion.")),
    // ── LES BASCULES DE L'AFFICHAGE ─────────────────────────────────────────
    Plain("monsterhp", "", TrText("Montre ou cache la barre de vie d'un monstre attaqué.")),
    Plain("emblem", "dpaqmffpa doaqmffpa", TrText("Montre ou cache les emblèmes de guilde.")),
    Plain("window", "wi", TrText("Aimante ou libère les fenêtres.")),
    Plain("miss", "", TrText("Montre ou cache les « Miss ».")),
    Plain("skillfail", "sf", TrText("Montre ou cache les messages d'échec de skill.")),
    Plain("quake", "", TrText("Active ou coupe les secousses d'écran.")),
    Plain("camera", "", TrText("Colle la caméra au personnage, ou la laisse rattraper.")),
    Plain("zoom", "", TrText("Étend ou ramène le dézoom.")),
    Plain("lightmap", "", TrText("Montre ou cache les ombres de la carte.")),
    Plain("fog", "", TrText("Montre ou cache le brouillard.")),
    Plain("cashshop", "", TrText("Montre ou cache le bouton du Vote Shop.")),
    Plain("noshift", "ns", TrText("Force la cible sans tenir Maj.")),
    Plain("noctrl", "nc", TrText("Poursuit l'attaque sans tenir Ctrl.")),
    Plain("shopping", "sh", TrText("Ouvre une échoppe d'un simple clic.")),
    Plain("showshop", "", TrText("Montre ou cache les panneaux d'échoppe.")),
    Plain("notalkmsg", "nm", TrText("Masque ou montre la parole dans la conversation.")),
    Plain("notalkmsg2", "nm2", TrText("Masque ou montre la parole, celle de la guilde comprise.")),
    Taking("echelle", "scale", "<facteur>",
           TrText("Règle l'échelle de l'interface ; seule, la dit.")),
    Plain("bm", "battlemode", TrText("Bascule le battle mode : la barre de saisie se replie.")),
    Plain("bgm", "", TrText("Coupe ou rétablit la musique.")),
    Plain("snd", "sound", TrText("Coupe ou rétablit les bruits.")),
    Plain("effect", "", TrText("Coupe ou rétablit les effets.")),
    Plain("mineffect", "minimize", TrText("Allège ou rétablit les effets.")),
    Plain("skyeffect", "", TrText("Coupe ou rétablit les effets du ciel.")),
    // ── LES FENÊTRES ─────────────────────────────────────────────────────────
    Plain("macros", "", TrText("Ouvre ou ferme la fenêtre des macros.")),
    Plain("savechat", "", TrText("Enregistre le journal de conversation.")),
    Plain("atlas", "recipes recettes", TrText("Ouvre l'atlas des recettes.")),
    Taking("navi", "navigation", "[<carte> <x> <y> | <recherche>]",
           TrText("Ouvre la navigation, guide vers une case ou cherche.")),
    Plain("marche", "", TrText("Ouvre le marché.")),
    Taking("db", "", "[<recherche>]", TrText("Ouvre la base de données, sur cette recherche.")),
    Plain("ranking", "", TrText("Ouvre le tableau des classements.")),
    Plain("blacksmith", "", TrText("Demande le classement des forgerons.")),
    Plain("alchemist", "", TrText("Demande le classement des alchimistes.")),
    Plain("taekwon", "", TrText("Demande le classement des taekwons.")),
    // ── LES ANNONCES DU STAFF ───────────────────────────────────────────────
    Staff("b", TrText("Annonce à tout le serveur, ton nom en tête.")),
    Staff("nb", TrText("Annonce à tout le serveur, sans ton nom.")),
    Staff("lb", TrText("Annonce sur la carte, ton nom en tête.")),
    Staff("nlb", TrText("Annonce sur la carte, sans ton nom.")),
});

constexpr SlashCommand kNumberedEmotion =
    Taking("e", "", "<numéro>", TrText("Joue l'émoticône de ce numéro."));

}  // namespace

std::span<const SlashCommand> SlashCommands() { return kSlashCommands; }

const SlashCommand& NumberedEmotionCommand() { return kNumberedEmotion; }

const char* EmotionCommandHelp() { return TrText("Joue cette émoticône."); }

const char* ClientCommandsMenuLabel() { return TrText("Commandes du client"); }

}  // namespace moonclient
