const std = @import("std");

const DEFAULT_LLVM_VERSION = "llvmorg-21.1.8";

const FIP_HASH = "3710c353493bea6a6e5ade363bde6fb2e2285cb5";
const JSON_MINI_HASH = "a32d6e8319d90f5fa75f1651f30798c71464e4c6";

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    _ = b.findProgram(&.{"git"}, &.{}) catch @panic("Git not found on this system");
    _ = b.findProgram(&.{"cmake"}, &.{}) catch @panic("CMake not found on this system");
    _ = b.findProgram(&.{"ninja"}, &.{}) catch @panic("Ninja not found on this system");
    _ = b.findProgram(&.{"python"}, &.{}) catch @panic("Python3 not found on this system");

    const llvm_version = b.option([]const u8, "llvm-version", b.fmt("LLVM version to use. Default: {s}", .{DEFAULT_LLVM_VERSION})) orelse
        DEFAULT_LLVM_VERSION;
    const force_llvm_rebuild = b.option(bool, "force-llvm-rebuild", "Force rebuild LLVM") orelse
        false;

    // Update Json Mini
    const update_json_mini = try updateJsonMini(b);
    // Update Fip
    const update_fip = try updateFip(b);
    update_fip.step.dependOn(&update_json_mini.step);
    // Update LLVM
    const update_llvm = try updateLLVM(b, llvm_version);
    update_llvm.step.dependOn(&update_fip.step);
    // Build LLVM
    const build_llvm = try buildLLVM(b, target.result.os.tag, &update_llvm.step, force_llvm_rebuild);
    build_llvm.step.dependOn(&update_llvm.step);
    // Build flintc exe
    try buildFlintc(b, target, optimize, &build_llvm.step);
}

