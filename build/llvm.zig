const std = @import("std");

const makeEmptyStep = @import("../build.zig").makeEmptyStep;

pub fn build(
    b: *std.Build,
    previous_step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    force_rebuild: bool,
    jobs: usize,
    llvm_dep: *std.Build.Dependency,
    /// linux/mingw
    platform_name: []const u8,
    /// llvm-{ linux, mingw }
    llvm_platform_dirname: []const u8,
    /// vendor/llvm-{ linux, mingw }
    install_dir: []const u8,
) !*std.Build.Step {
    const cmake_exe_path: []const u8 = b.findProgram(&.{"cmake"}, &.{}) catch @panic("CMake not found on this system");
    _ = b.findProgram(&.{"ld.lld"}, &.{}) catch @panic("LLD not found on this system");
    _ = b.findProgram(&.{"ninja"}, &.{}) catch @panic("Ninja not found on this system");
    _ = b.findProgram(&.{ "python", "python3" }, &.{}) catch @panic("Python3 not found on this system");

    if (b.build_root.handle.openDir(b.graph.io, install_dir, .{})) |_| {
        // LLVM is already built, rebuilt only if requested
        if (force_rebuild) {
            try b.build_root.handle.deleteTree(b.graph.io, install_dir);
        } else {
            return makeEmptyStep(b);
        }
    } else |_| {}

    std.debug.print("-- Building LLVM for {s}\n", .{platform_name});

    // Setup LLVM
    const setup_llvm = std.Build.Step.Run.create(b, "llvm_setup");
    setup_llvm.addArg(cmake_exe_path);
    setup_llvm.addArg("-S");
    setup_llvm.addDirectoryArg(llvm_dep.path("llvm"));
    setup_llvm.addArg("-B");
    const llvm_build_dir = setup_llvm.addOutputDirectoryArg(llvm_platform_dirname);
    setup_llvm.addArgs(&.{ "-G", "Ninja" });
    setup_llvm.addArgs(&[_][]const u8{
        "-Wno-policy",
        b.fmt("-DCMAKE_INSTALL_PREFIX={s}", .{install_dir}),
        "-DCMAKE_BUILD_TYPE=MinSizeRel",
        b.fmt("-DCMAKE_C_COMPILER={s};cc;-target;{s}", .{ b.graph.zig_exe, switch (target.result.os.tag) {
            .linux => "x86_64-linux-musl",
            .windows => "x86_64-windows-gnu",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        } }),
        b.fmt("-DCMAKE_CXX_COMPILER={s};c++;-target;{s}", .{ b.graph.zig_exe, switch (target.result.os.tag) {
            .linux => "x86_64-linux-musl",
            .windows => "x86_64-windows-gnu",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        } }),
        b.fmt("-DCMAKE_ASM_COMPILER={s};cc;-target;{s}", .{ b.graph.zig_exe, switch (target.result.os.tag) {
            .linux => "x86_64-linux-musl",
            .windows => "x86_64-windows-gnu",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        } }),
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
    if (b.graph.host.result.os.tag != target.result.os.tag) {
        setup_llvm.addArg(switch (target.result.os.tag) {
            .linux => "-DCMAKE_SYSTEM_NAME=Linux",
            .windows => "-DCMAKE_SYSTEM_NAME=Windows",
            else => return error.TargetNeedsToBeLinuxOrWindows,
        });
    }
    setup_llvm.setEnvironmentVariable("CC", b.fmt("{s};cc", .{b.graph.zig_exe}));
    setup_llvm.setEnvironmentVariable("CXX", b.fmt("{s};c++", .{b.graph.zig_exe}));
    setup_llvm.setEnvironmentVariable("ASM", b.fmt("{s};cc", .{b.graph.zig_exe}));
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
    const build_llvm = std.Build.Step.Run.create(b, "llvm_build");
    build_llvm.addArg(cmake_exe_path);
    build_llvm.addArg("--build");
    build_llvm.addDirectoryArg(llvm_build_dir);
    build_llvm.addArgs(&[_][]const u8{ b.fmt("-j{d}", .{jobs}), "--target" } ++ components);
    build_llvm.step.dependOn(&setup_llvm.step);

    // Install main LLVM
    var install_run_steps: [components.len]*std.Build.Step = undefined;
    for (components, 0..) |comp, i| {
        const cmd = std.Build.Step.Run.create(b, b.fmt("llvm_install_{s}", .{comp}));
        cmd.addArg(cmake_exe_path);
        cmd.addArg("--install");
        cmd.addDirectoryArg(llvm_build_dir);
        cmd.addArgs(&.{ "--component", comp });
        if (i == 0) {
            cmd.step.dependOn(&build_llvm.step);
        } else {
            cmd.step.dependOn(install_run_steps[i - 1]);
        }
        install_run_steps[i] = &cmd.step;
    }

    return install_run_steps[install_run_steps.len - 1];
}

pub fn link(exe: *std.Build.Step.Compile) !void {
    // Use command
    //     vendor/llvm-linux/bin/llvm-config --link-static --libs all | sed "s/ /\n/g" | sed "s/-l//g" | sed "s/^/\"/g" | sed "s/\$/\",/g"
    // To re-generate this list after a llvm version-upgrade
    const llvm_libs: []const []const u8 = &.{
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

    for (llvm_libs) |lib| {
        exe.root_module.linkSystemLibrary(lib, .{});
    }
}
