// CustomTextView.cpp
#include "CustomTextView.h"
#include <gdk/gdk.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <algorithm>
#include <cstring>
#include <iostream> // для простого логирования ошибок
#include <cassert>

// --- UTF-8 helpers-------------------------------
// Возвращает индекс lead-byte, который лежит <= pos-1 (т.е. начало символа,
// охватывающего байт pos-1), или 0 если не найдено. Без UB при pos == data_len.
static size_t utf8_prev_boundary(const char* data, size_t data_len, size_t pos) {
    if (data_len == 0) return 0;
    if (pos == 0) return 0;
    if (pos > data_len) pos = data_len;

    // Начинаем с байта перед pos — это безопасно
    size_t i = pos - 1;
    // Если текущий байт continuation (10xxxxxx), сдвигаемся влево до lead-byte
    while (i > 0 && (static_cast<unsigned char>(data[i]) & 0xC0) == 0x80) --i;
    return i;
}

// Возвращает позицию lead-byte для следующего символа (т.е. начало символа,
// который начинается не раньше чем в pos). Если pos уже указывает на lead-byte,
// вернёт pos. Если pos >= data_len — вернёт data_len.
static size_t utf8_next_boundary(const char* data, size_t data_len, size_t pos) {
    if (data_len == 0) return 0;
    if (pos >= data_len) return data_len;
    size_t i = pos;
    // Если pos указывает внутри continuation байтов, продвигаемся вправо
    while (i < data_len && (static_cast<unsigned char>(data[i]) & 0xC0) == 0x80) ++i;
    return i;
}


// === ctor/dtor =============================================================
CustomTextView::CustomTextView() {
    m_font_desc.set_family("Monospace");
    m_font_desc.set_size(10 * PANGO_SCALE);

    set_focusable(true);

    set_draw_func([this](const Cairo::RefPtr<Cairo::Context>& cr, int width, int height){
        draw_with_cairo(cr, width, height);
    });

    // Key controller
    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &CustomTextView::on_key_pressed), false);
    add_controller(key_controller);

    // Click gestures for mouse press
    auto click = Gtk::GestureClick::create();
    click->signal_pressed().connect(sigc::mem_fun(*this, &CustomTextView::on_gesture_pressed), false);
    click->signal_released().connect(sigc::mem_fun(*this, &CustomTextView::on_gesture_released), false);
    add_controller(click);

    // ensure flags initial state
    m_mouse_selecting = false;
    m_sel_anchor = -1;
    m_desired_column_px = -1;

    // Motion controller
    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect(sigc::mem_fun(*this, &CustomTextView::on_motion), false);
    add_controller(motion);

    // Scroll controller (optional)
    auto scroll = Gtk::EventControllerScroll::create();
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


// === public API ============================================================
void CustomTextView::set_tree(Tree* tree) {
    m_tree = tree;
    reload_from_tree();
}

void CustomTextView::reload_from_tree() {
    update_size_request();
    queue_draw();
}

void CustomTextView::mark_dirty() {
    m_dirty = true;
    queue_draw();
}

