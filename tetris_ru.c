#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>

#define WIDTH 10
#define HEIGHT 20

#define GREEN "\033[92m"
#define RESET "\033[0m"

int board[HEIGHT][WIDTH] = {0};

int shapes[7][4][4][4] = {
    // I-piece (Zsynchronizowane stany pionowe na kolumnie 1)
    {
        {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0}},
        {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0}}
    },
    // J-piece
    {
        {{1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,1,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,1,0}, {0,0,1,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0}}
    },
    // L-piece
    {
        {{0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,1,0}, {1,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}}
    },
    // O-piece (Square)
    {
        {{1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}}
    },
    // S-piece (Zsynchronizowane stany parzyste i nieparzyste)
    {
        {{0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0}},
        {{0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0}}
    },
    // T-piece
    {
        {{0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,1,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}}
    },
    // Z-piece (Zsynchronizowane stany parzyste i nieparzyste)
    {
        {{1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}}
    }
};

int current_shape;
int next_shape;
int current_rotation_state = 0;
int current_rotation[4][4];
int cur_x = 3, cur_y = 0;
int score = 0;
int lines_cleared = 0;
int level = 0;
int start_level = 0;
int game_over = 0;

int show_next = 1;
int show_text = 1;

char player_name[20] = "";
struct termios orig_termios;

void reset_terminal_mode() {
    printf("\033[?25h");
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_noncanonical_mode() {
    struct termios new_termios;
    tcgetattr(0, &orig_termios);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &new_termios);
    atexit(reset_terminal_mode);
    printf("\033[?25l");
    printf("\033[2J");
}

int kbhit() {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

void copy_shape(int src[4][4], int dest[4][4]) {
    for(int i = 0; i < 4; i++)
        for(int j = 0; j < 4; j++)
            dest[i][j] = src[i][j];
}

int check_collision(int shape[4][4], int x, int y) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (shape[i][j]) {
                int nx = x + j;
                int ny = y + i;
                if (nx < 0 || nx >= WIDTH || ny >= HEIGHT) return 1;
                if (ny >= 0 && board[ny][nx]) return 1;
            }
        }
    }
    return 0;
}

void draw_game_custom(int board_rows, int right_rows, int left_rows);
void draw_game();

void merge_piece() {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (current_rotation[i][j]) {
                int ny = cur_y + i;
                int nx = cur_x + j;
                if (ny >= 0 && ny < HEIGHT && nx >= 0 && nx < WIDTH) {
                    board[ny][nx] = 1;
                }
            }
        }
    }
    
    int base_piece_score = 20 * (level + 1);
    if (!show_next) {
        base_piece_score *= 2;
    }
    score += base_piece_score;

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            current_rotation[i][j] = 0;
}

void remove_lines() {
    int full_lines[HEIGHT];
    int count = 0;

    for (int i = 0; i < HEIGHT; i++) {
        int full = 1;
        for (int j = 0; j < WIDTH; j++) {
            if (!board[i][j]) { full = 0; break; }
        }
        if (full) full_lines[count++] = i;
    }

    if (count > 0) {
        int temp_board[HEIGHT][WIDTH];
        memset(temp_board, 0, sizeof(temp_board));
        int new_row = HEIGHT - 1;

        for (int i = HEIGHT - 1; i >= 0; i--) {
            int is_full = 0;
            for (int c = 0; c < count; c++) {
                if (full_lines[c] == i) { is_full = 1; break; }
            }
            if (!is_full) {
                for (int j = 0; j < WIDTH; j++) {
                    temp_board[new_row][j] = board[i][j];
                }
                new_row--;
            }
        }

        for (int f = 0; f < 3; f++) {
            for (int c = 0; c < count; c++) {
                for (int j = 0; j < WIDTH; j++) {
                    board[full_lines[c]][j] = (f % 2 == 0) ? 0 : 1;
                }
            }
            draw_game();
            usleep(70000);
        }

        for (int i = 0; i < HEIGHT; i++) {
            for (int j = 0; j < WIDTH; j++) {
                board[i][j] = temp_board[i][j];
            }
        }

        score += count * 100 * (level + 1);
        lines_cleared += count;
        level = start_level + (lines_cleared / 10);
    }
}

