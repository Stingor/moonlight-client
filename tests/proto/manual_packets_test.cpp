#include "ro/proto/manual_packets.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_reader.h"
#include "ro/proto/packet_lengths.h"
#include "ro/proto/parsed_packets.h"

using ro::core::ByteReader;
using ro::proto::ActionRequest;
using ro::proto::CloseStorageRequest;
using ro::proto::CreatorNameRequest;
using ro::proto::DropItemRequest;
using ro::proto::EnterMapRequest;
using ro::proto::MapLoadedNotice;
using ro::proto::MatchesServerLayout;
using ro::proto::NameRequest;
using ro::proto::PacketLength;
using ro::proto::ParsedPacketFor;
using ro::proto::PickupRequest;
using ro::proto::StatUpRequest;
using ro::proto::StorageStoreRequest;
using ro::proto::StorageTakeRequest;
using ro::proto::TickRequest;
using ro::proto::TurnRequest;
using ro::proto::UnequipItemRequest;
using ro::proto::UseItemRequest;
using ro::proto::WalkRequest;

TEST_CASE("Écrits à la main : l'opcode d'entrée en jeu n'est pas celui qu'on croit",
          "[proto][manual]") {
  // Toute la documentation ancienne donne 0x0072 pour l'entrée en jeu. À cette
  // version, le serveur y a mis l'usage d'une compétence, et la table du client
  // dit la même chose. L'envoyer reviendrait à lancer un sort au lieu
  // d'entrer sur la carte.
  const auto usurped = ParsedPacketFor(0x0072);
  REQUIRE(usurped.has_value());
  CHECK(usurped->handler == "clif_parse_UseSkillToId");

  const auto enter = ParsedPacketFor(EnterMapRequest::kOpcode);
  REQUIRE(enter.has_value());
  CHECK(enter->handler == "clif_parse_WantToConnection");
}

TEST_CASE("Écrits à la main : la disposition s'accorde avec ce que le serveur lit",
          "[proto][manual]") {
  // Le serveur déclare, sur la ligne même qui enregistre ce paquet, les
  // décalages auxquels il lit ses champs. Une disposition écrite à la main qui
  // s'en écarterait lui ferait lire un jeton à la place d'un autre.
  const auto declared = ParsedPacketFor(EnterMapRequest::kOpcode);
  REQUIRE(declared.has_value());
  REQUIRE(declared->field_offsets.size() == 5);
  CHECK(declared->field_offsets[0] == 2);   // identifiant de compte
  CHECK(declared->field_offsets[1] == 6);   // identifiant de personnage
  CHECK(declared->field_offsets[2] == 10);  // premier jeton de session
  CHECK(declared->field_offsets[3] == 14);  // horloge du client
  CHECK(declared->field_offsets[4] == 18);  // sexe

  const auto length = PacketLength(EnterMapRequest::kOpcode);
  REQUIRE(length.has_value());
  CHECK(static_cast<std::size_t>(*length) == EnterMapRequest::kWireSize);
}

TEST_CASE("Écrits à la main : la demande d'entrée s'encode aux bons décalages", "[proto][manual]") {
  EnterMapRequest request;
  request.account_id = 2'000'123;
  request.character_id = 150'001;
  request.login_id1 = 0xAABBCCDD;
  request.client_tick = 42'000;
  request.sex = 1;

  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == EnterMapRequest::kWireSize);

  // Relu AUX DÉCALAGES QUE LE SERVEUR DÉCLARE, pas à ceux qu'on vient d'écrire :
  // c'est la seule lecture qui prouve quelque chose.
  const auto declared = ParsedPacketFor(EnterMapRequest::kOpcode);
  REQUIRE(declared.has_value());
  const auto& offsets = declared->field_offsets;

  ByteReader reader(*frame);
  CHECK(reader.U16() == EnterMapRequest::kOpcode);
  REQUIRE(reader.Seek(offsets[0]));
  CHECK(reader.U32() == request.account_id);
  REQUIRE(reader.Seek(offsets[1]));
  CHECK(reader.U32() == request.character_id);
  REQUIRE(reader.Seek(offsets[2]));
  CHECK(reader.U32() == request.login_id1);
  REQUIRE(reader.Seek(offsets[3]));
  CHECK(reader.U32() == request.client_tick);
  REQUIRE(reader.Seek(offsets[4]));
  CHECK(reader.U8() == request.sex);
}

TEST_CASE("Écrits à la main : l'avis de carte chargée n'a que son opcode", "[proto][manual]") {
  // Sans lui, le serveur n'envoie jamais le monde : ni les unités, ni l'état du
  // personnage. Deux octets qui décident de tout le reste.
  const auto frame = Encode(MapLoadedNotice());
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == 2);
  CHECK((*frame)[0] == 0x7D);
  CHECK((*frame)[1] == 0x00);
}

