const std = @import("std");

const flint_parser = @import("flint-parser.zig");

const FLINTC_VERSION = @import("../build.zig.zon").version;
const COMPILE_FLAGS = @import("../build.zig").compile_flags;

pub fn build(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    flint_parser_lib: *std.Build.Step.Compile,
    commit_hash: []const u8,
    build_date: []const u8,
) !void {
    const exe = b.addExecutable(.{
        .name = if (optimize == .Debug) "fls-debug" else "fls",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
            .pic = true,
        }),
    });
    b.installArtifact(exe);
    exe.root_module.linkLibrary(flint_parser_lib);
    exe.link_function_sections = true;
    exe.link_data_sections = true;
    exe.link_gc_sections = true;
    exe.compress_debug_sections = .zlib;
    exe.build_id = .fast;

    // Add Macros
    exe.root_module.addCMacro("FLINT_LSP", "");
    exe.root_module.addCMacro("VERSION", b.fmt("\"{s}\"", .{FLINTC_VERSION}));
    exe.root_module.addCMacro("COMMIT_HASH", commit_hash);
    exe.root_module.addCMacro("BUILD_DATE", build_date);
    if (optimize == .Debug) {
        exe.root_module.addCMacro("DEBUG_BUILD", "");
    }

    // Add Include paths
    exe.root_module.addIncludePath(b.path("include"));
    exe.root_module.addIncludePath(b.path("fls/include"));

    // Add C++ src files
    exe.root_module.addCSourceFiles(.{
        .files = &[_][]const u8{
            // LSP sources
            "fls/src/main.cpp",
            "fls/src/lsp_server.cpp",
            "fls/src/lsp_protocol.cpp",
            "fls/src/completion_data.cpp",
            "fls/src/completion.cpp",

            // fip.cpp is not part of the parser library
            "src/fip.cpp",
        },
        .flags = COMPILE_FLAGS,
    });
}
