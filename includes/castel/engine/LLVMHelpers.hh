#pragma once

#include <cstdint>
#include <vector>

#include <llvm/Support/IRBuilder.h>
#include <llvm/BasicBlock.h>
#include <llvm/Constants.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Type.h>
#include <llvm/Value.h>
#include <mpllvm/mpllvm.hh>

#include "castel/engine/GenerationEngine.hh"
#include "castel/engine/Box.hh"
#include "castel/utils/mpllvmExtensions.hh"

namespace castel
{

    namespace engine
    {

        class LLVMHelpers
        {

        public:

            LLVMHelpers    ( llvm::LLVMContext & llvmLLVMContext, llvm::IRBuilder< > & irBuilder, llvm::Module & module )
            : mLLVMContext ( llvmLLVMContext )
            , mIRBuilder   ( irBuilder   )
            , mModule      ( module      )
            {
            }

        public:

            llvm::Value * sizeOf( llvm::Type * type )
            {
                /* See here : http://nondot.org/sabre/LLVMNotes/SizeOf-OffsetOf-VariableSizedStructs.txt */
                llvm::Type * targetType = llvm::IntegerType::get( mLLVMContext, 32 );
                llvm::ConstantPointerNull * nullPointer = llvm::ConstantPointerNull::get( llvm::PointerType::get( type, 0 ) );
                return mIRBuilder.CreatePtrToInt( mpllvm::GEP< std::int64_t >::build( mLLVMContext, mIRBuilder, nullPointer, 1 ), targetType );
            }

            llvm::Value * allocateObject( llvm::Type * type, bool allocateOnTheStack = false )
            {
                if ( allocateOnTheStack ) {
                    /* Allocating on the stack using Alloca */
                    return mIRBuilder.CreateAlloca( type );
                } else {
                    /* Allocating using the heap allocation function */
                    llvm::Function * castelMalloc = mModule.getFunction( "castelMalloc" );
                    llvm::Value * raw = mIRBuilder.CreateCall( castelMalloc, this->sizeOf( type ) );

                    /* Casting the result to get the right result type */
                    return mIRBuilder.CreateBitCast( raw, llvm::PointerType::getUnqual( type ) );
                }
            }

            template < typename Type >
            llvm::Value * allocateObject( bool allocateOnTheStack = false )
            {
                return this->allocateObject( mpllvm::get< Type >( mLLVMContext ), allocateOnTheStack );
            }

            llvm::Value * allocateArray( llvm::Type * type, int count, bool allocateOnTheStack = false )
            {
                /* Crafts the array type */
                llvm::Type * arrayType = llvm::ArrayType::get( type, count );

                /* Allocates memory for multiple objects */
                llvm::Value * array = this->allocateObject( arrayType, allocateOnTheStack );

                /* Casts this array reference into a pointer */
                return mpllvm::GEP< std::int64_t, std::int64_t >::build( mLLVMContext, mIRBuilder, array, 0, 0 );
            }

            template < typename Type >
            llvm::Value * allocateArray( int count, bool allocateOnTheStack = false )
            {
                return this->allocateArray( mpllvm::get< Type >( mLLVMContext ), count, allocateOnTheStack );
            }

        public:

