#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace cmdpal::core {

inline std::string toLowerAscii(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

inline std::string trimAscii(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

// Returns a deterministic relevance score in [0, 100], or 0 if no match.
inline double scoreMatch(const std::string& query, const std::string& target, const std::string& subtitle = "") {
    std::string q = toLowerAscii(trimAscii(query));
    if (q.empty()) return 10.0;

    std::string t = toLowerAscii(target);
    if (t == q) return 100.0;
    if (t.rfind(q, 0) == 0) return 90.0;

    // Word boundary prefix match
    size_t pos = t.find(q);
    if (pos != std::string::npos) {
        bool wordBoundary = (pos == 0 || t[pos - 1] == ' ' || t[pos - 1] == '-' || t[pos - 1] == '_' || t[pos - 1] == '.');
        return wordBoundary ? 80.0 : 65.0;
    }

    // Subsequence fuzzy match on target
    size_t qi = 0;
    size_t consecutive = 0;
    size_t bestConsecutive = 0;
    for (size_t ti = 0; ti < t.size() && qi < q.size(); ++ti) {
        if (t[ti] == q[qi]) {
            ++qi;
            ++consecutive;
            bestConsecutive = std::max(bestConsecutive, consecutive);
        } else {
            consecutive = 0;
        }
    }
    if (qi == q.size()) {
        double ratio = static_cast<double>(q.size()) / static_cast<double>(std::max<size_t>(1, t.size()));
        return 35.0 + std::min(25.0, ratio * 20.0 + static_cast<double>(bestConsecutive) * 2.0);
    }

    // Fallback: subtitle substring match
    if (!subtitle.empty()) {
        std::string sub = toLowerAscii(subtitle);
        if (sub.find(q) != std::string::npos) return 25.0;
    }
    return 0.0;
}

}  // namespace cmdpal::core
