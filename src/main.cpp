#include <gtkmm.h>
#include "Tree.h"
#include "BinaryTreeFile.h"
#include <fstream>
#include <glib.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <sstream>

// ----------------------- Theme control -----------------------
enum class ThemeMode {
    LOCAL,      // не трогаем окружение
    ADWAITA,    // GTK_THEME=Adwaita
    IGNORE_USER // игнорируем ~/.config (подменяем XDG_CONFIG_HOME)
};

class ThemeGuard {
public:
    ThemeGuard(ThemeMode mode) : mode_(mode) {
        const char* curXdg = g_getenv("XDG_CONFIG_HOME");
        oldXdg_ = curXdg ? std::string(curXdg) : std::string();

        const char* curTheme = g_getenv("GTK_THEME");
        oldTheme_ = curTheme ? std::string(curTheme) : std::string();

        if (mode_ == ThemeMode::IGNORE_USER) {
            char tmpl[] = "/tmp/bt_editor_cfgXXXXXX";
            char* tmp = mkdtemp(tmpl);
            if (tmp) {
                tmpdir_ = std::string(tmp);
                g_setenv("XDG_CONFIG_HOME", tmpdir_.c_str(), TRUE);
            }
        } else if (mode_ == ThemeMode::ADWAITA) {
            g_setenv("GTK_THEME", "Adwaita", TRUE);
        }
    }

    ~ThemeGuard() {
        if (!oldXdg_.empty()) g_setenv("XDG_CONFIG_HOME", oldXdg_.c_str(), TRUE);
        else g_unsetenv("XDG_CONFIG_HOME");

        if (!oldTheme_.empty()) g_setenv("GTK_THEME", oldTheme_.c_str(), TRUE);
        else g_unsetenv("GTK_THEME");

        if (!tmpdir_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(tmpdir_, ec);
        }
    }

private:
    ThemeMode mode_;
    std::string oldXdg_;
    std::string oldTheme_;
    std::string tmpdir_;
};

static size_t count_words(const std::string& s) {
    std::istringstream iss(s);
    size_t cnt = 0;
    std::string w;
    while (iss >> w) ++cnt;
    return cnt;
}

// ----------------------- GUI -----------------------
class EditorWindow : public Gtk::ApplicationWindow {
public:
    EditorWindow() {
        
        // --- ВЕРНУЛ: Применение системной темы ---
        // Это важно для подтягивания ваших кастомных CSS из GNOME
        set_decorated(true);

        apply_system_theme();

        // --- HeaderBar: make it look GNOME-like ---
        m_header_bar.set_show_title_buttons(true);

        m_header_bar.get_style_context()->add_class("titlebar");
        m_header_bar.get_style_context()->add_class("flat"); // Убирает разделительную линию
        m_header_bar.get_style_context()->add_class("background"); // Гарантирует правильный фон

        // right area: search entry (compact) — используем член класса m_search
        m_search.set_hexpand(false);
        m_search.set_placeholder_text("Line number or text...");
        m_search.signal_activate().connect(sigc::mem_fun(*this, &EditorWindow::on_search_activate));

        // кнопка для показа нумерации (справа от поиска)
        m_btn_show_numbers.set_tooltip_text("Show numbered lines in a separate window");
        m_btn_show_numbers.set_margin_start(6);
        m_btn_show_numbers.signal_clicked().connect(sigc::mem_fun(*this, &EditorWindow::on_show_numbers_clicked));

        // Пакуем сначала кнопку, затем поле поиска (или в нужном порядке)
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

        // Signals
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

    virtual ~EditorWindow() {}

protected:
    // Это включает поддержку системных настроек темы
    void apply_system_theme() {
        auto settings = Gtk::Settings::get_default();
        if (settings) {
            settings->property_gtk_application_prefer_dark_theme() = true;
        }
    }

    void set_status(const std::string& s) {
        m_status.set_text(s);
    }

    void on_path_entry_changed() {
        auto path = m_file_entry.get_text();
        bool ok = !path.empty();
        m_btn_load_bin.set_sensitive(ok);
        m_btn_save_bin.set_sensitive(ok);
        m_btn_load_txt.set_sensitive(ok);
        m_btn_save_txt.set_sensitive(ok);
    }

    void on_textbuffer_changed() {
        auto buf = m_textview.get_buffer();
        if (!buf) return;
        Glib::ustring t = buf->get_text();
        std::string s = static_cast<std::string>(t);
        size_t chars = s.size();
        size_t words = count_words(s);
        auto iter = buf->get_insert()->get_iter();
        int cur_line = iter.get_line() + 1; // get_line() вернёт 0-based
        std::ostringstream oss;
        oss << "Chars: " << chars << "  Words: " << words << "  Line: " << cur_line;
        set_status(oss.str());
    }

    void on_file_entry_activate() {
        if (m_btn_load_bin.get_sensitive()) on_load_binary();
    }

    // --- Логика файлов (без изменений) ---
    void on_load_binary() {
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
            } else {
                // Проверяем валидность UTF-8; если невалидно — исправляем
                if (!g_utf8_validate(txt, -1, nullptr)) {
                    // g_utf8_make_valid вернёт новый буфер с заменами некорректных байт на U+FFFD
                    gchar* fixed = g_utf8_make_valid(txt, -1);
                    m_textview.get_buffer()->set_text(fixed);
                    g_free(fixed);
                } else {
                    m_textview.get_buffer()->set_text(txt);
                }
                delete[] txt;
            }
            bf.close();
            set_status("Loaded binary: " + path);
        } catch (const std::exception& e) {
            set_status(std::string("Error: ") + e.what());
        }
    }

    void on_save_binary() {
        std::string path = m_file_entry.get_text();
        if (path.empty()) { set_status("Provide path..."); return; }
        try {
            Glib::ustring text = m_textview.get_buffer()->get_text();
            Tree t;
            t.fromText(text.c_str(), text.bytes());
            BinaryTreeFile bf;
            if (!bf.openFile(path.c_str())) { set_status("Err open: " + path); return; }
            bf.saveTree(t);
            bf.close();
            set_status("Saved binary: " + path);
        } catch (const std::exception& e) {
            set_status(std::string("Error: ") + e.what());
        }
    }

    void on_load_text() {
        std::string path = m_file_entry.get_text();
        if (path.empty()) { set_status("Provide path..."); return; }
        try {
            std::ifstream in(path, std::ios::binary);
            if (!in) { set_status("Err open txt: " + path); return; }
            std::string s((std::istreambuf_iterator<char>(in)), {});
            m_textview.get_buffer()->set_text(s);
            set_status("Loaded txt: " + path);
        } catch (const std::exception& e) {
            set_status(std::string("Error: ") + e.what());
        }
    }

    void on_save_text() {
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

private:
    // UI Elements
    Gtk::HeaderBar m_header_bar;
    
    Gtk::Box m_root{Gtk::Orientation::VERTICAL};
    
    Gtk::Entry m_file_entry;
    
    Gtk::Button m_btn_load_bin;
    Gtk::Button m_btn_save_bin;
    Gtk::Button m_btn_load_txt;
    Gtk::Button m_btn_save_txt;
    Gtk::SearchEntry m_search;                 
    Gtk::Button m_btn_show_numbers{"#️⃣ Lines"};

    Gtk::ScrolledWindow m_scrolled;
    Gtk::TextView m_textview;
    Gtk::Label m_status;

    // --- вспомогательные методы для поиска и перехода по строкам ---
    void on_search_activate();                 // вызывается при Enter в поле поиска
    void on_show_numbers_clicked();            // показать нумерацию в отдельном окне
    void go_to_line_index(int lineIndex0Based);// перейти к строке (0-based)
};

