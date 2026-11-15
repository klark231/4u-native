// The chat over the game: chat_overlay.h. The last lines as plain text with a shadow in the top left corner, as AzaharPlus's
// overlay has them (their size, how many, the shadow, the background and how long they stay are the Chat page's), and the
// chat button: a rounded square in the menu's wine and gold that only opens the chat, with the count of lines not read on it.
#include "chat_overlay.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "chat.h"
#include "chat_text.h"
#include "imgui.h"

namespace chat {
namespace {
OverlayPort g_port;
struct Shown {
    Line line;
    double at;
};
std::vector<Shown> g_shown;
unsigned g_seen_serial;
bool g_serial_known;
// the button: its centre as fractions of the screen, its size (1: its own); a drag under way
constexpr float HOME_X = .11f, HOME_Y = .30f;  // (right of the column of buttons at the left edge, over the stick)
float g_bx = HOME_X, g_by = HOME_Y, g_bs = 1;
bool g_loaded, g_drag, g_dragged, g_on_screen;
float g_drag_dx, g_drag_dy, g_down_x, g_down_y, g_w = 1, g_h = 1;

void load() {
    if (g_loaded) return;
    g_loaded = true;
    float x, y, s;
    if (bubble(&x, &y, &s)) g_bx = std::clamp(x, 0.f, 1.f), g_by = std::clamp(y, 0.f, 1.f), g_bs = std::clamp(s, .6f, 2.f);
}
// A dp: the screen's short side is about 400 of them on a phone
float dp() { return std::max(1.f, std::min(g_w, g_h) / 400.f); }
float side() { return 56.f * dp() * g_bs; }  // (the chat button's side: 56 dp at its own size)
float radius() { return side() * .5f; }
bool menu_open() { return g_port.menu_open && g_port.menu_open(); }
bool wanted() {  // (the online chat on, or the hub's chat on and a hub up)
#ifdef __ANDROID__
    return bubble_shown() && g_port.controls_shown && g_port.controls_shown() && !menu_open() && (enabled() || (hub_chat() && hub_up()));
#else
    return false;
#endif
}
bool counts(int channel) {
    int o = overlay();
    if (o != OVERLAY_OFF && bridged() && (channel == HUB || channel == HUBCHAT)) return true;  // (Mixed: the room's and the hub's)
    return o == OVERLAY_ALL || (o == OVERLAY_HUB_FRIENDS && (channel == HUB || channel == HUBCHAT || channel == FRIEND));
}
ImU32 faded(ImU32 c, float a) { return (c & 0x00ffffffu) | ((ImU32)(((c >> 24) & 0xff) * a) << 24); }
}  // namespace

void overlay_setup(const OverlayPort &port) { g_port = port; }

bool overlay_frame() {
    if (!active()) {  // (both chats off, as they come: nothing over the game, and the button takes no finger)
        if (g_serial_known || g_on_screen) g_shown.clear(), g_on_screen = g_drag = false, g_serial_known = false;
        return false;
    }
    unsigned last = last_serial();
    if (!g_serial_known) g_seen_serial = last, g_serial_known = true;
    double now = SDL_GetTicks() / 1000.0;
    const bool menu = menu_open();
    if (last != g_seen_serial) {
        for (const Line &l : since(g_seen_serial))
            if (counts(l.channel) && !menu) g_shown.push_back({l, now});  // (yours too: as "You")
        g_seen_serial = last;
        const size_t most = (size_t)std::clamp(look("lines", 8), 1, 20);
        if (g_shown.size() > most) g_shown.erase(g_shown.begin(), g_shown.end() - (long)most);
    }
    const int stay = look("fade", 10);  // (seconds a line stays before it fades out over one more; 0: it stays)
    if (stay > 0)
        g_shown.erase(std::remove_if(g_shown.begin(), g_shown.end(), [now, stay](const Shown &s) { return now - s.at > stay + 1.0; }), g_shown.end());
    if (menu) g_shown.clear();
    const bool want = wanted();
    if (!want) g_on_screen = false;  // (gone - the hub left, say - and not drawn: no finger for it either)
    return !g_shown.empty() || want;
}

// A line of text with its shadow: a copy at the offset, a ring of faint ones for the radius (all in dp), then the text
void shadowed(ImDrawList *dl, ImFont *font, float fs, ImVec2 p, ImU32 col, const char *text, float wrap, float a) {
    const float d = dp(), r = look("shadowr", 2) * d * .6f, dx = look("shadowx", 1) * d, dy = look("shadowy", 1) * d;
    if (r > 0.f)
        for (int i = 0; i < 8; i++) {
            const float ang = i * 0.785398f;
            dl->AddText(font, fs, ImVec2(p.x + dx + cosf(ang) * r, p.y + dy + sinf(ang) * r), faded(IM_COL32(0, 0, 0, 255), a * .3f), text, nullptr, wrap);
        }
    dl->AddText(font, fs, ImVec2(p.x + dx, p.y + dy), faded(IM_COL32(0, 0, 0, 255), a * .85f), text, nullptr, wrap);
    dl->AddText(font, fs, p, faded(col, a), text, nullptr, wrap);
}

void overlay_draw(float w, float h) {
    g_w = w, g_h = h;
    load();
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    const float d = dp();
    const double now = SDL_GetTicks() / 1000.0;
    g_on_screen = wanted();
    if (g_on_screen) {  // (a rounded square, a speech bubble in it, the lines not read in a red circle; all of it as faint as Button opacity says)
        const float op = std::clamp(look("buttonop", 100), 10, 100) / 100.f, sd = side(), bx = g_bx * w, by = g_by * h;
        const ImVec2 lo(bx - sd * .5f, by - sd * .5f), hi(bx + sd * .5f, by + sd * .5f);
        const float round = sd * .3f;
        // (the touch controls' colours: a wine plate with a bronze edge, a gold bubble with wine lines in it)
        const ImU32 wine = IM_COL32(50, 25, 24, 236), pressed = IM_COL32(84, 44, 32, 245), edge = IM_COL32(196, 150, 70, 255),
                    gold = IM_COL32(226, 182, 92, 255), ink = IM_COL32(50, 25, 24, 255);
        dl->AddRectFilled(ImVec2(lo.x, lo.y + 2 * d), ImVec2(hi.x, hi.y + 2 * d), faded(IM_COL32(0, 0, 0, 255), .25f * op), round);
        dl->AddRectFilled(lo, hi, faded(g_drag && g_dragged ? pressed : wine, op), round);
        dl->AddRect(lo, hi, faded(edge, op), round, 0, std::max(1.5f, 2 * d));
        const float bw = sd * .46f, bh = sd * .34f;
        const ImVec2 a(bx - bw * .5f, by - bh * .62f), b(bx + bw * .5f, by + bh * .38f);
        dl->AddRectFilled(a, b, faded(gold, op), bh * .22f);
        const ImVec2 tail[3] = {ImVec2(bx - bw * .38f, b.y - 1), ImVec2(bx - bw * .38f, b.y + bh * .34f), ImVec2(bx - bw * .06f, b.y - 1)};
        dl->AddConvexPolyFilled(tail, 3, faded(gold, op));
        for (int i = 0; i < 3; i++) {
            const float ly = a.y + bh * (.22f + i * .26f);
            dl->AddRectFilled(ImVec2(a.x + bw * .16f, ly), ImVec2(b.x - bw * (i == 2 ? .42f : .16f), ly + bh * .1f), faded(ink, op), bh * .05f);
        }
        int unread = 0;
        for (int c = 0; c < CHANNELS; c++)
            if (counts(c) || c == FRIEND) unread += chat::unread(c);
        if (unread) {
            char n[8];
            snprintf(n, sizeof n, "%d", std::min(unread, 99));
            const float fs = sd * .26f, rr = sd * .19f;
            const ImVec2 c(hi.x - rr * .3f, lo.y + rr * .3f);
            dl->AddCircleFilled(c, rr, faded(IM_COL32(176, 34, 30, 255), op), 24);  // (the menu's red rule)
            dl->AddCircle(c, rr, faded(IM_COL32(196, 150, 70, 255), op), 24, std::max(1.f, d));
            const ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0, n);
            dl->AddText(font, fs, ImVec2(c.x - ts.x * .5f, c.y - ts.y * .5f), faded(IM_COL32(255, 255, 255, 255), op), n);
        }
    }
    if (g_shown.empty()) return;
    // the lines, top left, the oldest first: "Name: text" in white with its shadow, on a faint black if Background opacity says
    const float fs = std::clamp(look("textsize", 14), 8, 40) * d, pad = 4 * d, wrap = std::min(w * .6f, 520 * d);
    const float x = 20 * d, top = 20 * d;
    struct Row {
        std::string text;
        float a, h;
    };
    std::vector<Row> rows;
    float height = 0, width = 0;
    const int stay = look("fade", 10);
    for (const Shown &s : g_shown) {
        const float age = (float)(now - s.at);
        const float a = stay <= 0 ? 1.f : std::clamp(std::min(age / .15f, ((float)stay + 1.f - age) / 1.f), 0.f, 1.f);
        Row r;
        r.text = (s.line.system ? std::string() : chat::who(s.line) + ": ") + line_text(s.line);
        r.a = a;
        const ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, wrap, r.text.c_str());
        r.h = ts.y;
        height += ts.y, width = std::max(width, ts.x);
        rows.push_back(std::move(r));
    }
    const float bg = std::clamp(look("bg", 40), 0, 100) / 100.f;
    float alpha_all = 0;
    for (const Row &r : rows) alpha_all = std::max(alpha_all, r.a);
    if (bg > 0.f && !rows.empty())
        dl->AddRectFilled(ImVec2(x - pad, top - pad), ImVec2(x + width + pad, top + height + pad), faded(IM_COL32(0, 0, 0, 255), bg * alpha_all), 4 * d);
    float y = top;
    for (const Row &r : rows) {
        shadowed(dl, font, fs, ImVec2(x, y), IM_COL32(255, 255, 255, 255), r.text.c_str(), wrap, r.a);
        y += r.h;
    }
}

