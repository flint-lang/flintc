#ifndef __NODE_TYPE_HPP__
#define __NODE_TYPE_HPP__

/// NodeType
///     The type a node could have
enum NodeType {
    NONE,
    // --- DEFINITIONS ---
    IMPORT, FUNCTION, DATA, FUNC, ENTITY, ENUM, ERROR, VARIANT,
    // --- STATEMENTS ---
    DECL_EXPLICIT, DECL_INFERED, ASSIGNMENT, FOR_LOOP, ENH_FOR_LOOP, PAR_FOR_LOOP, WHILE_LOOP, IF, ELSE_IF, ELSE, RETURN

};

#endif
