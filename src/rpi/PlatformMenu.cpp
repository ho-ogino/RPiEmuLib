/*
    Platform Menu System
    This file implements an on-screen menu system that reads menu.cfg
*/

#include "PlatformCommon.h"
#ifdef USE_EXTERNAL_EMU
#include "../emu.h"
#else
#include "PlatformEmu.h"
#endif

#include "PlatformConfig.h"
#include "PlatformScreen.h"
#include "PlatformSound.h"
#include "PlatformMenu.h"
#include "PlatformConfig.h"
#include "vicesound.h"

#include "EmuController.h"
#include "PlatformMenu.h"
#include "FBConsole.hpp"

#include <circle/logger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fatfs/ff.h>
#include <circle/timer.h>

class CLogger;
extern CLogger* get_logger();

extern const char* create_emu_path(const char *tag, const char *format, ...);

// Constructor
PlatformMenu::PlatformMenu(EmuController* emulator)
:
    m_screen(nullptr)
{
    this->m_emulator = emulator;

    m_fb_console = NULL;
    m_num_items = 0;
    m_current_depth = 0;
    m_selected_index = 0;
    m_visible = false;
    m_capturing_keys = false;
    
    // キーリピート用の変数を初期化
    m_key_pressed = 0;
    m_key_repeat_timer = 0;
    m_is_repeating = false;
    
    // Initialize arrays
    memset(m_menu_items, 0, sizeof(m_menu_items));
    memset(m_current_menu_path, 0, sizeof(m_current_menu_path));
    
    // ファイルブラウザの初期化
    memset(&m_file_browser, 0, sizeof(m_file_browser));
    m_file_browser.browser_active = false;
    m_file_browser.current_page = 0;
    m_file_browser.entry_count = 0;
    m_file_browser.selected_index = 0;
    m_file_browser.page_count = 0;
    m_file_browser.target_menu_index = -1;
    m_file_browser.current_dir[0] = '\0';
    m_file_browser.cache_start_index = 0;
    m_file_browser.total_entries = 0;
    m_file_browser.has_parent_dir = false;
}

// Destructor
PlatformMenu::~PlatformMenu() 
{
    // Nothing to free
}

// Initialize the menu system
bool PlatformMenu::initialize(EmuController* emulator, PlatformScreen* screen) 
{
    if (!emulator) {
        return false;
    }
    
    this->m_emulator = emulator;
    this->m_screen = screen;
    m_fb_console = NULL;
    update_draw_buffer();

    // このタイミングで確実にconfigは拾えるはず
    this->m_config = emulator->get_platform_config();
    
    // Load menu configuration
    return load_menu_file("menu.cfg");
}

void PlatformMenu::update_draw_buffer()
{
    if(!m_screen) {
        get_logger()->Write("PlatformMenu", LogError, "update_draw_buffer: screen is NULL");
        return;
    }

    get_logger()->Write("PlatformMenu", LogNotice, "update_draw_buffer");
    if(m_fb_console) {
        get_logger()->Write("PlatformMenu", LogNotice, "update_draw_buffer: delete fb_console");
        delete m_fb_console;
        m_fb_console = NULL;
        get_logger()->Write("PlatformMenu", LogNotice, "update_draw_buffer: fb_console is NULL");
    }

    int width = m_screen->get_draw_width();
    int height = m_screen->get_draw_height();

    get_logger()->Write("PlatformMenu", LogNotice, "update_draw_buffer: new fb_console width=%d, height=%d", width, height);
    m_fb_console = new FBConsole(width, height);
    get_logger()->Write("PlatformMenu", LogNotice, "update_draw_buffer: fb_console->init");
    m_fb_console->init(m_screen->get_draw_buffer_ptr(), m_screen->get_draw_width());
    get_logger()->Write("PlatformMenu", LogNotice, "update_draw_buffer: end");

    // 上下40ドット分のヘッダーとフッターを引いた分を表示する
    m_files_per_page = (height - 40 - 40) / 16;
}

// Toggle menu visibility
void PlatformMenu::toggle_menu() 
{
    // If we're about to show the menu, load the current config
    if (!m_visible) {
        m_emulator->load_config();
    }
    
    m_visible = !m_visible;
    m_capturing_keys = m_visible;
    
    if (m_visible) {
        // Reset to top level when showing menu
        m_current_depth = 0;
        m_selected_index = 0;
        
        // 初期表示時にセパレーターを選択しないように調整
        adjust_cursor_position();
        
        // キーリピート用の変数を初期化
        m_key_pressed = 0;
        m_key_repeat_timer = 0;
        m_is_repeating = false;

        // 初回描画(メニューはキーを押した時しか描画しないため、ここで描画)
        draw_menu();
    } else {
        // When hiding menu, save the config
        m_emulator->update_config();
        m_emulator->save_config();
    }
}

// カーソル位置を調整（セパレーターの上にカーソルがある場合は移動）
void PlatformMenu::adjust_cursor_position()
{
    int current_parent = (m_current_depth > 0) ? m_current_menu_path[m_current_depth - 1] : -1;
    int items_in_level = 0;
    
    // 現在のカーソル位置にあるアイテムを特定
    int count = 0;
    int item_index = -1;
    for (int i = 0; i < m_num_items; i++) {
        if (m_menu_items[i].parent_index == current_parent) {
            if (count == m_selected_index) {
                item_index = i;
                break;
            }
            count++;
            items_in_level++;
        }
    }
    
    // セパレーターの場合は次の非セパレーター項目を探す
    if (item_index >= 0 && m_menu_items[item_index].type == MENU_TYPE_SEPARATOR) {
        // まず下方向に探す
        int next_index = m_selected_index;
        bool found = false;
        
        while (next_index < items_in_level - 1) {
            next_index++;
            count = 0;
            for (int i = 0; i < m_num_items; i++) {
                if (m_menu_items[i].parent_index == current_parent) {
                    if (count == next_index) {
                        if (m_menu_items[i].type != MENU_TYPE_SEPARATOR) {
                            m_selected_index = next_index;
                            found = true;
                        }
                        break;
                    }
                    count++;
                }
            }
            if (found) break;
        }
        
        // 下に見つからなければ上方向に探す
        if (!found) {
            int prev_index = m_selected_index;
            while (prev_index > 0) {
                prev_index--;
                count = 0;
                for (int i = 0; i < m_num_items; i++) {
                    if (m_menu_items[i].parent_index == current_parent) {
                        if (count == prev_index) {
                            if (m_menu_items[i].type != MENU_TYPE_SEPARATOR) {
                                m_selected_index = prev_index;
                                found = true;
                            }
                            break;
                        }
                        count++;
                    }
                }
                if (found) break;
            }
        }
    }
}

// Process key input
void PlatformMenu::process_key(int key, bool down) 
{
    if (!m_visible) {
        return;
    }
    
    if (down) {
        // キーが押された時
        m_key_pressed = key;
        m_key_repeat_timer = CTimer::GetClockTicks64() / 1000; // ミリ秒単位の時間を取得
        m_is_repeating = false;
        
        // 最初のキー入力処理
        if (m_file_browser.browser_active) {
            handle_file_browser_key(key);
        } else {
            handle_key_input(key);
        }
    } else {
        // キーが離された時
        if (key == m_key_pressed) {
            m_key_pressed = 0;
        }
    }
}

