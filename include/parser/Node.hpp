#ifndef NODE
#define NODE

#include "NodeList.hpp"
#include <regex>
#include <unordered_map>

class Node {

    bool isElementNode(lxb_dom_node_t* node) const {
        return node->type == LXB_DOM_NODE_TYPE_ELEMENT;
    }

    bool isTextNode(lxb_dom_node_t* node) const {
        return node->type == LXB_DOM_NODE_TYPE_TEXT;
    }

    bool hasChildElements(lxb_dom_node_t* node) const {
        return !lxb_dom_node_is_empty(node);
    }

    lxb_dom_node_t* getNextElementNode(lxb_dom_node_t* node) const{
        lxb_dom_node_t* childNode = node;
        while(childNode!=nullptr){
            if(isElementNode(childNode)) break;
            childNode = lxb_dom_node_next(childNode);
        }
        return childNode;
    }

    lxb_dom_node_t* getPrevElementNode(lxb_dom_node_t* node) const{
        lxb_dom_node_t* childNode = node;
        while(childNode!=nullptr){
            if(isElementNode(childNode)) break;
            childNode = lxb_dom_node_prev(childNode);
        }
        return childNode;
    }

public:
    Node(lxb_dom_node_t* node, lxb_html_document_t* document , DomCollection&  col)
        : node_(node), document_(document),collection_(col) {}

    lxb_dom_node_t* get() const noexcept{
        return node_;
    }

    bool hasChildElements () const{
        return hasChildElements(node_);
    }

    std::unique_ptr<Node> firstChild() const {
        if(!hasChildElements(node_)) return nullptr;
        lxb_dom_node_t* childNode = lxb_dom_node_first_child(node_);
        childNode = getNextElementNode(childNode);
        if(!childNode) return nullptr;
        return std::make_unique<Node>(childNode ,document_ ,collection_);
    }

    std::unique_ptr<Node> lastChild() const {
        if(!hasChildElements(node_)) return nullptr;
        lxb_dom_node_t* childNode = lxb_dom_node_last_child(node_);
        childNode = getPrevElementNode(childNode);
        if(!childNode) return nullptr;
        return std::make_unique<Node>(childNode ,document_ ,collection_);
    }

    std::unique_ptr<Node> nextSibling() const {
        lxb_dom_node_t* siblingNode = getNextElementNode(node_);;
        if(!siblingNode) return nullptr;
        return std::make_unique<Node>(siblingNode,document_,collection_);
    }

    std::unique_ptr<Node> prevSibling() const {
        lxb_dom_node_t* siblingNode = getPrevElementNode(node_);;
        if(!siblingNode) return nullptr;
        return std::make_unique<Node>(siblingNode,document_,collection_);
    }

    const std::string getAttribute(const std::string& name) const {
        if(!isElementNode(node_)) return {};

        auto* element = lxb_dom_interface_element(node_);
        if (!element) {
            throw std::runtime_error("Node is not an element");
        }
        auto* attr = lxb_dom_element_attr_by_name(element, reinterpret_cast<const lxb_char_t*>(name.c_str()), name.length());
        if (!attr) {
            return ""; 
        }
        std::size_t len;
        const char* str = reinterpret_cast<const char*>(lxb_dom_attr_value(attr, &len));
        return std::string(str,len);
    }

    std::unique_ptr<NodeList> getChildElements() const {
        collection_.clean();
        if(!hasChildElements(node_)) return nullptr;
        return getElementsByTagName("*");
    }

    const bool hasAttributes() const {
        if(!isElementNode(node_)) return false;
        return lxb_dom_element_has_attributes(lxb_dom_interface_element(node_));
    }

    const bool hasAttribute(const std::string& attr) const{
        if(!isElementNode(node_)) return {};
        auto* element = lxb_dom_interface_element(node_);
        const lxb_char_t* name = reinterpret_cast<const lxb_char_t*>(attr.c_str());
        return lxb_dom_element_has_attribute(element ,name , attr.length());
    }

