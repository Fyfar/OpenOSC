#include "openosc/ui/components/decode_table.hpp"
#include "openosc/ui/fonts.hpp"
#include "openosc/ui/screens/screen_layout.hpp"
#include <cstdio>
#include <cstring>

namespace openosc::ui {
namespace {
using decoding::TableText;
constexpr int Row = ScreenLayout::DecoderRow;
void text(lv_layer_t *layer, const char *value, uint32_t color, int x, int y, int right) {
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.font = &openosc_mono_12;
    label.color = lv_color_hex(color);
    label.text = value;
    label.text_local = 1;
    label.flag = LV_TEXT_FLAG_EXPAND; // One row: never wrapped (TableText fits the width).
    const lv_area_t box = {x, y, right, y + Row - 1};
    lv_draw_label(layer, &label, &box);
}
void fill(lv_layer_t *layer, const lv_area_t &area, uint32_t color) {
    lv_draw_rect_dsc_t box;
    lv_draw_rect_dsc_init(&box);
    box.bg_color = lv_color_hex(color); box.bg_opa = LV_OPA_COVER;
    lv_draw_rect(layer, &box, &area);
}
bool same(const DecodeTable::Content &a, const DecodeTable::Content &b) {
    return a.set == b.set && a.sets == b.sets && a.revision == b.revision && a.selected == b.selected && a.empty == b.empty &&
           a.settings == b.settings && a.detail == b.detail && a.detailLine == b.detailLine;
}
} // namespace
void DecodeTable::create(lv_obj_t *parent) {
    object_ = lv_obj_create(parent);
    lv_obj_remove_style_all(object_);
    lv_obj_set_scrollable(object_, false);
    lv_obj_add_event_cb(object_, draw, LV_EVENT_DRAW_MAIN, this);
    content_ = {}; info_[0] = 0; first_ = 0; rows_ = {}; drawn_ = false;
    lv_obj_set_hidden(object_, true);
}
void DecodeTable::colors(const Theme &theme) {
    foreground_ = theme.foreground; background_ = theme.background; muted_ = theme.muted; panel_ = theme.panel;
    warning_ = theme.generator;
    if (object_) lv_obj_invalidate(object_);
}
void DecodeTable::place(int y, const DecoderTableRows &rows) {
    if (rows != rows_) { rows_ = rows; drawn_ = false; } // The window follows the new rows.
    lv_obj_set_pos(object_, 0, y);
    lv_obj_set_size(object_, 320, rows.height());
    if (lv_obj_is_hidden(object_)) lv_obj_set_hidden(object_, false);
    lv_obj_move_foreground(object_); // Over the elements (Elements::apply raises them on each activation).
}
void DecodeTable::hide() { if (object_ && !lv_obj_is_hidden(object_)) lv_obj_set_hidden(object_, true); }
void DecodeTable::show(const Content &content, const char *info) {
    if (!object_) return;
    Content next = content;
    if (!next.empty) next.empty = "";
    const unsigned count = next.set ? next.set->count() : 0;
    if (count && next.selected >= count) next.selected = count - 1;
    unsigned first = first_;
    if (next.selected < first) first = next.selected;
    const unsigned rows = rows_.events;
    if (rows && next.selected >= first + rows) first = next.selected - rows + 1;
    if (first + rows > count) first = count > rows ? count - rows : 0;
    if (std::strcmp(info_, info ? info : "")) { // Only its row: the info row, or Split's title.
        std::snprintf(info_, sizeof info_, "%s", info ? info : "");
        lv_area_t area;
        lv_obj_get_coords(object_, &area);
        area.y2 = area.y1 + Row - 1;
        lv_obj_invalidate_area(object_, &area);
    }
    if (drawn_ && same(next, content_) && first == first_) return;
    content_ = next; first_ = first; drawn_ = true;
    lv_obj_invalidate(object_);
}
void DecodeTable::title(const TableText::Line &header, const char *info, TableText::Line &out) {
    unsigned from = 1, to = 1 + TableText::Time; // The header's time cell.
    while (from < to && header[from] == ' ') ++from;
    while (to > from && header[to - 1] == ' ') --to;
    const int length = std::snprintf(out, sizeof out, "%.*s %s", int(to - from), header + from, info ? info : "");
    if (length >= int(sizeof out)) out[sizeof out - 2] = '~'; // Cut: said so, never scaled.
}
void DecodeTable::draw(lv_event_t *event) {
    const auto &table = *static_cast<DecodeTable *>(lv_event_get_user_data(event));
    auto c = table.content_;
    if (c.set && (!c.sets || c.sets->published() != c.set || c.sets->revision() != c.revision)) c.set = nullptr; // Stale.
    auto *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(table.object_, &area);
    fill(layer, area, table.background_);
    const int left = area.x1 + TextX, right = area.x2 - TextX;
    const DecoderTableRows &rows = table.rows_;
    const int header = area.y1 + rows.header();
    const auto row = [&](unsigned r) { return area.y1 + rows.event(r); };
    fill(layer, {area.x1, area.y1, area.x2, row(0) - 1}, table.panel_); // The info row (if any) and the header.
    if (rows.info) text(layer, table.info_, table.foreground_, left, area.y1, right);
    TableText::Line line;
    const auto unit = c.set ? TableText::unit(*c.set) : TableText::Unit{};
    TableText::header(c.set, unit, line);
    if (!rows.info) { // Split: the one title row, the time's unit and the info.
        TableText::Line heading;
        title(line, table.info_, heading);
        text(layer, heading, table.foreground_, left, header, right);
    }
    if (c.detail && c.set && c.settings) {
        if (rows.info) text(layer, "DETAIL  Up/Down scroll  MENU/F4 back", table.muted_, left, header, right);
        for (unsigned r = 0; r < rows.events && TableText::detail(*c.set, c.selected, *c.settings, c.detailLine + r, line); ++r)
            text(layer, line, table.foreground_, left, row(r), right);
        return;
    }
    if (rows.info) text(layer, line, table.muted_, left, header, right);
    const unsigned count = c.set ? c.set->count() : 0;
    if (!count) {
        text(layer, c.empty, table.muted_, left, row(0), right);
        return;
    }
    for (unsigned r = 0; r < rows.events && table.first_ + r < count; ++r) {
        const unsigned index = table.first_ + r;
        const bool selected = index == c.selected;
        const int y = row(r);
        const auto &e = c.set->event(index);
        const bool error = TableText::error(e); // Its code says it too: never by colour alone.
        if (selected) fill(layer, {area.x1, y, area.x2, y + Row - 1}, table.foreground_);
        TableText::row(*c.set, index, unit, selected, line);
        text(layer, line, selected ? table.background_ : error ? table.warning_ : table.foreground_, left, y, right);
    }
}
} // namespace openosc::ui
