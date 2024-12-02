const std = @import("std");

pub fn build(b: *std.Build) void {
    const target_linux = b.resolveTargetQuery(.{ .cpu_arch = .x86_64, .os_tag = .linux });
    const target_windows = b.resolveTargetQuery(.{ .cpu_arch = .x86_64, .os_tag = .windows });

    const optimize = b.standardOptimizeOption(.{});
    // Options
    //const tests = b.option(bool, "Test", "Runs all builtin tests (default: false)") orelse false;

    const source_files = [_][]const u8{
        "src/error/error.cpp", "src/lexer/lexer.cpp", "src/linker/linker.cpp", "src/parser/parser.cpp", "src/parser/signature.cpp",
    };

    // --- LINUX ---
    const flintc_linux = b.addExecutable(.{
        .name = "flintc-linux",
        .target = target_linux,
        .optimize = optimize,
    });
    flintc_linux.linkLibCpp();
    flintc_linux.addIncludePath(b.path("src"));
    flintc_linux.addCSourceFile(.{ .file = .{ .cwd_relative = "src/main.cpp" } });
    flintc_linux.addCSourceFiles(.{ .files = &source_files });
    b.installArtifact(flintc_linux);

    // Run the flint-linux file
    const run_linux = b.addRunArtifact(flintc_linux);
    run_linux.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_linux.addArgs(args);
    }
    const run_step_linux = b.step("run-linux", "Run for linux");
    run_step_linux.dependOn(&run_linux.step);

    // --- WINDOWS ---
    const flintc_windows = b.addExecutable(.{
        .name = "flintc-windows",
        .target = target_windows,
        .optimize = optimize,
        .strip = true, // remove .pdb output file
    });
    flintc_windows.linkLibCpp();
    flintc_windows.addIncludePath(b.path("src"));
    flintc_windows.addCSourceFile(.{ .file = .{ .cwd_relative = "src/main.cpp" } });
    flintc_windows.addCSourceFiles(.{ .files = &source_files });
    b.installArtifact(flintc_windows);

    // Run the flint-windows file
    const run_windows = b.addRunArtifact(flintc_windows);
    run_windows.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_windows.addArgs(args);
    }
    const run_step_windows = b.step("run-windows", "Run for windows");
    run_step_windows.dependOn(&run_windows.step);

    // --- UNIT TESTS ---
    const unit_tests = b.addTest(.{
        .name = "test",
        .root_source_file = .{ .cwd_relative = "src/tests.zig" },
        .target = target_linux,
        .optimize = optimize,
    });
    unit_tests.linkLibCpp();
    unit_tests.addIncludePath(b.path("src"));
    unit_tests.addCSourceFiles(.{ .files = &source_files });
    unit_tests.addCSourceFile(.{ .file = .{ .cwd_relative = "src/tests.cpp" } });
    const run_unit_tests = b.addRunArtifact(unit_tests);
    const step_unit_tests = b.step("test", "Run unit tests");
    step_unit_tests.dependOn(&run_unit_tests.step);

    run_unit_tests.has_side_effects = true;
}
