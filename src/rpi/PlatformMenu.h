#ifndef _PLATFORM_MENU_H_
#define _PLATFORM_MENU_H_

#include "PlatformCommon.h"
#include "PlatformScreen.h"

// FatFsのためのヘッダ
#include <fatfs/ff.h>

class FBConsole;
class EmuController;
class PlatformConfig;

// Constants
#define MAX_MENU_ITEMS 256      // Maximum number of menu items
#define MAX_MENU_DEPTH 8        // Maximum menu depth
#define MAX_STRING_LENGTH 256   // Maximum length of strings
#define MAX_FILE_ENTRIES 64    // ファイルブラウザがキャッシュする最大エントリ数

// キーリピート設定
#define KEY_REPEAT_DELAY 500    // 最初のリピートまでの遅延（ミリ秒）
#define KEY_REPEAT_INTERVAL 100 // 2回目以降のリピート間隔（ミリ秒）

// Menu item types
#define MENU_TYPE_SEPARATOR 0
#define MENU_TYPE_SUBMENU   1
#define MENU_TYPE_BOOL      2
#define MENU_TYPE_FILE      3
#define MENU_TYPE_ACTION    4
#define MENU_TYPE_INT       5
#define MENU_TYPE_INT_SELECT 6

// Key codes for menu navigation
#define MENU_KEY_UP      0x26
#define MENU_KEY_DOWN    0x28
#define MENU_KEY_LEFT    0x25
#define MENU_KEY_RIGHT   0x27
#define MENU_KEY_ENTER   0x0D
#define MENU_KEY_ESC     0x1B
#define MENU_KEY_F12     0x7B

// Menu colors
#define MENU_COLOR_BACKGROUND 0x0000  // Black
#define MENU_COLOR_TEXT       0xFFFF  // White
#define MENU_COLOR_SELECTED   0x001F  // Blue
#define MENU_COLOR_HEADER     0xF800  // Red
#define MENU_COLOR_VALUE      0x07E0  // Green

// Menu item structure
typedef struct {
    char display_name[MAX_STRING_LENGTH];  // Display name
    char original_path[MAX_STRING_LENGTH]; // Original full path (e.g. "Drive/FD0/Insert")
    int type;                             // Item type (bool, file, action, etc.)
    char action[MAX_STRING_LENGTH];       // Action string
    char config_name[MAX_STRING_LENGTH];  // Config name
    int parent_index;                     // Parent menu item index
    int first_child_index;                // First child menu item index
    int num_children;                     // Number of child menu items
} MenuItemInfo;

// ファイルブラウザの状態を保持する構造体
typedef struct {
    FILINFO entries[MAX_FILE_ENTRIES]; // ファイルエントリのキャッシュ
    int entry_count;                   // 合計エントリ数
    int current_page;                  // 現在表示しているページ
    int selected_index;                // 選択されているファイルのインデックス
    int page_count;                    // 合計ページ数
    int cache_start_index;             // キャッシュの先頭インデックス
    int total_entries;                 // ディレクトリ内の合計エントリ数（キャッシュされていないものも含む）
    bool has_parent_dir;               // 親ディレクトリへの項目があるかどうか
    char current_dir[MAX_STRING_LENGTH]; // 現在のディレクトリパス
    char initial_root_dir[MAX_STRING_LENGTH]; // 初期ルートディレクトリ（これより上には行けない）
    bool browser_active;               // ブラウザがアクティブかどうか
    int target_menu_index;             // 元のメニュー項目のインデックス
} FileBrowserState;

// Platform Menu class
class PlatformMenu {
private:
    MenuItemInfo m_menu_items[MAX_MENU_ITEMS];  // Menu items array
    int m_num_items;                           // Total number of menu items
    
    int m_current_menu_path[MAX_MENU_DEPTH];  // Current menu path (indexes)
    int m_current_depth;                      // Current menu depth
    int m_selected_index;                     // Currently selected item index

    int m_files_per_page;                     // 1ページあたりの表示ファイル数
    
    bool m_visible;                           // Is menu visible
    bool m_capturing_keys;                    // Is menu capturing key input
    
    // キーリピート用の変数
    int m_key_pressed;                        // 現在押されているキー
    uint64_t m_key_repeat_timer;              // キーリピートタイマー
    bool m_is_repeating;                      // リピート中かどうか
    
    PlatformScreen* m_screen;
    FBConsole* m_fb_console;                  // Frame buffer console for drawing

    EmuController* m_emulator;
    
    // ファイルブラウザ関連
    FileBrowserState m_file_browser;          // ファイルブラウザの状態

    PlatformConfig* m_config;
    
    // Internal methods
    void draw_menu();
    void handle_key_input(int key);
    void handle_selection();
    bool load_menu_file(const char* filename);
    void parse_menu_item(const char* line);
    bool handle_action(const char* action);
    void toggle_bool_value(int item_index);
    void set_int_select_value(int item_index);
    void handle_file_selection(int item_index);
    bool get_config_bool(const char* config_name);
    int get_config_int(const char* config_name, int* config_value, bool* is_same);
    const char* get_config_string(const char* config_name);
    void apply_config_bool(const char* config_name, bool value);
    void apply_config_int(const char* config_name, int value);
    void apply_config_string(const char* config_name, const char* value);
    
    // ファイルブラウザ関連の関数
    bool init_file_browser(const char* path, int menu_index);
    void draw_file_browser();
    void handle_file_browser_key(int key);
    bool load_file_entries(const char* path);
    bool update_file_cache(int new_index);
    void select_file_entry(int index);
    void save_disk_changes();
    void save_tape_changes();
    void adjust_cursor_position();
    
public:
    PlatformMenu(EmuController* emulator);
    ~PlatformMenu();
    
    bool initialize(EmuController* emulator, PlatformScreen* screen);
    void toggle_menu();
    bool is_visible() { return m_visible; }
    bool is_capturing_keys() { return m_capturing_keys; }
    void process_key(int key, bool down);
    void draw();
    int update();

    void update_draw_buffer();
};

#endif // _PLATFORM_MENU_H_ 