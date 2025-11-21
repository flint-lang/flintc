const std = @import("std");
const builtin = @import("builtin");

const DEFAULT_LLVM_VERSION = "llvmorg-19.1.7";

const FIP_HASH = "3710c353493bea6a6e5ade363bde6fb2e2285cb5";
const JSON_MINI_HASH = "a32d6e8319d90f5fa75f1651f30798c71464e4c6";

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const llvm_version = b.option(
        []const u8,
        "llvm-version",
        b.fmt("LLVM version to use. Default: {s}", .{DEFAULT_LLVM_VERSION}),
    ) orelse DEFAULT_LLVM_VERSION;

    const rebuild_llvm = b.option(
        bool,
        "rebuild-llvm",
        "Force rebuild LLVM",
    ) orelse false;

    std.fs.cwd().access("vendor", .{}) catch {
        try std.fs.cwd().makeDir("vendor");
    };

    const update_llvm = try updateLLVM(b, llvm_version);
    const update_fip = try updateFip(b);
    update_fip.step.dependOn(&update_llvm.step);
    const update_json_mini = try updateJsonMini(b);
    update_json_mini.step.dependOn(&update_fip.step);
    const update_zlib = try updateZlib(b);
    update_zlib.step.dependOn(&update_json_mini.step);

    const build_zlib = try buildZlib(
        b,
        switch (target.result.os.tag) {
            .linux => .linux,
            .windows => .windows,
            else => return error.TargetNeedsToBeLinuxOrWindows,
        },
        &update_zlib.step,
    );
    build_zlib.step.dependOn(&update_zlib.step);

    const build_llvm = try buildLLVM(
        b,
        switch (target.result.os.tag) {
            .linux => .linux,
            .windows => .windows,
            else => return error.TargetNeedsToBeLinuxOrWindows,
        },
        &build_zlib.step,
        rebuild_llvm,
    );
    build_llvm.step.dependOn(&update_llvm.step);

    try buildCompiler(b, target, optimize, &build_llvm.step);
}

fn buildCompiler(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    previous_step: *std.Build.Step,
) !void {
    std.debug.print("-- Building the 'flintc' compiler...\n", .{});
    const exe = b.addExecutable(.{
        .name = "flintc",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
        }),
    });
    exe.step.dependOn(previous_step);
    b.installArtifact(exe);
    if (optimize == .Debug) {
        exe.root_module.addCMacro("DEBUG_BUILD", "");
    }

    // Add commit hash
    const commit_hash: []const u8 = blk: {
        const commit_hash = b.run(&[_][]const u8{ "git", "rev-parse", "--short", "HEAD" });
        break :blk std.mem.trim(u8, commit_hash, &std.ascii.whitespace);
    };
    std.debug.print("-- Commit Hash is '{s}'...\n", .{commit_hash});
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
    std.debug.print("-- Build Date is {s}...\n", .{build_date});
    exe.root_module.addCMacro("BUILD_DATE", build_date);

    // Add LLVM paths
    const llvm_dir = switch (target.result.os.tag) {
        .linux => "vendor/llvm-linux",
        .windows => "vendor/llvm-mingw",
        else => return error.TargetNeedsToBeLinuxOrWindows,
    };

    // Include paths
    exe.root_module.addIncludePath(b.path(b.fmt("{s}/include", .{llvm_dir})));
    exe.root_module.addIncludePath(b.path("vendor/sources/fip"));
    exe.root_module.addIncludePath(b.path("vendor/sources/json-mini/include"));
    exe.root_module.addIncludePath(b.path("tests"));
    exe.root_module.addIncludePath(b.path("include"));

    // Library paths
    exe.root_module.addLibraryPath(b.path(b.fmt("{s}/lib", .{llvm_dir})));
    exe.root_module.addLibraryPath(switch (target.result.os.tag) {
        .linux => b.path("vendor/zlib-linux/lib"),
        .windows => b.path("vendor/zlib-mingw/lib"),
        else => return error.TargetNeedsToBeLinuxOrWindows,
    });

    var dir: std.fs.Dir = try std.fs.cwd().openDir("src", .{ .iterate = true });
    defer dir.close();
    var cpp_files: std.ArrayList([]const u8) = .empty;
    defer cpp_files.deinit(b.allocator);
    try collectCPPFiles(dir, "src", b, &cpp_files);

    // Add compiler flags and standard libraries
    exe.root_module.addCSourceFiles(.{
        .files = cpp_files.items,
        .flags = &[_][]const u8{
            "-std=c++17",
            "-stdlib=libc++",
            "-D_GNU_SOURCE",
            "-D__STDC_CONSTANT_MACROS",
            "-D__STDC_FORMAT_MACROS",
            "-D__STDC_LIMIT_MACROS",
        },
    });
    exe.root_module.addCSourceFile(.{
        .file = b.path("vendor/sources/fip/toml/tomlc17.c"),
        .flags = &[_][]const u8{
            "-D_GNU_SOURCE",
            "-D__STDC_CONSTANT_MACROS",
            "-D__STDC_FORMAT_MACROS",
            "-D__STDC_LIMIT_MACROS",
        },
    });

    // Library linking
    if (target.result.os.tag == .linux) {
        exe.root_module.linkSystemLibrary("rt", .{});
        exe.root_module.linkSystemLibrary("dl", .{});
    }
    exe.root_module.linkSystemLibrary("m", .{});
    exe.root_module.linkSystemLibrary("z", .{});
    exe.root_module.linkSystemLibrary("pthread", .{});
    exe.root_module.linkSystemLibrary("lldCOFF", .{});
    exe.root_module.linkSystemLibrary("lldELF", .{});
    exe.root_module.linkSystemLibrary("lldCommon", .{});
    for (LLVM_LIBS) |lib| {
        exe.root_module.linkSystemLibrary(lib, .{});
    }

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}

