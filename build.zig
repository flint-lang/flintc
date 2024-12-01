const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
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
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