TEST_CASE("Écrits à la main : la réapparition porte son TYPE, et c'est 0", "[proto][manual]") {
  // 🔴 LE TYPE DÉCIDE DE TOUT, et les deux valeurs sont voisines : 0 fait
  // réapparaître au point de sauvegarde, 1 RENVOIE À LA SÉLECTION DE
  // PERSONNAGE. Une inversion ne se verrait qu'en jeu, et elle sortirait le
  // joueur de sa partie au moment où il voulait y revenir.
  const auto frame = Encode(ro::proto::RestartRequest{});
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == 3);
  CHECK((*frame)[0] == 0xB2);
  CHECK((*frame)[1] == 0x00);
  CHECK((*frame)[2] == ro::proto::RestartRequest::kSavePoint);
  // ⚠ ET LES DEUX TYPES NE SE CONFONDENT PAS : c'est le seul champ du paquet.
  CHECK(ro::proto::RestartRequest::kSavePoint != ro::proto::RestartRequest::kCharacterSelect);
}

TEST_CASE("Écrits à la main : se relever sur place n'a que son opcode", "[proto][manual]") {
  // Le serveur décide seul s'il l'accorde — un jeton de Siegfried, ou l'état de
  // régénération —, et son refus est silencieux. Deux octets, rien d'autre.
  const auto frame = Encode(ro::proto::AutoReviveRequest{});
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == 2);
  CHECK((*frame)[0] == 0x92);
  CHECK((*frame)[1] == 0x02);
}

TEST_CASE("Écrits à la main : mémoriser la position n'a que son opcode", "[proto][manual]") {
  // Le serveur prend la carte et la case de sa session : l'opcode 0x011D seul.
  const auto frame = Encode(ro::proto::RememberWarpPointRequest{});
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == 2);
  CHECK((*frame)[0] == 0x1D);
  CHECK((*frame)[1] == 0x01);
}

TEST_CASE("Écrits à la main : une disposition qui dérive est refusée", "[proto][manual]") {
  // Le contrôle est fait à chaque encodage. S'il ne mordait pas, la dérive
  // passerait inaperçue jusqu'à ce que le serveur lise de travers.
  const std::vector<std::uint16_t> exact = {2, 6, 10, 14, 18};
  const std::vector<std::uint16_t> extra = {2, 6, 10, 14, 18, 22};
  const std::string_view handler = "clif_parse_WantToConnection";

  CHECK(MatchesServerLayout(EnterMapRequest::kOpcode, handler, exact));
  // Un gestionnaire qui n'est pas le sien.
  CHECK_FALSE(MatchesServerLayout(EnterMapRequest::kOpcode, "clif_parse_LoadEndAck", exact));
  // Les bons décalages, mais un de trop.
  CHECK_FALSE(MatchesServerLayout(EnterMapRequest::kOpcode, handler, extra));
  // Un opcode que le serveur ne lit pas.
  CHECK_FALSE(MatchesServerLayout(0xFFFF, handler, exact));
}

TEST_CASE("Écrits à la main : le serveur accepte quatre opcodes pour marcher", "[proto][manual]") {
  // C'est ainsi qu'il sert plusieurs générations de clients à la fois. Trois
  // portent la même disposition, le quatrième lit la destination ailleurs — le
  // confondre avec eux ferait marcher vers une case tirée du bourrage.
  //
  // Ce test n'existe pas pour décrire le serveur mais pour garder le CHOIX : si
  // une régénération déplaçait notre opcode ou changeait son décalage, il
  // échouerait ici plutôt qu'en jeu.
  for (const std::uint16_t opcode : std::array<std::uint16_t, 3>{0x035F, 0x0881, 0x0953}) {
    const auto declared = ro::proto::ParsedPacketFor(opcode);
    REQUIRE(declared.has_value());
    CHECK(declared->handler == "clif_parse_WalkToXY");
    REQUIRE(declared->field_offsets.size() == 1);
    CHECK(declared->field_offsets[0] == 2);
  }

  const auto odd_one = ro::proto::ParsedPacketFor(0x00a7);
  REQUIRE(odd_one.has_value());
  CHECK(odd_one->handler == "clif_parse_WalkToXY");
  REQUIRE(odd_one->field_offsets.size() == 1);
  CHECK(odd_one->field_offsets[0] == 6);
}

TEST_CASE("Écrits à la main : la marche porte la destination juste après l'opcode",
          "[proto][manual]") {
  WalkRequest request;
  request.packed_destination = {0x11, 0x22, 0x33};

  const auto frame = ro::proto::Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == WalkRequest::kWireSize);

  ByteReader reader(*frame);
  CHECK(reader.U16() == WalkRequest::kOpcode);
  CHECK(reader.U8() == 0x11);
  CHECK(reader.U8() == 0x22);
  CHECK(reader.U8() == 0x33);
  CHECK(reader.ok());
}

TEST_CASE("Écrits à la main : la marche s'accorde avec la table du serveur", "[proto][manual]") {
  const auto length = PacketLength(WalkRequest::kOpcode);
  REQUIRE(length.has_value());
  CHECK(static_cast<std::size_t>(*length) == WalkRequest::kWireSize);

  static constexpr std::array<std::uint16_t, 1> kOffsets = {2};
  CHECK(MatchesServerLayout(WalkRequest::kOpcode, "clif_parse_WalkToXY", kOffsets));
}

