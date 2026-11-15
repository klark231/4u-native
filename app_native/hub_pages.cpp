// The hub list's pages: "<  1  2  3  >" with "Hubs 16-30 of 41" over it, in the menu's wine and gold, while the game's
// search screen is open and the hub servers have more hubs than one search can show (15: mh4n/hub.h). A tap on a number
// gives the game that page of hubs, the arrows the one before or after; the game's own search picks them up within a
// second, there is no Retry to press. Placed by the screen, centred under the FPS counter, clear of the thumbs.
#include "hub_pages.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>

#include "imgui.h"
#include "mh4n/hub.h"
#include "mh4n/rooms.h"
#include "ui.h"

namespace hub = mh4n::hub;
namespace rooms = mh4n::rooms;

namespace {
constexpr int kNumbers = 8;  // page numbers shown at once; with more pages they follow the chosen one
bool g_on;
float g_w = 1, g_h = 1;

struct Geo {
    float x, y, w, h;  // the bar
    float side, gap, pad;
};
int numbers(const hub::Pages& p) { return std::min(p.count, kNumbers); }
int first_number(const hub::Pages& p) { return std::clamp(p.chosen - 3, 0, std::max(0, p.count - kNumbers)); }
float dp() { return std::max(1.f, std::min(g_w, g_h) / 400.f); }
Geo geo(const hub::Pages& p) {
    Geo g;
    const float d = dp();
    g.side = 36 * d, g.gap = 6 * d, g.pad = 6 * d;
    const int cnt = numbers(p) + 2;
    g.w = cnt * g.side + (cnt - 1) * g.gap + 2 * g.pad;
    g.h = g.side + 2 * g.pad;
    g.x = (g_w - g.w) * .5f;
    g.y = g_h * .13f;
    return g;
}
ImU32 fade(ImU32 c, float a) { return (c & 0x00ffffffu) | ((ImU32)(((c >> 24) & 0xff) * a) << 24); }
}  // namespace

void hub_pages_frame() {
    const hub::Pages p = hub::GetPages();
    g_on = p.count > 1 && hub::GameSearching() && !ui_settings_open() && rooms::GetHubStatus().active;
    if (!g_on) return;
    ImGuiIO& io = ImGui::GetIO();
    g_w = io.DisplaySize.x, g_h = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    const Geo g = geo(p);
    const float d = dp(), line = std::max(1.f, 1.5f * d), r = g.side * .2f;
    // (the touch controls' Hunter colours, as gu-native's strip: wine, bronze edge, gold; the chosen page lit)
    const ImU32 panel = IM_COL32(50, 25, 24, 236), edge = IM_COL32(196, 150, 70, 255), button = IM_COL32(74, 24, 22, 242),
                gold = IM_COL32(226, 182, 92, 255), ink = IM_COL32(240, 228, 200, 255), lit = IM_COL32(226, 182, 92, 255),
                lit_ink = IM_COL32(40, 14, 14, 255);
    dl->AddRectFilled(ImVec2(g.x, g.y), ImVec2(g.x + g.w, g.y + g.h), panel, r * 1.3f);
    dl->AddRect(ImVec2(g.x, g.y), ImVec2(g.x + g.w, g.y + g.h), edge, r * 1.3f, 0, line);
    const int cnt = numbers(p) + 2, first = first_number(p);
    for (int i = 0; i < cnt; i++) {
        const ImVec2 a(g.x + g.pad + i * (g.side + g.gap), g.y + g.pad), b(a.x + g.side, a.y + g.side);
        const bool arrow = i == 0 || i == cnt - 1, chosen = !arrow && first + i - 1 == p.chosen;
        dl->AddRectFilled(a, b, chosen ? lit : button, r);
        dl->AddRect(a, b, edge, r, 0, line);
        const ImVec2 m((a.x + b.x) * .5f, (a.y + b.y) * .5f);
        const ImU32 c = chosen ? lit_ink : arrow ? gold : ink;
        if (arrow) {
            const float dir = i == 0 ? -1.f : 1.f, ch = g.side * .2f;
            const ImVec2 tip(m.x + dir * ch * .7f, m.y), t1(m.x - dir * ch * .5f, m.y - ch), t2(m.x - dir * ch * .5f, m.y + ch);
            dl->AddLine(t1, tip, c, g.side * .08f), dl->AddLine(t2, tip, c, g.side * .08f);
        } else {
            char t[8];
            std::snprintf(t, sizeof t, "%d", first + i);
            const float fs = g.side * .5f;
            const ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0, t);
            dl->AddText(font, fs, ImVec2(m.x - ts.x * .5f, m.y - ts.y * .5f), c, t);
        }
    }
    // which hubs the page holds, on a small plate over the bar
    char t[64];
    std::snprintf(t, sizeof t, "Hubs %d-%d of %d", p.first, p.last, p.total);
    const float fs = 13 * d, ch = 22 * d;
    const ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0, t);
    const float cw = ts.x + ch, cx = g.x + g.w * .5f, cy = g.y - 4 * d - ch * .5f;
    const ImVec2 c0(cx - cw * .5f, cy - ch * .5f), c1(cx + cw * .5f, cy + ch * .5f);
    dl->AddRectFilled(c0, c1, panel, ch * .5f);
    dl->AddRect(c0, c1, fade(edge, .9f), ch * .5f, 0, line);
    dl->AddText(font, fs, ImVec2(cx - ts.x * .5f, cy - ts.y * .5f), ink, t);
}

bool hub_pages_down(float fx, float fy) {
    if (!g_on) return false;
    const hub::Pages p = hub::GetPages();
    const Geo g = geo(p);
    const float x = fx * g_w, y = fy * g_h;
    const int cnt = numbers(p) + 2;
    for (int i = 0; i < cnt; i++) {
        const float ax = g.x + g.pad + i * (g.side + g.gap), ay = g.y + g.pad;
        const float mx = g.gap * .5f, my = g.side * .3f;  // (a little slop round each, half the gap: none overlap)
        if (x < ax - mx || x > ax + g.side + mx || y < ay - my || y > ay + g.side + my) continue;
        if (i == 0) hub::StepPage(-1);
        else if (i == cnt - 1) hub::StepPage(1);
        else hub::ChoosePage(first_number(p) + i - 1);
        return true;
    }
    // (the bar between the buttons takes the finger too, so a near miss doesn't move the camera)
    return x >= g.x && x <= g.x + g.w && y >= g.y && y <= g.y + g.h;
}
