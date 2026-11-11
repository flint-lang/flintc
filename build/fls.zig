const std = @import("std");

const flint_parser = @import("flint-parser.zig");

const FLINTC_VERSION = @import("../build.zig.zon").version;
const COMPILE_FLAGS = @import("../build.zig").compile_flags;
const DYNAMIC_FLAGS = @import("../build.zig").dynamic_flags;

pub fn build(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    flint_parser_lib: *std.Build.Step.Compile,
    commit_hash: []const u8,
    build_date: []const u8,
    llvm_dynamic: bool,
    stdcpp_include: ?[]const u8,
    stdcpp_lib: ?[]const u8,
    glibc_include: ?[]const u8,
) !*std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = if (optimize == .debug) "fls-debug" else "fls",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = !llvm_dynamic,
            .pic = true,
            .strip = optimize != .debug,
        }),
    });
    exe.root_module.linkLibrary(flint_parser_lib);
    exe.link_function_sections = true;
    exe.link_data_sections = true;
    exe.link_gc_sections = true;
    exe.compress_debug_sections = .zlib;
    exe.build_id = .fast;

    // Add Macros
    exe.root_module.addCMacro("FLINT_LSP", "");
    exe.root_module.addCMacro("VERSION", b.fmt("{q}", .{FLINTC_VERSION}));
    exe.root_module.addCMacro("COMMIT_HASH", b.fmt("{q}", .{commit_hash}));
    exe.root_module.addCMacro("BUILD_DATE", b.fmt("{q}", .{build_date}));
    if (optimize == .debug) {
        exe.root_module.addCMacro("DEBUG_BUILD", "");
    }

    // Add Include paths
    exe.root_module.addIncludePath(b.path("include"));
    exe.root_module.addIncludePath(b.path("fls/include"));

    if (llvm_dynamic) {
        const stdcpp_include_dir = stdcpp_include orelse
            return error.MissingOptionStdcppInclude;
        const stdcpp_library = stdcpp_lib orelse
            return error.MissingOptionStdcppLib;
        const stdcpp_library_dir = std.fs.path.dirname(stdcpp_library) orelse
            return error.CannotResolveStdcppLibraryDir;
        const glibc_include_dir = glibc_include orelse
            return error.MissingOptionGlibcInclude;

        var it = std.mem.splitScalar(u8, stdcpp_include_dir, ':');
        while (it.next()) |p| {
            if (p.len > 0) {
                exe.root_module.addSystemIncludePath(b.graph.cwdRelativePath(p));
            }
        }

        exe.root_module.addSystemIncludePath(b.graph.cwdRelativePath(glibc_include_dir));
        exe.root_module.addObjectFile(b.graph.cwdRelativePath(stdcpp_library));

        exe.root_module.addLibraryPath(b.graph.cwdRelativePath(stdcpp_library_dir));
        exe.root_module.linkSystemLibrary("gcc_s", .{});

        exe.root_module.linkSystemLibrary("m", .{});
        exe.root_module.linkSystemLibrary("pthread", .{});
        exe.root_module.linkSystemLibrary("dl", .{});
    }

    const flags = if (llvm_dynamic)
        COMPILE_FLAGS ++ DYNAMIC_FLAGS
    else
        COMPILE_FLAGS;

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
        .flags = flags,
    });
    return exe;
}