TEST_CASE("Écrits à la main : frapper et ramasser viennent du même bloc que la marche",
          "[proto][manual]") {
  // Le dernier bloc que 20250716 traverse donne 0x0881 à la marche, 0x088e aux
  // actions et 0x0933 au ramassage. Ce test garde le CHOIX : une régénération
  // qui déplacerait l'un d'eux échouerait ici plutôt qu'en jeu.
  const auto action = ParsedPacketFor(ActionRequest::kOpcode);
  REQUIRE(action.has_value());
  CHECK(action->handler == "clif_parse_ActionRequest");
  REQUIRE(action->field_offsets.size() == 2);
  CHECK(action->field_offsets[0] == 2);  // la cible
  CHECK(action->field_offsets[1] == 6);  // l'action
  const auto action_length = PacketLength(ActionRequest::kOpcode);
  REQUIRE(action_length.has_value());
  CHECK(static_cast<std::size_t>(*action_length) == ActionRequest::kWireSize);

  const auto pickup = ParsedPacketFor(PickupRequest::kOpcode);
  REQUIRE(pickup.has_value());
  CHECK(pickup->handler == "clif_parse_TakeItem");
  REQUIRE(pickup->field_offsets.size() == 1);
  CHECK(pickup->field_offsets[0] == 2);  // l'objet
  const auto pickup_length = PacketLength(PickupRequest::kOpcode);
  REQUIRE(pickup_length.has_value());
  CHECK(static_cast<std::size_t>(*pickup_length) == PickupRequest::kWireSize);
}

TEST_CASE("Écrits à la main : la demande de nom passe par le DERNIER bloc qui l'inscrit",
          "[proto][manual]") {
  // 🔴 ET CE BLOC N'EST PAS DANS `clif_packetdb.hpp`. Le serveur y inscrit
  // `clif_parse_GetCharNameRequest` sous quatre numéros au fil des versions —
  // 0x0368, 0x096A, 0x094A, puis 0x0898 —, et `clif_shuffle.hpp`, inclus APRÈS,
  // le remet sous 0x0368 pour tout PACKETVER au-dessus de 20180307. S'arrêter au
  // premier fichier fait émettre 0x0898, que le serveur lit alors comme une
  // autre demande.
  //
  // ⚠ ET 0x0368 A PORTÉ L'AUTRE SENS : deux blocs intermédiaires l'attribuent à
  // `clif_parse_SolveCharName`, qui répond sous un identifiant de PERSONNAGE.
  // Les deux paquets font six octets et lisent au même décalage : les confondre
  // ne raterait rien, il chercherait un autre annuaire.
  const auto name = ParsedPacketFor(NameRequest::kOpcode);
  REQUIRE(name.has_value());
  CHECK(name->handler == "clif_parse_GetCharNameRequest");
  REQUIRE(name->field_offsets.size() == 1);
  CHECK(name->field_offsets[0] == 2);  // l'acteur
  const auto name_length = PacketLength(NameRequest::kOpcode);
  REQUIRE(name_length.has_value());
  CHECK(static_cast<std::size_t>(*name_length) == NameRequest::kWireSize);

  NameRequest request;
  request.actor_id = 0x0102ABCD;
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == NameRequest::kWireSize);
  CHECK((*frame)[0] == 0x68);
  CHECK((*frame)[1] == 0x03);
  CHECK((*frame)[2] == 0xCD);
  CHECK((*frame)[5] == 0x01);
}

TEST_CASE("Écrits à la main : le demi-tour n'est pas sous 0x009B, et il est TROUÉ",
          "[proto][manual]") {
  // 🔴 TOUTE LA DOCUMENTATION ANCIENNE DONNE 0x009B À CETTE DEMANDE. À cette
  // version, 0x009B porte l'entrée en jeu — et le serveur inscrit
  // `clif_parse_ChangeDir` sous six numéros au fil des versions. Un seul est
  // long de DIX octets ; les autres en font cinq. Se tromper ferait lire la
  // direction dans le bourrage d'un paquet plus court.
  const auto usurped = ParsedPacketFor(0x009B);
  REQUIRE(usurped.has_value());
  CHECK(usurped->handler != "clif_parse_ChangeDir");

  const auto turn = ParsedPacketFor(TurnRequest::kOpcode);
  REQUIRE(turn.has_value());
  CHECK(turn->handler == "clif_parse_ChangeDir");
  // ⚠ ET LES DEUX CHAMPS NE SE SUIVENT PAS. La structure du serveur déclare la
  // tête sur deux octets, mais `clif_parse_ChangeDir` la lit par RFIFOB, un
  // seul octet, au décalage quatre — et le corps au neuvième. Quatre octets de
  // bourrage séparent les deux.
  REQUIRE(turn->field_offsets.size() == 2);
  CHECK(turn->field_offsets[0] == 4);  // le regard
  CHECK(turn->field_offsets[1] == 9);  // le corps
  const auto turn_length = PacketLength(TurnRequest::kOpcode);
  REQUIRE(turn_length.has_value());
  CHECK(static_cast<std::size_t>(*turn_length) == TurnRequest::kWireSize);

  TurnRequest request;
  request.head_direction = 2;
  request.direction = 6;
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == TurnRequest::kWireSize);
  CHECK((*frame)[0] == 0x85);
  CHECK((*frame)[1] == 0x00);
  CHECK((*frame)[4] == 2);
  CHECK((*frame)[9] == 6);
  // Le bourrage est NUL, et non laissé au hasard de la pile.
  CHECK((*frame)[2] == 0);
  CHECK((*frame)[3] == 0);
  CHECK((*frame)[5] == 0);
  CHECK((*frame)[8] == 0);
}

