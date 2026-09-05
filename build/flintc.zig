const std = @import("std");

const llvm = @import("llvm.zig");
const flint_parser = @import("flint-parser.zig");

const FLINTC_VERSION = @import("../build.zig.zon").version;
const COMPILE_FLAGS = @import("../build.zig").compile_flags;

pub fn build(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    flint_parser_lib: *std.Build.Step.Compile,
    previous_step: *std.Build.Step,
    llvm_prebuilt_dir: ?[]const u8,
    commit_hash: []const u8,
    build_date: []const u8,
) !*std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = if (optimize == .Debug) "flintc-debug" else "flintc",
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
    exe.root_module.addCMacro("VERSION", b.fmt("\"{s}\"", .{FLINTC_VERSION}));
    exe.root_module.addCMacro("COMMIT_HASH", commit_hash);
    exe.root_module.addCMacro("BUILD_DATE", build_date);
    if (optimize == .Debug) {
        exe.root_module.addCMacro("DEBUG_BUILD", "");
    }

    const llvm_dir = if (llvm_prebuilt_dir) |dir| dir else switch (target.result.os.tag) {
        .linux => "vendor/llvm-linux",
        .windows => "vendor/llvm-mingw",
        else => return error.TargetNeedsToBeLinuxOrWindows,
    };

    // Add Include paths
    exe.root_module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{llvm_dir}) });
    exe.root_module.addIncludePath(b.path("tests"));
    exe.root_module.addIncludePath(b.path("include"));

    // Add Library paths
    exe.root_module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{llvm_dir}) });

    // Collect C++ files
    var src_dir: std.Io.Dir = try std.Io.Dir.cwd().openDir(b.graph.io, "src", .{ .iterate = true });
    defer src_dir.close(b.graph.io);
    var walker = try src_dir.walk(b.allocator);
    defer walker.deinit();
    while (try walker.next(b.graph.io)) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.basename, ".cpp") and
            (std.mem.containsAtLeast(u8, entry.path, 1, "generator") or
                std.mem.eql(u8, entry.basename, "main.cpp") or
                std.mem.eql(u8, entry.basename, "linker.cpp") or
                std.mem.eql(u8, entry.basename, "fip.cpp")))
        {
            exe.root_module.addCSourceFile(.{
                .file = b.path(b.pathJoin(&.{ "src", entry.path })),
                .flags = COMPILE_FLAGS,
            });
        }
    }

    // Library linking
    if (target.result.os.tag == .windows) {
        exe.root_module.linkSystemLibrary("ole32", .{});
    }
    exe.root_module.linkSystemLibrary("lldMinGW", .{});
    exe.root_module.linkSystemLibrary("lldCOFF", .{});
    exe.root_module.linkSystemLibrary("lldELF", .{});
    exe.root_module.linkSystemLibrary("lldCommon", .{});

    // Link LLVM libraries
    try llvm.link(b, previous_step, exe);
    return exe;
}
