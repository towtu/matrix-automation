#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include <cmath>
#include <algorithm>
#include <map>

using namespace std;

// ==========================================
// SHARED DATA TYPES
// ==========================================

enum TokenType { LBRACKET, RBRACKET, COMMA, PLUS, MINUS, MULTIPLY, NUMBER, UNKNOWN, END_TOKEN, NONE_TOKEN };
struct Token { TokenType type; string value; };

// States
enum NFAState { S_NONE, S0, S1, S2, S3, S4, S_FINAL };
enum DFAState { D_NONE, D_START, D_ACCEPT };
enum AnimMode { MODE_NONE, MODE_NFA, MODE_DFA };

// ==========================================
// PART 1: LEXER (NFA -> DFA Sequence)
// ==========================================

class Lexer {
public:
    string input;
    int pos;
    AnimMode mode = MODE_NONE;
    NFAState nfaState = S_NONE;
    NFAState nfaTarget = S_NONE;
    int nfaStep = 0; 
    DFAState dfaState = D_NONE;
    int dfaIdx = 0;
    string currentNumBuild = "";
    Token readyToken = {NONE_TOKEN, ""}; 
    bool finished = false;

    void init(string s) { 
        input = s; pos = 0; mode = MODE_NONE; 
        readyToken = {NONE_TOKEN, ""}; 
        finished = false;
    }
    
    char peek(int offset = 0) { if (pos + offset >= input.length()) return 0; return input[pos + offset]; }

    bool step() {
        if (readyToken.type != NONE_TOKEN) return false; 
        if (finished) return false;
        
        if (mode == MODE_NONE) {
            char c = peek();
            while (pos < input.length() && isspace(input[pos])) { pos++; c = peek(); }
            if (pos >= input.length()) { readyToken = {END_TOKEN, "EOF"}; finished = true; return false; }
            
            if (isdigit(c)) { 
                mode = MODE_NFA; nfaState = S0; nfaStep = 0; currentNumBuild = ""; return true; 
            }
            pos++;
            if (c == '[') readyToken = {LBRACKET, "["};
            else if (c == ']') readyToken = {RBRACKET, "]"};
            else if (c == ',') readyToken = {COMMA, ","};
            else if (c == '+') readyToken = {PLUS, "+"};
            else if (c == '-') readyToken = {MINUS, "-"};
            else if (c == '*') readyToken = {MULTIPLY, "*"};
            else readyToken = {UNKNOWN, string(1, c)};
            return false;
        }

        if (mode == MODE_NFA) {
            char c = peek();
            switch (nfaStep) {
                case 0: nfaState = S0; nfaTarget = S1; currentNumBuild += c; pos++; nfaStep = 1; break;
                case 1: nfaState = S1; nfaTarget = S2; nfaStep = 2; break;
                case 2: nfaState = S2; if (isdigit(peek())) { nfaTarget = S3; nfaStep = 3; } else { nfaTarget = S_FINAL; nfaStep = 5; } break;
                case 3: nfaState = S3; nfaTarget = S4; currentNumBuild += peek(); pos++; nfaStep = 4; break;
                case 4: nfaState = S4; nfaTarget = S2; nfaStep = 2; break;
                case 5: nfaState = S_FINAL; nfaTarget = S_NONE; mode = MODE_DFA; dfaState = D_START; dfaIdx = 0; return true;
            }
            return true;
        }

        if (mode == MODE_DFA) {
            if (dfaIdx <= currentNumBuild.length()) {
                if (dfaState == D_START) {
                    if (dfaIdx < currentNumBuild.length()) { dfaState = D_ACCEPT; dfaIdx++; return true; }
                }
                else if (dfaState == D_ACCEPT) {
                    if (dfaIdx < currentNumBuild.length()) { dfaIdx++; return true; }
                    else { mode = MODE_NONE; dfaState = D_NONE; readyToken = {NUMBER, currentNumBuild}; return false; }
                }
            }
        }
        return false;
    }
};

// ==========================================
// PART 2: PDA
// ==========================================

struct LogEntry { string input, action, stackState; };

class ParserEngine {
public:
    stack<string> pdaStack;
    Lexer lexer;
    vector<Token> tokenStream;
    int tokenCursor = 0;
    bool lexingPhase = true; 
    bool isLocked = false, isFinished = false;
    
    int expectedRowLength = -1, currentRowLength = 0;
    bool inRow = false;
    int matrix1Cols = -1; 
    
