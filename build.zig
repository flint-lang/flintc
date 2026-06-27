const std = @import("std");

const FLINTC_VERSION = @import("build.zig.zon").version;
const DEFAULT_LLVM_VERSION = "llvmorg-21.1.8";
const FIP_VERSION = "aa697b3e0c513e3e8c7cdbfa957789a3927c7222";

const JSON_MINI_HASH = "a32d6e8319d90f5fa75f1651f30798c71464e4c6";

pub fn build(b: *std.Build) !void {
    const OSTag = enum { linux, windows };
    _ = b.findProgram(&.{"ld.lld"}, &.{}) catch @panic("LLD not found on this system");
    _ = b.findProgram(&.{"cmake"}, &.{}) catch @panic("CMake not found on this system");
    _ = b.findProgram(&.{"ninja"}, &.{}) catch @panic("Ninja not found on this system");
    _ = b.findProgram(&.{"python"}, &.{}) catch @panic("Python3 not found on this system");

    const host_target = b.resolveTargetQuery(.{});
    const optimize = b.standardOptimizeOption(.{});

    const external_llvm_dir = b.option([]const u8, "llvm-dir", "Path to external LLVM installation.");
    const external_llvm_prebuilt: bool = b.option(bool, "llvm-prebuilt", "Whether to use the 'llvm-dir' as a prebuilt llvm version, which skips building llvm.") orelse false;
    if (external_llvm_prebuilt and external_llvm_dir == null) {
        @panic("To use 'llvm-prebuilt' a 'llvm-dir' needs to be provided.");
    }
    const external_skip_llvm_config = b.option(bool, "skip-llvm-config", "Wehther to skip llvm-config being executed.") orelse false;
    const external_fip_dir = b.option([]const u8, "fip-dir", "Path to external FIP installation. Skips fetching FIP.");
    const external_json_mini_dir = b.option([]const u8, "json-mini-dir", "Path to external json-mini installation. Skips fetching json-mini.");
    const external_hash = b.option([]const u8, "git-hash", "Git hash of the project needed for nix-build.");

    if (external_llvm_dir == null or external_json_mini_dir == null or external_fip_dir == null) {
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

    // Update Json Mini
    const update_json_mini = if (external_json_mini_dir) |_| try makeEmptyStep(b) else try updateJsonMini(b);
    // Update FIP
    const update_fip = if (external_fip_dir) |_| blk: {
        const step = try makeEmptyStep(b);
        step.step.dependOn(&update_json_mini.step);
        break :blk step;
    } else try updateFip(b, &update_json_mini.step);

    // Update LLVM if no external LLVM is passed
    var last_step = update_fip;
    if (external_llvm_dir == null) {
        last_step = try updateLLVM(b, &update_fip.step, llvm_version);
    }
    // Build LLVM if the extern LLVM dir is not prebuilt
    const llvm_step = if (external_llvm_prebuilt)
        try makeEmptyStep(b)
    else
        try buildLLVM(b, &last_step.step, target, force_llvm_rebuild, jobs, external_llvm_dir);
    const llvm_dir: ?[]const u8 = if (external_llvm_prebuilt) external_llvm_dir else null;

    // Build flintc exe
    const flintc_exe = try buildFlintc(b, &llvm_step.step, target, optimize, commit_hash, build_date, external_fip_dir, external_json_mini_dir, llvm_dir, external_skip_llvm_config);
    const flintc_exe_install = b.addInstallArtifact(flintc_exe, .{});
    b.getInstallStep().dependOn(&flintc_exe_install.step);
    // Build FLS exe
    const fls_exe = try buildFLS(b, &update_fip.step, target, optimize, commit_hash, build_date, external_fip_dir);
    fls_exe.step.dependOn(&flintc_exe.step);
    const fls_exe_install = b.addInstallArtifact(fls_exe, .{});
    b.getInstallStep().dependOn(&fls_exe_install.step);

    // Build all
    const build_all_step = b.step("all", "Build all targets");
    var last_target_step: *std.Build.Step = &fls_exe.step;
    for (targets(b)) |t| {
        const build_llvm_step = try buildLLVM(b, &llvm_step.step, t, force_llvm_rebuild, jobs, external_llvm_dir);

        const flintc_exe_debug = try buildFlintc(b, &build_llvm_step.step, t, .Debug, commit_hash, build_date, external_fip_dir, external_json_mini_dir, llvm_dir, external_skip_llvm_config);
        flintc_exe_debug.step.dependOn(last_target_step);
        build_all_step.dependOn(&b.addInstallArtifact(flintc_exe_debug, .{}).step);

        const fls_exe_debug = try buildFLS(b, &update_fip.step, t, .Debug, commit_hash, build_date, external_fip_dir);
        fls_exe_debug.step.dependOn(&flintc_exe_debug.step);
        build_all_step.dependOn(&b.addInstallArtifact(fls_exe_debug, .{}).step);

        const flintc_exe_release = try buildFlintc(b, &build_llvm_step.step, t, .ReleaseSmall, commit_hash, build_date, external_fip_dir, external_json_mini_dir, llvm_dir, external_skip_llvm_config);
        flintc_exe_release.step.dependOn(&fls_exe_debug.step);
        build_all_step.dependOn(&b.addInstallArtifact(flintc_exe_release, .{}).step);

        const fls_exe_release = try buildFLS(b, &update_fip.step, t, .ReleaseSmall, commit_hash, build_date, external_fip_dir);
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
        test_cmd.addArg("--file");
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
    previous_step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    commit_hash: []const u8,
    build_date: []const u8,
    external_fip_dir: ?[]const u8,
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

    const fip_dir = if (external_fip_dir) |dir| dir else "vendor/sources/fip";

    // Add Include paths
    exe.root_module.addIncludePath(.{ .cwd_relative = fip_dir });
    exe.root_module.addIncludePath(b.path("include"));
    exe.root_module.addIncludePath(b.path("fls/include"));

    // zig fmt: off
    // Add C++ src files
    exe.root_module.addCSourceFiles(.{
        .files = &[_][]const u8{
            // LSP sources
            "fls/src/main.cpp",
            "fls/src/lsp_server.cpp",
            "fls/src/lsp_protocol.cpp",
            "fls/src/completion_data.cpp",
            "fls/src/completion.cpp",

            // Sources from the main project
            "src/error/base_error.cpp",
            "src/error/err_expr_call_of_undefined_function.cpp",
            "src/parser/error_node.cpp",
            "src/parser/file_node.cpp",
            "src/parser/namespace.cpp",
            "src/parser/parser.cpp",
            "src/parser/parser_definition.cpp",
            "src/parser/parser_expression.cpp",
            "src/parser/parser_statement.cpp",
            "src/parser/parser_util.cpp",
            "src/parser/scope.cpp",
            "src/parser/type.cpp",
            "src/analyzer.cpp",
            "src/debug.cpp",
            "src/fip.cpp",
            "src/lexer.cpp",
            "src/matcher.cpp",
            "src/profiler.cpp",
            "src/resolver.cpp",
        },
        .flags = &[_][]const u8{
            "-std=c++17",                   // Set C++ standard to C++17
            "-Werror",                      // Treat warnings as errors
            "-Wall",                        // Enable most warnings
            "-Wextra",                      // Enable extra warnings
            "-Wshadow",                     // Warn about shadow variables
            "-Wcast-align",                 // Warn about pointer casts that increase alignment requirement
            "-Wcast-qual",                  // Warn about casts that remove const qualifier
            "-Wunused",                     // Warn about unused variables
            "-Wold-style-cast",             // Warn about C-style casts
            "-Wdouble-promotion",           // Warn about float being implicitly promoted to double
            "-Wformat=2",                   // Warn about printf/scanf/strftime/strfmon format string issue
            "-Wundef",                      // Warn if an undefined identifier is evaluated in an #if
            "-Wpointer-arith",              // Warn about sizeof(void) and add/sub with void*
            "-Wunreachable-code",           // Warn about unreachable code
            "-Wno-deprecated-declarations", // Ignore deprecation warnings
            "-Wno-deprecated",              // Ignore general deprecation warnings
            "-fno-omit-frame-pointer",      // Prevent omitting frame pointer for debugging and stack unwinding
            "-funwind-tables",              // Generate unwind tables for stack unwinding
            "-ffunction-sections",          // Place each function in its own section
            "-fdata-sections",              // Place each data object in its own section
            "-fstandalone-debug",           // Emit standalone debug information
            "-Wdeprecated-declarations",    // Warn about deprecated declarations
        },
    });
    // zig fmt: on

    // Add toml C src file for FIP
    exe.root_module.addCSourceFile(.{
        .file = .{ .cwd_relative = b.fmt("{s}/toml/tomlc17.c", .{fip_dir}) },
    });

    exe.step.dependOn(previous_step);

    return exe;
}

fn buildFlintc(
    b: *std.Build,
    previous_step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    commit_hash: []const u8,
    build_date: []const u8,
    external_fip_dir: ?[]const u8,
    external_json_mini_dir: ?[]const u8,
    external_llvm_dir: ?[]const u8,
    external_skip_llvm_config: bool,
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
    const fip_dir = if (external_fip_dir) |dir| dir else "vendor/sources/fip";
    const json_mini_dir = if (external_json_mini_dir) |dir| dir else "vendor/sources/json-mini";

    // Add Include paths
    exe.root_module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{llvm_dir}) });
    exe.root_module.addIncludePath(.{ .cwd_relative = fip_dir });
    exe.root_module.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{json_mini_dir}) });
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
        if (entry.kind == .file and std.mem.endsWith(u8, entry.basename, ".cpp")) {
            try cpp_files.append(b.allocator, try b.allocator.dupe(u8, entry.path));
        }
    }

    // zig fmt: off
    // Add C++ src files
    exe.root_module.addCSourceFiles(.{
        .root = b.path("src"),
        .files = cpp_files.items,
        .flags = &[_][]const u8{
            "-std=c++17",                   // Set C++ standard to C++17
            "-Werror",                      // Treat warnings as errors
            "-Wall",                        // Enable most warnings
            "-Wextra",                      // Enable extra warnings
            "-Wshadow",                     // Warn about shadow variables
            "-Wcast-align",                 // Warn about pointer casts that increase alignment requirement
            "-Wcast-qual",                  // Warn about casts that remove const qualifier
            "-Wunused",                     // Warn about unused variables
            "-Wold-style-cast",             // Warn about C-style casts
            "-Wdouble-promotion",           // Warn about float being implicitly promoted to double
            "-Wformat=2",                   // Warn about printf/scanf/strftime/strfmon format string issue
            "-Wundef",                      // Warn if an undefined identifier is evaluated in an #if
            "-Wpointer-arith",              // Warn about sizeof(void) and add/sub with void*
            "-Wunreachable-code",           // Warn about unreachable code
            "-Wno-deprecated-declarations", // Ignore deprecation warnings
            "-Wno-deprecated",              // Ignore general deprecation warnings
            "-fno-omit-frame-pointer",      // Prevent omitting frame pointer for debugging and stack unwinding
            "-funwind-tables",              // Generate unwind tables for stack unwinding
            "-ffunction-sections",          // Place each function in its own section
            "-fdata-sections",              // Place each data object in its own section
            "-fstandalone-debug",           // Emit standalone debug information
            // "-Wdeprecated-declarations", // Warn about deprecated declarations
        },
    });
    // zig fmt: on

    // Add toml C src file for FIP
    exe.root_module.addCSourceFile(.{
        .file = .{ .cwd_relative = b.fmt("{s}/toml/tomlc17.c", .{fip_dir}) },
    });

    // Library linking
    if (target.result.os.tag == .windows) {
        exe.root_module.linkSystemLibrary("ole32", .{});
    }
    exe.root_module.linkSystemLibrary("lldCOFF", .{});
    exe.root_module.linkSystemLibrary("lldELF", .{});
    exe.root_module.linkSystemLibrary("lldCommon", .{});

    // Link LLVM libraries
    try linkWithLLVM(b, previous_step, exe, target, llvm_dir, external_skip_llvm_config);

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

