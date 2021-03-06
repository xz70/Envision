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

#include "VDiffFrame.h"

#include "VisualizationBase/src/VisualizationManager.h"
#include "VisualizationBase/src/declarative/DeclarativeItem.hpp"

#include "ModelBase/src/nodes/Text.h"

#include "VisualizationBase/src/items/VText.h"

#include "../nodes/DiffFrame.h"

namespace VersionControlUI
{

DEFINE_ITEM_COMMON(VDiffFrame, "item")

VDiffFrame::VDiffFrame(Visualization::Item* parent, NodeType* node, const StyleType* style)
	: Super{parent, node, style}
{
	createObjectPathCrumbsVisualizationList();
}

void VDiffFrame::createObjectPathCrumbsVisualizationList()
{
	for (auto objectPathCrumbData : node()->objectPathCrumbsDataOldNode())
		objectPathCrumbsOldNode_.append(new ObjectPathCrumb{objectPathCrumbData, nullptr});
	for (auto objectPathCrumbData : node()->objectPathCrumbsDataNewNode())
		objectPathCrumbsNewNode_.append(new ObjectPathCrumb{objectPathCrumbData, nullptr});
}

void VDiffFrame::initializeForms()
{
	auto oldCrumbs = (new Visualization::SequentialLayoutFormElement{})
					->setHorizontal()
					->setSpaceBetweenElements(5)
					->setListOfItems([](Visualization::Item* i) {
						auto vc = static_cast<VDiffFrame*>(i);
						return vc->objectPathCrumbsOldNode_;
					});

	auto newCrumbs = (new Visualization::SequentialLayoutFormElement{})
					->setHorizontal()
					->setSpaceBetweenElements(5)
					->setListOfItems([](Visualization::Item* i) {
						auto vc = static_cast<VDiffFrame*>(i);
						return vc->objectPathCrumbsNewNode_;
					});

	auto oldVersion = item(&I::oldVersionNode_, [](I* v) {
			return v->node()->oldVersionNode();});

	auto newVersion = item(&I::newVersionNode_, [](I* v) {
			return v->node()->newVersionNode();});

	auto noNodeFound = item<Visualization::Static>(&I::nodeNotFoundIcon_, &StyleType::nodeNotFoundIcon);

	auto dummy = item<Visualization::Static>(&I::dummyIcon_, &StyleType::dummyIcon);

	// form with both nodes and only one object path

	auto infoGrid = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(50)
			->setLeftMargin(10)
			->setRightMargin(10)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setColumnStretchFactor(0, 1)
			->put(0, 0, oldCrumbs);

	auto diffGrid = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(50)
			->setLeftMargin(10)
			->setRightMargin(10)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setColumnStretchFactor(0, 1)
			->setColumnStretchFactor(1, 1)
			->setRowStretchFactor(0, 1)
			->setColumnHorizontalAlignment(0, Visualization::LayoutStyle::Alignment::Right)
			->setColumnHorizontalAlignment(1, Visualization::LayoutStyle::Alignment::Left)
			->put(0, 0, oldVersion)
			->put(1, 0, newVersion);

	auto container = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(30)
			->setVerticalSpacing(15)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setLeftMargin(10)
			->setRightMargin(10)
			->setColumnStretchFactor(0, 1)
			->setColumnHorizontalAlignment(0, Visualization::LayoutStyle::Alignment::Right)
			->setColumnHorizontalAlignment(1, Visualization::LayoutStyle::Alignment::Left)
			->setRowStretchFactor(0, 1)
			->setRowStretchFactor(1, 1)
			->put(0, 0, infoGrid)
			->put(0, 1, diffGrid);


	addForm(container);

	// form with only new node available

	infoGrid = (new Visualization::GridLayoutFormElement{})
				->setHorizontalSpacing(50)
				->setLeftMargin(10)
				->setRightMargin(10)
				->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
				->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
				->setColumnStretchFactor(0, 1)
				->put(0, 0, newCrumbs);

	diffGrid = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(50)
			->setLeftMargin(10)
			->setRightMargin(10)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setColumnStretchFactor(0, 1)
			->setColumnStretchFactor(1, 1)
			->setRowStretchFactor(0, 1)
			->setRowVerticalAlignment(0, Visualization::LayoutStyle::Alignment::Center)
			->setColumnHorizontalAlignment(0, Visualization::LayoutStyle::Alignment::Center)
			->setColumnHorizontalAlignment(1, Visualization::LayoutStyle::Alignment::Center)
			->put(0, 0, noNodeFound)
			->put(1, 0, newVersion);

	container = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(30)
			->setVerticalSpacing(15)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setLeftMargin(10)
			->setRightMargin(10)
			->setColumnStretchFactor(0, 1)
			->setRowStretchFactor(0, 1)
			->setRowStretchFactor(1, 1)
			->setColumnHorizontalAlignment(0, Visualization::LayoutStyle::Alignment::Center)
			->put(0, 0, infoGrid)
			->put(0, 1, diffGrid);

	addForm(container);

	// form with only old node available

	infoGrid = (new Visualization::GridLayoutFormElement{})
				->setHorizontalSpacing(50)
				->setLeftMargin(10)
				->setRightMargin(10)
				->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
				->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
				->setColumnStretchFactor(0, 1)
				->put(0, 0, oldCrumbs);

	diffGrid = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(50)
			->setLeftMargin(10)
			->setRightMargin(10)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setColumnStretchFactor(0, 1)
			->setColumnStretchFactor(1, 1)
			->setRowStretchFactor(0, 1)
			->setRowVerticalAlignment(0, Visualization::LayoutStyle::Alignment::Center)
			->setColumnHorizontalAlignment(0, Visualization::LayoutStyle::Alignment::Center)
			->setColumnHorizontalAlignment(1, Visualization::LayoutStyle::Alignment::Center)
			->put(0, 0, oldVersion)
			->put(1, 0, noNodeFound);

	container = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(30)
			->setVerticalSpacing(15)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setLeftMargin(10)
			->setRightMargin(10)
			->setColumnStretchFactor(0, 1)
			->setColumnHorizontalAlignment(0, Visualization::LayoutStyle::Alignment::Center)
			->put(0, 0, infoGrid)
			->put(0, 1, diffGrid);

	addForm(container);

	// form with two object paths and two nodes

	infoGrid = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(50)
			->setLeftMargin(10)
			->setRightMargin(10)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setColumnStretchFactor(0, 1)
			->setColumnStretchFactor(1, 1)
			->put(0, 0, oldCrumbs)
			->put(1, 0, newCrumbs);

	auto infoWithComponentTypeGrid = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(50)
			->setLeftMargin(10)
			->setRightMargin(10)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setColumnStretchFactor(0, 1)
			->setRowStretchFactor(0, 1)
			->put(0, 0, infoGrid);

	diffGrid = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(50)
			->setLeftMargin(10)
			->setRightMargin(10)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setColumnStretchFactor(0, 1)
			->setColumnStretchFactor(1, 1)
			->setRowStretchFactor(0, 1)
			->setColumnHorizontalAlignment(0, Visualization::LayoutStyle::Alignment::Right)
			->setColumnHorizontalAlignment(1, Visualization::LayoutStyle::Alignment::Left)
			->put(0, 0, oldVersion)
			->put(1, 0, newVersion);

	container = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(30)
			->setVerticalSpacing(15)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setLeftMargin(10)
			->setRightMargin(10)
			->setColumnStretchFactor(0, 1)
			->setColumnHorizontalAlignment(0, Visualization::LayoutStyle::Alignment::Center)
			->put(0, 0, infoWithComponentTypeGrid)
			->put(0, 1, diffGrid);

	addForm(container);

	diffGrid = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(50)
			->setLeftMargin(10)
			->setRightMargin(10)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setColumnStretchFactor(0, 1)
			->setRowStretchFactor(0, 1)
			->setRowVerticalAlignment(0, Visualization::LayoutStyle::Alignment::Center)
			->setColumnHorizontalAlignment(0, Visualization::LayoutStyle::Alignment::Center)
			->put(0, 0, dummy);

	container = (new Visualization::GridLayoutFormElement{})
			->setHorizontalSpacing(30)
			->setVerticalSpacing(15)
			->setHorizontalAlignment(Visualization::LayoutStyle::Alignment::Center)
			->setNoBoundaryCursors([](Item*){return true;})->setNoInnerCursors([](Item*){return true;})
			->setLeftMargin(10)
			->setRightMargin(10)
			->setColumnStretchFactor(0, 1)
			->setRowStretchFactor(0, 1)
			->setColumnHorizontalAlignment(0, Visualization::LayoutStyle::Alignment::Center)
			->put(0, 0, diffGrid);

	addForm(container);

}

bool VDiffFrame::isSensitiveToScale() const
{
	return true;
}

qreal VDiffFrame::scaleFactor()
{
	qreal factor = Visualization::VisualizationManager::instance().mainScene()->mainViewScalingFactor();
	qreal scale;
	if (factor >= 1.0)
		scale = 1.0;
	else if (factor >= 0.007)
		scale = 1.0/factor;
	else
		scale = 1.0;

	return scale;
}

void VDiffFrame::scaleVisualizations()
{

	auto scale = scaleFactor();

	if (nodeNotFoundIcon_)
		nodeNotFoundIcon_->setScale(scale);

	if (dummyIcon_)
		dummyIcon_->setScale(scale);

	for (auto crumb : objectPathCrumbsNewNode_)
		crumb->setScale(scale);

	for (auto crumb : objectPathCrumbsOldNode_)
		crumb->setScale(scale);
}

void VDiffFrame::determineChildren()
{
	Super::determineChildren();
	scaleVisualizations();
}

int VDiffFrame::determineForm()
{
	static const int FORM_WITH_SINGLE_OBJECT_PATH = 0;
	static const int FORM_CONTAINING_ONLY_NEW_NODE = 1;
	static const int FORM_CONTAINING_ONLY_OLD_NODE = 2;
	static const int FORM_WITH_TWO_OBJECT_PATHS = 3;
	static const int FORM_FOR_DUMMY_PAIR = 4;

	if (node()->isDummyDiffFrame())
		return FORM_FOR_DUMMY_PAIR;

	if (node()->twoObjectPathsDefined())
		return FORM_WITH_TWO_OBJECT_PATHS;
	else
		if (!node()->oldVersionNode())
			return FORM_CONTAINING_ONLY_NEW_NODE;
		else if (!node()->newVersionNode())
			return FORM_CONTAINING_ONLY_OLD_NODE;
		else
			return FORM_WITH_SINGLE_OBJECT_PATH;
}

}