void spawn_piece() {
    current_shape = next_shape;
    next_shape = rand() % 7;
    current_rotation_state = 0;
    copy_shape(shapes[current_shape][current_rotation_state], current_rotation);
    cur_x = WIDTH / 2 - 2;
    cur_y = 0;
    if (check_collision(current_rotation, cur_x, cur_y)) {
        game_over = 1;
    }
}

void draw_menu(char typed_char) {
    char buffer[4096];
    int ptr = 0;
    ptr += sprintf(buffer + ptr, "\033[H%s", GREEN);
    ptr += sprintf(buffer + ptr, "\n\n        [ ]\n        Т Е Т Р И С\n                [ ]\n\n\n\n\n\n\n\n\n\n\n");
    ptr += sprintf(buffer + ptr, "    ВАШ УРОВЕНЬ? (0-9) - ");
    if (typed_char != '\0') ptr += sprintf(buffer + ptr, "%c", typed_char);
    ptr += sprintf(buffer + ptr, "%s", RESET);
    fputs(buffer, stdout);
    fflush(stdout);
}

int utf8_strlen(const char *s) {
    int count = 0;
    while (*s) {
        if ((*s & 0xC0) != 0x80) count++;
        s++;
    }
    return count;
}

void print_padded_left(char *buffer, int *ptr, const char *format, ...) {
    char temp[64];
    va_list args;
    va_start(args, format);
    vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);

    int visual_len = utf8_strlen(temp);
    int target_cols = 32;
    
    *ptr += sprintf(buffer + *ptr, "%s", temp);
    while (visual_len < target_cols) {
        *ptr += sprintf(buffer + *ptr, " ");
        visual_len++;
    }
}