// Обработчик Enter в поле поиска
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
        Glib::ustring gtxt = buf->get_text();
        Tree t;
        t.fromText(gtxt.c_str(), static_cast<int>(gtxt.bytes())); // bytes() — байты UTF-8
        char* line = t.getLine(lineIndex0Based);
        if (line) {
            set_status("Line " + std::to_string(lineIndex0Based + 1) + ": " + std::string(line));
            delete[] line;
        } else {
            set_status("Line " + std::to_string(lineIndex0Based + 1) + " (no data in tree)");
        }
    } catch (const std::exception& e) {
        set_status(std::string("Tree error: ") + e.what());
    }
}


// Показать окно с нумерацией строк (readonly)
void EditorWindow::on_show_numbers_clicked() {
    auto buf = m_textview.get_buffer();
    if (!buf) { set_status("No buffer"); return; }

    Glib::ustring all = buf->get_text();
    // Разбиваем по '\n' и формируем нумерованный текст
    std::string plain = static_cast<std::string>(all);
    std::ostringstream numbered;
    size_t lineno = 1;
    size_t pos = 0;
    while (pos <= plain.size()) {
        size_t next = plain.find('\n', pos);
        if (next == std::string::npos) next = plain.size();
        std::string line = plain.substr(pos, next - pos);
        numbered << lineno << ": " << line << "\n";
        lineno++;
        pos = next + 1;
    }

    // Создаём модальное окно с read-only TextView
    auto win = new Gtk::Window();          // gtkmm4: без WindowType
    win->set_default_size(600, 400);
    win->set_modal(true);
    // делаем окно транзиентным к основному — чтобы закрывать/фокус правильно работало
    win->set_transient_for(*this);
    win->set_title("Numbered lines");

    auto sc = Gtk::make_managed<Gtk::ScrolledWindow>();
    sc->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);

    auto tv = Gtk::make_managed<Gtk::TextView>();
    tv->set_editable(false);
    tv->set_wrap_mode(Gtk::WrapMode::NONE);
    tv->get_style_context()->add_class("monospace"); // чтобы ровнее выглядело
    sc->set_child(*tv);

    win->set_child(*sc);

    // Вставляем текст
    auto tbuf = tv->get_buffer();
    tbuf->set_text(numbered.str());

    // При закрытии окна — удалим его (чтобы не накапливать)
    win->signal_hide().connect([win]() {
        delete win;
    });

    win->present();
}


int main(int argc, char* argv[]) {
        // Включаем GTK Inspector (вызывается через Ctrl+Shift+D или Ctrl+Shift+I)
    // g_setenv("GTK_DEBUG", "interactive", TRUE);
    g_setenv("GDK_BACKEND", "wayland", TRUE);
    
    ThemeGuard guard(ThemeMode::LOCAL);
    auto app = Gtk::Application::create("org.example.binarytreeeditor");
    
    // ThemeGuard guard(ThemeMode::LOCAL);
    // auto app = Gtk::Application::create("org.example.binarytreeeditor");
    return app->make_window_and_run<EditorWindow>(argc, argv);
}