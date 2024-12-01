const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{ .default_target = .{ .cpu_arch = .x86_64 } });
    const optimize = b.standardOptimizeOption(.{});

    // Options
    //const tests = b.option(bool, "Test", "Runs all builtin tests (default: false)") orelse false;

    const exe = b.addExecutable(.{
        .name = "flintc",
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibCpp();
    exe.addIncludePath(b.path("src"));

    const source_files = [_][]const u8{
        "src/main.cpp", "src/error/error.cpp", "src/lexer/lexer.cpp", "src/linker/linker.cpp", "src/parser/parser.cpp", "src/parser/signature.cpp",
    };
    exe.addCSourceFiles(.{ .files = &source_files });

    b.installArtifact(exe);

    // This *creates* a Run step in the build graph, to be executed when another
    // step is evaluated that depends on it. The next line below will establish
    // such a dependency.
    const run_cmd = b.addRunArtifact(exe);

    // By making the run step depend on the install step, it will be run from the
    // installation directory rather than directly from within the cache directory.
    // This is not necessary, however, if the application depends on other installed
    // files, this ensures they will be present and in the expected location.
    run_cmd.step.dependOn(b.getInstallStep());

    // This allows the user to pass arguments to the application in the build
    // command itself, like this: `zig build run -- arg1 arg2 etc`
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    // This creates a build step. It will be visible in the `zig build --help` menu,
    // and can be selected like this: `zig build run`
    // This will evaluate the `run` step rather than the default, which is "install".
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
