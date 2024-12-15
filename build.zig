const std = @import("std");

pub fn build(b: *std.Build) void {
    const optimization = b.standardOptimizeOption(.{});
    // Options
    //const tests = b.option(bool, "Test", "Runs all builtin tests (default: false)") orelse false;

    const source_files = [_][]const u8{
        "src/error/error.cpp", // error
        "src/lexer/lexer.cpp", // lexer
        "src/linker/linker.cpp", //linker
        "src/parser/parser.cpp", //parser
        "src/parser/signature.cpp", // signature
        "src/debug.cpp", // debug
        "src/generator/generator.cpp", // generator
    };
    const compile_flags = [_][]const u8{
        "-g", // generate debug information like stack traces
        "-O0", // disable optimization for easier debugging
        "-fno-omit-frame-pointer", // ensures that the frame pointer is reserved for accurate stack traces
        "-DEBUG", // enables assertions and other debugging-related macros
        "-std=c++17", // Use C++ standard 17
        // LLVM Flags
        "-I/nix/store/3yn73cwkl9sqmcw41jkmzv33bq7qldkp-llvm-19.1.4-dev/include",
        //"-fno-exceptions",
        "-funwind-tables",
        "-D_GNU_SOURCE",
        "-D__STDC_CONSTANT_MACROS",
        "-D__STDC_FORMAT_MACROS",
        "-D__STDC_LIMIT_MACROS",
    };

    // --- LINUX ---
    add_linux_executable(b, optimization, &source_files, &compile_flags);
    // --- WINDOWS ---
    add_windows_executable(b, optimization, &source_files, &compile_flags);
    // --- MACOS ---
    add_macos_executable(b, optimization, &source_files, &compile_flags);
    // --- UNIT TESTS ---
    add_unit_tests(b, optimization, &source_files, &compile_flags);
}

pub fn add_linux_executable(b: *std.Build, optimization: std.builtin.OptimizeMode, source_files: []const []const u8, flags: []const []const u8) void {
    const target_linux = b.resolveTargetQuery(.{ .cpu_arch = .x86_64, .os_tag = .linux });
    const flintc_linux = b.addExecutable(.{ .name = "flintc-linux", .target = target_linux, .optimize = optimization });
    flintc_linux.linkLibCpp();
    flintc_linux.addIncludePath(b.path("src"));
    flintc_linux.addCSourceFile(.{ .file = .{ .cwd_relative = "src/main.cpp" }, .flags = flags });
    flintc_linux.addCSourceFiles(.{ .files = source_files, .flags = flags });
    b.installArtifact(flintc_linux);

    // Run the flint-linux file
    const run_linux = b.addRunArtifact(flintc_linux);
    run_linux.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_linux.addArgs(args);
    }
    const run_step_linux = b.step("run-linux", "Run for linux");
    run_step_linux.dependOn(&run_linux.step);
}

pub fn add_macos_executable(b: *std.Build, optimization: std.builtin.OptimizeMode, source_files: []const []const u8, flags: []const []const u8) void {
    const target_mac = b.resolveTargetQuery(.{ .cpu_arch = .aarch64, .os_tag = .macos });
    const flintc_mac = b.addExecutable(.{ .name = "flintc-mac", .target = target_mac, .optimize = optimization });
    flintc_mac.linkLibCpp();
    flintc_mac.addIncludePath(b.path("src"));
    flintc_mac.addCSourceFile(.{ .file = .{ .cwd_relative = "src/main.cpp" }, .flags = flags });
    flintc_mac.addCSourceFiles(.{ .files = source_files, .flags = flags });
    b.installArtifact(flintc_mac);

    // Run the flint-linux file
    const run_mac = b.addRunArtifact(flintc_mac);
    run_mac.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_mac.addArgs(args);
    }
    const run_step_mac = b.step("run-mac", "Run for MacOS");
    run_step_mac.dependOn(&run_mac.step);
}

pub fn add_windows_executable(b: *std.Build, optimization: std.builtin.OptimizeMode, source_files: []const []const u8, flags: []const []const u8) void {
    const target_windows = b.resolveTargetQuery(.{ .cpu_arch = .x86_64, .os_tag = .windows });
    const flintc_windows = b.addExecutable(.{
        .name = "flintc-windows",
        .target = target_windows,
        .optimize = optimization,
        .strip = true, // remove .pdb output file
    });
    flintc_windows.linkLibCpp();
    flintc_windows.addIncludePath(b.path("src"));
    flintc_windows.addCSourceFile(.{ .file = .{ .cwd_relative = "src/main.cpp" }, .flags = flags });
    flintc_windows.addCSourceFiles(.{ .files = source_files, .flags = flags });
    b.installArtifact(flintc_windows);

    // Run the flint-windows file
    const run_windows = b.addRunArtifact(flintc_windows);
    run_windows.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_windows.addArgs(args);
    }
    const run_step_windows = b.step("run-windows", "Run for windows");
    run_step_windows.dependOn(&run_windows.step);
}

pub fn add_unit_tests(b: *std.Build, optimization: std.builtin.OptimizeMode, source_files: []const []const u8, flags: []const []const u8) void {
    const target_linux = b.resolveTargetQuery(.{ .cpu_arch = .x86_64, .os_tag = .linux });
    const unit_tests = b.addTest(.{
        .name = "test",
        .root_source_file = .{ .cwd_relative = "src/tests.zig" },
        .target = target_linux,
        .optimize = optimization,
    });
    unit_tests.linkLibCpp();
    unit_tests.addIncludePath(b.path("src"));
    unit_tests.addCSourceFiles(.{ .files = source_files, .flags = flags });
    unit_tests.addCSourceFile(.{ .file = .{ .cwd_relative = "src/tests.cpp" }, .flags = flags });
    b.installArtifact(unit_tests);

    const run_unit_tests = b.addRunArtifact(unit_tests);
    run_unit_tests.has_side_effects = true;

    const step_unit_tests = b.step("test", "Run unit tests");
    step_unit_tests.dependOn(&run_unit_tests.step);
}
