#ifndef PARSER
#define PARSER

#include "Document.hpp"

class Parser {
    std::unique_ptr<lxb_html_parser_t, decltype(&lxb_html_parser_destroy)>
        parser {lxb_html_parser_create(), &lxb_html_parser_destroy};

    static void check(const lxb_status_t status, const char* errMsg) {
        if (status != LXB_STATUS_OK) {
            throw std::runtime_error(errMsg);
        }
    }

public:
    Parser() {
        lxb_status_t status = lxb_html_parser_init(parser.get());
        check(status, "Failed to initialize HTML parser");
    }

    Document createDOM(const std::string& msg) {
        auto* document_ptr = lxb_html_parse(parser.get(),
                                            reinterpret_cast<const lxb_char_t *>(msg.c_str()),
                                            msg.length());
        if (!document_ptr) {
            throw std::runtime_error("Failed to parse HTML");
        }
        return Document(std::unique_ptr<lxb_html_document_t, decltype(&lxb_html_document_destroy)>(document_ptr, &lxb_html_document_destroy));
    }
};

#endif