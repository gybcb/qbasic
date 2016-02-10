﻿/*
    defination of QBASIC Abstruct Syntax Tree
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


#ifndef __AST_H__
#define __AST_H__

#include <string>
#include <list>
#include <map>
#include <iostream>
#include <memory>

//#include <boost/shared_ptr.hpp>
//#include <boost/scoped_ptr.hpp>

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include "qbc.h"

enum MathOperator {
	OPERATOR_ADD = 1 , // +
	OPERATOR_SUB , // -
	OPERATOR_MUL , // *
	OPERATOR_DIV , // /
	OPERATOR_MOD , // % , MOD
	OPERATOR_POWER , // ^

	OPERATOR_EQUL , // == , =
	OPERATOR_NOTEQUL, // <> , >< , != as in basic
	OPERATOR_LESS , // <
	OPERATOR_LESSEQU , // <=
	OPERATOR_GREATER , // >
	OPERATOR_GREATEREQUL , // >=
};


enum Linkage {
	STATIC = 1,			// 静态函数，无导出.
	EXTERN,				// 导出函数.
	IMPORTC,			// 导入C函数，这样就可以使用 C 函数调用了，算是我提供的一个扩展吧.
};

enum ReferenceType {
	BYVALUE,		// 传值.
	BYREF,			// 引用，实质就是指针了. 函数的默认参数是传引用.
};

class CodeBlockAST;
class FunctionDimAST;
// 逐层传递 给各个 virtual 操作, 虽然很大, 但是不传指针, 直接传值.
class ASTContext
{
public:
	ASTContext()
		: llvmfunc(0)
		, codeblock(0)
		, block(0)
		, func(0)
	{}

	llvm::Function*		llvmfunc;	// 当前的函数位置.
	CodeBlockAST*		codeblock;	// 当前的代码块位置, 用于符号表.
	llvm::BasicBlock*	block;		// 当前的插入位置.
	llvm::Module*		module;		// 模块.
	FunctionDimAST*		func;
};

// allow us to use shared ptr to manage the memory
class AST // :public boost::enable_shared_from_this<AST>
{
public:
	AST();
	virtual ~AST() = 0; // use pure virtual deconstruction
private:
	AST(const AST &);
	AST & operator =(const AST &);
};

// 语句有, 声明语句和表达式语句和函数调用语句.
class StatementAST: public AST
{
public:
    StatementAST():parent(0){}
	CodeBlockAST * parent;	// 避免循环引用 :) , 用在查找变量声明的时候用到. 回溯法.
	std::string	LABEL;		// label , if there is. then we can use goto
							// must be uniq among function bodys
	virtual llvm::BasicBlock* Codegen(ASTContext) = 0;
	virtual ~StatementAST() {}
};
typedef StatementAST* StatementASTPtr;

typedef std::list<StatementASTPtr> StatementsAST;

class EmptyStmtAST : public StatementAST
{
public:
	EmptyStmtAST() {}
    llvm::BasicBlock* Codegen(ASTContext);
};

#include "type.hpp"

// dim 就是定义一个名字,变量或者函数/过程.
class DimAST: public StatementAST
{
public:
	DimAST(const std::string _name, ExprTypeASTPtr _type);
	// ExprType type; // the type of the expresion
	std::string		name; //定义的符号的名字.
	ExprTypeASTPtr	type; //定义的符号的类型. 将在 typetable 获得type的定义.
	virtual llvm::BasicBlock* Codegen(ASTContext) = 0; // generate alloca
	// gengerate deconstructions , default is don't do that, override on subclass
	virtual llvm::BasicBlock* valuedegen(ASTContext ctx){ return ctx.block; };

	virtual	llvm::Value*	getptr(ASTContext ctx) = 0; // the location for the allocated value
	virtual	llvm::Value*	getval(ASTContext ctx) = 0;
	
    virtual ~DimAST() {}
};

typedef std::shared_ptr<DimAST>	DimASTPtr;


class AssigmentAST : public StatementAST
{
	AssignmentExprASTPtr	assignexpr;
public:
    AssigmentAST(NamedExprAST* lval, ExprAST* rval);	
    llvm::BasicBlock* Codegen(ASTContext);
};

// 一个表达式构成的空语句, 当然, 最有可能的是函数调用而已.
class ExprStmtAST : public StatementAST
{
	ExprASTPtr		expr;
public:
	ExprStmtAST(ExprAST * exp);
    virtual llvm::BasicBlock* Codegen(ASTContext);
};

// 有调用就有返回, 这个是返回语句.
class ReturnAST : public StatementAST
{
	ExprASTPtr			expr;
public:
    ReturnAST(ExprAST* expr);
    virtual llvm::BasicBlock* Codegen(ASTContext);
};

// 代码块, 主要用来隔离变量的作用域. 在 BASIC 中, 代码只有2个作用域 : 全局和本地变量
// 代码块包含有符号表, 符号表由变量声明或者定义语句填充.
// 执行符号查找的时候就是到这里查找符号表.
class FunctionDimAST;
class CodeBlockAST : public StatementAST
{
public:
	std::vector<StatementASTPtr>			statements;	

	CodeBlockAST*							parent;		// 父作用域.
	std::map<std::string, DimAST*>			symbols;	// 符号表, 映射到定义语句,获得定义语句.

    virtual llvm::BasicBlock* Codegen(ASTContext ctx);
	virtual llvm::BasicBlock* GenLeave(ASTContext);

	int  find(StatementAST* child);
	void addchild(StatementAST* item);
	void addchild(StatementsAST* items);

	CodeBlockAST() : parent(NULL) {}
    CodeBlockAST(StatementsAST * items);
	CodeBlockAST(StatementAST * item);

};
typedef std::shared_ptr<CodeBlockAST>	CodeBlockASTPtr;

// IF 语句.
class IFStmtAST : public StatementAST
{
public:
	IFStmtAST(ExprASTPtr expr) : _expr(expr) {}
	ExprASTPtr _expr;
	CodeBlockASTPtr _then;
	CodeBlockASTPtr _else;
    virtual llvm::BasicBlock* Codegen(ASTContext);
};

// 所有循环语句的基类, while until for 三种循环.
class LoopAST : public StatementAST
{
	CodeBlockASTPtr	loopbody; // 循环体.
public:
	LoopAST(CodeBlockAST* body) : loopbody(body) {}
	// 生成循环体.
    llvm::BasicBlock*		  bodygen(ASTContext);
    virtual llvm::BasicBlock* Codegen(ASTContext) = 0;
};

// while 语句.
class WhileLoopAST : public LoopAST
{
	//循环条件.
	ExprASTPtr	condition;
public:
	WhileLoopAST(ExprASTPtr _condition, CodeBlockAST* body);

    virtual llvm::BasicBlock* Codegen(ASTContext);
};

class ForLoopAST : public LoopAST
{
	NamedExprASTPtr	refID;
	ExprASTPtr		start,end;
	ExprASTPtr		step;

public:
    ForLoopAST(NamedExprAST * id, ExprAST * start, ExprAST * end, ExprAST * step, CodeBlockAST* body);
    virtual llvm::BasicBlock* Codegen(ASTContext);
};

class VariableDimAST : public DimAST
{
	llvm::Value* alloca_var;
public:
	VariableDimAST(const std::string _name,  ExprTypeASTPtr _type);
	virtual llvm::BasicBlock* Codegen(ASTContext);
    virtual llvm::BasicBlock* valuedegen(ASTContext ctx);
    virtual llvm::Value* getptr(ASTContext ctx);
	virtual	llvm::Value* getval(ASTContext ctx);
};
typedef std::shared_ptr<VariableDimAST> VariableDimASTPtr;
typedef std::list<VariableDimASTPtr> VariableDimList;

// 结构体声明.
class StrucDimAST : public StatementAST
{
public:
    StrucDimAST(const std::string _typename,VariableDimList members);
	std::string	Typename; //新定义的结构体的类型.
	VariableDimList members; //成员列表
    virtual llvm::BasicBlock* Codegen(ASTContext ctx); // 这里要到父codebloks注册自己这个类型，以便后续声明使用.
};

class ArgumentDimAST : public VariableDimAST
{
	// 如果没有本地修改, 就可以直接使用传入的变量,如果本地进行修改了, 就重新分配一个.
	llvm::Value* modified_stackvar;
public:
	ArgumentDimAST(const std::string _name, ExprTypeASTPtr	_type);
	virtual llvm::BasicBlock* Codegen(ASTContext);
    virtual llvm::Value* getptr(ASTContext ctx);
	virtual	llvm::Value* getval(ASTContext ctx);
};
typedef std::shared_ptr<ArgumentDimAST> ArgumentDimASTPtr;

//变量定义也是一个父 codeblock , 这个 codeblock 将成为函数体 codeblock 的父节点.
//但是和函数体积本身不处在同一个节点,故而在函数内部定义的同名变量将导致参数无法访问,吼吼.
class ArgumentDimsAST : public CodeBlockAST
{
public:
	std::vector<ArgumentDimASTPtr> dims;
    ArgumentDimsAST() {}
};
typedef std::shared_ptr<ArgumentDimsAST> ArgumentDimsASTPtr;

//函数 AST , 有 body 没 body 来区别是不是定义或者声明.
//函数即是声明又是代码块, 故而这里使用了双重继承.
// NOTE : 小心使用双重继承.
class FunctionDimAST: public DimAST
{
	friend class ReturnAST;
private:
	llvm::BasicBlock*		returnblock; // insert before this ! please !
	llvm::Function*			target;
	llvm::Value*			retval; // allocated for return value, should use that for return.
	llvm::Value*			setret(ASTContext ctx, ExprASTPtr expr);
public:
	
	Linkage		linkage; //链接类型。static? extern ?
	std::list<VariableDimASTPtr> args_type; //checked by CallExpr.
	ArgumentDimsASTPtr	callargs; // 加到 body 这个 codeblock 符号表,这样 body 能访问到.
	CodeBlockASTPtr	body; //函数体.

    FunctionDimAST(const std::string _name, ExprTypeASTPtr, ArgumentDimsAST * callargs = NULL);
	//如果是声明, 为 dim 生成 llvm::Function * 声明供使用.
    virtual llvm::BasicBlock* Codegen(ASTContext);

	//get a pointer to function
    virtual	llvm::Value* getptr(ASTContext ctx);
	virtual	llvm::Value* getval(ASTContext ctx)
	{
		return this->target;
	}
};

class DefaultMainFunctionAST : public FunctionDimAST
{
public:
	DefaultMainFunctionAST(CodeBlockAST * body);
};

typedef std::list<ExprASTPtr>	FunctionParameterListAST;

////////////////////////////////////////////////////////////////////////////////
//内建函数语句. PRINT , 为 PRINT 生成特殊的函数调用:)
////////////////////////////////////////////////////////////////////////////////

//打印目的地. 默认打印到屏幕.
class PrintIntroAST : public ConstNumberExprAST
{
public:
	PrintIntroAST();
    llvm::BasicBlock* Codegen(ASTContext);
};
typedef std::shared_ptr<PrintIntroAST> PrintIntroASTPtr;

class PrintStmtAST: public StatementAST
{
public:
	PrintIntroASTPtr	print_intro;
	ExprListASTPtr		callargs;
	PrintStmtAST(PrintIntroAST *, ExprListAST *);
    virtual llvm::BasicBlock* Codegen(ASTContext);
};

#endif // __AST_H__

