#include "EditorWindow.h"
#include "BinaryTreeFile.h"
#include <fstream>
#include <glib.h>
#include <iostream>
#include <sstream>
#include <string>

// Определение вспомогательной функции
static size_t count_words(const std::string& s) {
    std::istringstream iss(s);
    size_t cnt = 0;
    std::string w;
    while (iss >> w) ++cnt;
    return cnt;
}

EditorWindow::EditorWindow() {
    
    // --- Применение системной темы ---
    set_decorated(true);

    // --- HeaderBar: make it look GNOME-like ---
    m_header_bar.set_show_title_buttons(true);
    m_header_bar.get_style_context()->add_class("titlebar");
    m_header_bar.get_style_context()->add_class("flat"); 
    m_header_bar.get_style_context()->add_class("background"); 

    // right area: search entry (compact)
    m_search.set_hexpand(false);
    m_search.set_placeholder_text("Line number or text...");
    m_search.signal_activate().connect(sigc::mem_fun(*this, &EditorWindow::on_search_activate));

    // кнопка для показа нумерации (справа от поиска)
    m_btn_show_numbers.set_tooltip_text("Show numbered lines in a separate window");
    m_btn_show_numbers.set_margin_start(6);
    m_btn_show_numbers.signal_clicked().connect(sigc::mem_fun(*this, &EditorWindow::on_show_numbers_clicked));

    m_header_bar.pack_end(m_btn_show_numbers);
    m_header_bar.pack_end(m_search);

    set_titlebar(m_header_bar);

    // Root container
    m_root.set_orientation(Gtk::Orientation::VERTICAL);
    set_child(m_root);

    // --- Карточка управления (Frame) ---
    auto file_card = Gtk::Frame();
    file_card.set_margin_top(10);
    file_card.set_margin_bottom(5);
    file_card.set_margin_start(10);
    file_card.set_margin_end(10);
    m_root.append(file_card);

    auto file_box = Gtk::Box(Gtk::Orientation::HORIZONTAL, 12);
    file_box.set_margin_top(8);
    file_box.set_margin_bottom(8);
    file_box.set_margin_start(8);
    file_box.set_margin_end(8);
    file_card.set_child(file_box);

    m_file_entry.set_placeholder_text("Path to .bin or .txt file (full path)");
    m_file_entry.set_hexpand(true);
    file_box.append(m_file_entry);

    // --- Кнопки  ---
    m_btn_load_bin.set_label("📂 Load Binary");
    m_btn_save_bin.set_label("💾 Save Binary");
    m_btn_load_txt.set_label("📄 Load Text");
    m_btn_save_txt.set_label("✏️ Save Text");

    // CSS классы сохранены, чтобы системная тема их подхватила
    m_btn_load_bin.get_style_context()->add_class("suggested-action");
    m_btn_save_bin.get_style_context()->add_class("secondary");
    m_btn_load_txt.get_style_context()->add_class("secondary");
    m_btn_save_txt.get_style_context()->add_class("secondary");

    m_btn_load_bin.set_tooltip_text("Load .bin tree file into the editor");
    m_btn_save_bin.set_tooltip_text("Serialize current text into .bin");
    m_btn_load_txt.set_tooltip_text("Load plain text into editor");
    m_btn_save_txt.set_tooltip_text("Save editor text to a plain file");

    file_box.append(m_btn_load_bin);
    file_box.append(m_btn_save_bin);
    file_box.append(m_btn_load_txt);
    file_box.append(m_btn_save_txt);

    // --- Карточка текста (Frame) ---
    auto text_card = Gtk::Frame();
    text_card.set_margin_top(5);
    text_card.set_margin_bottom(10);
    text_card.set_margin_start(10);
    text_card.set_margin_end(10);

    // VEXPAND: Решение проблемы "маленького поля для текста"
    text_card.set_vexpand(true);
    text_card.set_hexpand(true);

    m_root.append(text_card);

    m_scrolled.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_scrolled.set_vexpand(true); // Скролл тоже должен расширяться
    
    text_card.set_child(m_scrolled);
    
    m_textview.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    m_textview.set_left_margin(5); 
    m_textview.set_right_margin(5);
    
    m_scrolled.set_child(m_textview);

    // --- Статус бар ---
    auto status_box = Gtk::Box(Gtk::Orientation::HORIZONTAL, 8);
    status_box.set_margin_top(6);
    status_box.set_margin_bottom(6);
    status_box.set_margin_start(8);
    status_box.set_margin_end(8);

    Gtk::Label status_icon;
    status_icon.set_text("💾");
    status_box.append(status_icon);

    m_status.set_text("Ready");
    status_box.append(m_status);
    m_root.append(status_box);

    // Signals (НЕ ИЗМЕНЯЛИСЬ)
    m_btn_load_bin.signal_clicked().connect(sigc::mem_fun(*this, &EditorWindow::on_load_binary));
    m_btn_save_bin.signal_clicked().connect(sigc::mem_fun(*this, &EditorWindow::on_save_binary));
    m_btn_load_txt.signal_clicked().connect(sigc::mem_fun(*this, &EditorWindow::on_load_text));
    m_btn_save_txt.signal_clicked().connect(sigc::mem_fun(*this, &EditorWindow::on_save_text));

    m_file_entry.signal_changed().connect(sigc::mem_fun(*this, &EditorWindow::on_path_entry_changed));
    on_path_entry_changed(); 

    if (auto buf = m_textview.get_buffer()) {
        buf->signal_changed().connect(sigc::mem_fun(*this, &EditorWindow::on_textbuffer_changed));
    }

    m_file_entry.signal_activate().connect(sigc::mem_fun(*this, &EditorWindow::on_file_entry_activate));
    
    set_default_size(950, 700);

    present();
}

