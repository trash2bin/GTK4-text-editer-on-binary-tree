#include "CustomTextView.h"
#include <gdk/gdk.h>
#include <glib.h>
#include <algorithm>
#include <cstring>

// --- UTF-8 helpers -------------------------------------------------------
// Возвращает индекс <= pos, являющийся началом UTF-8 символа (lead byte).
static size_t utf8_prev_boundary(const char* data, size_t data_len, size_t pos) {
    if (pos == 0) return 0;
    if (pos > data_len) pos = data_len;
    size_t i = pos;
    // если pos == data_len и последний байт continuation, шагнём назад
    while (i > 0) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if ((c & 0xC0) != 0x80) { // not continuation (10xxxxxx)
            // 'i' is a lead byte; but if pos points inside char, we want that lead
            return i;
        }
        --i;
    }
    return 0;
}

// Возвращает индекс >= pos, являющийся началом следующего/текущего UTF-8 символа.
// Т.е. если pos в середине символа — вернёт позицию следующего lead-byte.
static size_t utf8_next_boundary(const char* data, size_t data_len, size_t pos) {
    if (pos >= data_len) return data_len;
    size_t i = pos;
    while (i < data_len) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if ((c & 0xC0) != 0x80) return i;
        ++i;
    }
    return data_len;
}


// === ctor/dtor ===
CustomTextView::CustomTextView() {
    m_font_desc.set_family("Monospace");
    m_font_desc.set_size(10 * PANGO_SCALE);

    // сделать виджет фокусируемым (можно и в EditorWindow, но лучше здесь — локально)
    set_focusable(true);

    // draw func (gtkmm4)
    set_draw_func([this](const Cairo::RefPtr<Cairo::Context>& cr, int width, int height){
        draw_with_cairo(cr, width, height);
    });

    // Key controller
    auto key_controller = Gtk::EventControllerKey::create();
    // ---------- ВНИМАНИЕ: добавляем ", false" чтобы соответствовать signature connect(slot, after)
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &CustomTextView::on_key_pressed), false);
    add_controller(key_controller);

    // Click gestures for mouse press
    auto click = Gtk::GestureClick::create();
    // signal_pressed returns void(int, double, double) so connect similarly
    click->signal_pressed().connect(sigc::mem_fun(*this, &CustomTextView::on_gesture_pressed), false);
    add_controller(click);

    // Motion controller
    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect(sigc::mem_fun(*this, &CustomTextView::on_motion), false);
    add_controller(motion);

    // Scroll controller (optional)
    auto scroll = Gtk::EventControllerScroll::create();
    // signal_scroll has signature bool(double, double) — connect needs the "after" arg too
    scroll->signal_scroll().connect(sigc::mem_fun(*this, &CustomTextView::on_scroll), false);
    add_controller(scroll);

    // caret blink
    m_caret_timer = Glib::signal_timeout().connect([this]() {
        m_show_caret = !m_show_caret;
        queue_draw();
        return true;
    }, 500);

}

CustomTextView::~CustomTextView() {
    if (m_caret_timer.connected()) m_caret_timer.disconnect();
}

// === public API ===
void CustomTextView::set_tree(Tree* tree) {
    m_tree = tree;
    m_dirty = true;
    reload_from_tree();
}

void CustomTextView::reload_from_tree() {
    if (!m_tree) {
        m_textCache.clear();
        m_lineOffsets.clear();
        update_size_request();
        queue_draw();
        return;
    }
    if (m_tree->getRoot()) {
        char* raw = m_tree->toText();
        if (raw) {
            // используем ::strlen (и подключили <cstring>)
            m_textCache.assign(raw, static_cast<size_t>(::strlen(raw)));
            delete[] raw;
        } else {
            m_textCache.clear();
        }
    } else {
        m_textCache.clear();
    }
    m_dirty = false;
    ensure_text_cache();
    update_size_request();
    queue_draw();
}

void CustomTextView::mark_dirty() {
    m_dirty = true;
    queue_draw();
}

void CustomTextView::set_cursor_byte_offset(int offset) {
    if (offset < 0) offset = 0;
    if (offset > static_cast<int>(m_textCache.size())) offset = static_cast<int>(m_textCache.size());
    m_cursor_byte_offset = offset;
    queue_draw();
}

// Реализация сделана const — как и в заголовке
int CustomTextView::get_cursor_line_index() const {
    if (m_lineOffsets.empty()) return 0;
    int byteOffset = m_cursor_byte_offset;
    auto lo = 0, hi = static_cast<int>(m_lineOffsets.size()) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (m_lineOffsets[mid] <= byteOffset) lo = mid + 1;
        else hi = mid - 1;
    }
    return std::max(0, hi);
}

