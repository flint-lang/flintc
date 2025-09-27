#include "lsp_protocol.hpp"

#include <iostream>
#include <string>

std::string extract_request_id(const std::string &content) {
    // Simple extraction to find "id": number or "id":number
    size_t id_pos = content.find("\"id\":");
    if (id_pos != std::string::npos) {
        size_t start = content.find_first_of("0123456789", id_pos);
        if (start != std::string::npos) {
            size_t end = content.find_first_of(",}", start);
            if (end != std::string::npos) {
                return content.substr(start, end - start);
            }
        }
    }
    return "1"; // fallback
}

bool contains_method(const std::string &content, const char *method) {
    std::string method_pattern = "\"method\":\"";
    method_pattern += method;
    method_pattern += "\"";
    return content.find(method_pattern) != std::string::npos;
}

void send_lsp_response(const std::string &response) {
    std::cout << "Content-Length: " << response.length() << "\r\n\r\n" << response << std::flush;
}
