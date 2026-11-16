// The settings menu's chat (S32) and room passwords (S35), included by settings_panel.cpp in its own namespace: the port's
// chat (chat/chat.h, after tabarong's designs PORT_CHAT.md and MATCHMAKING_KEY_GENERATOR.md) on two pages of their own,
// Chat and Friends (View::Chat, View::Friends), opened from Multiplayer (chat_entry_rows); the question before the relays
// connect the first time (View::ChatOn); the room's Generate a password and Share this room. The MHGU port has the same
// pages (runtime/settings_chat.inc), with the matchmaking key in place of the room.
// The chat's settings are its own (chat.txt beside mh4u-app.ini), applied at once: Save and leaving without it leave them
// (but Room messages over the game, [Network] Notices, as before). Its texts carry their Spanish beside them.

int g_chat = -1;  // the chat the page and the sheet show (FRIEND: with g_chat_peer, a friend's key); -1: not picked yet (the one
                  // picked last, chat.txt; else the room's while in one, else Global)
std::string g_chat_peer;
// The chat picked last, the first time the page or the sheet shows (the checks after it put it right if it is gone: Mixed
// ended, a friend removed; the one kept stays)
void restore_chat() {
    if (g_chat >= 0) return;
    int c;
    std::string peer;
    if (chat::chosen(&c, &peer)) g_chat = c, g_chat_peer = peer;
}
// A chat the player picked (the page's list, the sheet's button): kept, so it is the one shown after a restart and the one
// whose lines show over the game (Lines over the game: The chat picked)
void pick_chat(int channel, const std::string& peer) {
    g_chat = channel, g_chat_peer = peer;
    chat::set_chosen(channel, peer);
}
// The order the chats are offered in, in the page's list and on the sheet's button alike: the room's and the hub's, the
// online ones, the views of several at once (while Play online with is Mixed), then each friend
std::vector<int> chat_order() {
    std::vector<int> v = {chat::HUB, chat::HUBCHAT, chat::GENERAL, chat::GLOBAL};
    if (chat::bridged()) v.push_back(chat::MIXED), v.push_back(chat::MIXED_ALL);
    return v;
}
const char* cs(const char* en, const char* es) {
    chat::set_spanish(spanish());
    return chat::t(en, es);
}
std::string chat_hosts() {  // (the relays without wss://, for the question and the help)
    std::string all;
    for (const std::string& r : chat::relays()) all += (all.empty() ? "" : ", ") + (r.rfind("wss://", 0) == 0 ? r.substr(6) : r);
    return all;
}
std::string chat_on_detail() {
    char b[2048];
    snprintf(b, sizeof b,
             cs("The online chat (Global, General and friends) connects this device to public Nostr relays that anyone can use: "
                "%s. As with any server on the internet, they can see your IP address, and anyone can read Global and "
                "General: never share anything private there. While it is on, your chat name, your game and whether you "
                "are in a hub or room can be seen by anyone, so your friends find you. Messages to a friend are encrypted "
                "on this device. The "
                "project runs no server and there is no account: you are a key made on this device (your friend code). "
                "Nothing is saved anywhere, and the chat works only while you are connected. You can turn it off or choose "
                "other relays at any time. The room's chat needs none of this.",
                "El chat en línea (Global, General y amigos) conecta este dispositivo a relés públicos de Nostr que cualquiera "
                "puede usar: %s. Como cualquier servidor de internet, ven tu dirección IP, y cualquiera puede leer Global y "
                "General: nunca compartas nada privado ahí. Mientras está activo, cualquiera puede ver tu nombre en el "
                "chat, tu juego y si estás en una sala, para que tus amigos te encuentren. Los mensajes a un amigo se "
                "cifran en este dispositivo. El "
                "proyecto no tiene servidores y no hay cuentas: eres una clave creada en este dispositivo (tu código de "
                "amigo). No se guarda nada en ningún lado, y el chat funciona solo mientras estás conectado. Puedes "
                "apagarlo o elegir otros relés cuando quieras. El chat de la sala no necesita nada de esto."),
             chat_hosts().c_str());
    return b;
}
void chat_turn_on() {
    if (chat::notice_seen()) {
        chat::set_enabled(true);
        return;
    }
    push(View::ChatOn, "dialog.cancel");
}
void chat_open(int channel, const std::string& peer) {
    pick_chat(channel, peer);
    open_chat_sheet();
}
// A room shared in the chat: where to (the room's own players are already in it: friends online, General, Global)
void chat_share(int kind, const std::string& a, const std::string& b) {
    std::vector<std::string> names;
    std::vector<std::pair<int, std::string>> to;
    for (const chat::Person& p : chat::people())
        if (p.is_friend && p.mutual && p.online) names.push_back(p.name + " (" + cs("friend", "amigo") + ")"), to.push_back({chat::FRIEND, p.id});
    names.push_back(chat::channel_name(chat::GENERAL)), to.push_back({chat::GENERAL, ""});
    names.push_back(chat::channel_name(chat::GLOBAL)), to.push_back({chat::GLOBAL, ""});
    Row row;
    row.id = "chat.share", row.title = cs("Share it with", "Compartirla con");
    choices(row, names, -1, [to, kind, a, b](int i) {
        std::string why;
        if (i < 0 || i >= (int)to.size()) return;
        if (!chat::share(to[i].first, to[i].second, kind, a, b, &why)) s.error = chat::explain(why);
        else chat_open(to[i].first, to[i].second);
    });
}
// S35: a new password for the room (128 random bits made here, as MHGU's matchmaking key), copied for the friends
void chat_new_password() {
    const std::string key = chat::new_key();
    s.draft.password = key;
    const bool copied = SDL_SetClipboardText(key.c_str());
    s.error = copied ? cs("A new password, copied. Host or join the room with it, then share the room in the chat.",
                          "Una contraseña nueva, copiada. Crea o entra a la sala con ella, y comparte la sala en el chat.")
                     : cs("A new password (it could not be copied). Host or join the room with it, then share the room in the chat.",
                          "Una contraseña nueva (no se pudo copiar). Crea o entra a la sala con ella, y comparte la sala en el chat.");
    std::fprintf(stderr, "[Info] App: a new room password was generated (%s)\n", copied ? "copied" : "not copied");
}
// The room's rows: Generate a password (under Password), and Share this room while in one
void chat_password_row(std::vector<Row>& r) {
    action(r, "net.newpass", cs("Generate a password", "Generar una contraseña"), []() { chat_new_password(); });
    r.back().help = cs("A random password (32 letters and digits), copied: host a room with it, or give it to the room's host. "
                       "Then share the room in the chat so your friends join with one tap.",
                       "Una contraseña al azar (32 letras y números), copiada: crea una sala con ella, o dásela a quien la "
                       "crea. Después comparte la sala en el chat y tus amigos entran con un toque.");
}
void chat_share_room_row(std::vector<Row>& r, const rooms::Status& st) {
    std::string at = st.host + ":" + std::to_string(st.port);
    if (st.hosting) {  // (this phone's room: where the others find it, not the loopback)
        std::vector<std::string> mine = net_local_addresses();
        if (!mine.empty()) at = mine[0] + ":" + std::to_string(st.hosted_port);
    }
    const std::string password = s.draft.password;
    action(r, "net.share", cs("Share this room in the chat", "Compartir esta sala en el chat"), [at, password]() {
        if (!chat::enabled()) {
            chat_turn_on();
            return;
        }
        chat_share(chat::SHARE_ROOM, at, password);
    });
    r.back().value = at;
    r.back().help = cs("Sends the room's address and password to a friend online, or to General or Global, as a line they join "
                       "with one tap. A room on this phone is reached from the internet only with its port forwarded to it.",
                       "Manda la dirección y la contraseña de la sala a un amigo en línea, o a General o Global, como una línea "
                       "con la que entran con un toque. Una sala en este teléfono se alcanza desde internet solo con su puerto "
                       "redirigido a él.");
}
void join_room();
void chat_use_share(const chat::Line& l) {
    if (l.share != chat::SHARE_ROOM) return;
    s.draft.address = l.share_a, s.draft.password = l.share_b;
    first_page(3);  // (Multiplayer: page_ids)
    focus("net.join");
    join_room();
}