    std::unique_ptr<std::unordered_map<std::string, std::string>> getAttributes() const {
        auto mp = std::make_unique<std::unordered_map<std::string, std::string>>();
        if(!isElementNode(node_)) return mp;
        auto* element = lxb_dom_interface_element(node_);
        lxb_dom_attr_t* attr = lxb_dom_element_first_attribute(element);
        std::size_t len = 0;
        const lxb_char_t *temp = nullptr;
        
        while (attr != nullptr) {
            std::string key , value;
            temp = lxb_dom_attr_qualified_name(attr, &len);
            if (temp != nullptr) {
                key = std::string(reinterpret_cast<const char*>(temp), len); 
            }
            temp = lxb_dom_attr_value(attr, &len);
            if (temp != nullptr) {
                value = std::string(reinterpret_cast<const char*>(temp), len); 
            }
            mp->emplace(key, value);
            attr = lxb_dom_element_next_attribute(attr);
        }
        return mp;
    }

    const std::string text() const{
        std::string str;
        lxb_char_t * s {nullptr};
        std::size_t len = 0;
        lxb_dom_node_t* childNode = lxb_dom_node_first_child(node_);
        while(childNode!=nullptr){
            if(isTextNode(childNode)){
                s = lxb_dom_node_text_content(childNode,&len);
                str += std::string(reinterpret_cast<const char*>(s),len);
            }
            childNode = lxb_dom_node_next(childNode);
        }

        return str;
    }

    std::unique_ptr<NodeList> getElementsByClassName(const std::string& className) const {
        collection_.clean();
        if(!isElementNode(node_) || !hasChildElements(node_)) return nullptr;
        auto* element = lxb_dom_interface_element(node_);
        const lxb_char_t * name = reinterpret_cast<const lxb_char_t *>(className.c_str());
        lxb_status_t status = lxb_dom_elements_by_class_name(element, collection_.get(), name, className.length());
        if (status != LXB_STATUS_OK) {
            throw std::runtime_error("Failed to collect child elements by tag name");
        }
        return std::make_unique<NodeList>(collection_, document_);
    }

    std::unique_ptr<NodeList> getElementsByTagName(const std::string& className) const {
        collection_.clean();
        if(!isElementNode(node_) || !hasChildElements(node_)) return nullptr;
        auto* element = lxb_dom_interface_element(node_);
        const lxb_char_t * name = reinterpret_cast<const lxb_char_t *>(className.c_str());
        lxb_status_t status = lxb_dom_elements_by_tag_name(element, collection_.get(), name, className.length());
        if (status != LXB_STATUS_OK) {
            throw std::runtime_error("Failed to collect child elements by tag name");
        }
        return std::make_unique<NodeList>(collection_, document_);
    }

    std::unique_ptr<NodeList> getElementsByAttribute(const std::string& key, const std::string& value) const {
        collection_.clean();
        if(!isElementNode(node_) || !hasChildElements(node_)) return nullptr;
        auto* element = lxb_dom_interface_element(node_);
        const lxb_char_t * name = reinterpret_cast<const lxb_char_t *>(key.c_str());
        const lxb_char_t * val = reinterpret_cast<const lxb_char_t *>(value.c_str());
        lxb_status_t status = lxb_dom_elements_by_attr(element, collection_.get(), name, key.length(), val, value.length(), true);
        if (status != LXB_STATUS_OK) {
            throw std::runtime_error("Failed to collect child elements by tag name");
        }
        return std::make_unique<NodeList>(collection_, document_);
    }

    std::unique_ptr<std::vector<std::string>> getLinksMatching(const std::string& pattern = "") const {
        std::regex regexPattern(pattern);
        std::vector<std::string> matchingLinks;
        
        auto linksNodeList = this->getElementsByTagName("a");
        if (linksNodeList) {
            for (std::size_t i = 0; i < linksNodeList->length(); ++i) {
                auto linkNode = linksNodeList->item(i);
                if (linkNode) {
                    std::string url = linkNode->getAttribute("href");
                    if (pattern.empty() || std::regex_match(url, regexPattern)) {
                        matchingLinks.push_back(url);
                    }
                }
            }
        }
        return std::make_unique<std::vector<std::string>>(matchingLinks);
    }

private:
    lxb_dom_node_t* node_;
    lxb_html_document_t* document_;
    DomCollection& collection_;
};

#endif