    string statusMessage, lastAction, lastOperation = ""; 
    vector<string> justPushed; 
    vector<LogEntry> history; 

    void reset(string input) {
        while(!pdaStack.empty()) pdaStack.pop();
        pdaStack.push("$"); pdaStack.push("S");
        lexer.init(input); tokenStream.clear(); tokenCursor = 0;
        lexingPhase = true; isLocked = false; isFinished = false;
        expectedRowLength = -1; currentRowLength = 0; inRow = false; matrix1Cols = -1; 
        statusMessage = "Phase 1: Lexing"; lastAction = "Init"; lastOperation = "";
        justPushed.clear(); history.clear(); addLog("Init");
    }

    void triggerError(string msg) { statusMessage = "ERROR: " + msg; lastAction = "STOPPED"; isLocked = true; addLog("ERROR: " + msg); }
    void pushStack(vector<string> items) {
        lastOperation = "PUSH " + to_string(items.size()); justPushed = items;
        for (const string& s : items) pdaStack.push(s);
        addLog("PUSH " + to_string(items.size()) + " Rules");
    }
    void addLog(string act) {
        string s = "";
        if (pdaStack.empty()) s = "empty";
        else {
            stack<string> t = pdaStack; vector<string> v;
            while(!t.empty()){ v.push_back(t.top()); t.pop(); }
            for(int i=v.size()-1; i>=0; i--) s += v[i] + " ";
        }
        string inStr = (lexingPhase) ? "LEX" : (tokenCursor < tokenStream.size() ? tokenStream[tokenCursor].value : "EOF");
        history.push_back({inStr, act, s});
    }

