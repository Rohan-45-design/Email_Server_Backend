#include "mime_parser.h"
#include "mime_decoder.h"
#include <sstream>
#include <algorithm>

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

MimeHeaderMap MimeParser::parseHeaders(std::string& data) {
    MimeHeaderMap headers;
    std::istringstream iss(data);
    std::string line, lastKey;

    while (std::getline(iss, line)) {
        if (line == "\r" || line.empty()) break;

        if (line[0] == ' ' || line[0] == '\t') {
            headers[lastKey] += " " + line;
            continue;
        }

        auto pos = line.find(':');
        if (pos != std::string::npos) {
            lastKey = toLower(line.substr(0, pos));
            headers[lastKey] = line.substr(pos + 1);
        }
    }

    return headers;
}

bool MimePart::isMultipart() const {
    auto it = headers.find("content-type");
    return it != headers.end() &&
           it->second.find("multipart/") != std::string::npos;
}

std::string MimePart::contentType() const {
    auto it = headers.find("content-type");
    return it == headers.end() ? "" : it->second;
}

std::string MimePart::filename() const {
    auto it = headers.find("content-disposition");
    if (it == headers.end()) return "";

    auto pos = it->second.find("filename=");
    if (pos == std::string::npos) return "";

    pos += 9;
    if (it->second[pos] == '"') {
        auto end = it->second.find('"', pos + 1);
        return it->second.substr(pos + 1, end - pos - 1);
    }
    return it->second.substr(pos);
}

MimePart MimeParser::parsePart(
    const std::string& data,
    const std::string& boundary
) {
    MimePart part;
    std::string content = data;

    part.headers = parseHeaders(content);

    auto encIt = part.headers.find("content-transfer-encoding");
    std::string encoding = encIt == part.headers.end()
        ? "7bit" : encIt->second;

    if (!boundary.empty()) {
        std::string delim = "--" + boundary;
        size_t pos = 0;
        while ((pos = content.find(delim, pos)) != std::string::npos) {
            size_t start = pos + delim.size();
            size_t end = content.find(delim, start);
            if (end == std::string::npos) break;

            std::string childData =
                content.substr(start, end - start);

            part.children.push_back(
                parsePart(childData, "")
            );
            pos = end;
        }
    } else {
        part.body = MimeDecoder::decodeTransferEncoding(
            content, encoding
        );
    }

    return part;
}

MimeMessage MimeParser::parse(const std::string& raw) {
    MimeMessage msg;
    std::string data = raw;

    msg.headers = parseHeaders(data);

    std::string boundary;
    auto it = msg.headers.find("content-type");
    if (it != msg.headers.end()) {
        auto pos = it->second.find("boundary=");
        if (pos != std::string::npos) {
            boundary = it->second.substr(pos + 9);
            if (boundary.front() == '"')
                boundary = boundary.substr(1, boundary.size() - 2);
        }
    }

    msg.root = parsePart(data, boundary);
    return msg;
}