// Draw the menu
void PlatformMenu::draw() 
{
    static bool browser_changed = false;
    static bool browser_pre_active = false;
    if (!m_visible || !m_fb_console) {
        return;
    }

    // ファイルブラウザのアクティブ状態が変化したらtrueにする(描画される)
    browser_changed = m_file_browser.browser_active != browser_pre_active;
    browser_pre_active = m_file_browser.browser_active;
    
    // キーリピート処理
    if (m_key_pressed || browser_changed) {
        uint64_t current_time = CTimer::GetClockTicks64() / 1000; // ミリ秒単位の時間を取得
        
        if(m_key_pressed) {
            if (!m_is_repeating) {
                // 初回のリピート開始前の遅延
                if (current_time - m_key_repeat_timer > KEY_REPEAT_DELAY) {
                    m_is_repeating = true;

                    // キー処理を実行
                    if (m_file_browser.browser_active) {
                        handle_file_browser_key(m_key_pressed);
                    } else {
                        handle_key_input(m_key_pressed);
                    }
                    m_key_repeat_timer = current_time;
                }
            } else {
                // 2回目以降のリピート
                if (current_time - m_key_repeat_timer > KEY_REPEAT_INTERVAL) {
                    // キー処理を実行
                    if (m_file_browser.browser_active) {
                        handle_file_browser_key(m_key_pressed);
                    } else {
                        handle_key_input(m_key_pressed);
                    }
                    m_key_repeat_timer = current_time;
                }
            }
        }

        // キーが押された時しか描画しない
        // ファイルブラウザがアクティブな場合はそちらを描画
        if (m_file_browser.browser_active) {
            draw_file_browser();
        } else {
            draw_menu();
        }
    }
}

// Internal method to draw the menu
void PlatformMenu::draw_menu() 
{
    int width = m_fb_console->getWidth();
    int height = m_fb_console->getHeight();
    
    // Draw background
    m_fb_console->clear(MENU_COLOR_BACKGROUND);
    
    // Draw header
    char header[64];
    sprintf(header, "%s Settings - F12 to close", DEVICE_NAME);
    m_fb_console->drawString(header, m_fb_console->getWidth() / 2 - strlen(header) * 8 / 2, 10, MENU_COLOR_HEADER, MENU_COLOR_BACKGROUND, false);
    
    // 現在のメニュー階層を表示
    if (m_current_depth > 0) {
        char path[MAX_STRING_LENGTH * 2] = "";
        char part[MAX_STRING_LENGTH];
        
        // 各階層のパスを構築
        for (int i = 0; i < m_current_depth; i++) {
            int menu_idx = m_current_menu_path[i];
            if (i > 0) {
                strcat(path, " > ");
            }
            memset(part, 0, sizeof(part));
            strcpy(part, m_menu_items[menu_idx].display_name);
            strcat(path, part);
        }
        
        // パス表示
        m_fb_console->drawString(path, 10, 25, MENU_COLOR_TEXT, MENU_COLOR_BACKGROUND, false);
    }
    
    // Draw menu items for current level
    int y_pos = 40;
    int count = 0;
    int current_parent = (m_current_depth > 0) ? m_current_menu_path[m_current_depth - 1] : -1;
    
    for (int i = 0; i < m_num_items; i++) {
        if (m_menu_items[i].parent_index == current_parent) {
            char item_text[MAX_STRING_LENGTH * 2];
            memset(item_text, 0, sizeof(item_text));  // バッファを完全にクリアする
            
            // セパレーター処理
            if (m_menu_items[i].type == MENU_TYPE_SEPARATOR) {
                // セパレーターの場合は横線を描画する
                int line_y = y_pos;
                int line_start_x = 10;
                int line_width = width - 20; // 画面幅から左右のマージンを引く
                
                // 横線を描画（白色）
                m_fb_console->drawHLine(line_start_x, line_start_x + line_width, line_y, MENU_COLOR_TEXT);

                // テキストは空にする（描画しない）
                item_text[0] = '\0';
            } else {
                // 名前を設定
                strcpy(item_text, m_menu_items[i].display_name);

                char tmp_str[10];
                
                // タイプに基づいてインジケータを追加
                if (m_menu_items[i].type == MENU_TYPE_SUBMENU) {
                    strcat(item_text, " >");
                } else if (m_menu_items[i].type == MENU_TYPE_BOOL) {
                    // 設定から現在の値を取得
                    bool value = get_config_bool(m_menu_items[i].config_name);
                    strcat(item_text, value ? ": ON" : ": OFF");
                } else if (m_menu_items[i].type == MENU_TYPE_INT_SELECT) {
                    // 同じ値の場合「V」を表示
                    bool is_same;
                    int config_value;
                    int value = get_config_int(m_menu_items[i].config_name, &config_value, &is_same);
                    sprintf(item_text, "%s%s", is_same ? "[X] " : "[ ] ", m_menu_items[i].display_name);
                } else if (m_menu_items[i].type == MENU_TYPE_INT) {
                    // 値をそのまま表示
                    bool is_same;
                    int config_value;
                    int value = get_config_int(m_menu_items[i].config_name, &config_value, &is_same);
                    sprintf(tmp_str, ": %d", value);
                    strcat(item_text, tmp_str);
                }
            }
            
            // 選択項目をハイライト
            FBConsoleColor text_color = (count == m_selected_index) ? MENU_COLOR_SELECTED : MENU_COLOR_TEXT;
            
            // メニュー項目を描画 (セパレーターの場合は既に線を描画済みなのでスキップ)
            if (m_menu_items[i].type != MENU_TYPE_SEPARATOR) {
                m_fb_console->drawString(item_text, 20, y_pos, text_color, MENU_COLOR_BACKGROUND, false);
            }
            
            // FILE型項目の場合、下に現在のファイルパスを表示
            if (m_menu_items[i].type == MENU_TYPE_FILE) {
                char file_path[MAX_STRING_LENGTH];
                memset(file_path, 0, sizeof(file_path));  // バッファを完全にクリアする
                
                // 設定から現在のファイルパスを取得
                strcpy(file_path, get_config_string(m_menu_items[i].config_name));
                
                // テキストオーバーフローを避けるために最大幅を計算
                int max_width = width - 50;  // マージンを残す
                
                // 必要ならパスを切り詰める（単純な方法）
                if (strlen(file_path) > max_width / 8) {  // 適合する文字数の大まかな見積もり
                    file_path[max_width / 8 - 3] = '.';
                    file_path[max_width / 8 - 2] = '.';
                    file_path[max_width / 8 - 1] = '.';
                    file_path[max_width / 8] = '\0';
                }
                
                // ファイルが挿入されていない場合のメッセージを表示
                if (file_path[0] == '\0') {
                    strcpy(file_path, "< Not Inserted >");
                    m_fb_console->drawString(file_path, 40, y_pos + 12, MENU_COLOR_VALUE, MENU_COLOR_BACKGROUND, false);
                } else {
                    m_fb_console->drawString(file_path, 40, y_pos + 12, MENU_COLOR_TEXT, MENU_COLOR_BACKGROUND, false);
                }
                
                // 追加の行を考慮して次の項目のy位置を調整
                y_pos += 12;
            }
            
            // セパレーターの場合は高さを少し小さくする（通常アイテムの約2/3）
            if (m_menu_items[i].type == MENU_TYPE_SEPARATOR) {
                y_pos += 10; // 通常の16の代わりに10にする
            } else {
                y_pos += 16;
            }
            count++;
        }
    }
    
    // Draw navigation help
    const char* navigation_help = "Use arrows to navigate, Enter to select, ESC to go back";
    m_fb_console->drawString(navigation_help, m_fb_console->getWidth() / 2 - strlen(navigation_help) * 8 / 2, height - 20, MENU_COLOR_TEXT, MENU_COLOR_BACKGROUND, false);
}

