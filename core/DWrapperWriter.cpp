// SPDX-FileCopyrightText: Loren Burkholder
//
// SPDX-License-Identifier: GPL-3.0

#include "DWrapperWriter.h"

#include <algorithm>
#include <ctime>
#include <format>
#include <iostream>
#include <stack>
#include <string>

#include "Utils.h"

using namespace polyglot;

DWrapperWriter::~DWrapperWriter() {}

void DWrapperWriter::write(const AST &ast, std::ostream &out)
{
    auto t = std::time(nullptr);
    std::string timeStr = std::asctime(std::localtime(&t));
    out << std::format(
        R"(// *** WARNING: autogenerated file, do not modify. Changes will be overwritten. ***
// Generated by Polyglot version {} at {}.
// This file contains symbols that have been exported from {} into D.

module {};

@nogc:
)",
        Utils::POLYGLOT_VERSION,
        timeStr.substr(0, timeStr.size() - 1), // remove the '\n'
        "C++", // TODO: make this dynamic
        ast.moduleName);

    if (ast.language == Language::Cpp)
        out << "extern(C++):\n";
    out << "\n";

    for (const auto &node : ast.nodes)
    {
        if (node->cppNamespace == nullptr)
            m_namespaceOrganizer.childNodes.push_back(node);
        else
        {
            std::stack<std::string> namespaces;
            for (auto ns = node->cppNamespace; ns != nullptr; ns = ns->parentNamespace)
                namespaces.push(ns->name);
            NamespaceOrganizer *ns = &m_namespaceOrganizer;
            while (!namespaces.empty())
            {
                bool found{false};
                for (int i = 0; i < ns->childNamespaces.size(); ++i)
                {
                    if (ns->childNamespaces[i]->currentNamespace.name == namespaces.top())
                    {
                        ns = ns->childNamespaces[i];
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    auto childNs = new NamespaceOrganizer;
                    childNs->currentNamespace.name = namespaces.top();
                    ns->childNamespaces.push_back(childNs);
                    ns = childNs;
                }
                namespaces.pop();
            }
            ns->childNodes.push_back(node);
        }
    }

    writeFromNamespaceOrganizer(ast, &m_namespaceOrganizer, out);
    out.flush();
}

std::string DWrapperWriter::getTypeString(const QualifiedType &type) const
{
    std::string typeString;
    if (type.isConst)
        typeString += "const ";
    if (type.isReference)
        typeString += "ref ";

    switch (type.baseType)
    {
    case Type::Bool:
        typeString += "bool";
        break;
    case Type::Void:
        typeString += "void";
        break;
    case Type::Char:
        typeString += "char";
        break;
    case Type::Char16:
        typeString += "wchar";
        break;
    case Type::Char32:
        typeString += "dchar";
        break;
    case Type::Int8:
        typeString += "byte";
        break;
    case Type::Int16:
        typeString += "short";
        break;
    case Type::Int32:
        typeString += "int";
        break;
    case Type::Int64:
        typeString += "long";
        break;
    case Type::Int128:
        typeString += "cent";
        break;
    case Type::Uint8:
        typeString += "ubyte";
        break;
    case Type::Uint16:
        typeString += "ushort";
        break;
    case Type::Uint32:
        typeString += "uint";
        break;
    case Type::Uint64:
        typeString += "ulong";
        break;
    case Type::Uint128:
        typeString += "ucent";
        break;
    case Type::Float32:
        typeString += "float";
        break;
    case Type::Float64:
        typeString += "double";
        break;
    case Type::Float128:
        typeString += "real";
        break;
    case Type::Enum:
    case Type::Class:
        if (type.nameString.empty())
            throw std::runtime_error("Enum or class name was not provided to DWrapperWriter");
        else
            typeString += type.nameString;
        break;
    case Type::CppStdString:
        typeString += "basic_string";
        break;
    case Type::Undefined:
        throw std::runtime_error("Undefined type in DWrapperWriter::getTypeString()");
        break;
    }

    if (type.isPointer)
        typeString += " *";

    return typeString;
}

std::string DWrapperWriter::getValueString(const Value &value) const
{
    switch (value.type)
    {
    case Type::Bool:
        return std::to_string(std::get<bool>(value.value));
        break;
    case Type::Char:
        return std::to_string(std::get<char>(value.value));
        break;
    case Type::Char16:
        return std::to_string(std::get<char16_t>(value.value));
        break;
    case Type::Char32:
        return std::to_string(std::get<char32_t>(value.value));
        break;
    case Type::Int8:
    case Type::Int16:
    case Type::Int32:
    case Type::Int64:
        return std::to_string(std::get<int64_t>(value.value));
        break;
    case Type::Uint8:
    case Type::Uint16:
    case Type::Uint32:
    case Type::Uint64:
        return std::to_string(std::get<uint64_t>(value.value));
        break;
    case Type::Float32:
    case Type::Float64:
        return std::to_string(std::get<double>(value.value));
        break;
    case Type::Enum:
    case Type::Class:
        throw std::runtime_error("Enum or class expressions are not yet supported here");
        break;
    case Type::CppStdString:
        return std::get<std::string>(value.value);
        break;
    case Type::Int128:
    case Type::Uint128:
    case Type::Float128:
    case Type::Void:
    case Type::Undefined:
    default:
        throw std::runtime_error("Bad or unsupported type in DWrapperWriter::getValueString()");
        break;
    }
}