EditorWindow::~EditorWindow()=default;


void EditorWindow::set_status(const std::string& s) {
    m_status.set_text(s);
}


void EditorWindow::on_path_entry_changed() {
    auto path = m_file_entry.get_text();
    bool ok = !path.empty();
    m_btn_load_bin.set_sensitive(ok);
    m_btn_save_bin.set_sensitive(ok);
    m_btn_load_txt.set_sensitive(ok);
    m_btn_save_txt.set_sensitive(ok);
}

void EditorWindow::on_textbuffer_changed() {
    if (m_syncing) return; // защита от рекурсивных изменений

    auto buf = m_textview.get_buffer();
    if (!buf) return;

    // Получаем новый текст (UTF-8 bytes) как std::string
    Glib::ustring gtxt = buf->get_text();
    auto new_text = static_cast<std::string>(gtxt);

    // Если первый раз (m_last_text пуст) — инициализируем Tree
    if (m_last_text.empty() && m_tree.getRoot() == nullptr) {
        m_syncing = true;
        m_tree.clear();
        if (!new_text.empty()) m_tree.fromText(new_text.c_str(), static_cast<int>(new_text.size()));
        m_last_text = new_text;
        m_syncing = false;
    }

    // Быстрая проверка: если строки равны — ничего не делаем
    if (new_text == m_last_text) {
        // но обновим статус (символы/слова/строка)
        size_t chars = new_text.size();
        size_t words = count_words(new_text);
        auto iter = buf->get_insert()->get_iter();
        int cur_line = iter.get_line() + 1;
        std::ostringstream oss;
        oss << "Chars: " << chars << "  Words: " << words << "  Line: " << cur_line;
        set_status(oss.str());
        return;
    }

    // Вычисляем минимальный префикс L
    auto old_len = static_cast<int>(m_last_text.size());
    auto new_len = static_cast<int>(new_text.size());
    int L = 0;
    int min_len = (old_len < new_len) ? old_len : new_len;
    while (L < min_len && m_last_text[L] == new_text[L]) ++L;

    // Вычисляем минимальный суффикс R (но не переходящий через L)
    int R = 0;
    while (R < (old_len - L) && R < (new_len - L)
           && m_last_text[old_len - 1 - R] == new_text[new_len - 1 - R]) {
        ++R;
    }

    // Теперь исходное удаление и вставка
    int delLen = old_len - L - R;        // может быть 0
    int insLen = new_len - L - R;        // может быть 0

    // Применяем изменения в дереве. Отключаем синхронизацию, т.к. изменения буфера уже применены пользователем.
    // Мы изменяем внутреннюю структуру m_tree в соответствии с новым буфером.
    if (delLen > 0) {
        m_tree.erase(L, delLen);
        ++m_edit_ops_count;
    }
    if (insLen > 0) {
        const char* insBuf = new_text.data() + L; // байтовый указатель
        m_tree.insert(L, insBuf, insLen);
        ++m_edit_ops_count;
    }

    // Обновляем snapshot (после успешных операций)
    m_last_text.swap(new_text); // быстро swap

    // Обновляем статус (символы/слова/строка)
    size_t chars = m_last_text.size();
    size_t words = count_words(m_last_text);
    auto iter = buf->get_insert()->get_iter();
    int cur_line = iter.get_line() + 1;
    std::ostringstream oss;
    oss << "Chars: " << chars << "  Words: " << words << "  Line: " << cur_line;
    set_status(oss.str());
}