            llvm::Function * boxToFunction( llvm::Value * genericBox, std::int32_t arity )
            {
                /* Ensures that the generic dynamic box is really a function */
                this->forceBoxType< engine::Box::Type::Function >( genericBox );

                /* Casts the generic dynamic box into a Function box */
                llvm::Value * functionBox = mIRBuilder.CreateBitCast( genericBox, llvm::PointerType::getUnqual( mModule.getTypeByName( "box.function" ) ) );

                /**** START : checks function arity ****/
                llvm::Value * arityIndex = mpllvm::GEP< std::int64_t, std::int32_t >::build( mLLVMContext, mIRBuilder, functionBox, 0, 1 );
                llvm::Value * functionArity = mIRBuilder.CreateLoad( arityIndex );
                llvm::Value * expectedArity = llvm::ConstantInt::get( mLLVMContext, llvm::APInt( 32, arity ) );
                llvm::Value * arityCheck = mIRBuilder.CreateICmpNE( functionArity, expectedArity );

                llvm::Function * outerFunction = mIRBuilder.GetInsertBlock( )->getParent( );

                llvm::BasicBlock * thenBranch = llvm::BasicBlock::Create( mLLVMContext, "then", outerFunction );
                llvm::BasicBlock * finallyBranch = llvm::BasicBlock::Create( mLLVMContext, "finally" );

                llvm::Value * conditionalBranching = mIRBuilder.CreateCondBr( arityCheck, thenBranch, finallyBranch );

                mIRBuilder.SetInsertPoint( thenBranch );

                llvm::Function * runtimeCastelCrash = mModule.getFunction( "castelCrash" );
                llvm::Value * errorMessage = mpllvm::GEP< std::int64_t >::build( mLLVMContext, mIRBuilder, llvm::ConstantPointerNull::get( mpllvm::get< char const * >( mLLVMContext ) ), 0 );
                mIRBuilder.CreateCall( runtimeCastelCrash, errorMessage );
                mIRBuilder.CreateUnreachable( );

                outerFunction->getBasicBlockList( ).push_back( finallyBranch );

                mIRBuilder.SetInsertPoint( finallyBranch );
                /**** END : checks function arity ****/

                /**** START : crafts function type ****/
                std::vector< llvm::Type * > argumentsTypes;
                argumentsTypes.push_back( mpllvm::get< engine::Box *** >( mLLVMContext ) );
                argumentsTypes.insert( argumentsTypes.begin( ), arity, mpllvm::get< engine::Box * >( mLLVMContext ) );

                llvm::FunctionType * functionType = llvm::FunctionType::get( mpllvm::get< engine::Box * >( mLLVMContext ), argumentsTypes, false );
                /**** END : crafts function type ****/

                /* Loads the LLVM function pointer (as void*) */
                llvm::Value * functionIndex = mpllvm::GEP< std::int64_t, std::int32_t >::build( mLLVMContext, mIRBuilder, functionBox, 0, 2 );
                llvm::Value * genericFunctionPointer = mIRBuilder.CreateLoad( functionIndex );

                /* Casts the function pointer from void* to an LLVM function pointer */
                return static_cast< llvm::Function * >( mIRBuilder.CreateBitCast( genericFunctionPointer, llvm::PointerType::getUnqual( functionType ) ) );

            }

            llvm::Value * functionToBox( llvm::Function * llvmFunction, llvm::Value * environment = nullptr )
            {
                /* Sets a default (empty) environment unless specified */
                if ( environment == nullptr )
                    llvm::ConstantPointerNull::get( mpllvm::get< engine::Box *** >( mLLVMContext ) );

                /* Allocates enough memory for the new box */
                llvm::Value * functionBox = this->allocateObject( mModule.getTypeByName( "box.function" ) );

                /* Compute fields indexes */
                llvm::Value * typeIndex        = mpllvm::GEP< std::int64_t, std::int32_t >::build( mLLVMContext, mIRBuilder, functionBox, 0, 0 );
                llvm::Value * arityIndex       = mpllvm::GEP< std::int64_t, std::int32_t >::build( mLLVMContext, mIRBuilder, functionBox, 0, 1 );
                llvm::Value * functionIndex    = mpllvm::GEP< std::int64_t, std::int32_t >::build( mLLVMContext, mIRBuilder, functionBox, 0, 2 );
                llvm::Value * environmentIndex = mpllvm::GEP< std::int64_t, std::int32_t >::build( mLLVMContext, mIRBuilder, functionBox, 0, 3 );

                /* Casts the function pointer from an LLVM function pointer to void* */
                llvm::Value * genericFunctionPointer = mIRBuilder.CreateBitCast( llvmFunction, mpllvm::get< void * >( mLLVMContext ) );

                /* Populate box data */
                mIRBuilder.CreateStore( this->boxType< engine::Box::Type::Function >( ), typeIndex );
                mIRBuilder.CreateStore( llvm::ConstantInt::get( mLLVMContext, llvm::APInt( 32, llvmFunction->arg_size( ) - 1 ) ), arityIndex );
                mIRBuilder.CreateStore( genericFunctionPointer, functionIndex );
                mIRBuilder.CreateStore( environment ? environment : llvm::ConstantPointerNull::get( mpllvm::get< engine::Box *** >( mLLVMContext ) ), environmentIndex );

                /* Casts the function box into a generic dynamic box */
                return this->boxToGeneric( functionBox );
            }

