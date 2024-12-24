#ifndef __LINKER_HPP__
#define __LINKER_HPP__

#include "parser/ast/file_node.hpp"

/// Linker
///     The linker is responsible for linking all .ll files together and creating the executable from the .ll files
class Linker {
  public:
    static void resolve_links(FileNode &file_node);
};

#endif
