#include "MapBasedGlobalLockImpl.h"

#include <iostream>
#include <cstdio>

using namespace std;

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
	
	if (this->_backend.size() < this->_max_size) {
		if (this->_backend.find(key) != this->_backend.end()) {
		} else {
			cout << "OK\n";
		}
	}

	return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) { return false; }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) { return false; }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) { return false; }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const { return false; }

} // namespace Backend
} // namespace Afina