void EditorWindow::on_file_entry_activate() {
    if (m_btn_load_bin.get_sensitive()) on_load_binary();
}

// --- Логика файлов ---
void EditorWindow::on_load_binary() {
    std::string path = m_file_entry.get_text();
    if (path.empty()) { set_status("Provide path..."); return; }
    try {
        BinaryTreeFile bf;
        if (!bf.openFile(path.c_str())) { set_status("Cannot open binary: " + path); return; }
        //  Инициализация дерева
        m_tree.clear();        
        bf.loadTree(m_tree);

        m_last_text = m_tree.toText();

        // Обновление буфера TextView
        m_textview.get_buffer()->set_text(m_last_text);

        bf.close();
        set_status("Loaded binary: " + path);
    } catch (const std::ios_base::failure& e) {
        set_status(std::string("File I/O error: ") + e.what());
    } catch (const std::invalid_argument& e) {
        set_status(std::string("Invalid argument: ") + e.what());
    } catch (const std::bad_alloc&) {  // Убрали параметр 'e' так как он не используется
        set_status("Memory allocation failed");
    }
}

void EditorWindow::on_save_binary() {
    std::string path = m_file_entry.get_text();
    if (path.empty()) { set_status("Provide path..."); return; }

    try {
        BinaryTreeFile bf;
        if (!bf.openFile(path.c_str())) { set_status("Err open: " + path); return; }
        bf.saveTree(m_tree);
        bf.close();
        set_status("Saved binary: " + path);
    } catch (const std::ios_base::failure& e) {
        set_status(std::string("File I/O error: ") + e.what());
    } catch (const std::bad_alloc&) {
        set_status("Memory allocation failed");
    }
}


void EditorWindow::on_load_text() {
    std::string path = m_file_entry.get_text();
    if (path.empty()) { set_status("Provide path..."); return; }
    try {
        // Чтение файла целиком в std::string
        std::ifstream in(path, std::ios::binary);
        if (!in) { set_status("Err open txt: " + path); return; }
        std::string file_text((std::istreambuf_iterator<char>(in)), {});

        //  Инициализация дерева
        m_syncing = true;
        m_tree.clear();
        if (!file_text.empty()) {
            m_tree.fromText(file_text.c_str(), static_cast<int>(file_text.size()));
        }
        m_last_text = file_text;

        // Обновление буфера TextView
        m_textview.get_buffer()->set_text(file_text);
        m_syncing = false;

        set_status("Loaded txt: " + path);
    } catch (const std::ios_base::failure& e) {
        set_status(std::string("File I/O error: ") + e.what());
    } catch (const std::bad_alloc&) {
        set_status("Memory allocation failed");
    }
}

