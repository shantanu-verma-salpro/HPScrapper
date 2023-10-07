
#include <iostream>
#include <string>
#include "../include/parser/Document.hpp"
#include "../include/parser/Parser.hpp"
#include <lexbor/html/html.h>
#include <lexbor/dom/interfaces/element.h>

void run_parse_example() {
    std::string htmlContent = R"(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Test Page</title>
        </head>
        <body>
            <div class="col-md">
                <div>Text 1 inside col-md</div>
                <a href="http://example.com">Example Link</a>
                <div data-custom="value">Text 2 inside col-md</div>
            </div>
            <div class="col-md">
                <div>Text 3 inside col-md</div>
            </div>
        </body>
        </html>
    )";

    Parser parser;
    Document doc = parser.createDOM(htmlContent);

    auto root = doc.rootElement();
    auto colMdElements = root->getElementsByClassName("col-md");

    for (std::size_t i = 0; i < colMdElements->length(); ++i) {
        auto colMdNode = colMdElements->item(i);
        auto divElements = colMdNode->getElementsByTagName("div");

        for (std::size_t j = 0; j < divElements->length(); ++j) {
            auto divNode = divElements->item(j);
            std::cout << divNode->text() << '\n';

            if(divNode->hasAttributes()) {
                auto attributes = divNode->getAttributes();
                for(const auto& [attr, value] : *attributes) {
                    std::cout << "Attribute: " << attr << ", Value: " << value << std::endl;
                }
            }

            if(divNode->hasAttribute("data-custom")) {
                std::cout << "Data-custom attribute: " << divNode->getAttribute("data-custom") << '\n';
            }
        }

        if (colMdNode->hasChildElements()) {
            auto firstChild = colMdNode->firstChild();
            auto lastChild = colMdNode->lastChild();

            std::cout << "First child's text content: " << firstChild->text() << '\n';
            std::cout << "Last child's text content: " << lastChild->text() << '\n';
        }

        auto links = colMdNode->getLinksMatching("http://example.com");
        for(const auto& link : *links) {
            std::cout << "Matching Link: " << link << '\n';
        }
    }

}