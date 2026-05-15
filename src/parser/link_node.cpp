#include "parser/ast/definitions/link_node.hpp"
#include "parser/ast/expressions/function_reference_node.hpp"

LinkNode::LinkNode(                              //
    const Hash &file_hash,                       //
    const unsigned int line,                     //
    const unsigned int column,                   //
    const unsigned int length,                   //
    std::unique_ptr<FunctionReferenceNode> &src, //
    std::unique_ptr<FunctionReferenceNode> &dest //
    ) :
    DefinitionNode(file_hash, line, column, length, {}),
    src(std::move(src)),
    dest(std::move(dest)) {}

LinkNode::~LinkNode() = default;
LinkNode::LinkNode(LinkNode &&) = default;
LinkNode &LinkNode::operator=(LinkNode &&) = default;