// Handle keyboard input
void PlatformMenu::handle_key_input(int key) 
{
    int current_parent = (m_current_depth > 0) ? m_current_menu_path[m_current_depth - 1] : -1;
    int items_in_level = 0;
    
    // Count items in current level
    for (int i = 0; i < m_num_items; i++) {
        if (m_menu_items[i].parent_index == current_parent) {
            items_in_level++;
        }
    }
    
    // Handle different keys
    switch (key) {
        case MENU_KEY_UP:
            if (m_selected_index > 0) {
                m_selected_index--;
                
                // セパレーターの上にカーソルがある場合は、さらに上に移動
                int count = 0;
                int item_index = -1;
                for (int i = 0; i < m_num_items; i++) {
                    if (m_menu_items[i].parent_index == current_parent) {
                        if (count == m_selected_index) {
                            item_index = i;
                            break;
                        }
                        count++;
                    }
                }
                
                // セパレーターならさらに上へ
                if (item_index >= 0 && m_menu_items[item_index].type == MENU_TYPE_SEPARATOR) {
                    if (m_selected_index > 0) {
                        m_selected_index--;
                    } else {
                        // 最初の項目がセパレーターなら次の項目へ
                        m_selected_index++;
                    }
                }
            }
            break;
            
        case MENU_KEY_DOWN:
            if (m_selected_index < items_in_level - 1) {
                m_selected_index++;
                
                // セパレーターの上にカーソルがある場合は、さらに下に移動
                int count = 0;
                int item_index = -1;
                for (int i = 0; i < m_num_items; i++) {
                    if (m_menu_items[i].parent_index == current_parent) {
                        if (count == m_selected_index) {
                            item_index = i;
                            break;
                        }
                        count++;
                    }
                }
                
                // セパレーターならさらに下へ
                if (item_index >= 0 && m_menu_items[item_index].type == MENU_TYPE_SEPARATOR) {
                    if (m_selected_index < items_in_level - 1) {
                        m_selected_index++;
                    } else {
                        // 最後の項目がセパレーターなら前の項目へ
                        m_selected_index--;
                    }
                }
            }
            break;
            
        case MENU_KEY_ENTER:
            handle_selection();
            break;
            
        case MENU_KEY_ESC:
            // Go back one level
            if (m_current_depth > 0) {
                m_current_depth--;
                m_selected_index = 0;
                
                // 初期表示時にセパレーターを選択しないように調整
                int count = 0;
                for (int i = 0; i < m_num_items; i++) {
                    if (m_menu_items[i].parent_index == current_parent) {
                        if (count == m_selected_index && m_menu_items[i].type == MENU_TYPE_SEPARATOR) {
                            // 最初の項目がセパレーターなら次の項目へ
                            m_selected_index++;
                        }
                        count++;
                    }
                }
            } else {
                // トップレベルでは何もしない (ESCキーを無視)
                // toggle_menu()を呼ぶとシステム全体のmenu_enabledと不整合が生じるため
            }
            break;
            
        case MENU_KEY_F12:
            // Close menu
            toggle_menu();
            break;
    }
}

// Handle selection of menu item
void PlatformMenu::handle_selection() 
{
    int current_parent = (m_current_depth > 0) ? m_current_menu_path[m_current_depth - 1] : -1;
    int count = 0;
    int selected_item_index = -1;
    
    // Find the actual index of the selected item
    for (int i = 0; i < m_num_items; i++) {
        if (m_menu_items[i].parent_index == current_parent) {
            if (count == m_selected_index) {
                selected_item_index = i;
                break;
            }
            count++;
        }
    }
    
    if (selected_item_index < 0) {
        return;
    }
    
    // Handle based on type
    switch (m_menu_items[selected_item_index].type) {
        case MENU_TYPE_SUBMENU:
            // Enter submenu
            if (m_current_depth < MAX_MENU_DEPTH) {
                m_current_menu_path[m_current_depth] = selected_item_index;
                m_current_depth++;
                m_selected_index = 0;
                
                // サブメニューに入った時もセパレーターを選択しないように
                adjust_cursor_position();
            }
            break;
            
        case MENU_TYPE_BOOL:
            // Toggle bool value
            toggle_bool_value(selected_item_index);
            break;
        
        case MENU_TYPE_INT_SELECT:
            set_int_select_value(selected_item_index);
            break;
            
        case MENU_TYPE_FILE:
            // Open file selection
            {
                CLogger* logger = get_logger();
                if (logger) {
                    logger->Write("PlatformMenu", LogNotice, "file selection started: index=%d, type=%d, display_name=%s, config_name=%s", 
                               selected_item_index, 
                               m_menu_items[selected_item_index].type,
                               m_menu_items[selected_item_index].display_name,
                               m_menu_items[selected_item_index].config_name);
                }
                handle_file_selection(selected_item_index);
            }
            break;
            
        case MENU_TYPE_ACTION:
            // Execute action
            handle_action(m_menu_items[selected_item_index].action);
            break;
    }
}

/* ───────────── 共通ユーティリティ ───────────── */
static inline void skip_ws(char **p)
{
    while (**p == ' ' || **p == '\t') (*p)++;
}

static inline int safe_copy(char *dst, const char *src, size_t max) /* NUL 終端保証 */
{
    size_t n = strlen(src);
    memcpy(dst, src, n);
    dst[n] = '\0';
    return (int)n;
}