    void step() {
        justPushed.clear(); lastOperation = "";
        if (isLocked || isFinished) return;
        
        // --- PHASE 1: LEXING ---
        if (lexingPhase) {
            if (lexer.readyToken.type != NONE_TOKEN) {
                tokenStream.push_back(lexer.readyToken);
                lastAction = "Lexer: Generated " + lexer.readyToken.value;
                addLog("Token: " + lexer.readyToken.value);
                if (lexer.readyToken.type == END_TOKEN) {
                    lexingPhase = false; 
                    statusMessage = "Phase 2: Parsing (PDA)";
                    lastAction = "Lexing Done. Starting PDA.";
                }
                lexer.readyToken = {NONE_TOKEN, ""}; 
                return;
            }
            bool busy = lexer.step();
            if (busy) {
                if (lexer.mode == MODE_NFA) lastAction = "Lexer: 1. NFA Running...";
                else if (lexer.mode == MODE_DFA) lastAction = "Lexer: 2. DFA Verifying...";
            } 
            return;
        }

        // --- PHASE 2: PARSING ---
        if (pdaStack.empty()) return;
        string top = pdaStack.top();
        Token currentToken = tokenStream[tokenCursor];

        if (top == "$") {
            if (currentToken.type == END_TOKEN) { 
                statusMessage = "ACCEPTED"; lastAction = "Done"; isFinished = true; pdaStack.pop(); addLog("ACCEPTED"); return; 
            } else { triggerError("Trailing characters found"); return; }
        }

        bool isTerminal = (top == "[" || top == "]" || top == "," || top == "+" || top == "-" || top == "*" || top == "num");
        
        if (isTerminal) {
            bool match = false;
            if (top == "[" && currentToken.type == LBRACKET) match = true;
            else if (top == "]" && currentToken.type == RBRACKET) match = true;
            else if (top == "," && currentToken.type == COMMA) match = true;
            else if (top == "+" && currentToken.type == PLUS) match = true;
            else if (top == "-" && currentToken.type == MINUS) match = true;
            else if (top == "*" && currentToken.type == MULTIPLY) match = true;
            else if (top == "num" && currentToken.type == NUMBER) match = true;

            if (match) {
                // --- STRICT SEMANTIC CHECKS ---
                if (top == "num" && inRow) {
                    currentRowLength++;
                }
                else if (top == "]" && inRow) {
                    // Check 1: Minimum Size (1x1 not allowed)
                    if (currentRowLength < 2) {
                         triggerError("Invalid Matrix: 1x1 not allowed");
                         return;
                    }

                    // Check 2: Matrix 2 vs Matrix 1
                    if (matrix1Cols != -1) {
                         if (currentRowLength != matrix1Cols) {
                             triggerError("Dimension Mismatch! Matrix 1=" + to_string(matrix1Cols) + ", Matrix 2=" + to_string(currentRowLength));
                             return;
                         }
                    }

                    // Check 3: Row Consistency
                    if (expectedRowLength == -1) { 
                        expectedRowLength = currentRowLength; 
                        addLog("Set Dim: " + to_string(expectedRowLength)); 
                    } 
                    else {
                        if (currentRowLength != expectedRowLength) { 
                            triggerError("Row Mismatch! Exp " + to_string(expectedRowLength) + ", Got " + to_string(currentRowLength)); 
                            return; 
                        }
                    }
                    
                    currentRowLength = 0; inRow = false;
                }
                else if (top == "+" || top == "-" || top == "*") { 
                    if (expectedRowLength != -1) {
                        matrix1Cols = expectedRowLength;
                        addLog("Locked Matrix 1 Dim: " + to_string(matrix1Cols));
                    }
                    expectedRowLength = -1; currentRowLength = 0; inRow = false; 
                }
                
                lastAction = "PDA: Matched " + top; lastOperation = "POP & MATCH";
                pdaStack.pop(); tokenCursor++; addLog("Match " + top);
            } else { triggerError("Expected " + top); }
        } else {
            pdaStack.pop();
            if (top == "S") { pushStack({"M", "OP", "M"}); }
            else if (top == "OP") {
                if (currentToken.type == PLUS) pushStack({"+"});
                else if (currentToken.type == MINUS) pushStack({ "-"});
                else if (currentToken.type == MULTIPLY) pushStack({"*"});
                else triggerError("Expected OP");
            }
            else if (top == "M") { pushStack({"Core", "S_OPT"}); }
            else if (top == "S_OPT") { if (currentToken.type == NUMBER) pushStack({"num"}); else addLog("Epsilon"); }
            else if (top == "Core") { if (currentToken.type == LBRACKET) { pushStack({"]", "Inside", "["}); } else triggerError("Exp ["); }
            else if (top == "Inside") { 
                if (currentToken.type == LBRACKET) pushStack({"RowList"}); 
                else if (currentToken.type == NUMBER) {
                    pushStack({"NumList"}); 
                    // FIX: START COUNTING FOR 1D MATRICES TOO!
                    inRow = true; 
                    currentRowLength = 0; 
                } 
                else triggerError("Invalid"); 
            }
            else if (top == "RowList") { pushStack({"RowTail", "Row"}); }
            else if (top == "Row") { 
                if (currentToken.type == LBRACKET) { 
                    pushStack({"]", "NumList", "["}); 
                    inRow = true; 
                    currentRowLength = 0; 
                } else triggerError("Row needs ["); 
            }
            else if (top == "RowTail") { if (currentToken.type == COMMA) { pushStack({"RowList", ","}); } else addLog("Epsilon"); }
            else if (top == "NumList") { if (currentToken.type == NUMBER) { pushStack({"NumTail", "num"}); } else triggerError("Exp Num"); }
            else if (top == "NumTail") { if (currentToken.type == COMMA) { pushStack({"NumList", ","}); } else addLog("Epsilon"); }
        }
    }
};

ParserEngine engine;
char inputBuffer[256] = "[10,20]+[30,40]"; 

// ==========================================
// RENDER HELPERS
// ==========================================
void DrawArrow(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col) {
    dl->AddLine(p1, p2, col, 2.0f);
    float angle = atan2(p2.y - p1.y, p2.x - p1.x); float sz = 10.0f;
    dl->AddTriangleFilled(p2, ImVec2(p2.x-sz*cos(angle-0.5), p2.y-sz*sin(angle-0.5)), ImVec2(p2.x-sz*cos(angle+0.5), p2.y-sz*sin(angle+0.5)), col);
}
void DrawNode(ImDrawList* dl, ImVec2 pos, string label, int isActive, bool isFinal) {
    ImU32 col = isActive ? IM_COL32(255, 140, 0, 255) : IM_COL32(200, 200, 200, 255);
    dl->AddCircleFilled(pos, 20.0f, col); dl->AddCircle(pos, 20.0f, IM_COL32(0,0,0,255), 0, 2.0f);
    if(isFinal) dl->AddCircle(pos, 16.0f, IM_COL32(0,0,0,255), 0, 2.0f);
    ImVec2 txtSz = ImGui::CalcTextSize(label.c_str());
    dl->AddText(ImVec2(pos.x - txtSz.x/2, pos.y - txtSz.y/2), IM_COL32(0,0,0,255), label.c_str());
}
void DrawSelfLoop(ImDrawList* dl, ImVec2 pos, ImU32 col) {
    dl->AddBezierCubic(ImVec2(pos.x-10, pos.y-20), ImVec2(pos.x-30, pos.y-60), ImVec2(pos.x+30, pos.y-60), ImVec2(pos.x+10, pos.y-20), col, 2.0f);
    dl->AddTriangleFilled(ImVec2(pos.x+10, pos.y-20), ImVec2(pos.x+15, pos.y-28), ImVec2(pos.x+20, pos.y-22), col);
}