void EditorWindow::on_save_text() {
    std::string path = m_file_entry.get_text();
    if (path.empty()) { set_status("Provide path..."); return; }

    try {
        std::ofstream out(path, std::ios::binary);
        if (!out) { set_status("Err write txt: " + path); return; }

        if (!m_tree.getRoot()) {
            // пустое дерево → создаём пустой файл
            out.close();
            set_status("Saved txt (empty): " + path);
            return;
        }

        int total_len = m_tree.getRoot()->getLength();
        int chunk_size = 4096; // можно регулировать размер буфера

        for (int offset = 0; offset < total_len; offset += chunk_size) {
            int len = std::min(chunk_size, total_len - offset);
            char* buf = m_tree.getTextRange(offset, len);
            out.write(buf, len);
            delete[] buf;//NOSONAR  // освобождаем память
        }

        set_status("Saved txt: " + path);

    } catch (const std::ios_base::failure& e) {
        set_status(std::string("File I/O error: ") + e.what());
    } catch (const std::bad_alloc&) {
        set_status("Memory allocation failed");
    }
}


// --- Поиск и навигация  ---
void EditorWindow::on_search_activate() {
    auto queryStr = static_cast<std::string>(m_search.get_text());
    if (queryStr.empty()) {
        set_status("Search: empty");
        return;
    }

    // --- Поиск по номеру строки (1-based) ---
    bool is_number = true;
    for (char c : queryStr) {
        if (!std::isdigit(static_cast<unsigned char>(c))) { 
            is_number = false; 
            break; 
        }
    }

    if (is_number) {
        try {
            long val = std::stol(queryStr);
            if (val <= 0) {
                set_status("Line numbers are 1-based (enter >= 1)");
                return;
            }
            go_to_line_index(static_cast<int>(val - 1)); // 0-based
        } catch (const std::invalid_argument&) {
            set_status("Invalid line number format");
        } catch (const std::out_of_range&) {
            set_status("Line number is too large");
        }
        return;
    }

    // --- Текстовый поиск через дерево (возвращает номер строки) ---
    auto buf = m_textview.get_buffer();
    if (!buf) { set_status("No buffer"); return; }

    const char* pattern = queryStr.c_str();
    auto patternLen = static_cast<int>(queryStr.size());

    // Ищем номер строки (0-based), где начинается совпадение
    int lineNumber = m_tree.findSubstringLine(pattern, patternLen);
    if (lineNumber == -1) {
        set_status("Not found: \"" + queryStr + "\"");
        return;
    }

    // Получаем строку из дерева (char* — нужно delete[])
    char* lineBuf = m_tree.getLine(lineNumber);
    if (!lineBuf) {
        set_status("Found line but failed to get its text");
        return;
    }
    std::string lineStr(lineBuf);
    delete[] lineBuf;//NOSONAR // освобождаем, как требует контракт getLine

    // Локально ищем шаблон внутри строки (байтовый поиск корректен для UTF-8 точного совпадения)
    std::size_t localBytePos = lineStr.find(queryStr);
    if (localBytePos == std::string::npos) {
        // Технически это маловероятно, но на всякий случай — сообщаем об ошибке.
        set_status("Found line, but substring not found inside it (unexpected)");
        return;
    }

    // Преобразуем байтовый оффсет внутри строки в количество UTF-8 символов (codepoints)
    // g_utf8_strlen подсчитывает кодовые точки в первых `localBytePos` байтах.
    auto charOffsetInLine = g_utf8_strlen(lineStr.c_str(), static_cast<int>(localBytePos));

    // Длина шаблона в символах (для корректного выделения)
    auto patternCharLen = g_utf8_strlen(pattern, patternLen);

    // Получим итератор на начало строки в буфере
    Gtk::TextBuffer::iterator it_line_start = buf->get_iter_at_line(lineNumber);

    // Продвинем итератор на charOffsetInLine символов (используем цикл, чтобы быть совместимым)
    Gtk::TextBuffer::iterator it_start = it_line_start;
    for (int k = 0; k < charOffsetInLine; ++k) {
        if (!it_start) break;
        it_start.forward_char();
    }

    // Создаём it_end и продвигаем на длину шаблона (в символах)
    Gtk::TextBuffer::iterator it_end = it_start;
    for (int k = 0; k < patternCharLen; ++k) {
        if (!it_end) break;
        it_end.forward_char();
    }

    // Выделяем и скроллим
    buf->select_range(it_start, it_end);
    m_textview.scroll_to(it_start, 0.0);

    // Показываем строку в статусе (1-based)
    set_status("Found at line " + std::to_string(lineNumber + 1));
}