/* 追加：1 コンポーネントだけを安全にコピー */
static inline void copy_component(char *dst, const char *src, size_t max)
{
    size_t len = strcspn(src, "/");          /* '/' か NUL までの長さ */
    if (len >= max) len = max - 1;           /* オーバーフロー防止 */
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/* path が既存 submenu ならその index、無ければ生成して返す */
static int find_or_add_submenu(const char *full_path,
                               const char *disp_name,
                               int parent,
                               MenuItemInfo *items,
                               int *num)
{
    for (int i = 0; i < *num; ++i)
        if (items[i].type == MENU_TYPE_SUBMENU &&
            strcmp(items[i].original_path, full_path) == 0)
            return i;

    /* 生成 */
    if (*num >= MAX_MENU_ITEMS) return -1;

    const int idx = (*num)++;
    MenuItemInfo *mi = &items[idx];
    memset(mi, 0, sizeof(*mi));

    copy_component(mi->display_name, disp_name, sizeof(mi->display_name));
    safe_copy(mi->original_path, full_path, sizeof(mi->original_path));
    mi->type         = MENU_TYPE_SUBMENU;
    mi->parent_index = parent;
    if (parent >= 0) {
        if (items[parent].num_children == 0)
            items[parent].first_child_index = idx;
        items[parent].num_children++;
    }
    return idx;
}

/* ───────────── 本体 ───────────── */
void PlatformMenu::parse_menu_item(const char *line)
{
    char buf[MAX_STRING_LENGTH * 4];
    safe_copy(buf, line, sizeof(buf));
    char *p = buf;

    CLogger* logger = get_logger();
    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "menu parsing started: %s", line);
    }

    /* 空行／コメント行を除外 */
    skip_ws(&p);
    if (*p == '\0' || *p == '#') return;

    /* 1) 表示名＝パス */
    if (*p != '"') return;
    char *q = strchr(++p, '"');
    if (!q) return;
    *q = '\0';                     /* p … "Drive/FD0/Insert" 等 */

    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "menu parsing: display_name=%s", p);
    }

    /* 2) type トークン */
    char *type_tok = q + 1;
    skip_ws(&type_tok);
    while (*type_tok == ',') { ++type_tok; skip_ws(&type_tok); }

    int type = -1;
    if      (strncmp(type_tok, "bool",      4) == 0) { type = MENU_TYPE_BOOL;      type_tok += 4; }
    else if (strncmp(type_tok, "file",      4) == 0) { type = MENU_TYPE_FILE;      type_tok += 4; }
    else if (strncmp(type_tok, "action",    6) == 0) { type = MENU_TYPE_ACTION;    type_tok += 6; }
    else if (strncmp(type_tok, "separator", 9) == 0) { type = MENU_TYPE_SEPARATOR; type_tok += 9; }
    else if (strncmp(type_tok, "int_select", 10) == 0) { type = MENU_TYPE_INT_SELECT; type_tok += 10; }
    else if (strncmp(type_tok, "int",       3) == 0) { type = MENU_TYPE_INT;       type_tok += 3; }
    else return;

    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "menu parsing: type=%d", type);
    }

    /* 3) 追加パラメータ（"..."） */
    skip_ws(&type_tok);
    while (*type_tok == ',') { ++type_tok; skip_ws(&type_tok); }

    char *param = NULL;
    if (*type_tok == '"') {
        param = ++type_tok;
        char *e = strchr(param, '"');
        if (!e) return;
        *e = '\0';
    }

    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "menu parsing: param=%s", param);
    }

    /* 4) 階層構築 ------------------------------------------------------ */
    /* セパレーター用の特別処理 */
    if (type == MENU_TYPE_SEPARATOR) {
        // セパレーターは特別処理
        int  parent   = -1;
        
        // パスが空の場合、トップレベルのセパレーターとして扱う
        if (p[0] == '\0') {
            // トップレベルのセパレーター
            if (m_num_items >= MAX_MENU_ITEMS) return;
            MenuItemInfo *mi = &m_menu_items[m_num_items];
            memset(mi, 0, sizeof(*mi));
            
            strcpy(mi->display_name, "separator");
            safe_copy(mi->original_path, "separator", sizeof(mi->original_path));
            
            mi->type = MENU_TYPE_SEPARATOR;
            mi->parent_index = -1; // トップレベル
            
            m_num_items++;
            
            if (logger) {
                logger->Write("PlatformMenu", LogNotice, "Added top-level separator at index %d", m_num_items - 1);
            }
            
            return;
        }
        
        // パスの末尾に '/' がある場合 ("CMT/", separator など)
        int path_len = strlen(p);
        if (path_len > 0 && p[path_len - 1] == '/') {
            // 末尾のスラッシュを削除
            p[path_len - 1] = '\0';
            
            // 対応するサブメニューを探す
            for (int i = 0; i < m_num_items; i++) {
                if (m_menu_items[i].type == MENU_TYPE_SUBMENU && 
                    strcmp(m_menu_items[i].original_path, p) == 0) {
                    parent = i;
                    break;
                }
            }
            
            // サブメニュー内のセパレーターとして登録
            if (m_num_items >= MAX_MENU_ITEMS) return;
            MenuItemInfo *mi = &m_menu_items[m_num_items];
            memset(mi, 0, sizeof(*mi));
            
            strcpy(mi->display_name, "separator");
            safe_copy(mi->original_path, "separator", sizeof(mi->original_path));
            
            mi->type = MENU_TYPE_SEPARATOR;
            mi->parent_index = parent;
            
            if (parent >= 0) {
                if (m_menu_items[parent].num_children == 0)
                    m_menu_items[parent].first_child_index = m_num_items;
                m_menu_items[parent].num_children++;
            }
            
            m_num_items++;
            
            if (logger) {
                logger->Write("PlatformMenu", LogNotice, "Added separator in submenu '%s' at index %d", 
                             p, m_num_items - 1);
            }
            
            return;
        }
    }
    
    /* 通常の階層構築処理 */
    /* 末端 (leaf) を除く全セグメントを順次生成／検索 */
    int  parent   = -1;
    char path_so_far[MAX_STRING_LENGTH] = "";
    char *seg     = p;
    while (1) {
        char *slash = strchr(seg, '/');
        int  last   = (slash == NULL);
        size_t seg_len = last ? strlen(seg) : (size_t)(slash - seg);

        if (seg_len == 0) break;   /* "//" などは無視 */

        /* path_so_far = 既存 + "/" + seg */
        if (path_so_far[0]) strncat(path_so_far, "/", sizeof(path_so_far) - 1);
        strncat(path_so_far, seg, seg_len);

        if (!last) {
            char seg_name[MAX_STRING_LENGTH];
            safe_copy(seg_name, seg, seg_len + 1);
            parent = find_or_add_submenu(path_so_far, seg_name, parent,
                                         m_menu_items, &m_num_items);
            // display_nameを表示
            if (logger) {
                logger->Write("PlatformMenu", LogNotice, "menu parsing(!last): display_name=%s", m_menu_items[parent].display_name);
            }
        } else {
            /* 5) leaf 自体を登録 --------------------------------------- */
            if (m_num_items >= MAX_MENU_ITEMS) return;
            MenuItemInfo *mi = &m_menu_items[m_num_items];
            memset(mi, 0, sizeof(*mi));

            copy_component(mi->display_name, seg, sizeof(mi->display_name));
            safe_copy(mi->original_path, p, sizeof(mi->original_path));

            if (logger) {   
                logger->Write("PlatformMenu", LogNotice, "menu parsing: original_path=%s", mi->original_path);
            }

            mi->type         = type;
            mi->parent_index = parent;
            if (parent >= 0) {
                if (m_menu_items[parent].num_children == 0)
                    m_menu_items[parent].first_child_index = m_num_items;
                m_menu_items[parent].num_children++;
            }

            if (param) {
                if (type == MENU_TYPE_BOOL || type == MENU_TYPE_FILE || type == MENU_TYPE_INT_SELECT || type == MENU_TYPE_INT)
                    safe_copy(mi->config_name, param, sizeof(mi->config_name));
                else if (type == MENU_TYPE_ACTION)
                    safe_copy(mi->action, param, sizeof(mi->action));
            }

            if (type == MENU_TYPE_FILE) {
                if (CLogger *lg = get_logger())
                    lg->Write("PlatformMenu", LogNotice,
                              "registered: idx=%d type=FILE name=%s cfg=%s path=%s",
                              m_num_items, mi->display_name,
                              mi->config_name, mi->original_path);
            } else if (type == MENU_TYPE_INT_SELECT) {
                if (CLogger *lg = get_logger())
                    lg->Write("PlatformMenu", LogNotice,
                              "registered: idx=%d type=INT_SELECT name=%s cfg=%s path=%s",
                              m_num_items, mi->display_name,
                              mi->config_name, mi->original_path);
            }

            m_num_items++;
            break;
        }
        seg = slash + 1;
    }
}

