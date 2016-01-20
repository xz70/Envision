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

#include "UnaryOperation.h"
#include "../types/PointerType.h"
#include "../types/ErrorType.h"

#include "ModelBase/src/nodes/TypedListDefinition.h"
DEFINE_TYPED_LIST(OOModel::UnaryOperation)

namespace OOModel {

COMPOSITENODE_DEFINE_EMPTY_CONSTRUCTORS(UnaryOperation)
COMPOSITENODE_DEFINE_TYPE_REGISTRATION_METHODS(UnaryOperation)

REGISTER_ATTRIBUTE(UnaryOperation, operand, Expression, false, false, true)
REGISTER_ATTRIBUTE(UnaryOperation, opr, Integer, false, false, true)


UnaryOperation::UnaryOperation(const OperatorTypes& op, Expression* operand)
: Super(nullptr, UnaryOperation::getMetaData())
{
	setOp(op);
	if (operand) setOperand(operand);
}

Type* UnaryOperation::type()
{
	if (opr() == DEREFERENCE)
	{
		auto t = operand()->type();
		if (PointerType* pt = dynamic_cast<PointerType*>(t))
		{
			auto bt = pt->baseType()->clone();
			SAFE_DELETE(t);
			return bt;
		} else
		{
			SAFE_DELETE(t);
			return new ErrorType{"dereferencing a value that is not a pointer"};
		}
	}
	else if (opr() == ADDRESSOF)
		return new PointerType{operand()->type(), false};
	else
		return operand()->type();
}

}
