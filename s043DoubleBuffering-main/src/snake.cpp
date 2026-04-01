#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <windows.h>
#include <conio.h>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

using namespace std;

constexpr int mWidth = 40;
constexpr int mHeight = 20;
constexpr int MAX_TAIL = 256;

HANDLE hOutput1 = INVALID_HANDLE_VALUE;
HANDLE hOutput2 = INVALID_HANDLE_VALUE;
COORD coord = {0, 0};
DWORD bytesWritten = 0;
bool bufferSwapFlag = false;
char ScreenData[mHeight + 6][mWidth + 6];

enum class eDirection { STOP = 0, LEFT, RIGHT, UP, DOWN };
enum class GameState { MENU, PLAYING, PAUSED, GAME_OVER, EXIT };
GameState gameState = GameState::MENU;

int headX = 0, headY = 0;
int fruitX = 0, fruitY = 0;
int score = 0;
int tailX[MAX_TAIL];
int tailY[MAX_TAIL];
int nTail = 0;
eDirection dir = eDirection::STOP;

const char* directionToString(eDirection d) {
    switch (d) {
        case eDirection::LEFT: return "LEFT";
        case eDirection::RIGHT: return "RIGHT";
        case eDirection::UP: return "UP";
        case eDirection::DOWN: return "DOWN";
        default: return "STOP";
    }
}

eDirection stringToDirection(const string& s) {
    if (s == "LEFT") return eDirection::LEFT;
    if (s == "RIGHT") return eDirection::RIGHT;
    if (s == "UP") return eDirection::UP;
    if (s == "DOWN") return eDirection::DOWN;
    return eDirection::STOP;
}

bool isOpposite(eDirection a, eDirection b) {
    return (a == eDirection::LEFT && b == eDirection::RIGHT) ||
           (a == eDirection::RIGHT && b == eDirection::LEFT) ||
           (a == eDirection::UP && b == eDirection::DOWN) ||
           (a == eDirection::DOWN && b == eDirection::UP);
}

int extractInt(const string& src, const string& key, int defaultValue = 0) {
    auto pos = src.find(key);
    if (pos == string::npos) return defaultValue;
    pos = src.find(':', pos);
    if (pos == string::npos) return defaultValue;
    pos++;
    while (pos < src.size() && (isspace((unsigned char)src[pos]) || src[pos] == '"' || src[pos] == ',' || src[pos] == '{' || src[pos] == '[')) pos++;
    if (pos >= src.size() || (!isdigit((unsigned char)src[pos]) && src[pos] != '-')) return defaultValue;
    size_t end = pos;
    if (src[end] == '-') end++;
    while (end < src.size() && isdigit((unsigned char)src[end])) end++;
    if (end == pos) return defaultValue;
    return stoi(src.substr(pos, end - pos));
}

string extractString(const string& src, const string& key, const string& defaultValue = "") {
    auto pos = src.find(key);
    if (pos == string::npos) return defaultValue;
    pos = src.find(':', pos);
    if (pos == string::npos) return defaultValue;
    pos = src.find('"', pos);
    if (pos == string::npos) return defaultValue;
    auto end = src.find('"', pos + 1);
    if (end == string::npos) return defaultValue;
    return src.substr(pos + 1, end - pos - 1);
}