// The room's chat: a switch of its own, off as it comes (1.0), as MHGU's hub chat. On, the room's lines come here and over
// the game beside the chat button, which shows in a room; off, the port's chat takes no part in the room, and its lines show
// with Room messages over the game as in 0.1.9.3
void chat_hub_row(std::vector<Row>& r) {
    Row row;
    row.id = "chat.hub", row.title = chat::hub_chat_name();
    const bool on = chat::hub_chat();
    row.toggle = on, row.value = on ? cs("On", "Activo") : cs("Off", "Apagado");
    row.help = cs("Talk with everyone in the room you are in, emulator players too. On: their lines show here and over the game, "
                  "and in a room the chat button opens the chat. It travels with the room itself: no relay. Off: someone's line "
                  "shows with Room messages over the game, as before.",
                  "Habla con todos los de la sala en la que estás, también los de un emulador. Activo: sus líneas se ven acá y "
                  "sobre el juego, y en una sala el botón del chat abre el chat. Viaja con la sala misma: sin relés. Apagado: "
                  "la línea de alguien se ve con Mensajes de la sala sobre el juego, como antes.");
    row.activate = []() { chat::set_hub_chat(!chat::hub_chat()); };
    row.adjust = [](int d) { chat::set_hub_chat(d > 0); };
    r.push_back(std::move(row));
}
void chat_status_row(std::vector<Row>& r) {
    int up = 0, total = 0;
    const chat::Status st = chat::status(&up, &total);
    Row row;
    row.id = "chat.status", row.title = cs("Online chat", "Chat en línea");
    row.value = chat::status_text(st, up, total);
    if (st == chat::ONLINE) row.value += " - " + std::to_string(chat::online_count()) + " " + cs("online", "en línea");
    row.toggle = st == chat::UNAVAILABLE ? -1 : chat::enabled() ? 1 : 0;
    if (st == chat::NO_RELAY) row.ink = theme().warn;
    row.enabled = st != chat::UNAVAILABLE;
    row.help = std::string(cs("Global, General and your friends travel through public relays (", "Global, General y tus amigos viajan por relés públicos (")) +
               chat_hosts() + cs("), only while this is on. The room's chat needs no relay.", "), solo mientras esto está activo. El chat de la sala no necesita relés.");
    if (st == chat::NO_RELAY && !chat::status_reason().empty()) row.help += "\n" + chat::status_reason();
    row.activate = []() {
        if (chat::enabled()) chat::set_enabled(false);
        else chat_turn_on();
    };
    row.adjust = [](int d) {
        if (d > 0 && !chat::enabled()) chat_turn_on();
        if (d < 0) chat::set_enabled(false);
    };
    r.push_back(std::move(row));
    if (chat::enabled())  // (the switch alone shows: the relays' state under it)
        note(r, "chat.state", chat::status_text(st, up, total) + (st == chat::ONLINE ? ", " + std::to_string(chat::online_count()) + " " + cs("online", "en línea") : std::string()),
             st == chat::ONLINE ? NET_GREEN : st == chat::NO_RELAY ? theme().warn : touch_look().dim);
}
// The chat dialog's opacity (20 to 100 %), kept in mh4u-app.ini [Chat] DialogOpacity
int g_dialog_opacity = -1;
int chat_dialog_opacity() {
    if (g_dialog_opacity < 0) g_dialog_opacity = std::clamp(atoi(app_setting("Chat", "DialogOpacity", "90").c_str()), 20, 100);
    return g_dialog_opacity;
}
void set_chat_dialog_opacity(int pct) {
    g_dialog_opacity = std::clamp(pct, 20, 100);
    app_setting_set("Chat", "DialogOpacity", std::to_string(g_dialog_opacity));
    app_settings_save();
}
// One number of the overlay's look (chat::look): a row that adds step per turn, within lo and hi
void look_row(std::vector<Row>& r, const char* id, const char* title, const char* key, int def, int lo, int hi, int step,
              const char* unit, const char* help) {
    const int now = chat::look(key, def);
    Row row;
    row.id = id, row.title = title, row.value = std::to_string(now) + unit, row.number = true, row.help = help;
    const std::string k = key;
    row.adjust = [k, def, lo, hi, step](int d) { chat::set_look(k.c_str(), std::clamp(chat::look(k.c_str(), def) + d * step, lo, hi)); };
    r.push_back(std::move(row));
}
int g_bubble_pct = -1;
void chat_page_rows(std::vector<Row>& r) {
    cs("", "");  // (the language)
    std::vector<chat::Person> people = chat::people();
    restore_chat();
    if (g_chat < 0) g_chat = chat::bridged() ? chat::MIXED : chat::hub_chat() && chat::hub_up() ? chat::HUB : chat::GLOBAL;
    if ((g_chat == chat::MIXED || g_chat == chat::MIXED_ALL) && !chat::bridged())
        g_chat = chat::hub_chat() && chat::hub_up() ? chat::HUB : chat::GLOBAL;  // (Mixed ended)
    {  // (a room joined since the page last showed and the online chat off: the room's chat, not the Global or General that
       // a page first seen before the room had picked, which only says to turn the online chat on; picking them again stays)
        static bool hub_was_up;
        const bool hub_now = chat::hub_chat() && chat::hub_up();
        if (hub_now && !hub_was_up && !chat::enabled() && (g_chat == chat::GLOBAL || g_chat == chat::GENERAL)) g_chat = chat::HUB;
        hub_was_up = hub_now;
    }
    bool found = g_chat != chat::FRIEND;  // (MIXED too: a view)
    for (const chat::Person& p : people)
        if (g_chat == chat::FRIEND && p.id == g_chat_peer && p.is_friend) found = true;
    if (!found) g_chat = chat::GLOBAL, g_chat_peer.clear();
    chat_hub_row(r);  // (the two switches, both off as they come)
    chat_status_row(r);
    {  // the chat shown: the room, General, Global, each friend
        std::vector<std::string> names;
        std::vector<std::pair<int, std::string>> to;
        int at = 0;
        for (int c : chat_order()) {
            int n = chat::unread(c);
            names.push_back(chat::channel_name(c) + (n ? " (" + std::to_string(n) + ")" : std::string()));
            to.push_back({c, ""});
            if (g_chat == c) at = (int)to.size() - 1;
        }
        for (const chat::Person& p : people)
            if (p.is_friend) {
                int n = chat::unread(chat::FRIEND, p.id);
                names.push_back(std::string(cs("Friend: ", "Amigo: ")) + p.name + (n ? " (" + std::to_string(n) + ")" : std::string()));
                to.push_back({chat::FRIEND, p.id});
                if (g_chat == chat::FRIEND && g_chat_peer == p.id) at = (int)to.size() - 1;
            }
        add_choice(r, "chat.channel", cs("Chat", "Chat"), at, names, [to](int i) {
            if (i >= 0 && i < (int)to.size()) pick_chat(to[i].first, to[i].second);
        });
        r.back().help = chat::channel_help(g_chat);
    }
    // (the lines and what to say are the chat button's dialog, over the game: View::ChatDialog; this page is its settings)
    action(r, "chat.dialog", cs("Open the chat", "Abrir el chat"), []() { open_chat_sheet(); });
    r.back().help = cs("The chat dialog the chat button opens over the game: the lines, and a field to say something.",
                       "El diálogo del chat que abre el botón del chat sobre el juego: las líneas y un campo para decir algo.");
    std::vector<chat::Line> lines = chat::lines(g_chat, g_chat_peer, 30);
    std::vector<std::pair<std::string, std::string>> senders;  // (the people in the lines: id, name)
    for (const chat::Line& l : lines)
        if (!l.mine && !l.system && l.from != chat::my_key()) {
            bool seen = false;
            for (auto& x : senders) seen |= x.first == l.from;
            if (!seen) senders.push_back({l.from, l.name});
        }
    if (!senders.empty() && g_chat != chat::FRIEND) {
        std::vector<std::string> names;
        for (auto& x : senders) names.push_back(x.second);
        Row row;
        row.id = "chat.people", row.title = cs("Someone in this chat", "Alguien de este chat");
        row.value = std::to_string(senders.size());
        row.help = cs("Add one of them as a friend, mute them (their lines no longer show here) or block them (nothing of "
                      "theirs reaches you). Only you are affected, on this device.",
                      "Agrega a alguno como amigo, sílencialo (sus líneas ya no se ven acá) o bloquéalo (no te llega nada "
                      "suyo). Solo te afecta a ti, en este dispositivo.");
        row.menu = true;
        Row copy = row;
        row.activate = [copy, names, senders]() {
            choices(copy, names, -1, [senders](int i) {
                if (i < 0 || i >= (int)senders.size()) return;
                const std::string id = senders[i].first, name = senders[i].second;
                const bool key = id.size() == 64;
                std::vector<std::string> what;
                if (key) what.push_back(cs("Add as a friend", "Agregar como amigo"));
                what.push_back(cs("Mute", "Silenciar"));
                what.push_back(cs("Block", "Bloquear"));
                Row who;
                who.id = "chat.people", who.title = name;
                choices(who, what, -1, [id, name, key](int k) {
                    if (!key) k++;
                    if (k == 0) chat::add_friend_key(id, name);
                    else if (k == 1) chat::set_muted(id, name, true);
                    else if (k == 2) chat::set_blocked(id, name, true);
                });
            });
        };
        r.push_back(std::move(row));
    }
    action(r, "chat.friends", cs("Friends", "Amigos"), []() { push(View::Friends, "friends.code"); });
    {
        int on = 0;
        for (const chat::Person& p : people) on += p.is_friend && p.online;
        r.back().value = std::to_string(on) + " " + cs("online", "en línea");
    }
    header(r, "chat.options", cs("Options", "Opciones"));
    {  // (shown in this order; chat::Overlay keeps its numbers, as chat.txt has them)
        static const int kOverlay[] = {chat::OVERLAY_OFF, chat::OVERLAY_CHAT, chat::OVERLAY_HUB_FRIENDS, chat::OVERLAY_ALL};
        int at = 1;
        for (int i = 0; i < 4; i++)
            if (kOverlay[i] == chat::overlay()) at = i;
        add_choice(r, "chat.overlay", cs("Lines over the game", "Líneas sobre el juego"), at,
                   {cs("Off", "Apagado"), cs("The chat picked", "El chat elegido"), cs("Room and friends", "Sala y amigos"), cs("All", "Todos")},
                   [](int i) { chat::set_overlay(kOverlay[std::clamp(i, 0, 3)]); });
        r.back().help = cs("New lines show over the game, top left, and fade out after the fade delay. The chat picked: only "
                           "the lines of the chat chosen above (or in the chat button's dialog).",
                           "Las líneas nuevas se ven sobre el juego, arriba a la izquierda, y se desvanecen tras el tiempo de "
                           "desvanecido. El chat elegido: solo las del chat elegido arriba (o en el diálogo del botón del chat).");
    }
    add_bool(r, "net.notices", "Room messages over the game", &s.draft.notices, "Notices");
    {  // the lines' look (AzaharPlus's and CitraMMJ's chat overlay settings), at once
        look_row(r, "chat.look.textsize", cs("Chat text size", "Tamaño del texto del chat"), "textsize", 14, 8, 40, 1, "",
                 cs("The size of the lines over the game.", "El tamaño de las líneas sobre el juego."));
        look_row(r, "chat.look.lines", cs("Chat lines", "Líneas del chat"), "lines", 8, 1, 20, 1, "",
                 cs("How many lines show over the game at most.", "Cuántas líneas se ven como máximo sobre el juego."));
        look_row(r, "chat.look.fade", cs("Fade delay", "Tiempo de desvanecido"), "fade", 15, 0, 60, 1, " s",
                 cs("Seconds a line stays before it fades out. 0: it never fades.", "Segundos que una línea queda antes de desvanecerse. 0: nunca se desvanece."));
        look_row(r, "chat.look.shadowr", cs("Shadow radius", "Radio de la sombra"), "shadowr", 2, 0, 10, 1, "",
                 cs("How soft the shadow around the text is. 0: none but its offset.", "Qué tan suave es la sombra del texto. 0: solo su desplazamiento."));
        look_row(r, "chat.look.shadowx", cs("Shadow X offset", "Desplazamiento X de la sombra"), "shadowx", 1, -8, 8, 1, "",
                 cs("The shadow's offset to the right (left if below 0).", "El desplazamiento de la sombra a la derecha (a la izquierda si es menor que 0)."));
        look_row(r, "chat.look.shadowy", cs("Shadow Y offset", "Desplazamiento Y de la sombra"), "shadowy", 1, -8, 8, 1, "",
                 cs("The shadow's offset down (up if below 0).", "El desplazamiento de la sombra hacia abajo (hacia arriba si es menor que 0)."));
        look_row(r, "chat.look.bg", cs("Background opacity", "Opacidad del fondo"), "bg", 40, 0, 100, 5, " %",
                 cs("The black behind the lines. 0: none.", "El negro detrás de las líneas. 0: ninguno."));
    }
    {
        Row row;
        row.id = "chat.bubble", row.title = cs("Chat button", "Botón del chat");
        bool on = chat::bubble_shown();
        row.toggle = on, row.value = on ? cs("On", "Activo") : cs("Off", "Apagado");
        row.help = cs("A button over the game while the online chat is on, or the room chat is on and you are in a room: "
                      "a tap opens the chat, a drag moves it. The number on it: lines not read yet.",
                      "Un botón sobre el juego mientras el chat en línea está activo, o el chat de la sala está activo "
                      "y estás en una sala: un toque abre el chat, arrastrarlo lo mueve. El número: líneas sin leer.");
        row.activate = []() { chat::set_bubble_shown(!chat::bubble_shown()); };
        row.adjust = [](int d) { chat::set_bubble_shown(d > 0); };
        r.push_back(std::move(row));
        g_bubble_pct = (int)std::lround(chat::bubble_scale() * 56);  // (dp: 56 at its own size)
        Row size;
        size.id = "chat.bubble.size", size.title = cs("Chat button size", "Tamaño del botón del chat");
        size.value = std::to_string(g_bubble_pct) + " dp";
        size.number = true;
        size.help = cs("The chat button's size.", "El tamaño del botón del chat.");
        size.adjust = [](int d) { chat::bubble_size(std::clamp(g_bubble_pct + d * 4, 34, 112) / 56.f); };
        r.push_back(std::move(size));
        look_row(r, "chat.look.buttonop", cs("Chat button opacity", "Opacidad del botón del chat"), "buttonop", 80, 10, 100, 5, " %",
                 cs("How faint the chat button and its number are.", "Qué tan tenues son el botón del chat y su número."));
        action(r, "chat.bubble.reset", cs("Chat button back in its place", "Botón del chat a su lugar"), []() { chat::bubble_reset(); });
        r.back().arrow = false;
        Row dlg;  // (the chat dialog: how much of the game shows through it)
        dlg.id = "chat.dialog.opacity", dlg.title = cs("Chat dialog opacity", "Opacidad del diálogo del chat");
        dlg.value = std::to_string(chat_dialog_opacity()) + " %";
        dlg.number = true;
        dlg.help = cs("The chat dialog the chat button opens, over the game: 20 % lets the game show through, 100 % does not.",
                      "El diálogo del chat que abre el botón del chat, sobre el juego: 20 % deja ver el juego, 100 % no.");
        dlg.adjust = [](int d) { set_chat_dialog_opacity(chat_dialog_opacity() + d * 10); };
        r.push_back(std::move(dlg));
        action(r, "chat.look.reset", cs("Reset the chat look", "Restablecer el aspecto del chat"), []() {
            chat::reset_look();
            set_chat_dialog_opacity(90);
        });
        r.back().arrow = false;
        r.back().help = cs("Text, lines, shadow, background, fade, button opacity and the dialog's opacity back as they come.",
                           "Texto, líneas, sombra, fondo, desvanecido, opacidad del botón y del diálogo como vienen.");
    }
    {
        Row row;
        row.id = "chat.name", row.title = cs("Your name in the chat", "Tu nombre en el chat");
        row.value = chat::own_name().empty() ? chat::name() + cs(" (the nickname)", " (el apodo)") : chat::own_name();
        row.help = cs("Empty: your room nickname. Up to 24 characters.", "Vacío: tu apodo de sala. Hasta 24 caracteres.");
        row.arrow = true;
        row.activate = []() {
            open_keyboard(cs("Your name in the chat", "Tu nombre en el chat"), chat::own_name(), KB_ANY, 24, "", [](const std::string& t, std::string&) {
                chat::set_name(t);
                return true;
            });
        };
        r.push_back(std::move(row));
    }
    {
        const std::vector<std::string> def = chat::default_relays(), now = chat::relays();
        const bool custom = now != def;
        std::string list;
        for (const std::string& x : now) list += (list.empty() ? "" : " ") + x;
        add_choice(r, "chat.relays", cs("Relays", "Relés"), custom ? 1 : 0, {cs("The usual ones", "Los de siempre"), custom ? list : std::string(cs("Custom...", "Otros..."))},
                   [list](int i) {
                       if (i == 0) {
                           chat::set_relays({});
                           return;
                       }
                       open_keyboard(cs("Relays", "Relés"), list, KB_ANY, 600, "wss://...", [](const std::string& t, std::string& error) {
                           std::vector<std::string> v;
                           size_t a = 0;
                           while (a < t.size()) {
                               size_t b = t.find_first_of(" ,", a);
                               std::string x = t.substr(a, b == std::string::npos ? std::string::npos : b - a);
                               if (!x.empty() && x.rfind("wss://", 0) != 0) x = "wss://" + x;
                               if (x.size() > 6) v.push_back(x);
                               if (b == std::string::npos) break;
                               a = b + 1;
                           }
                           if (v.empty()) {
                               error = cs("Type one or more relays, as wss://relay.example.org, separated by spaces.",
                                          "Escribe uno o más relés, como wss://relay.example.org, separados por espacios.");
                               return false;
                           }
                           chat::set_relays(v);
                           return true;
                       });
                   });
        r.back().help = std::string(cs("The Nostr relays the online chat goes through. The usual ones: ", "Los relés de Nostr por los que va el chat en línea. Los de siempre: ")) +
                        "relay.damus.io, nos.lol, relay.primal.net. " +
                        cs("Everyone you want to talk to needs at least one relay in common with you.",
                           "Todos con los que quieras hablar necesitan al menos un relé en común contigo.");
    }
    note(r, "chat.credit", cs("Chat and room keys after the designs of tabarong.", "Chat y claves de sala según los diseños de tabarong."), touch_look().dim);
}
void friends_page_rows(std::vector<Row>& r) {
    cs("", "");
    chat_status_row(r);
    {
        Row row;
        row.id = "friends.code", row.title = cs("Your friend code", "Tu código de amigo");
        row.value = chat::my_code();
        row.help = cs("Give it to a friend: they add you with it while you are both online. A tap copies it. It is made on "
                      "this device: deleting the app makes a new one.",
                      "Dáselo a un amigo: te agrega con él mientras los dos están en línea. Un toque lo copia. Se crea en este "
                      "dispositivo: borrar la app crea otro.");
        row.activate = []() {
            const std::string c = chat::my_code();
            if (SDL_SetClipboardText(c.c_str())) s.error = c + cs(": copied.", ": copiado.");
        };
        r.push_back(std::move(row));
    }
    action(r, "friends.add", cs("Add a friend", "Agregar un amigo"), []() {
        open_keyboard(cs("Their friend code", "Su código de amigo"), "", KB_ANY, 20, "XXXX-XXXX-XXXX", [](const std::string& typed, std::string& error) {
            std::string why;
            if (chat::add_friend(typed, &why)) return true;
            error = chat::explain(why);
            return false;
        });
    });
    r.back().help = cs("Type their code (XXXX-XXXX-XXXX). They are found as soon as both of you are online, and get your "
                       "request; you can talk privately once they add you back.",
                       "Escribe su código (XXXX-XXXX-XXXX). Se lo encuentra apenas los dos estén en línea, y le llega tu "
                       "solicitud; pueden hablar en privado cuando te agregue también.");
    std::vector<chat::Person> people = chat::people();
    bool any = false;
    for (const chat::Person& p : people) {
        if (!p.request || p.is_friend || p.blocked) continue;
        if (!any) header(r, "friends.requests", cs("Requests", "Solicitudes"));
        any = true;
        Row row;
        row.id = "friends.req." + p.id, row.title = p.name, row.value = cs("wants to be your friend", "quiere ser tu amigo");
        row.menu = true;
        Row copy = row;
        const std::string id = p.id, name = p.name;
        row.activate = [copy, id, name]() {
            choices(copy, {cs("Accept", "Aceptar"), cs("Ignore", "Ignorar"), cs("Block", "Bloquear")}, -1, [id, name](int k) {
                if (k == 0) chat::accept(id);
                else if (k == 1) chat::remove(id);
                else chat::set_blocked(id, name, true);
            });
        };
        r.push_back(std::move(row));
    }
    header(r, "friends.list", cs("Friends", "Amigos"));
    any = false;
    const rooms::Status st = rooms::GetStatus();
    for (const chat::Person& p : people) {
        if (!p.is_friend && !p.pending) continue;
        any = true;
        Row row;
        row.id = "friends.f." + (p.pending ? p.code : p.id);
        row.title = p.pending ? cs("Code ", "Código ") + chatc::show_code(p.code) : p.name.empty() ? chatc::show_code(p.code) : p.name;
        if (p.pending) row.value = cs("looking for them...", "buscándolo...");
        else if (!p.mutual) row.value = cs("waiting for them to add you", "esperando que te agregue");
        else if (p.online) row.value = p.in_hub ? cs("online, in a room", "en línea, en una sala") : cs("online", "en línea");
        else row.value = p.seen ? std::string(cs("seen ", "visto ")) + chat::ago(p.seen) : std::string(cs("offline", "desconectado"));
        if (p.online && p.mutual) row.value += p.game == "mh4u" ? " (MH4U)" : p.game == "mhgu" ? " (MHGU)" : "";
        if (p.online && p.mutual) row.ink = NET_GREEN;
        row.help = std::string(cs("Friend code: ", "Código de amigo: ")) + chatc::show_code(p.code);
        row.menu = true;
        Row copy = row;
        const chat::Person q = p;
        const bool room = in_room(st);
        row.activate = [copy, q, room]() {
            std::vector<std::string> what;
            std::vector<int> act;
            if (q.is_friend && q.mutual) what.push_back(cs("Talk", "Hablar")), act.push_back(0);
            if (q.is_friend && q.mutual && q.online && room) what.push_back(cs("Send this room", "Mandar esta sala")), act.push_back(1);
            if (!q.pending) what.push_back(q.muted ? cs("Unmute", "Quitar silencio") : cs("Mute", "Silenciar")), act.push_back(2);
            if (!q.pending) what.push_back(cs("Block", "Bloquear")), act.push_back(3);
            what.push_back(cs("Remove", "Quitar")), act.push_back(4);
            choices(copy, what, -1, [q, act](int k) {
                if (k < 0 || k >= (int)act.size()) return;
                std::string why;
                switch (act[k]) {
                case 0: chat_open(chat::FRIEND, q.id); break;
                case 1: {
                    const rooms::Status now = rooms::GetStatus();
                    std::string at = now.host + ":" + std::to_string(now.port);
                    if (now.hosting) {
                        std::vector<std::string> mine = net_local_addresses();
                        if (!mine.empty()) at = mine[0] + ":" + std::to_string(now.hosted_port);
                    }
                    if (!chat::share(chat::FRIEND, q.id, chat::SHARE_ROOM, at, s.draft.password, &why)) s.error = chat::explain(why);
                    else chat_open(chat::FRIEND, q.id);
                    break;
                }
                case 2: chat::set_muted(q.id, q.name, !q.muted); break;
                case 3: chat::set_blocked(q.id, q.name, true); break;
                default: chat::remove(q.pending ? "code:" + q.code : q.id); break;
                }
            });
        };
        r.push_back(std::move(row));
    }
    if (!any) note(r, "friends.none", cs("No friends yet: add one with their code.", "Todavía no tienes amigos: agrega uno con su código."), touch_look().dim);
    any = false;
    for (const chat::Person& p : people) {
        if (!p.muted && !p.blocked) continue;
        if (!any) header(r, "friends.quiet", cs("Muted and blocked", "Silenciados y bloqueados"));
        any = true;
        Row row;
        row.id = "friends.q." + p.id;
        row.title = p.name.empty() ? (p.id.rfind("room:", 0) == 0 ? p.id.substr(5) : p.id.substr(0, 12)) : p.name;
        row.value = p.blocked ? cs("blocked", "bloqueado") : cs("muted", "silenciado");
        row.help = cs("A tap lets them back. Only you are affected, on this device.", "Un toque lo deja volver. Solo te afecta a ti, en este dispositivo.");
        const std::string id = p.id, name = p.name;
        const bool blocked = p.blocked;
        row.activate = [id, name, blocked]() {
            if (blocked) chat::set_blocked(id, name, false);
            chat::set_muted(id, name, false);
        };
        r.push_back(std::move(row));
    }
}
// Multiplayer's rows: the chat and the friends, each a page of its own (they take the place of the room's chat rows)
void chat_entry_rows(std::vector<Row>& r) {
    header(r, "chat.header", cs("Chat and friends", "Chat y amigos"));
    int unread = 0;
    for (int c = 0; c < chat::CHANNELS; c++) unread += chat::unread(c);
    action(r, "chat.open", cs("Chat", "Chat"), []() { push(View::Chat, "chat.channel"); });
    int up = 0, total = 0;
    chat::Status st = chat::status(&up, &total);
    r.back().value = (unread ? std::to_string(unread) + " " + cs("unread", "sin leer") + " - " : std::string()) + chat::chat_state_text(st, up, total);
    r.back().help = cs("Talk with the room you are in, with every player of the ports (Global) or of this game (General), and "
                       "with your friends. Off until you turn on the room chat or the online chat there; until then nothing "
                       "of the chat shows over the game (the room's own messages show as before: Room messages over the game).",
                       "Habla con la sala en la que estás, con todos los jugadores de los ports (Global) o de este juego "
                       "(General), y con tus amigos. Apagado hasta que actives ahí el chat de la sala o el chat en línea; "
                       "hasta entonces no se ve nada del chat sobre el juego (los mensajes de la sala se ven como antes: "
                       "Mensajes de la sala sobre el juego).");
    action(r, "friends.open", cs("Friends", "Amigos"), []() { push(View::Friends, "friends.code"); });
    int on = 0, requests = 0;
    for (const chat::Person& p : chat::people()) on += p.is_friend && p.online, requests += p.request && !p.is_friend && !p.blocked;
    r.back().value = (requests ? std::to_string(requests) + " " + cs("requests", "solicitudes") + " - " : std::string()) + std::to_string(on) + " " + cs("online", "en línea");
    r.back().help = cs("Your friend code, adding friends, and who you muted or blocked.", "Tu código de amigo, agregar amigos, y a quién silenciaste o bloqueaste.");
}
