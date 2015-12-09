/***********************************************************************************************************************
 **
 ** Copyright (c) 2011, 2014 ETH Zurich
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 ** following conditions are met:
 **
 **    * Redistributions of source code must retain the above copyright notice, this list of conditions and the
 **      following disclaimer.
 **    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 **      following disclaimer in the documentation and/or other materials provided with the distribution.
 **    * Neither the name of the ETH Zurich nor the names of its contributors may be used to endorse or promote products
 **      derived from this software without specific prior written permission.
 **
 **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 ** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 ** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 ** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 ** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 ** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 ** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **
 **********************************************************************************************************************/

#include "ClangAstVisitor.h"
#include "ExpressionVisitor.h"
#include "../CppImportUtilities.h"
#include "TemplateArgumentVisitor.h"

#include <clang/AST/Comment.h>

namespace CppImport {

ClangAstVisitor::ClangAstVisitor(OOModel::Project* project, CppImportLogger* logger)
 : macroImporter_(project, envisionToClangMap_, clang_), log_{logger}
{
	trMngr_ = new TranslateManager(clang_, project, this);
	exprVisitor_ = new ExpressionVisitor(this, log_);
	utils_ = new CppImportUtilities(log_, exprVisitor_, envisionToClangMap_);
	exprVisitor_->setUtilities(utils_);
	trMngr_->setUtils(utils_);
	templArgVisitor_ = new TemplateArgumentVisitor(exprVisitor_, utils_, log_);
	ooStack_.push(project);

	commentParser_ = new CommentParser();
}

ClangAstVisitor::~ClangAstVisitor()
{
	SAFE_DELETE(utils_);
	SAFE_DELETE(exprVisitor_);
	SAFE_DELETE(trMngr_);

	SAFE_DELETE(commentParser_);
}

void ClangAstVisitor::setSourceManagerAndPreprocessor(const clang::SourceManager* sourceManager,
																		const clang::Preprocessor* preprocessor)
{
	Q_ASSERT(sourceManager);
	Q_ASSERT(preprocessor);
	clang_.setSourceManager(sourceManager);
	clang_.setPreprocessor(preprocessor);
	macroImporter_.startTranslationUnit();
}

Model::Node*ClangAstVisitor::ooStackTop()
{
	return ooStack_.top();
}

void ClangAstVisitor::pushOOStack(Model::Node* node)
{
	Q_ASSERT(node);
	ooStack_.push(node);
}

Model::Node*ClangAstVisitor::popOOStack()
{
	return ooStack_.pop();
}

bool ClangAstVisitor::VisitDecl(clang::Decl* decl)
{
	if (!shouldImport(decl->getLocation()))
		return true;

	if (decl && !llvm::isa<clang::AccessSpecDecl>(decl))
	{
		log_->writeError(className_, decl, CppImportLogger::Reason::NOT_SUPPORTED);
		return true;
	}
	return Base::VisitDecl(decl);
}

bool ClangAstVisitor::TraverseNamespaceDecl(clang::NamespaceDecl* namespaceDecl)
{
	if (!shouldImport(namespaceDecl->getLocation()))
		return true;
	OOModel::Module* ooModule = trMngr_->insertNamespace(namespaceDecl);
	if (!ooModule)
	{
		log_->writeError(className_, namespaceDecl, CppImportLogger::Reason::INSERT_PROBLEM);
		// this is a severe error which should not happen therefore stop visiting
		return false;
	}
	// visit the body of the namespace
	ooStack_.push(ooModule);
	clang::DeclContext::decl_iterator it = namespaceDecl->decls_begin();
	for (;it!=namespaceDecl->decls_end();++it)
	{
		TraverseDecl(*it);
	}
	ooStack_.pop();
	return true;
}

bool ClangAstVisitor::TraverseClassTemplateDecl(clang::ClassTemplateDecl* classTemplate)
{
	if (!shouldImport(classTemplate->getLocation()) || !classTemplate->isThisDeclarationADefinition())
		return true;

	OOModel::Class* ooClass = nullptr;
	if (trMngr_->insertClassTemplate(classTemplate, ooClass))
	{
		TraverseClass(classTemplate->getTemplatedDecl(), ooClass);
		// visit type arguments if any
		auto templateParamList = classTemplate->getTemplateParameters();
		for (auto templateParameter : *templateParamList)
			ooClass->typeArguments()->append(templArgVisitor_->translateTemplateArg(templateParameter));
	}
	return true;
}

bool ClangAstVisitor::TraverseClassTemplateSpecializationDecl
(clang::ClassTemplateSpecializationDecl* specializationDecl)
{
	if (!shouldImport(specializationDecl->getLocation()))
		return true;

	//	explicit instation declaration
	if (!specializationDecl->isExplicitSpecialization() && specializationDecl->isExplicitInstantiationOrSpecialization())
	{
		auto ooExplicitTemplateInst = trMngr_->insertExplicitTemplateInstantiation(specializationDecl);
		if (!ooExplicitTemplateInst)
			// already inserted
			return true;

		auto typeLoc = specializationDecl->getTypeAsWritten()->getTypeLoc()
							.castAs<clang::TemplateSpecializationTypeLoc>();

		auto ooRef = createReference(typeLoc.getTemplateNameLoc());
		for (unsigned i = 0; i < typeLoc.getNumArgs(); i++)
			ooRef->typeArguments()->append(utils_->translateTemplateArgument(typeLoc.getArgLoc(i)));

		if (specializationDecl->getQualifier())
			ooRef->setPrefix(utils_->translateNestedNameSpecifier(specializationDecl->getQualifierLoc()));

		ooExplicitTemplateInst->setInstantiatedClass(ooRef);
		// add to tree
		if (auto decl = DCast<OOModel::Declaration>(ooStack_.top()))
			decl->subDeclarations()->append(ooExplicitTemplateInst);
		else if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
			itemList->append(new OOModel::DeclarationStatement(ooExplicitTemplateInst));
		else
			log_->writeError(className_, specializationDecl, CppImportLogger::Reason::INSERT_PROBLEM);
		return true;
	}

	OOModel::Class* ooClass = nullptr;
	if (trMngr_->insertClassTemplateSpec(specializationDecl, ooClass))
	{
		TraverseClass(specializationDecl, ooClass);

		auto originalParams = specializationDecl->getSpecializedTemplate()->getTemplateParameters();
		auto typeLoc = specializationDecl->getTypeAsWritten()->getTypeLoc()
							.castAs<clang::TemplateSpecializationTypeLoc>();
		// visit type arguments if any
		for (unsigned i = 0; i < typeLoc.getNumArgs(); i++)
		{
			auto typeArg = new OOModel::FormalTypeArgument();
			typeArg->setSpecializationExpression(utils_->translateTemplateArgument(typeLoc.getArgLoc(i)));
			typeArg->setName(QString::fromStdString(originalParams->getParam(i)->getNameAsString()));
			ooClass->typeArguments()->append(typeArg);
		}
	}
	return true;
}

bool ClangAstVisitor::TraverseCXXRecordDecl(clang::CXXRecordDecl* recordDecl)
{
	if (!shouldImport(recordDecl->getLocation()) || !recordDecl->isThisDeclarationADefinition())
		return true;

	OOModel::Class* ooClass = nullptr;
	if (trMngr_->insertClass(recordDecl, ooClass))
	{
		TraverseClass(recordDecl, ooClass);
		// visit type arguments if any
		if (auto describedTemplate = recordDecl->getDescribedClassTemplate())
		{
			auto templateParamList = describedTemplate->getTemplateParameters();
			for (auto templateParameter : *templateParamList)
				ooClass->typeArguments()->append(templArgVisitor_->translateTemplateArg(templateParameter));
		}
	}
	else
		log_->writeError(className_, recordDecl, CppImportLogger::Reason::NOT_SUPPORTED);
	return true;
}

bool ClangAstVisitor::TraverseFunctionDecl(clang::FunctionDecl* functionDecl)
{
	if (!shouldImport(functionDecl->getLocation()) || llvm::isa<clang::CXXMethodDecl>(functionDecl))
		return true;

	if (auto ooFunction = trMngr_->insertFunctionDecl(functionDecl))
	{
		if (!ooFunction->parent())
		{
			// insert in tree
			if (auto curProject = DCast<OOModel::Project>(ooStack_.top()))
				curProject->methods()->append(ooFunction);
			else if (auto curModule = DCast<OOModel::Module>(ooStack_.top()))
				curModule->methods()->append(ooFunction);
			else if (auto curClass = DCast<OOModel::Class>(ooStack_.top()))
			{
				// happens in the case of friend functions
				log_->writeError(className_, functionDecl, CppImportLogger::Reason::NOT_SUPPORTED);
				curClass->methods()->append(ooFunction);
			}
			else if (auto statements = DCast<OOModel::StatementItemList>(ooStack_.top()))
			{
				// This happens in statement lists
				statements->append(new OOModel::DeclarationStatement(ooFunction));
			}
			else
				log_->writeError(className_, functionDecl, CppImportLogger::Reason::INSERT_PROBLEM);
		}
		if (!ooFunction->items()->size())
			// only visit the body if we have not yet visited it
			// handle body, typeargs and storage specifier
			TraverseFunction(functionDecl, ooFunction);
	}
	else
		log_->writeError(className_, functionDecl, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::TraverseFunctionTemplateDecl(clang::FunctionTemplateDecl* functionDecl)
{
	// this node does not provide any special information
	// therefore it is sufficient to just visit the templated function
	return TraverseDecl(functionDecl->getTemplatedDecl());
}

bool ClangAstVisitor::TraverseVarDecl(clang::VarDecl* varDecl)
{
	if (!shouldImport(varDecl->getLocation()))
		return true;

	if (llvm::isa<clang::ParmVarDecl>(varDecl))
		return true;

	// check if this variable is only from a explicit/implicit template instantiation - if so we do not translate it.
	if (clang::TSK_ExplicitInstantiationDeclaration == varDecl->getTemplateSpecializationKind() ||
			clang::TSK_ExplicitInstantiationDefinition == varDecl->getTemplateSpecializationKind() ||
			clang::TSK_ImplicitInstantiation == varDecl->getTemplateSpecializationKind())
		return true;

	OOModel::VariableDeclaration* ooVarDecl = nullptr;
	OOModel::VariableDeclarationExpression* ooVarDeclExpr = nullptr;
	QString varName = QString::fromStdString(varDecl->getNameAsString());

	bool wasDeclared = false;
	if (varDecl->isStaticDataMember())
	{
		if (!(ooVarDecl = trMngr_->insertStaticField(varDecl, wasDeclared)))
		{
			log_->writeError(className_, varDecl, CppImportLogger::Reason::NO_PARENT);
			return true;
		}
	}
	else
	{
		if (!inBody_)
		{
			ooVarDecl = new OOModel::VariableDeclaration(varName);
			ooVarDeclExpr = new OOModel::VariableDeclarationExpression(ooVarDecl);
			ooExprStack_.push(ooVarDeclExpr);
		}
		else if (auto project = DCast<OOModel::Project>(ooStack_.top()))
		{
			ooVarDecl = new OOModel::Field(varName);
			project->fields()->append(ooVarDecl);
		}
		else if (auto module = DCast<OOModel::Module>(ooStack_.top()))
		{
			ooVarDecl = new OOModel::Field(varName);
			module->fields()->append(ooVarDecl);
		}
		else if (auto ooClass = DCast<OOModel::Class>(ooStack_.top()))
		{
			ooVarDecl = new OOModel::Field(varName);
			ooClass->fields()->append(ooVarDecl);
		}
		else if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
		{
			ooVarDecl = new OOModel::VariableDeclaration(varName);
			// TODO: remove variabledeclaration expression as soon we have a DeclarationStatement
			itemList->append(new OOModel::ExpressionStatement(new OOModel::VariableDeclarationExpression(ooVarDecl)));
		}
		else
		{
			log_->writeWarning(className_, varDecl, CppImportLogger::Reason::INSERT_PROBLEM);
			return true;
		}
	}

	if (varDecl->hasInit())
	{
		bool inBody = inBody_;
		inBody_ = false;
		TraverseStmt(varDecl->getInit()->IgnoreImplicit());
		if (!ooExprStack_.empty())
		{
			// make sure we have not ourself as init (if init couldn't be converted)
			if (ooVarDeclExpr != ooExprStack_.top())
				ooVarDecl->setInitialValue(ooExprStack_.pop());
		}
		else
			log_->writeError(className_, varDecl->getInit(), CppImportLogger::Reason::NOT_SUPPORTED);
		inBody_ = inBody;
	}

	mapAst(varDecl, ooVarDecl);
	if (wasDeclared)
		// we know the rest of the information already
		return true;

	// set the type
	ooVarDecl->setTypeExpression(utils_->translateQualifiedType(varDecl->getTypeSourceInfo()->getTypeLoc()));
	// modifiers
	ooVarDecl->modifiers()->set(utils_->translateStorageSpecifier(varDecl->getStorageClass()));
	return true;
}

bool ClangAstVisitor::TraverseFieldDecl(clang::FieldDecl* fieldDecl)
{
	if (!shouldImport(fieldDecl->getLocation()))
		return true;
	OOModel::Field* field = trMngr_->insertField(fieldDecl);
	if (!field)
	{
		log_->writeError(className_, fieldDecl, CppImportLogger::Reason::NO_PARENT);
		return true;
	}
	if (fieldDecl->hasInClassInitializer())
	{
		bool inBody = inBody_;
		inBody_ = false;
		TraverseStmt(fieldDecl->getInClassInitializer());
		if (!ooExprStack_.empty())
			field->setInitialValue(ooExprStack_.pop());
		else
			log_->writeError(className_, fieldDecl->getInClassInitializer(), CppImportLogger::Reason::NOT_SUPPORTED);
		inBody_ = inBody;
	}

	field->setTypeExpression(utils_->translateQualifiedType(fieldDecl->getTypeSourceInfo()->getTypeLoc()));
	// modifiers
	field->modifiers()->set(utils_->translateAccessSpecifier(fieldDecl->getAccess()));
	return true;
}

bool ClangAstVisitor::TraverseEnumDecl(clang::EnumDecl* enumDecl)
{
	if (!shouldImport(enumDecl->getLocation()))
		return true;

	auto ooEnumClass = new OOModel::Class(QString::fromStdString(enumDecl->getNameAsString()),
													  OOModel::Class::ConstructKind::Enum);

	// insert in tree
	if (auto curProject = DCast<OOModel::Project>(ooStack_.top()))
		curProject->classes()->append(ooEnumClass);
	else if (auto curModule = DCast<OOModel::Module>(ooStack_.top()))
		curModule->classes()->append(ooEnumClass);
	else if (auto curClass = DCast<OOModel::Class>(ooStack_.top()))
		curClass->classes()->append(ooEnumClass);
	else if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
		itemList->append(new OOModel::DeclarationStatement(ooEnumClass));
	else
	{
		log_->writeWarning(className_, enumDecl, CppImportLogger::Reason::INSERT_PROBLEM);
		// no need to further process this enum
		return true;
	}
	clang::EnumDecl::enumerator_iterator it = enumDecl->enumerator_begin();
	bool inBody = inBody_;
	inBody_ = false;
	for (;it!=enumDecl->enumerator_end();++it)
	{
		// check if there is an initializing expression if so visit it first and then add it to the enum
		if (auto e = it->getInitExpr())
		{
			TraverseStmt(e);
			ooEnumClass->enumerators()->append(new OOModel::Enumerator
														  (QString::fromStdString(it->getNameAsString()), ooExprStack_.pop()));
		}
		else
			ooEnumClass->enumerators()->append(new OOModel::Enumerator(QString::fromStdString(it->getNameAsString())));
	}
	inBody_ = inBody;
	mapAst(enumDecl, ooEnumClass);
	return true;
}

bool ClangAstVisitor::WalkUpFromTypedefNameDecl(clang::TypedefNameDecl* typedefDecl)
{
	// This method is a walkup such that it covers both subtypes (typedef and typealias)
	if (!shouldImport(typedefDecl->getLocation()))
		return true;
	if (auto ooTypeAlias = trMngr_->insertTypeAlias(typedefDecl))
	{
		ooTypeAlias->setTypeExpression(utils_->translateQualifiedType(typedefDecl->getTypeSourceInfo()->getTypeLoc()));
		ooTypeAlias->setName(QString::fromStdString(typedefDecl->getNameAsString()));
		ooTypeAlias->modifiers()->set(utils_->translateAccessSpecifier(typedefDecl->getAccess()));
		if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
			itemList->append(new OOModel::DeclarationStatement(ooTypeAlias));
		else if (auto declaration = DCast<OOModel::Declaration>(ooStack_.top()))
			declaration->subDeclarations()->append(ooTypeAlias);
		else
			log_->writeError(className_, typedefDecl, CppImportLogger::Reason::INSERT_PROBLEM);
	}
	return true;
}

bool ClangAstVisitor::TraverseTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl* typeAliasTemplate)
{
	if (!shouldImport(typeAliasTemplate->getLocation()))
		return true;
	if (auto ooTypeAlias = trMngr_->insertTypeAliasTemplate(typeAliasTemplate))
	{
		auto typeAlias = typeAliasTemplate->getTemplatedDecl();
		ooTypeAlias->setTypeExpression(utils_->translateQualifiedType(typeAlias->getTypeSourceInfo()->getTypeLoc()));
		ooTypeAlias->setName(QString::fromStdString(typeAliasTemplate->getNameAsString()));
		// type arguments
		auto templateParamList = typeAliasTemplate->getTemplateParameters();
		for (auto templateParameter : *templateParamList)
			ooTypeAlias->typeArguments()->append(templArgVisitor_->translateTemplateArg(templateParameter));
		// insert in tree
		if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
			itemList->append(new OOModel::DeclarationStatement(ooTypeAlias));
		else if (auto declaration = DCast<OOModel::Declaration>(ooStack_.top()))
			declaration->subDeclarations()->append(ooTypeAlias);
		else
			log_->writeError(className_, typeAliasTemplate, CppImportLogger::Reason::INSERT_PROBLEM);
	}
	return true;
}

bool ClangAstVisitor::TraverseNamespaceAliasDecl(clang::NamespaceAliasDecl* namespaceAlias)
{
	if (!shouldImport(namespaceAlias->getLocation()))
		return true;
	if (auto ooTypeAlias = trMngr_->insertNamespaceAlias(namespaceAlias))
	{
		ooTypeAlias->setName(QString::fromStdString(namespaceAlias->getNameAsString()));

		auto nameRef = createReference(namespaceAlias->getAliasLoc());
		if (namespaceAlias->getQualifier())
			nameRef->setPrefix(utils_->translateNestedNameSpecifier(namespaceAlias->getQualifierLoc()));
		ooTypeAlias->setTypeExpression(nameRef);
		if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
			itemList->append(new OOModel::DeclarationStatement(ooTypeAlias));
		else if (auto declaration = DCast<OOModel::Declaration>(ooStack_.top()))
			declaration->subDeclarations()->append(ooTypeAlias);
		else
		{
			deleteNode(ooTypeAlias);
			log_->writeError(className_, namespaceAlias, CppImportLogger::Reason::INSERT_PROBLEM);
		}
	}
	return true;
}

bool ClangAstVisitor::TraverseUsingDecl(clang::UsingDecl* usingDecl)
{
	if (!shouldImport(usingDecl->getLocation()))
		return true;
	if (auto ooNameImport = trMngr_->insertUsingDecl(usingDecl))
	{
		auto nameRef = createReference(usingDecl->getNameInfo().getSourceRange());
		if (auto prefix = usingDecl->getQualifierLoc())
			nameRef->setPrefix(utils_->translateNestedNameSpecifier(prefix));
		ooNameImport->setImportedName(nameRef);
		if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
			itemList->append(new OOModel::DeclarationStatement(ooNameImport));
		else if (auto declaration = DCast<OOModel::Declaration>(ooStack_.top()))
			declaration->subDeclarations()->append(ooNameImport);
		else
		{
			deleteNode(ooNameImport);
			log_->writeError(className_, usingDecl, CppImportLogger::Reason::INSERT_PROBLEM);
		}
	}
	return true;
}

bool ClangAstVisitor::TraverseUsingDirectiveDecl(clang::UsingDirectiveDecl* usingDirectiveDecl)
{
	if (!shouldImport(usingDirectiveDecl->getLocation()))
		return true;
	if (auto ooNameImport = trMngr_->insertUsingDirective(usingDirectiveDecl))
	{
		auto nameRef = createReference(usingDirectiveDecl->getIdentLocation());
		if (auto prefix = usingDirectiveDecl->getQualifierLoc())
			nameRef->setPrefix(utils_->translateNestedNameSpecifier(prefix));
		ooNameImport->setImportedName(nameRef);
		if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
			itemList->append(new OOModel::DeclarationStatement(ooNameImport));
		else if (auto declaration = DCast<OOModel::Declaration>(ooStack_.top()))
			declaration->subDeclarations()->append(ooNameImport);
		else
		{
			deleteNode(ooNameImport);
			log_->writeError(className_, usingDirectiveDecl, CppImportLogger::Reason::INSERT_PROBLEM);
		}
	}
	return true;
}

bool ClangAstVisitor::TraverseUnresolvedUsingValueDecl(clang::UnresolvedUsingValueDecl* unresolvedUsing)
{
	if (!shouldImport(unresolvedUsing->getLocation()))
		return true;
	if (auto ooNameImport = trMngr_->insertUnresolvedUsing(unresolvedUsing))
	{
		auto nameRef = createReference(unresolvedUsing->getNameInfo().getSourceRange());
		if (auto prefix = unresolvedUsing->getQualifierLoc())
			nameRef->setPrefix(utils_->translateNestedNameSpecifier(prefix));
		ooNameImport->setImportedName(nameRef);
		if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
			itemList->append(new OOModel::DeclarationStatement(ooNameImport));
		else if (auto declaration = DCast<OOModel::Declaration>(ooStack_.top()))
			declaration->subDeclarations()->append(ooNameImport);
		else
		{
			deleteNode(ooNameImport);
			log_->writeError(className_, unresolvedUsing, CppImportLogger::Reason::INSERT_PROBLEM);
		}
	}
	return true;
}

bool ClangAstVisitor::TraverseStmt(clang::Stmt* S)
{
	if (S && llvm::isa<clang::Expr>(S))
	{
		// always ignore implicit stuff
		auto expr = exprVisitor_->translateExpression(S->IgnoreImplicit());

		if (!inBody_)
			ooExprStack_.push(expr);
		else if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
			itemList->append(new OOModel::ExpressionStatement(expr));

		return true;
	}

	return Base::TraverseStmt(S);
}

bool ClangAstVisitor::VisitStmt(clang::Stmt* S)
{
	if (S && !llvm::isa<clang::CompoundStmt>(S) && !llvm::isa<clang::NullStmt>(S))
	{
		log_->writeError(className_, S, CppImportLogger::Reason::NOT_SUPPORTED);
		return true;
	}
	return Base::VisitStmt(S);
}

bool ClangAstVisitor::TraverseIfStmt(clang::IfStmt* ifStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		auto ooIfStmt = new OOModel::IfStatement();
		// append the if stmt to current stmt list
		itemList->append(ooIfStmt);
		// condition
		bool inBody = inBody_;
		inBody_ = false;
		if (auto varDecl = ifStmt->getConditionVariable())
			TraverseDecl(varDecl);
		else
			TraverseStmt(ifStmt->getCond());
		ooIfStmt->setCondition(ooExprStack_.pop());
		inBody_ = true;
		// then branch
		ooStack_.push(ooIfStmt->thenBranch());
		TraverseStmt(ifStmt->getThen());
		ooStack_.pop();
		// else branch
		ooStack_.push(ooIfStmt->elseBranch());
		TraverseStmt(ifStmt->getElse());
		inBody_ = inBody;
		ooStack_.pop();
		mapAst(ifStmt, ooIfStmt);
	}
	else
		log_->writeError(className_, ifStmt, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::TraverseWhileStmt(clang::WhileStmt* whileStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		auto ooLoop = new OOModel::LoopStatement();
		// append the loop to current stmt list
		itemList->append(ooLoop);
		// condition
		bool inBody = inBody_;
		inBody_ = false;
		if (auto varDecl = whileStmt->getConditionVariable())
			TraverseDecl(varDecl);
		else
			TraverseStmt(whileStmt->getCond());
		inBody_ = true;
		ooLoop->setCondition(ooExprStack_.pop());
		// body
		ooStack_.push(ooLoop->body());
		TraverseStmt(whileStmt->getBody());
		inBody_ = inBody;
		ooStack_.pop();
		mapAst(whileStmt, ooLoop);
	}
	else
		log_->writeError(className_, whileStmt, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::TraverseDoStmt(clang::DoStmt* doStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		auto ooLoop = new OOModel::LoopStatement(OOModel::LoopStatement::LoopKind::PostCheck);
		// append the loop to current stmt list
		itemList->append(ooLoop);
		// condition
		bool inBody = inBody_;
		inBody_ = false;
		TraverseStmt(doStmt->getCond());
		ooLoop->setCondition(ooExprStack_.pop());
		// body
		inBody_ = true;
		ooStack_.push(ooLoop->body());
		TraverseStmt(doStmt->getBody());
		inBody_ = inBody;
		ooStack_.pop();
		mapAst(doStmt, ooLoop);
	}
	else
		log_->writeError(className_, doStmt, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::TraverseForStmt(clang::ForStmt* forStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		auto ooLoop = new OOModel::LoopStatement();
		itemList->append(ooLoop);
		bool inBody = inBody_;
		inBody_ = false;
		// init
		TraverseStmt(forStmt->getInit());
		if (!ooExprStack_.empty())
		ooLoop->setInitStep(ooExprStack_.pop());
		// condition
		TraverseStmt(forStmt->getCond());
		if (!ooExprStack_.empty())
		ooLoop->setCondition(ooExprStack_.pop());
		// update
		TraverseStmt(forStmt->getInc());
		if (!ooExprStack_.empty())
		ooLoop->setUpdateStep(ooExprStack_.pop());
		inBody_ = true;
		// body
		ooStack_.push(ooLoop->body());
		TraverseStmt(forStmt->getBody());
		inBody_ = inBody;
		ooStack_.pop();
		mapAst(forStmt, ooLoop);
	}
	else
		log_->writeError(className_, forStmt, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::TraverseCXXForRangeStmt(clang::CXXForRangeStmt* forRangeStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		auto ooLoop = new OOModel::ForEachStatement();
		const clang::VarDecl* loopVar = forRangeStmt->getLoopVariable();
		itemList->append(ooLoop);
		ooLoop->setVarName(QString::fromStdString(loopVar->getNameAsString()));
		ooLoop->setVarType(utils_->translateQualifiedType(loopVar->getTypeSourceInfo()->getTypeLoc()));
		bool inBody = inBody_;
		inBody_ = false;
		TraverseStmt(forRangeStmt->getRangeInit());
		if (!ooExprStack_.empty())
			ooLoop->setCollection(ooExprStack_.pop());
		inBody_ = true;
		//body
		ooStack_.push(ooLoop->body());
		TraverseStmt(forRangeStmt->getBody());
		ooStack_.pop();
		inBody_ = inBody;
		mapAst(forRangeStmt, ooLoop);
	}
	else
		log_->writeError(className_, forRangeStmt, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::TraverseReturnStmt(clang::ReturnStmt* returnStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		auto ooReturn = new OOModel::ReturnStatement();
		itemList->append(ooReturn);
		// return expression
		bool inBody = inBody_;
		inBody_ = false;
		TraverseStmt(returnStmt->getRetValue());
		if (!ooExprStack_.empty())
			ooReturn->values()->append(ooExprStack_.pop());
		else
			log_->writeError(className_, returnStmt->getRetValue(), CppImportLogger::Reason::NOT_SUPPORTED);
		inBody_ = inBody;
		mapAst(returnStmt, ooReturn);
	}
	else
		log_->writeError(className_, returnStmt, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::TraverseDeclStmt(clang::DeclStmt* declStmt)
{
	if (inBody_)
	{
		for (auto declIt = declStmt->decl_begin(); declIt != declStmt->decl_end(); ++declIt)
			TraverseDecl(*declIt);
		return true;
	}
	if (!declStmt->isSingleDecl())
	{
		auto ooComma = new OOModel::CommaExpression();
		bool inBody = inBody_;
		inBody_ = false;
		QList<OOModel::Expression*> exprList;
		for (auto declIt = declStmt->decl_begin(); declIt != declStmt->decl_end(); declIt++)
		{
			TraverseDecl(*declIt);
			if (!ooExprStack_.empty())
				exprList.append(ooExprStack_.pop());
			else
				log_->writeError(className_, *declIt, CppImportLogger::Reason::NOT_SUPPORTED);
		}
		int size = exprList.size();
		auto currentCommaExpr = ooComma;
		for (int i = 0; i < size; i++)
		{
			if ((i+2) < size)
			{
				currentCommaExpr->setLeft(exprList.at(i));
				auto next = new OOModel::CommaExpression();
				currentCommaExpr->setRight(next);
				currentCommaExpr = next;
			}
			else if ((i+1) < size)
				currentCommaExpr->setLeft(exprList.at(i));
			else
				currentCommaExpr->setRight(exprList.at(i));
		}

		if (!(inBody_ = inBody))
			ooExprStack_.push(ooComma);
		else
			log_->writeError(className_, declStmt, CppImportLogger::Reason::INSERT_PROBLEM);
		return true;
	}
	return TraverseDecl(declStmt->getSingleDecl());
}

bool ClangAstVisitor::TraverseCXXTryStmt(clang::CXXTryStmt* tryStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		auto ooTry = new OOModel::TryCatchFinallyStatement();
		itemList->append(ooTry);
		bool inBody = inBody_;
		inBody_ = true;
		// visit the body
		ooStack_.push(ooTry->tryBody());
		TraverseStmt(tryStmt->getTryBlock());
		ooStack_.pop();
		// visit catch blocks
		unsigned end = tryStmt->getNumHandlers();
		for (unsigned i = 0; i < end; i++)
		{
			TraverseStmt(tryStmt->getHandler(i));
			ooTry->catchClauses()->append(ooStack_.pop());
		}
		inBody_ = inBody;
		mapAst(tryStmt, ooTry);
	}
	else
		log_->writeError(className_, tryStmt, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::TraverseCXXCatchStmt(clang::CXXCatchStmt* catchStmt)
{
	auto ooCatch = new OOModel::CatchClause();
	// save inBody var
	bool inBody = inBody_;
	inBody_ = false;
	// visit exception to catch
	if (catchStmt->getExceptionDecl())
	{
		TraverseDecl(catchStmt->getExceptionDecl());
		if (!ooExprStack_.empty())
			ooCatch->setExceptionToCatch(ooExprStack_.pop());
	}
	// visit catch body
	inBody_ = true;
	ooStack_.push(ooCatch->body());
	TraverseStmt(catchStmt->getHandlerBlock());
	ooStack_.pop();
	// finish up
	inBody_ = inBody;
	ooStack_.push(ooCatch);
	mapAst(catchStmt, ooCatch);
	return true;
}

bool ClangAstVisitor::TraverseSwitchStmt(clang::SwitchStmt* switchStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		auto ooSwitchStmt = new OOModel::SwitchStatement();
		itemList->append(ooSwitchStmt);
		// save inbody var
		bool inBody = inBody_;
		inBody_ = false;
		// traverse condition
		if (auto varDecl = switchStmt->getConditionVariable())
			TraverseDecl(varDecl);
		else
			TraverseStmt(switchStmt->getCond());
		if (!ooExprStack_.empty())
			ooSwitchStmt->setSwitchExpression(ooExprStack_.pop());
		// body
		inBody_ = true;
		if (auto body = llvm::dyn_cast<clang::CompoundStmt>(switchStmt->getBody()))
		{
			ooStack_.push(ooSwitchStmt->body());
			// Visit everything before the first case/default statement
			auto bodyIt = body->body_begin();
			while (bodyIt != body->body_end() && !llvm::isa<clang::CaseStmt>(*bodyIt) &&
					!llvm::isa<clang::DefaultStmt>(*bodyIt))
				TraverseStmt(*bodyIt++);

			// push a dummy itemlist such that at every case/default statement we can first pop the stack
			auto itemList = new OOModel::StatementItemList();
			ooStack_.push(itemList);
			// visit the rest
			while (bodyIt != body->body_end())
				TraverseStmt(*bodyIt++);

			// pops the body from the last case statement
			ooStack_.pop();
			// delete the dummy list
			deleteNode(itemList);
			// pop the body of the switch statement
			ooStack_.pop();
			mapAst(switchStmt, ooSwitchStmt);
		}
		else
			log_->writeError(className_, switchStmt, CppImportLogger::Reason::NOT_SUPPORTED);
		// restore inbody var
		inBody_ = inBody;
	}
	else
		log_->writeError(className_, switchStmt, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::TraverseCaseStmt(clang::CaseStmt* caseStmt)
{
	// pop the body of the previous case
	ooStack_.pop();
	auto ooSwitchCase = new OOModel::CaseStatement();
	// insert in tree
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
		itemList->append(ooSwitchCase);
	else
	{
		log_->writeError(className_, caseStmt, CppImportLogger::Reason::INSERT_PROBLEM);
		return true;
	}
	// traverse condition
	inBody_ = false;
	TraverseStmt(caseStmt->getLHS());
	if (!ooExprStack_.empty())
		ooSwitchCase->setCaseExpression(ooExprStack_.pop());
	inBody_ = true;
	// traverse statements/body
	ooStack_.push(ooSwitchCase->body());
	TraverseStmt(caseStmt->getSubStmt());
	mapAst(caseStmt, ooSwitchCase);
	return true;
}

bool ClangAstVisitor::TraverseCompoundStmt(clang::CompoundStmt* compoundStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		/*
		 * in case this compound statement gets translated to a statement item list we are manually
		 * visiting children in order to be able to insert empty lines and comments.
		 *
		 * comments processing 1 of 3.
		 * process comments which are on separate lines.
		 * we might process some comments which are later reassociated with declarations.
		 */

		// calculate the presumed locations for the beginning and end of this compound statement.
		auto presumedLocationStart = clang_.sourceManager()->getPresumedLoc(compoundStmt->getLocStart());
		auto presumedLocationEnd = clang_.sourceManager()->getPresumedLoc(compoundStmt->getLocEnd());

		// assert clang behaves as expected.
		Q_ASSERT(presumedLocationStart.getFilename() == presumedLocationEnd.getFilename());
		Q_ASSERT(presumedLocationStart.getLine() <= presumedLocationEnd.getLine());

		/*
		 * to increase performance we precompute a subset of the comments of this translation unit containing
		 * only the comments which are in range of this compound statement.
		 */
		QList<Comment*> listComments;
		for (auto comment : comments_)
				if (presumedLocationStart.getFilename() == comment->fileName() &&
					 presumedLocationStart.getLine() <= comment->lineStart() &&
					 comment->lineEnd() <= presumedLocationEnd.getLine())
					listComments.append(comment);

		/*
		 * keep track of the line the last child has ended on.
		 * initially this location is the beginning of the compound statement itself.
		 */
		auto lastChildEndLine = clang_.sourceManager()->getPresumedLineNumber(compoundStmt->getLocStart());
		bool firstLine = true;

		// traverse children
		for (auto child : compoundStmt->children())
		{
			// calculate the line on which the current child starts
			auto currentChildStartLine = clang_.sourceManager()->getPresumedLineNumber(child->getSourceRange().getBegin());

			// check that we are in a valid case where the end of the last child comes before the start of the current one.
			if (lastChildEndLine < currentChildStartLine)
				// "visit" each line between the two children
				for (auto currentLine = lastChildEndLine; currentLine < currentChildStartLine; currentLine++)
				{
					// try to find a comment that was not yet attached to any node located on the current line
					Comment* commentOnLine = nullptr;
					bool emptyLine = true;
					for (auto comment : listComments)
						if (!comment->node() && comment->lineStart() == currentLine)
						{
							commentOnLine = comment;
							emptyLine = false;
							break;
						}
						else if (comment->lineStart() < currentLine && currentLine <= comment->lineEnd())
							emptyLine = false;

					if (emptyLine && !firstLine)
						// if no comment was found that means that the line is empty
						itemList->append(new OOModel::ExpressionStatement());
					else if (commentOnLine)
					{
						// insert the found comment at the current line
						commentOnLine->insertIntoItemList(itemList);

						// in case the comment takes up multiple lines we have to skip over the additional lines as well
						currentLine = commentOnLine->lineEnd();
					}

					firstLine = false;
				}

			firstLine = false;
			TraverseStmt(child);

			// update the location on which the last child ended
			lastChildEndLine = clang_.sourceManager()->getPresumedLineNumber(child->getLocEnd()) + 1;
		}

		return true;
	}

	return Base::TraverseCompoundStmt(compoundStmt);
}

bool ClangAstVisitor::TraverseDefaultStmt(clang::DefaultStmt* defaultStmt)
{
	// pop the body of the previous case
	ooStack_.pop();
	auto ooDefaultCase = new OOModel::CaseStatement();
	// insert in tree
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
		itemList->append(ooDefaultCase);
	else
	{
		log_->writeError(className_, defaultStmt, CppImportLogger::Reason::INSERT_PROBLEM);
		return true;
	}
	// traverse statements/body
	ooStack_.push(ooDefaultCase->body());
	TraverseStmt(defaultStmt->getSubStmt());
	mapAst(defaultStmt, ooDefaultCase);
	return true;
}

bool ClangAstVisitor::TraverseBreakStmt(clang::BreakStmt* breakStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		auto ooBreakStmt = new OOModel::BreakStatement();
		mapAst(breakStmt, ooBreakStmt);
		itemList->append(ooBreakStmt);
	}
	else
		log_->writeError(className_, breakStmt, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::TraverseContinueStmt(clang::ContinueStmt* continueStmt)
{
	if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
	{
		auto ooContinueStmt = new OOModel::ContinueStatement();

		mapAst(continueStmt, ooContinueStmt);

		itemList->append(ooContinueStmt);
	}
	else
		log_->writeError(className_, continueStmt, CppImportLogger::Reason::INSERT_PROBLEM);
	return true;
}

bool ClangAstVisitor::shouldUseDataRecursionfor (clang::Stmt*)
{
	return false;
}

bool ClangAstVisitor::TraverseMethodDecl(clang::CXXMethodDecl* methodDecl, OOModel::Method::MethodKind kind)
{
	OOModel::Method* ooMethod = trMngr_->insertMethodDecl(methodDecl, kind);
	if (!ooMethod)
	{
		// TODO: at the moment we only consider a method where the parent has been visited.
		if (trMngr_->containsClass(methodDecl->getParent()))
			log_->writeError(className_, methodDecl, CppImportLogger::Reason::NO_PARENT);
		return true;
	}
	if (!ooMethod->items()->size())
	{
		// we only translate the following if the method is not yet defined (therefore the body is empty)
		// note that the following code might get executed twice once for the declaration and once for the definition.

		// translate modifiers
		ooMethod->modifiers()->set(utils_->translateAccessSpecifier(methodDecl->getAccess()));

		// handle body, typeargs and storage specifier
		TraverseFunction(methodDecl, ooMethod);

		// member initializers
		if (auto constructor = llvm::dyn_cast<clang::CXXConstructorDecl>(methodDecl))
		{
			if (constructor->getNumCtorInitializers() && !ooMethod->memberInitializers()->size())
			{
				// if the method already has member initializer we do not have to consider them anymore
				for (auto initIt = constructor->init_begin(); initIt != constructor->init_end(); ++initIt)
				{
					if (!(*initIt)->isWritten())
						continue;
					if (auto ooMemberInit = utils_->translateMemberInit((*initIt)))
						ooMethod->memberInitializers()->append(ooMemberInit);
				}
			}
		}
	}

	mapAst(methodDecl, ooMethod);

	return true;
}

void ClangAstVisitor::TraverseClass(clang::CXXRecordDecl* recordDecl, OOModel::Class* ooClass)
{
	Q_ASSERT(ooClass);
	// insert in tree
	if (auto curProject = DCast<OOModel::Project>(ooStack_.top()))
		curProject->classes()->append(ooClass);
	else if (auto curModule = DCast<OOModel::Module>(ooStack_.top()))
		curModule->classes()->append(ooClass);
	else if (auto curClass = DCast<OOModel::Class>(ooStack_.top()))
		curClass->classes()->append(ooClass);
	else if (auto itemList = DCast<OOModel::StatementItemList>(ooStack_.top()))
		itemList->append(new OOModel::DeclarationStatement(ooClass));
	else
		log_->writeError(className_, recordDecl, CppImportLogger::Reason::INSERT_PROBLEM);

	// visit child decls
	if (recordDecl->isThisDeclarationADefinition())
	{
		ooStack_.push(ooClass);
		for (auto declIt = recordDecl->decls_begin(); declIt!=recordDecl->decls_end(); ++declIt)
		{
			if (auto fDecl = llvm::dyn_cast<clang::FriendDecl>(*declIt))
			{
				// Class type
				if (auto type = fDecl->getFriendType())
					insertFriendClass(type, ooClass);
				// Functions
				if (auto friendDecl = fDecl->getFriendDecl())
				{
					if (!llvm::isa<clang::FunctionDecl>(friendDecl))
						log_->writeError(className_, friendDecl, CppImportLogger::Reason::NOT_SUPPORTED);
					else
						insertFriendFunction(llvm::dyn_cast<clang::FunctionDecl>(friendDecl), ooClass);
				}
			}
			else
				TraverseDecl(*declIt);
		}
		ooStack_.pop();

		// visit base classes
		for (auto base_itr = recordDecl->bases_begin(); base_itr!=recordDecl->bases_end(); ++base_itr)
			ooClass->baseClasses()->append(utils_->translateQualifiedType(base_itr->getTypeSourceInfo()->getTypeLoc()));
	}

	// set modifiers
	ooClass->modifiers()->set(utils_->translateAccessSpecifier(recordDecl->getAccess()));
}

void ClangAstVisitor::TraverseFunction(clang::FunctionDecl* functionDecl, OOModel::Method* ooFunction)
{
	Q_ASSERT(ooFunction);
	// only visit the body if we are at the definition
	if (functionDecl->isThisDeclarationADefinition())
	{
		if (ooFunction->items()->size())
			/* TODO: this is a double defined function this comes from functions defined in the header.
			* We might need to give this some attention as soon as we support macros
			* (could be that we include the header with different defines) but for now we just ignore it. */
			return;
		ooStack_.push(ooFunction->items());
		bool inBody = inBody_;
		inBody_ = true;
		if (auto body = functionDecl->getBody())
			TraverseStmt(body);
		inBody_ = inBody;
		ooStack_.pop();
	}
	// visit type arguments if any & if not yet visited
	if (!ooFunction->typeArguments()->size())
	{
		if (auto functionTemplate = functionDecl->getDescribedFunctionTemplate())
		{
			auto templateParamList = functionTemplate->getTemplateParameters();
			for (auto templateParameter : *templateParamList)
				ooFunction->typeArguments()->append(templArgVisitor_->translateTemplateArg(templateParameter));
		}
		if (auto specArgs = functionDecl->getTemplateSpecializationArgsAsWritten())
		{
			unsigned templateArgs = specArgs->NumTemplateArgs;
			auto astTemplateArgsList = specArgs->getTemplateArgs();
			auto templateParamList = functionDecl->getPrimaryTemplate()->getTemplateParameters();
			for (unsigned i = 0; i < templateArgs; i++)
			{
				auto typeArg = new OOModel::FormalTypeArgument();
				typeArg->setName(QString::fromStdString(templateParamList->getParam(i)->getNameAsString()));
				typeArg->setSpecializationExpression(utils_->translateTemplateArgument(astTemplateArgsList[i]));
				ooFunction->typeArguments()->append(typeArg);
			}
		}
	}
	// modifiers
	ooFunction->modifiers()->set(utils_->translateStorageSpecifier(functionDecl->getStorageClass()));
	if (functionDecl->isInlineSpecified())
		ooFunction->modifiers()->set(OOModel::Modifier::Inline);
	if (functionDecl->isVirtualAsWritten())
		ooFunction->modifiers()->set(OOModel::Modifier::Virtual);
	if (functionDecl->hasAttr<clang::OverrideAttr>())
		ooFunction->modifiers()->set(OOModel::Modifier::Override);
}

void ClangAstVisitor::insertFriendClass(clang::TypeSourceInfo* typeInfo, OOModel::Class* ooClass)
{
	if (clang::CXXRecordDecl* recordDecl = typeInfo->getType().getTypePtr()->getAsCXXRecordDecl())
		ooClass->friends()->append(new OOModel::ReferenceExpression
											(QString::fromStdString(recordDecl->getNameAsString())));
}

void ClangAstVisitor::insertFriendFunction(clang::FunctionDecl* friendFunction, OOModel::Class* ooClass)
{
	if (friendFunction->isThisDeclarationADefinition())
	{
		// TODO: this is at the moment the only solution to handle this
		ooStack_.push(ooClass);
		TraverseDecl(friendFunction);
		ooStack_.pop();
	}
	// this should happen anyway that it is clearly visible that there is a friend
	// TODO: this is not really a method call but rather a reference
	auto ooMCall = new OOModel::MethodCallExpression(QString::fromStdString(friendFunction->getNameAsString()));
	// TODO: handle return type & arguments & type arguments
	ooClass->friends()->append(ooMCall);
}

bool ClangAstVisitor::shouldImport(const clang::SourceLocation& location)
{
	QString fileName;
	if (auto file = clang_.sourceManager()->getPresumedLoc(location).getFilename())
		fileName = QString(file);
	if (clang_.sourceManager()->isInSystemHeader(location) || fileName.isEmpty() || fileName.toLower().contains("qt"))
		return importSysHeader_;
	return true;
}

void ClangAstVisitor::mapAst(clang::SourceRange clangAstNode, Model::Node* envisionAstNode)
{
	envisionToClangMap_.mapAst(clangAstNode, envisionAstNode);
}

void ClangAstVisitor::mapAst(clang::Stmt*, Model::Node*)
{
}

void ClangAstVisitor::mapAst(clang::Decl* clangAstNode, Model::Node* envisionAstNode)
{
	/*
	 * comments processing 2 of 3.
	 * process comments which are associated with declarations.
	 */
	if (auto compositeNode = DCast<Model::CompositeNode>(envisionAstNode))
		if (auto commentForDeclaration = clangAstNode->getASTContext().getRawCommentForDeclNoCache(clangAstNode))
			for (auto comment : comments_)
				if (comment->rawComment() == commentForDeclaration)
				{
					// uncomment the following line to not reassociate comments.
					// if (comment->node()) break;

					// we found a comment for this declaration

					/*
					 * if the comment is already associated with a node and it is not the current node then
					 * assert that it was added to a statement item list and remove it from said list so we can associate
					 * it with this declaration (that is in general more precise).
					 */
					if (comment->node() && comment->node() != envisionAstNode)
						comment->removeFromItemList();

					// at this point the comment is not associated with a node or it is associated with the current node.
					Q_ASSERT(!comment->node() || comment->node() == envisionAstNode);

					if (!comment->node())
					{
						// if it was not yet associated with any node then associate it with the current node.
						comment->setNode(envisionAstNode);
						compositeNode->setComment(new Comments::CommentNode(comment->text()));
					}
					break;
				}

	envisionToClangMap_.mapAst(clangAstNode, envisionAstNode);
}

void ClangAstVisitor::beforeTranslationUnit(clang::ASTContext& astContext)
{
	auto comments = astContext.getRawCommentList().getComments();
	for (auto it = comments.begin(); it != comments.end(); it++)
		comments_.append(new Comment(*it, *clang_.sourceManager()));
}

void ClangAstVisitor::endTranslationUnit()
{
	macroImporter_.endTranslationUnit();

	/*
	 * comments processing 3 of 3.
	 * process comments which are on the same line as statements.
	 */
	for (auto it = envisionToClangMap_.begin(); it != envisionToClangMap_.end(); it++)
	{
		auto nodePresumedLocation = clang_.sourceManager()->getPresumedLoc(it.value().getBegin());

		for (Comment* comment : comments_)
		{
			if (comment->node() ||
				 nodePresumedLocation.getFilename() != comment->fileName() ||
				 nodePresumedLocation.getLine() != comment->lineStart()) continue;

			// calculate the parent of the current node which is a direct child of a statement item list.
			auto lastNodeBeforeList = it.key();
			auto parent = lastNodeBeforeList->parent();
			while (parent)
			{
				if (auto itemList = DCast<OOModel::StatementItemList>(parent))
				{
					// add the comment to the parent of the current node which is a direct child of a statement item list.
					comment->insertIntoItemList(itemList, itemList->indexOf(lastNodeBeforeList));
					break;
				}

				lastNodeBeforeList = parent;
				parent = lastNodeBeforeList->parent();
			}
		}
	}

	envisionToClangMap_.clear();
}

void ClangAstVisitor::endEntireImport()
{
	macroImporter_.endEntireImport();
}

void ClangAstVisitor::deleteNode(Model::Node* node)
{
	QList<Model::Node*> workList{node};
	while (!workList.empty())
	{
		auto current = workList.takeLast();
		workList << current->children();
		envisionToClangMap_.remove(current);
	}

	SAFE_DELETE(node);
}

OOModel::ReferenceExpression* ClangAstVisitor::createReference(clang::SourceRange range)
{
	auto ref = new OOModel::ReferenceExpression(clang_.unexpandedSpelling(range.getBegin()));
	envisionToClangMap_.mapAst(range, ref);
	return ref;
}

} // namespace cppimport
