#include "features/web/WebCoordinator.hpp"

#include "core/SearchMatch.hpp"

#include <cctype>
#include <cstdio>

namespace cmdpal::features::web {

void WebCoordinator::initialize(core::CoreServices& core) {
    core_ = &core;
}

void WebCoordinator::shutdown() {
    core_ = nullptr;
}

std::vector<core::SettingDescriptor> WebCoordinator::settingDescriptors() const {
    return {
        {"web.bangs", "Web Bangs & Search Aliases", "Add, edit, or remove search aliases (g, w, yt) and URL templates",
         "web", {"web", "bang", "alias", "google", "youtube", "search"}},
        {"web.bookmarks", "Web Bookmarks", "Add, rename, reorder, and remove saved bookmarks",
         "web", {"bookmark", "url", "favorites"}}
    };
}

std::string WebCoordinator::urlEncode(const std::string& value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out.push_back('+');
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0xF]);
            out.push_back(kHex[c & 0xF]);
        }
    }
    return out;
}

std::string WebCoordinator::buildSearchUrl(const std::string& urlTemplate, const std::string& query) {
    std::string encoded = urlEncode(query);
    std::string result = urlTemplate;
    size_t pos = result.find("{query}");
    if (pos != std::string::npos) {
        result.replace(pos, 7, encoded);
    } else {
        result += encoded;
    }
    return result;
}

UrlValidationResult WebCoordinator::validateUrlStatic(const std::string& input) {
    std::string trimmed = core::trimAscii(input);
    if (trimmed.empty()) return {false, false, "", "Empty input"};
    if (trimmed.find(' ') != std::string::npos ||
        trimmed.find('\t') != std::string::npos ||
        trimmed.find('\r') != std::string::npos ||
        trimmed.find('\n') != std::string::npos) {
        return {false, false, "", "Contains whitespace"};
    }

    // Check for explicit scheme (scheme:...)
    size_t colonPos = trimmed.find(':');
    size_t slashPos = trimmed.find('/');
    if (colonPos != std::string::npos && (slashPos == std::string::npos || colonPos < slashPos)) {
        std::string scheme = core::toLowerAscii(trimmed.substr(0, colonPos));
        bool validSchemeChars = !scheme.empty();
        for (char c : scheme) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' && c != '.') {
                validSchemeChars = false;
                break;
            }
        }
        if (validSchemeChars) {
            // Check if it's a port on a host (e.g., localhost:3000/path) vs a scheme
            bool isHostPort = (trimmed.compare(colonPos, 3, "://") != 0) &&
                              colonPos + 1 < trimmed.size() &&
                              std::isdigit(static_cast<unsigned char>(trimmed[colonPos + 1]));
            if (!isHostPort) {
                if (scheme == "http" || scheme == "https") {
                    if (trimmed.compare(colonPos, 3, "://") != 0) {
                        return {false, false, "", "Missing :// after http/https"};
                    }
                    std::string rest = trimmed.substr(colonPos + 3);
                    size_t hostEnd = rest.find_first_of("/?#");
                    std::string host = hostEnd == std::string::npos ? rest : rest.substr(0, hostEnd);
                    if (host.empty()) {
                        return {false, false, "", "Missing host"};
                    }
                    return {true, false, trimmed, ""};
                }
                // Reject dangerous or non-HTTP(S) schemes
                return {false, true, "", "Unsupported or dangerous URL scheme: " + scheme};
            }
        }
    }

    // Schemeless URL normalization (only if it clearly looks like domain.tld[/path] or localhost[:port])
    size_t hostEnd = trimmed.find_first_of("/?#");
    std::string hostPort = hostEnd == std::string::npos ? trimmed : trimmed.substr(0, hostEnd);
    size_t portColon = hostPort.find(':');
    std::string host = portColon == std::string::npos ? hostPort : hostPort.substr(0, portColon);
    if (portColon != std::string::npos) {
        std::string portStr = hostPort.substr(portColon + 1);
        if (portStr.empty()) return {false, false, "", "Empty port"};
        for (char c : portStr) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return {false, false, "", "Invalid port"};
        }
    }

    std::string hostLower = core::toLowerAscii(host);
    if (hostLower == "localhost") {
        return {true, false, "http://" + trimmed, ""};
    }

    size_t dotPos = host.rfind('.');
    if (dotPos == std::string::npos || dotPos == 0 || dotPos + 2 >= host.size()) {
        return {false, false, "", "Not a URL"};
    }
    std::string tld = host.substr(dotPos + 1);
    for (char c : tld) {
        if (!std::isalpha(static_cast<unsigned char>(c))) {
            return {false, false, "", "Invalid TLD"};
        }
    }
    for (char c : host) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-') {
            return {false, false, "", "Invalid host characters"};
        }
    }
    return {true, false, "https://" + trimmed, ""};
}

