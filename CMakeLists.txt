cmake_minimum_required(VERSION 3.19)
project(Porphyrin)

set(CMAKE_CXX_STANDARD 14)


set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSEMANTIC_DEBUG")

include_directories(.)

add_executable(g--
        main.cpp
        Utils/PreProcess.cpp Include/PreProcess.h
        Utils/Lexical.cpp Include/Lexical.h
        Class/Character.cpp Class/Character.h
        Class/Nonterminal.cpp Class/Nonterminal.h
        Class/TerminalChar.cpp Class/TerminalChar.h
        Class/LR1item.cpp Class/LR1item.h
        Class/Closure.cpp Class/Closure.h
        Utils/Syntax.cpp Include/Syntax.h
        Class/LR1Table.cpp Class/LR1Table.h
        Class/Node.cpp Class/Node.h
        Utils/Semantic.cpp Utils/ControlStatement.cpp Include/Semantic.h Class/Diagnose.cpp Class/Diagnose.h Utils/ActionFunctionTable.cpp Utils/SemanticArrays.cpp)