TEST_CASE("Écrits à la main : le vieil opcode d'action est tout de bourrage", "[proto][manual]") {
  // 0x0190 est encore accepté, mais à vingt-trois octets dont dix-huit ne
  // portent rien : la cible au neuvième, l'action au vingt-deuxième. On ne
  // l'emploie pas, et ce test dit pourquoi.
  const auto old = ParsedPacketFor(0x0190);
  REQUIRE(old.has_value());
  CHECK(old->handler == "clif_parse_ActionRequest");
  REQUIRE(old->field_offsets.size() == 2);
  CHECK(old->field_offsets[0] == 9);
  CHECK(old->field_offsets[1] == 22);
}

TEST_CASE("Écrits à la main : l'action s'encode aux décalages que le serveur lit",
          "[proto][manual]") {
  ActionRequest request;
  request.target_id = 0x01020304;
  request.action = ActionRequest::kAttackRepeatedly;

  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == ActionRequest::kWireSize);

  const auto declared = ParsedPacketFor(ActionRequest::kOpcode);
  REQUIRE(declared.has_value());
  ByteReader reader(*frame);
  CHECK(reader.U16() == ActionRequest::kOpcode);
  REQUIRE(reader.Seek(declared->field_offsets[0]));
  CHECK(reader.U32() == 0x01020304);
  REQUIRE(reader.Seek(declared->field_offsets[1]));
  CHECK(reader.U8() == 7);
  CHECK(reader.ok());
}

TEST_CASE("Écrits à la main : le ramassage s'encode aux décalages que le serveur lit",
          "[proto][manual]") {
  PickupRequest request;
  request.item_id = 0xAABBCCDD;

  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == PickupRequest::kWireSize);

  const auto declared = ParsedPacketFor(PickupRequest::kOpcode);
  REQUIRE(declared.has_value());
  ByteReader reader(*frame);
  CHECK(reader.U16() == PickupRequest::kOpcode);
  REQUIRE(reader.Seek(declared->field_offsets[0]));
  CHECK(reader.U32() == 0xAABBCCDD);
  CHECK(reader.ok());
}

TEST_CASE("Écrits à la main : la parole porte le NOM de qui parle", "[proto][manual]") {
  // Le serveur compare ce nom au sien et rejette tout le message s'il diffère.
  // C'est aussi le seul paquet sortant de longueur variable : sa tête la porte.
  ro::proto::ChatRequest request;
  request.speaker = "Alice";
  request.text = "@speed 200";

  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  const std::string body = "Alice : @speed 200";
  REQUIRE(frame->size() == ro::proto::ChatRequest::kHeaderSize + body.size() + 1);

  const auto declared = ParsedPacketFor(ro::proto::ChatRequest::kOpcode);
  REQUIRE(declared.has_value());
  CHECK(declared->handler == "clif_parse_GlobalMessage");
  // La table du serveur dit moins un : la longueur est dans le paquet.
  const auto length = PacketLength(ro::proto::ChatRequest::kOpcode);
  REQUIRE(length.has_value());
  CHECK(*length == ro::proto::kVariableLength);

  ByteReader reader(*frame);
  CHECK(reader.U16() == ro::proto::ChatRequest::kOpcode);
  REQUIRE(reader.Seek(declared->field_offsets[0]));
  CHECK(reader.U16() == frame->size());
  REQUIRE(reader.Seek(declared->field_offsets[1]));
  for (const char letter : body) {
    CHECK(reader.U8() == static_cast<std::uint8_t>(letter));
  }
  // Le zéro terminal compte dans la longueur : le serveur lit une chaîne C.
  CHECK(reader.U8() == 0);
  CHECK(reader.ok());
}

TEST_CASE("Écrits à la main : une parole trop longue est refusée", "[proto][manual]") {
  // Au-delà de ce que le serveur accepte, il jette le message entier : mieux
  // vaut le dire ici que de croire l'avoir envoyé.
  ro::proto::ChatRequest request;
  request.speaker = "Alice";
  request.text = std::string(300, 'a');
  CHECK_FALSE(Encode(request).has_value());
}

