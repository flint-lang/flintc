const std = @import("std");
const builtin = @import("builtin");

const Platform = enum { linux, windows };

var platform: Platform = Platform.linux;

const llvm_version = "llvmorg-19.1.7";

pub fn build(b: *std.Build) !void {
    platform = b.option(Platform, "platform", "Select Platform") orelse Platform.linux;

    const target = switch (platform) {
        .linux => b.resolveTargetQuery(.{ .cpu_arch = .x86_64, .os_tag = .linux }),
        .windows => b.resolveTargetQuery(.{ .cpu_arch = .x86_64, .os_tag = .windows }),
    };
    const optimize = b.standardOptimizeOption(.{});

    var vendor_dir = blk: {
        if (std.fs.cwd().openDir("vendor", .{})) |dir| {
            break :blk dir;
        } else |err| {
            std.debug.print("'vendor' path does not exist, creating it now (err: {any})..\n", .{err});
            std.fs.cwd().makeDir("vendor") catch unreachable;
            break :blk std.fs.cwd().openDir("vendor", .{}) catch unreachable;
        }
    };
    defer vendor_dir.close();
    const update_llvm = try updateLLVM(b);
    const update_json_mini = try updateJsonMini(b);
    update_json_mini.step.dependOn(&update_llvm.step);

    const build_llvm = try buildLLVM(b, switch (target.result.os.tag) {
        .linux => .linux,
        .windows => .windows,
        else => return error.TargetNeedsToBeLinuxOrWindows,
    });
    build_llvm.step.dependOn(&update_json_mini.step);
    try buildCompiler(b, target, optimize, &build_llvm.step);
}

fn buildCompiler(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    build_llvm_step: *std.Build.Step,
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
    exe.step.dependOn(build_llvm_step);
    b.installArtifact(exe);
    if (optimize == .Debug) {
        exe.root_module.addCMacro("DEBUG_BUILD", "");
    }

    // Add Commit Hash
    const commit_hash = executeSystemCommand(b, &[_][]const u8{ "git", "rev-parse", "--short", "HEAD" }) catch "unknown";
    std.debug.print("-- Commit Hash is '{s}'...\n", .{commit_hash});
    exe.root_module.addCMacro("COMMIT_HASH", b.fmt("\"{s}\"", .{commit_hash}));

    // Add date
    const current_timestamp = std.time.timestamp();
    const epoch_seconds = std.time.epoch.EpochSeconds{ .secs = @as(u64, @intCast(current_timestamp)) };
    const epoch_day = epoch_seconds.getEpochDay();
    const year_day = epoch_day.calculateYearDay();
    const month_day = year_day.calculateMonthDay();
    const build_date = b.fmt("\"{d}-{d:0>2}-{d:0>2}\"", .{
        year_day.year,
        month_day.month.numeric(),
        month_day.day_index + 1, // day_index is 0-based
    });
    std.debug.print("-- Build Date is '{s}'...\n", .{build_date});
    exe.root_module.addCMacro("BUILD_DATE", build_date);

    var cpp_files = std.ArrayList([]const u8).init(b.allocator);
    defer cpp_files.deinit();

    var dir = try std.fs.cwd().openDir("src", .{ .iterate = true });
    defer dir.close();
    try collectCPPFiles(dir, "src", b, &cpp_files);

    // Add LLVM paths
    exe.addLibraryPath(b.path("vendor/llvm-linux/lib"));
    exe.addIncludePath(b.path("vendor/llvm-linux/include"));

    // Create the llvm-config step
    const llvm_config_step = b.addSystemCommand(&[_][]const u8{ "./vendor/llvm-linux/bin/llvm-config", "--link-static", "--libs", "all" });
    llvm_config_step.step.dependOn(build_llvm_step);

    // Capture the output to a lazy path
    const llvm_config_output = llvm_config_step.captureStdOut();

    // Create a simple step that reads the output and gives you the string
    const process_output_step = createStringProcessorStep(b, llvm_config_output, exe);
    process_output_step.dependOn(&llvm_config_step.step);
    exe.step.dependOn(process_output_step);

    exe.linkSystemLibrary("lldCOFF");
    exe.linkSystemLibrary("lldELF");
    exe.linkSystemLibrary("lldCommon");

    // Add compiler flags and standard libraries
    exe.addCSourceFiles(.{
        .files = cpp_files.items,
        .flags = &.{
            "-std=c++17",
            "-stdlib=libc++",
            "-D_GNU_SOURCE",
            "-D__STDC_CONSTANT_MACROS",
            "-D__STDC_FORMAT_MACROS",
            "-D__STDC_LIMIT_MACROS",
        },
    });

    // Additional system libraries
    exe.linkSystemLibrary("rt");
    exe.linkSystemLibrary("dl");
    exe.linkSystemLibrary("m");
    exe.linkSystemLibrary("z");

    // Project includes
    exe.addIncludePath(b.path("vendor/json-mini/include"));
    exe.addIncludePath(b.path("tests"));
    exe.addIncludePath(b.path("include"));

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}

