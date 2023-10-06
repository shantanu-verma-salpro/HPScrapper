#ifndef DOC
#define DOC

#include "Node.hpp"

class Document {
public:

    Document(std::unique_ptr<lxb_html_document_t, decltype(&lxb_html_document_destroy)>&& doc)
        : doc_(std::move(doc)), collection_(doc_.get()) {}

    std::unique_ptr<Node> rootElement() {
        lxb_html_body_element_t* bodyElement = lxb_html_document_body_element(doc_.get());
        if (!bodyElement) {
            throw std::runtime_error("Failed to obtain body element");
        }
        return std::make_unique<Node>(lxb_dom_interface_node(bodyElement), doc_.get(), collection_);
    }

private:
    static inline void deleter(lxb_dom_collection_t* collection){
        lxb_dom_collection_destroy(collection,true);
    }
    std::unique_ptr<lxb_html_document_t, decltype(&lxb_html_document_destroy)> doc_;
    DomCollection collection_;
};

#endif