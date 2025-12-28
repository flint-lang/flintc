const std = @import("std");

const FLINTC_VERSION = @import("build.zig.zon").version;
const DEFAULT_LLVM_VERSION = "llvmorg-21.1.8";

const FIP_HASH = "3710c353493bea6a6e5ade363bde6fb2e2285cb5";
const JSON_MINI_HASH = "a32d6e8319d90f5fa75f1651f30798c71464e4c6";

pub fn build(b: *std.Build) !void {
    const OSTag = enum { linux, windows };
    _ = b.findProgram(&.{"git"}, &.{}) catch @panic("Git not found on this system");
    _ = b.findProgram(&.{"cmake"}, &.{}) catch @panic("CMake not found on this system");
    _ = b.findProgram(&.{"ninja"}, &.{}) catch @panic("Ninja not found on this system");
    _ = b.findProgram(&.{"python"}, &.{}) catch @panic("Python3 not found on this system");
    _ = b.findProgram(&.{"ld.lld"}, &.{}) catch @panic("LLD not found on this system");

    const host_target = b.resolveTargetQuery(.{});
    const optimize = b.standardOptimizeOption(.{});

    const llvm_version = b.option([]const u8, "llvm-version", b.fmt("LLVM version to use. Default: {s}", .{DEFAULT_LLVM_VERSION})) orelse
        DEFAULT_LLVM_VERSION;
    const force_llvm_rebuild = b.option(bool, "rebuild-llvm", "Force rebuild LLVM") orelse
        false;
    const jobs = b.option(usize, "jobs", "Number of cores to use for building LLVM") orelse
        (try std.Thread.getCpuCount() - 2);
    const target_option: OSTag = b.option(OSTag, "target", "The OS to build for") orelse switch (host_target.result.os.tag) {
        .linux => .linux,
        .windows => .windows,
        else => @panic("Unsupported OS"),
    };
    const target_query: std.Target.Query = switch (target_option) {
        .linux => .{
            .cpu_arch = .x86_64,
            .cpu_model = .baseline,
            .os_tag = .linux,
            .os_version_min = .{ .semver = .{ .major = 6, .minor = 12, .patch = 0 } },
            .abi = .gnu,
            .glibc_version = .{ .major = 2, .minor = 40, .patch = 0 },
        },
        .windows => .{
            .cpu_arch = .x86_64,
            .cpu_model = .baseline,
            .os_tag = .windows,
            .os_version_min = .{ .windows = .win7 },
            .abi = .gnu,
            .glibc_version = .{ .major = 2, .minor = 40, .patch = 0 },
        },
    };
    const target = b.resolveTargetQuery(target_query);

    // Update Json Mini
    const update_json_mini = try updateJsonMini(b);
    // Update Fip
    const update_fip = try updateFip(b, &update_json_mini.step);
    // Update LLVM
    const update_llvm = try updateLLVM(b, &update_fip.step, llvm_version);
    // Build LLVM
    const build_llvm = try buildLLVM(b, &update_llvm.step, target, force_llvm_rebuild, jobs);
    // Build flintc exe
    try buildFlintc(b, &build_llvm.step, target, optimize);
    // Build FLS exe
    try buildFLS(b, &update_fip.step, target, optimize);
}

