/***********************************************************************************************************************
 **
 ** Copyright (c) 2011, 2016 ETH Zurich
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

#pragma once

#include "../versioncontrolui_api.h"

#include "VisualizationBase/src/items/ItemWithNode.h"
#include "VisualizationBase/src/declarative/DeclarativeItem.h"
#include "VisualizationBase/src/declarative/DeclarativeItemBaseStyle.h"
#include "VDiffFrameStyle.h"

#include "VisualizationBase/src/items/Item.h"
#include "VisualizationBase/src/items/VText.h"
#include "VisualizationBase/src/items/Text.h"
#include "VisualizationBase/src/items/Static.h"

#include "../nodes/DiffFrame.h"

#include "ObjectPathCrumb.h"

namespace VersionControlUI
{

class DiffFrame;

class VERSIONCONTROLUI_API VDiffFrame : public Super<Visualization::ItemWithNode<VDiffFrame,
		Visualization::DeclarativeItem<VDiffFrame>, DiffFrame>>
{
	ITEM_COMMON(VDiffFrame)

	public:
		VDiffFrame(Visualization::Item* parent, NodeType* node, const StyleType* style = itemStyles().get());
		static void initializeForms();
		virtual bool isSensitiveToScale() const override;
		virtual void determineChildren() override;
		virtual int determineForm() override;

		qreal scaleFactor();

		QList<Visualization::Item*> objectPathCrumbsOldNode();
		QList<Visualization::Item*> objectPathCrumbsNewNode();

	private:
		Visualization::Item* oldVersionNode_{};
		Visualization::Item* newVersionNode_{};

		Visualization::Static* nodeNotFoundIcon_{};
		Visualization::Static* dummyIcon_{};

		QList<Visualization::Item*> objectPathCrumbsOldNode_{};
		QList<Visualization::Item*> objectPathCrumbsNewNode_{};

		void scaleVisualizations();

		void createObjectPathCrumbsVisualizationList();
};

inline QList<Visualization::Item*> VDiffFrame::objectPathCrumbsOldNode() {return objectPathCrumbsOldNode_;}
inline QList<Visualization::Item*> VDiffFrame::objectPathCrumbsNewNode() {return objectPathCrumbsNewNode_;}


}
