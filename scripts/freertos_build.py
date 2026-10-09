"""Build only STM32Cube's selected FreeRTOS sources for the Stage 1 smoke test."""

from pathlib import Path

Import("env")

framework = env.PioPlatform().get_package_dir("framework-stm32cubef4")
if not framework:
    raise RuntimeError("The installed STM32CubeF4 framework package is required.")

source = Path(framework) / "Middlewares" / "Third_Party" / "FreeRTOS" / "Source"
selected = (
    "tasks.c",
    "queue.c",
    "list.c",
    "portable/GCC/ARM_CM4F/port.c",
    "portable/MemMang/heap_4.c",
)
for relative in selected:
    if not (source / relative).is_file():
        raise RuntimeError("Missing bundled FreeRTOS source: " + relative)

env.AppendUnique(CPPPATH=[
    str(Path(env.subst("$PROJECT_DIR")) / "include"),
    str(source / "include"),
    str(source / "portable" / "GCC" / "ARM_CM4F"),
])
env.AppendUnique(LINKFLAGS=[
    "-Wl,-Map," + str(Path(env.subst("$BUILD_DIR")) / "firmware.map"),
])

# PRE scripts can add program objects before PlatformIO constructs the ELF.
# Excluding everything first prevents a second port/heap or unused middleware.
env.BuildSources(
    str(Path(env.subst("$BUILD_DIR")) / "FreeRTOS"),
    str(source),
    "-<*> " + " ".join("+<" + relative + ">" for relative in selected),
)
