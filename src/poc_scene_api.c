// This file provides the public API implementations for the scene system.
// The actual implementations are in scene.c, scene_object.c, and mesh.c.
// This file just exposes them through the public API defined in poc_engine.h.

#include "scene.h"
#include "scene_object.h"
#include "mesh.h"

// Note: The public API functions in poc_engine.h have the same signatures
// as the internal functions, so the linker will automatically resolve them.
// No wrapper functions are needed - the internal implementations serve
// as the public API implementations directly.