void draw_game_custom(int board_rows, int right_rows, int left_rows) {
    char buffer[16384];
    int ptr = 0;

    ptr += sprintf(buffer + ptr, "\033[H%s", GREEN);

    int temp_board[HEIGHT][WIDTH];
    for(int i = 0; i < HEIGHT; i++)
        for(int j = 0; j < WIDTH; j++)
            temp_board[i][j] = board[i][j];

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (current_rotation[i][j]) {
                int ny = cur_y + i;
                int nx = cur_x + j;
                if (ny >= 0 && ny < HEIGHT && nx >= 0 && nx < WIDTH) {
                    temp_board[ny][nx] = 2;
                }
            }
        }
    }

    for (int i = 0; i < HEIGHT + 2; i++) {
        if (i < left_rows) {
            if (i == 0)      print_padded_left(buffer, &ptr, "ПОЛНЫХ СТРОК: %d", lines_cleared);
            else if (i == 1) print_padded_left(buffer, &ptr, "УРОВЕНЬ:      %d", level);
            else if (i == 2) {
                int display_score = score % 1000;
                print_padded_left(buffer, &ptr, "СЧЕТ:         %d", display_score);
            }
            else if (i == 3) {
                if (score >= 2000)      print_padded_left(buffer, &ptr, "              **");
                else if (score >= 1000) print_padded_left(buffer, &ptr, "              *");
                else                    print_padded_left(buffer, &ptr, "");
            }
            else if (show_next && i >= 8 && i <= 11) {
                int shape_row = i - 8;
                char piece_row_str[20] = "";
                for (int sj = 0; sj < 4; sj++) {
                    if (shapes[next_shape][0][shape_row][sj]) {
                        strcat(piece_row_str, "[ ]");
                    } else {
                        strcat(piece_row_str, "   ");
                    }
                }
                print_padded_left(buffer, &ptr, "  %s", piece_row_str);
            } else {
                print_padded_left(buffer, &ptr, "");
            }
        } else {
            print_padded_left(buffer, &ptr, "");
        }

        if (i < board_rows) {
            if (i < HEIGHT) {
                ptr += sprintf(buffer + ptr, "<!");
                for (int j = 0; j < WIDTH; j++) {
                    if (temp_board[i][j] == 0) {
                        ptr += sprintf(buffer + ptr, " .");
                    } else {
                        ptr += sprintf(buffer + ptr, "[]");
                    }
                }
                ptr += sprintf(buffer + ptr, "!>");
            } else if (i == HEIGHT) {
                ptr += sprintf(buffer + ptr, "<!");
                for (int j = 0; j < WIDTH; j++) {
                    ptr += sprintf(buffer + ptr, "==");
                }
                ptr += sprintf(buffer + ptr, "!>");
            } else {
                ptr += sprintf(buffer + ptr, "  ");
                for (int j = 0; j < WIDTH; j++) {
                    ptr += sprintf(buffer + ptr, "\\/");
                }
            }
        } else {
            for (int j = 0; j < 24; j++) ptr += sprintf(buffer + ptr, " ");
        }

        if (show_text && i < right_rows) {
            if (i == 1)      ptr += sprintf(buffer + ptr, "    7: НАЛЕВО 9: НАПРАВО");
            else if (i == 2) ptr += sprintf(buffer + ptr, "        8:ПОВОРОТ");
            else if (i == 3) ptr += sprintf(buffer + ptr, "    4:УСКОРИТЬ 5:СБРОСИТЬ");
            else if (i == 4) ptr += sprintf(buffer + ptr, "    1: ПОКАЗАТЬ СЛЕДУЮЩУЮ");
            else if (i == 5) ptr += sprintf(buffer + ptr, "    0: СТЕРЕТЬ ЭТОТ ТЕКСТ");
            else if (i == 6) ptr += sprintf(buffer + ptr, "    ПРОБЕЛ - СБРОСИТЬ");
        }

        ptr += sprintf(buffer + ptr, "\033[K\n");
    }

    ptr += sprintf(buffer + ptr, "%s", RESET);
    fputs(buffer, stdout);
    fflush(stdout);
}

void draw_game() {
    draw_game_custom(HEIGHT + 2, HEIGHT + 2, HEIGHT + 2);
}

void animate_board_reveal() {
    printf("\033[2J");
    
    for (int r = 1; r <= HEIGHT + 2; r++) {
        draw_game_custom(r, 0, 0);
        usleep(25000);
    }
    
    for (int r = 1; r <= 8; r++) {
        draw_game_custom(HEIGHT + 2, r, 0);
        usleep(30000);
    }

    for (int r = 1; r <= HEIGHT + 2; r++) {
        draw_game_custom(HEIGHT + 2, 8, r);
        usleep(25000);
    }
}

