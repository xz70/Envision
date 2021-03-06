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

#include "ClangAstConsumer.h"

#include "CppImportManager.h"
#include "CppImportPlugin.h"
#include "Logger/src/Log.h"

namespace CppImport {

ClangAstConsumer::ClangAstConsumer(ClangAstVisitor* visitor)
	: clang::ASTConsumer{}, astVisitor_{visitor}
{}

void ClangAstConsumer::HandleTranslationUnit(clang::ASTContext& astContext)
{
	// show import progress
	CppImportManager::processedTranslationUnits()++;

	int percent = 100 * CppImportManager::processedTranslationUnits()
											/ CppImportManager::totalTranslationUnits();
	QString fileName = QString::fromStdString(astContext.getSourceManager().getFileEntryForID(
				astContext.getSourceManager().getMainFileID())->getName().str());

	CppImportPlugin::log().info("[ " + QString::number(percent) +" %] importing: " + fileName);

	astVisitor_->beforeTranslationUnit(astContext);
	astVisitor_->TraverseDecl(astContext.getTranslationUnitDecl());
	astVisitor_->endTranslationUnit();
}

void ClangAstConsumer::setCompilerInstance(const clang::CompilerInstance* compilerInstance)
{
	Q_ASSERT(compilerInstance);
	clang::SourceManager* mngr = &compilerInstance->getSourceManager();
	Q_ASSERT(mngr);
	astVisitor_->setSourceManagerAndPreprocessor(mngr, &compilerInstance->getPreprocessor());
}

}
