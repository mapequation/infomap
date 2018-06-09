#include "InfomapIterator.h"
#include "InfoNode.h"
#include <utility> // std::pair
#include "../utils/Log.h"

namespace infomap {

	InfomapIterator& InfomapIterator::operator++()
	{
		InfoNode* curr = m_current;
		InfoNode* infomapRoot = curr->getInfomapRoot();
		if (infomapRoot != nullptr)
		{
			curr = infomapRoot;
		}

		if(curr->firstChild != nullptr)
		{
			curr = curr->firstChild;
			++m_depth;
			m_path.push_back(0);
		}
		else
		{
			// Current node is a leaf
			// Presupposes that the next pointer can't reach out from the current parent.
			tryNext:
			while(curr->next == nullptr)
			{
				if (curr->parent != nullptr)
				{
					curr = curr->parent;
					--m_depth;
					m_path.pop_back();
					if(curr == m_root) // Check if back to beginning
					{
						m_current = nullptr;
						return *this;
					}
					if (m_moduleIndexLevel < 0) {
						 if (curr->isLeafModule()) // TODO: Generalize to -2 for second level to bottom
							 ++m_moduleIndex;
					}
					else if (static_cast<unsigned int>(m_moduleIndexLevel) == m_depth)
						++m_moduleIndex;
				}
				else
				{
					InfoNode* infomapOwner = curr->owner;
					if (infomapOwner != nullptr)
					{
						curr = infomapOwner;
						if(curr == m_root) // Check if back to beginning
						{
							m_current = nullptr;
							return *this;
						}
						goto tryNext;
					}
					else // null also if no children in first place
					{
						m_current = nullptr;
						return *this;
					}
				}
			}
			curr = curr->next;
			++m_path.back();
		}
		m_current = curr;
		return *this;
	}

	// -------------------------------------
	// InfomapModuleIterator
	// -------------------------------------

	InfomapIterator& InfomapModuleIterator::operator++()
	{
		InfomapIterator::operator++();
		if (m_current->isLeaf())
		while (!isEnd() && m_current->isLeaf()) {
			InfomapIterator::operator++();
		}
		return *this;
	}

	// -------------------------------------
	// InfomapLeafModuleIterator
	// -------------------------------------

	void InfomapLeafModuleIterator::init()
	{
		while (!isEnd() && !m_current->isLeafModule()) {
			InfomapIterator::operator++();
		}
	}

	InfomapIterator& InfomapLeafModuleIterator::operator++()
	{
		InfomapIterator::operator++();
		while (!isEnd() && !m_current->isLeafModule()) {
			InfomapIterator::operator++();
		}
		return *this;
	}

	// -------------------------------------
	// InfomapLeafIterator
	// -------------------------------------

	void InfomapLeafIterator::init()
	{
		while (!isEnd() && !m_current->isLeaf()) {
			InfomapIterator::operator++();
		}
	}

	InfomapIterator& InfomapLeafIterator::operator++()
	{
		InfomapIterator::operator++();
		while (!isEnd() && !m_current->isLeaf()) {
			InfomapIterator::operator++();
		}
		return *this;
	}

	// -------------------------------------
	// InfomapIteratorPhysical
	// -------------------------------------

	InfomapIterator& InfomapIteratorPhysical::operator++()
	{
		if (m_physNodes.empty()) {
			// Iterate modules
			Log(1) << "\n++modules" << std::flush;
			InfomapIterator::operator++();
			if (m_current->isLeaf()) {
				// Copy current iterator to restart after iterating through the leaf nodes
				auto firstLeafIt = *this;
				Log(1) << " -> first leaf node among " << m_current->parent->childDegree() << std::flush;
				// If on a leaf node, loop through and aggregate to physical nodes
				while (!isEnd() && m_current->isLeaf()) {
					Log(1) << "\n  ++isLeaf -> stateId: " << m_current->stateId << ", physId: " << m_current->physicalId << std::flush;
					auto ret = m_physNodes.insert(std::make_pair(m_current->physicalId, InfoNode(*m_current)));
					auto& physNode = ret.first->second;
					if (ret.second) {
						// New physical node, use same parent as the state leaf node
						physNode.parent = m_current->parent;
						Log(1) << " -> save new phys node: " << m_current->data << std::flush;
					}
					else {
						// Not inserted, add flow to existing physical node
						//TODO: If exitFlow should be correct, flow between memory nodes within same physical node should be subtracted.
						Log(1) << " -> += " << m_current->data << std::flush;
						physNode.data += m_current->data;
					}
					InfomapIterator::operator++();
				}
				// Store current iterator to continue with after iterating physical leaf nodes
				m_oldIter = *this;
				// Reset path/depth/moduleIndex to values for first leaf node
				// *this = firstLeafIt;
				m_path = firstLeafIt.m_path;
				m_depth = firstLeafIt.m_depth;
				m_moduleIndex = firstLeafIt.m_moduleIndex;
				Log(1) << "\n => Aggregated " << m_physNodes.size() << " physical nodes" << std::flush;
				Log(1) << ", oldIter path size: " << m_oldIter.path().size();
				if (m_oldIter.path().size() > 0) Log(1) << " (" << m_oldIter.path().back() << ")";
				// Set current node to the first physical node
				m_physIter = m_physNodes.begin();
				m_current = &m_physIter->second;
				Log(1) << " -> set currrent node to first physical node " << m_current->physicalId << ": " << m_current->data << std::flush;
			}
		}
		else {
			// Iterate physical nodes instead of leaf state nodes
			++m_physIter;
			++m_path.back();
			Log(1) << "\n++physIter" << std::flush;
			if (m_physIter == m_physNodes.end()) {
				// End of leaf nodes
				Log(1) << " -> end -> clearing " << m_physNodes.size() << " phys nodes" << std::flush;
				m_physNodes.clear();
				m_path.pop_back();
				// reset iterator to the one after the leaf nodes
				*this = m_oldIter;
				Log(1) << ", reset to old iter at depth " << depth() << " path.size: " << m_path.size() << " ";
				if (m_path.size() > 0) Log(1) << " (" << m_path.back() << ") ";
				if (!isEnd()) { Log(1) << ", data: " << m_current->data << std::flush; }
			}
			else {
				Log(1) << " -> stateId: " << m_current->stateId << ", physId: " << m_current->physicalId
				<< ", data: " << m_current->data << std::flush;
				// Set iterator node to the currently iterated physical node
				m_current = &m_physIter->second;
			}
		}
		return *this;
	}

	// -------------------------------------
	// InfomapLeafIteratorPhysical
	// -------------------------------------

	void InfomapLeafIteratorPhysical::init()
	{
		while (!isEnd() && !m_current->isLeaf()) {
			InfomapIterator::operator++();
		}
	}

	InfomapIterator& InfomapLeafIteratorPhysical::operator++()
	{
		InfomapIteratorPhysical::operator++();
		while (!isEnd() && !m_current->isLeaf()) {
			InfomapIterator::operator++();
		}
		return *this;
	}

}