void draw_game_over_name_prompt(const char *name_buf) {
    char buffer[16384];
    int ptr = 0;
    ptr += sprintf(buffer + ptr, "\033[H%s", GREEN);

    int temp_board[HEIGHT][WIDTH];
    for(int i = 0; i < HEIGHT; i++)
        for(int j = 0; j < WIDTH; j++)
            temp_board[i][j] = board[i][j];

    for (int i = 0; i < HEIGHT + 2; i++) {
        if (i == 0)      print_padded_left(buffer, &ptr, "ПОЛНЫХ СТРОК: %d", lines_cleared);
        else if (i == 1) print_padded_left(buffer, &ptr, "УРОВЕНЬ:      %d", level);
        else if (i == 2) {
            int display_score = score % 1000;
            print_padded_left(buffer, &ptr, "СЧЕТ:         %d", display_score);
        }
        else if (i == 3) {
            if (score >= 2000)      print_padded_left(buffer, &ptr, "              **");
            else if (score >= 1000) print_padded_left(buffer, &ptr, "              *");
            else                    print_padded_left(buffer, &ptr, "");
        }
        else if (i == 15) print_padded_left(buffer, &ptr, "ВАШЕ ИМЯ? %s_", name_buf);
        else {
            print_padded_left(buffer, &ptr, "");
        }

        if (i < HEIGHT) {
            ptr += sprintf(buffer + ptr, "<!");
            for (int j = 0; j < WIDTH; j++) {
                if (temp_board[i][j] == 0) {
                    ptr += sprintf(buffer + ptr, " .");
                } else {
                    ptr += sprintf(buffer + ptr, "[]");
                }
            }
            ptr += sprintf(buffer + ptr, "!>");
        } else if (i == HEIGHT) {
            ptr += sprintf(buffer + ptr, "<!");
            for (int j = 0; j < WIDTH; j++) {
                ptr += sprintf(buffer + ptr, "==");
            }
            ptr += sprintf(buffer + ptr, "!>");
        } else {
            ptr += sprintf(buffer + ptr, "  ");
            for (int j = 0; j < WIDTH; j++) {
                ptr += sprintf(buffer + ptr, "\\/");
            }
        }

        if (show_text) {
            if (i == 1)      ptr += sprintf(buffer + ptr, "    7: НАЛЕВО 9: НАПРАВО");
            else if (i == 2) ptr += sprintf(buffer + ptr, "        8:ПОВОРОТ");
            else if (i == 3) ptr += sprintf(buffer + ptr, "    4:УСКОРИТЬ 5:СБРОСИТЬ");
            else if (i == 4) ptr += sprintf(buffer + ptr, "    1: ПОКАЗАТЬ СЛЕДУЮЩУЮ");
            else if (i == 5) ptr += sprintf(buffer + ptr, "    0: СТЕРЕТЬ ЭТОТ ТЕКСТ");
            else if (i == 6) ptr += sprintf(buffer + ptr, "    ПРОБЕЛ - СБРОСИТЬ");
        }

        ptr += sprintf(buffer + ptr, "\033[K\n");
    }

    ptr += sprintf(buffer + ptr, "%s", RESET);
    fputs(buffer, stdout);
    fflush(stdout);
}

void draw_game_over_score_screen(const char *name, int final_level, int final_score) {
    char buffer[4096];
    int ptr = 0;
    ptr += sprintf(buffer + ptr, "\033[H%s", GREEN);
    ptr += sprintf(buffer + ptr, "\nИМЯ       УРОВЕНЬ    СЧЕТ\n");
    int display_final_score = final_score % 1000;
    ptr += sprintf(buffer + ptr, "%-10s %5d     %5d\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n", name, final_level, display_final_score);
    ptr += sprintf(buffer + ptr, "ЕЩЕ ПАРТИЮ? (ДА/НЕТ) - _");
    ptr += sprintf(buffer + ptr, "%s", RESET);
    fputs(buffer, stdout);
    fflush(stdout);
}