// Load menu configuration from file
bool PlatformMenu::load_menu_file(const char* filename) 
{
    char line[1024];
    get_logger()->Write("PlatformMenu", LogNotice, "load_menu_file: %s", filename);

    FILE* fp = fopen(filename, "r");
    
    if (!fp) {
        return false;
    }
    
    m_num_items = 0;
    
    // Read and parse each line
    while (fgets(line, sizeof(line), fp) && m_num_items < MAX_MENU_ITEMS) {
        get_logger()->Write("PlatformMenu", LogNotice, "load_menu_file: line=%s", line);
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[len-1] = '\0';
        }
        
        parse_menu_item(line);
    }
    
    fclose(fp);
    
    // サブメニュー階層の調整
    // 同じ階層に同じ名前のメニュー項目があれば修正
    for (int i = 0; i < m_num_items; i++) {
        for (int j = 0; j < m_num_items; j++) {
            if (i != j && 
                m_menu_items[i].parent_index == m_menu_items[j].parent_index && 
                strcmp(m_menu_items[i].display_name, m_menu_items[j].display_name) == 0) {
                
                // サブメニューの場合
                if (m_menu_items[i].type == MENU_TYPE_SUBMENU && m_menu_items[j].type == MENU_TYPE_SUBMENU) {
                    // original_pathを比較して、より適切な親を選択
                    char* i_last_slash = strrchr(m_menu_items[i].original_path, '/');
                    char* j_last_slash = strrchr(m_menu_items[j].original_path, '/');
                    
                    // 両方のサブメニューに子アイテムがある場合、片方の子を他方に移動
                    for (int k = 0; k < m_num_items; k++) {
                        if (m_menu_items[k].parent_index == j) {
                            m_menu_items[k].parent_index = i;
                        }
                    }
                }
            }
        }
    }
    
    return true;
}

// Toggle boolean configuration value
void PlatformMenu::toggle_bool_value(int item_index) 
{
    if (item_index < 0 || item_index >= m_num_items) {
        return;
    }
    
    if (m_menu_items[item_index].type != MENU_TYPE_BOOL) {
        return;
    }

    // iniではなくconfigをいじる(これでsave_configすればiniが書き変わる)
    bool current_value = get_config_bool(m_menu_items[item_index].config_name);
    apply_config_bool(m_menu_items[item_index].config_name, !current_value);
}

// Set int select value
void PlatformMenu::set_int_select_value(int item_index) 
{
    if (item_index < 0 || item_index >= m_num_items) {
        return;
    }

    if (m_menu_items[item_index].type != MENU_TYPE_INT_SELECT) {
        return;
    }

    // 選択された値を取得
    int config_value;
    bool is_same;
    int selected_value = get_config_int(m_menu_items[item_index].config_name, &config_value, &is_same);

    // config_valueで得られた値をそのまま値として設定する
    apply_config_int(m_menu_items[item_index].config_name, config_value);

    // ストレッチモードが変わったら実行してやる
    if(strncmp(m_menu_items[item_index].config_name, "display_stretch", 15) == 0) {
        m_emulator->set_host_window_size(-1, -1, false);
    }
}

// Handle file selection
void PlatformMenu::handle_file_selection(int item_index) 
{
    if (item_index < 0 || item_index >= m_num_items) {
        CLogger* logger = get_logger();
        if (logger) {
            logger->Write("PlatformMenu", LogError, "invalid menu_index: %d (array size: %d)", item_index, m_num_items);
        }
        return;
    }
    
    CLogger* logger = get_logger();
    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "file selection started: %s (index: %d, config_name: %s)", 
                     m_menu_items[item_index].display_name, 
                     item_index, 
                     m_menu_items[item_index].config_name);
    }
    
    // ファイルブラウザを初期化して表示
    // menu_items[item_index].original_pathにCMTがあれば「tapes」フォルダ、そうでなれば「disks」フォルダをルートにする
    const char* root_dir = "/disks";
    if (strstr(m_menu_items[item_index].original_path, "CMT") != NULL) {
        root_dir = "/tapes";
    }   
    bool result = init_file_browser(root_dir, item_index);

    
    // どちらも見つからない場合はルートから
    if (!result) {
        result = init_file_browser("/", item_index);
    }
    
    if (!result && logger) {
        logger->Write("PlatformMenu", LogError, "file browser initialization failed");
    }
}

// ファイルブラウザの初期化
bool PlatformMenu::init_file_browser(const char* path, int menu_index) 
{
    CLogger* logger = get_logger();
    
    // 状態初期化
    memset(&m_file_browser, 0, sizeof(m_file_browser));
    m_file_browser.browser_active = true;
    m_file_browser.current_page = 0;
    m_file_browser.selected_index = 0;
    m_file_browser.target_menu_index = menu_index;
    m_file_browser.cache_start_index = 0;
    m_file_browser.total_entries = 0;
    m_file_browser.has_parent_dir = false;
    
    // 初期ルートディレクトリを保存
    strncpy(m_file_browser.initial_root_dir, path, MAX_STRING_LENGTH - 1);
    m_file_browser.initial_root_dir[MAX_STRING_LENGTH - 1] = '\0';
    
    // パスをコピー
    strncpy(m_file_browser.current_dir, path, MAX_STRING_LENGTH - 1);
    m_file_browser.current_dir[MAX_STRING_LENGTH - 1] = '\0';
    
    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "file browser initialized: %s", path);
    }
    
    // ファイルエントリ読み込み
    return load_file_entries(path);
}