TEST_CASE("Écrits à la main : l'émoticône passe par le paquet ÉTENDU", "[proto][manual]") {
  // Le vieux paquet existe encore et le serveur l'écoute, mais son gestionnaire
  // relit l'octet de poids faible de l'opcode faute de décalage déclaré. Le
  // client moderne emploie celui des émoticônes de boutique, lot zéro.
  ro::proto::EmotionRequest request;
  request.emotion_id = 33;

  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == ro::proto::EmotionRequest::kWireSize);

  const auto declared = ParsedPacketFor(ro::proto::EmotionRequest::kOpcode);
  REQUIRE(declared.has_value());
  CHECK(declared->handler == "clif_parse_cash_emotion_use");
  // Le gestionnaire lit ses deux champs en dur : la table n'en déclare aucun.
  CHECK(declared->field_offsets.empty());

  ByteReader reader(*frame);
  CHECK(reader.U16() == ro::proto::EmotionRequest::kOpcode);
  CHECK(reader.U16() == ro::proto::EmotionRequest::kBasePack);
  CHECK(reader.U16() == 33);
  CHECK(reader.ok());
}

TEST_CASE("Paquets manuels : l'effet porte la taille d'un corps", "[proto][manual]") {
  // Le serveur écrit ce paquet à la main, sans structure : opcode, identifiant
  // de l'acteur, numéro d'effet. C'est le seul chemin par lequel la taille d'un
  // corps arrive au client.
  const std::array<std::uint8_t, 10> frame = {0xF3, 0x01, 0x2A, 0x00, 0x00,
                                              0x00, 0xA6, 0x01, 0x00, 0x00};
  ro::proto::SpecialEffectNotice notice;
  REQUIRE(Decode(frame, notice));
  CHECK(notice.actor_id == 42);
  // 422 : EF_GIANTBODY, celui qui fait grandir un corps.
  CHECK(notice.effect_id == 422);

  // Une trame trop courte, ou d'un autre opcode, ne se lit pas.
  const std::array<std::uint8_t, 9> court = {0xF3, 0x01, 0x2A, 0x00, 0x00, 0x00, 0xA6, 0x01, 0x00};
  CHECK_FALSE(Decode(court, notice));
  std::array<std::uint8_t, 10> other = frame;
  other[0] = 0xF4;
  CHECK_FALSE(Decode(other, notice));
}

TEST_CASE("Paquets manuels : l'effet à valeur tient sur quatorze octets", "[proto][manual]") {
  // ⚠ Ce n'est PAS la structure engendrée. Le serveur déclare pour notre
  // PACKETVER un ZC_NOTIFY_EFFECT3 d'opcode 0x0B69 dont la valeur fait
  // soixante-quatre bits, et ne l'émet jamais : clif_specialeffect_value écrit
  // 0x0284 en dur sur quatorze octets, valeur comprise sur trente-deux bits.
  const std::array<std::uint8_t, 14> frame = {0x84, 0x02, 0x2A, 0x00, 0x00, 0x00, 0x91,
                                              0x02, 0x00, 0x00, 0x2C, 0x01, 0x00, 0x00};
  ro::proto::ValuedEffectNotice notice;
  REQUIRE(Decode(frame, notice));
  CHECK(notice.actor_id == 42);
  CHECK(notice.effect_id == 657);
  CHECK(notice.value == 300);

  // La valeur est SIGNÉE : le client l'étend au signe avant de la montrer.
  std::array<std::uint8_t, 14> negative = frame;
  negative[10] = 0xFF;
  negative[11] = 0xFF;
  negative[12] = 0xFF;
  negative[13] = 0xFF;
  REQUIRE(Decode(negative, notice));
  CHECK(notice.value == -1);

  // La longueur que le serveur déclare pour cet opcode est bien celle-là : si
  // elle changeait, le lecteur de trames couperait ailleurs et cet essai ne le
  // verrait pas.
  const auto length = PacketLength(ro::proto::ValuedEffectNotice::kOpcode);
  REQUIRE(length.has_value());
  CHECK(static_cast<std::size_t>(*length) == ro::proto::ValuedEffectNotice::kWireSize);

  // Une trame de la taille de l'AUTRE forme ne se lit pas.
  const std::array<std::uint8_t, 18> longue = {0x84, 0x02, 0x2A, 0x00, 0x00, 0x00,
                                               0x91, 0x02, 0x00, 0x00, 0x2C, 0x01,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  CHECK_FALSE(Decode(longue, notice));
  std::array<std::uint8_t, 14> other = frame;
  other[0] = 0x85;
  CHECK_FALSE(Decode(other, notice));
}

TEST_CASE("Écrits à la main : le battement d'horloge tient la session ouverte", "[proto][manual]") {
  // Ce paquet n'est pas décoratif. Le serveur coupe toute session dont il n'a
  // rien reçu depuis stall_time secondes — soixante par défaut — et la coupure
  // est PROPRE, donc muette : un client qui ne bat pas la mesure tombe au bout
  // d'une minute d'inactivité sans qu'aucune erreur ne soit dite.
  //
  // Quatre versions du même paquet se succèdent dans la table du serveur ;
  // c'est la DERNIÈRE inscrite qui vaut, et non la première trouvée.
  //
  // ⚠ La dernière n'est pas dans la table de base. Le serveur monte la sienne
  // en deux temps — sa table, puis le MÉLANGE qui la recouvre — et le mélange
  // reprend 0x0363, celui que la table de base donne au battement, pour le
  // diriger vers la CHUTE D'UN OBJET. Cet essai garde les deux : le nôtre, et
  // celui qu'il ne faut pas prendre.
  const auto stolen = ParsedPacketFor(0x0363);
  REQUIRE(stolen.has_value());
  CHECK(stolen->handler == "clif_parse_DropItem");

  const auto tick = ParsedPacketFor(TickRequest::kOpcode);
  REQUIRE(tick.has_value());
  CHECK(tick->handler == "clif_parse_TickSend");
  REQUIRE(tick->field_offsets.size() == 1);
  CHECK(tick->field_offsets[0] == 2);

  const auto length = PacketLength(TickRequest::kOpcode);
  REQUIRE(length.has_value());
  CHECK(static_cast<std::size_t>(*length) == TickRequest::kWireSize);

  TickRequest request;
  request.client_tick = 0x1234'5678;
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == TickRequest::kWireSize);
  // Petit-boutiste, comme tout le reste du protocole.
  const std::vector<std::uint8_t> expected = {0x60, 0x03, 0x78, 0x56, 0x34, 0x12};
  CHECK(*frame == expected);
}

