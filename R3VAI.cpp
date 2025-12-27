#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// ===== CONFIG =====
const std::string VERSION_URL   = "https://drive.google.com/uc?export=download&id=1Ba3PLQxlsB2PEHS8DLiIv36-jfUfjvCr";
const std::string RESPONSES_URL = "https://drive.google.com/uc?export=download&id=1nWuW8bsZPk7b9p8uPX0TOkEpXBpiVJbx";

const std::string VERSION_FILE   = "version.txt";
const std::string RESPONSES_FILE = "responses.txt";

// ===== DOWNLOAD HELPER =====
bool downloadFile(const std::string& url, const std::string& output) {
    std::string cmd =
        "curl -fsSL \"" + url + "\" -o \"" + output + "\""
        " || wget -q \"" + url + "\" -O \"" + output + "\"";

    return std::system(cmd.c_str()) == 0;
}

// ===== FILE HELPERS =====
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void ensureFilesUpToDate() {
    bool versionMissing   = !fs::exists(VERSION_FILE);
    bool responsesMissing = !fs::exists(RESPONSES_FILE);

    if (versionMissing) {
        std::cout << "[INFO] Downloading version.txt\n";
        downloadFile(VERSION_URL, VERSION_FILE);
    }

    const std::string remoteVersionFile = "version_remote.txt";
    downloadFile(VERSION_URL, remoteVersionFile);

    std::string localVersion  = readFile(VERSION_FILE);
    std::string remoteVersion = readFile(remoteVersionFile);

    if (responsesMissing || localVersion != remoteVersion) {
        std::cout << "[INFO] Updating responses.txt\n";
        downloadFile(RESPONSES_URL, RESPONSES_FILE);
        downloadFile(VERSION_URL, VERSION_FILE);
    }

    fs::remove(remoteVersionFile);
}

// ===== TEXT HELPERS =====
std::string toLower(std::string s) {
    for (char& c : s) c = std::tolower(static_cast<unsigned char>(c));
    return s;
}

// ===== LEVENSHTEIN DISTANCE =====
int levenshtein(const std::string& a, const std::string& b) {
    size_t lenA = a.size();
    size_t lenB = b.size();

    std::vector<std::vector<int>> dp(lenA + 1, std::vector<int>(lenB + 1));

    for (size_t i = 0; i <= lenA; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= lenB; ++j) dp[0][j] = j;

    for (size_t i = 1; i <= lenA; ++i) {
        for (size_t j = 1; j <= lenB; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i - 1][j] + 1,
                dp[i][j - 1] + 1,
                dp[i - 1][j - 1] + cost
            });
        }
    }
    return dp[lenA][lenB];
}

// ===== RESPONSE SYSTEM =====
std::unordered_map<std::string, std::vector<std::string>> loadResponses() {
    std::unordered_map<std::string, std::vector<std::string>> responses;
    std::ifstream file(RESPONSES_FILE);

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        size_t sep = line.find('|');
        if (sep == std::string::npos) continue;

        std::string trigger = toLower(line.substr(0, sep));
        std::string rest    = line.substr(sep + 1);

        std::stringstream ss(rest);
        std::string piece;
        std::vector<std::string> options;

        while (std::getline(ss, piece, ';')) {
            if (!piece.empty() && piece != " ") {
                if (piece[0] == ';') piece.erase(0, 1);
                options.push_back(piece);
            }
        }

        responses[trigger] = options;
    }

    return responses;
}

std::string getResponse(
    const std::unordered_map<std::string, std::vector<std::string>>& data,
    const std::string& input
) {
    std::string user = toLower(input);

    // Exact match first
    auto exact = data.find(user);
    if (exact != data.end()) {
        const auto& options = exact->second;
        return options[rand() % options.size()];
    }

    // Fuzzy match
    int bestScore = 999;
    std::string bestKey;

    for (const auto& pair : data) {
        int score = levenshtein(user, pair.first);
        if (score < bestScore) {
            bestScore = score;
            bestKey = pair.first;
        }
    }

    // Accept only close matches
    if (bestScore <= 2 && !bestKey.empty()) {
        const auto& options = data.at(bestKey);
        return options[rand() % options.size()];
    }

    return "I don't know how to respond to that.";
}

// ===== MAIN =====
int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    ensureFilesUpToDate();
    auto responses = loadResponses();

    std::string input;
    while (true) {
        std::cout << "You: ";
        std::getline(std::cin, input);

        if (input == "exit") break;

        std::cout << getResponse(responses, input) << "\n";
    }
}
