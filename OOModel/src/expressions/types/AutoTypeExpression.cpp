/***********************************************************************************************************************
**
** Copyright (c) 2011, 2014 ETH Zurich
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
** following conditions are met:
**
**    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following
**      disclaimer.
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
***********************************************************************************************************************/

#include "AutoTypeExpression.h"

#include "../../declarations/VariableDeclaration.h"
#include "../../statements/ForEachStatement.h"
#include "../../types/ErrorType.h"
#include "../../types/PointerType.h"
#include "../../types/ReferenceType.h"
#include "PointerTypeExpression.h"
#include "ReferenceTypeExpression.h"

#include "ModelBase/src/nodes/TypedList.hpp"
template class Model::TypedList<OOModel::AutoTypeExpression>;

namespace OOModel {

DEFINE_COMPOSITE_EMPTY_CONSTRUCTORS(AutoTypeExpression)
DEFINE_COMPOSITE_TYPE_REGISTRATION_METHODS(AutoTypeExpression)

std::unique_ptr<Type> AutoTypeExpression::type()
{
	/**
	 * TODO: like this we return the wrong type for auto &&
	 * note however that const and volatile are supported as TypeQualifierExpression
	 * adds the qualifier when calling the type() method
	 **/
	auto p = parent();
	Model::Node* current = this;
	VariableDeclaration* varDecl = nullptr;
	ForEachStatement* forEachStatement = nullptr;
	while (!(varDecl = DCast<VariableDeclaration>(p)) && !(forEachStatement = DCast<ForEachStatement>(p)))
	{
		current = p;
		p = p->parent();
		Q_ASSERT(p);
	}

	Q_ASSERT(varDecl || forEachStatement);

	if (varDecl)
	{
		if (!varDecl->initialValue())
			return std::unique_ptr<Type>{new ErrorType{"No initial value in auto type"}};
		auto initType = varDecl->initialValue()->type();
		bool isInitValueType = initType->isValueType();
		if (varDecl == p)
			return initType;
		if (DCast<ReferenceTypeExpression>(current))
			return std::unique_ptr<Type>{new ReferenceType{std::move(initType), isInitValueType}};
		if (DCast<PointerTypeExpression>(current))
			return std::unique_ptr<Type>{new PointerType{std::move(initType), isInitValueType}};
	}

	if (forEachStatement)
	{
		// TODO: Add languge specific rules to infer the correct type
		// For starters we could also cheat and just take the first tempalte parameter, if any
		return std::unique_ptr<Type>{
			new ErrorType{"Infering the type of a collection element in a for each loop is not currently supported."}};
	}

	return std::unique_ptr<Type>{new ErrorType{"Could not find type of auto expression"}};
}

}