fn updateLLVM(b: *std.Build) !*std.Build.Step.Run {
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
        const clone_llvm_step = b.addSystemCommand(&[_][]const u8{ "git", "clone", "--depth", "1", "--branch", llvm_version, "https://github.com/llvm/llvm-project.git" });
        clone_llvm_step.setCwd(b.path("vendor/sources"));
        return clone_llvm_step;
    }
}

fn updateJsonMini(b: *std.Build) !*std.Build.Step.Run {
    // 1. Check if json-mini exists in vendor directory
    std.debug.print("-- Updating the 'json-mini' repository\n", .{});
    if (std.fs.cwd().openDir("vendor/json-mini", .{})) |_| {
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'json-mini'...\n", .{});
            return makeEmptyStep(b);
        }
        // 2. Fetch json-mini
        const fetch_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "fetch" });
        fetch_json_mini_step.setCwd(b.path("vendor/json-mini"));

        // 3. Pull json-mini
        const pull_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "pull" });
        pull_json_mini_step.setCwd(b.path("vendor/json-mini"));
        pull_json_mini_step.step.dependOn(&fetch_json_mini_step.step);
        return pull_json_mini_step;
    } else |_| {
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, unable to clone dependency 'json-mini'...\n", .{});
            return error.NoInternetConnection;
        }
        // 2. Clone json-mini
        const clone_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "clone", "https://github.com/flint-lang/json-mini.git" });
        clone_json_mini_step.setCwd(b.path("vendor"));
        return clone_json_mini_step;
    }
}

fn buildLLVM(b: *std.Build, target: Platform) !*std.Build.Step.Run {
    const build_name: []const u8 = switch (target) {
        .linux => "linux",
        .windows => "mingw",
    };
    const root_dir = try std.fs.cwd().realpathAlloc(b.allocator, ".");
    const llvm_build_dir = b.fmt("{s}/.zig-cache/llvm-{s}", .{ root_dir, build_name });
    const runtimes_build_dir = b.fmt("{s}/runtimes", .{llvm_build_dir});
    const install_dir = b.fmt("{s}/vendor/llvm-{s}", .{ root_dir, build_name });

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
        "-DCMAKE_C_COMPILER=clang",
        "-DCMAKE_CXX_COMPILER=clang++",
        "-DLLVM_ENABLE_RUNTIMES=libunwind;libcxx;libcxxabi",
        "-DLLVM_ENABLE_PIC=ON",
        // Static library settings
        "-DLIBCXX_ENABLE_SHARED=OFF",
        "-DLIBCXX_ENABLE_STATIC=ON",
        "-DLIBCXXABI_ENABLE_SHARED=OFF",
        "-DLIBCXXABI_ENABLE_STATIC=ON",
        "-DLIBUNWIND_ENABLE_SHARED=OFF",
        "-DLIBUNWIND_ENABLE_STATIC=ON",
        // Runtime configuration (using Arch settings where appropriate)
        "-DLIBCXX_HAS_MUSL_LIBC=OFF",
        "-DLIBCXX_CXX_ABI=libcxxabi",
        "-DLIBCXXABI_USE_LLVM_UNWINDER=OFF", // Match Arch
        "-DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON",
    });
    // Build LLVM runtimes
    const build_runtime = b.addSystemCommand(&[_][]const u8{ "cmake", "--build", runtimes_build_dir, b.fmt("-j{d}", .{try std.Thread.getCpuCount() - 1}) });
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
        "-DCMAKE_C_COMPILER=clang",
        "-DCMAKE_CXX_COMPILER=clang++",
        // Force use of our libc++
        "-DCMAKE_CXX_FLAGS=-stdlib=libc++",
        b.fmt("-DCMAKE_EXE_LINKER_FLAGS=-L{s}/lib -stdlib=libc++ -lc++abi -lunwind", .{install_dir}),
        // Minimal LLVM config for static builds
        "-DLLVM_ENABLE_PROJECTS=lld",
        "-DLLVM_ENABLE_RTTI=ON",
        "-DLLVM_ENABLE_EH=ON",
        "-DLLVM_BUILD_STATIC=ON",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DLLVM_ENABLE_PIC=ON",
        "-DLLVM_TARGET_ARCH=X86",
        "-DLLVM_TARGETS_TO_BUILD=X86",
        // Disable what we don't need
        "-DLLVM_INCLUDE_TESTS=OFF",
        "-DLLVM_INCLUDE_EXAMPLES=OFF",
        "-DLLVM_INCLUDE_BENCHMARKS=OFF",
        "-DLLVM_BUILD_TOOLS=ON",
        "-DLLVM_INCLUDE_TOOLS=ON",
        "-DLLVM_TOOL_LLVM_CONFIG_BUILD=ON",
        "-DLLVM_BUILD_UTILS=OFF",
        "-DLLVM_INCLUDE_UTILS=OFF",
        // External dependencies
        "-DLLVM_ENABLE_ZLIB=FORCE_ON",
        "-DZLIB_USE_STATIC_LIBS=ON",
        "-DLLVM_ENABLE_ZSTD=OFF",
        "-DLLVM_ENABLE_LIBXML2=OFF",
        "-DZLIB_LIBRARY=/usr/lib/libz.a",
        "-DZLIB_INCLUDE_DIR=/usr/include",
    });
    setup_main.step.dependOn(&install_runtime.step);
    // Build main LLVM
    const build_main = b.addSystemCommand(&[_][]const u8{ "cmake", "--build", llvm_build_dir, b.fmt("-j{d}", .{try std.Thread.getCpuCount() - 1}) });
    build_main.step.dependOn(&setup_main.step);
    // Install main LLVM
    const install_main = b.addSystemCommand(&[_][]const u8{ "cmake", "--install", llvm_build_dir });
    install_main.step.dependOn(&build_main.step);

    return install_main;
}