TEST_CASE("Verbes du sac : le serveur ecoute DEUX opcodes pour utiliser, et un seul convient",
          "[proto][manual]") {
  // 🔴 LE PIEGE DE CE LOT. `clif_parse_UseItem` repond a 0x009F comme a 0x0439 :
  // meme geste, dispositions DIFFERENTES. Choisir au hasard n'echoue pas — le
  // serveur lit des octets aux mauvaises places et agit sur autre chose.
  //
  // Ce qui tranche est la table de longueurs du CLIENT : 0x009F
  // y tient sur vingt octets, et le serveur declare y lire un champ a l'offset
  // VINGT — donc hors du paquet. Il ne peut pas etre celui-la.
  const auto usurped = ParsedPacketFor(0x009F);
  REQUIRE(usurped.has_value());
  CHECK(usurped->handler == "clif_parse_UseItem");
  const auto usurped_length = PacketLength(0x009F);
  REQUIRE(usurped_length.has_value());
  REQUIRE(usurped->field_offsets.size() == 2);
  CHECK(usurped->field_offsets[1] >= static_cast<std::uint16_t>(*usurped_length));

  const auto use = ParsedPacketFor(UseItemRequest::kOpcode);
  REQUIRE(use.has_value());
  CHECK(use->handler == "clif_parse_UseItem");
  REQUIRE(use->field_offsets.size() == 2);
  const auto use_length = PacketLength(UseItemRequest::kOpcode);
  REQUIRE(use_length.has_value());
  CHECK(static_cast<std::size_t>(*use_length) == UseItemRequest::kWireSize);
}

TEST_CASE("Verbes du sac : retirer et jeter s'accordent avec ce que le serveur lit",
          "[proto][manual]") {
  const auto takeoff = ParsedPacketFor(UnequipItemRequest::kOpcode);
  REQUIRE(takeoff.has_value());
  CHECK(takeoff->handler == "clif_parse_UnequipItem");
  CHECK(takeoff->field_offsets.size() == 1);
  const auto takeoff_length = PacketLength(UnequipItemRequest::kOpcode);
  REQUIRE(takeoff_length.has_value());
  CHECK(static_cast<std::size_t>(*takeoff_length) == UnequipItemRequest::kWireSize);

  const auto drop = ParsedPacketFor(DropItemRequest::kOpcode);
  REQUIRE(drop.has_value());
  CHECK(drop->handler == "clif_parse_DropItem");
  CHECK(drop->field_offsets.size() == 2);
  const auto drop_length = PacketLength(DropItemRequest::kOpcode);
  REQUIRE(drop_length.has_value());
  CHECK(static_cast<std::size_t>(*drop_length) == DropItemRequest::kWireSize);
}

TEST_CASE("Verbes du sac : les champs s'ecrivent AUX DECALAGES QUE LE SERVEUR LIT",
          "[proto][manual]") {
  // ⚠ ON RELIT AUX DECALAGES DECLARES, jamais a des nombres ecrits ici : c'est
  // ce qui fait de cet essai autre chose qu'une repetition de l'encodeur.
  UseItemRequest use;
  use.client_slot = 0x1234;
  use.account_id = 0x0A0B0C0D;
  const auto used = Encode(use);
  REQUIRE(used.has_value());
  REQUIRE(used->size() == UseItemRequest::kWireSize);
  {
    const auto declared = ParsedPacketFor(UseItemRequest::kOpcode);
    REQUIRE(declared.has_value());
    ByteReader reader(*used);
    CHECK(reader.U16() == UseItemRequest::kOpcode);
    REQUIRE(reader.Seek(declared->field_offsets[0]));
    CHECK(reader.U16() == 0x1234);
    REQUIRE(reader.Seek(declared->field_offsets[1]));
    CHECK(reader.U32() == 0x0A0B0C0D);
    CHECK(reader.ok());
  }

  UnequipItemRequest takeoff;
  takeoff.client_slot = 7;
  const auto taken = Encode(takeoff);
  REQUIRE(taken.has_value());
  REQUIRE(taken->size() == UnequipItemRequest::kWireSize);
  {
    const auto declared = ParsedPacketFor(UnequipItemRequest::kOpcode);
    REQUIRE(declared.has_value());
    ByteReader reader(*taken);
    CHECK(reader.U16() == UnequipItemRequest::kOpcode);
    REQUIRE(reader.Seek(declared->field_offsets[0]));
    CHECK(reader.U16() == 7);
    CHECK(reader.ok());
  }

  DropItemRequest drop;
  drop.client_slot = 9;
  drop.amount = 250;
  const auto dropped = Encode(drop);
  REQUIRE(dropped.has_value());
  REQUIRE(dropped->size() == DropItemRequest::kWireSize);
  {
    const auto declared = ParsedPacketFor(DropItemRequest::kOpcode);
    REQUIRE(declared.has_value());
    ByteReader reader(*dropped);
    CHECK(reader.U16() == DropItemRequest::kOpcode);
    REQUIRE(reader.Seek(declared->field_offsets[0]));
    CHECK(reader.U16() == 9);
    REQUIRE(reader.Seek(declared->field_offsets[1]));
    CHECK(reader.U16() == 250);
    CHECK(reader.ok());
  }
}