bool bubble_down(float fx, float fy) {
    if (!g_on_screen) return false;
    const float x = fx * g_w, y = fy * g_h;
    if (std::hypot(x - g_bx * g_w, y - g_by * g_h) > radius() * 1.15f) return false;
    g_drag = true, g_dragged = false;
    g_down_x = x, g_down_y = y, g_drag_dx = g_bx * g_w - x, g_drag_dy = g_by * g_h - y;
    return true;
}
void bubble_finger(bool up, float fx, float fy) {
    if (!g_drag) return;
    const float x = fx * g_w, y = fy * g_h;
    if (std::hypot(x - g_down_x, y - g_down_y) > g_h * .02f) g_dragged = true;
    if (g_dragged) {
        const float r = radius();
        g_bx = std::clamp((x + g_drag_dx) / g_w, r / g_w, 1 - r / g_w);
        g_by = std::clamp((y + g_drag_dy) / g_h, r / g_h, 1 - r / g_h);
    }
    if (!up) return;
    g_drag = false;
    if (g_dragged) {
        set_bubble(g_bx, g_by, g_bs);
        return;
    }
    if (g_port.log) g_port.log("[chat] the chat button: the menu's chat");
    if (g_port.open_chat) g_port.open_chat();
}
void bubble_size(float s) {
    load();
    g_bs = std::clamp(s, .6f, 2.f);
    set_bubble(g_bx, g_by, g_bs);
}
float bubble_scale() {
    load();
    return g_bs;
}
void bubble_reset() {
    load();
    g_bx = HOME_X, g_by = HOME_Y, g_bs = 1;
    set_bubble(g_bx, g_by, g_bs);
}

}  // namespace chat
