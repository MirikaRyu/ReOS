set_languages("c++latest")

add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})

option("debug", {default = false, description = "Enable Debug Build"})

target("ReOS")
    set_kind("binary")
    
    -- Files
    add_files("src/**.S")
    add_files("src/**.cpp")
    add_files("src/**.ixx")
    add_files("src/**.cppm", {public = false})
    
    -- Policies
    set_policy("build.optimization.lto", true)
    set_policy("build.c++.modules.std", false)
    set_policy("build.c++.modules.culling", false)
    set_policy("check.auto_ignore_flags", false)
    
    -- Architecture Flags
    add_cxflags(
        "-ffreestanding",
        "-mcmodel=medany",
        "--specs=nosys.specs --specs=nano.specs -nostartfiles",
        "-Wall -Wextra -Wpedantic -Werror",
        "-Wshadow -Wpointer-arith -Wcast-align",
        "-Wredundant-decls -Wdouble-promotion -Wno-psabi",
        "-fstack-protector-strong",
        "-fdiagnostics-color=always"
    )
    add_cxxflags(
        "-Wnull-dereference -Wsuggest-override -Wno-missing-field-initializers",
        "-Wnon-virtual-dtor -Woverloaded-virtual -Wdelete-non-virtual-dtor",
        "-fno-rtti -fno-exceptions -fno-threadsafe-statics"
    )
    add_ldflags(
        "-T" .. "src/arch/riscv/link.ld",
        "-Wl,--print-memory-usage,-Map=build/ReOS.map,--no-warn-rwx-segments,--gc-sections"
    )
    
    -- Build Options
    if has_config("debug") then
        set_optimize("none")
        set_symbols("debug")
        add_defines("DEBUG")
    else
        set_optimize("fastest")
        set_symbols("debug")
        add_defines("NDBUG")
    end
target_end()