fn updateLLVM(b: *std.Build, previous_step: *std.Build.Step, llvm_version: []const u8) !*std.Build.Step.Run {
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
        reset_llvm_cmd.step.dependOn(previous_step);

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
        clone_llvm_step.step.dependOn(previous_step);

        return clone_llvm_step;
    }
}

fn updateFip(b: *std.Build, previous_step: *std.Build.Step) !*std.Build.Step.Run {
    // 1. Check if fip exists in vendor directory
    std.debug.print("-- Updating the 'fip' repository\n", .{});
    if (std.Io.Dir.cwd().openDir(b.graph.io, "vendor/sources/fip", .{})) |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'fip'...\n", .{});
            return makeEmptyStep(b);
        }

        // 3. Reset hard
        const reset_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard", "-q" });
        reset_fip_cmd.setName("reset_fip");
        reset_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        reset_fip_cmd.step.dependOn(previous_step);

        // 4. Fetch fip
        const fetch_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "fetch" });
        fetch_fip_cmd.setName("fetch_fip");
        fetch_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        fetch_fip_cmd.step.dependOn(&reset_fip_cmd.step);

        // 5. Checkout fip main
        const checkout_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", "main" });
        checkout_fip_cmd.setName("checkout_fip");
        checkout_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        checkout_fip_cmd.step.dependOn(&fetch_fip_cmd.step);

        // 6. Pull fip
        const pull_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "pull", "-fq" });
        pull_fip_cmd.setName("pull_fip");
        pull_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        pull_fip_cmd.step.dependOn(&checkout_fip_cmd.step);

        // 7. Checkout fip hash
        const checkout_fip_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", FIP_VERSION });
        checkout_fip_hash_cmd.setName("checkout_fip_hash");
        checkout_fip_hash_cmd.setCwd(b.path("vendor/sources/fip"));
        checkout_fip_hash_cmd.step.dependOn(&pull_fip_cmd.step);

        return checkout_fip_hash_cmd;
    } else |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, unable to clone dependency 'fip'...\n", .{});
            return error.NoInternetConnection;
        }

        // 3. Clone fip
        const fetch_fip_complete_step = b.addSystemCommand(&[_][]const u8{ "git", "clone", "https://github.com/flint-lang/fip.git", "vendor/sources/fip" });
        fetch_fip_complete_step.setName("fetch_fip_complete");
        fetch_fip_complete_step.step.dependOn(previous_step);

        // 4. Checkout fip hash
        const checkout_fip_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", FIP_VERSION });
        checkout_fip_hash_cmd.setName("checkout_fip_hash");
        checkout_fip_hash_cmd.setCwd(b.path("vendor/sources/fip"));
        checkout_fip_hash_cmd.step.dependOn(&fetch_fip_complete_step.step);

        return checkout_fip_hash_cmd;
    }
}

