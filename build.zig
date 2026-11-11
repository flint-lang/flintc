const std = @import("std");

const OSTag = enum { linux, windows };
const BuildTarget = enum { linux_debug, linux_release, windows_debug, windows_release };

const flint_parser = @import("build/flint-parser.zig");
const llvm = @import("build/llvm.zig");
const flintc = @import("build/flintc.zig");
const fls = @import("build/fls.zig");
const fip = @import("build/fip.zig");

pub fn build(b: *std.Build) !void {
    const optimize = b.standardOptimizeOption(.{});

    // Source options for nix
    const o_fip_source = b.option(
        []const u8,
        "fip-source",
        "Path to FIP source directory",
    );
    const o_tomlc17_source = b.option(
        []const u8,
        "tomlc17-source",
        "Path to tomlc17 source directory",
    );

    // LLVM Options
    const o_llvm_jobs = b.option(usize, "llvm-jobs", "Number of cores to use for building LLVM") orelse
        (try std.Thread.getCpuCount() - 2);
    const o_llvm_rebuild = b.option(bool, "llvm-rebuild", "Force rebuild LLVM") orelse
        false;
    const o_llvm_dynamic = b.option(bool, "llvm-dynamic", "Whether to link dynamically with LLVM. This option only exists for NixOS") orelse
        false;
    const o_stdcpp_include = b.option([]const u8, "stdcpp-include", "Colon-separated GCC/libstdc++ include paths");
    const o_stdcpp_lib = b.option([]const u8, "stdcpp-lib", "Absolute path to the libstdc++ shared library");
    const o_glibc_include = b.option([]const u8, "glibc-include", "Path to glibc headers for dynamic builds");
    const o_glibc_lib = b.option([]const u8, "glibc-lib", "Path to glibc libraries for dynamic builds");
    const o_musl_lib = b.option([]const u8, "musl-lib", "Path to musl libraries for dynamic builds");
    const o_llvm_include = b.option([]const u8, "llvm-include", "Path to LLVM headers for dynamic builds");
    const o_llvm_lib = b.option([]const u8, "llvm-lib", "Path to LLVM libraries for dynamic builds");
    const o_lld_include = b.option([]const u8, "lld-include", "Path to LLD headers for dynamic builds");
    const o_lld_lib = b.option([]const u8, "lld-lib", "Path to LLD libraries for dynamic builds");
    const o_zlib_lib = b.option([]const u8, "zlib-lib", "Path to zlib libraries for dynamic builds");

    const commit_hash = b.option([]const u8, "commit-hash", "Flint Git revision") orelse blk: {
        const git_exe_path: []const u8 = b.findProgram(.{ .names = &.{"git"} }) orelse
            break :blk "unknown";
        break :blk std.mem.trim(
            u8,
            b.run(&.{ git_exe_path, "rev-parse", "--short", "HEAD" }),
            &std.ascii.whitespace,
        );
    };
    std.debug.print("-- Commit Hash is {q}\n", .{commit_hash});

    const commit_date = b.option([]const u8, "commit-date", "Date of last commit") orelse blk: {
        const git_exe_path: []const u8 = b.findProgram(.{ .names = &.{"git"} }) orelse
            break :blk "unknown";
        break :blk std.mem.trim(
            u8,
            b.run(&[_][]const u8{ git_exe_path, "log", "-1", "--date=format:%Y-%m-%d", "--format=%cd" }),
            &std.ascii.whitespace,
        );
    };
    std.debug.print("-- Commit Date is {s}\n", .{commit_date});

    const o_target: OSTag = b.option(OSTag, "target", "The OS to build for") orelse
        switch (b.graph.host.result.os.tag) {
            .linux => .linux,
            .windows => .windows,
            else => @panic("Unsupported OS"),
        };
    const target = resolveTarget(b, .{ .os = o_target }, o_llvm_dynamic);
    const o_all = b.option(bool, "all", "Build all targets") orelse
        false;
    const only_build_flint_parser = b.option(bool, "only-build-flint-parser", "Build only the flintc-parser library") orelse
        false;
    if (only_build_flint_parser) {
        const fip_lib = try fip.build(b, target, optimize, o_fip_source, o_tomlc17_source);
        _ = try flint_parser.build(
            b,
            target,
            optimize,
            try makeEmptyStep(b),
            fip_lib,
            false,
            o_llvm_dynamic,
            o_stdcpp_include,
            o_glibc_include,
        );
        return;
    }

    var targets_to_build: std.EnumArray(BuildTarget, bool) = .initFill(o_all);
    var single_build: ?struct {
        t: BuildTarget,
        s: *std.Build.Step.Compile,
    } = null;
    if (!o_all) {
        var target_int: u2 = switch (o_target) {
            .linux => 0,
            .windows => 2,
        };
        if (optimize != .debug) {
            target_int += 1;
        }
        targets_to_build.set(@fromBackingInt(target_int), true);
        single_build = .{ .t = @fromBackingInt(target_int), .s = undefined };
    }

    var last_step: *std.Build.Step = try makeEmptyStep(b);
    var targets_it = targets_to_build.iterator();
    while (targets_it.next()) |kvp| {
        if (!kvp.value.*) {
            continue;
        }
        const opt = resolveOptimize(kvp.key, optimize);
        const tar = resolveTarget(b, .{ .target = kvp.key }, o_llvm_dynamic);

        const platform_name: []const u8 = switch (tar.result.os.tag) {
            .linux => "linux",
            .windows => "mingw",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        };
        const llvm_platform_dirname: []const u8 = b.fmt("llvm-{s}", .{platform_name});
        const install_dir: []const u8 = b.fmt("vendor/{s}", .{llvm_platform_dirname});

        const fip_lib = try fip.build(b, tar, opt, o_fip_source, o_tomlc17_source);
        const flint_parser_lib_flintc = try flint_parser.build(b, tar, opt, last_step, fip_lib, false, o_llvm_dynamic, o_stdcpp_include, o_glibc_include);
        const flint_parser_lib_fls = try flint_parser.build(b, tar, opt, last_step, fip_lib, true, o_llvm_dynamic, o_stdcpp_include, o_glibc_include);
        const llvm_step = try llvm.build(b, &flint_parser_lib_flintc.step, tar, o_llvm_rebuild, o_llvm_jobs, platform_name, llvm_platform_dirname, install_dir, o_llvm_dynamic);
        const fls_exe = try fls.build(b, tar, opt, flint_parser_lib_fls, commit_hash, commit_date, o_llvm_dynamic, o_stdcpp_include, o_stdcpp_lib, o_glibc_include);
        const fls_install_step = &b.addInstallArtifact(fls_exe, .{}).step;
        b.getInstallStep().dependOn(fls_install_step);
        const flintc_exe = try flintc.build(
            b,
            tar,
            opt,
            flint_parser_lib_flintc,
            llvm_step,
            commit_hash,
            commit_date,
            o_llvm_dynamic,
            o_stdcpp_include,
            o_stdcpp_lib,
            o_glibc_include,
            o_glibc_lib,
            o_musl_lib,
            o_llvm_include,
            o_llvm_lib,
            o_lld_include,
            o_lld_lib,
            o_zlib_lib,
        );
        const flintc_install_step = &b.addInstallArtifact(flintc_exe, .{}).step;
        b.getInstallStep().dependOn(flintc_install_step);
        flintc_install_step.dependOn(fls_install_step);
        if (single_build) |*s| {
            s.s = flintc_exe;
        }
        last_step = flintc_install_step;
    }

    // Testing
    const test_step = b.step("test", "Test the app");
    if (optimize == .debug) {
        std.log.info("The 'test' build option requires a release build!", .{});
    } else if (single_build) |s| {
        const test_cmd = b.addRunArtifact(s.s);
        test_cmd.addFileArg(b.path("tests/tests.ft"));
        test_cmd.addArgs(&[_][]const u8{ "--test", "--run", "--rebuild-core" });
        // test_cmd.addDirectoryArg2(.{ .relative = .{ .base = .install_bin } }, .{});
        test_cmd.setCwd(b.path("tests"));
        test_cmd.has_side_effects = true;
        test_cmd.step.dependOn(last_step);
        test_step.dependOn(&test_cmd.step);
    }
}

/// Create a no-op step that meets the return type requirements
pub fn makeEmptyStep(b: *std.Build) !*std.Build.Step {
    const run_step = std.Build.Step.Run.create(b, "make_empty_step");
    run_step.addArgs(&.{ b.graph.zig_exe, "version" });
    _ = run_step.captureStdOut(.{});
    return &run_step.step;
}

fn resolveTarget(b: *std.Build, t: union(enum) { os: OSTag, target: BuildTarget }, llvm_dynamic: bool) std.Build.ResolvedTarget {
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
            .abi = if (llvm_dynamic) .gnu else .musl,
            .glibc_version = if (llvm_dynamic) .{
                .major = 2,
                .minor = 42,
                .patch = 0,
            } else null,
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
        .linux_debug, .windows_debug => .debug,
        .linux_release, .windows_release => if (release_mode != .debug) release_mode else .small,
    };
}

pub const dynamic_flags = &[_][]const u8{
    "-nostdinc++",
    "-nostdlib++",
    "-nostdinc",
    "-nobuiltininc",
};

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
