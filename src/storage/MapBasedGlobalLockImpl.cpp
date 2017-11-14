#include "MapBasedGlobalLockImpl.h"

#include <iostream>

using namespace std;

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
	size_t max_priority = 0;

	if (this->_priorities.size() != 0) {
		max_priority = (*this->_priorities.rbegin()).first;
	}

	if (this->_backend.size() >= this->_max_size) {
		this->_backend.erase((*this->_priorities.begin()).second);
		this->_priorities.erase(this->_priorities.begin());
	}

	map< string, pair<size_t, string> >::iterator it = this->_backend.find(key);

	if (it == this->_backend.end()) {
		pair<size_t, string> p = make_pair(max_priority + 1, key);

		this->_priorities.insert(p);

		pair<size_t, string> p1;
		p1.first = p.first;
		p1.second = value;

		this->_backend[key] = p1;
	} else {
		pair<size_t, string> p;
		p.first = (*it).second.first;
		p.second = (*it).first;
		this->_priorities.erase(p);

		p.first = max_priority + 1;
		this->_priorities.insert(p);

		(*it).second.first = max_priority + 1;
		(*it).second.second = value;
	}

	return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
	if (this->_backend.find(key) == this->_backend.end()) {
		Put(key, value);

		return true;
	}

	return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
	if (this->_backend.find(key) != this->_backend.end()) {
		Put(key, value);

		return true;
	}

	return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
	map< string, pair<size_t, string> >::iterator it = this->_backend.find(key);

	if (it != this->_backend.end()) {
		pair<size_t, string> p;
		p.first = (*it).second.first;
		p.second = (*it).first;

		this->_priorities.erase(p);
		this->_backend.erase(it);
	}

	return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
	map< string, pair<size_t, string> >::const_iterator it = this->_backend.find(key);

	if (it == this->_backend.end()) {
		return false;
	}

	value = it->second.second;

	return true; 
}
} // namespace Backend
} // namespace Afina