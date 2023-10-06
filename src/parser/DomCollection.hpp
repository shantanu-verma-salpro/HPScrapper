#ifndef DOCC
#define DOCC

#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <stdexcept>
#include <memory>

class Node;
class Document;
class NodeList;
class Parser;

class DomCollection {
public:
    DomCollection(lxb_html_document_t* document) 
        : collection_(lxb_dom_collection_make(&document->dom_document, 128), &DomCollection::deleter) {
        if (!collection_) {
            throw std::runtime_error("Failed to create DOM collection");
        }
    }
    
    lxb_dom_collection_t* get() const {
        return collection_.get();
    }

    void clean() const {
        lxb_dom_collection_clean(collection_.get());
    }

private:
    static void deleter(lxb_dom_collection_t* collection) {
        lxb_dom_collection_destroy(collection, true);
    }

    std::unique_ptr<lxb_dom_collection_t, decltype(&deleter)> collection_;
};

#endif