/***********************************************************************************************************************
**
** Copyright (c) 2011, 2015 ETH Zurich
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
** following conditions are met:
**
** * Redistributions of source code must retain the above copyright notice, this list of conditions and the following
** disclaimer.
** * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
** following disclaimer in the documentation and/or other materials provided with the distribution.
** * Neither the name of the ETH Zurich nor the names of its contributors may be used to endorse or promote products
** derived from this software without specific prior written permission.
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

#include "ConflictUnitDetector.h"
#include "ConflictPairs.h"
#include "ChangeDescription.h"

namespace FilePersistence {

ConflictUnitDetector::ConflictUnitDetector(QSet<QString>& conflictTypes, QString revisionIdA,
														 QString revisionIdB, QString revisionIdBase) :
	conflictTypes_{conflictTypes}, revisionIdA_{revisionIdA}, revisionIdB_{revisionIdB}, revisionIdBase_{revisionIdBase} {}

ConflictUnitDetector::~ConflictUnitDetector() {}

LinkedChangesTransition ConflictUnitDetector::run(ChangeDependencyGraph& cdgA,
									ChangeDependencyGraph& cdgB,
									QSet<std::shared_ptr<const ChangeDescription>>& conflictingChanges,
									ConflictPairs& conflictPairs, LinkedChangesSet& linkedChangesSet)
{
	affectedCUsA_ = computeAffectedCUs(cdgA);
	affectedCUsB_ = computeAffectedCUs(cdgB);
	LinkedChangesTransition transition(linkedChangesSet, cdgA, cdgB);
	// In all conflict units...
	for (auto conflictRootId : affectedCUsA_.keys())
	{
		// ...that are modified by both branches...
		if (affectedCUsB_.keys().contains(conflictRootId))
		{
			// ...we take every change from A...
			for (auto changeA : affectedCUsA_.values(conflictRootId))
			{
				// ...mark it as conflicting...
				conflictingChanges.insert(changeA);
				// ...and related to the other changes in this CU
				transition.insert(changeA->id(), true, changeA);
				// ...and take every change from B...
				for (auto changeB : affectedCUsB_.values(conflictRootId))
				{
					// ...mark it conflicting and record the conflict pair.
					conflictingChanges.insert(changeB);
					conflictPairs.insert(changeA, changeB);
					// also record it as being related
					transition.insert(changeB->id(), false, changeB);
				}
			}
		}
		else
		{
			// CU is not in conflict, just record change links.
			for (auto changeA : affectedCUsA_.values(conflictRootId))
			{
				transition.insert(changeA->id(), true, changeA);
			}
		}
	}
	// also mark changes of same CU as related in B if the CU is not in conflict
	for (auto conflictRootId : affectedCUsB_.keys())
	{
		if (!affectedCUsA_.keys().contains(conflictRootId))
		{
			for (auto changeB : affectedCUsB_.values(conflictRootId))
			{
				transition.insert(changeB->id(), false, changeB);
			}
		}
	}
	return transition;
}

/**
 * Looks for a change in cdg ot get a node to get a PersistentUnit and returns it.
 * FIXME This is really ugly and I hate to do it like this. I hope we find a good solution for this.
 */
GenericPersistentUnit* getPersUnit(ChangeDependencyGraph& cdg)
{
	for (auto change : cdg.changes().values())
	{
		if (change->nodeA()) return change->nodeA()->persistentUnit();
		else if (change->nodeB()) return change->nodeB()->persistentUnit();
	}
	Q_ASSERT(false);
}

IdToChangeMultiHash ConflictUnitDetector::computeAffectedCUs(ChangeDependencyGraph cdg)
{
	IdToChangeMultiHash affectedCUs;
	for (auto change : cdg.changes()) {
		Model::NodeIdType conflictRootA;
		Model::NodeIdType conflictRootB;
		switch (change->type()) {
			case ChangeType::Stationary:
			case ChangeType::Deletion:
				conflictRootA = findConflictUnit(change);
				affectedCUs.insert(conflictRootA, change);
				break;
			case ChangeType::Insertion:
				conflictRootB = findConflictUnit(change);
				affectedCUs.insert(conflictRootB, change);
				break;
			case ChangeType::Move:
				conflictRootA = findConflictUnit(change);
				conflictRootB = findConflictUnit(change);
				affectedCUs.insert(conflictRootA, change);
				affectedCUs.insert(conflictRootB, change);
				break;
			default:
				Q_ASSERT(false);
		}
	}
	return affectedCUs;
}

Model::NodeIdType ConflictUnitDetector::findConflictUnit(std::shared_ptr<ChangeDescription>& change)
{
	// find closest ancestor of node that exists in base
	GenericNode* inBase = change->nodeA();
	GenericNode* node = change->nodeB();
	while (inBase == nullptr || !node->parentId().isNull())
	{
		node = node->parent();
		// FIXME load in base
	}
	if (inBase)
	{
		while (!conflictTypes_.contains(inBase->type()))
			inBase = inBase->parent();
		return inBase->id();
	}
	else
	{
		// no ancestor in base. branch created new root. return 0.
		return QUuid(0);
	}
}

} /* namespace FilePersistence */