int run_game() {
    memset(board, 0, sizeof(board));
    score = 0;
    lines_cleared = 0;
    level = start_level;
    game_over = 0;
    next_shape = rand() % 7;
    
    animate_board_reveal();

    spawn_piece();

    int drop_counter = 0;

    while (!game_over) {
        draw_game();

        if (kbhit()) {
            char c = getchar();
            if (c == 'q' || c == 'Q') return 0;
            
            if (c == '\033') {
                char next1 = getchar();
                if (next1 == 'O' || next1 == '[') {
                    char next2 = getchar();
                    if (next2 == 'p' || next2 == 'M') c = '0';
                    else if (next2 == 'q' || next2 == 'Q') c = '1';
                    else if (next2 == 's' || next2 == 'S') c = '4';
                    else if (next2 == 't' || next2 == 'T') c = '5';
                    else if (next2 == 'u' || next2 == 'U') c = '7';
                    else if (next2 == 'r' || next2 == 'R') c = '8';
                    else if (next2 == 'v' || next2 == 'V') c = '9';
                }
            }

            if (c == '7') {
                if (!check_collision(current_rotation, cur_x - 1, cur_y)) cur_x--;
            } else if (c == '9') {
                if (!check_collision(current_rotation, cur_x + 1, cur_y)) cur_x++;
            } else if (c == '8') {
                int next_state = (current_rotation_state + 1) % 4;
                int test_rot[4][4];
                copy_shape(shapes[current_shape][next_state], test_rot);

                if (!check_collision(test_rot, cur_x, cur_y)) {
                    current_rotation_state = next_state;
                    copy_shape(test_rot, current_rotation);
                } else if (!check_collision(test_rot, cur_x - 1, cur_y)) {
                    cur_x--;
                    current_rotation_state = next_state;
                    copy_shape(test_rot, current_rotation);
                } else if (!check_collision(test_rot, cur_x + 1, cur_y)) {
                    cur_x++;
                    current_rotation_state = next_state;
                    copy_shape(test_rot, current_rotation);
                } else if (!check_collision(test_rot, cur_x - 2, cur_y)) {
                    cur_x -= 2;
                    current_rotation_state = next_state;
                    copy_shape(test_rot, current_rotation);
                } else if (!check_collision(test_rot, cur_x + 2, cur_y)) {
                    cur_x += 2;
                    current_rotation_state = next_state;
                    copy_shape(test_rot, current_rotation);
                }
            } else if (c == '4') {
                if (!check_collision(current_rotation, cur_x, cur_y + 1)) cur_y++;
            } else if (c == '5' || c == ' ') {
                while (!check_collision(current_rotation, cur_x, cur_y + 1)) {
                    cur_y++;
                }
                merge_piece();
                remove_lines();
                spawn_piece();
                drop_counter = 0;
            } else if (c == '1') {
                show_next = !show_next;
            } else if (c == '0') {
                show_text = !show_text;
            }
        }

        drop_counter++;
        int speed_threshold = 35 - (level * 3);
        if (speed_threshold < 4) speed_threshold = 4;

        if (drop_counter >= speed_threshold) {
            if (!check_collision(current_rotation, cur_x, cur_y + 1)) {
                cur_y++;
            } else {
                merge_piece();
                remove_lines();
                spawn_piece();
            }
            drop_counter = 0;
        }

        usleep(30000);
    }

    memset(player_name, 0, sizeof(player_name));
    int name_len = 0;
    while (1) {
        draw_game_over_name_prompt(player_name);
        if (kbhit()) {
            char c = getchar();
            if (c == '\n' || c == '\r') {
                if (name_len == 0) strcpy(player_name, "Ы");
                break;
            } else if (c == 127 || c == '\b') {
                if (name_len > 0) {
                    name_len--;
                    player_name[name_len] = '\0';
                }
            } else if (c >= 32 && c <= 126 && name_len < 10) {
                player_name[name_len++] = c;
                player_name[name_len] = '\0';
            }
        }
        usleep(30000);
    }

    printf("\033[2J");
    while (1) {
        draw_game_over_score_screen(player_name, level, score);
        if (kbhit()) {
            char c = getchar();
            if (c == 'y' || c == 'Y') return 1;
            if (c == 'n' || c == 'N' || c == 'q' || c == 'Q') return 0;
        }
        usleep(30000);
    }
}

int main() {
    srand(time(NULL));
    set_noncanonical_mode();

    int menu_active = 1;
    char typed = '\0';

    while (menu_active) {
        draw_menu(typed);
        if (kbhit()) {
            char c = getchar();
            if (c == 'q' || c == 'Q') return 0;
            if (c >= '0' && c <= '9') {
                typed = c;
                draw_menu(typed);
                usleep(400000);
                start_level = c - '0';
                menu_active = 0;
            }
        }
        usleep(30000);
    }

    printf("\033[2J");
    while (1) {
        int play_again = run_game();
        if (!play_again) break;
        printf("\033[2J");
    }
    return 0;
}
