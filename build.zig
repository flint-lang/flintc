const std = @import("std");

const flint_parser = @import("build_flint-parser.zig");

pub const FLINTC_VERSION = @import("build.zig.zon").version;
const DEFAULT_LLVM_VERSION = "llvmorg-22.1.8";

pub fn build(b: *std.Build) !void {
    const OSTag = enum { linux, windows };
    _ = b.findProgram(&.{"ld.lld"}, &.{}) catch @panic("LLD not found on this system");
    _ = b.findProgram(&.{"cmake"}, &.{}) catch @panic("CMake not found on this system");
    _ = b.findProgram(&.{"ninja"}, &.{}) catch @panic("Ninja not found on this system");
    _ = b.findProgram(&.{"python"}, &.{}) catch @panic("Python3 not found on this system");

    const host_target = b.resolveTargetQuery(.{});
    const optimize = b.standardOptimizeOption(.{});

    const only_build_flint_parser = b.option(bool, "only-build-flint-parser", "Build only the flintc-parser library") orelse
        false;

    const external_llvm_dir = b.option([]const u8, "llvm-dir", "Path to external LLVM installation.");
    const external_llvm_prebuilt: bool = b.option(bool, "llvm-prebuilt", "Whether to use the 'llvm-dir' as a prebuilt llvm version, which skips building llvm.") orelse false;
    if (external_llvm_prebuilt and external_llvm_dir == null) {
        @panic("To use 'llvm-prebuilt' a 'llvm-dir' needs to be provided.");
    }
    const external_hash = b.option([]const u8, "git-hash", "Git hash of the project needed for nix-build.");

    if (external_llvm_dir == null) {
        // Git is only needed when at least one source is not provided externally and thus needs to be fetched
        _ = b.findProgram(&.{"git"}, &.{}) catch @panic("Git not found on this system");
    }

    const llvm_version = b.option([]const u8, "llvm-version", b.fmt("LLVM version to use. Default: {s}", .{DEFAULT_LLVM_VERSION})) orelse
        DEFAULT_LLVM_VERSION;
    const force_llvm_rebuild = b.option(bool, "llvm-rebuild", "Force rebuild LLVM") orelse
        false;
    const jobs = b.option(usize, "jobs", "Number of cores to use for building LLVM") orelse
        (try std.Thread.getCpuCount() - 2);
    const target_option: OSTag = b.option(OSTag, "target", "The OS to build for") orelse
        switch (host_target.result.os.tag) {
            .linux => .linux,
            .windows => .windows,
            else => @panic("Unsupported OS"),
        };

    const target = targets(b)[@intFromEnum(target_option)];

    const flint_parser_lib = try flint_parser.build_flint_parser_lib(b, target, optimize, .master);
    b.installArtifact(flint_parser_lib);

    if (only_build_flint_parser) {
        flint_parser_lib.installHeadersDirectory(b.path("include/analyzer"), "analyzer", .{ .include_extensions = &.{".hpp"} });
        flint_parser_lib.installHeadersDirectory(b.path("include/error"), "error", .{ .include_extensions = &.{".hpp"} });
        flint_parser_lib.installHeadersDirectory(b.path("include/lexer"), "lexer", .{ .include_extensions = &.{".hpp"} });
        flint_parser_lib.installHeadersDirectory(b.path("include/matcher"), "matcher", .{ .include_extensions = &.{".hpp"} });
        flint_parser_lib.installHeadersDirectory(b.path("include/parser"), "parser", .{ .include_extensions = &.{".hpp"} });
        flint_parser_lib.installHeadersDirectory(b.path("include/resolver"), "resolver", .{ .include_extensions = &.{".hpp"} });

        flint_parser_lib.installHeader(b.path("include/colors.hpp"), "colors.hpp");
        flint_parser_lib.installHeader(b.path("include/debug.hpp"), "debug.hpp");
        flint_parser_lib.installHeader(b.path("include/fip.hpp"), "fip.hpp");
        flint_parser_lib.installHeader(b.path("include/globals.hpp"), "globals.hpp");
        flint_parser_lib.installHeader(b.path("include/persistent_thread_pool.hpp"), "persistent_thread_pool.hpp");
        flint_parser_lib.installHeader(b.path("include/profiler.hpp"), "profiler.hpp");
        flint_parser_lib.installHeader(b.path("include/single_executor_guard.hpp"), "single_executor_guard.hpp");
        flint_parser_lib.installHeader(b.path("include/types.hpp"), "types.hpp");
        return;
    }

    const commit_hash: []const u8 = blk: {
        const hash = if (external_hash) |hash| hash else std.mem.trim(
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

    // Update LLVM if no external LLVM is passed
    const last_step = if (external_llvm_dir == null)
        try updateLLVM(b, llvm_version)
    else
        try makeEmptyStep(b);

    // Build LLVM if the extern LLVM dir is not prebuilt
    const llvm_step = if (external_llvm_prebuilt)
        try makeEmptyStep(b)
    else
        try buildLLVM(b, &last_step.step, target, force_llvm_rebuild, jobs, external_llvm_dir);
    const llvm_dir: ?[]const u8 = if (external_llvm_prebuilt) external_llvm_dir else null;

    // Build flintc exe
    const flintc_exe = try buildFlintc(b, &llvm_step.step, target, optimize, commit_hash, build_date, llvm_dir, flint_parser_lib);
    const flintc_exe_install = b.addInstallArtifact(flintc_exe, .{});
    b.getInstallStep().dependOn(&flintc_exe_install.step);
    // Build FLS exe
    const fls_exe = try buildFLS(b, target, optimize, commit_hash, build_date, flint_parser_lib);
    fls_exe.step.dependOn(&flintc_exe.step);
    const fls_exe_install = b.addInstallArtifact(fls_exe, .{});
    b.getInstallStep().dependOn(&fls_exe_install.step);

    // Build all
    const build_all_step = b.step("all", "Build all targets");
    var last_target_step: *std.Build.Step = &fls_exe.step;
    for (targets(b)) |t| {
        const build_llvm_step = try buildLLVM(b, &llvm_step.step, t, force_llvm_rebuild, jobs, external_llvm_dir);

        const flint_parser_lib_debug = try flint_parser.build_flint_parser_lib(b, t, .Debug, .master);
        const flint_parser_lib_release = try flint_parser.build_flint_parser_lib(b, t, .ReleaseSmall, .master);

        const flintc_exe_debug = try buildFlintc(b, &build_llvm_step.step, t, .Debug, commit_hash, build_date, llvm_dir, flint_parser_lib_debug);
        flintc_exe_debug.step.dependOn(last_target_step);
        build_all_step.dependOn(&b.addInstallArtifact(flintc_exe_debug, .{}).step);

        const fls_exe_debug = try buildFLS(b, t, .Debug, commit_hash, build_date, flint_parser_lib_debug);
        fls_exe_debug.step.dependOn(&flintc_exe_debug.step);
        build_all_step.dependOn(&b.addInstallArtifact(fls_exe_debug, .{}).step);

        const flintc_exe_release = try buildFlintc(b, &build_llvm_step.step, t, .ReleaseSmall, commit_hash, build_date, llvm_dir, flint_parser_lib_release);
        flintc_exe_release.step.dependOn(&fls_exe_debug.step);
        build_all_step.dependOn(&b.addInstallArtifact(flintc_exe_release, .{}).step);

        const fls_exe_release = try buildFLS(b, t, .ReleaseSmall, commit_hash, build_date, flint_parser_lib_release);
        fls_exe_release.step.dependOn(&flintc_exe_release.step);
        build_all_step.dependOn(&b.addInstallArtifact(fls_exe_release, .{}).step);
        last_target_step = &fls_exe_release.step;
    }

    // Testing
    const test_step = b.step("test", "Test the app");
    if (optimize == .Debug) {
        std.log.info("The 'test' build option requires a release build!", .{});
    } else {
        const test_cmd = b.addRunArtifact(flintc_exe);
        test_cmd.addFileArg(b.path("examples/tests.ft"));
        test_cmd.addArgs(&[_][]const u8{ "--test", "--run" });
        test_cmd.addPathDir(b.getInstallPath(.bin, ""));
        test_cmd.setCwd(b.path("examples"));
        test_cmd.has_side_effects = true;
        test_cmd.step.dependOn(&flintc_exe_install.step);
        test_cmd.step.dependOn(&fls_exe_install.step);
        test_step.dependOn(&test_cmd.step);
    }
}

fn buildFLS(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    commit_hash: []const u8,
    build_date: []const u8,
    flint_parser_lib: *std.Build.Step.Compile,
) !*std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = if (optimize == .Debug) "fls-debug" else "fls",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
            .pic = true,
        }),
        .version = try .parse(FLINTC_VERSION),
    });
    exe.link_function_sections = true;
    exe.link_data_sections = true;
    exe.link_gc_sections = true;
    exe.compress_debug_sections = .zlib;
    exe.build_id = .fast;

    // Add Macros
    exe.root_module.addCMacro("FLINT_LSP", "");
    exe.root_module.addCMacro("VERSION", b.fmt("\"{s}\"", .{FLINTC_VERSION}));
    exe.root_module.addCMacro("COMMIT_HASH", commit_hash);
    exe.root_module.addCMacro("BUILD_DATE", build_date);
    if (optimize == .Debug) {
        exe.root_module.addCMacro("DEBUG_BUILD", "");
    }

    // Add Include paths
    exe.root_module.addIncludePath(b.path("include"));
    exe.root_module.addIncludePath(b.path("fls/include"));

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
        .flags = flint_parser.compile_flags,
    });

    // Link the flint parser library
    exe.root_module.linkLibrary(flint_parser_lib);
    return exe;
}

