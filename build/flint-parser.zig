const std = @import("std");

const FLINTC_VERSION = @import("../build.zig.zon").version;
const COMPILE_FLAGS = @import("../build.zig").compile_flags;
const DYNAMIC_FLAGS = @import("../build.zig").dynamic_flags;

pub fn build(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    prev_step: *std.Build.Step,
    fip_lib: *std.Build.Step.Compile,
    lsp_mode: bool,
    llvm_dynamic: bool,
    stdcpp_include: ?[]const u8,
    glibc_include: ?[]const u8,
) !*std.Build.Step.Compile {
    const name: []const u8 = if (lsp_mode) "flint-parser-lsp" else "flint-parser";
    const lib = b.addLibrary(.{
        .name = name,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = !llvm_dynamic,
            .pic = true,
        }),
        .linkage = .static,
    });
    b.installArtifact(lib);
    lib.link_function_sections = true;
    lib.link_data_sections = true;
    lib.link_gc_sections = true;
    lib.compress_debug_sections = .zlib;
    lib.build_id = .fast;

    lib.root_module.addCMacro("VERSION", b.fmt("\"{s}\"", .{FLINTC_VERSION}));
    if (optimize == .debug) {
        lib.root_module.addCMacro("DEBUG_BUILD", "");
    }
    if (lsp_mode) {
        lib.root_module.addCMacro("FLINT_LSP", "");
    }

    lib.root_module.linkLibrary(fip_lib);
    lib.installLibraryHeaders(fip_lib);

    if (llvm_dynamic) {
        const stdcpp_include_dir = stdcpp_include orelse
            return error.MissingOptionStdcppInclude;
        const glibc_include_dir = glibc_include orelse
            return error.MissingOptionGlibcInclude;

        var it = std.mem.splitScalar(u8, stdcpp_include_dir, ':');
        while (it.next()) |p| {
            if (p.len > 0) {
                lib.root_module.addSystemIncludePath(b.graph.cwdRelativePath(p));
            }
        }

        lib.root_module.addSystemIncludePath(b.graph.cwdRelativePath(glibc_include_dir));
    }

    // Add Include paths
    lib.root_module.addIncludePath(b.path("include"));

    // Collect C++ files
    var src_dir: std.Io.Dir = try b.root.openDir(b.graph.io, "src", .{ .iterate = true });
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

    const flags = if (llvm_dynamic)
        COMPILE_FLAGS ++ DYNAMIC_FLAGS
    else
        COMPILE_FLAGS;

    // Add C++ src files
    lib.root_module.addCSourceFiles(.{
        .root = b.path("src"),
        .files = cpp_files.items,
        .flags = flags,
    });

    const to_install_dirs = [_][]const u8{
        "analyzer",
        "error",
        "lexer",
        "matcher",
        "parser",
        "resolver",
    };
    const to_install_files = [_][]const u8{
        "colors",
        "debug",
        "fip",
        "globals",
        "persistent_thread_pool",
        "profiler",
        "single_executor_guard",
        "types",
    };

    var last: *std.Build.Step = prev_step;
    for (to_install_dirs) |dir| {
        const install = b.addInstallDirectory(.{
            .source_dir = b.path(b.fmt("include/{s}", .{dir})),
            .install_dir = .header,
            .install_subdir = dir,
        });
        install.step.dependOn(last);
        last = &install.step;
    }

    for (to_install_files) |file| {
        const install = b.addInstallHeaderFile(b.path(b.fmt("include/{s}.hpp", .{file})), b.fmt("{s}.hpp", .{file}));
        install.step.dependOn(last);
        last = &install.step;
    }

    lib.step.dependOn(last);
    return lib;
}