void CustomTextView::set_cursor_byte_offset(int offset) {
    if (!m_tree) return;
    
    int maxLen = 0;
    
    // Используем isEmpty() 
    if (!m_tree->isEmpty()) { 
        maxLen = m_tree->getRoot()->getLength();
    }

    if (offset < 0) offset = 0;
    if (offset > maxLen) offset = maxLen;
    
    m_cursor_byte_offset = offset;
    m_show_caret = true; 
    queue_draw();
}
// Нам нужно найти, в какой строке находится курсор.
// Так как в Tree.h нет метода getLineIndexByOffset, мы используем бинарный поиск
// по номерам строк, используя быстрый m_tree->getOffsetForLine().
int CustomTextView::find_line_index_by_byte_offset(int targetOffset) const {
    if (!m_tree || m_tree->isEmpty()) return 0;

    int low = 0;
    int high = m_tree->getTotalLineCount() - 1;
    int result = 0;

    while (low <= high) {
        int mid = low + (high - low) / 2;
        int midOffset = m_tree->getOffsetForLine(mid);

        if (midOffset <= targetOffset) {
            result = mid; // запоминаем как кандидата
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return result;
}

int CustomTextView::get_cursor_line_index() const {
    return find_line_index_by_byte_offset(m_cursor_byte_offset);
}

// === controllers handlers ================================================
bool CustomTextView::on_key_pressed(guint keyval, guint /*keycode*/, Gdk::ModifierType /*state*/) {
    if (!m_tree) return false;

    // Вспомогательная лямбда для удаления диапазона и обновления UI
    auto perform_erase = [&](int start, int len) {
        try {
            m_tree->erase(start, len);
        } catch (const std::exception& e) {
            std::cerr << "Tree::erase error: " << e.what() << '\n';
        }
        clear_selection();
        reload_from_tree(); // Теперь это быстрая операция
        set_cursor_byte_offset(start);
    };

    // 1. Обработка BACKSPACE
    if (keyval == GDK_KEY_BackSpace) {
        // Если есть выделение - удаляем его
        if (m_sel_start >= 0 && m_sel_len > 0) {
            perform_erase(m_sel_start, m_sel_len);
            return true;
        }

        // Удаление символа слева
        if (m_cursor_byte_offset > 0) {
            // Находим строку, в которой курсор
            int lineIdx = find_line_index_by_byte_offset(m_cursor_byte_offset);
            int lineStart = m_tree->getOffsetForLine(lineIdx);
            int localOffset = m_cursor_byte_offset - lineStart;

            int lenToDelete = 1; // По умолчанию (например, удаляем \n на границе)

            if (localOffset > 0) {
                // Мы внутри строки, нужно определить длину UTF-8 символа перед курсором
                char* rawLine = m_tree->getLine(lineIdx);
                if (rawLine) {
                    std::unique_ptr<char[]> guard(rawLine);
                    const char* curPtr = rawLine + localOffset;
                    const char* prevPtr = g_utf8_prev_char(curPtr);
                    lenToDelete = static_cast<int>(curPtr - prevPtr);
                }
            } else {
                // Мы в начале строки, удаляем символ перед нами (это \n предыдущей строки)
                // Оставляем lenToDelete = 1
            }
            
            perform_erase(m_cursor_byte_offset - lenToDelete, lenToDelete);
        }
        return true;
    } 
    
    // 2. Обработка DELETE
    else if (keyval == GDK_KEY_Delete) {
        if (m_sel_start >= 0 && m_sel_len > 0) {
            perform_erase(m_sel_start, m_sel_len);
            return true;
        }

        int maxLen = m_tree->getRoot() ? m_tree->getRoot()->getLength() : 0;
        if (m_cursor_byte_offset < maxLen) {
            int lineIdx = find_line_index_by_byte_offset(m_cursor_byte_offset);
            int lineStart = m_tree->getOffsetForLine(lineIdx);
            int localOffset = m_cursor_byte_offset - lineStart;

            int lenToDelete = 1;

            // Получаем строку, чтобы узнать длину следующего символа
            char* rawLine = m_tree->getLine(lineIdx);
            if (rawLine) {
                std::unique_ptr<char[]> guard(rawLine);
                size_t lineLen = std::strlen(rawLine);
                
                // Если курсор не в самом конце строки (не перед \n или концом файла)
                if (localOffset < (int)lineLen) {
                    const char* curPtr = rawLine + localOffset;
                    const char* nextPtr = g_utf8_next_char(curPtr);
                    lenToDelete = static_cast<int>(nextPtr - curPtr);
                }
                // Иначе удаляем 1 байт (это \n)
            }
            
            perform_erase(m_cursor_byte_offset, lenToDelete);
        }
        return true;
    } 
    
    // 3. Стрелка ВЛЕВО
    else if (keyval == GDK_KEY_Left) {
        if (m_cursor_byte_offset > 0) {
            int lineIdx = find_line_index_by_byte_offset(m_cursor_byte_offset);
            int lineStart = m_tree->getOffsetForLine(lineIdx);
            int localOffset = m_cursor_byte_offset - lineStart;
            
            int step = 1;
            if (localOffset > 0) {
                char* rawLine = m_tree->getLine(lineIdx);
                if (rawLine) {
                    std::unique_ptr<char[]> guard(rawLine);
                    const char* curPtr = rawLine + localOffset;
                    const char* prevPtr = g_utf8_prev_char(curPtr);
                    step = static_cast<int>(curPtr - prevPtr);
                }
            }
            set_cursor_byte_offset(m_cursor_byte_offset - step);
        }
        clear_selection();
        return true;
    } 
    
    // 4. Стрелка ВПРАВО
    else if (keyval == GDK_KEY_Right) {
        int maxLen = m_tree->getRoot() ? m_tree->getRoot()->getLength() : 0;
        if (m_cursor_byte_offset < maxLen) {
            int lineIdx = find_line_index_by_byte_offset(m_cursor_byte_offset);
            int lineStart = m_tree->getOffsetForLine(lineIdx);
            int localOffset = m_cursor_byte_offset - lineStart;
            
            int step = 1;
            char* rawLine = m_tree->getLine(lineIdx);
            if (rawLine) {
                std::unique_ptr<char[]> guard(rawLine);
                size_t lineLen = std::strlen(rawLine);
                if (localOffset < (int)lineLen) {
                    const char* curPtr = rawLine + localOffset;
                    const char* nextPtr = g_utf8_next_char(curPtr);
                    step = static_cast<int>(nextPtr - curPtr);
                }
            }
            set_cursor_byte_offset(m_cursor_byte_offset + step);
        }
        clear_selection();
        return true;
    } 
    
    // 5. ENTER
    else if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        char ch = '\n';
        try {
            m_tree->insert(m_cursor_byte_offset, &ch, 1);
        } catch (const std::exception& e) {
            std::cerr << "Tree::insert error: " << e.what() << '\n';
        }
        reload_from_tree();
        set_cursor_byte_offset(m_cursor_byte_offset + 1);
        clear_selection(); // Обычно Enter сбрасывает выделение
        return true;
    }

    // 6. Печатные символы
    gunichar uc = gdk_keyval_to_unicode(keyval);
    if (uc != 0 && !g_unichar_iscntrl(uc)) {
        char buf[8] = {0};
        int bytes = g_unichar_to_utf8(uc, buf);
        
        // Если текст выделен - заменяем его
        if (m_sel_start >= 0 && m_sel_len > 0) {
            try { m_tree->erase(m_sel_start, m_sel_len); } catch(...) {}
            m_cursor_byte_offset = m_sel_start;
            clear_selection();
        }
        
        try {
            m_tree->insert(m_cursor_byte_offset, buf, bytes);
        } catch (const std::exception& e) {
            std::cerr << "Tree::insert error: " << e.what() << '\n';
        }
        reload_from_tree();
        set_cursor_byte_offset(m_cursor_byte_offset + bytes);
        return true;
    }

    return false;
}


void CustomTextView::on_gesture_released(int /*n_press*/, double /*x*/, double /*y*/) {
    // Завершаем drag-selection
    if (!m_mouse_selecting) return;
    m_mouse_selecting = false;

    // Если ничего не выделено — сбросили якорь
    if (m_sel_start < 0 || m_sel_len == 0) {
        m_sel_anchor = -1;
    } else {
        // оставляем текущее выделение и курсор в его конце
        set_cursor_byte_offset(m_sel_start + m_sel_len);
    }
}


void CustomTextView::on_gesture_pressed(int /*n_press*/, double x, double y) {
    if (!m_tree) return;
    
    clear_selection();
    grab_focus();

    // Мы убрали m_dirty / ensure_text_cache, так как данные всегда в дереве.
    // Просто обновляем размеры, если вдруг дерево поменялось извне (редкий кейс)
    // update_size_request(); 

    // Вся магия вычисления теперь в одной строчке, которую мы написали ранее:
    int newOffset = get_byte_offset_at_xy(x, y);

    m_mouse_selecting = true;
    m_sel_anchor = newOffset;
    
    select_range_bytes(newOffset, 0);
    set_cursor_byte_offset(newOffset);
}

void CustomTextView::on_motion(double x, double y) {
    // если не в режиме выделения — ничего не делаем
    if (!m_mouse_selecting) return;
    if (!m_tree) return;

    int currentOffset = get_byte_offset_at_xy(x, y);

    // Обновляем выделение между якорем и текущей позицией
    if (m_sel_anchor < 0) {
        m_sel_anchor = currentOffset;
    }
    
    int selBeg = std::min(m_sel_anchor, currentOffset);
    int selEnd = std::max(m_sel_anchor, currentOffset);
    
    select_range_bytes(selBeg, selEnd - selBeg);

    // Курсор всегда на том конце выделения, который двигает мышь
    set_cursor_byte_offset(currentOffset);
}


bool CustomTextView::on_scroll(double /*dx*/, double /*dy*/) {
    // not intercepted
    return false;
}



void CustomTextView::recompute_metrics() {
    auto layout = create_pango_layout("X");
    layout->set_font_description(m_font_desc);
    int w = 0, h = 0;
    layout->get_pixel_size(w, h);
    m_line_height = h > 0 ? h : 16;

    layout->set_text("M");
    layout->get_pixel_size(w, h);
    m_char_width = w > 0 ? w : 8;
}

void CustomTextView::update_size_request() {
    if (!m_tree) return;
    
    // Если дерево пустое, считаем, что у нас 1 пустая строка
    if (m_tree->isEmpty()) {
        int h = m_line_height + (TOP_MARGIN * 2);
        set_size_request(-1, h);
        return;
    }

    int total_lines = m_tree->getTotalLineCount(); 
    if (total_lines == 0) total_lines = 1;
    
    int h = total_lines * m_line_height + (TOP_MARGIN * 2);
    set_size_request(-1, h);
}


// === ОТРИСОВКА (СЕРДЦЕ ОПТИМИЗАЦИИ) ===
void CustomTextView::draw_with_cairo(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) {
    if (!m_tree) {
        // Заливаем белым, если дерева нет
        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->paint();
        return;
    }

    // Рисуем фон
    cr->set_source_rgb(0.3, 0.3, 0.3); // Или цвет из темы
    cr->paint();

    // !!! ВАЖНО: Если дерево пустое, рисуем только курсор (если нужен) и выходим
    if (m_tree->isEmpty()) {
        if (m_show_caret) {
            cr->set_source_rgb(0, 0, 0);
            cr->rectangle(LEFT_MARGIN, TOP_MARGIN, 1.5, m_line_height);
            cr->fill();
        }
        return;
    }

    // Определяем видимую область (Culling)
    double clip_x1, clip_y1, clip_x2, clip_y2;
    cr->get_clip_extents(clip_x1, clip_y1, clip_x2, clip_y2);

    int total_lines = m_tree->getTotalLineCount();
    
    // Вычисляем индексы первой и последней видимой строки
    int first_line = static_cast<int>((clip_y1 - TOP_MARGIN) / m_line_height);
    int last_line = static_cast<int>((clip_y2 - TOP_MARGIN) / m_line_height) + 1;

    // Ограничиваем диапазоном [0, total_lines]
    first_line = std::clamp(first_line, 0, std::max(0, total_lines - 1));
    last_line = std::clamp(last_line, 0, total_lines);
    if (last_line <= first_line) last_line = first_line + 1;

    // Цвета
    Gdk::RGBA text_color("black");
    Gdk::RGBA sel_bg(0.2, 0.4, 0.8, 0.4); // Выделение

    // Цикл ТОЛЬКО по видимым строкам
    for (int i = first_line; i < last_line; ++i) {
        // === МАГИЯ ДЕРЕВА ===
        // Получаем текст только одной строки.
        // getLine возвращает char*, который мы должны освободить.
        char* rawLine = m_tree->getLine(i);
        if (!rawLine) continue;

        // Используем unique_ptr для авто-удаления rawLine
        std::unique_ptr<char[]> lineGuard(rawLine);
        std::string lineStr(rawLine); 

        // Если строка оканчивается на \n, Pango лучше его не скармливать для отрисовки
        if (!lineStr.empty() && lineStr.back() == '\n') {
            lineStr.pop_back();
        }

        int lineStartOffset = m_tree->getOffsetForLine(i);
        int lineEndOffset = lineStartOffset + (int)std::strlen(rawLine); // с учетом \n
        
        int y_pos = TOP_MARGIN + i * m_line_height;

        // --- Отрисовка выделения (Selection) ---
        int sel_s = m_sel_start;
        int sel_e = m_sel_start + m_sel_len;

        // Пересекается ли глобальное выделение с этой строкой?
        if (m_sel_len > 0 && sel_s < lineEndOffset && sel_e > lineStartOffset) {
            int local_s = std::max(sel_s, lineStartOffset) - lineStartOffset;
            int local_e = std::min(sel_e, lineEndOffset) - lineStartOffset;
            
            // Layout для измерения ширины выделения
            auto layout = create_pango_layout(lineStr);
            layout->set_font_description(m_font_desc);
            
            // Конвертируем байтовые индексы в пиксели X
            int x1, x2;
            Pango::Rectangle rect;
            
            // X1
            layout->get_cursor_pos(std::min((int)lineStr.size(), local_s), rect, rect);
            x1 = LEFT_MARGIN + rect.get_x() / PANGO_SCALE;
            
            // X2
            layout->get_cursor_pos(std::min((int)lineStr.size(), local_e), rect, rect);
            x2 = LEFT_MARGIN + rect.get_x() / PANGO_SCALE;
            
            // Если выделение идет дальше конца текста (на символ \n), добавляем ширину пробела
            if (local_e > (int)lineStr.size()) {
                x2 += 8; 
            }

            cr->set_source_rgba(sel_bg.get_red(), sel_bg.get_green(), sel_bg.get_blue(), sel_bg.get_alpha());
            cr->rectangle(x1, y_pos, x2 - x1, m_line_height);
            cr->fill();
        }

        // --- Отрисовка текста ---
        if (!lineStr.empty()) {
            auto layout = create_pango_layout(lineStr);
            layout->set_font_description(m_font_desc);
            
            cr->move_to(LEFT_MARGIN, y_pos);
            cr->set_source_rgb(1,1,1);
            pango_cairo_show_layout(cr->cobj(), layout->gobj());
        }
    }

    // Рисуем курсор
    // Для оптимизации рисуем только если курсор попадает в видимые строки
    if (m_show_caret && m_cursor_byte_offset >= 0) {
        int cursorLineIdx = find_line_index_by_byte_offset(m_cursor_byte_offset);
        
        if (cursorLineIdx >= first_line && cursorLineIdx < last_line) {
            char* rawLine = m_tree->getLine(cursorLineIdx);
            if (rawLine) {
                std::unique_ptr<char[]> guard(rawLine);
                std::string lineStr(rawLine);
                if (!lineStr.empty() && lineStr.back() == '\n') lineStr.pop_back();

                int lineStart = m_tree->getOffsetForLine(cursorLineIdx);
                int offsetInLine = m_cursor_byte_offset - lineStart;
                
                // Защита от выхода за границы строки
                if (offsetInLine > (int)lineStr.size()) offsetInLine = (int)lineStr.size();

                auto layout = create_pango_layout(lineStr);
                layout->set_font_description(m_font_desc);
                
                Pango::Rectangle pos;
                layout->get_cursor_pos(offsetInLine, pos, pos);
                
                int cx = LEFT_MARGIN + pos.get_x() / PANGO_SCALE;
                int cy = TOP_MARGIN + cursorLineIdx * m_line_height;

                cr->set_source_rgb(0, 0, 0);
                cr->rectangle(cx, cy, 1.5, m_line_height);
                cr->fill();
            }
        }
    }
}


int CustomTextView::get_byte_offset_at_xy(double x, double y) {
    if (!m_tree) return 0;
    if (m_tree->isEmpty()) return 0;
    
    //  Какая это строка визуально?
    int lineIdx = static_cast<int>((y - TOP_MARGIN) / m_line_height);
    int total = m_tree->getTotalLineCount();
    
    if (lineIdx < 0) lineIdx = 0;
    if (lineIdx >= total) lineIdx = total - 1;

    // Получаем начало строки из дерева
    int lineStartOffset = m_tree->getOffsetForLine(lineIdx);
    
    // Получаем текст строки для измерения Pango
    char* rawLine = m_tree->getLine(lineIdx);
    if (!rawLine) return lineStartOffset;
    
    std::unique_ptr<char[]> guard(rawLine);
    std::string lineStr(rawLine);
    // Для расчетов позиции курсора \n в конце обычно не нужен, 
    // но если строка пустая (только \n), lineStr станет "", что ок.
    if (!lineStr.empty() && lineStr.back() == '\n') lineStr.pop_back();

    // Спрашиваем Pango: какой байт соответствует X?
    auto layout = create_pango_layout(lineStr);
    layout->set_font_description(m_font_desc);
    
    int index = 0;
    int trailing = 0;
    int clickX = static_cast<int>(x) - LEFT_MARGIN;

    layout->xy_to_index(clickX * PANGO_SCALE, 0, index, trailing);

    // xy_to_index возвращает индекс байта символа.
    // trailing > 0 значит кликнули в правую половину символа -> сдвигаем к следующему
    const char* ptr = lineStr.c_str() + index;
    if (trailing > 0) {
        ptr = g_utf8_next_char(ptr);
    }
    
    int offsetInLine = static_cast<int>(ptr - lineStr.c_str());
    
    return lineStartOffset + offsetInLine;
}

// === selection / misc =====================================================
void CustomTextView::select_range_bytes(int startByte, int lengthBytes) {
    if (!m_tree) return;

    // 1. Получаем реальную длину текста из дерева (O(1) или O(logN))
    int maxLen = 0;
    if (m_tree->getRoot()) {
        maxLen = m_tree->getRoot()->getLength();
    }

    // 2. Валидация входных данных
    if (startByte < 0) startByte = 0;
    if (startByte > maxLen) startByte = maxLen;

    if (lengthBytes <= 0) {
        m_sel_start = -1;
        m_sel_len = 0;
        queue_draw();
        return;
    }

    int endByte = startByte + lengthBytes;
    if (endByte > maxLen) endByte = maxLen;

    // 3. Установка выделения
    // Мы доверяем источнику вызова (мышь/клавиатура), что байты попадают на границы символов.
    // В виртуальном режиме проверка "глобальных" границ UTF-8 слишком дорогая операция,
    // так как пришлось бы склеивать фрагменты листьев дерева.
    
    if (endByte <= startByte) {
        m_sel_start = -1;
        m_sel_len = 0;
    } else {
        m_sel_start = startByte;
        m_sel_len = endByte - startByte;
    }
    
    queue_draw();
}

void CustomTextView::clear_selection() {
    m_sel_start = -1;
    m_sel_len = 0;
    queue_draw();
}

void CustomTextView::scroll_to_byte_offset(int byteOffset) {
    if (!m_tree) return;

    // 1. Ограничиваем offset
    int maxLen = m_tree->getRoot() ? m_tree->getRoot()->getLength() : 0;
    if (byteOffset < 0) byteOffset = 0;
    if (byteOffset > maxLen) byteOffset = maxLen;

    // 2. Находим индекс строки через Дерево (Virtual List logic)
    // Используем тот же алгоритм, что и в get_cursor_line_index
    int lineIndex = find_line_index_by_byte_offset(byteOffset);

    // 3. Вычисляем целевую Y координату
    int y = lineIndex * m_line_height; // Используем TOP_MARGIN если нужно точное позиционирование: + TOP_MARGIN

    // 4. Стандартная логика GTK для поиска ScrolledWindow и прокрутки
    Gtk::Widget* w = this;
    Gtk::ScrolledWindow* found_sw = nullptr;
    
    // Поднимаемся по иерархии виджетов
    while (w) {
        auto parent = w->get_parent();
        if (!parent) {
            // Дошли до окна, но скролл не нашли
            break;
        }
        found_sw = dynamic_cast<Gtk::ScrolledWindow*>(parent);
        if (found_sw) {
            break;
        }
        w = parent;
    }

    if (found_sw) {
        auto vadj = found_sw->get_vadjustment();
        if (vadj) {
            // Учитываем размер страницы, чтобы не скроллить, если курсор уже виден
            double page_size = vadj->get_page_size();
            double current_val = vadj->get_value();
            double target_y = static_cast<double>(y);
            
            // Если курсор выше видимой области -> скроллим вверх
            if (target_y < current_val) {
                vadj->set_value(target_y);
            }
            // Если курсор ниже видимой области -> скроллим вниз
            else if (target_y + m_line_height > current_val + page_size) {
                 vadj->set_value(target_y + m_line_height - page_size);
            }
            // Иначе (курсор и так виден) ничего не делаем
        }
    }
}