fn updateJsonMini(b: *std.Build) !*std.Build.Step.Run {
    // 1. Check if json-mini exists in vendor directory
    std.debug.print("-- Updating the 'json-mini' repository\n", .{});
    if (std.Io.Dir.cwd().openDir(b.graph.io, "vendor/sources/json-mini", .{})) |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'json-mini'...\n", .{});
            return makeEmptyStep(b);
        }

        // 3. Reset hard
        const reset_json_mini_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard", "-q" });
        reset_json_mini_cmd.setName("reset_json_mini");
        reset_json_mini_cmd.setCwd(b.path("vendor/sources/json-mini"));

        // 4. Fetch json-mini
        const fetch_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "fetch" });
        fetch_json_mini_step.setName("fetch_json_mini");
        fetch_json_mini_step.setCwd(b.path("vendor/sources/json-mini"));
        fetch_json_mini_step.step.dependOn(&reset_json_mini_cmd.step);

        // 5. Checkout json-mini main
        const checkout_json_mini_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", "main" });
        checkout_json_mini_cmd.setName("checkout_json_mini");
        checkout_json_mini_cmd.setCwd(b.path("vendor/sources/json-mini"));
        checkout_json_mini_cmd.step.dependOn(&fetch_json_mini_step.step);

        // 6. Pull json-mini
        const pull_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "pull", "-fq" });
        pull_json_mini_step.setName("pull_json_mini");
        pull_json_mini_step.setCwd(b.path("vendor/sources/json-mini"));
        pull_json_mini_step.step.dependOn(&checkout_json_mini_cmd.step);

        // 7. Checkout json-mini hash
        const checkout_json_mini_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", JSON_MINI_HASH });
        checkout_json_mini_hash_cmd.setName("checkout_json_mini_hash");
        checkout_json_mini_hash_cmd.setCwd(b.path("vendor/sources/json-mini"));
        checkout_json_mini_hash_cmd.step.dependOn(&pull_json_mini_step.step);

        return checkout_json_mini_hash_cmd;
    } else |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, unable to clone dependency 'json-mini'...\n", .{});
            return error.NoInternetConnection;
        }

        // 3. Clone json-mini
        const clone_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "clone", "https://github.com/flint-lang/json-mini.git", "vendor/sources/json-mini" });
        clone_json_mini_step.setName("clone_json_mini");

        // 4. Checkout fip hash
        const checkout_json_mini_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", JSON_MINI_HASH });
        checkout_json_mini_hash_cmd.setName("checkout_json_mini_hash");
        checkout_json_mini_hash_cmd.setCwd(b.path("vendor/sources/json-mini"));
        checkout_json_mini_hash_cmd.step.dependOn(&clone_json_mini_step.step);

        return checkout_json_mini_hash_cmd;
    }
}