void DWrapperWriter::writeFromNamespaceOrganizer(const AST &ast, const NamespaceOrganizer *organizer, std::ostream &out)
{
    if (ast.language == Language::Cpp && !organizer->currentNamespace.name.empty())
    {
        out << std::string(m_indentationDepth, '\t') << "extern(C++, " << organizer->currentNamespace.name << ")\n";
        out << std::string(m_indentationDepth, '\t') << "{\n";
        ++m_indentationDepth;
    }

    auto writeFunctionString = [this, &ast, &out](const polyglot::FunctionNode &function, bool isClassMethod) {
        out << std::string(m_indentationDepth, '\t');
        if (ast.language != Language::Cpp)
            out << std::format(R"(pragma(mangle, "{}") )", function.mangledName);
        if (isClassMethod && !function.isVirtual)
            out << "final ";
        out << (function.isNoreturn ? "noreturn" : getTypeString(function.returnType)) << ' ' << function.functionName
            << '(';
        std::string params;
        for (const auto &param : function.parameters)
        {
            params += getTypeString(param.type) + ' ' + param.name;
            if (param.value.has_value())
                params += " = " + getValueString(param.value.value());
            params += ", ";
        }
        out << params.substr(0, params.size() - 2) + ");";
        // TODO: am I missing any other qualifiers?
    };

    auto previousNodeType = ASTNodeType::Undefined;
    for (const auto &node : organizer->childNodes)
    {
        if (node->nodeType() != ASTNodeType::Function ||
            (previousNodeType != ASTNodeType::Function && previousNodeType != ASTNodeType::Undefined))
            out << "\n";

        if (node->nodeType() == ASTNodeType::Function)
        {
            auto function = dynamic_cast<FunctionNode *>(node);
            if (function == nullptr)
                throw std::runtime_error("Node claimed to be FunctionNode, but cast failed");

            writeFunctionString(*function, false);
        }
        else if (node->nodeType() == ASTNodeType::Enum)
        {
            auto e = dynamic_cast<EnumNode *>(node);
            if (e == nullptr)
                throw std::runtime_error("Node claimed to be EnumNode, but cast failed");

            out << std::string(m_indentationDepth, '\t') << "enum " << e->enumName << '\n'
                << std::string(m_indentationDepth, '\t') << "{\n";
            for (const auto &enumerator : e->enumerators)
            {
                out << std::string(m_indentationDepth + 1, '\t') << enumerator.name;
                if (enumerator.value.has_value())
                    out << " = " + getValueString(enumerator.value.value());
                out << ",\n";
            }
            out << std::string(m_indentationDepth, '\t') << "}";
        }
        else if (node->nodeType() == ASTNodeType::Class)
        {
            auto classNode = dynamic_cast<ClassNode *>(node);
            if (classNode == nullptr)
                throw std::runtime_error("Node claimed to be ClassNode, but cast failed");

            out << std::string(m_indentationDepth, '\t');
            if (classNode->type == polyglot::ClassNode::Type::Class)
                out << "class ";
            else
                out << "struct ";
            out << classNode->name << '\n'
                << std::string(m_indentationDepth, '\t') << "{\n"
                << std::string(m_indentationDepth, '\t') << "public:\n";

            ++m_indentationDepth;
            for (const auto &constructor : classNode->constructors)
            {
                out << std::string(m_indentationDepth, '\t');
                // TODO: figure out why C++ constructors don't mangle properly
                out << std::format(R"(pragma(mangle, "{}") this()", constructor.mangledName);
                std::string params;
                for (const auto &param : constructor.parameters)
                {
                    params += getTypeString(param.type) + ' ' + param.name;
                    if (param.value.has_value())
                        params += " = " + getValueString(param.value.value());
                    params += ", ";
                }
                out << params.substr(0, params.size() - 2) + ");";
                out << "\n";
            }

            if (classNode->destructor.has_value())
            {
                out << std::string(m_indentationDepth, '\t');
                out << std::format(R"(pragma(mangle, "{}") )", classNode->destructor->mangledName);
                out << "~this();\n";
            }

            if (!classNode->methods.empty())
            {
                out << "\n";
                for (const auto &method : classNode->methods)
                {
                    writeFunctionString(method, true);
                    out << "\n";
                }
            }

            if (!classNode->members.empty())
            {
                out << "\n";
                for (const auto &member : classNode->members)
                {
                    out << std::string(m_indentationDepth, '\t') << getTypeString(member.type) + ' ' + member.name;
                    if (member.value.has_value())
                        out << " = " << getValueString(member.value.value());
                    out << ";\n";
                }
            }
            --m_indentationDepth;

            out << std::string(m_indentationDepth, '\t') << "}";
        }

        out << "\n";
        previousNodeType = node->nodeType();
    }

    for (const auto &ns : organizer->childNamespaces)
    {
        out << "\n";
        writeFromNamespaceOrganizer(ast, ns, out);
    }

    if (ast.language == Language::Cpp && !organizer->currentNamespace.name.empty())
    {
        --m_indentationDepth;
        out << std::string(m_indentationDepth, '\t') << "}\n";
    }
}