void RenderNFA() {
    ImGui::Begin("Part 1 & 2: Lexical (NFA then DFA)", NULL);
    ImGui::Dummy(ImVec2(600, 320)); 
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos(); 
    float offX = p.x + 20, offY = p.y + 40;
    ImGui::SetCursorPos(ImVec2(20, 25)); ImGui::Text("1. Thompson's NFA (Creation)");
    NFAState nS = (engine.lexer.mode == MODE_NFA) ? engine.lexer.nfaState : S_NONE;
    NFAState nT = (engine.lexer.mode == MODE_NFA) ? engine.lexer.nfaTarget : S_NONE;
    auto NC = [&](NFAState s, NFAState d) { return (nS == s && nT == d) ? IM_COL32(255, 100, 0, 255) : IM_COL32(100, 100, 100, 255); };
    ImVec2 n0(offX+30, offY+80), n1(offX+110, offY+80), n2(offX+190, offY+80), n3(offX+270, offY+40), n4(offX+350, offY+40), nF(offX+430, offY+80);
    DrawArrow(dl, ImVec2(n0.x+20, n0.y), ImVec2(n1.x-20, n1.y), NC(S0, S1));
    DrawArrow(dl, ImVec2(n1.x+20, n1.y), ImVec2(n2.x-20, n2.y), NC(S1, S2));
    dl->AddBezierQuadratic(ImVec2(n2.x+15, n2.y-15), ImVec2(n2.x+50, n2.y-60), ImVec2(n3.x-20, n3.y), NC(S2, S3), 2.0f);
    DrawArrow(dl, ImVec2(n3.x+20, n3.y), ImVec2(n4.x-20, n4.y), NC(S3, S4));
    dl->AddBezierQuadratic(ImVec2(n4.x-5, n4.y+20), ImVec2(n3.x+50, n3.y+80), ImVec2(n2.x+15, n2.y+15), NC(S4, S2), 2.0f);
    DrawArrow(dl, ImVec2(n2.x+20, n2.y), ImVec2(nF.x-20, nF.y), NC(S2, S_FINAL));
    DrawNode(dl, n0, "0", nS==S0, false); DrawNode(dl, n1, "1", nS==S1, false); DrawNode(dl, n2, "2", nS==S2, false);
    DrawNode(dl, n3, "3", nS==S3, false); DrawNode(dl, n4, "4", nS==S4, false); DrawNode(dl, nF, "F", nS==S_FINAL, true);
    float dOffY = offY + 140;
    ImGui::SetCursorPos(ImVec2(20, 160)); ImGui::Text("2. Optimized DFA (Verification)");
    DFAState dS = (engine.lexer.mode == MODE_DFA) ? engine.lexer.dfaState : D_NONE;
    ImU32 dAct = IM_COL32(255, 100, 0, 255); ImU32 dNorm = IM_COL32(100, 100, 100, 255);
    ImVec2 d0(offX+100, dOffY+50), d1(offX+300, dOffY+50);
    DrawArrow(dl, ImVec2(d0.x+20, d0.y), ImVec2(d1.x-20, d1.y), (dS == D_START) ? dAct : dNorm);
    DrawSelfLoop(dl, d1, (dS == D_ACCEPT) ? dAct : dNorm);
    DrawNode(dl, d0, "Start", dS==D_START, false); DrawNode(dl, d1, "Acc", dS==D_ACCEPT, true);
    ImGui::SetCursorPosY(300);
    if (engine.lexer.mode == MODE_NFA) ImGui::TextColored(ImVec4(1,0.5f,0,1), "Building: %s", engine.lexer.currentNumBuild.c_str());
    else if (engine.lexer.mode == MODE_DFA) ImGui::TextColored(ImVec4(0,0.5f,1,1), "Verifying: %s", engine.lexer.currentNumBuild.c_str());
    else ImGui::TextColored(ImVec4(0,0,0,0.5f), "Lexer Idle");
    ImGui::SetCursorPosY(330); ImGui::Separator(); ImGui::Text("Generated Tokens:");
    for (const auto& t : engine.tokenStream) { ImGui::SameLine(); ImGui::Button((t.type == NUMBER ? "NUM:" + t.value : t.value).c_str()); }
    ImGui::End();
}