fn linkWithLLVM(
    b: *std.Build,
    previous_step: *std.Build.Step,
    exe: *std.Build.Step.Compile,
    target: std.Build.ResolvedTarget,
    llvm_dir: []const u8,
    external_skip_llvm_config: bool,
) !void {
    const LinkLLVMLibsStep = struct {
        step: std.Build.Step,
        exe: *std.Build.Step.Compile,
        output_file: ?std.Build.LazyPath,
        static_lib_names: ?[]const []const u8,

        pub fn make(step: *std.Build.Step, _: std.Build.Step.MakeOptions) !void {
            const self: *@This() = @fieldParentPtr("step", step);

            if (self.static_lib_names) |lib_names| {
                for (lib_names) |lib_name| {
                    self.exe.root_module.linkSystemLibrary(lib_name, .{});
                }
                return;
            }

            const b_llvm = self.step.owner;
            const file_path = self.output_file.?.getPath(b_llvm);

            const contents: []const u8 = try std.Io.Dir.cwd().readFileAlloc(b_llvm.graph.io, file_path, b_llvm.allocator, .limited(std.math.maxInt(u32)));
            defer b_llvm.allocator.free(contents);

            const dash_l = "-l";
            var it = std.mem.tokenizeScalar(u8, contents, ' ');
            while (it.next()) |token| {
                const trimmed = std.mem.trim(u8, token, &std.ascii.whitespace);
                std.debug.assert(trimmed.len > 0);
                std.debug.assert(std.mem.startsWith(u8, trimmed, dash_l));

                const lib_name = trimmed[dash_l.len..];
                self.exe.root_module.linkSystemLibrary(lib_name, .{});
            }
        }
    };

    const static_llvm_libs: ?[]const []const u8 = if (external_skip_llvm_config) blk: {
        // Fill this array with the output of `llvm-config --link-static --libs all` for your system.
        break :blk &.{
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
            "LLVMLTO",
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
            "LLVMFrontendHLSL",
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
            "LLVMDebugInfoDWARFLowLevel",
            "LLVMObject",
            "LLVMTextAPI",
            "LLVMMCParser",
            "LLVMIRReader",
            "LLVMAsmParser",
            "LLVMMC",
            "LLVMBitReader",
            "LLVMFuzzerCLI",
            "LLVMCore",
            "LLVMRemarks",
            "LLVMBitstreamReader",
            "LLVMBinaryFormat",
            "LLVMTargetParser",
            "LLVMTableGen",
            "LLVMSupport",
            "LLVMDemangle",
        };
    } else null;

    const llvm_config_cmd = if (external_skip_llvm_config) null else blk: {
        const llvm_config_path = b.fmt("{s}/bin/{s}", .{ llvm_dir, switch (target.result.os.tag) {
            .linux => "llvm-config",
            .windows => "llvm-config.exe",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        } });
        const cmd = b.addSystemCommand(&.{ llvm_config_path, "--link-static", "--libs", "all" });
        cmd.step.dependOn(previous_step);
        break :blk cmd;
    };

    const link_llvm_libs_step = try b.allocator.create(LinkLLVMLibsStep);
    link_llvm_libs_step.* = .{
        .step = std.Build.Step.init(.{
            .id = .custom,
            .name = "Link LLVM libraries from llvm-config",
            .owner = b,
            .makeFn = LinkLLVMLibsStep.make,
        }),
        .exe = exe,
        .output_file = if (llvm_config_cmd) |cmd| cmd.captureStdOut(.{}) else null,
        .static_lib_names = static_llvm_libs,
    };
    if (llvm_config_cmd) |cmd| {
        link_llvm_libs_step.step.dependOn(&cmd.step);
    }
    exe.step.dependOn(&link_llvm_libs_step.step);
}

/// Create a no-op Run step that meets the return type requirements
fn makeEmptyStep(b: *std.Build) !*std.Build.Step.Run {
    const run_step = b.addSystemCommand(&[_][]const u8{ "zig", "version" });
    run_step.setName("make_empty_step");
    _ = run_step.captureStdOut(.{});
    return run_step;
}

fn hasInternetConnection(b: *std.Build) bool {
    std.debug.print("in\n", .{});
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
    std.debug.print("out\n", .{});
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
