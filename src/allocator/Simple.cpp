#include <cmath>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <afina/allocator/Simple.h>
#include <afina/allocator/Pointer.h>
#include <afina/allocator/Error.h>

using namespace std;

namespace Afina {
namespace Allocator {

const size_t BITES_PER_BYTE = 8;

Simple::Simple(void *base, size_t size) : _base(base), _base_len(size), _data_end((char *) base) {
	//each element requires place for 2 size_t numbers: 
	//index and size of allocated memory
	this->_sys_start = (size_t *) (((char *) base) + size - sizeof(size_t));
	//put default values meaning that block is empty
	this->_sys_start[0] = -1;
	this->_sys_start[-1] = 0;
	//end is included
	this->_sys_end = this->_sys_start - 1;
	//pointer to the top of empty blocks queue
	this->_top = base;
	//
	*((size_t **) this->_top) = nullptr;
	//put size of empty block in front of this block
	*(((size_t *) this->_top) + 1) = size;
	
	//cout << "BASE = " << this->_base << endl;

	//TODO: what if size if less than sizeof(size_t) ???
}


void Simple::print_sys() {
	//cout << "\n\n";

	//cout << "TOP: " << this->_top << endl;
	//cout << "IN TOP: " << *((void **) this->_top) << endl;
	size_t sys_len = (this->_sys_start - this->_sys_end) + 1;
	for (int i = 0; i < sys_len; i += 2) {
		//cout << "SYS: " << this->_sys_start[-i] << ' ' << this->_sys_start[-i - 1] << endl;
	}
	//cout << "\n\n";

	// for (int k = 0; k < this->_num_of_blocks; k++) {
	// 	cout << *(this->_pointers - k) << ' ';
	// }
	//cout << "\n";
}

void Simple::print_queue() {
	//cout << "\n\n";
	void *queue = this->_top;
	while (queue != nullptr) {
		//cout << "QUEUE: q, *q, *(q+1) = " << queue << ' ' << *((void **)queue) << ' ' << *((size_t *) queue + 1) << endl; 
		queue = *((void **) queue);
	}
}

/**
 * TODO: semantics
 * @param N size_t
 */
Pointer Simple::alloc(size_t N) {
	if (N == 0) {
		Pointer p = Pointer();
		p.set(nullptr);


		return p;
	}

	//fix this
	if (N < 2 * sizeof(size_t)) {
		throw AllocError(AllocErrorType::NoMemory, "N is too small");
	}

	if (this->_top == nullptr) {
		throw AllocError(AllocErrorType::NoMemory, "No empty blocks");
	}

	void *prev = nullptr;
	void *queue = this->_top;
	size_t *sizes = (size_t *) queue + 1;

	//cout << "Searching free block of size: " << N << endl;

	while (queue != nullptr) {
	 	size_t available_size = *sizes;

	 	//cout << "AVAILAVLE = " << available_size << "\n";

	 	if (available_size < N) {
	 		prev = queue;
	 		queue = *((void **) queue);
	 		sizes = ((size_t *) queue) + 1;

	 		continue;
	 	}

	 	//cout << "FOUND BLOCK: " << available_size << ' ' << queue << "\n";
	 	break;
	}

	//cout << "Checking block state\n";

	//TODO: defrag
	if (!(queue != nullptr && *sizes >= N)) {
		throw AllocError(AllocErrorType::NoMemory, "Not found mem");
	}

	if (prev == nullptr) {
		prev = &(this->_top);
	}

	size_t sys_len = (this->_sys_start - this->_sys_end) + 1;
	Pointer p = Pointer(this->_base);

	//cout << "Filling sys info\n";

	//check data is short enough
	char *data_end = ((char *) queue) + N;
	if (data_end > (char *) this->_sys_end) {
		//cout << "Found -1 and N is too big\n\n\n";
		throw AllocError(AllocErrorType::NoMemory, "Not found mem");
	}

	//Filling sys array and ptr
	bool flag = 0;
	for (int i = 0; i < sys_len; i += 2) {
		if (this->_sys_start[-i] == -1) {
			flag = 1;	
			p.set(this->_sys_start - i);
			this->_sys_start[-i] = ((char *) queue) - ((char *) this->_base);
			this->_sys_start[-i - 1] = N;

			//cout << "idx = " << ((char *) queue) - ((char *) this->_base) << "\n";

			if (data_end > this->_data_end) {
				this->_data_end = data_end;
			}
		}
	}
	if (!flag) {
		//TODO: expansion of sys => decreasing size of last free block
		if (data_end > (char *) (this->_sys_end - 2 * sizeof(size_t)) 
			|| this->_data_end >= (char *) (this->_sys_end - 2 * sizeof(size_t))) {
			//cout << "Requires extending sys and N is too big\n\n\n";
			throw AllocError(AllocErrorType::NoMemory, "Not found mem");
		}

		this->_sys_end -= 2;
		this->_sys_end[1] = ((char *) queue) - ((char *) this->_base);
		this->_sys_end[0] = N;
		p.set(this->_sys_end + 1);

		//cout << "!flag and idx = " << ((char *) queue) - ((char *) this->_base) << endl;


		if (data_end > (this->_data_end)) {
			this->_data_end = data_end;
		}
	}

	//cout << "ptr0 = " << this->_top << "\n";

	size_t data_left = 0;
	if (*sizes >= N) {
		data_left = (*sizes) - N;
	}

	if (data_left < 2 * sizeof(size_t)) {
		*((void **) prev) = *((void **) queue);
	} else {
		*((void **) prev) = ((char *) queue) + N;
		
		void *ptr = ((char *) queue) + N;
		*((void **) ptr) = *((void **) queue);
		*((size_t *) ptr + 1) = data_left;
	}

	//cout << "ptr1 = " << this->_top << "\n";
	//cout << endl << endl << endl;

	return p;
}

void Simple::merge_empty_blocks() {
	void *queue = this->_top;
	size_t *sizes = (size_t *) queue + 1;


	while (queue != nullptr) {
		size_t total_size = *sizes;
		void *last = *((void **) queue);

		//cout << "MERGING, before size = " << *sizes << endl;

		while (last != nullptr) {
			if (((char *) last - (char *) queue) - total_size >= 2 * sizeof(size_t)) {
				break;
			}

			total_size = ((char *) last - (char *) queue) + *(((size_t *) last) + 1);
			last = *((void **) last);
		}

		*sizes = total_size;
		queue = last;

		//cout << "MERGING, new size = " << total_size << endl;
	}
}

void Simple::insert_into_queue(void *it, size_t len) {
	void **ptr = (void **) it;
	void *queue = this->_top;

	if (queue == nullptr || queue > it) {
		*ptr = this->_top;
		*((size_t *) it + 1) = len;
		this->_top = it;
	}

	while (queue != nullptr && queue < it) {	
		void *next = *((void **) queue);
		if (queue < it) {
			if (next == nullptr) {
				*((size_t *) ptr + 1) = len;
				*ptr = nullptr;
				*((void **) queue) = it;
				break;
			} else if (it < next) {
				*((size_t *) it + 1) = len;
				*ptr = next;
				*((void **) queue) = it;
				break;
			}
		}
		queue = *((void **) queue);
	}
}

/**
 * TODO: semantics
 * @param p Pointer
 * @param N size_t
 */
void Simple::realloc(Pointer &p, size_t N) {

	if (p.get() == nullptr) {
		p = alloc(N);

		return;
	}

	//fix this
	if (N < 2 * sizeof(size_t)) {
		throw AllocError(AllocErrorType::NoMemory, "N is too small");
	}

	size_t sys_len = (this->_sys_start - this->_sys_end) + 1;
	size_t idx = (char *) p.get() - (char *) this->_base;

	//cout << "realloc idx = " << idx << endl;
	size_t len = 0;

	int sys_index = -1;
	for (int i = 0; i < sys_len; i += 2) {
		if (this->_sys_start[-i] == idx) {
			len = this->_sys_start[-i - 1];
			sys_index = i;
			break;
		}
	}

	//cout << "len = " << len << endl;
	if (len == 0) {
		return;
	}


	if (N <= len) {
		size_t diff = len - N;

		if (diff >= 2 * sizeof(size_t)) {
			void **ptr = (void **) p.get() + N;
			*ptr = this->_top;
			*((size_t *) ptr + 1) = len;
			this->_top = ptr;

			this->_sys_start[-idx - 1] = N;
		}

		return;
	}

	merge_empty_blocks();

	//cout << "N > len\n";
	// N > len
	size_t diff = N - len;
	void *prev = nullptr;
	void *queue = this->_top;
	size_t *sizes = (size_t *) queue + 1;
	void *it = p.get();

	//trying to expand block
	while (queue != nullptr) {
		if (queue < it) {
			prev = queue;
			queue = *((void **) queue);
	 		sizes = (size_t *)((void **) queue + 1);
			
			continue;
		}

		//nearest free block too far
		if ((char *) queue - ((char *) it + len) >= 2 * sizeof(size_t)) {
			break;
		}

	 	size_t available_size = *sizes + ((char *) queue - ((char *) it + len));

	 	//cout << "AVAILAVLE FOR EXPANDING = " << available_size << "\n";

	 	if (available_size < diff) {
	 		break;
	 	}

	 	if (diff - available_size < 2 * sizeof(size_t)) {
	 		if (prev == nullptr) {
	 			this->_top =  (char *) it + N;
	 		} else {
	 			//*((void **) prev) = *((void **) pr);
	 			*((void **) prev) = *((void **) queue);
	 		}
	 		this->_sys_start[-idx - 1] = N;

	 		return;
	 	}

	 	void *chunk = (char *) it + N;
	 	if (prev == nullptr) {
	 		this->_top = chunk;
	 	} else {
	 		*((void **) prev) = chunk;
	 	}
	 	*((void **) chunk) = *((void **) queue);//pr;
	 	*((size_t *) chunk + 1) = diff - available_size;
	 	this->_sys_start[-idx - 1] = N;

	 	return;
	}


	//Find free block otherwise
	queue = this->_top;
	prev = nullptr;

	while (queue != nullptr) {
		size_t available_size = *((size_t *) queue + 1);

		if (available_size < N) {
			prev = queue;
			queue = *((void **) queue);

			continue;
		}

		void *next = *((void **) queue);

		memcpy(queue, it, len);
		this->_sys_start[-sys_index] = (char *) queue - (char *) this->_base;
		this->_sys_start[-sys_index - 1] = N;

		if (available_size - N < 2 * sizeof(size_t)) {
			*((void **) prev) = next;
			insert_into_queue(it, len);
			return;
		}

		insert_into_queue(it, len);
		insert_into_queue(queue, available_size - N);

		return;
	}	

	throw AllocError(AllocErrorType::NoMemory, "Not found mem");
}

/**
 * TODO: semantics
 * @param p Pointer
 */
void Simple::free(Pointer &p) {
	if (p.get() == nullptr) {
		return;
	}

	size_t idx = (char *) p.get() - (char *) this->_base;

	size_t sys_len = (this->_sys_start - this->_sys_end) + 1;
	for (int i = 0; i < sys_len; i += 2) {
		if (this->_sys_start[-i] == idx) {
			size_t len = this->_sys_start[-i - 1];

			////cout << "DELETING: " << i << ' ' << idx << ' ' << len << endl;

			if (len >= 2 * sizeof(size_t)) {
				void *it = p.get();
				insert_into_queue(it, len);
			}

			this->_sys_start[-i] = -1;
			this->_sys_start[-i - 1] = 0;

			break;
		}
	}

	//cout << "Freeing idx = " << idx << ' ' << sys_len << endl;

	p.set(nullptr);
}

/**
 * TODO: semantics
 */
void Simple::defrag() {
	char *defraged_end = (char *) this->_base;
	size_t sys_len = (this->_sys_start - this->_sys_end) + 1;
	size_t prev_min = -1;

	//cout << "END before defrag = " << (void *) defraged_end << endl;
	print_sys();

	for (;;) {
		size_t mmin = -1;
		int idx = -1;

		for (int i = 0; i < sys_len; i += 2) {

			if (prev_min == -1) {
				if (this->_sys_start[-i] < mmin) {
					mmin = this->_sys_start[-i];
					idx = i;
				}
			}

			if (this->_sys_start[-i] > prev_min && this->_sys_start[-i] < mmin) {
				mmin = this->_sys_start[-i];
				idx = i;
			}
		}

		////cout << "Mmin = " << mmin << endl;

		if (mmin == -1) {
			break;
		}

		if (defraged_end == (char *) this->_base + mmin) {
			prev_min = mmin;
			defraged_end += this->_sys_start[-idx - 1];
			continue;
		}

		this->_sys_start[-idx] = defraged_end - (char *) this->_base;
		memmove(defraged_end, (char *) this->_base + mmin, this->_sys_start[-idx - 1]);
		defraged_end += this->_sys_start[-idx - 1];

		prev_min = mmin;
	}

	this->_top = nullptr;
	if (defraged_end < (char *) (this->_sys_end - 2)) {
		this->_top = defraged_end;
		*((void **) this->_top) = nullptr;
		*((size_t *) this->_top + 1) = this->_base_len - (defraged_end - (char *) this->_base);
	}


	//cout << "END after defrag = " << (void *) defraged_end << endl;

	print_sys();
}

/**
 * TODO: semantics
 */
std::string Simple::dump() const { return ""; }

} // namespace Allocator
} // namespace Afina