void EditorWindow::go_to_line_index(int lineIndex0Based) {
    auto buf = m_textview.get_buffer();
    if (!buf) { set_status("No buffer"); return; }

    int total_lines = buf->get_line_count();
    if (lineIndex0Based < 0 || lineIndex0Based >= total_lines) {
        std::ostringstream oss;
        oss << "Line out of range (1.." << total_lines << ")";
        set_status(oss.str());
        return;
    }

    // Получаем итераторы начала и конца строки
    Gtk::TextBuffer::iterator it_start = buf->get_iter_at_line(lineIndex0Based);
    Gtk::TextBuffer::iterator it_end;
    if (lineIndex0Based + 1 < total_lines) it_end = buf->get_iter_at_line(lineIndex0Based + 1);
    else it_end = buf->end();

    // Выделяем диапазон и скроллим
    buf->select_range(it_start, it_end);
    m_textview.scroll_to(it_start, 0.0);

    // Дополнительно: получим строку из Tree и покажем в статусе (проверка твоей логики)
    try {
        char* line = m_tree.getLine(lineIndex0Based);
        if (line) {
            set_status("Line " + std::to_string(lineIndex0Based + 1) + ": " + std::string(line));
            delete[] line; //NOSONAR
        } else {
            set_status("Line " + std::to_string(lineIndex0Based + 1) + " (no data in tree)");
        }
    } catch (const std::out_of_range& e) {
        set_status(std::string("Line index out of range: ") + e.what());
    } catch (const std::bad_alloc&) {
        set_status("Memory allocation failed while getting line");
    }
}



// Показать окно с нумерацией строк (readonly)
void EditorWindow::on_show_numbers_clicked() {
    if (!m_tree.getRoot()) { set_status("Tree empty"); return; }

    std::ostringstream numbered;
    size_t total_lines = m_tree.getTotalLineCount();
    for (size_t i = 0; i < total_lines; ++i) {
        int start = m_tree.getOffsetForLine(static_cast<int>(i));
        int end   = (i + 1 < total_lines) ? m_tree.getOffsetForLine(static_cast<int>(i) + 1)
                                          : m_tree.getRoot()->getLength();
        char* lineBuf = m_tree.getTextRange(start, end - start);
        numbered << (i + 1) << ": " << std::string(lineBuf, end - start);
        delete[] lineBuf; //NOSONAR
    }

    // Создаём модальное окно с read-only TextView
    auto win = new Gtk::Window(); //NOSONAR
    win->set_default_size(600, 400);
    win->set_modal(true);
    win->set_transient_for(*this);
    win->set_title("Numbered lines");

    auto sc = Gtk::make_managed<Gtk::ScrolledWindow>();
    sc->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);

    auto tv = Gtk::make_managed<Gtk::TextView>();
    tv->set_editable(false);
    tv->set_wrap_mode(Gtk::WrapMode::NONE);
    tv->get_style_context()->add_class("monospace"); 
    sc->set_child(*tv);
    win->set_child(*sc);

    tv->get_buffer()->set_text(numbered.str());

    win->signal_hide().connect([win]() { delete win; }); //NOSONAR
    win->present();
}
