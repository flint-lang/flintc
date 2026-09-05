const std = @import("std");

const OSTag = enum { linux, windows };
const BuildTarget = enum { linux_debug, linux_release, windows_debug, windows_release };

const flint_parser = @import("build/flint-parser.zig");
const llvm = @import("build/llvm.zig");
const flintc = @import("build/flintc.zig");
const fls = @import("build/fls.zig");

pub fn build(b: *std.Build) !void {
    const host_target = b.resolveTargetQuery(.{});
    const optimize = b.standardOptimizeOption(.{});

    const o_target: OSTag = b.option(OSTag, "target", "The OS to build for") orelse
        switch (host_target.result.os.tag) {
            .linux => .linux,
            .windows => .windows,
            else => @panic("Unsupported OS"),
        };
    const target = resolveTarget(b, .{ .os = o_target });
    const o_all = b.option(bool, "all", "Build all targets") orelse
        false;
    const only_build_flint_parser = b.option(bool, "only-build-flint-parser", "Build only the flintc-parser library") orelse
        false;
    if (only_build_flint_parser) {
        _ = try flint_parser.build(b, target, optimize, try makeEmptyStep(b), only_build_flint_parser, .master);
        return;
    }

    const o_git_hash = b.option([]const u8, "git-hash", "Git hash of the project needed for nix-build.");
    const commit_hash: []const u8 = blk: {
        const hash = if (o_git_hash) |hash| hash else std.mem.trim(
            u8,
            b.run(&[_][]const u8{ "git", "rev-parse", "--short", "HEAD" }),
            &std.ascii.whitespace,
        );
        break :blk b.fmt("\"{s}\"", .{hash});
    };
    std.debug.print("-- Commit Hash is {s}\n", .{commit_hash});

    const build_date: []const u8 = blk: {
        const current_timestamp: u64 = @intCast(std.Io.Timestamp.now(b.graph.io, .real).toSeconds());
        const epoch_seconds: std.time.epoch.EpochSeconds = .{ .secs = current_timestamp };
        const epoch_day = epoch_seconds.getEpochDay();
        const year_day = epoch_day.calculateYearDay();
        const month_day = year_day.calculateMonthDay();
        break :blk b.fmt("\"{d}-{d:0>2}-{d:0>2}\"", .{
            year_day.year,
            month_day.month.numeric(),
            month_day.day_index + 1, // day_index is 0-based
        });
    };
    std.debug.print("-- Build Date is {s}\n", .{build_date});

    // LLVM Options
    const o_llvm_jobs = b.option(usize, "llvm-jobs", "Number of cores to use for building LLVM") orelse
        (try std.Thread.getCpuCount() - 2);
    const o_llvm_prebuilt_dir = b.option([]const u8, "llvm-prebuilt-dir", "Path to prebuilt LLVM installation.");
    const o_llvm_version = b.option([]const u8, "llvm-version", b.fmt("LLVM version to use. Default: {s}", .{llvm.DEFAULT_LLVM_VERSION})) orelse
        llvm.DEFAULT_LLVM_VERSION;
    const o_llvm_rebuild = b.option(bool, "llvm-rebuild", "Force rebuild LLVM") orelse
        false;

    var targets_to_build: std.EnumArray(BuildTarget, bool) = .initFill(o_all);
    var single_build: ?struct {
        t: BuildTarget,
        s: *std.Build.Step.Compile,
    } = null;
    if (!o_all) {
        var target_int: u4 = switch (o_target) {
            .linux => 0,
            .windows => 2,
        };
        if (optimize != .Debug) {
            target_int += 1;
        }
        targets_to_build.set(@enumFromInt(target_int), true);
        single_build = .{ .t = @enumFromInt(target_int), .s = undefined };
    }

    var last_step: *std.Build.Step =
        if (o_llvm_prebuilt_dir == null)
            try llvm.update(b, o_llvm_version)
        else
            try makeEmptyStep(b);
    var targets_it = targets_to_build.iterator();
    while (targets_it.next()) |kvp| {
        if (!kvp.value.*) {
            continue;
        }
        const opt = resolveOptimize(kvp.key, optimize);
        const tar = resolveTarget(b, .{ .target = kvp.key });

        const flint_parser_lib = try flint_parser.build(b, tar, opt, last_step, false, .master);
        const llvm_step = try llvm.build(b, tar, &flint_parser_lib.step, o_llvm_prebuilt_dir, o_llvm_rebuild, o_llvm_jobs);
        const flintc_exe = try flintc.build(b, tar, opt, flint_parser_lib, llvm_step, o_llvm_prebuilt_dir, commit_hash, build_date);
        try fls.build(b, tar, opt, flint_parser_lib, commit_hash, build_date);
        if (single_build) |*s| {
            s.s = flintc_exe;
        }
        last_step = &flintc_exe.step;
    }

    // Testing
    const test_step = b.step("test", "Test the app");
    if (optimize == .Debug) {
        std.log.info("The 'test' build option requires a release build!", .{});
    } else if (single_build) |s| {
        const test_cmd = b.addRunArtifact(s.s);
        test_cmd.addFileArg(b.path("examples/tests.ft"));
        test_cmd.addArgs(&[_][]const u8{ "--test", "--run" });
        test_cmd.addPathDir(b.getInstallPath(.bin, ""));
        test_cmd.setCwd(b.path("examples"));
        test_cmd.has_side_effects = true;
        test_cmd.step.dependOn(last_step);
        test_step.dependOn(&test_cmd.step);
    }
}

/// Create a no-op Run step that meets the return type requirements
pub fn makeEmptyStep(b: *std.Build) !*std.Build.Step {
    const run_step = b.addSystemCommand(&[_][]const u8{ "zig", "version" });
    run_step.setName("make_empty_step");
    _ = run_step.captureStdOut(.{});
    return &run_step.step;
}

fn resolveTarget(b: *std.Build, t: union(enum) { os: OSTag, target: BuildTarget }) std.Build.ResolvedTarget {
    const os_select: OSTag = switch (t) {
        .os => |os| os,
        .target => |target| switch (target) {
            .linux_debug, .linux_release => .linux,
            .windows_debug, .windows_release => .windows,
        },
    };
    return switch (os_select) {
        .linux => b.resolveTargetQuery(.{
            .cpu_model = .baseline,
            .cpu_arch = .x86_64,
            .os_tag = .linux,
            .abi = .musl,
        }),
        .windows => b.resolveTargetQuery(.{
            .cpu_model = .baseline,
            .cpu_arch = .x86_64,
            .os_tag = .windows,
            .abi = .gnu,
        }),
    };
}

fn resolveOptimize(t: BuildTarget, release_mode: std.builtin.OptimizeMode) std.builtin.OptimizeMode {
    return switch (t) {
        .linux_debug, .windows_debug => .Debug,
        .linux_release, .windows_release => if (release_mode != .Debug) release_mode else .ReleaseSmall,
    };
}

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