void RenderPDA() {
    ImGui::Begin("Part 3: CFG Stack", NULL);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float y = p.y + 30;
    vector<string> sv; stack<string> temp = engine.pdaStack;
    while(!temp.empty()){ sv.push_back(temp.top()); temp.pop(); }
    if (!engine.lastOperation.empty()) { ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5); ImGui::TextColored(ImVec4(0,0,0.8f,1), "OP: %s", engine.lastOperation.c_str()); y += 25; }
    for (const string& item : sv) {
        ImU32 boxColor = IM_COL32(230, 230, 230, 255); 
        if (item == "[" || item == "]" || item == "," || item == "num" || item == "+" || item == "-") boxColor = IM_COL32(180, 255, 180, 255); 
        for(const string& pushed : engine.justPushed) { if(pushed == item) { boxColor = IM_COL32(255, 255, 150, 255); break; } }
        dl->AddRectFilled(ImVec2(p.x+10, y), ImVec2(p.x+150, y+25), boxColor); dl->AddRect(ImVec2(p.x+10, y), ImVec2(p.x+150, y+25), IM_COL32(0,0,0,255)); dl->AddText(ImVec2(p.x+20, y+5), IM_COL32(0, 0, 0, 255), item.c_str()); y += 30;
    }
    ImGui::Dummy(ImVec2(0, y - p.y + 20)); 
    ImGui::End();
}

void RenderTrace() {
    ImGui::Begin("Trace Log", NULL);
    if (ImGui::BeginTable("TraceTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthFixed, 50.0f); ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 150.0f); ImGui::TableSetupColumn("Stack State", ImGuiTableColumnFlags_WidthStretch); ImGui::TableHeadersRow();
        for (const auto& log : engine.history) { ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("%s", log.input.c_str()); ImGui::TableSetColumnIndex(1); ImGui::Text("%s", log.action.c_str()); ImGui::TableSetColumnIndex(2); ImGui::Text("%s", log.stackState.c_str()); }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndTable();
    }
    ImGui::End();
}

int main(int, char**) {
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(1200, 900, "Full Compiler Sequence Visualizer", NULL, NULL);
    if (window == NULL) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsLight(); 
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    engine.reset("[10,20]+[30,40]"); 
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImVec2(1200, 80));
        ImGui::Begin("Controls", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
        ImGui::Text("Expression:"); ImGui::SameLine(); ImGui::InputText("##Input", inputBuffer, 256); ImGui::SameLine();
        if (ImGui::Button("Reset / Load")) engine.reset(inputBuffer); ImGui::SameLine();
        bool disabled = engine.isLocked; if (disabled) ImGui::BeginDisabled();
        if (ImGui::Button("STEP >>", ImVec2(150, 40))) engine.step();
        if (disabled) ImGui::EndDisabled();
        if (engine.isFinished) ImGui::TextColored(ImVec4(0,0.8f,0,1), "RESULT: %s", engine.statusMessage.c_str());
        if (engine.isLocked && !engine.isFinished) ImGui::TextColored(ImVec4(1,0,0,1), "RESULT: %s", engine.statusMessage.c_str());
        ImGui::End();
        ImGui::SetNextWindowPos(ImVec2(0, 80)); ImGui::SetNextWindowSize(ImVec2(600, 400)); RenderNFA();
        ImGui::SetNextWindowPos(ImVec2(600, 80)); ImGui::SetNextWindowSize(ImVec2(600, 400)); RenderPDA();
        ImGui::SetNextWindowPos(ImVec2(0, 480)); ImGui::SetNextWindowSize(ImVec2(1200, 420)); RenderTrace();
        ImGui::Render();
        int dw, dh; glfwGetFramebufferSize(window, &dw, &dh); glViewport(0, 0, dw, dh); glClearColor(0.9f, 0.9f, 0.95f, 1.0f); glClear(GL_COLOR_BUFFER_BIT); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); glfwSwapBuffers(window);
    }
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); glfwDestroyWindow(window); glfwTerminate();
    return 0;
}