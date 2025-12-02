#ifndef CUSTOM_TEXT_VIEW_H
#define CUSTOM_TEXT_VIEW_H

#include <gtkmm.h>
#include "Tree.h"
#include <vector>
#include <string>

class CustomTextView : public Gtk::DrawingArea {
public:
    CustomTextView();
    ~CustomTextView() override;

    void set_tree(Tree* tree);
    void reload_from_tree();
    void mark_dirty();

    int get_cursor_byte_offset() const { return m_cursor_byte_offset; }
    void set_cursor_byte_offset(int offset);

    // helper for EditorWindow scrolling/status
    int get_line_height_for_ui() const { return m_line_height; }
    int get_cursor_line_index() const;

    // selection API
    void select_range_bytes(int startByte, int lengthBytes); // выделить диапазон
    void clear_selection();                                   // снять выделение

    // helper: прокрутить так, чтобы байтовый оффсет оказался вверху/в центре
    void scroll_to_byte_offset(int byteOffset);


protected:
    // handlers attached to controllers (gtkmm4 style)
    bool on_key_pressed(guint keyval, guint keycode, Gdk::ModifierType state);
    void on_gesture_pressed(int n_press, double x, double y);
    void on_motion(double x, double y);
    bool on_scroll(double dx, double dy);

    // draw
    void draw_with_cairo(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height);

private:
    void ensure_text_cache();
    void recompute_metrics();
    void update_size_request();
    int measure_prefix_pixels_in_line(const std::string& line, size_t bytePrefixLen);

private:
    Tree* m_tree{nullptr};
    std::string m_textCache;
    std::vector<int> m_lineOffsets;

    Pango::FontDescription m_font_desc;
    int m_line_height{16};
    int m_char_width{8};

    int m_cursor_byte_offset{0};
    bool m_show_caret{true};
    sigc::connection m_caret_timer;

    int m_sel_start = -1; // -1 => нет выделения
    int m_sel_len = 0;

    bool m_dirty{true};
};
#endif // CUSTOM_TEXT_VIEW_H
