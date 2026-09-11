const std = @import("std");
const fip = @import("fip");

const FLINTC_VERSION = @import("../build.zig.zon").version;
const COMPILE_FLAGS = @import("../build.zig").compile_flags;

pub fn build(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    prev_step: *std.Build.Step,
    install_headers: bool,
    lib_mode: fip.LibMode,
    lsp_mode: bool,
) !*std.Build.Step.Compile {
    const name: []const u8 = if (lsp_mode) "flint-parser-lsp" else "flint-parser";
    const lib = b.addLibrary(.{
        .name = name,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
            .pic = true,
        }),
        .linkage = .static,
    });
    b.installArtifact(lib);
    lib.step.dependOn(prev_step);
    lib.link_function_sections = true;
    lib.link_data_sections = true;
    lib.link_gc_sections = true;
    lib.compress_debug_sections = .zlib;
    lib.build_id = .fast;

    lib.root_module.addCMacro("VERSION", b.fmt("\"{s}\"", .{FLINTC_VERSION}));
    if (optimize == .Debug) {
        lib.root_module.addCMacro("DEBUG_BUILD", "");
    }
    if (lsp_mode) {
        lib.root_module.addCMacro("FLINT_LSP", "");
    }

    const fip_dep = b.dependency("fip", .{ .target = target, .optimize = optimize, .@"lib-mode" = lib_mode });
    lib.root_module.linkLibrary(fip_dep.artifact("fip"));
    lib.installLibraryHeaders(fip_dep.artifact("fip"));

    // Add Include paths
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
        .flags = COMPILE_FLAGS,
    });

    if (install_headers) {
        lib.installHeadersDirectory(b.path("include/analyzer"), "analyzer", .{ .include_extensions = &.{".hpp"} });
        lib.installHeadersDirectory(b.path("include/error"), "error", .{ .include_extensions = &.{".hpp"} });
        lib.installHeadersDirectory(b.path("include/lexer"), "lexer", .{ .include_extensions = &.{".hpp"} });
        lib.installHeadersDirectory(b.path("include/matcher"), "matcher", .{ .include_extensions = &.{".hpp"} });
        lib.installHeadersDirectory(b.path("include/parser"), "parser", .{ .include_extensions = &.{".hpp"} });
        lib.installHeadersDirectory(b.path("include/resolver"), "resolver", .{ .include_extensions = &.{".hpp"} });

        lib.installHeader(b.path("include/colors.hpp"), "colors.hpp");
        lib.installHeader(b.path("include/debug.hpp"), "debug.hpp");
        lib.installHeader(b.path("include/fip.hpp"), "fip.hpp");
        lib.installHeader(b.path("include/globals.hpp"), "globals.hpp");
        lib.installHeader(b.path("include/persistent_thread_pool.hpp"), "persistent_thread_pool.hpp");
        lib.installHeader(b.path("include/profiler.hpp"), "profiler.hpp");
        lib.installHeader(b.path("include/single_executor_guard.hpp"), "single_executor_guard.hpp");
        lib.installHeader(b.path("include/types.hpp"), "types.hpp");
    }
    return lib;
}
