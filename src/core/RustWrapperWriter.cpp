#include "RustWrapperWriter.h"

#include <ctime>
#include <format>
#include <string>

#include "../cppscanner/Utils.h"

using namespace polyglot;

void RustWrapperWriter::write(const AST &ast, std::ostream &out) const
{
    auto t = std::time(nullptr);
    std::string timeStr = std::asctime(std::localtime(&t));
    out << std::format(
        R"(// *** WARNING: autogenerated file, do not modify. Changes will be overwritten. ***
// Generated by Polyglot version {} at {}.
// This file contains symbols that have been exported from {} into Rust.
)",
        Utils::POLYGLOT_VERSION,
        timeStr.substr(0, timeStr.size() - 1), // remove the '\n'
        ast.moduleName,
        "C++"); // TODO: make this dynamic

    auto previousNodeType = ASTNodeType::Undefined;
    for (const auto &node : ast.nodes)
    {
        if (node->nodeType() == ASTNodeType::Function)
        {
            auto function = dynamic_cast<FunctionNode *>(node);
            if (function == nullptr)
                throw std::runtime_error("Node claimed to be FunctionNode, but cast failed");

            if (previousNodeType != ASTNodeType::Function)
                out << "\nextern {\n";

            out << std::format("\t" R"(#[link_name = "{}"] pub fn )", function->mangledName) << function->functionName << '(';

            std::string params;
            // note that Rust doesn't support default arguments
            for (const auto &param : function->parameters)
                params += param.name + ": " + getTypeString(param.type) + ", ";
            out << params.substr(0, params.size() - 2) + ')';

            if (function->returnType.baseType != Type::Void)
                out << " -> " << getTypeString(function->returnType);
            out << ";\n";
        }
        else
        {
            if (previousNodeType == ASTNodeType::Function)
                out << "}\n\n";

            if (node->nodeType() == ASTNodeType::Enum)
            {
                auto e = dynamic_cast<EnumNode *>(node);
                if (e == nullptr)
                    throw std::runtime_error("Node claimed to be EnumNode, but cast failed");

                out << "#[repr(C)]\npub enum " << e->enumName << "\n{\n";
                for (const auto &enumerator : e->enumerators)
                {
                    out << '\t' << enumerator.name;
                    if (enumerator.value.has_value())
                        out << " = " + getValueString(enumerator.value.value());
                    out << ",\n";
                }
                out << "}\n";
            }
        }

        previousNodeType = node->nodeType();
    }

    if (previousNodeType == ASTNodeType::Function)
        out << "}\n";

    out.flush();
}

std::string RustWrapperWriter::getTypeString(const QualifiedType &type) const
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
//    case Type::Char:
//        typeString += "char";
//        break;
//    case Type::Char16:
//        typeString += "wchar";
//        break;
    case Type::Char32:
        typeString += "char";
        break;
    case Type::Int8:
        typeString += "i8";
        break;
    case Type::Int16:
        typeString += "i16";
        break;
    case Type::Int32:
        typeString += "i32";
        break;
    case Type::Int64:
        typeString += "i64";
        break;
    case Type::Int128:
        typeString += "i128";
        break;
    case Type::Uint8:
        typeString += "u8";
        break;
    case Type::Uint16:
        typeString += "u16";
        break;
    case Type::Uint32:
        typeString += "u32";
        break;
    case Type::Uint64:
        typeString += "u64";
        break;
    case Type::Uint128:
        typeString += "u128";
        break;
    case Type::Float32:
        typeString += "f32";
        break;
    case Type::Float64:
        typeString += "f64";
        break;
//    case Type::Float128:
//        typeString += "real";
//        break;
    case Type::Enum:
    case Type::Class:
        if (type.nameString.has_value())
            typeString += type.nameString.value();
        else
            throw std::runtime_error("Enum or class name was not provided to RustWrapperWriter");
        break;
    case Type::CppStdString:
        typeString += "basic_string";
        break;
    case Type::Undefined:
    default:
        throw std::runtime_error("Undefined type in RustWrapperWriter::getTypeString()");
        break;
    }

    if (type.isPointer)
        typeString += " *";

    return typeString;
}

std::string RustWrapperWriter::getValueString(const Value &value) const
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
        throw std::runtime_error("Bad or unsupported type in RustWrapperWriter::getValueString()");
        break;
    }
}
