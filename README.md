# Porphyrin
A C-like language compiler

## nonterminal Sequence
    Source
    Declarations
    Declaration
    VarDeclaration
    FunDeclaration
    Const
    Type
    DeclaredVars
    DeclaredVar
    DecIndices
    Expr
    VariableUse
    UseIndices
    IdLeftBrace
    HereIsParameter
    Compound
    Parameters
    Parameter
    Statements
    Statement
    Expression
    If
    While
    Return
    IfSymbol
    RightBraceSetIfWhile
    WhileSymbol
    LeftCurlyBraceFore
    Comma
    Assignment
    LogicalOr
    LogicalAnd
    Comparison
    CmpSymbol
    Addition
    AddSymbol
    Multiplication
    MulSymbol
    Item
    Call
    Num
    HereIsArgument
    Arguments
    Argument

## Memory organization
### Stack Segment
Stack grow from High to Low  

| Address | Content     |
|---------|-------------|
| Low     |             |
|         | temp vars   |
|         | def vars    |
|         | old ebp     |
|         | return addr |
|         | arguments   |
| High    |             |