fn updateLLVM(b: *std.Build, llvm_version: []const u8) !*std.Build.Step.Run {
    std.debug.print("-- Updating the 'llvm-project' repository\n", .{});
    // 1. Check if llvm-project exists in vendor directory
    if (std.fs.cwd().openDir("vendor/sources/llvm-project", .{})) |_| {
        // 2. llvm is cloned, just continue
        return try makeEmptyStep(b);
    } else |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, unable to clone dependency 'llvm-project'...\n", .{});
            return error.NoInternetConnection;
        }
        // 3. Clone llvm
        const clone_llvm_step = b.addSystemCommand(&[_][]const u8{ "git", "clone", "--depth", "1", "--branch", llvm_version, "https://github.com/llvm/llvm-project.git", "vendor/sources/llvm-project" });
        return clone_llvm_step;
    }
}

fn updateFip(b: *std.Build) !*std.Build.Step.Run {
    // 1. Check if fip exists in vendor directory
    std.debug.print("-- Updating the 'fip' repository\n", .{});
    if (std.fs.cwd().openDir("vendor/sources/fip", .{})) |_| {
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'fip'...\n", .{});
            return makeEmptyStep(b);
        }
        // 2. Checkout fip main
        const checkout_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-f", "main" });
        checkout_fip_cmd.setCwd(b.path("vendor/sources/fip"));

        // 3. Reset hard
        const reset_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard" });
        reset_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        reset_fip_cmd.step.dependOn(&checkout_fip_cmd.step);

        // 4. Fetch fip
        const fetch_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "fetch" });
        fetch_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        fetch_fip_cmd.step.dependOn(&reset_fip_cmd.step);

        // 5. Pull fip
        const pull_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "pull" });
        pull_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        pull_fip_cmd.step.dependOn(&fetch_fip_cmd.step);

        // 6. Checkout fip hash
        const checkout_fip_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", FIP_HASH });
        checkout_fip_hash_cmd.setCwd(b.path("vendor/sources/fip"));
        checkout_fip_hash_cmd.step.dependOn(&pull_fip_cmd.step);
        return checkout_fip_hash_cmd;
    } else |_| {
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, unable to clone dependency 'fip'...\n", .{});
            return error.NoInternetConnection;
        }
        // 2. Clone fip
        const fetch_fip_complete_step = b.addSystemCommand(&[_][]const u8{ "git", "clone", "https://github.com/flint-lang/fip.git", "vendor/sources/fip" });

        // 3. Checkout fip hash
        const checkout_fip_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", "main" });
        checkout_fip_hash_cmd.setCwd(b.path("vendor/sources/fip"));
        checkout_fip_hash_cmd.step.dependOn(&fetch_fip_complete_step.step);
        return checkout_fip_hash_cmd;
    }
}

