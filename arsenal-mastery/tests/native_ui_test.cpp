#include "native_ui.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

struct Drawing {
    struct Rect { float x, y, w, h; unsigned colour; };
    std::vector<Rect> rectangles;
    std::vector<std::string> labels;
};
static void rect(void* ctx, float x, float y, float w, float h, unsigned colour) {
    static_cast<Drawing*>(ctx)->rectangles.push_back({x, y, w, h, colour});
}
static void text(void* ctx, float, float, float, unsigned, const char* value) {
    static_cast<Drawing*>(ctx)->labels.emplace_back(value);
}
int main() {
    Drawing d;
    openrac_overlay overlay{};
    overlay.context = &d;
    overlay.rect = rect;
    overlay.text = text;
    arsenal::native_ui::Snapshot s{};
    s.enabled = true;
    s.playing = true;
    s.held_weapon = 6;
    s.has_ammo = true;
    s.level = 5;
    s.pal = true;
    s.smooth_progress = .5f;
    arsenal::native_ui::draw(&overlay, s);
    assert(d.labels.size() == 1 && d.labels[0] == "V5");
    assert(d.rectangles.size() >= 4);
    assert(std::fabs(d.rectangles[0].y - 42.0f) < 0.01f);

    d = {};
    s.pal = false;
    s.big_ammo = true;
    s.level = 99;
    s.popups[0] = {6, 99, 1};
    s.popup_count = 1;
    s.tick = 20;
    arsenal::native_ui::draw(&overlay, s);
    assert(d.labels.size() == 2 && d.labels[0] == "V99" && d.labels[1] == "V99");
    assert(std::fabs(d.rectangles[0].y - 50.0f) < 0.01f);

    d = {};
    s.tick = 125;
    arsenal::native_ui::draw(&overlay, s);
    assert(d.labels.size() == 1); // popup expired; weapon gauge remains

    d = {};
    s.playing = false;
    arsenal::native_ui::draw(&overlay, s);
    assert(d.rectangles.empty() && d.labels.empty());

    // Real pause grid: only owned weapons receive bars, plus the wrench header.
    d = {};
    s.menu_weapons = true;
    s.menu_cells[0] = {2, true, 3, 0.5f};
    s.menu_cells[1] = {7, false, 4, 0.25f};
    s.wrench_level = 8;
    s.wrench_progress = 0.75f;
    arsenal::native_ui::draw(&overlay, s);
    assert(d.labels.size() == 2);
    assert(d.labels[0] == "V3" && d.labels[1] == "V8");
    assert(d.rectangles.size() >= 8);
    assert(std::fabs(d.rectangles[0].x - 29.8f) < 0.02f);
    assert(std::fabs(d.rectangles[0].y - 129.3f) < 0.02f);

    // No permanent XP text / popups when the menu is gone or the option is disabled.
    d = {};
    s.menu_weapons = false;
    arsenal::native_ui::draw(&overlay, s);
    assert(d.labels.empty() && d.rectangles.empty());
    s.enabled = false;
    s.playing = true;
    d = {};
    arsenal::native_ui::draw(&overlay, s);
    assert(d.labels.empty() && d.rectangles.empty());

    assert(arsenal::native_ui::level_colour(5) == 0xFFC830FFu);
    assert(arsenal::native_ui::level_colour(99) == 0xFF9030FFu);
    return 0;
}
