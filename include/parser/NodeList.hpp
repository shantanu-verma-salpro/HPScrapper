#ifndef NODEL
#define NODEL

#include "DomCollection.hpp"
#include <vector>

class NodeList {
    std::vector<lxb_dom_node_t*> nodePtr;
public:
    NodeList(DomCollection& collection, lxb_html_document_t* document)
        : collection_(collection), document_(document) {
            std::size_t len = length();
            nodePtr.reserve(len); 
            lxb_dom_node_t* node { nullptr };
            for(std::size_t i = 0; i < len; ++i){
                node = lxb_dom_collection_node(collection_.get(), i);
                if(!lxb_dom_node_is_empty(node)) nodePtr.emplace_back(node);
            }
        }

    const std::size_t length() const {
        return lxb_dom_collection_length(collection_.get());
    }

    std::unique_ptr<Node> item(const std::size_t index) const {
        if(index >= nodePtr.size()) return nullptr; 
        return std::make_unique<Node>(nodePtr.at(index), document_, collection_);
    }

private:
    DomCollection& collection_;
    lxb_html_document_t* document_;
};

#endif