fn updateJsonMini(b: *std.Build) !*std.Build.Step.Run {
    // 1. Check if json-mini exists in vendor directory
    std.debug.print("-- Updating the 'json-mini' repository\n", .{});
    if (std.fs.cwd().openDir("vendor/sources/json-mini", .{})) |_| {
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'json-mini'...\n", .{});
            return makeEmptyStep(b);
        }
        // 2. Checkout json-mini main
        const checkout_json_mini_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-f", "main" });
        checkout_json_mini_cmd.setCwd(b.path("vendor/sources/json-mini"));

        // 3. Reset hard
        const reset_json_mini_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard" });
        reset_json_mini_cmd.setCwd(b.path("vendor/sources/json-mini"));
        reset_json_mini_cmd.step.dependOn(&checkout_json_mini_cmd.step);

        // 4. Fetch json-mini
        const fetch_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "fetch" });
        fetch_json_mini_step.setCwd(b.path("vendor/sources/json-mini"));
        fetch_json_mini_step.step.dependOn(&reset_json_mini_cmd.step);

        // 5. Pull json-mini
        const pull_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "pull" });
        pull_json_mini_step.setCwd(b.path("vendor/sources/json-mini"));
        pull_json_mini_step.step.dependOn(&fetch_json_mini_step.step);

        // 6. Checkout fip hash
        const checkout_json_mini_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", JSON_MINI_HASH });
        checkout_json_mini_hash_cmd.setCwd(b.path("vendor/sources/json-mini"));
        checkout_json_mini_hash_cmd.step.dependOn(&pull_json_mini_step.step);
        return checkout_json_mini_hash_cmd;
    } else |_| {
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, unable to clone dependency 'json-mini'...\n", .{});
            return error.NoInternetConnection;
        }
        // 2. Clone json-mini
        const clone_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "clone", "https://github.com/flint-lang/json-mini.git", "vendor/sources/json-mini" });

        // 3. Checkout fip hash
        const checkout_json_mini_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", JSON_MINI_HASH });
        checkout_json_mini_hash_cmd.setCwd(b.path("vendor/sources/json-mini"));
        checkout_json_mini_hash_cmd.step.dependOn(&clone_json_mini_step.step);
        return checkout_json_mini_hash_cmd;
    }
}

fn updateZlib(b: *std.Build) !*std.Build.Step.Run {
    // 1. Check if zlib exists in vendor directory
    std.debug.print("-- Updating the 'zlib' repository\n", .{});
    if (std.fs.cwd().openDir("vendor/sources/zlib", .{})) |_| {
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'zlib'...\n", .{});
            return makeEmptyStep(b);
        }
        // 2. Fetch zlib
        const fetch_zlib_step = b.addSystemCommand(&[_][]const u8{ "git", "fetch" });
        fetch_zlib_step.setCwd(b.path("vendor/sources/zlib"));

        // 3. Pull zlib
        const pull_zlib_step = b.addSystemCommand(&[_][]const u8{ "git", "pull" });
        pull_zlib_step.setCwd(b.path("vendor/sources/zlib"));
        pull_zlib_step.step.dependOn(&fetch_zlib_step.step);
        return pull_zlib_step;
    } else |_| {
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, unable to clone dependency 'zlib'...\n", .{});
            return error.NoInternetConnection;
        }
        // 2. Clone zlib
        const clone_zlib_step = b.addSystemCommand(&[_][]const u8{ "git", "clone", "https://github.com/madler/zlib.git", "vendor/sources/zlib" });
        return clone_zlib_step;
    }
}

