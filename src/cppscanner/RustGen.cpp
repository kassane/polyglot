// SPDX-FileCopyrightText: Loren Burkholder
//
// SPDX-License-Identifier: GPL-3.0

#include "RustGen.h"

#include <string>

#include "CppBindingGenerator.h"
#include "Utils.h"

std::string RustGen::getRustFileHeader(const std::string &moduleName)
{
    if (moduleName.size() <= 0)
        throw std::runtime_error("Null module name was passed");

    auto t = std::time(nullptr);
    std::string timeStr = std::asctime(std::localtime(&t));
    return std::format(
        R"(// *** WARNING: autogenerated file, do not modify. Changes will be overwritten. ***
// Generated by Polyglot version {} at {}.
// This file contains symbols that have been exported from other languages into Rust.)",
        POLYGLOT_VERSION,
        timeStr.substr(0, timeStr.size() - 1), // remove the '\n'
        moduleName);
}

std::string RustGen::getRustFileFooter()
{
    return {};
}

std::string RustGen::getBeginFunctionBlock()
{
    return "extern {";
}

std::string RustGen::getEndFunctionBlock()
{
    return "}";
}

std::string RustGen::getRustTypeString(const clang::QualType &type, const clang::Decl *decl)
{
    std::string typeString;
    if (type->isPointerType())
        typeString += "* ";
    if (type.isConstQualified())
        typeString += "const ";

    if (type->isVoidType() || type->isVoidPointerType())
        typeString += "void";
    if (type->isBooleanType())
        typeString += "bool";
    else if (type->isCharType())
        typeString += "char";
    else if (type->isChar16Type() || type->isWideCharType())
        typeString += "wchar";
    else if (type->isChar32Type())
        typeString += "dchar";
    else if (type->isIntegerType())
    {
        if (!Utils::isFixedWidthIntegerType(type))
        {
            auto &diagnostics = decl->getASTContext().getDiagnostics();
            auto id = diagnostics.getDiagnosticIDs()->getCustomDiagID(clang::DiagnosticIDs::Warning,
                                                                      "Use fixed-width integer types for "
                                                                      "portablility");
            diagnostics.Report(decl->getBeginLoc(), id);
        }

        if (type->isUnsignedIntegerType())
            typeString += 'u';
        else
            typeString += 'i';

        auto size = decl->getASTContext().getTypeSize(type);
        switch (size)
        {
        case 8:
        case 16:
        case 32:
        case 64:
        case 128:
            typeString += std::to_string(size);
            break;
        default:
            throw std::runtime_error(std::string("Unrecognized integer size: ") + std::to_string(size));
        }
    }
    else if (type->isFloatingType())
    {
        auto size = decl->getASTContext().getTypeSize(type);
        switch (size)
        {
        case 32:
        case 64:
            typeString += 'f' + std::to_string(size);
            break;
        default:
            throw std::runtime_error(std::string("Unrecognized floating-point size: ") + std::to_string(size));
        }
    }
    else if (auto enumType = type->getAs<clang::EnumType>(); enumType)
        typeString += enumType->getDecl()->getNameAsString();

    return typeString;
}

std::string RustGen::getRustExprValueString(const clang::Expr *defaultValue, const clang::ASTContext &context)
{
    clang::Expr::EvalResult result;
    if (!defaultValue->EvaluateAsConstantExpr(result, context))
        throw std::runtime_error("Failed to evaluate expression");

    //    if (result.Val.isNullPointer())
    //        return "null";

    std::string retval;
    llvm::raw_string_ostream stream(retval);
    defaultValue->printPretty(stream, nullptr, clang::PrintingPolicy(context.getLangOpts()));
    return retval;
}
