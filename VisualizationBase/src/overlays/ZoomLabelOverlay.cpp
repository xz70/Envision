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
#include "ZoomLabelOverlay.h"
#include "shapes/Shape.h"
#include "../declarative/DeclarativeItemDef.h"
#include "VisualizationBase/src/icons/Icon.h"
#include "VisualizationBase/src/icons/IconStyle.h"
#include "VisualizationBase/src/items/StaticStyle.h"
#include "VisualizationBase/src/items/Text.h"
#include "VisualizationBase/src/items/Static.h"
#include "VisualizationBase/src/items/TextStyle.h"
#include "VisualizationBase/src/items/RootItem.h"
#include "VisualizationBase/src/overlays/OverlayAccessor.h"

namespace Visualization {

ITEM_COMMON_DEFINITIONS(ZoomLabelOverlay, "item")

QHash<Item*, ZoomLabelOverlay*>& ZoomLabelOverlay::itemToOverlay()
{
	static QHash<Item*, ZoomLabelOverlay*> map;
	return map;
}

ZoomLabelOverlay::ZoomLabelOverlay(Item* itemWithLabel, const StyleType* style) : Super{{itemWithLabel}, style}
{
	Q_ASSERT(itemWithLabel->node());
	Q_ASSERT(itemWithLabel->node()->definesSymbol());
	iconStyle_ = associatedItemIconStyle();
	itemToOverlay().insert(itemWithLabel, this);
}

void ZoomLabelOverlay::determineChildren()
{
	Super::determineChildren();
	text_->setText(associatedItemText());
}

void ZoomLabelOverlay::updateGeometry(int availableWidth, int availableHeight)
{
	Super::updateGeometry(availableWidth, availableHeight);

	setPos(associatedItem()->scenePos());

	auto maxScale = 1/mainViewScalingFactor();
	auto widthScale = associatedItem()->widthInScene() / (double) widthInLocal();
	auto heightScale = associatedItem()->heightInScene() / (double) heightInLocal();
	auto scaleToUse = maxScale;
	if (widthScale < scaleToUse) scaleToUse = widthScale;
	if (heightScale < scaleToUse) scaleToUse = heightScale;

	setScale(scaleToUse);
}

bool ZoomLabelOverlay::isSensitiveToScale() const
{
	return true;
}

int ZoomLabelOverlay::determineForm()
{
	return iconStyle_ ? 1 : 0;
}

void ZoomLabelOverlay::initializeForms()
{
	addForm(item<Text>(&I::text_, [](I* v){return v->associatedItemTextStyle();}));

	addForm(grid({{item<Static>(&I::icon_, [](I* v){ return v->iconStyle_;}),
		item<Text>(&I::text_, [](I* v){return v->associatedItemTextStyle();})}})
			  ->setVerticalAlignment(LayoutStyle::Alignment::Center));
}

const StaticStyle* ZoomLabelOverlay::associatedItemIconStyle() const
{
	const StaticStyle* iconStyle{};
	for (auto child : associatedItem()->childItems())
	{
		if ( auto childStaticStyle = dynamic_cast<const StaticStyle*>(child->style()) )
			if (dynamic_cast<const IconStyle*> (&childStaticStyle->itemStyle()))
		{
			if (!iconStyle) iconStyle = childStaticStyle;
			else return nullptr; // We found at least 2 icons => ambiguous
		}

	}

	return iconStyle; // Could be nullptr to indicate no icon;
}

const QString& ZoomLabelOverlay::associatedItemText() const
{
	return associatedItem()->node()->symbolName();
}

const TextStyle* ZoomLabelOverlay::associatedItemTextStyle() const
{
	const TextStyle* textStyle{};
	for (auto child : associatedItem()->childItems())
		if ( auto textChild = DCast<TextRenderer>(child) )
		{
			if (textChild->text() == associatedItemText())
			{
				if (!textStyle) textStyle = textChild->style();
				else
				{
					//we have found more than one text item representing the same string, ambiguous
					textStyle = nullptr;
					break;
				}
			}
		}

	return textStyle ? textStyle : Text::itemStyles().get();
}

QList<Item*> ZoomLabelOverlay::itemsThatShouldHaveZoomLabel(Scene* scene)
{
	QList<Item*> result;

	const double OVERLAY_SCALE_TRESHOLD = 0.5;
	auto scalingFactor = scene->mainViewScalingFactor();

	if (scalingFactor >= OVERLAY_SCALE_TRESHOLD)
	{
		itemToOverlay().clear();
		return result;
	}


	QList<Item*> stack = scene->topLevelItems();
	while (!stack.isEmpty())
	{
		auto item = stack.takeLast();

			if (item->widthInParent() * scalingFactor < OVERLAY_MIN_WIDTH
					|| item->heightInParent() * scalingFactor < OVERLAY_MIN_HEIGHT)
				continue;

		auto definesSymbol = !DCast<RootItem>(item) && item->node() && item->node()->definesSymbol();

		if (definesSymbol) result.append(item);
		stack.append(item->childItems());
	}

	return result;
}

void ZoomLabelOverlay::setItemPositionsAndHideOverlapped(OverlayGroup& group)
{
	static int postUpdateRevision{};
	++postUpdateRevision;

	for (auto accessor : group.overlays())
		static_cast<ZoomLabelOverlay*>(accessor->overlayItem())->postUpdate(postUpdateRevision);
}

void ZoomLabelOverlay::postUpdate(int revision)
{
	// If already updated, do nothing
	if (postUpdateRevision_ == revision) return;
	postUpdateRevision_ = revision;

	// Make sure all overlays above this one are updated
	auto item = associatedItem()->parent();
	while (item)
	{
		auto overlayIt = itemToOverlay().find(item);
		if (overlayIt != itemToOverlay().end())
		{
			overlayIt.value()->postUpdate(revision);
			break;
		}

		item = item->parent();
	}

	// Finally adjust the position of this overlay so that it does not overlap or if that's impossible, hide it.
	adjustPositionOrHide();
}

inline void ZoomLabelOverlay::reduceRect(QRect& rectToReduce, const QRect& rectToExclude)
{
	if (!rectToReduce.intersects(rectToExclude)) return;
	else
	{
		rectToReduce.setWidth(0);
		rectToReduce.setHeight(0);
	}
}


void ZoomLabelOverlay::adjustPositionOrHide()
{
	auto availableRect = associatedItem()->sceneBoundingRect().toRect();
	
	auto item = associatedItem()->parent();
	while (item)
	{
		auto overlayIt = itemToOverlay().find(item);
		if (overlayIt != itemToOverlay().end() && overlayIt.value()->isVisible())
			reduceRect(availableRect, overlayIt.value()->sceneBoundingRect().toRect());

		item = item->parent();
	}

	// At this point we know in what amount of space we're supposed to fit.

	// If the space is too small don't bother trying to compute a scale
	bool visible = true;
	auto scalingFactor = scene()->mainViewScalingFactor();
	if (availableRect.width() * scalingFactor < OVERLAY_MIN_WIDTH
			|| availableRect.height() * scalingFactor < OVERLAY_MIN_HEIGHT) visible = false;

	// If there might be enough space, then try to fit in
	if (visible)
	{
		setPos(availableRect.topLeft());

		auto maxScale = 1/scalingFactor;
		auto widthScale = availableRect.width() / (double) widthInLocal();
		auto heightScale = availableRect.height() / (double) heightInLocal();
		auto scaleToUse = maxScale;
		if (widthScale < scaleToUse) scaleToUse = widthScale;
		if (heightScale < scaleToUse) scaleToUse = heightScale;

		setScale(scaleToUse);
	}

	if (visible != isVisible()) setVisible(visible);
}

} /* namespace Visualization */