fn buildZlib(b: *std.Build, target: std.Target.Os.Tag, previous_step: *std.Build.Step) !*std.Build.Step.Run {
    const build_name: []const u8 = switch (target) {
        .linux => "linux",
        .windows => "mingw",
        else => return error.TargetNeedsToBeLinuxOrWindows,
    };
    const root_dir = try std.fs.cwd().realpathAlloc(b.allocator, ".");
    defer b.allocator.free(root_dir);
    const zlib_build_dir = b.fmt("{s}/.zig-cache/zlib-{s}", .{ root_dir, build_name });
    const install_dir = b.fmt("{s}/vendor/zlib-{s}", .{ root_dir, build_name });

    if (std.fs.cwd().openDir(install_dir, .{})) |_| {
        // Zlib is already built, no need to build it again
        return try makeEmptyStep(b);
    } else |_| {}

    std.debug.print("-- Building Zlib for {s}\n", .{build_name});

    // Setup Zlib
    const setup_main = b.addSystemCommand(&[_][]const u8{
        "cmake",
        "-S",
        "vendor/sources/zlib",
        "-B",
        zlib_build_dir,
        b.fmt("-DCMAKE_INSTALL_PREFIX={s}", .{install_dir}),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DZLIB_BUILD_SHARED=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        "-DCMAKE_C_FLAGS=-fPIC",
    });
    setup_main.step.dependOn(previous_step);
    // Build Zlib
    const build_zlib = b.addSystemCommand(&[_][]const u8{ "cmake", "--build", zlib_build_dir, b.fmt("-j{d}", .{try std.Thread.getCpuCount() - 1}) });
    build_zlib.step.dependOn(&setup_main.step);
    // Install Zlib
    const install_zlib = b.addSystemCommand(&[_][]const u8{ "cmake", "--install", zlib_build_dir });
    install_zlib.step.dependOn(&build_zlib.step);

    return install_zlib;
}

