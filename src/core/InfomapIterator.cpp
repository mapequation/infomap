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
			InfomapIterator::operator++();
			if (isEnd()) {
				return *this;
			}
			if (m_current->isLeaf()) {
				// Copy current iterator to restart after iterating through the leaf nodes
				auto firstLeafIt = *this;
				// If on a leaf node, loop through and aggregate to physical nodes
				while (!isEnd() && m_current->isLeaf()) {
					auto ret = m_physNodes.insert(std::make_pair(m_current->physicalId, InfoNode(*m_current)));
					auto& physNode = ret.first->second;
					if (ret.second) {
						// New physical node, use same parent as the state leaf node
						physNode.parent = m_current->parent;
					}
					else {
						// Not inserted, add flow to existing physical node
						//TODO: If exitFlow should be correct, flow between memory nodes within same physical node should be subtracted.
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
				// Set current node to the first physical node
				m_physIter = m_physNodes.begin();
				m_current = &m_physIter->second;
			}
		}
		else {
			// Iterate physical nodes instead of leaf state nodes
			++m_physIter;
			++m_path.back();
			if (m_physIter == m_physNodes.end()) {
				// End of leaf nodes
				m_physNodes.clear();
				m_path.pop_back();
				// reset iterator to the one after the leaf nodes
				*this = m_oldIter;
			}
			else {
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
			InfomapIteratorPhysical::operator++();
		}
	}

	InfomapIterator& InfomapLeafIteratorPhysical::operator++()
	{
		InfomapIteratorPhysical::operator++();
		while (!isEnd() && !m_current->isLeaf()) {
			InfomapIteratorPhysical::operator++();
		}
		return *this;
	}



	// -------------------------------------
	// InfomapUpIterator
	// -------------------------------------

	InfomapUpIterator& InfomapUpIterator::operator++()
	{
		m_current = m_current->parent;
		if (m_current != nullptr && m_current->owner != nullptr) {
			m_current = m_current->owner;
		}
		return *this;
	}

}