fn buildFLS(b: *std.Build, previous_step: *std.Build.Step, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) !void {
    const exe = b.addExecutable(.{
        .name = if (optimize == .Debug) "fls-debug" else "fls",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        }),
        .version = try .parse(FLINTC_VERSION),
    });
    b.installArtifact(exe);
    exe.root_module.addCMacro("FLINT_LSP", "");
    if (optimize == .Debug) {
        exe.root_module.addCMacro("DEBUG_BUILD", "");
    } else {
        exe.lto = .full;
    }
    exe.link_function_sections = true;
    exe.link_data_sections = true;
    exe.compress_debug_sections = .zlib;
    exe.build_id = .fast;

    // Add commit hash
    const commit_hash: []const u8 = blk: {
        const commit_hash = b.run(&[_][]const u8{ "git", "rev-parse", "--short", "HEAD" });
        break :blk std.mem.trim(u8, commit_hash, &std.ascii.whitespace);
    };
    std.debug.print("-- Commit Hash is '{s}'\n", .{commit_hash});
    exe.root_module.addCMacro("COMMIT_HASH", b.fmt("\"{s}\"", .{commit_hash}));

    // Add build date
    const build_date: []const u8 = blk: {
        const current_timestamp: u64 = @intCast(std.time.timestamp());
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
    exe.root_module.addCMacro("BUILD_DATE", build_date);

    // Add Include paths
    exe.root_module.addIncludePath(b.path("vendor/sources/fip"));
    exe.root_module.addIncludePath(b.path("include"));
    exe.root_module.addIncludePath(b.path("fls/include"));

    // zig fmt: off
    // Add C++ src files
    exe.root_module.addCSourceFiles(.{
        .root = b.path("fls"),
        .files = &[_][]const u8{
            // LSP sources
            "src/main.cpp",
            "src/lsp_server.cpp",
            "src/lsp_protocol.cpp",
            "src/completion_data.cpp",
            "src/completion.cpp",

            // Sources from the main project
            "../src/error/base_error.cpp",
            "../src/error/err_expr_call_of_undefined_function.cpp",
            "../src/parser/error_node.cpp",
            "../src/parser/namespace.cpp",
            "../src/parser/parser.cpp",
            "../src/parser/parser_definition.cpp",
            "../src/parser/parser_expression.cpp",
            "../src/parser/parser_statement.cpp",
            "../src/parser/parser_util.cpp",
            "../src/parser/type.cpp",
            "../src/analyzer.cpp",
            "../src/debug.cpp",
            "../src/fip.cpp",
            "../src/lexer.cpp",
            "../src/matcher.cpp",
            "../src/profiler.cpp",
            "../src/resolver.cpp",
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
        .file = b.path("vendor/sources/fip/toml/tomlc17.c"),
    });

    exe.step.dependOn(previous_step);
}

fn buildFlintc(b: *std.Build, previous_step: *std.Build.Step, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) !void {
    const exe = b.addExecutable(.{
        .name = if (optimize == .Debug) "flintc-debug" else "flintc",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        }),
        .version = try .parse(FLINTC_VERSION),
    });
    b.installArtifact(exe);
    if (optimize == .Debug) {
        exe.root_module.addCMacro("DEBUG_BUILD", "");
    }
    exe.link_function_sections = true;
    exe.link_data_sections = true;
    exe.compress_debug_sections = .zlib;
    exe.build_id = .fast;

    const run_step = b.step("run", "Run the app");
    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    // Add commit hash
    const commit_hash: []const u8 = blk: {
        const commit_hash = b.run(&[_][]const u8{ "git", "rev-parse", "--short", "HEAD" });
        break :blk std.mem.trim(u8, commit_hash, &std.ascii.whitespace);
    };
    std.debug.print("-- Commit Hash is '{s}'\n", .{commit_hash});
    exe.root_module.addCMacro("COMMIT_HASH", b.fmt("{s}", .{commit_hash}));

    // Add build date
    const build_date: []const u8 = blk: {
        const current_timestamp: u64 = @intCast(std.time.timestamp());
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
    exe.root_module.addCMacro("BUILD_DATE", build_date);

    const llvm_dir = switch (target.result.os.tag) {
        .linux => "vendor/llvm-linux",
        .windows => "vendor/llvm-mingw",
        else => return error.TargetNeedsToBeLinuxOrWindows,
    };

    // Add Include paths
    exe.root_module.addSystemIncludePath(b.path(b.fmt("{s}/include", .{llvm_dir})));
    exe.root_module.addIncludePath(b.path("vendor/sources/fip"));
    exe.root_module.addIncludePath(b.path("vendor/sources/json-mini/include"));
    exe.root_module.addIncludePath(b.path("tests"));
    exe.root_module.addIncludePath(b.path("include"));

    // Add Library paths
    exe.root_module.addLibraryPath(b.path(b.fmt("{s}/lib", .{llvm_dir})));

    // Collect C++ files
    var src_dir: std.fs.Dir = try std.fs.cwd().openDir("src", .{ .iterate = true });
    defer src_dir.close();
    var cpp_files: std.ArrayList([]const u8) = .empty;
    defer cpp_files.deinit(b.allocator);
    var walker = try src_dir.walk(b.allocator);
    defer walker.deinit();
    while (try walker.next()) |entry| {
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
        .file = b.path("vendor/sources/fip/toml/tomlc17.c"),
    });

    // Library linking
    if (target.result.os.tag == .windows) {
        exe.root_module.linkSystemLibrary("ole32", .{});
    }
    exe.root_module.linkSystemLibrary("lldCOFF", .{});
    exe.root_module.linkSystemLibrary("lldELF", .{});
    exe.root_module.linkSystemLibrary("lldCommon", .{});

    // Link LLVM libraries
    try linkWithLLVM(b, previous_step, exe, target, llvm_dir);
}

