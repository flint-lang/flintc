# Architecture of the Flint Compiler

In this file, i will write down every aspect of the Flint Compiler, all the design choices and the features that are implemented. I will also focus on why something was implemented the way it is.

## Lexer

## Parser


```cpp
void parse_file(ProgramNode &program, std::string &file);

void add_next_main_node(ProgramNode &program, std::vector<TokenContext> &tokens);

NodeType what_is_this(const std::vector<TokenContext> &tokens);

std::pair<FuncNode, uint> get_func_node(const std::vector<TokenContext> &definition, const std::vector<TokenContext> &tokens);

std::pair<DataNode, uint> get_data_node(const std::vector<TokenContext> &definition, const std::vector<TokenContext> &tokens);

std::pair<ImportNode, uint> get_import_node(const std::vector<TokenContext> &definition);
```


### AST Generation

The AST Generation is part of the Parser.

The following AST Nodes are planned and / or implemented:

```cpp
enum NodeType;      // The type of the node
class ASTNode;      // The base class for all AST Nodes

// Expressions
class ExpressionNode : ASTNode;         // Base class for all expressions
class LiteralNode : ExpressionNode;     // Represents literal values
class VariableNode : ExpressionNode;    // Represents variables or identifiers
class BinaryOpNode : ExpressionNode;    // Represents binary operations
class UnaryOpNode : ExpressionNode;     // Represents unary operations
class CallNode : ExpressionNode;        // Represents function or method calls

// Statements
class StatementNode : ASTNode;          // Base class for all statements
class DeclarationNode : StatementNode;  // Represents variable or data declarations
class AssignmentNode : StatementNode;   // Represents assignment statements
class IfNode : StatementNode;           // Represents if statements
class ForLoopNode : StatementNode;      // Represents both traditional and enhanced for loops
class WhileNode : StatementNode;        // Represents while loops
class ReturnNode : StatementNode;       // Represents return statements

// Definitions
class FunctionNode : ASTNode;           // Represents function definitions
class DataNode : ASTNode;               // Represents data definitions
class FuncNode : ASTNode;               // Represents func module definitions
class LinkNode : ASTNode;               // Represents links within entities
class EntityNode : ASTNode;             // Represents entities and their func/data relationships
class ErrorNode : ASTNode;              // Represents error type definitions
class ErrorValueNode : ASTNode;         // Represents errors and exceptions
class OptNode : ASTNode;                // Represents optional types
class VariantNode : ASTNode;            // Represents variant type definitions
class EnumNode : ASTNode;               // Represents enum type definitions
class ImportNode : ASTNode;             // Represents the use definitions

// Program Structure
class ProgramNode : ASTNode;            // Root node of the AST
```

## Linker

The Linker originally was part of the Lexer. One file was tokenized, then it was checked for fiile imports, and then the Linker would also tokenize the linked file and include all TokenContext elements at the index the import was in the TokenContext vector. This meant that
1. All Tokens would need to "know" from which file they were, making the TokenContext struct bigger than it needed to be
2. Circular references were a pain in the butt to detect
3. Caches of already compiled files were practically eliminated, making it impossible to skip files where no changes have taken place

This is why it was decided to move the Linker after the Parser, to ensure a much easier, smoother and more performant compiler pipeline.
