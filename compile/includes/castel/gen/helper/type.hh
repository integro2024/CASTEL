#pragma once

#include <llvm/TypeBuilder.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Type.h>

#include "castel/runtime/boxes/Binding.hh"
#include "castel/runtime/boxes/Bool.hh"
#include "castel/runtime/boxes/Class.hh"
#include "castel/runtime/boxes/Dict.hh"
#include "castel/runtime/boxes/Function.hh"
#include "castel/runtime/boxes/List.hh"
#include "castel/runtime/boxes/Null.hh"
#include "castel/runtime/boxes/Number.hh"
#include "castel/runtime/boxes/Object.hh"
#include "castel/runtime/boxes/String.hh"
#include "castel/runtime/boxes/Undefined.hh"
#include "castel/runtime/Box.hh"

namespace llvm
{

    template <>
    struct TypeBuilder< bool, false > {
        static llvm::Type * get( llvm::LLVMContext & context ) {
            return llvm::Type::getInt1Ty( context );
        }
    };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::Box, XCompile > {
        static llvm::StructType * get( llvm::LLVMContext & context ) {
            llvm::Module tempModule( "", context );
            return tempModule.getTypeByName( "Box" );
        }
    };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::Binding, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::Bool, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::Class, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::Dict, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::Function, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::List, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::Null, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::Number, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::Object, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::String, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

    template < bool XCompile >
    struct TypeBuilder< castel::runtime::boxes::Undefined, XCompile >
        : public llvm::TypeBuilder< castel::runtime::Box, XCompile >
    { };

}

namespace castel
{

    namespace gen
    {

        namespace helper
        {

            template < typename T >
            inline auto type( llvm::LLVMContext & context ) -> decltype( llvm::TypeBuilder< T, false >::get( context ) )
            {
                return llvm::TypeBuilder< T, false >::get( context );
            }

        }

    }

}
