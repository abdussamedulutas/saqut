#include "vm/shadow_stack.hpp"
ShadowStack& jitShadowStack() { static ShadowStack s; return s; }
