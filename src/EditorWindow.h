#ifndef EDITORWINDOW_H
#define EDITORWINDOW_H

#include <gtkmm.h>
#include <string>
#include "Tree.h"

// Вспомогательная функция для подсчета слов, объявленная здесь, 
// но определенная в .cpp для инкапсуляции.
static size_t count_words(const std::string& s);

// глубокая иерархия унаследована от GTK
class EditorWindow : public Gtk::ApplicationWindow { // NOSONAR cpp:S110
public:
    EditorWindow();
    ~EditorWindow() override;

protected:
    // Вспомогательные методы
    void apply_system_theme();
    void set_status(const std::string& s);

    // Обработчики сигналов
    void on_path_entry_changed();
    void on_textbuffer_changed();
    void on_file_entry_activate();
    
    // Логика файлов/дерева
    void on_load_binary();
    void on_save_binary();
    void on_load_text();
    void on_save_text();

    // Поиск и навигация
    void on_search_activate();
    void on_show_numbers_clicked();
    void go_to_line_index(int lineIndex0Based);

private:
    // синхронизация с Tree
    Tree m_tree;
    std::string m_last_text;      // байтовая копия текста (UTF-8 bytes)
    bool m_syncing = false;       // если true — игнорировать изменения буфера (программные обновления)
    int m_edit_ops_count = 0;     // счетчик операций (для ребаланса)


    // Элементы пользовательского интерфейса
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
};

#endif // EDITORWINDOW_H