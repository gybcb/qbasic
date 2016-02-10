﻿/*
  Main Code Generation
  Copyright (C) 2012  microcai <microcai@fedoraproject.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <iostream>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Allocator.h>

#include "llvmwrapper.hpp"
#include "ast.hpp"
#include "type.hpp"

//#define debug	std::printf
#define debug(...)

llvm::BasicBlock* EmptyStmtAST::Codegen(ASTContext ctx)
{
    debug("empty statement called !\n");
    return ctx.block;
}

llvm::BasicBlock* PrintIntroAST::Codegen(ASTContext ctx)
{
    debug("PrintIntroAST expr for \n");
    return ctx.block;
}

//* 调用函数.
llvm::BasicBlock* ExprStmtAST::Codegen(ASTContext ctx)
{
    debug("call function? generating the call here!\n");
    // 然后调用吧.
    expr->getval(ctx);

    // 忽略返回数值.
    return ctx.block;
}

//TODO 为 print 语句生成.
llvm::BasicBlock* PrintStmtAST::Codegen(ASTContext ctx)
{
    bool need_brt = false;
    debug("generating llvm-IR for calling PRINT\n");
    assert(ctx.llvmfunc);

    //llvm::CallInst::Create();
    llvm::IRBuilder<> builder(ctx.llvmfunc->getContext());
    builder.SetInsertPoint(ctx.block);

    std::vector<llvm::Value*> args; // 先插入第3个开始的参数.
    std::string	printfmt;

    //第三个参数开始是 ... 参数对.
    if(! callargs->expression_list.empty()){
	// TODO : 支持字符串的版本修改第三个参数开始为参数对.
	//std::for_E
	for(auto argitem : callargs->expression_list)
	{
	    if(!argitem->type(ctx)){
		printfmt +="\n"; // 很重要,呵呵
		continue;
	    }
	    switch(argitem->type(ctx)->size()){ //按照大小来啊,果然.
		case sizeof(long): // 整数产量的类型.
		    if(argitem->type(ctx)->name(ctx) == "string"){
			printfmt += "%s\t";
			debug("add code for print list args type %%s\n");
		    }else{
			debug("add code for print list args type %%ld\n");
#if __x86_64
			printfmt += "%ld\t";
#else
			printfmt += "%d\t";
#endif
		    }
		    args.push_back(	argitem->getval(ctx) );
		    break;
#if __x86_64
		case sizeof(int):
		    printfmt += "%d\t";
		    args.push_back(	argitem->getval(ctx) );
		    break;
#endif
		case 0:
		    printfmt +="\n"; // 很重要,呵呵.
		default:
		    //TODO, 目前只需要支持 number , brt_print 也只是支持数字.
		    debug("print argument not supported\n");
	    }
	}
    }

    // 现在 brt 忽略第一个参数 , 其实质是 一个 map 到 FILE* 的转化, 由 btr_print 实现.
    // 第二个参数是打印列表.
    args.insert(args.begin(), builder.CreateGlobalStringPtr(printfmt));

    // builder.CreateCondBr();
    // 调用 print.
    if(need_brt){
	args.insert(args.begin(), qbc::getconstint(0) );

	llvm::Constant *brt_print =qbc::getbuiltinprotype(ctx,"brt_print");

	builder.CreateCall(brt_print,args ,"PRINT");
    }
    else{
	llvm::Constant *printf_func =qbc::getbuiltinprotype(ctx,"printf");

	builder.CreateCall(printf_func,args ,"PRINT_via_printf");
    }

    // delete the param list

    this->callargs.reset();

    return ctx.block;
}

// 获得分配的空间.
llvm::Value* VariableDimAST::getptr(ASTContext ctx)
{
    debug("get ptr of this alloca %p\n", alloca_var);
    return alloca_var;
}

// 获得变量的值.
llvm::Value* VariableDimAST::getval(ASTContext ctx)
{
    llvm::IRBuilder<> builder(ctx.block);

    return builder.CreateLoad(getptr(ctx));

#ifdef WIN32
    printf("%s\n", __FUNCTION__);
#else
    printf("%s\n", __func__);
#endif
    exit(1);
}

// 为变量分配空间.
llvm::BasicBlock* VariableDimAST::Codegen(ASTContext ctx)
{
    assert(ctx.llvmfunc);

    //map type name to type
    ExprTypeASTPtr exptype =  this->type;

    debug("allocate stack for var %s , type %s\n", name.c_str(), type->name(ctx).c_str());

    alloca_var = exptype->Alloca(ctx,this->name);

    // register with symbolic table
    ctx.codeblock->symbols.insert(std::make_pair(this->name,this));
    return ctx.block;
}

// de register with the symblic table
llvm::BasicBlock* VariableDimAST::valuedegen(ASTContext ctx)
{
    assert(ctx.llvmfunc);
    //map type name to type
    ExprTypeASTPtr exptype = this->type;
    debug("dellocate stack for var %s , type %s\n", name.c_str(), exptype->name(ctx).c_str());
    exptype->destory(ctx,alloca_var);
    return ctx.block;
}

// 把类型名注册到 typename table
llvm::BasicBlock* StrucDimAST::Codegen(ASTContext ctx)
{
    // 构建一个新的 ExprTypeAST*
    ExprTypeAST * newtype = new StructExprTypeAST(Typename);

    // 设定成员名字和类型的映射，还有名字和偏移

    //递归计算自己的大小.
    size_t	selfsize = 0;
    for(auto dimitem : this->members)
    {
	selfsize += dimitem->type->size();
    }

    return ctx.block;
    //dynamic_cast<StructExprTypeAST*>(this->type.get())->size(selfsize);
}

llvm::Value* ArgumentDimAST::getval(ASTContext ctx)
{
    debug("ArgumentDimAST:: geting val %s of argument\n", this->name.c_str());

    if(modified_stackvar){ // have local copy now
	llvm::IRBuilder<>	builder(ctx.block);
	return builder.CreateLoad(getptr(ctx));
    }
    // geting value from argument
    llvm::Function::arg_iterator arg_it =ctx.llvmfunc->arg_begin();
    for(;arg_it != ctx.llvmfunc->arg_end(); arg_it++){
	if(arg_it->getName() == this->name)
	    return arg_it;
    }
    debug("bug here %s \n",__FUNCTION__);
    exit(1);
}

llvm::Value* ArgumentDimAST::getptr(ASTContext ctx)
{
    debug("set val for argument\n");

    if(this->modified_stackvar){
	return modified_stackvar;
    }
    // REALLOCATE and update the pointer
    return this->modified_stackvar = this->type->Alloca(ctx,this->name);
}

llvm::BasicBlock* ArgumentDimAST::Codegen(ASTContext ctx)
{
    debug("generating function parameter of %s\n", this->name.c_str());

    llvm::Function::arg_iterator arg_it  = ctx.llvmfunc->arg_begin();

    int index = this->parent->find(this);
    for( int i = 0 ; i < index ; i++)
	arg_it ++;
    arg_it->setName(this->name);

    // register on symbols table

    ctx.codeblock->symbols.insert(std::make_pair(name,this));

    return ctx.block;
}

llvm::Value* FunctionDimAST::getptr(ASTContext ctx)
{
    return this->target;
}

// 设置返回值.
llvm::Value* FunctionDimAST::setret(ASTContext ctx,ExprASTPtr expr)
{
    llvm::IRBuilder<> builder(ctx.block);

    if(!retval)
	retval = static_cast<CallableExprTypeAST*>(type.get())->returntype->Alloca(ctx,"return value");

    llvm::Value* ret = expr->getval(ctx);

    builder.CreateStore(ret,ctx.func->retval);

    if(!returnblock)
	returnblock = llvm::BasicBlock::Create(ctx.module->getContext(), "ret",this->target);
    // jump to ret now !

    return builder.CreateBr(returnblock);
}

// 赋值语句, NOTE 直接调用赋值表达式.
llvm::BasicBlock* AssigmentAST::Codegen(ASTContext ctx)
{
    assert(ctx.llvmfunc);
    debug("called for number assigment\n");

    assignexpr->getval(ctx);
    return ctx.block;
}

llvm::BasicBlock* ReturnAST::Codegen(ASTContext ctx)
{
    ctx.func->setret(ctx,expr);
    return ctx.block;
}

// IF ELSE 语句.
llvm::BasicBlock* IFStmtAST::Codegen(ASTContext ctx)
{
    assert(ctx.llvmfunc);
    debug("if else statement\n");

    // true cond is always there
    llvm::BasicBlock* cond_true = llvm::BasicBlock::Create(ctx.llvmfunc->getContext(), "cond_true", ctx.llvmfunc);
    llvm::BasicBlock* cond_false ;

    if( this->_else){
	cond_false = llvm::BasicBlock::Create(ctx.llvmfunc->getContext(), "cond_false",ctx.llvmfunc);
    }
    llvm::BasicBlock* cond_continue = llvm::BasicBlock::Create(ctx.llvmfunc->getContext(), "continue", ctx.llvmfunc);

    if(! this->_else){
	cond_false = cond_continue;
    }

    llvm::IRBuilder<> builder(ctx.block);

    llvm::Value * expcond = this->_expr->getval(ctx);

    expcond = builder.CreateIntCast(expcond,qbc::getbooltype(),1);

    expcond = builder.CreateICmpNE(expcond, qbc::getconstfalse(), "tmp");
    builder.CreateCondBr(expcond, cond_true, cond_false);

    // generating true
    ctx.block = cond_true;

    this->_then->parent = ctx.codeblock;// NOTE important
    this->_then->Codegen(ctx);
    builder.SetInsertPoint(cond_true);
    builder.CreateBr(cond_continue);

    // generating false , if there is any
    if( this->_else){
	this->_else->parent = ctx.codeblock;// NOTE important
	ctx.block = cond_false;
	this->_else->Codegen(ctx);
	builder.SetInsertPoint(cond_false);
	builder.CreateBr(cond_continue);
    }
    builder.CreateBr(cond_continue);
    return cond_continue;
}

llvm::BasicBlock* LoopAST::bodygen(ASTContext ctx)
{
    loopbody->parent = ctx.codeblock;
    ctx.codeblock = loopbody.get();
    llvm::BasicBlock* newblo =  loopbody->Codegen(ctx);
    return newblo;
}

llvm::BasicBlock* WhileLoopAST::Codegen(ASTContext ctx)
{
    assert(ctx.llvmfunc);
    debug("generation code for while statement\n");

    llvm::BasicBlock* cond_while =
	llvm::BasicBlock::Create(ctx.llvmfunc->getContext(), "while", ctx.llvmfunc);

    llvm::BasicBlock* while_body =
	llvm::BasicBlock::Create(ctx.llvmfunc->getContext(), "whileloop", ctx.llvmfunc);

    llvm::BasicBlock* cond_continue =
	llvm::BasicBlock::Create(ctx.llvmfunc->getContext(), "whileend", ctx.llvmfunc);

    llvm::IRBuilder<> builder(ctx.block);
    //builder.SetInsertPoint(ctx.block);
    builder.CreateBr(cond_while);


    builder.SetInsertPoint(cond_while);
    ctx.block = cond_while;
    llvm::Value * expcond = this->condition->getval(ctx);
    expcond = builder.CreateIntCast(expcond,qbc::getbooltype(),true);
    expcond = builder.CreateICmpEQ(expcond, qbc::getconstfalse(), "tmp");
    builder.CreateCondBr(expcond, cond_continue, while_body);

    ctx.block = while_body;
    while_body = this->bodygen(ctx);
    builder.SetInsertPoint(while_body);
    builder.CreateBr(cond_while);

    cond_continue->moveAfter(while_body);

    return cond_continue;
}

//TODO, 生成 for loop
llvm::BasicBlock* ForLoopAST::Codegen(ASTContext ctx)
{
    ExprTypeASTPtr exprtype  = this->refID->type(ctx);

    // 变量赋予初始值.
    exprtype->getop()->operator_assign(ctx,refID,start);

    llvm::IRBuilder<> builder(ctx.block);

    llvm::BasicBlock* for_cond = llvm::BasicBlock::Create(ctx.llvmfunc->getContext(), "for", ctx.llvmfunc);
    llvm::BasicBlock* for_body = llvm::BasicBlock::Create(ctx.llvmfunc->getContext(), "forbody", ctx.llvmfunc);
    llvm::BasicBlock* for_out = llvm::BasicBlock::Create(ctx.llvmfunc->getContext(), "forend", ctx.llvmfunc);

    builder.CreateBr(for_cond);
    builder.SetInsertPoint(for_cond);


    ctx.block = for_cond; // 切换到  for_cond 生成代码.
    // 测试条件是否成立.
    llvm::Value * condval = exprtype->getop()->operator_comp(ctx,OPERATOR_LESSEQU,refID,end)->getval(ctx);

    condval = builder.CreateIntCast(condval,qbc::getbooltype(),1);
    builder.CreateCondBr(condval,for_body,for_out);

    ctx.block = for_body;
    ctx.block =	bodygen(ctx);

    builder.SetInsertPoint(ctx.block);

    // 为变量+1.
    ExprASTPtr tmp = exprtype->getop()->operator_add(ctx,refID,step);

    exprtype->getop()->operator_assign(ctx,refID,tmp);

    builder.CreateBr(for_cond);


    ctx.block = for_out;
    return for_out;
    debug("=========generating for loog======\n");
    //exit(2);
}


llvm::BasicBlock* CodeBlockAST::Codegen(ASTContext ctx)
{
    if(!ctx.llvmfunc ){
	debug("statements called with ctx.llvmfunc=null\n");
    }else{
	debug("statements called with good ctx.llvmfunc\n");
    }

    ctx.codeblock = this;

    for(auto stmt : statements)
    {
	if(stmt){
	    ctx.block =  stmt->Codegen(ctx);
	}
	else
	    debug("strange, stmt is null\n");
    }
    return ctx.block;
}

llvm::BasicBlock* CodeBlockAST::GenLeave(ASTContext ctx)
{
    //查找 block 里定义的变量, 撤销他们!.
    // register deallocate functions!
    // NOTE: in reverse order
    std::map< std::string, DimAST* >::reverse_iterator it = this->symbols.rbegin() , end = this->symbols.rend();
    for(;it != end ; it ++){
	ctx.block = it->second->valuedegen(ctx);
    }
    // clear the symblic table
    this->symbols.clear();
    //TODO , generate jump to the endblock
    return ctx.block;
}

// 生成函数 参数和反回值支持.
llvm::BasicBlock* FunctionDimAST::Codegen(ASTContext ctx)
{
    assert(!ctx.llvmfunc);
    assert(!ctx.block);

    ctx.func = this; // 设定当前函数.
    llvm::BasicBlock * blockforret = ctx.block;

    debug("generating function %s and its body now\n", this->name.c_str());

    //首先生成全局可用的外部辅助函数.
    llvm::IRBuilder<> builder(llvm::getGlobalContext());

    // 参数生成 args
    //为 ARG 生成代码!.
    std::vector<llvm::Type*>	args;

    //TODO need re-work

    if(callargs){
	std::vector< StatementASTPtr >::iterator it = callargs->statements.begin();

	for(; it != callargs->statements.end() ; it++)
	{
	    StatementASTPtr stp = *it;
	    ArgumentDimAST * dim = static_cast<ArgumentDimAST*>( stp );

	    args.push_back(dim->type->llvm_type(ctx));
	}
    }

    //函数返回类型.
    llvm::FunctionType *funcType =
	llvm::FunctionType::get(type ? type->llvm_type(ctx) : builder.getVoidTy(),args,true);

    target = ctx.llvmfunc =
	llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, this->name , ctx.module);

    llvm::BasicBlock *entry = llvm::BasicBlock::Create(builder.getContext(), "entrypoint", ctx.llvmfunc);

    //挂到全局名称表中.
    ctx.codeblock->symbols.insert(std::make_pair(this->name,this));

    //开始生成代码.
    ctx.block = entry;

    // 为参数设定 name
    if( callargs){
	this->callargs->parent = ctx.codeblock;
	ctx.block = callargs->Codegen(ctx);
	ctx.codeblock = this->callargs.get();
    }

    retval = static_cast<CallableExprTypeAST*>(type.get())->returntype->Alloca(ctx,"return value");

    //now code up the function body
    body->parent = ctx.codeblock;
    llvm::BasicBlock * bodyblock = body->Codegen(ctx);

    if(bodyblock != ctx.block){
	debug("body block changed!!!!\n");
    }
    ctx.block = bodyblock;

    if(returnblock){
	builder.CreateBr(returnblock);
	returnblock->moveAfter(bodyblock);
	builder.SetInsertPoint(returnblock);
	ctx.block = returnblock;
    }

    //生成变量撤销操作.
    ctx.block = body->GenLeave(ctx);
    builder.SetInsertPoint(ctx.block);

    if(retval)
	builder.CreateRet(builder.CreateLoad(retval));
    else if(type)
	builder.CreateRet(qbc::getconstlong(0)); // 返回 0.
    else
	builder.CreateRetVoid();
    return blockforret;
}