fn executeSystemCommand(b: *std.Build, command: []const []const u8) ![]const u8 {
    const result = try std.process.Child.run(.{
        .allocator = b.allocator,
        .argv = command,
    });

    // Remove all newlines from the system comman
    return std.mem.trimRight(u8, result.stdout, &[_]u8{ '\n', '\r' });
}

fn collectCPPFiles(dir: std.fs.Dir, dir_path: []const u8, b: *std.Build, files: *std.ArrayList([]const u8)) !void {
    var iter = dir.iterate();
    while (try iter.next()) |entry| {
        if (entry.kind == .file) {
            if (std.mem.endsWith(u8, entry.name, ".cpp")) {
                const full_path = try std.fs.path.join(b.allocator, &.{ dir_path, entry.name });
                try files.append(full_path);
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

fn createStringProcessorStep(b: *std.Build, lazy_path: std.Build.LazyPath, exe: *std.Build.Step.Compile) *std.Build.Step {
    const ProcessorStep = struct {
        step: std.Build.Step,
        lazy_path: std.Build.LazyPath,
        exe: *std.Build.Step.Compile,

        fn make(s: *std.Build.Step, options: std.Build.Step.MakeOptions) !void {
            _ = options;
            const self: *@This() = @fieldParentPtr("step", s);

            // Read the captured output file
            const output_path = self.lazy_path.getPath(s.owner);
            const llvm_libs_string = try std.fs.cwd().readFileAlloc(s.owner.allocator, output_path, 1024 * 1024);
            defer s.owner.allocator.free(llvm_libs_string);

            // Now you have your string - do your manual processing here
            var llvm_libs = std.mem.splitScalar(u8, std.mem.trim(u8, llvm_libs_string, " \n\r\t"), ' ');
            while (llvm_libs.next()) |lib| {
                if (lib.len == 0) continue;
                if (lib.len >= 2 and std.mem.startsWith(u8, lib, "-l")) {
                    self.exe.linkSystemLibrary(lib[2..]);
                }
            }
        }
    };

    const processor_step = b.allocator.create(ProcessorStep) catch @panic("OOM");
    processor_step.* = ProcessorStep{
        .step = std.Build.Step.init(.{
            .id = .custom,
            .name = "process-llvm-output",
            .owner = b,
            .makeFn = ProcessorStep.make,
        }),
        .lazy_path = lazy_path,
        .exe = exe,
    };

    return &processor_step.step;
}