fn buildFlintc(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, previous_step: *std.Build.Step) !void {
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

    const run_step = b.step("run", "Run the app");
    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const llvm_dir = switch (target.result.os.tag) {
        .linux => "vendor/llvm-linux",
        .windows => "vendor/llvm-mingw",
        else => return error.TargetNeedsToBeLinuxOrWindows,
    };

    // Add Include paths
    exe.root_module.addIncludePath(b.path(b.fmt("{s}/include", .{llvm_dir})));
    // exe.root_module.addSystemIncludePath(b.path(b.fmt("{s}/include/c++/v1", .{llvm_dir})));
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

    // Add C++ src files
    exe.root_module.addCSourceFiles(.{
        .root = b.path("src"),
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

    // Add toml C src file for FIP
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
    if (target.result.os.tag == .windows) {
        exe.root_module.linkSystemLibrary("ole32", .{});
    }
    exe.root_module.linkSystemLibrary("m", .{});
    exe.root_module.linkSystemLibrary("lldCOFF", .{});
    exe.root_module.linkSystemLibrary("lldELF", .{});
    exe.root_module.linkSystemLibrary("lldCommon", .{});
    for (LLVM_LIBS) |lib| {
        exe.root_module.linkSystemLibrary(lib, .{});
    }
}

fn buildLLVM(b: *std.Build, target: std.Target.Os.Tag, previous_step: *std.Build.Step, force_rebuild: bool) !*std.Build.Step.Run {
    const build_name: []const u8 = switch (target) {
        .linux => "linux",
        .windows => "mingw",
        else => return error.TargetNeedsToBeLinuxOrWindows,
    };
    const root_dir: []const u8 = try std.fs.cwd().realpathAlloc(b.allocator, ".");
    defer b.allocator.free(root_dir);
    const llvm_build_dir = b.fmt("{s}/.zig-cache/llvm-{s}", .{ root_dir, build_name });
    const install_dir = b.fmt("{s}/vendor/llvm-{s}", .{ root_dir, build_name });

    if (std.fs.cwd().openDir(install_dir, .{})) |dir| {
        // LLVM is already built, rebuilt only if requested
        if (force_rebuild) {
            try std.fs.cwd().deleteTree(llvm_build_dir);
            try dir.deleteTree("");
        } else {
            return makeEmptyStep(b);
        }
    } else |_| {}

    std.debug.print("-- Building LLVM for {s}\n", .{build_name});

    // Setup main LLVM
    const setup_main = b.addSystemCommand(&[_][]const u8{
        "cmake",
        "-S",
        "vendor/sources/llvm-project/llvm",
        "-B",
        llvm_build_dir,
        if (target == .windows) "-G" else "",
        if (target == .windows) "Ninja" else "",
        b.fmt("-DCMAKE_INSTALL_PREFIX={s}", .{install_dir}),
        "-DCMAKE_BUILD_TYPE=Release",
        b.fmt("-DCMAKE_C_COMPILER={s}", .{switch (target) {
            .linux => "zig;cc",
            .windows => "zig;cc",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        b.fmt("-DCMAKE_CXX_COMPILER={s}", .{switch (target) {
            .linux => "zig;c++",
            .windows => "zig;c++",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        b.fmt("-DCMAKE_ASM_COMPILER={s}", .{switch (target) {
            .linux => "zig;cc",
            .windows => "zig;cc",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        }}),
        // b.fmt("-DCMAKE_SYSTEM_NAME={s}", .{switch (target) {
        //     .linux => "Linux",
        //     .windows => "Windows",
        //     else => return error.TargetNeedsToBeLinuxOrWindows,
        // }}),
        "-DLLVM_ENABLE_PROJECTS=lld;clang",
        "-DLLVM_ENABLE_LIBXML2=OFF",
        "-DLLVM_ENABLE_TERMINFO=OFF",
        "-DLLVM_ENABLE_LIBEDIT=OFF",
        "-DLLVM_ENABLE_ASSERTIONS=ON",
        b.fmt("-DLLVM_PARALLEL_LINK_JOBS={d}", .{try std.Thread.getCpuCount() / 2}),

        "-DLLVM_TARGET_ARCH=X86",
        "-DLLVM_TARGETS_TO_BUILD=X86",
        "-DLIBCLANG_BUILD_STATIC=ON",

        "-DLLVM_ENABLE_ZLIB=OFF",
        "-DLLVM_ENABLE_ZSTD=OFF",
        "-DLLVM_ENABLE_PIC=OFF", // To avoid "error: unsupported linker arg:", "-Bsymbolic-functions"

        // https://github.com/ziglang/zig/issues/23546
        // https://codeberg.org/ziglang/zig/pulls/30073
        "-DCMAKE_LINK_DEPENDS_USE_LINKER=FALSE",

        // "-DCMAKE_VERBOSE_MAKEFILE=ON", // Increased build log verbosity
        // "-DLLVM_PARALLEL_COMPILE_JOBS=4",
    });
    setup_main.setEnvironmentVariable("ASM", "zig;cc");
    setup_main.setEnvironmentVariable("CC", "zig;cc");
    setup_main.setEnvironmentVariable("CXX", "zig;cxx");
    setup_main.setName("llvm_setup");
    setup_main.step.dependOn(previous_step);
    // Build main LLVM
    const build_main = b.addSystemCommand(&[_][]const u8{ "cmake", "--build", llvm_build_dir, b.fmt("-j{d}", .{try std.Thread.getCpuCount() / 2}) });
    build_main.setName("llvm_build");
    build_main.step.dependOn(&setup_main.step);
    // Install main LLVM
    const install_main = b.addSystemCommand(&[_][]const u8{ "cmake", "--install", llvm_build_dir });
    install_main.setName("llvm_install");
    install_main.step.dependOn(&build_main.step);

    return install_main;
}

fn updateLLVM(b: *std.Build, llvm_version: []const u8) !*std.Build.Step.Run {
    std.debug.print("-- Updating the 'llvm-project' repository\n", .{});
    // 1. Check if llvm-project exists in vendor directory
    if (std.fs.cwd().openDir("vendor/sources/llvm-project", .{})) |_| {
        // 2. llvm is cloned, just continue
        return makeEmptyStep(b);
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
        checkout_fip_cmd.setName("checkout_fip");
        checkout_fip_cmd.setCwd(b.path("vendor/sources/fip"));

        // 3. Reset hard
        const reset_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard" });
        reset_fip_cmd.setName("reset_fip");
        reset_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        reset_fip_cmd.step.dependOn(&checkout_fip_cmd.step);

        // 4. Fetch fip
        const fetch_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "fetch" });
        fetch_fip_cmd.setName("fetch_fip");
        fetch_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        fetch_fip_cmd.step.dependOn(&reset_fip_cmd.step);

        // 5. Pull fip
        const pull_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "pull" });
        pull_fip_cmd.setName("pull_fip");
        pull_fip_cmd.setCwd(b.path("vendor/sources/fip"));
        pull_fip_cmd.step.dependOn(&fetch_fip_cmd.step);

        // 6. Checkout fip hash
        const checkout_fip_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", FIP_HASH });
        checkout_fip_hash_cmd.setName("checkout_fip_hash");
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
        fetch_fip_complete_step.setName("fetch_fip_complete");

        // 3. Checkout fip hash
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
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'json-mini'...\n", .{});
            return makeEmptyStep(b);
        }
        // 2. Checkout json-mini main
        const checkout_json_mini_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-f", "main" });
        checkout_json_mini_cmd.setName("checkout_json_mini");
        checkout_json_mini_cmd.setCwd(b.path("vendor/sources/json-mini"));

        // 3. Reset hard
        const reset_json_mini_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard" });
        reset_json_mini_cmd.setName("reset_json_mini");
        reset_json_mini_cmd.setCwd(b.path("vendor/sources/json-mini"));
        reset_json_mini_cmd.step.dependOn(&checkout_json_mini_cmd.step);

        // 4. Fetch json-mini
        const fetch_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "fetch" });
        fetch_json_mini_step.setName("fetch_json_mini");
        fetch_json_mini_step.setCwd(b.path("vendor/sources/json-mini"));
        fetch_json_mini_step.step.dependOn(&reset_json_mini_cmd.step);

        // 5. Pull json-mini
        const pull_json_mini_step = b.addSystemCommand(&[_][]const u8{ "git", "pull" });
        pull_json_mini_step.setName("pull_json_mini");
        pull_json_mini_step.setCwd(b.path("vendor/sources/json-mini"));
        pull_json_mini_step.step.dependOn(&fetch_json_mini_step.step);

        // 6. Checkout fip hash
        const checkout_json_mini_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", JSON_MINI_HASH });
        checkout_json_mini_hash_cmd.setName("checkout_json_mini_hash");
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
        clone_json_mini_step.setName("clone_json_mini");

        // 3. Checkout fip hash
        const checkout_json_mini_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", JSON_MINI_HASH });
        checkout_json_mini_hash_cmd.setName("checkout_json_mini_hash");
        checkout_json_mini_hash_cmd.setCwd(b.path("vendor/sources/json-mini"));
        checkout_json_mini_hash_cmd.step.dependOn(&clone_json_mini_step.step);
        return checkout_json_mini_hash_cmd;
    }
}

/// Create a no-op Run step that meets the return type requirements
/// 'true' is a command that does nothing and returns success
fn makeEmptyStep(b: *std.Build) !*std.Build.Step.Run {
    const run_step = b.addSystemCommand(&[_][]const u8{ "zig", "version" });
    return run_step;
}

fn hasInternetConnection(b: *std.Build) bool {
    const s = std.net.tcpConnectToHost(b.allocator, "google.com", 443) catch return false;
    s.close();
    return true;
}

/// Generated with "llvm-config --link-static --libs all"
const LLVM_LIBS = [_][]const u8{
    "LLVMWindowsManifest",    "LLVMXRay",             "LLVMLibDriver",
    "LLVMDlltoolDriver",      "LLVMTelemetry",        "LLVMTextAPIBinaryReader",
    "LLVMCoverage",           "LLVMLineEditor",       "LLVMX86TargetMCA",
    "LLVMX86Disassembler",    "LLVMX86AsmParser",     "LLVMX86CodeGen",
    "LLVMX86Desc",            "LLVMX86Info",          "LLVMOrcDebugging",
    "LLVMOrcJIT",             "LLVMWindowsDriver",    "LLVMMCJIT",
    "LLVMJITLink",            "LLVMInterpreter",      "LLVMExecutionEngine",
    "LLVMRuntimeDyld",        "LLVMOrcTargetProcess", "LLVMOrcShared",
    "LLVMDWP",                "LLVMDWARFCFIChecker",  "LLVMDebugInfoLogicalView",
    "LLVMOption",             "LLVMObjCopy",          "LLVMMCA",
    "LLVMMCDisassembler",     "LLVMLTO",              "LLVMPasses",
    "LLVMHipStdPar",          "LLVMCFGuard",          "LLVMCoroutines",
    "LLVMipo",                "LLVMVectorize",        "LLVMSandboxIR",
    "LLVMLinker",             "LLVMFrontendOpenMP",   "LLVMFrontendOffloading",
    "LLVMObjectYAML",         "LLVMFrontendOpenACC",  "LLVMFrontendHLSL",
    "LLVMFrontendDriver",     "LLVMInstrumentation",  "LLVMFrontendDirective",
    "LLVMFrontendAtomic",     "LLVMExtensions",       "LLVMDWARFLinkerParallel",
    "LLVMDWARFLinkerClassic", "LLVMDWARFLinker",      "LLVMGlobalISel",
    "LLVMMIRParser",          "LLVMAsmPrinter",       "LLVMSelectionDAG",
    "LLVMCodeGen",            "LLVMTarget",           "LLVMObjCARCOpts",
    "LLVMCodeGenTypes",       "LLVMCGData",           "LLVMIRPrinter",
    "LLVMInterfaceStub",      "LLVMFileCheck",        "LLVMFuzzMutate",
    "LLVMScalarOpts",         "LLVMInstCombine",      "LLVMAggressiveInstCombine",
    "LLVMTransformUtils",     "LLVMBitWriter",        "LLVMAnalysis",
    "LLVMProfileData",        "LLVMSymbolize",        "LLVMDebugInfoBTF",
    "LLVMDebugInfoPDB",       "LLVMDebugInfoMSF",     "LLVMDebugInfoCodeView",
    "LLVMDebugInfoGSYM",      "LLVMDebugInfoDWARF",   "LLVMDebugInfoDWARFLowLevel",
    "LLVMObject",             "LLVMTextAPI",          "LLVMMCParser",
    "LLVMIRReader",           "LLVMAsmParser",        "LLVMMC",
    "LLVMBitReader",          "LLVMFuzzerCLI",        "LLVMCore",
    "LLVMRemarks",            "LLVMBitstreamReader",  "LLVMBinaryFormat",
    "LLVMTargetParser",       "LLVMTableGen",         "LLVMSupport",
    "LLVMDemangle",
};