UrlValidationResult WebCoordinator::validateAndNormalizeUrl(const std::string& input) const {
    return validateUrlStatic(input);
}

ParsedWebInput WebCoordinator::parseInput(const std::string& rawQuery) const {
    ParsedWebInput res;
    if (!core_) {
        res.remainingQuery = rawQuery;
        return res;
    }

    size_t spacePos = rawQuery.find(' ');
    if (spacePos == std::string::npos || spacePos == 0) {
        res.remainingQuery = rawQuery;
        return res;
    }

    std::string prefix = core::toLowerAscii(rawQuery.substr(0, spacePos));
    auto settings = core_->settings().snapshot();
    for (const auto& bang : settings.webBangs) {
        if (!bang.enabled) continue;
        if (core::toLowerAscii(bang.alias) == prefix) {
            res.chip = core::AliasChip{bang.alias, bang.label, bang.id};
            res.remainingQuery = rawQuery.substr(spacePos + 1);
            return res;
        }
    }

    res.remainingQuery = rawQuery;
    return res;
}

std::vector<core::SearchResult> WebCoordinator::query(const core::QueryContext& ctx) {
    if (!core_) return {};

    auto settings = core_->settings().snapshot();
    std::vector<core::SearchResult> results;

    // Case 1: Active bang chip
    if (ctx.activeChip.has_value()) {
        const auto& chip = *ctx.activeChip;
        const core::WebBangRule* matchedBang = nullptr;
        for (const auto& b : settings.webBangs) {
            if (b.enabled && b.id == chip.destinationId) {
                matchedBang = &b;
                break;
            }
        }
        if (!matchedBang) return {};

        std::string subQuery = core::trimAscii(ctx.trimmedQuery);
        if (subQuery.empty()) {
            // Show saved bookmarks for this destination or general bookmarks
            for (const auto& bm : settings.bookmarks) {
                if (!bm.bangId.empty() && bm.bangId != matchedBang->id) continue;
                core::SearchResult r;
                r.id = "bm:" + bm.id;
                r.title = bm.title;
                r.subtitle = bm.url;
                r.category = core::SearchCategory::Bookmarks;
                r.categoryLabel = "Bookmark";
                r.score = 85.0;
                r.providerId = providerId();
                r.actionId = bm.url;
                r.canHide = true;
                results.push_back(std::move(r));
            }
            if (results.empty()) {
                core::SearchResult hint;
                hint.id = "web:hint:" + matchedBang->id;
                hint.title = "Search " + matchedBang->label + "...";
                hint.subtitle = "Type a query and press Enter to open in your default browser";
                hint.category = core::SearchCategory::Web;
                hint.categoryLabel = matchedBang->label;
                hint.score = 80.0;
                hint.providerId = providerId();
                hint.actionId = "";
                results.push_back(std::move(hint));
            }
            return results;
        }

        std::string targetUrl = buildSearchUrl(matchedBang->urlTemplate, subQuery);
        core::SearchResult r;
        r.id = "web:search:" + matchedBang->id;
        r.title = "Search " + matchedBang->label + " for \"" + subQuery + "\"";
        r.subtitle = targetUrl;
        r.category = core::SearchCategory::Web;
        r.categoryLabel = matchedBang->label;
        r.score = 98.0;
        r.providerId = providerId();
        r.actionId = targetUrl;
        results.push_back(std::move(r));
        return results;
    }

    // Case 2: No active chip — check bookmarks
    for (const auto& bm : settings.bookmarks) {
        std::string bmResId = "bm:" + bm.id;
        if (core_->usage().isResultHidden(bmResId)) continue;
        double s = ctx.trimmedQuery.empty()
                       ? 8.0
                       : std::max(core::scoreMatch(ctx.trimmedQuery, bm.title, bm.url),
                                  core::scoreMatch(ctx.trimmedQuery, bm.url, ""));
        if (s <= 0.0) continue;

        core::SearchResult r;
        r.id = bmResId;
        r.title = bm.title;
        r.subtitle = bm.url;
        r.category = core::SearchCategory::Bookmarks;
        r.categoryLabel = "Bookmark";
        r.score = s;
        r.providerId = providerId();
        r.actionId = bm.url;
        r.canHide = true;
        results.push_back(std::move(r));
    }

    // Case 3: Direct URL recognition
    if (!ctx.trimmedQuery.empty()) {
        auto urlCheck = validateUrlStatic(ctx.trimmedQuery);
        if (urlCheck.isUrl) {
            core::SearchResult r;
            r.id = "web:url";
            r.title = "Open URL: " + urlCheck.normalizedUrl;
            r.subtitle = "Open in Windows default browser";
            r.category = core::SearchCategory::Web;
            r.categoryLabel = "URL";
            r.score = 96.0;
            r.providerId = providerId();
            r.actionId = urlCheck.normalizedUrl;
            results.push_back(std::move(r));
        } else if (!urlCheck.rejectedDangerousScheme) {
            // Default web search fallback (lower score than exact local matches)
            const core::WebBangRule* defBang = nullptr;
            for (const auto& b : settings.webBangs) {
                if (b.enabled && b.id == settings.defaultWebSearchId) {
                    defBang = &b;
                    break;
                }
            }
            if (!defBang && !settings.webBangs.empty()) {
                for (const auto& b : settings.webBangs) {
                    if (b.enabled) {
                        defBang = &b;
                        break;
                    }
                }
            }
            if (defBang) {
                std::string targetUrl = buildSearchUrl(defBang->urlTemplate, ctx.trimmedQuery);
                core::SearchResult r;
                r.id = "web:default";
                r.title = "Search " + defBang->label + " for \"" + ctx.trimmedQuery + "\"";
                r.subtitle = targetUrl;
                r.category = core::SearchCategory::Web;
                r.categoryLabel = defBang->label;
                r.score = 22.0;
                r.providerId = providerId();
                r.actionId = targetUrl;
                results.push_back(std::move(r));
            }
        }
    }

    return results;
}

core::ActionResult WebCoordinator::execute(const core::SearchResult& item) {
    if (!core_ || item.actionId.empty()) {
        return {false, false, "No URL to open", "", ""};
    }
    auto check = validateUrlStatic(item.actionId);
    if (!check.isUrl || check.rejectedDangerousScheme) {
        return {false, false, "Blocked unsafe or invalid URL: " + check.reason, "", ""};
    }
    bool ok = core_->platform().openExternalUrl(check.normalizedUrl);
    if (ok && item.category == core::SearchCategory::Bookmarks) {
        core_->usage().recordExecution(item.id, core_->platform().currentTimeMs());
    }
    return {ok, ok, ok ? "Opened in default browser" : "Failed to open browser", "", ""};
}

}  // namespace cmdpal::features::web
