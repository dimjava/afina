#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

using namespace std;

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
	char stack_end;

	ctx.High = max(StackBottom, &stack_end);
	ctx.Low = min(StackBottom, &stack_end);

	ptrdiff_t len = ctx.High - ctx.Low;

	if (get<0>(ctx.Stack) != nullptr) {
		delete[] get<0>(ctx.Stack);
	}

	ctx.Stack = make_tuple(new char[len], len);
	memcpy(get<0>(ctx.Stack), ctx.Low, len);
}

void Engine::Restore(context &ctx) {
	char cur_stack_end;

	if (ctx.Low < &cur_stack_end && ctx.High > &cur_stack_end) {
		Restore(ctx);
	}

	memcpy(StackBottom - get<1>(ctx.Stack), get<0>(ctx.Stack), get<1>(ctx.Stack));
	longjmp(ctx.Environment, 1);
}

void Engine::yield() {
	if (alive == nullptr) {
		return;
	}

	context *ctx = alive;

	// find routine != cur_routine
	while (ctx != nullptr && ctx == cur_routine) {
		ctx = ctx -> next;
	}

	if (ctx == nullptr) {
		return;
	}

	return sched(ctx);
}

void Engine::sched(void *routine_) {
	context *ctx = (context *) routine_;

	if (cur_routine != nullptr) {
		Store(*cur_routine);

		if (setjmp(cur_routine->Environment) != 0) {
			return;
		}
	}

	cur_routine = ctx;
	Restore(*ctx);
}

} // namespace Coroutine
} // namespace Afina
