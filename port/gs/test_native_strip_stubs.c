/* This CPU-only test preconditions the production recorder to avoid its GPU
   and texture-state paths. A trap is deliberately used, so reaching any
   untested path fails loudly rather than returning a misleading result. */
const char *dhdr_version(void) { return "synthetic-no-gpu"; }
#define TEST_TRAP(name) void name(void) { __builtin_trap(); }
TEST_TRAP(SDL_GetPerformanceCounter)
TEST_TRAP(dhdr_exact_batch)
TEST_TRAP(dhdr_exact_copy)
TEST_TRAP(dhdr_exact_submit)
TEST_TRAP(dhdr_exact_write)
TEST_TRAP(dhdr_texture_create)
TEST_TRAP(dhdr_texture_release)
TEST_TRAP(font_sharp_match)
TEST_TRAP(font_sharp_on)
TEST_TRAP(font_sharp_view)
TEST_TRAP(texpack_find)
TEST_TRAP(texpack_view)
