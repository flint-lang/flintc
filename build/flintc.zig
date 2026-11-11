const std = @import("std");

const llvm = @import("llvm.zig");
const flint_parser = @import("flint-parser.zig");

const FLINTC_VERSION = @import("../build.zig.zon").version;
const COMPILE_FLAGS = @import("../build.zig").compile_flags;
const DYNAMIC_FLAGS = @import("../build.zig").dynamic_flags;

pub fn build(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    flint_parser_lib: *std.Build.Step.Compile,
    previous_step: *std.Build.Step,
    commit_hash: []const u8,
    build_date: []const u8,
    llvm_dynamic: bool,
    stdcpp_include: ?[]const u8,
    stdcpp_lib: ?[]const u8,
    glibc_include: ?[]const u8,
    glibc_lib: ?[]const u8,
    musl_lib: ?[]const u8,
    llvm_include: ?[]const u8,
    llvm_lib: ?[]const u8,
    lld_include: ?[]const u8,
    lld_lib: ?[]const u8,
    zlib_lib: ?[]const u8,
) !*std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = if (optimize == .debug) "flintc-debug" else "flintc",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = !llvm_dynamic,
            .pic = true,
            .strip = optimize != .debug,
        }),
    });
    exe.step.dependOn(previous_step);
    exe.root_module.linkLibrary(flint_parser_lib);
    exe.link_function_sections = true;
    exe.link_data_sections = true;
    exe.link_gc_sections = true;
    exe.compress_debug_sections = .zlib;
    exe.build_id = .fast;

    // Add Macros
    exe.root_module.addCMacro("VERSION", b.fmt("{q}", .{FLINTC_VERSION}));
    exe.root_module.addCMacro("COMMIT_HASH", b.fmt("{q}", .{commit_hash}));
    exe.root_module.addCMacro("BUILD_DATE", b.fmt("{q}", .{build_date}));
    if (optimize == .debug) {
        exe.root_module.addCMacro("DEBUG_BUILD", "");
    }

    exe.root_module.addIncludePath(b.path("tests"));
    exe.root_module.addIncludePath(b.path("include"));
    if (llvm_dynamic) {
        const stdcpp_include_dir = stdcpp_include orelse
            return error.MissingOptionStdcppInclude;
        const stdcpp_library = stdcpp_lib orelse
            return error.MissingOptionStdcppLib;
        const stdcpp_library_dir = std.fs.path.dirname(stdcpp_library) orelse
            return error.CannotResolveStdcppLibraryDir;
        const glibc_include_dir = glibc_include orelse
            return error.MissingOptionGlibcInclude;
        const glibc_lib_dir = glibc_lib orelse
            return error.MissingOptionGlibcLib;
        const musl_lib_dir = musl_lib orelse
            return error.MissingOptionMuslLib;
        const llvm_include_dir = llvm_include orelse
            return error.MissingOptionLLVMInclude;
        const llvm_lib_dir = llvm_lib orelse
            return error.MissingOptionLLVMLib;
        const lld_include_dir = lld_include orelse
            return error.MissingOptionLLDInclude;
        const lld_lib_dir = lld_lib orelse
            return error.MissingOptionLLDLib;
        const zlib_lib_dir = zlib_lib orelse
            return error.MissingOptionZlibLib;

        var it = std.mem.splitScalar(u8, stdcpp_include_dir, ':');
        while (it.next()) |p| {
            if (p.len > 0) {
                exe.root_module.addSystemIncludePath(b.graph.cwdRelativePath(p));
            }
        }

        exe.root_module.addSystemIncludePath(b.graph.cwdRelativePath(glibc_include_dir));
        exe.root_module.addObjectFile(b.graph.cwdRelativePath(stdcpp_library));

        exe.root_module.addCMacro("NIX_GLIBC_LIB_PATH", b.fmt("{q}", .{glibc_lib_dir}));
        exe.root_module.addCMacro("NIX_DYNAMIC_LINKER", b.fmt("\"{s}/ld-linux-x86-64.so.2\"", .{glibc_lib_dir}));
        exe.root_module.addCMacro("NIX_MUSL_LIB_PATH", b.fmt("{q}", .{musl_lib_dir}));

        exe.root_module.addLibraryPath(b.graph.cwdRelativePath(stdcpp_library_dir));
        exe.root_module.linkSystemLibrary("gcc_s", .{});

        exe.root_module.addLibraryPath(b.graph.cwdRelativePath(zlib_lib_dir));
        exe.root_module.linkSystemLibrary("z", .{});

        exe.root_module.addSystemIncludePath(b.graph.cwdRelativePath(llvm_include_dir));
        exe.root_module.addSystemIncludePath(b.graph.cwdRelativePath(lld_include_dir));
        exe.root_module.addLibraryPath(b.graph.cwdRelativePath(llvm_lib_dir));
        exe.root_module.addLibraryPath(b.graph.cwdRelativePath(lld_lib_dir));

        exe.root_module.linkSystemLibrary("LLVM", .{});
        exe.root_module.linkSystemLibrary("lldCommon", .{});
        exe.root_module.linkSystemLibrary("lldELF", .{});
        exe.root_module.linkSystemLibrary("lldCOFF", .{});
        exe.root_module.linkSystemLibrary("lldMinGW", .{});

        exe.root_module.linkSystemLibrary("m", .{});
        exe.root_module.linkSystemLibrary("pthread", .{});
        exe.root_module.linkSystemLibrary("dl", .{});
    } else {
        const llvm_dir = switch (target.result.os.tag) {
            .linux => "vendor/llvm-linux",
            .windows => "vendor/llvm-mingw",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        };

        exe.root_module.addSystemIncludePath(b.path(b.fmt("{s}/include", .{llvm_dir})));
        exe.root_module.addLibraryPath(b.path(b.fmt("{s}/lib", .{llvm_dir})));

        try llvm.link(exe);
    }

    // Collect C++ files
    var src_dir: std.Io.Dir = try b.root.openDir(b.graph.io, "src", .{ .iterate = true });
    defer src_dir.close(b.graph.io);
    var walker = try src_dir.walk(b.allocator);
    defer walker.deinit();
    const flags = if (llvm_dynamic)
        COMPILE_FLAGS ++ DYNAMIC_FLAGS
    else
        COMPILE_FLAGS;
    while (try walker.next(b.graph.io)) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.basename, ".cpp") and
            (std.mem.containsAtLeast(u8, entry.path, 1, "generator") or
                std.mem.eql(u8, entry.basename, "main.cpp") or
                std.mem.eql(u8, entry.basename, "linker.cpp") or
                std.mem.eql(u8, entry.basename, "fip.cpp")))
        {
            exe.root_module.addCSourceFile(.{
                .file = b.path(b.pathJoin(&.{ "src", entry.path })),
                .flags = flags,
            });
        }
    }

    // Library linking
    if (target.result.os.tag == .windows) {
        exe.root_module.linkSystemLibrary("ole32", .{});
    }
    return exe;
}