            llvm::Value * callFunctionBox( llvm::Value * genericBox, std::vector< llvm::Value * > const & arguments )
            {
                /* Tries to load the inner function */
                llvm::Function * llvmFunction = this->boxToFunction( genericBox, arguments.size( ) );

                /* Casts the generic dynamic box into a Function box */
                llvm::Value * functionBox = mIRBuilder.CreateBitCast( genericBox, llvm::PointerType::getUnqual( mModule.getTypeByName( "box.function" ) ) );

                /* Computes environment field index */
                llvm::Value * environmentIndex = mpllvm::GEP< std::int64_t, std::int32_t >::build( mLLVMContext, mIRBuilder, functionBox, 0, 3 );

                /* Loads environment */
                llvm::Value * environment = mIRBuilder.CreateLoad( environmentIndex );

                /* Duplicates the arguments and adds the environment at the front */
                std::vector< llvm::Value * > duplicatedArguments;
                duplicatedArguments.push_back( environment );
                duplicatedArguments.insert( duplicatedArguments.end( ), arguments.begin( ), arguments.end( ) );

                /* Finally returns the call */
                return mIRBuilder.CreateCall( llvmFunction, duplicatedArguments );
            }

        public:

            llvm::Value * boxToDouble( llvm::Value * genericBox )
            {
                /* Ensures that the generic dynamic box is really a function */
                this->forceBoxType< engine::Box::Type::Double >( genericBox );

                /* Casts the generic dynamic box into a Double box */
                llvm::Value * doubleBox = mIRBuilder.CreateBitCast( genericBox, llvm::PointerType::getUnqual( mModule.getTypeByName( "box.double" ) ) );

                /* Loads and returns the internal value */
                llvm::Value * valueIndex = mpllvm::GEP< std::int64_t, std::int32_t >::build( mLLVMContext, mIRBuilder, doubleBox, 0, 1 );
                return mIRBuilder.CreateLoad( valueIndex );
            }

            llvm::Value * doubleToBox( double n )
            {
                /* We just forward this action to the master helper */
                return this->doubleToBox( llvm::ConstantFP::get( mLLVMContext, llvm::APFloat( n ) ) );
            }

            llvm::Value * doubleToBox( llvm::Value * value )
            {
                /* Allocates enough memory for the new box */
                llvm::Value * doubleBox = this->allocateObject( mModule.getTypeByName( "box.double" ) );

                /* Compute fields indexes */
                llvm::Value * typeIndex  = mpllvm::GEP< std::int64_t, std::int32_t >::build( mLLVMContext, mIRBuilder, doubleBox, 0, 0 );
                llvm::Value * valueIndex = mpllvm::GEP< std::int64_t, std::int32_t >::build( mLLVMContext, mIRBuilder, doubleBox, 0, 1 );

                /* Populate box data */
                mIRBuilder.CreateStore( this->boxType< engine::Box::Type::Double >( ), typeIndex );
                mIRBuilder.CreateStore( value, valueIndex );

                /* Casts the double box into a generic dynamic box */
                return this->boxToGeneric( doubleBox );
            }

        public:

            llvm::Value * boxToGeneric( llvm::Value * box )
            {
                /* Casts the input box into a generic dynamic box. It does no check on the input */
                return mIRBuilder.CreateBitCast( box, llvm::PointerType::getUnqual( mModule.getTypeByName( "box" ) ) );
            }

        public:

            template < engine::Box::Type Type >
            llvm::Value * boxType( void ) const
            {
                /* Return an integer LLVM value containing the template parameter box type */
                return llvm::ConstantInt::get( mLLVMContext, llvm::APInt( 32, static_cast< std::int32_t>( Type ) ) );
            }

            template < engine::Box::Type Type >
            llvm::Value * forceBoxType( llvm::Value * value )
            {
                /* security check here */

                return value;
            }

        private:

            llvm::LLVMContext & mLLVMContext;
            llvm::IRBuilder< > & mIRBuilder;
            llvm::Module & mModule;

        };

    }

}