// ファイルエントリの読み込み
bool PlatformMenu::load_file_entries(const char* path)
{
    CLogger* logger = get_logger();
    
    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "directory reading: %s", path);
    }
    
    // 初期ルート(/floppy または /floppies)より上には行けないようにする
    if (m_file_browser.initial_root_dir[0] != '\0' && strcmp(path, "/") == 0) {
        const char* initial_root = m_file_browser.initial_root_dir;
        if (initial_root[0] != '\0' && 
            (strcmp(m_file_browser.current_dir, initial_root) == 0 || 
             strncmp(m_file_browser.current_dir, initial_root, strlen(initial_root)) != 0)) {
            if (logger) {
                logger->Write("PlatformMenu", LogNotice, "cannot move above initial root directory");
            }
            return false;
        }
    }
    
    DIR dir;
    FILINFO temp_info;
    FRESULT fr;
    
    // ディレクトリをオープン
    fr = f_opendir(&dir, path);
    if (fr != FR_OK) {
        if (logger) {
            logger->Write("PlatformMenu", LogError, "directory opening failed: %s, error code %d", path, (int)fr);
        }
        return false;
    }
    
    // 新しいディレクトリに移動した場合は、すべてのエントリを数え直す
    if (m_file_browser.total_entries <= 0 || strcmp(path, m_file_browser.current_dir) != 0) {
        int total_count = 0;
        
        // ルートディレクトリ、または初期ルートでなければ親ディレクトリへのリンクを追加
        if (strcmp(path, "/") != 0 && 
           (m_file_browser.initial_root_dir[0] == '\0' || strcmp(path, m_file_browser.initial_root_dir) != 0)) {
            m_file_browser.has_parent_dir = true;
            total_count++; // ".." 用に1カウント増やす
        } else {
            m_file_browser.has_parent_dir = false;
        }
        
        for (;;) {
            fr = f_readdir(&dir, &temp_info);
            if (fr != FR_OK || temp_info.fname[0] == 0) {
                break;
            }
            
            // ドットで始まるファイルはスキップ（隠しファイル）
            if (temp_info.fname[0] == '.' && strcmp(temp_info.fname, "..") != 0) {
                continue;
            }
            
            total_count++;
        }
        
        // 合計エントリ数を設定
        m_file_browser.total_entries = total_count;
        
        // ページ数を計算
        m_file_browser.page_count = (m_file_browser.total_entries + m_files_per_page - 1) / m_files_per_page;
        if (m_file_browser.page_count < 1) {
            m_file_browser.page_count = 1; // 少なくとも1ページ
        }
        
        // 新しいディレクトリに移動した場合はキャッシュを先頭からにする
        m_file_browser.cache_start_index = 0;
        
        // ディレクトリを巻き戻す
        f_closedir(&dir);
        fr = f_opendir(&dir, path);
        if (fr != FR_OK) {
            if (logger) {
                logger->Write("PlatformMenu", LogError, "directory reopening failed: %s", path);
            }
            return false;
        }
    }
    
    // キャッシュする範囲を確認
    int display_start = m_file_browser.cache_start_index;
    int display_end = display_start + MAX_FILE_ENTRIES;
    
    // 合計エントリ数より大きい場合は調整
    if (display_end > m_file_browser.total_entries) {
        display_end = m_file_browser.total_entries;
    }
    
    // 実際にキャッシュするエントリ数
    m_file_browser.entry_count = display_end - display_start;
    
    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "cache range: %d-%d (total %d entries)", 
                     display_start, display_end - 1, m_file_browser.total_entries);
    }
    
    // エントリの読み込み
    int real_index = 0;  // 実際のファイルシステム上のインデックス
    int cache_index = 0; // キャッシュ配列内のインデックス
    
    // 親ディレクトリのエントリを追加（必要な場合）
    if (m_file_browser.has_parent_dir) {
        // キャッシュ範囲に含まれる場合のみ追加
        if (display_start == 0) {
            // キャッシュの先頭に親ディレクトリのエントリを追加
            FILINFO* fno = &m_file_browser.entries[cache_index++];
            strcpy(fno->fname, "..");
            fno->fattrib = AM_DIR;
            fno->fsize = 0;
        }
        real_index = 1;  // 実際のファイルシステム上では親ディレクトリの次から
    }
    
    // 表示開始位置までスキップ
    int skip_count = 0;
    if (m_file_browser.has_parent_dir) {
        skip_count = (display_start == 0) ? 0 : display_start - 1;
    } else {
        skip_count = display_start;
    }
    
    for (int i = 0; i < skip_count; i++) {
        fr = f_readdir(&dir, &temp_info);
        if (fr != FR_OK || temp_info.fname[0] == 0) {
            break;
        }
        
        // ドットファイルをスキップ
        while (temp_info.fname[0] == '.' && strcmp(temp_info.fname, "..") != 0) {
            fr = f_readdir(&dir, &temp_info);
            if (fr != FR_OK || temp_info.fname[0] == 0) {
                break;
            }
        }
        
        real_index++;
    }
    
    // 必要な分だけエントリを読み込む
    while (cache_index < m_file_browser.entry_count) {
        // 親ディレクトリのエントリが先頭にある場合はスキップ
        if (m_file_browser.has_parent_dir && display_start == 0 && cache_index == 0) {
            cache_index++;
            continue;
        }
        
        FILINFO* fno = &m_file_browser.entries[cache_index];
        
        fr = f_readdir(&dir, fno);
        if (fr != FR_OK || fno->fname[0] == 0) {
            break;
        }
        
        // ドットで始まるファイルはスキップ（隠しファイル）
        if (fno->fname[0] == '.' && strcmp(fno->fname, "..") != 0) {
            continue;
        }
        
        cache_index++;
        real_index++;
    }
    
    // ディレクトリクローズ
    f_closedir(&dir);
    
    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "file entry reading completed: cache %d entries, total %d entries", 
                      m_file_browser.entry_count, m_file_browser.total_entries);
    }
    
    return true;
}

// ファイルブラウザの描画
void PlatformMenu::draw_file_browser()
{
    int width = m_fb_console->getWidth();
    int height = m_fb_console->getHeight();
    
    // バックグラウンドをクリア
    m_fb_console->clear(MENU_COLOR_BACKGROUND);
    
    // ヘッダーを描画（タイトルとディレクトリパス）
    char header[MAX_STRING_LENGTH];
    
    // target_menu_indexが範囲内かチェック
    if (m_file_browser.target_menu_index >= 0 && m_file_browser.target_menu_index < m_num_items) {
        snprintf(header, sizeof(header), "file select - %s", 
                 m_menu_items[m_file_browser.target_menu_index].display_name);
    } else {
        snprintf(header, sizeof(header), "file select");
    }
    
    m_fb_console->drawString(header, 10, 10, MENU_COLOR_HEADER, MENU_COLOR_BACKGROUND, false);
    
    // 現在のディレクトリパスを表示
    m_fb_console->drawString(m_file_browser.current_dir, 10, 25, MENU_COLOR_TEXT, MENU_COLOR_BACKGROUND, false);
    
    // ページ情報を表示
    char page_info[64];
    snprintf(page_info, sizeof(page_info), "page: %d/%d (%d items)", 
             m_file_browser.current_page + 1, m_file_browser.page_count, m_file_browser.total_entries);
    m_fb_console->drawString(page_info, width - 200, 25, MENU_COLOR_TEXT, MENU_COLOR_BACKGROUND, false);
    
    // 現在のページのファイル項目を表示
    int start_index = m_file_browser.current_page * m_files_per_page;
    int end_index = start_index + m_files_per_page;
    if (end_index > m_file_browser.total_entries) {
        end_index = m_file_browser.total_entries;
    }
    
    int y_pos = 40;
    
    // 表示すべき項目数を計算
    int items_to_display = end_index - start_index;
    
    // 現在表示しているページの範囲がキャッシュにない場合は更新
    int current_page_end = start_index + items_to_display - 1;
    if (start_index < m_file_browser.cache_start_index || 
        current_page_end >= m_file_browser.cache_start_index + m_file_browser.entry_count) {
        update_file_cache(start_index);
    }
    
    // 現在のページの各項目を描画
    for (int i = 0; i < items_to_display; i++) {
        int global_index = start_index + i;
        
        // キャッシュ内のインデックスに変換
        int cache_index = global_index - m_file_browser.cache_start_index;
        
        // キャッシュから取得できる場合のみ表示（範囲外チェック）
        if (cache_index >= 0 && cache_index < m_file_browser.entry_count) {
            // キャッシュ内に存在する項目を描画
            FILINFO* fno = &m_file_browser.entries[cache_index];
            char item_text[MAX_STRING_LENGTH];
            
            // ディレクトリならアイコンを付ける
            if (fno->fattrib & AM_DIR) {
                snprintf(item_text, sizeof(item_text), "[DIR] %s", fno->fname);
            } else {
                strncpy(item_text, fno->fname, sizeof(item_text) - 1);
                item_text[sizeof(item_text) - 1] = '\0';
            }
            
            // 選択項目のハイライト
            FBConsoleColor text_color = (global_index == m_file_browser.selected_index) ? 
                                        MENU_COLOR_SELECTED : MENU_COLOR_TEXT;
            
            // 項目を描画
            m_fb_console->drawString(item_text, 20, y_pos, text_color, MENU_COLOR_BACKGROUND, false);
        }
        
        y_pos += 16;
    }
    
    // フッター（ナビゲーションヘルプ）を表示
    m_fb_console->drawString("↑↓: item select  ←→: page switch  Enter: select  ESC: back", 
                          10, height - 20, MENU_COLOR_TEXT, MENU_COLOR_BACKGROUND, false);
}