// === controllers handlers ===
bool CustomTextView::on_key_pressed(guint keyval, guint /*keycode*/, Gdk::ModifierType /*state*/) {
    if (!m_tree) return false;

    // Handle arrows, backspace, delete, enter, printable chars
    if (keyval == GDK_KEY_BackSpace) {
        // если есть выделение — удаляем его целиком
        if (m_sel_start >= 0 && m_sel_len > 0) {
            int s = m_sel_start;
            int l = m_sel_len;
            try {
                if (m_tree) m_tree->erase(s, l);
            } catch(...) {}
            clear_selection();
            reload_from_tree();
            // установить курсор в начало удалённого диапазона (s)
            set_cursor_byte_offset(s);
            return true;
        }

        // обычный Backspace (удалить предыдущий символ)
        if (m_cursor_byte_offset > 0) {
            const char* start = m_textCache.c_str();
            const char* curPtr = start + m_cursor_byte_offset;
            const char* prevPtr = g_utf8_prev_char(curPtr);
            int prevOff = static_cast<int>(prevPtr - start);
            int len = m_cursor_byte_offset - prevOff;
            if (len > 0) {
                try { if (m_tree) m_tree->erase(prevOff, len); } catch(...) {}
                reload_from_tree();
                set_cursor_byte_offset(prevOff);
            }
        }
        return true;
        } else if (keyval == GDK_KEY_Delete) {
            if (m_sel_start >= 0 && m_sel_len > 0) {
                int s = m_sel_start;
                int l = m_sel_len;
                try { if (m_tree) m_tree->erase(s, l); } catch(...) {}
                clear_selection();
                reload_from_tree();
                set_cursor_byte_offset(s);
                return true;
            }
            // иначе обычный Delete - удалить следующий символ
            if (m_cursor_byte_offset < static_cast<int>(m_textCache.size())) {
                const char* start = m_textCache.c_str();
                const char* curPtr = start + m_cursor_byte_offset;
                const char* nextPtr = g_utf8_next_char(curPtr);
                int nextOff = static_cast<int>(nextPtr - start);
                int len = nextOff - m_cursor_byte_offset;
                if (len > 0) {
                    try { if (m_tree) m_tree->erase(m_cursor_byte_offset, len); } catch(...) {}
                    reload_from_tree();
                    set_cursor_byte_offset(m_cursor_byte_offset);
                }
            }
            return true;
        } else if (keyval == GDK_KEY_Left) {
        if (m_cursor_byte_offset > 0) {
            const char* start = m_textCache.c_str();
            const char* curPtr = start + m_cursor_byte_offset;
            const char* prevPtr = g_utf8_prev_char(curPtr);
            auto prevOff = static_cast<int>(prevPtr - start);
            set_cursor_byte_offset(prevOff);
        }
        return true;
    } else if (keyval == GDK_KEY_Right) {
        if (m_cursor_byte_offset < static_cast<int>(m_textCache.size())) {
            const char* start = m_textCache.c_str();
            const char* curPtr = start + m_cursor_byte_offset;
            const char* nextPtr = g_utf8_next_char(curPtr);
            auto nextOff = static_cast<int>(nextPtr - start);
            set_cursor_byte_offset(nextOff);
        }
        return true;
    } else if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        const char ch = '\n';
        try { m_tree->insert(m_cursor_byte_offset, &ch, 1); } catch(...) {}
        reload_from_tree();
        set_cursor_byte_offset(m_cursor_byte_offset + 1);
        return true;
    }

    // Printable char: get unicode codepoint from keyval
    gunichar uc = gdk_keyval_to_unicode(keyval);
    if (uc != 0) {
        char buf[6] = {0};
        int bytes = g_unichar_to_utf8(uc, buf);
        try { m_tree->insert(m_cursor_byte_offset, buf, bytes); } catch(...) {}
        reload_from_tree();
        set_cursor_byte_offset(m_cursor_byte_offset + bytes);
        return true;
    }

    return false; // unhandled -> allow propagation
}