TEST_CASE("Coffre : les trois verbes ont ete RENUMEROTES, et pas tous au meme moment",
          "[proto][manual][coffre]") {
  // 🔴 LE PIEGE. 0x00F3, 0x00F5 et 0x00F7 sont les numeros que toute la
  // documentation nomme encore. Le melange de 2013 a deplace les deux
  // TRANSFERTS et laisse la FERMETURE ou elle etait : supposer qu'il les a tous
  // pris se trompe, et supposer qu'il n'a rien pris se trompe deux fois.
  CHECK(StorageStoreRequest::kOpcode == 0x08AC);
  CHECK(StorageTakeRequest::kOpcode == 0x0874);
  CHECK(CloseStorageRequest::kOpcode == 0x0193);

  const auto store = ParsedPacketFor(StorageStoreRequest::kOpcode);
  REQUIRE(store.has_value());
  CHECK(store->handler == "clif_parse_MoveToKafra");
  const auto take = ParsedPacketFor(StorageTakeRequest::kOpcode);
  REQUIRE(take.has_value());
  CHECK(take->handler == "clif_parse_MoveFromKafra");
  const auto close = ParsedPacketFor(CloseStorageRequest::kOpcode);
  REQUIRE(close.has_value());
  CHECK(close->handler == "clif_parse_CloseKafra");

  // 🔴 ET LES ANCIENS NUMEROS ONT ETE REPRIS PAR D'AUTRES GESTES. C'est ce qui
  // fait le cout de l'erreur : elle ne ferait pas rien, elle ferait autre chose.
  // L'essai le CONSTATE -- et c'est lui qui a corrige la premiere version de ce
  // commentaire, qui les croyait tous trois encore braques sur le coffre.
  const auto reprise_f3 = ParsedPacketFor(0x00F3);
  REQUIRE(reprise_f3.has_value());
  CHECK(reprise_f3->handler == "clif_parse_GlobalMessage");  // le personnage PARLE
  const auto reprise_f5 = ParsedPacketFor(0x00F5);
  REQUIRE(reprise_f5.has_value());
  CHECK(reprise_f5->handler == "clif_parse_TakeItem");  // il ramasse au sol

  // Le pire des trois : il garde un sens de coffre, mais c'est l'AUTRE SENS.
  // Fermer le coffre par ce numero-la viderait une case au hasard.
  const auto reprise_f7 = ParsedPacketFor(0x00F7);
  REQUIRE(reprise_f7.has_value());
  CHECK(reprise_f7->handler == "clif_parse_MoveFromKafra");
  CHECK(reprise_f7->handler != close->handler);

  // La table de longueurs du CLIENT : huit octets pour les deux
  // transferts, deux pour la fermeture, qui n'a aucun champ.
  const auto store_length = PacketLength(StorageStoreRequest::kOpcode);
  REQUIRE(store_length.has_value());
  CHECK(static_cast<std::size_t>(*store_length) == StorageStoreRequest::kWireSize);
  const auto take_length = PacketLength(StorageTakeRequest::kOpcode);
  REQUIRE(take_length.has_value());
  CHECK(static_cast<std::size_t>(*take_length) == StorageTakeRequest::kWireSize);
  const auto close_length = PacketLength(CloseStorageRequest::kOpcode);
  REQUIRE(close_length.has_value());
  CHECK(static_cast<std::size_t>(*close_length) == CloseStorageRequest::kWireSize);
  CHECK(close->field_offsets.empty());
}

