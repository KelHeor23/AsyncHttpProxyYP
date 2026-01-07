#include "headers.h"

#include <ranges>
#include <string_view>

using namespace std::string_view_literals;

constexpr bool svCompare(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

using Callback = std::function<void(std::string_view, std::string_view)>;

void iterHeaders(std::string_view req, Callback &&callback) {
    auto end = req.find("\r\n\r\n"sv);
    if (end == std::string_view::npos)
        return;

    auto start = req.find("\r\n"sv);
    if (start == std::string_view::npos || start >= end)
        return;

    const auto block = req.substr(start + 2, end - start - 2);

    auto chunks = block | std::views::split("\r\n"sv);

    for (auto &&it : chunks) {
        std::string_view chunk(it.begin(), it.end());
        if (chunk.empty())
            continue;

        auto colon = chunk.find(':');
        if (colon == std::string_view::npos)
            continue;

        std::string_view name = chunk.substr(0, colon);
        std::string_view value_raw = chunk.substr(colon + 1);

        auto first = value_raw.find_first_not_of(" \t"sv);
        if (first == std::string_view::npos) {
            callback(name, ""sv);
        } else {
            auto last = value_raw.find_last_not_of(" \t"sv);
            std::string_view value = value_raw.substr(first, last - first + 1);
            callback(name, value);
        }
    }
}

HostPort findHostPort(std::string_view req) {
    std::string_view host_with_port;

    iterHeaders(req, [&](std::string_view name, std::string_view value) {
        if (svCompare(name, "host"sv)) {
            host_with_port = value;
        }
    });

    if (host_with_port.empty()) {
        return {std::string{}, DEFFAULT_PORT};
    }

    size_t colon_pos = host_with_port.find(':');

    if (colon_pos == std::string_view::npos) {
        return {std::string(host_with_port), DEFFAULT_PORT};
    }

    std::string_view host_part = host_with_port.substr(0, colon_pos);
    std::string_view port_part = host_with_port.substr(colon_pos + 1);

    if (port_part.empty()) {
        return {std::string(host_part), DEFFAULT_PORT};
    }

    return {std::string(host_part), std::string(port_part)};
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    std::optional<size_t> result;

    iterHeaders(rsp, [&](std::string_view name, std::string_view value) {
        if (svCompare(name, "content-length"sv)) {
            size_t len = 0;
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), len);
            if (ec == std::errc{}) {
                result = len;
            }
        }
    });

    return result;
}