void CustomTextView::on_gesture_pressed(int /*n_press*/, double x, double y) {
    // захват фокуса при клике — обязательно, чтобы EventControllerKey начал получать события
    clear_selection();
    grab_focus();

    // compute clicked line and position using x,y
    const int left_margin = 6;
    const int top_margin = 4;

    int clickX = static_cast<int>(x) - left_margin;
    if (clickX < 0) clickX = 0;

    auto lineIndex = static_cast<int>((y - top_margin) / (m_line_height > 0 ? m_line_height : 1));
    if (lineIndex < 0) lineIndex = 0;
    if (lineIndex >= static_cast<int>(m_lineOffsets.size())) lineIndex = static_cast<int>(m_lineOffsets.size()) - 1;

    int lineStart = m_lineOffsets[lineIndex];
    int lineEnd = (lineIndex + 1 < static_cast<int>(m_lineOffsets.size())) ? m_lineOffsets[lineIndex + 1] - 1 : static_cast<int>(m_textCache.size());
    std::string lineStr;
    if (lineEnd > lineStart) lineStr.assign(m_textCache.data() + lineStart, lineEnd - lineStart);
    else lineStr.clear();

    // linear scan by UTF-8 chars to find approximate position
    const char* p = lineStr.c_str();
    const char* endp = p + lineStr.size();
    const char* it = p;
    int chosenPrefix = 0;
    while (it < endp) {
        const char* next = g_utf8_next_char(it);
        auto prefixLen = static_cast<int>(next - p);
        int px = measure_prefix_pixels_in_line(lineStr, prefixLen);
        if (px > clickX) break;
        chosenPrefix = prefixLen;
        it = next;
    }
    int newOffset = lineStart + chosenPrefix;
    set_cursor_byte_offset(newOffset);
}

void CustomTextView::on_motion(double /*x*/, double /*y*/) {
    // not used for now
}

bool CustomTextView::on_scroll(double /*dx*/, double /*dy*/) {
    // not intercepted
    return false;
}

// === drawing / cache ===
void CustomTextView::ensure_text_cache() {
    m_lineOffsets.clear();
    const char* s = m_textCache.c_str();
    auto len = static_cast<int>(m_textCache.size());
    m_lineOffsets.push_back(0);
    for (int i = 0; i < len; ++i) {
        if (s[i] == '\n') {
            if (i + 1 <= len) m_lineOffsets.push_back(i + 1);
        }
    }
    if (m_lineOffsets.empty()) m_lineOffsets.push_back(0);
    recompute_metrics();
    m_dirty = false;
}

void CustomTextView::recompute_metrics() {
    auto layout = create_pango_layout("X");
    layout->set_font_description(m_font_desc);
    int w, h;
    layout->get_pixel_size(w, h);
    m_line_height = h > 0 ? h : 16;
    layout->set_text("M");
    layout->get_pixel_size(w, h);
    m_char_width = w > 0 ? w : 8;
}

void CustomTextView::update_size_request() {
    auto total_lines = static_cast<int>(m_lineOffsets.size());
    if (total_lines == 0) total_lines = 1;
    int total_height = total_lines * m_line_height + 10;
    set_size_request(-1, total_height);
}

int CustomTextView::measure_prefix_pixels_in_line(const std::string& line, size_t bytePrefixLen) {
    if (bytePrefixLen > line.size()) bytePrefixLen = line.size();
    std::string tmp = line.substr(0, bytePrefixLen);
    auto layout = create_pango_layout(tmp);
    layout->set_font_description(m_font_desc);
    int w, h;
    layout->get_pixel_size(w, h);
    return w;
}