fn buildLLVM(b: *std.Build, previous_step: *std.Build.Step, target: std.Build.ResolvedTarget, force_rebuild: bool, jobs: usize) !*std.Build.Step.Run {
    const build_name: []const u8 = switch (target.result.os.tag) {
        .linux => "linux",
        .windows => "mingw",
        else => return error.TargetNeedsToBeLinuxOrWindows,
    };
    const llvm_build_dir = b.fmt(".zig-cache/llvm-{s}", .{build_name});
    const install_dir = b.fmt("vendor/llvm-{s}", .{build_name});

    if (std.fs.cwd().openDir(install_dir, .{})) |_| {
        // LLVM is already built, rebuilt only if requested
        if (force_rebuild) {
            try std.fs.cwd().deleteTree(install_dir);
        } else {
            return makeEmptyStep(b);
        }
    } else |_| {}
    if (force_rebuild) {
        try std.fs.cwd().deleteTree(llvm_build_dir);
    }

    std.debug.print("-- Building LLVM for {s}\n", .{build_name});

    // Setup LLVM
    const setup_llvm = b.addSystemCommand(&[_][]const u8{
        "cmake",
        "-S",
        "vendor/sources/llvm-project/llvm",
        "-B",
        llvm_build_dir,
        "-G",
        "Ninja",
        b.fmt("-DCMAKE_INSTALL_PREFIX={s}", .{install_dir}),
        "-DCMAKE_BUILD_TYPE=MinSizeRel",
        b.fmt("-DCMAKE_C_COMPILER={s}", .{switch (target.result.os.tag) {
            .linux => "zig;cc",
            .windows => "zig;cc;-target;x86_64-windows-gnu",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        b.fmt("-DCMAKE_CXX_COMPILER={s}", .{switch (target.result.os.tag) {
            .linux => "zig;c++",
            .windows => "zig;c++;-target;x86_64-windows-gnu",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        b.fmt("-DCMAKE_ASM_COMPILER={s}", .{switch (target.result.os.tag) {
            .linux => "zig;cc",
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
        "-DLLVM_ENABLE_PIC=OFF", // To avoid "error: unsupported linker arg:", "-Bsymbolic-functions"
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
    setup_llvm.setEnvironmentVariable("CXX", "zig;cxx");
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
    if (std.fs.cwd().openDir("vendor/sources/llvm-project", .{})) |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'llvm-project'...\n", .{});
            return makeEmptyStep(b);
        }

        // 3. Reset hard
        const reset_llvm_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard" });
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
    if (std.fs.cwd().openDir("vendor/sources/fip", .{})) |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'fip'...\n", .{});
            return makeEmptyStep(b);
        }

        // 3. Reset hard
        const reset_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard" });
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
        const pull_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "pull" });
        pull_fip_cmd.setName("pull_fip");
        pull_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        pull_fip_cmd.step.dependOn(&checkout_fip_cmd.step);

        // 7. Checkout fip hash
        const checkout_fip_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", FIP_HASH });
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
        const checkout_fip_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", "main" });
        checkout_fip_hash_cmd.setName("checkout_fip_hash");
        checkout_fip_hash_cmd.setCwd(b.path("vendor/sources/fip"));
        checkout_fip_hash_cmd.step.dependOn(&fetch_fip_complete_step.step);

        return checkout_fip_hash_cmd;
    }
}

fn updateJsonMini(b: *std.Build) !*std.Build.Step.Run {
    // 1. Check if json-mini exists in vendor directory
    std.debug.print("-- Updating the 'json-mini' repository\n", .{});
    if (std.fs.cwd().openDir("vendor/sources/json-mini", .{})) |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'json-mini'...\n", .{});
            return makeEmptyStep(b);
        }

        // 3. Reset hard
        const reset_json_mini_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard" });
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
        const pull_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "pull" });
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

fn linkWithLLVM(b: *std.Build, previous_step: *std.Build.Step, exe: *std.Build.Step.Compile, target: std.Build.ResolvedTarget, llvm_dir: []const u8) !void {
    const llvm_config_path = b.fmt("./{s}/bin/{s}", .{ llvm_dir, switch (target.result.os.tag) {
        .linux => "llvm-config",
        .windows => "llvm-config.exe",
        else => return error.TargetNeedsToBeLinuxOrWindows,
    } });
    const llvm_config_cmd = b.addSystemCommand(&.{ llvm_config_path, "--link-static", "--libs", "all" });
    llvm_config_cmd.step.dependOn(previous_step);

    const LinkLLVMLibsStep = struct {
        step: std.Build.Step,
        exe: *std.Build.Step.Compile,
        output_file: std.Build.LazyPath,

        pub fn make(step: *std.Build.Step, _: std.Build.Step.MakeOptions) !void {
            const self: *@This() = @fieldParentPtr("step", step);

            const b_llvm = self.step.owner;
            const file_path = self.output_file.getPath(b_llvm);

            const contents: []const u8 = try std.fs.cwd().readFileAlloc(b_llvm.allocator, file_path, std.math.maxInt(u32));
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

    const link_llvm_libs_step = try b.allocator.create(LinkLLVMLibsStep);
    link_llvm_libs_step.* = .{
        .step = std.Build.Step.init(.{
            .id = .custom,
            .name = "Link LLVM libraries from llvm-config",
            .owner = b,
            .makeFn = LinkLLVMLibsStep.make,
        }),
        .exe = exe,
        .output_file = llvm_config_cmd.captureStdOut(),
    };
    link_llvm_libs_step.step.dependOn(&llvm_config_cmd.step);
    exe.step.dependOn(&link_llvm_libs_step.step);
}

/// Create a no-op Run step that meets the return type requirements
fn makeEmptyStep(b: *std.Build) !*std.Build.Step.Run {
    const run_step = b.addSystemCommand(&[_][]const u8{ "zig", "version" });
    run_step.setName("make_empty_step");
    return run_step;
}

fn hasInternetConnection(b: *std.Build) bool {
    const s = std.net.tcpConnectToHost(b.allocator, "google.com", 443) catch return false;
    s.close();
    return true;
}
