/***********************************************************************************************************************
 **
 ** Copyright (c) 2011, 2013 ETH Zurich
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

#include "comments.h"
#include "nodes/Comment.h"
#include "nodes/CommentDiagram.h"
#include "SelfTest/src/SelfTestSuite.h"

#include "VisualizationBase/src/VisualizationManager.h"
#include "VisualizationBase/src/Scene.h"
#include "VisualizationBase/src/items/VComposite.h"
#include "VisualizationBase/src/items/VList.h"
#include "VisualizationBase/src/items/RootItem.h"
#include "VisualizationBase/src/nodes/TestBoxNode.h"

#include "ModelBase/src/test_nodes/BinaryNode.h"
#include "ModelBase/src/nodes/Text.h"
#include "ModelBase/src/nodes/List.h"
#include "ModelBase/src/model/Model.h"


namespace Comments {

using namespace Visualization;

TEST(Comments, SimpleTest)
{
	auto list = new Model::List();
	auto model = new Model::Model(list);

	model->beginModification(list, "set");

#if 1
	auto node = new CommentNode(
		"# Header 1\n"
		"Text *in bold* that spans more\n"
		"than one line\n"
		"===\n"
		"\n"
		"## Header 2\n"
		"---\n"
		"And later on, some more text...\n"
		"And what about a nice diagram like...\n"
		"[diagram#0]\n"
		"with, of course, some text afterwards!\n"
		"[[http://www.yahoo.com]]\n"
		"And to round it up, a beautiful image...\n"
		"[image#image.png]"
		);
#else
	auto node = new CommentDiagram();

	auto shape1 = new CommentDiagramShape(0, 50, 200, 50, Rectangle);
	shape1->setLabel("First shape");
	shape1->setShapeColor("blue");
	shape1->setTextColor("red");
	node->shapes()->append(shape1);

	auto shape2 = new CommentDiagramShape(100, 150, 200,  50, Ellipse);
	shape2->setLabel("Another shape");
	shape2->setShapeColor("magenta");
	shape2->setTextColor("yellow");
	node->shapes()->append(shape2);

	auto shape3 = new CommentDiagramShape(0, 150, 100, 150, Diamond);
	shape3->setLabel("Diamond");
	node->shapes()->append(shape3);

	node->connectors()->append(new CommentDiagramConnector(0, SE, 1, N));
	node->connectors()->append(new CommentDiagramConnector(1, N,  2, N));
	node->connectors()->append(new CommentDiagramConnector(2, N,  0, SW));
#endif

	list->append(node);

	model->endModification();

	auto top = new RootItem(list);
	auto scene = VisualizationManager::instance().mainScene();
	scene->addTopLevelItem(top);
	QApplication::processEvents();

	scene->scheduleUpdate();
	scene->listenToModel(model);

	CHECK_CONDITION(scene);
}

}
