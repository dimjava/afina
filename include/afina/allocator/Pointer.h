#include <cstdio>

#ifndef AFINA_ALLOCATOR_POINTER_H
#define AFINA_ALLOCATOR_POINTER_H

namespace Afina {
namespace Allocator {
// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Simple;

class Pointer {
private:
	void *ptr;
	void *base;
public:
    Pointer();

    Pointer(void *b) : base(b) {}

	//Pointer(const Pointer &);
	//Pointer(Pointer &&);

    //Pointer &operator=(const Pointer &);
    //Pointer &operator=(Pointer &&);

    void *get() const { 

    	if (ptr == nullptr) {
    		return nullptr;
    	}

    	return ((char *) base) + *((size_t *) ptr);
	}
    void set(void *p) { this->ptr = p; }
};

} // namespace Allocator
} // namespace Afina

#endif // AFINA_ALLOCATOR_POINTER_H