void CustomTextView::draw_with_cairo(const Cairo::RefPtr<Cairo::Context>& cr, int /*width*/, int /*height*/) {
    if (m_dirty) ensure_text_cache();

    try {
        Gdk::RGBA bg;
        auto ctx = get_style_context();
        // попытка
        bg.set("background");
        ctx->add_class("background"); // может не быть
        cr->set_source_rgba(bg.get_red(), bg.get_green(), bg.get_blue(), bg.get_alpha());
        // если нет — остаёмся с белым
        //cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->paint();
    } catch(...) {
        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->paint();
    }


    const int left_margin = 6;
    const int top_margin = 4;

    size_t nlines = m_lineOffsets.size();
    for (size_t i = 0; i < nlines; ++i) {
        int start = m_lineOffsets[i];
        int end = (i + 1 < nlines) ? m_lineOffsets[i + 1] - 1 : static_cast<int>(m_textCache.size());
        if (end < start) end = start;
        int len = end - start;

        // текст линии в байтах
        std::string line_tmp(m_textCache.data() + start, static_cast<size_t>(len));

        // --- selection drawing for this line (как у тебя было) ---
        bool has_selection_this_line = false;
        int sel_L = 0, sel_R = 0; // в байтах относительно начала строки
        if (m_sel_start >= 0 && m_sel_len > 0) {
            int sel_beg = m_sel_start;
            int sel_end = m_sel_start + m_sel_len; // exclusive
            // intersection with this line [start, end)
            int L = std::max(start, sel_beg);
            int R = std::min(end, sel_end);
            if (R > L) {
                has_selection_this_line = true;
                sel_L = L - start;
                sel_R = R - start;

                // измерим X начала и конца (в пикселях) внутри этой строки
                int x1 = left_margin + measure_prefix_pixels_in_line(line_tmp, static_cast<size_t>(sel_L));
                int x2 = left_margin + measure_prefix_pixels_in_line(line_tmp, static_cast<size_t>(sel_R));
                int y = top_margin + static_cast<int>(i) * m_line_height;
                double sel_a = 0.35; // прозрачность

                // fallback: light blue
                Gdk::RGBA sel_color;
                sel_color.set_red(0.15);
                sel_color.set_green(0.45);
                sel_color.set_blue(0.85);
                sel_color.set_alpha(sel_a);

                cr->save();
                cr->set_source_rgba(sel_color.get_red(), sel_color.get_green(), sel_color.get_blue(), sel_color.get_alpha());
                cr->rectangle(x1, y, x2 - x1, m_line_height);
                cr->fill();
                cr->restore();
            }
        }

        // --- DRAW TEXT: возможна сегментация на части для контраста в выделении ---
        // Если выделение пересекает часть строки, рисуем три фрагмента: before, selected, after.
        // В простом случае рисуем всю строку одним цветом.
        auto layout = create_pango_layout(""); // создадим layout, будем менять текст
        layout->set_font_description(m_font_desc);

        // helper: функция для рисования фрагмента строки с указанным смещением цвета
        auto draw_fragment = [&](size_t frag_start, size_t frag_len, const Gdk::RGBA& color) {
            if (frag_len == 0) return;
            if (frag_start >= line_tmp.size()) return;
            if (frag_start + frag_len > line_tmp.size()) frag_len = line_tmp.size() - frag_start;

            std::string frag(line_tmp.data() + frag_start, frag_len);
            layout->set_text(frag);

            // Смещение X от начала строки до начала фрагмента
            int offset_x = left_margin + measure_prefix_pixels_in_line(line_tmp, frag_start);
            int y = top_margin + static_cast<int>(i) * m_line_height;

            cr->save();
            cr->translate(offset_x, y);
            cr->set_source_rgba(color.get_red(), color.get_green(), color.get_blue(), color.get_alpha());
            pango_cairo_show_layout(cr->cobj(), layout->gobj());
            cr->restore();
        };


        // Цвет текста по умолчанию (попытайся взять из темы, иначе чёрный)
        Gdk::RGBA fg_color;
        try {
            auto ctx = get_style_context();
            // STATE_FLAG_NORMAL — обычное состояние (не фокус, не активное)
            fg_color = ctx->get_color();
        } catch(...) {
            // fallback на черный, если что-то пошло не так
            fg_color.set_red(0.0);
            fg_color.set_green(0.0);
            fg_color.set_blue(0.0);
            fg_color.set_alpha(1.0);
        }


        // Цвет текста внутри выделения (контрастный) — белый
        Gdk::RGBA sel_fg;
        sel_fg.set_red(1.0);
        sel_fg.set_green(1.0);
        sel_fg.set_blue(1.0);
        sel_fg.set_alpha(1.0);

        if (!has_selection_this_line) {
            // просто весь текст
            draw_fragment(0, line_tmp.size(), fg_color);
        } else {
            // before
            draw_fragment(0, static_cast<size_t>(sel_L), fg_color);
            // selected
            draw_fragment(static_cast<size_t>(sel_L), static_cast<size_t>(sel_R - sel_L), sel_fg);
            // after
            draw_fragment(static_cast<size_t>(sel_R), (line_tmp.size() - sel_R), fg_color);
        }
    } // конец цикла по строкам

    // caret
    if (m_show_caret) {
        int byteOffset = m_cursor_byte_offset;
        if (byteOffset < 0) byteOffset = 0;
        if (byteOffset > static_cast<int>(m_textCache.size())) byteOffset = static_cast<int>(m_textCache.size());
        int lineIndex = static_cast<int>(std::upper_bound(m_lineOffsets.begin(), m_lineOffsets.end(), byteOffset) - m_lineOffsets.begin() - 1);
        if (lineIndex < 0) lineIndex = 0;
        if (lineIndex >= static_cast<int>(m_lineOffsets.size())) lineIndex = static_cast<int>(m_lineOffsets.size()) - 1;
        int lineStart = m_lineOffsets[lineIndex];
        int prefixLen = byteOffset - lineStart;
        if (prefixLen < 0) prefixLen = 0;
        int lineEnd = (lineIndex + 1 < static_cast<int>(m_lineOffsets.size())) ? m_lineOffsets[lineIndex + 1] - 1 : static_cast<int>(m_textCache.size());
        std::string lineBytes;
        if (lineEnd > lineStart) lineBytes.assign(m_textCache.data() + lineStart, lineEnd - lineStart);
        else lineBytes.clear();
        int cursor_x = left_margin + measure_prefix_pixels_in_line(lineBytes, prefixLen);
        int cursor_y = top_margin + lineIndex * m_line_height;
        cr->save();
        cr->set_source_rgb(0.0, 0.0, 0.0);
        cr->rectangle(cursor_x, cursor_y, 1.0, m_line_height);
        cr->fill();
        cr->restore();
    }
}


