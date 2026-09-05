const std = @import("std");

const makeEmptyStep = @import("../build.zig").makeEmptyStep;

pub const DEFAULT_LLVM_VERSION = "llvmorg-22.1.8";

pub fn build(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    prev_step: *std.Build.Step,
    prebuilt_dir: ?[]const u8,
    rebuild: bool,
    jobs: usize,
) !*std.Build.Step {
    if (prebuilt_dir != null) {
        return prev_step;
    }
    return try buildLLVM(b, prev_step, target, rebuild, jobs, prebuilt_dir);
}

pub fn update(b: *std.Build, llvm_version: []const u8) !*std.Build.Step {
    _ = b.findProgram(&.{"git"}, &.{}) catch @panic("Git not found on this system");

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

        return &checkout_llvm_cmd.step;
    } else |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, unable to clone dependency 'llvm-project'...\n", .{});
            return error.NoInternetConnection;
        }

        // 3. Clone llvm
        const clone_llvm_step = b.addSystemCommand(&[_][]const u8{ "git", "clone", "--depth", "1", "--branch", llvm_version, "https://github.com/llvm/llvm-project.git", "vendor/sources/llvm-project" });
        clone_llvm_step.setName("clone_llvm");

        return &clone_llvm_step.step;
    }
}

pub fn link(
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

fn buildLLVM(
    b: *std.Build,
    previous_step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    force_rebuild: bool,
    jobs: usize,
    external_llvm_dir: ?[]const u8,
) !*std.Build.Step {
    _ = b.findProgram(&.{"ld.lld"}, &.{}) catch @panic("LLD not found on this system");
    _ = b.findProgram(&.{"cmake"}, &.{}) catch @panic("CMake not found on this system");
    _ = b.findProgram(&.{"ninja"}, &.{}) catch @panic("Ninja not found on this system");
    _ = b.findProgram(&.{"python"}, &.{}) catch @panic("Python3 not found on this system");

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

    return &install_runs[install_runs.len - 1].step;
}

fn hasInternetConnection(b: *std.Build) bool {
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