void initConsoleBuffers() {
    if (hOutput1 != INVALID_HANDLE_VALUE) CloseHandle(hOutput1);
    if (hOutput2 != INVALID_HANDLE_VALUE) CloseHandle(hOutput2);
    hOutput1 = CreateConsoleScreenBuffer(GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    hOutput2 = CreateConsoleScreenBuffer(GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    CONSOLE_CURSOR_INFO cci;
    cci.bVisible = FALSE;
    cci.dwSize = 1;
    SetConsoleCursorInfo(hOutput1, &cci);
    SetConsoleCursorInfo(hOutput2, &cci);
    SetConsoleActiveScreenBuffer(hOutput1);
}

void restoreDefaultBuffer() {
    SetConsoleActiveScreenBuffer(GetStdHandle(STD_OUTPUT_HANDLE));
    if (hOutput1 != INVALID_HANDLE_VALUE) CloseHandle(hOutput1);
    if (hOutput2 != INVALID_HANDLE_VALUE) CloseHandle(hOutput2);
    hOutput1 = hOutput2 = INVALID_HANDLE_VALUE;
}

void spawnFruit() {
    while (true) {
        int fx = rand() % (mWidth - 2) + 1;
        int fy = rand() % (mHeight - 2) + 1;
        if (fx == headX && fy == headY) continue;
        bool coll = false;
        for (int i = 0; i < nTail; i++) if (tailX[i] == fx && tailY[i] == fy) { coll = true; break; }
        if (!coll) { fruitX = fx; fruitY = fy; return; }
    }
}

void resetGame() {
    dir = eDirection::STOP;
    score = 0;
    nTail = 0;
    headX = mWidth / 2;
    headY = mHeight / 2;
    for (int i = 0; i < MAX_TAIL; i++) { tailX[i] = 0; tailY[i] = 0; }
    srand(static_cast<unsigned>(time(nullptr)));
    spawnFruit();
}

void drawFrame(bool showHint = false) {
    int row = 0;
    for (int j = 0; j < mWidth; j++) ScreenData[row][j] = '#';
    row++;
    for (int y = 1; y < mHeight - 1; y++) {
        ScreenData[row][0] = '#';
        for (int x = 1; x < mWidth - 1; x++) ScreenData[row][x] = ' ';
        ScreenData[row][mWidth - 1] = '#';
        row++;
    }
    for (int j = 0; j < mWidth; j++) ScreenData[row][j] = '#';
    row++;
    string info = "Score: " + to_string(score) + " Dir: " + directionToString(dir);
    for (int i = 0; i < mWidth; i++) ScreenData[row][i] = i < (int)info.size() ? info[i] : ' ';
    row++;
    string hint = showHint ? "PAUSED: [P]resume [M]menu [Q]quit [S]save" : "[P]pause [S]save [Q]quit";
    for (int i = 0; i < mWidth; i++) ScreenData[row][i] = i < (int)hint.size() ? hint[i] : ' ';
    if (fruitX > 0 && fruitX < mWidth - 1 && fruitY > 0 && fruitY < mHeight - 1) ScreenData[fruitY][fruitX] = 'F';
    if (headX > 0 && headX < mWidth - 1 && headY > 0 && headY < mHeight - 1) ScreenData[headY][headX] = 'O';
    for (int i = 0; i < nTail; i++) {
        int tx = tailX[i], ty = tailY[i];
        if (tx > 0 && tx < mWidth - 1 && ty > 0 && ty < mHeight - 1) ScreenData[ty][tx] = 'o';
    }
}

void displayBuffer() {
    int totalRows = mHeight + 2;
    HANDLE wall = bufferSwapFlag ? hOutput2 : hOutput1;
    for (int i = 0; i < totalRows; i++) {
        coord.X = 0;
        coord.Y = i;
        WriteConsoleOutputCharacterA(wall, ScreenData[i], mWidth, coord, &bytesWritten);
    }
    SetConsoleActiveScreenBuffer(wall);
    bufferSwapFlag = !bufferSwapFlag;
}

void showMenuScreen() {
    restoreDefaultBuffer();
    system("cls");
    cout << "##########################################\n";
    cout << "#      控制台贪吃蛇（双缓冲实现）       #\n";
    cout << "##########################################\n";
    cout << "N: 新游戏  L: 读取存档  Q: 退出\n";
    cout << "方向键或 WSAD 控制，P 暂停，S 保存，M 菜单\n";
    cout << "请选择: ";
}

void showPlayingScreen(bool paused = false) {
    drawFrame(paused);
    displayBuffer();
}

void showGameOverScreen() {
    drawFrame(false);
    string msg = "GAME OVER! Score:" + to_string(score) + " [R]重开 [M]菜单 [Q]退出 [S]存档";
    int row = mHeight + 1;
    for (int i = 0; i < mWidth; i++) ScreenData[row][i] = i < (int)msg.size() ? msg[i] : ' ';
    displayBuffer();
}

bool saveGame(const string& path) {
    ofstream ofs(path);
    if (!ofs) return false;
    ofs << "{\n";
    ofs << "  \"head\": {\"x\": " << headX << ", \"y\": " << headY << "},\n";
    ofs << "  \"dir\": \"" << directionToString(dir) << "\",\n";
    ofs << "  \"fruit\": {\"x\": " << fruitX << ", \"y\": " << fruitY << "},\n";
    ofs << "  \"score\": " << score << ",\n";
    ofs << "  \"tail\": [\n";
    for (int i = 0; i < nTail; i++) {
        ofs << "    {\"x\": " << tailX[i] << ", \"y\": " << tailY[i] << "}";
        if (i < nTail - 1) ofs << ",";
        ofs << "\n";
    }
    ofs << "  ]\n";
    ofs << "}\n";
    return true;
}

bool loadGame(const string& path) {
    if (!filesystem::exists(path)) return false;
    ifstream ifs(path);
    if (!ifs) return false;
    string content;
    {
        ostringstream os;
        os << ifs.rdbuf();
        content = os.str();
    }
    int hx = extractInt(content, "\"head\"", headX);
    int hy = extractInt(content, "\"head\"", headY);
    string d = extractString(content, "\"dir\"");
    int fx = extractInt(content, "\"fruit\"", fruitX);
    int fy = extractInt(content, "\"fruit\"", fruitY);
    int sc = extractInt(content, "\"score\"", score);
    if (hx <= 0 || hx >= mWidth - 1 || hy <= 0 || hy >= mHeight - 1) return false;
    headX = hx;
    headY = hy;
    dir = stringToDirection(d);
    fruitX = fx;
    fruitY = fy;
    score = sc;
    size_t tailPos = content.find("\"tail\"");
    int loadedTail = 0;
    if (tailPos != string::npos) {
        size_t arrayStart = content.find('[', tailPos);
        size_t arrayEnd = content.find(']', arrayStart);
        if (arrayStart != string::npos && arrayEnd != string::npos) {
            size_t pos = arrayStart + 1;
            while (pos < arrayEnd && loadedTail < MAX_TAIL) {
                size_t xKey = content.find("\"x\"", pos);
                if (xKey == string::npos || xKey > arrayEnd) break;
                int tx = extractInt(content.substr(xKey, arrayEnd - xKey), "\"x\"");
                size_t yKey = content.find("\"y\"", xKey);
                if (yKey == string::npos || yKey > arrayEnd) break;
                int ty = extractInt(content.substr(yKey, arrayEnd - yKey), "\"y\"");
                tailX[loadedTail] = tx;
                tailY[loadedTail] = ty;
                loadedTail++;
                pos = yKey + 1;
            }
        }
    }
    nTail = loadedTail;
    for (int i = nTail; i < MAX_TAIL; i++) { tailX[i] = 0; tailY[i] = 0; }
    return true;
}

void updateLogic() {
    if (dir == eDirection::STOP) return;
    int prevX = tailX[0];
    int prevY = tailY[0];
    if (nTail > 0) {
        tailX[0] = headX;
        tailY[0] = headY;
        for (int i = 1; i < nTail; i++) {
            int prev2X = tailX[i];
            int prev2Y = tailY[i];
            tailX[i] = prevX;
            tailY[i] = prevY;
            prevX = prev2X;
            prevY = prev2Y;
        }
    }
    switch (dir) {
        case eDirection::LEFT: headX--; break;
        case eDirection::RIGHT: headX++; break;
        case eDirection::UP: headY--; break;
        case eDirection::DOWN: headY++; break;
        default: break;
    }
    if (headX <= 0 || headX >= mWidth - 1 || headY <= 0 || headY >= mHeight - 1) { gameState = GameState::GAME_OVER; return; }
    for (int i = 0; i < nTail; i++) if (tailX[i] == headX && tailY[i] == headY) { gameState = GameState::GAME_OVER; return; }
    if (headX == fruitX && headY == fruitY) {
        score += 10;
        if (nTail < MAX_TAIL) nTail++;
        spawnFruit();
    }
}

void processInput() {
    if (!_kbhit()) return;
    int ch = _getch();
    int key = -1;
    bool special = (ch == 0 || ch == 224);
    if (special) key = _getch();
    auto applyDir = [&](eDirection newDir) { if (dir == eDirection::STOP || !isOpposite(dir, newDir)) dir = newDir; };
    if (gameState == GameState::MENU) {
        if (ch == 'n' || ch == 'N') { resetGame(); gameState = GameState::PLAYING; initConsoleBuffers(); return; }
        if (ch == 'l' || ch == 'L') {
            if (loadGame("savegame.json")) {
                gameState = GameState::PLAYING;
                initConsoleBuffers();
            } else {
                restoreDefaultBuffer();
                cout << "Load failed. 按任意键继续...";
                _getch();
                showMenuScreen();
            }
            return;
        }
        if (ch == 'q' || ch == 'Q') { gameState = GameState::EXIT; return; }
    } else if (gameState == GameState::PLAYING) {
        if (special) {
            if (key == 75) applyDir(eDirection::LEFT);
            if (key == 77) applyDir(eDirection::RIGHT);
            if (key == 72) applyDir(eDirection::UP);
            if (key == 80) applyDir(eDirection::DOWN);
            return;
        }
        if (ch == 'a' || ch == 'A') applyDir(eDirection::LEFT);
        if (ch == 'd' || ch == 'D') applyDir(eDirection::RIGHT);
        if (ch == 'w' || ch == 'W') applyDir(eDirection::UP);
        if (ch == 's' || ch == 'S') applyDir(eDirection::DOWN);
        if (ch == 'p' || ch == 'P') { gameState = GameState::PAUSED; return; }
        if (ch == 'q' || ch == 'Q') { gameState = GameState::EXIT; return; }
        if (ch == 'x' || ch == 'X') { gameState = GameState::GAME_OVER; return; }
        if (ch == 's' || ch == 'S') { saveGame("savegame.json"); return; }
    } else if (gameState == GameState::PAUSED) {
        if (ch == 'p' || ch == 'P') { gameState = GameState::PLAYING; return; }
        if (ch == 'm' || ch == 'M') { gameState = GameState::MENU; restoreDefaultBuffer(); showMenuScreen(); return; }
        if (ch == 'q' || ch == 'Q') { gameState = GameState::EXIT; return; }
        if (ch == 's' || ch == 'S') { saveGame("savegame.json"); return; }
    } else if (gameState == GameState::GAME_OVER) {
        if (ch == 'r' || ch == 'R') { resetGame(); gameState = GameState::PLAYING; initConsoleBuffers(); return; }
        if (ch == 'm' || ch == 'M') { gameState = GameState::MENU; restoreDefaultBuffer(); showMenuScreen(); return; }
        if (ch == 'q' || ch == 'Q') { gameState = GameState::EXIT; return; }
        if (ch == 's' || ch == 'S') { saveGame("savegame.json"); return; }
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleA("Console Double Buffering Snake");
    resetGame();
    showMenuScreen();
    while (gameState != GameState::EXIT) {
        if (gameState == GameState::MENU) { processInput(); Sleep(50); continue; }
        if (gameState == GameState::PLAYING) { showPlayingScreen(false); processInput(); updateLogic(); Sleep(100); continue; }
        if (gameState == GameState::PAUSED) { showPlayingScreen(true); processInput(); Sleep(100); continue; }
        if (gameState == GameState::GAME_OVER) { showGameOverScreen(); processInput(); Sleep(100); continue; }
    }
    restoreDefaultBuffer();
    cout << "\nThanks for playing!\n";
    return 0;
}