void CustomTextView::select_range_bytes(int startByte, int lengthBytes) {
    if (startByte < 0) startByte = 0;
    int maxb = static_cast<int>(m_textCache.size());
    if (startByte > maxb) startByte = maxb;

    if (lengthBytes <= 0) {
        m_sel_start = -1;
        m_sel_len = 0;
        queue_draw();
        return;
    }

    // clamp end
    int endByte = startByte + lengthBytes;
    if (endByte > maxb) endByte = maxb;

    // нормализуем к UTF-8 границам внутри соответствующей строки-хранителя (m_textCache)
    const char* data = m_textCache.data();
    size_t data_len = static_cast<size_t>(maxb);

    size_t sb = static_cast<size_t>(startByte);
    size_t eb = static_cast<size_t>(endByte);

    // подвинем начало влево до lead-byte (если оно в середине)
    size_t safe_sb = utf8_prev_boundary(data, data_len, sb);
    // сдвинем конец вправо до начала следующего символа (после последнего байта)
    size_t safe_eb = utf8_next_boundary(data, data_len, eb);

    // если получилось пусто — считаем как no-selection
    if (safe_eb <= safe_sb) {
        m_sel_start = -1;
        m_sel_len = 0;
    } else {
        m_sel_start = static_cast<int>(safe_sb);
        m_sel_len = static_cast<int>(safe_eb - safe_sb);
    }
    queue_draw();
}


void CustomTextView::clear_selection() {
    m_sel_start = -1;
    m_sel_len = 0;
    queue_draw();
}

void CustomTextView::scroll_to_byte_offset(int byteOffset) {
    if (byteOffset < 0) byteOffset = 0;
    if (byteOffset > static_cast<int>(m_textCache.size())) byteOffset = static_cast<int>(m_textCache.size());

    int lineIndex = 0;
    if (!m_lineOffsets.empty()) {
        auto it = std::upper_bound(m_lineOffsets.begin(), m_lineOffsets.end(), byteOffset);
        lineIndex = static_cast<int>(std::distance(m_lineOffsets.begin(), it)) - 1;
        if (lineIndex < 0) lineIndex = 0;
    }

    int y = lineIndex * m_line_height;

    // Идем вверх по дереву родителей, ищем Gtk::ScrolledWindow и Gtk::Window (топ-левел)
    Gtk::Widget* w = this;
    Gtk::ScrolledWindow* found_sw = nullptr;
    Gtk::Window* found_win = nullptr;

    while (w) {
        // родитель виджета
        auto parent = w->get_parent();
        if (!parent) {
            // возможно w сам и есть top-level window
            found_win = dynamic_cast<Gtk::Window*>(w);
            break;
        }

        // если родитель — ScrolledWindow, запомним его
        found_sw = dynamic_cast<Gtk::ScrolledWindow*>(parent);
        if (found_sw) {
            // можем дальше продолжать подниматься, но хватит — у нас есть scrolledwindow
            // также попробуем найти окно выше него
            Gtk::Widget* up = parent;
            while (up) {
                auto pup = up->get_parent();
                if (!pup) { found_win = dynamic_cast<Gtk::Window*>(up); break; }
                up = pup;
            }
            break;
        }

        // идём вверх
        w = parent;
    }

    // Если нашли scrolledwindow — устанавливем vadjustment
    if (found_sw) {
        if (auto vadj = found_sw->get_vadjustment()) {
            int maxv = static_cast<int>(vadj->get_upper() - vadj->get_page_size());
            if (y < 0) y = 0;
            if (y > maxv) y = maxv;
            vadj->set_value(y);
        }
        return;
    }

    // Если scrolledwindow не найден, но нашли окно, попробуем установить позицию через окно (менее точный путь)
    if (found_win) {
        // ничего специального делать — нельзя напрямую скроллить окно; просто ничего делать
        return;
    }

    // Ничего не найдено — просто ничего не делаем
}

