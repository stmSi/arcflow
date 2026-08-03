#ifndef AF_RUNTIME_INTERNAL_H
#define AF_RUNTIME_INTERNAL_H

#include "arcflow/arcflow_runtime.h"
#include "scene/scene.h"

AfResult af_instance_get_scene(AfRuntime *runtime, AfInstanceHandle instance,
                               const AfScene **out_scene);

#endif
