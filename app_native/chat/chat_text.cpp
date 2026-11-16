// The chat's texts: chat_text.h. Spanish follows the ports' rules (never the console maker's or the publisher's names).
#include "chat_text.h"

#include <atomic>
#include <cstdio>
#include <ctime>

namespace chat {
namespace {
std::atomic<bool> g_es{false};
std::string fmt1(const char *f, const std::string &a) {
    char b[512];
    snprintf(b, sizeof b, f, a.c_str());
    return b;
}
std::string game_title(const std::string &g) { return g == "mh4u" ? "MH4U" : "MHGU"; }
}  // namespace

void set_spanish(bool on) { g_es = on; }
bool spanish() { return g_es; }

std::string explain(const std::string &coded) {
    if (coded.size() < 2 || coded[0] != '\x01') return coded;
    size_t end = coded.find('\x01', 1);
    std::string what = coded.substr(1, end == std::string::npos ? std::string::npos : end - 1);
    std::string arg = end == std::string::npos ? "" : coded.substr(end + 1);
    if (what == "wait") return fmt1(t("Not so fast: wait %s s.", "No tan rápido: espera %s s."), arg);
    if (what == "same") return t("You just said that.", "Acabas de decir eso.");
    if (what == "offline")
        return t("The online chat is not connected: turn it on, or wait until a relay answers.",
                 "El chat en línea no está conectado: actívalo, o espera a que responda un relé.");
    if (what == "away") return fmt1(t("%s is not online: private messages reach only a friend who is connected.",
                                      "%s no está en línea: los mensajes privados solo le llegan a un amigo conectado."), arg);
    if (what == "notyet") return fmt1(t("%s has not added you back yet: they get your request when they are online.",
                                        "%s todavía no te agregó: le llega tu solicitud cuando esté en línea."), arg);
    if (what == "nohub")
        return game() == "mh4u" ? t("You are not in a room.", "No estás en una sala.") : t("You are not in a hub.", "No estás en una sala.");
    if (what == "hubchat")
        return game() == "mh4u" ? t("The room chat is off: turn it on above to talk in the room.",
                                    "El chat de la sala está apagado: actívalo arriba para hablar en la sala.")
                                : t("The hub chat is off: turn it on above to talk in the hub.",
                                    "El chat de la sala está apagado: actívalo arriba para hablar en la sala.");
    if (what == "nohubnet")
        return t("You are not in a hub: host one or join one in the game, and its players can read you here.",
                 "No estás en un hub: crea uno o entra a uno en el juego, y sus jugadores te leen aquí.");
    if (what == "hubshare")
        return t("Everyone here is already with you: share it with a friend, or in Global or General.",
                 "Todos acá ya están contigo: compártelo con un amigo, o en Global o General.");
    if (what == "badcode")
        return t("That is not a friend code: 12 letters and digits, as XXXX-XXXX-XXXX.",
                 "Eso no es un código de amigo: 12 letras y números, como XXXX-XXXX-XXXX.");
    if (what == "yours") return t("That is your own code.", "Ese es tu propio código.");
    if (what == "already") return fmt1(t("%s is already your friend.", "%s ya es tu amigo."), arg);
    if (what == "silenced")
        return fmt1(t("%s sent too many lines and is silenced here for 5 minutes.",
                      "%s mandó demasiadas líneas y queda silenciado acá por 5 minutos."), arg);
    if (what == "request")
        return fmt1(t("%s added you as a friend. Accept in Friends to talk privately.",
                      "%s te agregó como amigo. Acepta en Amigos para hablar en privado."), arg);
    if (what == "added")
        return fmt1(t("%s was found online and added. You can talk once they add you back.",
                      "%s fue encontrado en línea y agregado. Pueden hablar cuando te agregue también."), arg);
    if (what == "accepted") return fmt1(t("%s added you back: you can talk privately now.", "%s te agregó también: ya pueden hablar en privado."), arg);
    return what;
}

std::string line_text(const Line &l) {
    if (l.system) return explain(l.text);
    std::string s = l.text;
    if (l.share == SHARE_KEY) {
        std::string what = fmt1(t("Matchmaking key %s", "Clave de emparejamiento %s"), l.share_b);
        if (!l.share_a.empty()) what += fmt1(t(", hub server %s", ", servidor de salas %s"), l.share_a);
        s += (s.empty() ? "" : " ") + std::string("[") + what + "]";
    } else if (l.share == SHARE_ROOM) {
        std::string what = fmt1(t("Room %s", "Sala %s"), l.share_a);
        if (!l.share_b.empty()) what += fmt1(t(", password %s", ", contraseña %s"), l.share_b);
        s += (s.empty() ? "" : " ") + std::string("[") + what + "]";
    }
    return s;
}

std::string who(const Line &l) {
    if (l.system) return "";
    if (l.mine) return t("You", "Tú");  // (this device's own lines: no mark of a game or a room)
    if (l.channel == HUB) return l.name + " (" + t("Room", "Sala") + ")";
    if (l.channel == HUBCHAT) return l.name + " (Hub)";
    if (l.channel == FRIEND) return l.name;
    if (l.game == "mh4u") return l.name + " (MH4U)";
    if (l.game == "mhgu") return l.name + " (MHGU)";
    return l.name;
}

std::string channel_name(int channel) {
    switch (channel) {
    case MIXED: return t("Room + Hub", "Sala + Hub");
    case MIXED_ALL: return t("Room + Hub + Global", "Sala + Hub + Global");
    case HUBCHAT: return t("Hub", "Hub");
    case GLOBAL: return t("Global", "Global");
    case GENERAL: return std::string(t("General", "General")) + " (" + game_title(game()) + ")";
    case HUB: return game() == "mh4u" ? t("Room", "Sala") : t("Hub", "Sala");
    default: return t("Friends", "Amigos");
    }
}
std::string channel_help(int channel) {
    switch (channel) {
    case MIXED:
        return t("The room and the hub as one. What you say goes to both. What the others say in one passes into the other, as "
                 "\"Name (Hub): text\" or \"Name (Room): text\" under your nickname, so Azahar and Citra players and hub players read each other.",
                 "La sala y el hub como uno. Lo que dices va a los dos. Lo que dicen los demás en uno pasa al otro, como "
                 "\"Nombre (Hub): texto\" o \"Nombre (Sala): texto\" con tu apodo, así que los de Azahar y Citra y los del hub se leen.");
    case MIXED_ALL:
        return t("The room, the hub and Global as one. What you say goes to all three and you read all three. Global is not passed "
                 "on to the others: only the room and the hub pass into each other.",
                 "La sala, el hub y Global como uno. Lo que dices va a los tres y lees los tres. Global no se pasa a los demás: solo "
                 "la sala y el hub pasan de uno al otro.");
    case HUBCHAT:
        return t("Only the players of the hub you are in, over the online chat: nobody outside it reads it.",
                 "Solo los jugadores del hub en el que estás, por el chat en línea: nadie fuera de él lo lee.");
    case GLOBAL:
        return t("Everyone in the online chat, from both ports (MHGU and MH4U). Anyone can read it: never share anything private.",
                 "Todos los del chat en línea, de los dos ports (MHGU y MH4U). Cualquiera puede leerlo: nunca compartas nada privado.");
    case GENERAL:
        return fmt1(t("Everyone in the online chat who plays %s. Anyone can read it.", "Todos los del chat en línea que juegan %s. Cualquiera puede leerlo."),
                    game_title(game()));
    case HUB:
        return game() == "mh4u"
                   ? t("The room you are in: everyone in it reads it, emulator players too. It needs no online chat.",
                       "La sala en la que estás: la leen todos los que están en ella, también los de un emulador. No necesita el chat en línea.")
                   : t("The hub you are in: the hunters in it with this version read it. It travels with the hub itself and needs no online chat.",
                       "La sala en la que estás: la leen los cazadores que están en ella con esta versión. Viaja con la sala misma y no necesita el chat en línea.");
    default:
        return t("Only that friend can read it: it is encrypted on this device.", "Solo ese amigo puede leerlo: se cifra en este dispositivo.");
    }
}
std::string status_text(Status s, int up, int total) {
    char b[160];
    switch (s) {
    case ONLINE: snprintf(b, sizeof b, t("On, %d of %d relays", "Activo, %d de %d relés"), up, total); return b;
    case CONNECTING: return t("Connecting...", "Conectando...");
    case NO_RELAY: return t("No relay answers", "Ningún relé responde");
    case UNAVAILABLE: return t("Not on this system", "No en este sistema");
    default: return t("Off", "Apagado");
    }
}
std::string hub_chat_name() { return game() == "mh4u" ? t("Room chat", "Chat de la sala") : t("Hub chat", "Chat de la sala"); }
std::string chat_state_text(Status s, int up, int total) {
    if (enabled() || !hub_chat()) return status_text(s, up, total);
    return game() == "mh4u" ? t("Room chat only", "Solo el chat de la sala") : t("Hub chat only", "Solo el chat de la sala");
}
std::string ago(int64_t at) {
    int64_t d = (int64_t)std::time(nullptr) - at;
    char b[64];
    if (d < 60) return t("just now", "recién");
    if (d < 3600) snprintf(b, sizeof b, t("%d min ago", "hace %d min"), (int)(d / 60));
    else snprintf(b, sizeof b, t("%d h ago", "hace %d h"), (int)(d / 3600));
    return b;
}

}  // namespace chat