// ファイルブラウザのキー入力処理
void PlatformMenu::handle_file_browser_key(int key)
{
    switch (key) {
        case MENU_KEY_UP:
            // 前の項目へ移動
            if (m_file_browser.selected_index > 0) {
                m_file_browser.selected_index--;
                
                // ページをまたぐ場合は前のページへ
                if (m_file_browser.selected_index < m_file_browser.current_page * m_files_per_page) {
                    m_file_browser.current_page--;
                }
                
                // キャッシュ範囲外にアクセスする場合はキャッシュを更新
                if (m_file_browser.selected_index < m_file_browser.cache_start_index) {
                    update_file_cache(m_file_browser.selected_index);
                }
            }
            break;
            
        case MENU_KEY_DOWN:
            // 次の項目へ移動
            if (m_file_browser.selected_index < m_file_browser.total_entries - 1) {
                m_file_browser.selected_index++;
                
                // ページをまたぐ場合は次のページへ
                if (m_file_browser.selected_index >= (m_file_browser.current_page + 1) * m_files_per_page) {
                    m_file_browser.current_page++;
                }
                
                // キャッシュ範囲外にアクセスする場合はキャッシュを更新
                if (m_file_browser.selected_index >= m_file_browser.cache_start_index + m_file_browser.entry_count) {
                    update_file_cache(m_file_browser.selected_index);
                }
            }
            break;
            
        case MENU_KEY_LEFT:
            // 前のページへ
            if (m_file_browser.current_page > 0) {
                m_file_browser.current_page--;
                // ページの先頭を選択
                m_file_browser.selected_index = m_file_browser.current_page * m_files_per_page;
                
                // キャッシュ範囲外にアクセスする場合はキャッシュを更新
                if (m_file_browser.selected_index < m_file_browser.cache_start_index) {
                    update_file_cache(m_file_browser.selected_index);
                }
            }
            break;
            
        case MENU_KEY_RIGHT:
            // 次のページへ
            if (m_file_browser.current_page < m_file_browser.page_count - 1) {
                m_file_browser.current_page++;
                // ページの先頭を選択
                m_file_browser.selected_index = m_file_browser.current_page * m_files_per_page;
                
                // 存在しない項目を選択しないよう調整
                if (m_file_browser.selected_index >= m_file_browser.total_entries) {
                    m_file_browser.selected_index = m_file_browser.total_entries - 1;
                }
                
                // キャッシュ範囲外にアクセスする場合はキャッシュを更新
                if (m_file_browser.selected_index >= m_file_browser.cache_start_index + m_file_browser.entry_count) {
                    update_file_cache(m_file_browser.selected_index);
                }
            }
            break;
            
        case MENU_KEY_ENTER:
            // 項目選択
            select_file_entry(m_file_browser.selected_index);
            break;
            
        case MENU_KEY_ESC:
            // ブラウザを閉じる
            m_file_browser.browser_active = false;
            break;
            
        case MENU_KEY_F12:
            // メニュー全体を閉じる
            toggle_menu();
            break;
    }
}

// ファイル項目選択時の処理
void PlatformMenu::select_file_entry(int index)
{
    if (index < 0 || index >= m_file_browser.total_entries) {
        return;
    }
    
    CLogger* logger = get_logger();
    
    // インデックスをキャッシュ内のインデックスに変換
    int cache_index = index - m_file_browser.cache_start_index;
    
    // キャッシュ範囲外の場合はキャッシュを更新して再試行
    if (cache_index < 0 || cache_index >= m_file_browser.entry_count) {
        if (update_file_cache(index)) {
            // 更新後の新しいキャッシュインデックスを計算
            cache_index = index - m_file_browser.cache_start_index;
            // キャッシュ更新後も範囲外の場合は処理終了
            if (cache_index < 0 || cache_index >= m_file_browser.entry_count) {
                if (logger) {
                    logger->Write("PlatformMenu", LogError, "index out of range after cache update: %d", index);
                }
                return;
            }
        } else {
            if (logger) {
                logger->Write("PlatformMenu", LogError, "cache update failed: %d", index);
            }
            return; // キャッシュ更新失敗
        }
    }
    
    FILINFO* fno = &m_file_browser.entries[cache_index];
    
    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "selected: %s (index: %d, cache: %d)", 
                      fno->fname, index, cache_index);
    }
    
    // ディレクトリの場合はそのディレクトリに移動
    if (fno->fattrib & AM_DIR) {
        char new_path[MAX_STRING_LENGTH];
        
        // ".."（親ディレクトリ）の場合は特別処理
        if (strcmp(fno->fname, "..") == 0) {
            // 現在のパスから最後のディレクトリ部分を削除
            char* last_slash = strrchr(m_file_browser.current_dir, '/');
            if (last_slash) {
                if (last_slash == m_file_browser.current_dir) {
                    // ルートディレクトリへの場合
                    strcpy(new_path, "/");
                } else {
                    // 親ディレクトリへ
                    strncpy(new_path, m_file_browser.current_dir, last_slash - m_file_browser.current_dir);
                    new_path[last_slash - m_file_browser.current_dir] = '\0';
                    // 最後がルートでなくてスラッシュもない場合はルートディレクトリとする
                    if (new_path[0] == '\0') {
                        strcpy(new_path, "/");
                    }
                }
            } else {
                // スラッシュがない場合（通常はありえない）
                strcpy(new_path, "/");
            }
        } else {
            // 通常のディレクトリの場合
            if (strcmp(m_file_browser.current_dir, "/") == 0) {
                // ルートディレクトリの場合はスラッシュを一つだけにする
                snprintf(new_path, sizeof(new_path), "/%s", fno->fname);
            } else {
                // それ以外の場合はパスを連結
                snprintf(new_path, sizeof(new_path), "%s/%s", m_file_browser.current_dir, fno->fname);
            }
        }
        
        // 新しいディレクトリをロード
        m_file_browser.cache_start_index = 0; // キャッシュをリセット
        if (load_file_entries(new_path)) {
            // 成功したら現在のディレクトリを更新
            strncpy(m_file_browser.current_dir, new_path, sizeof(m_file_browser.current_dir) - 1);
            m_file_browser.current_dir[sizeof(m_file_browser.current_dir) - 1] = '\0';
            
            // ページとインデックスをリセット
            m_file_browser.current_page = 0;
            m_file_browser.selected_index = 0;
        }
    } else {
        // ファイルの場合はconfig設定に適用し、ブラウザを閉じる
        char full_path[MAX_STRING_LENGTH];
        
        // フルパスを構築
        if (strcmp(m_file_browser.current_dir, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", fno->fname);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", m_file_browser.current_dir, fno->fname);
        }
        
        if (logger) {
            logger->Write("PlatformMenu", LogNotice, "applying file: (%d) %s - menu_items array size: %d, config_name: %s", 
                         m_file_browser.target_menu_index, full_path, 
                         m_num_items,
                         (m_file_browser.target_menu_index >= 0 && m_file_browser.target_menu_index < m_num_items) 
                             ? m_menu_items[m_file_browser.target_menu_index].config_name 
                             : "out of range error");
        }
        
        // 設定を適用
        if (m_file_browser.target_menu_index >= 0 && m_file_browser.target_menu_index < m_num_items) {
            apply_config_string(m_menu_items[m_file_browser.target_menu_index].config_name, full_path);
        } else if (logger) {
            logger->Write("PlatformMenu", LogError, "invalid menu_index: %d (array size: %d)", 
                         m_file_browser.target_menu_index, m_num_items);
        }
        
        // ブラウザを閉じる
        m_file_browser.browser_active = false;
    }
}

