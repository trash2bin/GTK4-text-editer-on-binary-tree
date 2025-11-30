#include "EditorWindow.h"
#include "BinaryTreeFile.h"
#include <fstream>
#include <glib.h>
#include <iostream>
#include <sstream>
#include <string>

// Определение вспомогательной функции (НЕ ИЗМЕНЯЛАСЬ)
static size_t count_words(const std::string& s) {
    std::istringstream iss(s);
    size_t cnt = 0;
    std::string w;
    while (iss >> w) ++cnt;
    return cnt;
}

// ----------------------- Реализация EditorWindow -----------------------

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

    // --- Кнопки (ваши стили) ---
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

// ЛОГИКА ОСТАЕТСЯ ТА ЖЕ
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
    std::string new_text = static_cast<std::string>(gtxt);

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
    int old_len = static_cast<int>(m_last_text.size());
    int new_len = static_cast<int>(new_text.size());
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

    // Иногда ребалансить (экономично)
    if (m_edit_ops_count >= REBALANCE_THRESHOLD) {
        m_tree.rebalance();
        m_edit_ops_count = 0;
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
        Tree t;
        bf.loadTree(t);
        char* txt = t.toText();
        if (!txt) {
            m_textview.get_buffer()->set_text("");
            m_tree.clear();
            m_last_text.clear();
        } else {
            // Ветвь проверки UTF-8 как у тебя
            std::string str;
            if (!g_utf8_validate(txt, -1, nullptr)) {
                gchar* fixed = g_utf8_make_valid(txt, -1);
                str = std::string(fixed);
                g_free(fixed);
            } else {
                str = std::string(txt);
            }
            delete[] txt; // NOSONAR

            // Обновляем Tree и локальный snapshot в атомарной манере
            m_syncing = true;
            m_tree.clear();
            m_tree.fromText(str.c_str(), static_cast<int>(str.size()));
            m_last_text = str;
            m_textview.get_buffer()->set_text(str);
            m_syncing = false;
        }
        bf.close();
        set_status("Loaded binary: " + path);
    } catch (const std::exception& e) {
        set_status(std::string("Error: ") + e.what());
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
    } catch (const std::exception& e) {
        set_status(std::string("Error: ") + e.what());
    }
}


void EditorWindow::on_load_text() {
    std::string path = m_file_entry.get_text();
    if (path.empty()) { set_status("Provide path..."); return; }
    try {
        std::ifstream in(path, std::ios::binary);
        if (!in) { set_status("Err open txt: " + path); return; }
        std::string s((std::istreambuf_iterator<char>(in)), {});
        m_syncing = true;
        m_tree.clear();
        m_tree.fromText(s.c_str(), static_cast<int>(s.size()));
        m_last_text = s;
        m_textview.get_buffer()->set_text(s);
        m_syncing = false;
        set_status("Loaded txt: " + path);
    } catch (const std::exception& e) {
        set_status(std::string("Error: ") + e.what());
    }
}

void EditorWindow::on_save_text() {
    std::string path = m_file_entry.get_text();
    if (path.empty()) { set_status("Provide path..."); return; }
    try {
        Glib::ustring text = m_textview.get_buffer()->get_text();
        std::ofstream out(path, std::ios::binary);
        if (!out) { set_status("Err write txt: " + path); return; }
        out.write(text.data(), text.bytes());
        set_status("Saved txt: " + path);
    } catch (const std::exception& e) {
        set_status(std::string("Error: ") + e.what());
    }
}

// --- Поиск и навигация  ---
void EditorWindow::on_search_activate() {
    std::string query = static_cast<std::string>(m_search.get_text());
    if (query.empty()) {
        set_status("Search: empty");
        return;
    }

    // Если query состоит только из цифр — интерпретируем как номер строки (1-based в UI)
    bool is_number = true;
    for (char c : query) if (!std::isdigit(static_cast<unsigned char>(c))) { is_number = false; break; }

    if (is_number) {
        try {
            long val = std::stol(query);
            if (val <= 0) {
                set_status("Line numbers are 1-based (enter >= 1)");
                return;
            }
            // переводим в 0-based
            go_to_line_index(static_cast<int>(val - 1));
        } catch (...) {
            set_status("Invalid line number");
        }
        return;
    }

    // ---- Текстовый поиск: используем байтовый поиск по std::string ----
    auto buf = m_textview.get_buffer();
    if (!buf) { set_status("No buffer"); return; }

    // Получаем весь текст как std::string (байты UTF-8)
    std::string plain = static_cast<std::string>(buf->get_text());

    // Находим первое вхождение (байтовый find)
    size_t pos = plain.find(query);
    if (pos == std::string::npos) {
        set_status("Not found: \"" + query + "\"");
        return;
    }

    size_t start_offset = pos;
    size_t end_offset = pos + query.size();

    // Получаем TextIters по байтовым оффсетам (get_iter_at_offset принимает int offset)
    Gtk::TextBuffer::iterator it_start = buf->get_iter_at_offset(static_cast<int>(start_offset));
    Gtk::TextBuffer::iterator it_end   = buf->get_iter_at_offset(static_cast<int>(end_offset));

    // Выделяем и скроллим
    buf->select_range(it_start, it_end);
    m_textview.scroll_to(it_start, 0.0);

    // Для статуса показываем номер строки найденного вхождения
    int line_of_match = it_start.get_line() + 1; // 1-based для UI
    set_status("Found at line " + std::to_string(line_of_match));
}

// Переход к строке (0-based). Использует TextBuffer::get_iter_at_line
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
    } catch (const std::exception& e) {
        set_status(std::string("Tree error: ") + e.what());
    }
}


// Показать окно с нумерацией строк (readonly) (НЕ ИЗМЕНЯЛАСЬ)
void EditorWindow::on_show_numbers_clicked() {
    auto buf = m_textview.get_buffer();
    if (!buf) { set_status("No buffer"); return; }

    Glib::ustring all = buf->get_text();
    // Разбиваем по '\n' и формируем нумерованный текст
    auto plain = static_cast<std::string>(all);
    std::ostringstream numbered;
    size_t lineno = 1;
    size_t pos = 0;
    
    // Process text line by line
    while (pos <= plain.size()) {
        size_t next = plain.find('\n', pos);
        if (next == std::string::npos) next = plain.size();
        std::string line = plain.substr(pos, next - pos);
        numbered << lineno << ": " << line << "\n";
        lineno++;
        
        if (next == plain.size()) break; // End of string
        pos = next + 1;
    }
    
    // Handle the case where the file is completely empty (0 lines)
    if (plain.empty()) {
        numbered << "1: \n"; 
    }


    // Создаём модальное окно с read-only TextView
    auto win = new Gtk::Window();//NOSONAR
    win->set_default_size(600, 400);
    win->set_modal(true);
    // делаем окно транзиентным к основному 
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

    // Вставляем текст
    auto tbuf = tv->get_buffer();
    tbuf->set_text(numbered.str());

    // При закрытии окна — удалим его
    win->signal_hide().connect([win]() {
        delete win; //NOSONAR
    });

    win->present();
}