# Porphyrin
A C-like language compiler

## Compile Options
```
[optional]          -twin               设定汇编代码风格为Windows(函数名前无下划线_)
                    -o targetfile       设定目标文件
[optional]          -p                  存储中间结果到硬盘
                                            符号表（语法分析结果）
                                            函数表（语法分析结果）
                                            语法树可视化分布
                                            语义分析结果四元式
                                            警告信息
                                            错误信息
                                            优化结果四元式
                                            优化DAG可视化分布
                                            执行阶段
```

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

## Register Alloc Logic
对于result := opr1 op opr2  
如果opr1的值保存在寄存器B中，且B中仅保存opr1的值：
    如果opr1的值之后不会被引用，或者opr1和result为同一个符号，直接返回B
