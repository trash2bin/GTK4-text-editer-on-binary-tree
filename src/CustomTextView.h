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

    void on_gesture_released(int /*n_press*/, double /*x*/, double /*y*/);

private:
    // Глобальные константы отступов
    static constexpr int LEFT_MARGIN = 6;
    static constexpr int TOP_MARGIN = 4;
    
    void update_size_request();
    void recompute_metrics();

    // Получает текст конкретной строки из дерева и измеряет X
    int get_byte_offset_at_xy(double x, double y);
    
    // Вспомогательная функция для бинарного поиска строки по байтовому оффсету
    // (так как в Tree нет прямого метода getLineByOffset, но есть getOffsetForLine)
    int find_line_index_by_byte_offset(int byteOffset) const;

private:
    Tree* m_tree{nullptr};

    Pango::FontDescription m_font_desc;
    int m_line_height{16};
    int m_char_width{8};

    int m_cursor_byte_offset{0};
    bool m_show_caret{true};
    sigc::connection m_caret_timer;

    int m_sel_start = -1; // -1 => нет выделения
    int m_sel_len = 0;

    // в private:
    bool m_mouse_selecting = false;   // true — идёт drag-selection
    int m_sel_anchor = -1;            // байтовый оффсет начала выделения (якорь)
    int m_desired_column_px = -1;     // предпочитаемая горизонтальная позиция (px) для Up/Down


    bool m_dirty{true};
};
#endif // CUSTOM_TEXT_VIEW_H
