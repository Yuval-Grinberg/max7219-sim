// sketchgen — turns an Arduino sketch (.ino files) into one C++ file, the way the Arduino IDE does:
// all tabs are joined, function prototypes are added before the first function, and
// #line directives keep compiler errors pointing at your .ino files.
//
// usage: sketchgen OUT.cpp SKETCH_NAME MAIN.ino [OTHER.ino ...]
#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

static std::string readFile(const std::string &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) { std::cerr << "sketchgen: cannot read " << p << "\n"; exit(1); }
  std::stringstream ss;
  ss << f.rdbuf();
  std::string s = ss.str();
  if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s.erase(0, 3);
  std::string out;  // normalise CRLF
  for (size_t i = 0; i < s.size(); i++)
    if (!(s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n')) out += s[i];
  if (!out.empty() && out.back() != '\n') out += '\n';
  return out;
}

static std::string slashes(std::string p) {
  for (auto &c : p) if (c == '\\') c = '/';
  return p;
}

static std::string cEscape(const std::string &s) {
  std::string o;
  for (char c : s) {
    if (c == '\\' || c == '"') o += '\\';
    if (c == '\n') { o += "\\n"; continue; }
    o += c;
  }
  return o;
}

// Comments, string and char literals become spaces (newlines kept) so the scanner only sees code.
static std::string stripCode(const std::string &s, bool keepStrings = false) {
  std::string o = s;
  size_t i = 0, n = s.size();
  while (i < n) {
    if (s[i] == '/' && i + 1 < n && s[i + 1] == '/') {
      while (i < n && s[i] != '\n') o[i++] = ' ';
    } else if (s[i] == '/' && i + 1 < n && s[i + 1] == '*') {
      o[i] = o[i + 1] = ' ';
      i += 2;
      while (i < n && !(s[i] == '*' && i + 1 < n && s[i + 1] == '/')) { if (s[i] != '\n') o[i] = ' '; i++; }
      if (i < n) { o[i] = o[i + 1] = ' '; i += 2; }
    } else if (s[i] == '"' || s[i] == '\'') {
      char q = s[i];
      if (!keepStrings) o[i] = ' ';
      i++;
      while (i < n && s[i] != q && s[i] != '\n') {
        if (s[i] == '\\' && i + 1 < n) { if (!keepStrings) o[i] = ' '; i++; }
        if (!keepStrings) o[i] = ' ';
        i++;
      }
      if (i < n && s[i] == q) { if (!keepStrings) o[i] = ' '; i++; }
    } else {
      i++;
    }
  }
  return o;
}

static std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\n"), b = s.find_last_not_of(" \t\n");
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static std::string squeeze(const std::string &s) {
  std::string o;
  bool sp = false;
  for (char c : s) {
    if (c == ' ' || c == '\t' || c == '\n') { sp = true; continue; }
    if (sp && !o.empty()) o += ' ';
    sp = false;
    o += c;
  }
  return o;
}

// Ranges [a, b) of default values ("= ...") inside a parameter list (scanned on code without strings).
static std::vector<std::pair<size_t, size_t>> defaultRanges(const std::string &params) {
  std::vector<std::pair<size_t, size_t>> r;
  int depth = 0;
  size_t start = std::string::npos;
  for (size_t i = 0; i < params.size(); i++) {
    char c = params[i];
    if (c == '(' || c == '<' || c == '[' || c == '{') depth++;
    if (c == ')' || c == '>' || c == ']' || c == '}') depth--;
    if (depth == 0 && c == ',' && start != std::string::npos) { r.push_back({start, i}); start = std::string::npos; }
    if (depth == 0 && c == '=' && start == std::string::npos) start = i;
  }
  if (start != std::string::npos) r.push_back({start, params.size()});
  return r;
}

// '=' outside any parentheses means this is an initialiser, not a function header.
static bool hasTopLevelAssign(const std::string &h) {
  int depth = 0;
  for (char c : h) {
    if (c == '(') depth++;
    if (c == ')') depth--;
    if (c == '=' && depth == 0) return true;
  }
  return false;
}

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cerr << "usage: sketchgen OUT.cpp SKETCH_NAME MAIN.ino [OTHER.ino ...]\n";
    return 1;
  }
  std::string outPath = argv[1], name = argv[2];

  // Join the tabs, remembering where each file starts.
  struct Part { std::string path; size_t start; };
  std::vector<Part> parts;
  std::string code;
  std::string simArgs;
  std::regex simLine(R"(^[ \t]*//[ \t]*sim:(.*)$)");
  for (int i = 3; i < argc; i++) {
    std::string text = readFile(argv[i]);
    std::istringstream lines(text);
    std::string line;
    std::smatch m;
    while (std::getline(lines, line))
      if (std::regex_match(line, m, simLine)) simArgs += " " + trim(m[1].str());
    parts.push_back({slashes(argv[i]), code.size()});
    code += text;
  }

  std::string s = stripCode(code);
  std::string noComments = stripCode(code, true);
  std::set<std::string> skipWords = {"struct", "class", "enum", "union", "namespace", "typedef", "extern", "template",
                                     "if", "while", "for", "switch", "else", "do", "return", "using", "static_assert"};
  std::regex header(R"(^([A-Za-z_][\w\s\*&<>,:]*?[\s\*&])([A-Za-z_]\w*)\s*\(([\s\S]*)\)\s*(const)?\s*$)");

  std::vector<std::string> protos;
  std::set<std::string> seen;
  size_t insertAt = std::string::npos;
  int depth = 0;
  size_t stmtStart = 0;
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (depth == 0 && c == '#' && (i == 0 || s[i - 1] == '\n' || trim(s.substr(s.rfind('\n', i - 1) + 1, i - s.rfind('\n', i - 1) - 1)).empty())) {
      while (i < s.size() && !(s[i] == '\n' && s[i - 1] != '\\')) i++;  // preprocessor line
      stmtStart = i + 1;
      continue;
    }
    if (c == '{') {
      if (depth == 0) {
        std::string raw = s.substr(stmtStart, i - stmtStart);
        size_t hStart = stmtStart + std::min(raw.size(), raw.find_first_not_of(" \t\n"));
        std::string h = trim(raw);
        std::smatch m;
        std::string first = h.substr(0, h.find_first_of(" \t\n(<"));
        bool candidate = !h.empty() && !skipWords.count(first) && !hasTopLevelAssign(h) &&
                         h.find("extern \"C\"") == std::string::npos;
        if (candidate && std::regex_match(h, m, header)) {
          std::string fn = m[2].str();
          if (!skipWords.count(fn) && fn != "setup" && fn != "loop") {
            if (insertAt == std::string::npos) insertAt = stmtStart;
            // Like the Arduino IDE, default values move into the prototype (and out of the
            // definition, where repeating them would be an error).
            size_t pStart = hStart + (size_t)m.position(3), pLen = (size_t)m.length(3);
            std::string params = noComments.substr(pStart, pLen);
            for (auto &d : defaultRanges(s.substr(pStart, pLen)))
              for (size_t k = pStart + d.first; k < pStart + d.second; k++)
                if (code[k] != '\n') code[k] = ' ';
            std::string p = squeeze(m[1].str()) + " " + fn + "(" + squeeze(params) + ")" +
                            (m[4].matched ? " const" : "") + ";";
            if (seen.insert(p).second) protos.push_back(p);
          } else if (insertAt == std::string::npos && (fn == "setup" || fn == "loop")) {
            insertAt = stmtStart;
          }
        }
      }
      depth++;
    } else if (c == '}') {
      if (depth > 0) depth--;
      if (depth == 0) stmtStart = i + 1;
    } else if (c == ';' && depth == 0) {
      stmtStart = i + 1;
    }
  }
  if (insertAt != std::string::npos) {
    while (insertAt < s.size() && (s[insertAt] == ' ' || s[insertAt] == '\t' || s[insertAt] == '\n')) insertAt++;
    // back up to the start of that line so #line stays valid
    while (insertAt > 0 && code[insertAt - 1] != '\n') insertAt--;
  }

  auto partOf = [&](size_t pos) {
    size_t k = 0;
    for (size_t j = 0; j < parts.size(); j++) if (parts[j].start <= pos) k = j;
    return k;
  };
  auto lineAt = [&](size_t pos) {
    size_t k = partOf(pos);
    int line = 1;
    for (size_t j = parts[k].start; j < pos; j++) if (code[j] == '\n') line++;
    return line;
  };

  std::ostringstream out;
  out << "// Generated by sketchgen from " << name << " — edit the .ino, not this file.\n";
  out << "#include <Arduino.h>\n";
  for (size_t k = 0; k < parts.size(); k++) {
    size_t a = parts[k].start, b = k + 1 < parts.size() ? parts[k + 1].start : code.size();
    out << "#line 1 \"" << parts[k].path << "\"\n";
    if (insertAt != std::string::npos && insertAt >= a && insertAt < b) {
      out << code.substr(a, insertAt - a);
      for (auto &p : protos) out << p << "\n";
      out << "#line " << lineAt(insertAt) << " \"" << parts[k].path << "\"\n";
      out << code.substr(insertAt, b - insertAt);
    } else {
      out << code.substr(a, b - a);
    }
  }
  out << "\nconst char *sim_sketch_args = \"" << cEscape(trim(simArgs)) << "\";\n";
  out << "const char *sim_sketch_name = \"" << cEscape(name) << "\";\n";

  std::string result = out.str();
  std::ifstream old(outPath, std::ios::binary);
  if (old) {  // don't touch the file if nothing changed (saves a rebuild)
    std::stringstream ss;
    ss << old.rdbuf();
    if (ss.str() == result) return 0;
  }
  std::ofstream f(outPath, std::ios::binary);
  f << result;
  return f ? 0 : 1;
}