fn buildFlintc(
    b: *std.Build,
    previous_step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    commit_hash: []const u8,
    build_date: []const u8,
    external_llvm_dir: ?[]const u8,
    flint_parser_lib: *std.Build.Step.Compile,
) !*std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = if (optimize == .Debug) "flintc-debug" else "flintc",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
            .pic = true,
        }),
        .version = try .parse(FLINTC_VERSION),
    });
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

    const llvm_dir = if (external_llvm_dir) |dir| dir else switch (target.result.os.tag) {
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
    var cpp_files: std.ArrayList([]const u8) = .empty;
    defer cpp_files.deinit(b.allocator);
    var walker = try src_dir.walk(b.allocator);
    defer walker.deinit();
    while (try walker.next(b.graph.io)) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.basename, ".cpp") and
            (std.mem.containsAtLeast(u8, entry.path, 1, "generator") or
                std.mem.eql(u8, entry.basename, "main.cpp") or
                std.mem.eql(u8, entry.basename, "linker.cpp") or
                std.mem.eql(u8, entry.basename, "fip.cpp")))
        {
            try cpp_files.append(b.allocator, try b.allocator.dupe(u8, entry.path));
        }
    }

    // Add C++ src files
    exe.root_module.addCSourceFiles(.{
        .root = b.path("src"),
        .files = cpp_files.items,
        .flags = flint_parser.compile_flags,
    });

    // Library linking
    exe.root_module.linkLibrary(flint_parser_lib);
    if (target.result.os.tag == .windows) {
        exe.root_module.linkSystemLibrary("ole32", .{});
    }
    exe.root_module.linkSystemLibrary("lldMinGW", .{});
    exe.root_module.linkSystemLibrary("lldCOFF", .{});
    exe.root_module.linkSystemLibrary("lldELF", .{});
    exe.root_module.linkSystemLibrary("lldCommon", .{});

    // Link LLVM libraries
    try linkWithLLVM(b, previous_step, exe);
    return exe;
}