TEST_CASE("Coffre : la quantite prend QUATRE octets, la ou le jet n'en prend que deux",
          "[proto][manual][coffre]") {
  // Le serveur lit cette quantite avec RFIFOL. L'ecrire sur deux octets
  // laisserait deux octets de reste, et le paquet ne ferait plus la taille que
  // la table annonce.
  StorageStoreRequest store;
  store.client_slot = 42;
  store.amount = 70000;  // au-dela de ce que seize bits portent
  const auto stored = Encode(store);
  REQUIRE(stored.has_value());
  REQUIRE(stored->size() == StorageStoreRequest::kWireSize);
  {
    const auto declared = ParsedPacketFor(StorageStoreRequest::kOpcode);
    REQUIRE(declared.has_value());
    REQUIRE(declared->field_offsets.size() == 2);
    ByteReader reader(*stored);
    CHECK(reader.U16() == StorageStoreRequest::kOpcode);
    REQUIRE(reader.Seek(declared->field_offsets[0]));
    CHECK(reader.U16() == 42);
    REQUIRE(reader.Seek(declared->field_offsets[1]));
    CHECK(reader.U32() == 70000);
    CHECK(reader.ok());
  }

  StorageTakeRequest take;
  take.client_slot = 7;
  take.amount = 3;
  const auto taken = Encode(take);
  REQUIRE(taken.has_value());
  REQUIRE(taken->size() == StorageTakeRequest::kWireSize);
  {
    const auto declared = ParsedPacketFor(StorageTakeRequest::kOpcode);
    REQUIRE(declared.has_value());
    ByteReader reader(*taken);
    CHECK(reader.U16() == StorageTakeRequest::kOpcode);
    REQUIRE(reader.Seek(declared->field_offsets[0]));
    CHECK(reader.U16() == 7);
    REQUIRE(reader.Seek(declared->field_offsets[1]));
    CHECK(reader.U32() == 3);
    CHECK(reader.ok());
  }

  // La fermeture n'est QUE son opcode.
  const auto closed = Encode(CloseStorageRequest{});
  REQUIRE(closed.has_value());
  REQUIRE(closed->size() == CloseStorageRequest::kWireSize);
  ByteReader reader(*closed);
  CHECK(reader.U16() == CloseStorageRequest::kOpcode);
  CHECK(reader.ok());
}

TEST_CASE("Ecrits a la main : deux paquets montent des caracteristiques, et pas les memes",
          "[proto][manual][fiche]") {
  // 🔴 LE PIEGE. Le serveur DECLARE une structure pour 0x0B24, donc le generateur
  // l'a produite -- mais il la lit avec clif_parse_traitstatus_up, qui monte les
  // caracteristiques de METIER des quatriemes classes. Les six de la fiche
  // passent par 0x00BB, que lit clif_parse_StatusUp. Se tromper n'aurait produit
  // aucune erreur : l'autre paquet est accepte, et il monte autre chose.
  const auto notre = ParsedPacketFor(StatUpRequest::kOpcode);
  REQUIRE(notre.has_value());
  CHECK(notre->handler == "clif_parse_StatusUp");

  const auto neighbour = ParsedPacketFor(0x0B24);
  REQUIRE(neighbour.has_value());
  CHECK(neighbour->handler == "clif_parse_traitstatus_up");
  CHECK(neighbour->handler != notre->handler);

  // Les deux decalages que le serveur declare sur la ligne meme qui enregistre
  // le paquet.
  REQUIRE(notre->field_offsets.size() == 2);
  CHECK(notre->field_offsets[0] == 2);
  CHECK(notre->field_offsets[1] == 4);

  const auto length = PacketLength(StatUpRequest::kOpcode);
  REQUIRE(length.has_value());
  CHECK(static_cast<std::size_t>(*length) == StatUpRequest::kWireSize);
}

TEST_CASE("Ecrits a la main : la montee de caracteristique tient sur cinq octets",
          "[proto][manual][fiche]") {
  StatUpRequest request;
  request.parameter = 13;  // SP_STR
  request.amount = 3;

  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == StatUpRequest::kWireSize);

  const auto declared = ParsedPacketFor(StatUpRequest::kOpcode);
  REQUIRE(declared.has_value());
  const auto& offsets = declared->field_offsets;

  ByteReader reader(*frame);
  CHECK(reader.U16() == StatUpRequest::kOpcode);
  CHECK((*frame)[offsets[0]] == 13);
  CHECK((*frame)[static_cast<std::size_t>(offsets[0]) + 1] == 0);

  // ⚠ UN SEUL OCTET POUR LA QUANTITE. L'ecrire sur deux decalerait tout et le
  // serveur lirait le numero de la caracteristique de travers.
  CHECK((*frame)[offsets[1]] == 3);
  CHECK(offsets[1] + 1 == StatUpRequest::kWireSize);
}

TEST_CASE("Écrits à la main : le nom d'un forgeron se demande sous 0x0369", "[proto][manual]") {
  // ⚠ SOUS UN IDENTIFIANT DE PERSONNAGE, et par un autre gestionnaire que la
  // demande de nom d'un acteur : les deux font six octets et lisent au même
  // décalage, seul le gestionnaire les distingue.
  const auto declared = ParsedPacketFor(CreatorNameRequest::kOpcode);
  REQUIRE(declared.has_value());
  CHECK(declared->handler == "clif_parse_SolveCharName");
  REQUIRE(declared->field_offsets.size() == 1);
  CHECK(declared->field_offsets[0] == 2);

  CreatorNameRequest request;
  request.character_id = 150001;
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t> expected = {0x69, 0x03, 0xF1, 0x49, 0x02, 0x00};
  CHECK(*frame == expected);
}
