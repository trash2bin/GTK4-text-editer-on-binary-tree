#include "EditorWindow.h"
#include "CustomTextView.h"
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
    
    // Используем наш кастомный виджет вместо Gtk::TextView
    m_custom_view.set_can_focus(true);
    m_scrolled.set_child(m_custom_view);

    // Привязываем дерево к кастомному виду
    m_custom_view.set_tree(&m_tree);

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

    m_file_entry.signal_activate().connect(sigc::mem_fun(*this, &EditorWindow::on_file_entry_activate));
    
    set_default_size(950, 700);

    present();

    m_custom_view.grab_focus();

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

        // Обновление представления из дерева
        m_custom_view.reload_from_tree();

        // Обновляем snapshot m_last_text safely
        {
            char* raw = m_tree.toText();
            if (raw) { m_last_text.assign(raw); delete[] raw; }
            else m_last_text.clear();
        }

        // установить фокус на наш виджет (необязательно)
        m_custom_view.grab_focus();

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
        m_custom_view.reload_from_tree();
        m_custom_view.grab_focus();

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
    delete[] lineBuf; // NOSONAR

    // Найдём байтовую позицию совпадения внутри строки (байтовый поиск корректен для UTF-8)
    std::size_t localBytePos = lineStr.find(queryStr);
    if (localBytePos == std::string::npos) {
        set_status("Found line, but substring not found inside it (unexpected)");
        return;
    }

    // Устанавливаем курсор в CustomTextView на позицию начала совпадения
    int lineStart = m_tree.getOffsetForLine(lineNumber);
    int bytePos = lineStart + static_cast<int>(localBytePos);
    m_custom_view.set_cursor_byte_offset(bytePos);
    m_custom_view.select_range_bytes(bytePos, patternLen);
    m_custom_view.scroll_to_byte_offset(bytePos);
    m_custom_view.grab_focus(); // не обязательно, но удобно
    
    // Прокрутка: установим вертикальную позицию ScrolledWindow по номеру строки
    if (auto vadj = m_scrolled.get_vadjustment()) {
        int y = lineNumber * m_custom_view.get_line_height_for_ui();
        auto maxv = static_cast<int>(vadj->get_upper() - vadj->get_page_size());
        if (y < 0) y = 0;
        if (y > maxv) y = maxv;
        vadj->set_value(y);
    }

    // Показываем строку в статусе (1-based)
    set_status("Found at line " + std::to_string(lineNumber + 1));
}


void EditorWindow::go_to_line_index(int lineIndex0Based) {
    // Проверяем диапазон на стороне дерева
    auto total_lines = static_cast<int>(m_tree.getTotalLineCount());
    if (lineIndex0Based < 0 || lineIndex0Based >= total_lines) {
        std::ostringstream oss;
        oss << "Line out of range (1.." << total_lines << ")";
        set_status(oss.str());
        return;
    }

    // Получаем байтовый оффсет начала строки в дереве и ставим курсор
    int lineStart = m_tree.getOffsetForLine(lineIndex0Based);
    m_custom_view.set_cursor_byte_offset(lineStart);

    // Скроллим ScrolledWindow к нужной строке
    if (auto vadj = m_scrolled.get_vadjustment()) {
        int y = lineIndex0Based * m_custom_view.get_line_height_for_ui();
        auto maxv = static_cast<int>(vadj->get_upper() - vadj->get_page_size());
        if (y < 0) y = 0;
        if (y > maxv) y = maxv;
        vadj->set_value(y);
    }

    // Дополнительно: покажем строку в статусе (берём текст из дерева)
    try {
        char* line = m_tree.getLine(lineIndex0Based);
        if (line) {
            // Показываем небольшую часть строки в статусе (без перевода строки)
            std::string shown(line);
            delete[] line; // NOSONAR
            set_status("Line " + std::to_string(lineIndex0Based + 1) + ": " + shown);
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
// Более эффективная версия on_show_numbers: считываем весь текст один раз
void EditorWindow::on_show_numbers_clicked() {
    if (!m_tree.getRoot()) { set_status("Tree empty"); return; }

    size_t total_lines = m_tree.getTotalLineCount();
    if (total_lines == 0) {
        set_status("Tree has no lines");
        return;
    }

    // Получаем весь текст один раз (getTextRange на весь диапазон)
    int total_len = m_tree.getRoot()->getLength();
    char* all = m_tree.getTextRange(0, total_len); // владелец — мы
    if (!all) { set_status("Failed to extract text from tree"); return; }

    std::ostringstream numbered;

    for (size_t i = 0; i < total_lines; ++i) {
        int start = m_tree.getOffsetForLine(static_cast<int>(i));
        int end   = (i + 1 < total_lines) ? m_tree.getOffsetForLine(static_cast<int>(i) + 1) : total_len;
        int len = end - start;
        // безопасно создаём строку из байт (не предполагаем \0)
        numbered << (i + 1) << ": " << std::string(all + start, static_cast<size_t>(len));
    }
    delete[] all; //NOSONAR

    // окно — как раньше
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