fn buildLLVM(
    b: *std.Build,
    previous_step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    force_rebuild: bool,
    jobs: usize,
    external_llvm_dir: ?[]const u8,
) !*std.Build.Step.Run {
    const build_name: []const u8 = switch (target.result.os.tag) {
        .linux => "linux",
        .windows => "mingw",
        else => return error.TargetNeedsToBeLinuxOrWindows,
    };
    const llvm_build_dir = b.fmt(".zig-cache/llvm-{s}", .{build_name});
    const install_dir = b.fmt("vendor/llvm-{s}", .{build_name});
    const llvm_dir = if (external_llvm_dir) |dir| dir else "vendor/sources/llvm-project";

    if (std.Io.Dir.cwd().openDir(b.graph.io, install_dir, .{})) |_| {
        // LLVM is already built, rebuilt only if requested
        if (force_rebuild) {
            try std.Io.Dir.cwd().deleteTree(b.graph.io, install_dir);
        } else {
            return makeEmptyStep(b);
        }
    } else |_| {}
    if (force_rebuild) {
        try std.Io.Dir.cwd().deleteTree(b.graph.io, llvm_build_dir);
    }

    std.debug.print("-- Building LLVM for {s}\n", .{build_name});

    // Setup LLVM
    const setup_llvm = b.addSystemCommand(&[_][]const u8{
        "cmake",
        "-S",
        b.fmt("{s}/llvm", .{llvm_dir}),
        "-B",
        llvm_build_dir,
        "-G",
        "Ninja",
        b.fmt("-DCMAKE_INSTALL_PREFIX={s}", .{install_dir}),
        "-DCMAKE_BUILD_TYPE=MinSizeRel",
        b.fmt("-DCMAKE_C_COMPILER={s}", .{switch (target.result.os.tag) {
            .linux => "zig;cc;-target;x86_64-linux-musl",
            .windows => "zig;cc;-target;x86_64-windows-gnu",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        b.fmt("-DCMAKE_CXX_COMPILER={s}", .{switch (target.result.os.tag) {
            .linux => "zig;c++;-target;x86_64-linux-musl",
            .windows => "zig;c++;-target;x86_64-windows-gnu",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        b.fmt("-DCMAKE_ASM_COMPILER={s}", .{switch (target.result.os.tag) {
            .linux => "zig;cc;-target;x86_64-linux-musl",
            .windows => "zig;cc;-target;x86_64-windows-gnu",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        if (b.resolveTargetQuery(.{}).result.os.tag == target.result.os.tag) "" else switch (target.result.os.tag) {
            .linux => "-DCMAKE_SYSTEM_NAME=Linux",
            .windows => "-DCMAKE_SYSTEM_NAME=Windows",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        },
        "-DBUILD_SHARED_LIBS=OFF",

        "-DLLVM_TARGET_ARCH=X86",
        "-DLLVM_TARGETS_TO_BUILD=X86",

        "-DLLVM_ENABLE_PROJECTS=lld",
        "-DLLVM_ENABLE_ASSERTIONS=ON",
        "-DLLVM_ENABLE_CURL=OFF",
        "-DLLVM_ENABLE_HTTPLIB=OFF",
        "-DLLVM_ENABLE_FFI=OFF",
        "-DLLVM_ENABLE_LIBEDIT=OFF",
        "-DLLVM_ENABLE_LIBXML2=OFF",
        "-DLLVM_ENABLE_Z3_SOLVER=OFF",
        "-DLLVM_ENABLE_ZLIB=OFF",
        "-DLLVM_ENABLE_ZSTD=OFF",

        "-DLLVM_INCLUDE_BENCHMARKS=OFF",
        "-DLLVM_INCLUDE_DOCS=OFF",
        "-DLLVM_INCLUDE_EXAMPLES=OFF",
        "-DLLVM_INCLUDE_RUNTIMES=OFF",
        "-DLLVM_INCLUDE_TESTS=OFF",
        "-DLLVM_INCLUDE_UTILS=OFF",

        "-DLLVM_BUILD_STATIC=ON",
        "-DLLVM_BUILD_BENCHMARKS=OFF",
        "-DLLVM_BUILD_DOCS=OFF",
        "-DLLVM_BUILD_EXAMPLES=OFF",
        "-DLLVM_BUILD_RUNTIME=OFF",
        "-DLLVM_BUILD_TESTS=OFF",
        "-DLLVM_BUILD_UTILS=OFF",

        // https://github.com/ziglang/zig/issues/23546
        // https://codeberg.org/ziglang/zig/pulls/30073
        "-DCMAKE_LINK_DEPENDS_USE_LINKER=FALSE", // To avoid "error: unsupported linker arg:", "--dependency-file"

        "-DCMAKE_C_FLAGS=-mcpu=baseline",
        "-DCMAKE_CXX_FLAGS=-mcpu=baseline",

        // "-DCMAKE_VERBOSE_MAKEFILE=ON", // Increased build log verbosity
        "-DCMAKE_INSTALL_MESSAGE=NEVER",
        b.fmt("-DLLVM_PARALLEL_COMPILE_JOBS={d}", .{jobs}),
        b.fmt("-DLLVM_PARALLEL_LINK_JOBS={d}", .{jobs}),
    });
    setup_llvm.setEnvironmentVariable("CC", "zig;cc");
    setup_llvm.setEnvironmentVariable("CXX", "zig;c++");
    setup_llvm.setEnvironmentVariable("ASM", "zig;cc");
    setup_llvm.setName("llvm_setup");
    setup_llvm.step.dependOn(previous_step);

    // Build main LLVM
    const components = [_][]const u8{
        "llvm-headers",
        "lld-headers",
        "llvm-libraries",
        "llvm-config",
        "lldCommon",
        "lldELF",
        "lldCOFF",
        "lldMinGW",
        "install-llvm-libraries",
    };
    const build_llvm = b.addSystemCommand(&[_][]const u8{
        "cmake",                 "--build",  llvm_build_dir,
        b.fmt("-j{d}", .{jobs}), "--target",
    } ++ components);
    build_llvm.setName("llvm_build");
    build_llvm.step.dependOn(&setup_llvm.step);

    // Install main LLVM
    var install_runs: [components.len]*std.Build.Step.Run = undefined;
    for (components, 0..) |comp, i| {
        const cmd = b.addSystemCommand(&[_][]const u8{
            "cmake", "--install", llvm_build_dir, "--component", comp,
        });
        cmd.setName(b.fmt("llvm_install_{s}", .{comp}));
        if (i == 0) {
            cmd.step.dependOn(&build_llvm.step);
        } else {
            cmd.step.dependOn(&install_runs[i - 1].step);
        }
        install_runs[i] = cmd;
    }

    return install_runs[install_runs.len - 1];
}

fn updateLLVM(b: *std.Build, llvm_version: []const u8) !*std.Build.Step.Run {
    std.debug.print("-- Updating the 'llvm-project' repository\n", .{});
    // 1. Check if llvm-project exists in vendor directory
    if (std.Io.Dir.cwd().openDir(b.graph.io, "vendor/sources/llvm-project", .{})) |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'llvm-project'...\n", .{});
            return makeEmptyStep(b);
        }

        // 3. Reset hard
        const reset_llvm_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard", "-q" });
        reset_llvm_cmd.setName("reset_llvm");
        reset_llvm_cmd.setCwd(b.path("vendor/sources/llvm-project"));

        // 4. Fetch llvm-project
        const fetch_llvm_cmd = b.addSystemCommand(&[_][]const u8{ "git", "fetch", "-fq", "--depth", "1", "origin", "tag", llvm_version });
        fetch_llvm_cmd.setName("fetch_llvm");
        fetch_llvm_cmd.setCwd(b.path("vendor/sources/llvm-project"));
        fetch_llvm_cmd.step.dependOn(&reset_llvm_cmd.step);

        // 5. Checkout llvm-project at tag of `llvm_version`
        const checkout_llvm_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", llvm_version });
        checkout_llvm_cmd.setName("checkout_llvm");
        checkout_llvm_cmd.setCwd(b.path("vendor/sources/llvm-project"));
        checkout_llvm_cmd.step.dependOn(&fetch_llvm_cmd.step);

        return checkout_llvm_cmd;
    } else |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, unable to clone dependency 'llvm-project'...\n", .{});
            return error.NoInternetConnection;
        }

        // 3. Clone llvm
        const clone_llvm_step = b.addSystemCommand(&[_][]const u8{ "git", "clone", "--depth", "1", "--branch", llvm_version, "https://github.com/llvm/llvm-project.git", "vendor/sources/llvm-project" });
        clone_llvm_step.setName("clone_llvm");

        return clone_llvm_step;
    }
}

fn linkWithLLVM(
    b: *std.Build,
    previous_step: *std.Build.Step,
    exe: *std.Build.Step.Compile,
) !void {
    const LinkLLVMLibsStep = struct {
        step: std.Build.Step,
        exe: *std.Build.Step.Compile,
        static_lib_names: []const []const u8,

        pub fn make(step: *std.Build.Step, _: std.Build.Step.MakeOptions) !void {
            const self: *@This() = @fieldParentPtr("step", step);
            for (self.static_lib_names) |lib_name| {
                self.exe.root_module.linkSystemLibrary(lib_name, .{});
            }
        }
    };

    // Use command
    //     vendor/llvm-linux/bin/llvm-config --link-static --libs all | sed "s/ /\n/g" | sed "s/-l//g" | sed "s/^/\"/g" | sed "s/\$/\",/g"
    // To re-generate this list after a llvm version-upgrade
    const static_llvm_libs: []const []const u8 = &.{
        "LLVMWindowsManifest",
        "LLVMXRay",
        "LLVMLibDriver",
        "LLVMDlltoolDriver",
        "LLVMTelemetry",
        "LLVMTextAPIBinaryReader",
        "LLVMCoverage",
        "LLVMLineEditor",
        "LLVMX86TargetMCA",
        "LLVMX86Disassembler",
        "LLVMX86AsmParser",
        "LLVMX86CodeGen",
        "LLVMX86Desc",
        "LLVMX86Info",
        "LLVMOrcDebugging",
        "LLVMOrcJIT",
        "LLVMWindowsDriver",
        "LLVMMCJIT",
        "LLVMJITLink",
        "LLVMInterpreter",
        "LLVMExecutionEngine",
        "LLVMRuntimeDyld",
        "LLVMOrcTargetProcess",
        "LLVMOrcShared",
        "LLVMDWP",
        "LLVMDWARFCFIChecker",
        "LLVMDebugInfoLogicalView",
        "LLVMOption",
        "LLVMObjCopy",
        "LLVMMCA",
        "LLVMMCDisassembler",
        "LLVMDTLTO",
        "LLVMLTO",
        "LLVMPlugins",
        "LLVMPasses",
        "LLVMHipStdPar",
        "LLVMCFGuard",
        "LLVMCoroutines",
        "LLVMipo",
        "LLVMVectorize",
        "LLVMSandboxIR",
        "LLVMLinker",
        "LLVMFrontendOpenMP",
        "LLVMFrontendOffloading",
        "LLVMObjectYAML",
        "LLVMFrontendOpenACC",
        "LLVMFrontendDriver",
        "LLVMInstrumentation",
        "LLVMFrontendDirective",
        "LLVMFrontendAtomic",
        "LLVMExtensions",
        "LLVMDWARFLinkerParallel",
        "LLVMDWARFLinkerClassic",
        "LLVMDWARFLinker",
        "LLVMGlobalISel",
        "LLVMMIRParser",
        "LLVMAsmPrinter",
        "LLVMSelectionDAG",
        "LLVMCodeGen",
        "LLVMTarget",
        "LLVMObjCARCOpts",
        "LLVMCodeGenTypes",
        "LLVMCGData",
        "LLVMCAS",
        "LLVMIRPrinter",
        "LLVMInterfaceStub",
        "LLVMFileCheck",
        "LLVMFuzzMutate",
        "LLVMScalarOpts",
        "LLVMInstCombine",
        "LLVMAggressiveInstCombine",
        "LLVMTransformUtils",
        "LLVMBitWriter",
        "LLVMAnalysis",
        "LLVMProfileData",
        "LLVMSymbolize",
        "LLVMDebugInfoBTF",
        "LLVMDebugInfoPDB",
        "LLVMDebugInfoMSF",
        "LLVMDebugInfoCodeView",
        "LLVMDebugInfoGSYM",
        "LLVMDebugInfoDWARF",
        "LLVMObject",
        "LLVMTextAPI",
        "LLVMMCParser",
        "LLVMIRReader",
        "LLVMAsmParser",
        "LLVMMC",
        "LLVMDebugInfoDWARFLowLevel",
        "LLVMBitReader",
        "LLVMFrontendHLSL",
        "LLVMFuzzerCLI",
        "LLVMABI",
        "LLVMCore",
        "LLVMRemarks",
        "LLVMBitstreamReader",
        "LLVMBinaryFormat",
        "LLVMTargetParser",
        "LLVMTableGen",
        "LLVMSupportLSP",
        "LLVMSupport",
        "LLVMDemangle",
    };

    const link_llvm_libs_step = try b.allocator.create(LinkLLVMLibsStep);
    link_llvm_libs_step.* = .{
        .step = std.Build.Step.init(.{
            .id = .custom,
            .name = "Link LLVM libraries",
            .owner = b,
            .makeFn = LinkLLVMLibsStep.make,
        }),
        .exe = exe,
        .static_lib_names = static_llvm_libs,
    };
    link_llvm_libs_step.step.dependOn(previous_step);
    exe.step.dependOn(&link_llvm_libs_step.step);
}

/// Create a no-op Run step that meets the return type requirements
pub fn makeEmptyStep(b: *std.Build) !*std.Build.Step.Run {
    const run_step = b.addSystemCommand(&[_][]const u8{ "zig", "version" });
    run_step.setName("make_empty_step");
    _ = run_step.captureStdOut(.{});
    return run_step;
}

pub fn hasInternetConnection(b: *std.Build) bool {
    const hostname: std.Io.net.HostName = .{ .bytes = "google.com" };
    const conn: std.Io.net.Stream = hostname.connect(
        b.graph.io,
        443,
        .{
            .mode = .stream,
            .protocol = .tcp,
            .timeout = .none,
        },
    ) catch return false;
    conn.close(b.graph.io);
    return true;
}

/// Target[0] is Linux
///
/// Target[1] is Windows
fn targets(b: *std.Build) [2]std.Build.ResolvedTarget {
    return [_]std.Build.ResolvedTarget{
        b.resolveTargetQuery(.{
            .cpu_model = .baseline,
            .cpu_arch = .x86_64,
            .os_tag = .linux,
            .abi = .musl,
        }),
        b.resolveTargetQuery(.{
            .cpu_model = .baseline,
            .cpu_arch = .x86_64,
            .os_tag = .windows,
            .abi = .gnu,
        }),
    };
}
