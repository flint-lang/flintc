const std = @import("std");

const FLINTC_VERSION = @import("build.zig").FLINTC_VERSION;

const hasInternetConnection = @import("build.zig").hasInternetConnection;
const makeEmptyStep = @import("build.zig").makeEmptyStep;

// zig fmt: off
pub const compile_flags = &[_][]const u8{
    "-std=c++20",                           // Set C++ standard to C++20
    "-Werror",                              // Treat warnings as errors
    "-Wall",                                // Enable most warnings
    "-Wextra",                              // Enable extra warnings
    "-Wshadow",                             // Warn about shadow variables
    "-Wcast-align",                         // Warn about pointer casts that increase alignment requirement
    "-Wcast-qual",                          // Warn about casts that remove const qualifier
    "-Wunused",                             // Warn about unused variables
    "-Wold-style-cast",                     // Warn about C-style casts
    "-Wdouble-promotion",                   // Warn about float being implicitly promoted to double
    "-Wformat=2",                           // Warn about printf/scanf/strftime/strfmon format string issue
    "-Wundef",                              // Warn if an undefined identifier is evaluated in an #if
    "-Wpointer-arith",                      // Warn about sizeof(void) and add/sub with void*
    "-Wunreachable-code",                   // Warn about unreachable code
    "-fno-omit-frame-pointer",              // Prevent omitting frame pointer for debugging and stack unwinding
    "-funwind-tables",                      // Generate unwind tables for stack unwinding
    "-ffunction-sections",                  // Place each function in its own section
    "-fdata-sections",                      // Place each data object in its own section
    "-fstandalone-debug",                   // Emit standalone debug information
    "-fno-sanitize=undefined",              // Disable sanitizer to prevent "missing ubsan" compile errors
    "-Wno-unused-command-line-argument",    // Supresses "argument unused during compilation" warning
};
// zig fmt: on

pub fn build_flint_parser_lib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) !*std.Build.Step.Compile {
    const lib = b.addLibrary(.{
        .name = "flint-parser",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
            .pic = true,
        }),
        .linkage = .static,
    });
    lib.link_function_sections = true;
    lib.link_data_sections = true;
    lib.link_gc_sections = true;
    lib.compress_debug_sections = .zlib;
    lib.build_id = .fast;

    lib.root_module.addCMacro("VERSION", b.fmt("\"{s}\"", .{FLINTC_VERSION}));
    if (optimize == .Debug) {
        lib.root_module.addCMacro("DEBUG_BUILD", "");
    }

    // Add Include paths
    lib.root_module.addIncludePath(b.dependency("fip", .{}).path("."));
    lib.root_module.addIncludePath(b.path("include"));

    // Collect C++ files
    const path = try b.path(".").getPath4(b, &lib.step);
    var src_dir: std.Io.Dir = try path.openDir(b.graph.io, "src", .{ .iterate = true });
    defer src_dir.close(b.graph.io);
    var cpp_files: std.ArrayList([]const u8) = .empty;
    defer cpp_files.deinit(b.allocator);
    var walker = try src_dir.walk(b.allocator);
    defer walker.deinit();
    while (try walker.next(b.graph.io)) |entry| {
        if (entry.kind == .file and
            std.mem.endsWith(u8, entry.basename, ".cpp") and
            !std.mem.containsAtLeast(u8, entry.path, 1, "generator") and
            !std.mem.eql(u8, entry.basename, "main.cpp") and
            !std.mem.eql(u8, entry.basename, "linker.cpp") and
            !std.mem.eql(u8, entry.basename, "fip.cpp"))
        {
            try cpp_files.append(b.allocator, try b.allocator.dupe(u8, entry.path));
        }
    }

    // Add C++ src files
    lib.root_module.addCSourceFiles(.{
        .root = b.path("src"),
        .files = cpp_files.items,
        .flags = compile_flags,
    });

    // Add toml C src file for FIP
    lib.root_module.addCSourceFile(.{
        .file = b.dependency("fip", .{}).path("toml/tomlc17.c"),
        .flags = &[_][]const u8{
            "-fno-sanitize=undefined", // Disable sanitizer to prevent "missing ubsan" compile errors
        },
    });

    return lib;
}