// Handle action based on action string
bool PlatformMenu::handle_action(const char* action) 
{
    if (!action || !m_emulator) {
        return false;
    }
    
    // Parse the action string, which has format "Command:Parameter"
    char action_copy[MAX_STRING_LENGTH];
    strcpy(action_copy, action);
    
    char* colon = strchr(action_copy, ':');
    int param = -1;
    
    if (colon) {
        *colon = '\0';
        param = atoi(colon + 1);
    }
    
    // Handle various actions
    if (strcmp(action_copy, "FDClose") == 0 && param >= 0) {
        // Close floppy disk
        m_emulator->eject_floppy_disk(param);
        return true;
    } 
    else if (strcmp(action_copy, "FDCreate") == 0 && param >= 0) {
        // Create blank disk - would need implementation
        // For now, just close the current disk
        m_emulator->eject_floppy_disk(param);
        return true;
    }
    else if (strcmp(action_copy, "CMTEject") == 0 && param >= 0) {
        // Close CMT
        m_emulator->eject_tape(param);
        return true;
    }
    else if (strcmp(action_copy, "CMTPlay") == 0 && param >= 0) {
        // Play CMT
        m_emulator->push_play(param);
        return true;
    }
    else if (strcmp(action_copy, "CMTStop") == 0 && param >= 0) {
        // Stop CMT
        m_emulator->push_stop(param);
        return true;
    }
    else if (strcmp(action_copy, "CMTForward") == 0 && param >= 0) {
        // Fast Forward CMT
        m_emulator->push_fast_forward(param);
        return true;
    }
    else if (strcmp(action_copy, "CMTRewind") == 0 && param >= 0) {
        // Fast Rewind CMT
        m_emulator->push_fast_rewind(param);
        return true;
    }
    else if (strcmp(action_copy, "CMTAPSSForward") == 0 && param >= 0) {
        // APSS Forward CMT
        m_emulator->push_apss_forward(param);
        return true;
    }
    else if (strcmp(action_copy, "CMTAPSSRewind") == 0 && param >= 0) {
        // APSS Rewind CMT
        m_emulator->push_apss_rewind(param);
        return true;
    }
    else if (strcmp(action_copy, "Reset") == 0) {
        // Computer reset
        m_emulator->reset();
        // メニューも閉じる
        toggle_menu();
        return true;
    }
    else if (strcmp(action_copy, "NMI") == 0) {
        // NMI
        m_emulator->special_reset();
        return true;
    }
    else if (strcmp(action_copy, "About") == 0) {
        // Show about info - not implemented in this basic version
        return true;
    }
    else if (strcmp(action_copy, "License") == 0) {
        // Show license info - not implemented in this basic version
        return true;
    } else if (strcmp(action_copy, "SaveDiskChanges") == 0) {
        // Save disk changes
        save_disk_changes();
        return true;
    } else if (strcmp(action_copy, "SaveTapeChanges") == 0) {
        // Save tape changes
        save_tape_changes();
        return true;
    }
    return false;
} 

void PlatformMenu::save_disk_changes()
{
    // ディスクの変更を保存
    // 一度クローズして開きなおす……
    for(int drv = 0; drv < USE_FLOPPY_DISK; drv++) {
        if(m_emulator->is_floppy_disk_inserted(drv)) {
            m_emulator->close_floppy_disk(drv);
            m_emulator->open_floppy_disk(drv, m_config->get_config()->current_floppy_disk_path[drv], 0);
        }
    }
}

void PlatformMenu::save_tape_changes()
{
    // テープの変更を保存
    // 一度クローズしてから再生して止めてやる(強引)
    for(int drv = 0; drv < USE_TAPE; drv++) {
        if(m_emulator->is_tape_inserted(drv)) {
            m_emulator->close_tape(drv);
            m_emulator->play_tape(drv, m_config->get_config()->current_tape_path[drv]);
            m_emulator->push_stop(drv);
        }
    }
}

bool PlatformMenu::get_config_bool(const char* config_name)
{
    return m_config->get_config_bool(config_name);
}

int PlatformMenu::get_config_int(const char* config_name, int* config_value, bool* is_same)
{
    return m_config->get_config_int(config_name, config_value, is_same);
}

const char* PlatformMenu::get_config_string(const char* config_name)
{
    return m_config->get_config_string(config_name);
}

void PlatformMenu::apply_config_bool(const char* config_name, bool value)
{
    m_config->apply_config_bool(config_name, value);
}

void PlatformMenu::apply_config_int(const char* config_name, int value)
{
    m_config->apply_config_int(config_name, value);
}

void PlatformMenu::apply_config_string(const char* config_name, const char* value)
{
    // config_nameの末尾に [(数字)] というのがあれば、その数字を事前に取得しておく
    int index = -1;
    char* bracket = strchr(config_name, '[');
    if (bracket) {
        index = atoi(bracket + 1);
    }

    get_logger()->Write("PlatformMenu", LogNotice, "apply_config_string: %s, value: %s, index: %d", config_name, value, index);

    if(strncmp(config_name, "current_floppy_disk_path[", 25) == 0 && index >= 0) {
        m_emulator->insert_floppy_disk(index, value);
    }
    else if(strncmp(config_name, "current_tape_path[", 18) == 0 && index >= 0) {
        m_emulator->play_tape(index, value);
    }
    else if(strncmp(config_name, "record_tape_path[", 17) == 0 && index >= 0) {
        m_emulator->record_tape(index, value);
    }

    // 特例として↑のようにエミュレーター側で反映させてやる
    // mpConfig->apply_config_string(config_name, value);
}

// キャッシュ範囲を更新して再読み込み
bool PlatformMenu::update_file_cache(int new_index)
{
    CLogger* logger = get_logger();
    
    // 表示されるページを保証するためのキャッシュ更新
    // 新しいインデックスを中心に、表示に必要なページサイズを考慮する
    
    // 現在のページを特定
    int current_page = new_index / m_files_per_page;
    int page_start = current_page * m_files_per_page;
    
    // 新しいキャッシュの開始と終了を計算
    // 最低でも現在のページを含める
    int new_start = page_start;
    
    // MAX_FILE_ENTRIESを最大限に活用するため、余分なスペースがあれば前後のページも含める
    int pages_to_cache = MAX_FILE_ENTRIES / m_files_per_page;
    
    // 可能なら前後に均等にページを配置
    int half_pages = pages_to_cache / 2;
    new_start = page_start - (half_pages * m_files_per_page);
    
    // 範囲調整（最小0）
    if (new_start < 0) new_start = 0;
    
    // 末尾に十分なエントリがない場合は前半を増やす
    int new_end = new_start + MAX_FILE_ENTRIES;
    if (new_end > m_file_browser.total_entries) {
        new_end = m_file_browser.total_entries;
        new_start = new_end - MAX_FILE_ENTRIES;
        if (new_start < 0) new_start = 0;
    }
    
    if (logger) {
        logger->Write("PlatformMenu", LogNotice, "cache range updated: %d → %d (page %d, page start %d)", 
                      m_file_browser.cache_start_index, new_start, current_page, page_start);
    }
    
    // キャッシュ開始位置を更新
    m_file_browser.cache_start_index = new_start;
    
    // 現在のディレクトリを再読み込み
    return load_file_entries(m_file_browser.current_dir);
}
