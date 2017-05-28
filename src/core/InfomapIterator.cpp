#include "InfomapIterator.h"
#include "InfoNode.h"

namespace infomap {

	// InfoNode* InfomapIterator::current()
	// {
	// 	return m_current;
	// }

	// InfoNode* InfomapIterator::operator->()
	// {
	// 	return m_current;
	// }

	// InfoNode& InfomapIterator::operator*()
	// {
	// 	return *m_current;
	// }

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
}