fn buildLLVM(b: *std.Build, target: std.Target.Os.Tag, previous_step: *std.Build.Step, force_rebuild: bool) !*std.Build.Step.Run {
    const build_name: []const u8 = switch (target) {
        .linux => "linux",
        .windows => "mingw",
        else => return error.TargetNeedsToBeLinuxOrWindows,
    };
    const root_dir = try std.fs.cwd().realpathAlloc(b.allocator, ".");
    defer b.allocator.free(root_dir);
    const llvm_build_dir = b.fmt("{s}/.zig-cache/llvm-{s}", .{ root_dir, build_name });
    const runtimes_build_dir = b.fmt("{s}/runtimes", .{llvm_build_dir});
    const install_dir = b.fmt("{s}/vendor/llvm-{s}", .{ root_dir, build_name });
    const zlib_library_path: []const u8 = switch (target) {
        .linux => b.fmt("{s}/vendor/zlib-linux", .{root_dir}),
        .windows => b.fmt("{s}/vendor/zlib-mingw", .{root_dir}),
        else => return error.TargetNeedsToBeLinuxOrWindows,
    };

    if (std.fs.cwd().openDir(install_dir, .{})) |_| {
        // LLVM is already built, no need to build it again
        if (!force_rebuild) {
            return try makeEmptyStep(b);
        }
    } else |_| {}

    std.debug.print("-- Building LLVM for {s}\n", .{build_name});

    // Setup LLVM runtimes
    const setup_runtimes = b.addSystemCommand(&[_][]const u8{
        "cmake",
        "-S",
        "vendor/sources/llvm-project/runtimes",
        "-B",
        runtimes_build_dir,
        b.fmt("-DCMAKE_INSTALL_PREFIX={s}", .{install_dir}),
        "-DCMAKE_BUILD_TYPE=Release",
        b.fmt("-DCMAKE_C_COMPILER={s}", .{switch (target) {
            .linux => "zig;cc",
            .windows => "zig;cc", // Just use zig cc, CMake handles the rest
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        b.fmt("-DCMAKE_CXX_COMPILER={s}", .{switch (target) {
            .linux => "zig;c++",
            .windows => "zig;c++", // Just use zig c++, CMake handles the rest
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        b.fmt("-DCMAKE_ASM_COMPILER={s}", .{switch (target) {
            .linux => "zig;cc",
            .windows => "zig;cc", // Just use zig cc, CMake handles the rest
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        b.fmt("-DCMAKE_SYSTEM_NAME={s}", .{switch (target) {
            .linux => "Linux",
            .windows => "Windows",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        "-DLLVM_ENABLE_RUNTIMES=libcxx;libcxxabi",
        "-DLLVM_ENABLE_PIC=ON",
        // Static library settings
        "-DLIBCXX_ENABLE_SHARED=OFF",
        "-DLIBCXXABI_ENABLE_SHARED=OFF",
        // Runtime configuration
        "-DLIBCXX_CXX_ABI=libcxxabi",
        "-DLIBCXXABI_USE_LLVM_UNWINDER=OFF",
        "-DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON",
    });
    setup_runtimes.step.dependOn(previous_step);
    // Build LLVM runtimes
    const build_runtime = b.addSystemCommand(&[_][]const u8{ "cmake", "--build", runtimes_build_dir, b.fmt("-j{d}", .{try std.Thread.getCpuCount() - 2}) });
    build_runtime.step.dependOn(&setup_runtimes.step);
    // Install LLVM runtimes
    const install_runtime = b.addSystemCommand(&[_][]const u8{ "cmake", "--install", runtimes_build_dir });
    install_runtime.step.dependOn(&build_runtime.step);

    // Setup main LLVM
    const setup_main = b.addSystemCommand(&[_][]const u8{
        "cmake",
        "-S",
        "vendor/sources/llvm-project/llvm",
        "-B",
        llvm_build_dir,
        b.fmt("-DCMAKE_INSTALL_PREFIX={s}", .{install_dir}),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_C_COMPILER=zig;cc",
        "-DCMAKE_CXX_COMPILER=zig;c++",
        "-DCMAKE_ASM_COMPILER=zig;cc",
        b.fmt("-DCMAKE_SYSTEM_NAME={s}", .{switch (target) {
            .linux => "Linux",
            .windows => "Windows",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        // Force use of our libc++
        b.fmt("-DCMAKE_EXE_LINKER_FLAGS=-L{s}/lib -stdlib=libc++ -lc++abi", .{install_dir}),
        // Minimal LLVM config for static builds
        "-DLLVM_ENABLE_PROJECTS=lld;clang",
        "-DLLVM_ENABLE_RTTI=ON",
        "-DLLVM_ENABLE_EH=ON",
        "-DLLVM_BUILD_STATIC=ON",
        "-DLLVM_ENABLE_PIC=ON",
        "-DLLVM_TARGET_ARCH=X86",
        "-DLLVM_TARGETS_TO_BUILD=X86",
        // Disable what we don't need
        "-DLLVM_BUILD_TESTS=OFF",
        "-DLLVM_INCLUDE_TESTS=OFF",
        "-DLLVM_BUILD_EXAMPLES=OFF",
        "-DLLVM_INCLUDE_EXAMPLES=OFF",
        "-DLLVM_BUILD_BENCHMARKS=OFF",
        "-DLLVM_INCLUDE_BENCHMARKS=OFF",
        "-DLLVM_TOOL_LLVM_CONFIG_BUILD=ON",
        // External dependencies
        "-DLLVM_ENABLE_ZLIB=FORCE_ON",
        "-DZLIB_USE_STATIC_LIBS=ON",
        "-DLLVM_ENABLE_ZSTD=OFF",
        "-DLLVM_ENABLE_LIBXML2=OFF",
        "-DCMAKE_LINK_DEPENDS_USE_LINKER=OFF",
        //
        "-DCLANG_BUILD_TOOLS=OFF",
        "-DCLANG_INCLUDE_TESTS=OFF",
        "-DCLANG_INCLUDE_DOCS=OFF",
        "-DCLANG_BUILD_EXAMPLES=OFF",
        "-DCLANG_ENABLE_ARCMT=OFF",
        "-DCLANG_ENABLE_STATIC_ANALYZER=OFF",
        "-DCLANG_ENABLE_OBJC_REWRITER=OFF",
        "-DCLANG_PLUGIN_SUPPORT=OFF",
        "-DCLANG_TOOL_C_INDEX_TEST_BUILD=OFF",
        "-DLIBCLANG_BUILD_STATIC=ON",
        "-DCLANG_ENABLE_BOOTSTRAP=OFF",
        "-DCLANG_TOOL_CLANG_SHLIB_BUILD=OFF",
        //
        "-DBUILD_SHARED_LIBS=OFF",
        "-DLLVM_BUILD_LLVM_DYLIB=OFF",
        "-DLLVM_LINK_LLVM_DYLIB=OFF",
        "-DCLANG_LINK_CLANG_DYLIB=OFF",
        "-DCMAKE_AR=/usr/bin/ar",
        "-DCMAKE_RANLIB=/usr/bin/ranlib",
        "-DCROSS_TOOLCHAIN_FLAGS_NATIVE=-DCMAKE_C_COMPILER=cc;-DCMAKE_CXX_COMPILER=c++;-DCMAKE_AR=/usr/bin/ar;-DCMAKE_RANLIB=/usr/bin/ranlib",
        // Testing stuff
        // "-DCMAKE_VERBOSE_MAKEFILE=ON", // Increased build log verbosity
        // "-DLLVM_PARALLEL_COMPILE_JOBS=4",
        // "-DLLVM_PARALLEL_LINK_JOBS=2",
        switch (target) {
            .linux => b.fmt("-DZLIB_LIBRARY={s}/lib/libz.a", .{zlib_library_path}),
            .windows => b.fmt("-DZLIB_LIBRARY={s}/lib/libz.a", .{zlib_library_path}),
            else => return error.TargetNeedsToBeLinuxOrWindows,
        },
        switch (target) {
            .linux => b.fmt("-DZLIB_INCLUDE_DIRS={s}/include", .{zlib_library_path}),
            .windows => b.fmt("-DZLIB_INCLUDE_DIRS={s}/include", .{zlib_library_path}),
            else => return error.TargetNeedsToBeLinuxOrWindows,
        },
    });
    setup_main.step.dependOn(&install_runtime.step);
    // Build main LLVM
    const build_main = b.addSystemCommand(&[_][]const u8{ "cmake", "--build", llvm_build_dir, b.fmt("-j{d}", .{try std.Thread.getCpuCount() - 2}) });
    build_main.step.dependOn(&setup_main.step);
    // Install main LLVM
    const install_main = b.addSystemCommand(&[_][]const u8{ "cmake", "--install", llvm_build_dir });
    install_main.step.dependOn(&build_main.step);

    return install_main;
}

fn collectCPPFiles(dir: std.fs.Dir, dir_path: []const u8, b: *std.Build, files: *std.ArrayList([]const u8)) !void {
    var iter = dir.iterate();
    while (try iter.next()) |entry| {
        if (entry.kind == .file) {
            if (std.mem.endsWith(u8, entry.name, ".cpp")) {
                const full_path = try std.fs.path.join(b.allocator, &.{ dir_path, entry.name });
                try files.append(b.allocator, full_path);
            }
        } else if (entry.kind == .directory) {
            const sub_path = try std.fs.path.join(b.allocator, &.{ dir_path, entry.name });
            var sub_dir = try dir.openDir(entry.name, .{ .iterate = true });
            defer sub_dir.close();
            try collectCPPFiles(sub_dir, sub_path, b, files);
        }
    }
}

/// Create a no-op Run step that meets the return type requirements
/// 'true' is a command that does nothing and returns success
fn makeEmptyStep(b: *std.Build) !*std.Build.Step.Run {
    const run_step = b.addSystemCommand(&[_][]const u8{"true"});
    return run_step;
}

fn hasInternetConnection(b: *std.Build) bool {
    const s = std.net.tcpConnectToHost(b.allocator, "google.com", 443) catch return false;
    s.close();
    return true;
}

// Generated with "llvm-config --link-static --libs all"
const LLVM_LIBS = [_][]const u8{
    "LLVMWindowsManifest",    "LLVMXRay",                 "LLVMLibDriver",
    "LLVMDlltoolDriver",      "LLVMTextAPIBinaryReader",  "LLVMCoverage",
    "LLVMLineEditor",         "LLVMSandboxIR",            "LLVMX86TargetMCA",
    "LLVMX86Disassembler",    "LLVMX86AsmParser",         "LLVMX86CodeGen",
    "LLVMX86Desc",            "LLVMX86Info",              "LLVMOrcDebugging",
    "LLVMOrcJIT",             "LLVMWindowsDriver",        "LLVMMCJIT",
    "LLVMJITLink",            "LLVMInterpreter",          "LLVMExecutionEngine",
    "LLVMRuntimeDyld",        "LLVMOrcTargetProcess",     "LLVMOrcShared",
    "LLVMDWP",                "LLVMDebugInfoLogicalView", "LLVMDebugInfoGSYM",
    "LLVMOption",             "LLVMObjectYAML",           "LLVMObjCopy",
    "LLVMMCA",                "LLVMMCDisassembler",       "LLVMLTO",
    "LLVMPasses",             "LLVMHipStdPar",            "LLVMCFGuard",
    "LLVMCoroutines",         "LLVMipo",                  "LLVMVectorize",
    "LLVMLinker",             "LLVMInstrumentation",      "LLVMFrontendOpenMP",
    "LLVMFrontendOffloading", "LLVMFrontendOpenACC",      "LLVMFrontendHLSL",
    "LLVMFrontendDriver",     "LLVMExtensions",           "LLVMDWARFLinkerParallel",
    "LLVMDWARFLinkerClassic", "LLVMDWARFLinker",          "LLVMCodeGenData",
    "LLVMGlobalISel",         "LLVMMIRParser",            "LLVMAsmPrinter",
    "LLVMSelectionDAG",       "LLVMCodeGen",              "LLVMTarget",
    "LLVMObjCARCOpts",        "LLVMCodeGenTypes",         "LLVMIRPrinter",
    "LLVMInterfaceStub",      "LLVMFileCheck",            "LLVMFuzzMutate",
    "LLVMScalarOpts",         "LLVMInstCombine",          "LLVMAggressiveInstCombine",
    "LLVMTransformUtils",     "LLVMBitWriter",            "LLVMAnalysis",
    "LLVMProfileData",        "LLVMSymbolize",            "LLVMDebugInfoBTF",
    "LLVMDebugInfoPDB",       "LLVMDebugInfoMSF",         "LLVMDebugInfoDWARF",
    "LLVMObject",             "LLVMTextAPI",              "LLVMMCParser",
    "LLVMIRReader",           "LLVMAsmParser",            "LLVMMC",
    "LLVMDebugInfoCodeView",  "LLVMBitReader",            "LLVMFuzzerCLI",
    "LLVMCore",               "LLVMRemarks",              "LLVMBitstreamReader",
    "LLVMBinaryFormat",       "LLVMTargetParser",         "LLVMTableGen",
    "LLVMSupport",            "LLVMDemangle",
};
