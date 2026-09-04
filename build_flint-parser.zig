const std = @import("std");

const FLINTC_VERSION = @import("build.zig").FLINTC_VERSION;
pub const FIP_VERSION = "v0.4.1";

const hasInternetConnection = @import("build.zig").hasInternetConnection;
const makeEmptyStep = @import("build.zig").makeEmptyStep;

// zig fmt: off
pub const compile_flags = &[_][]const u8{
    "-std=c++20",                           // Set C++ standard to C++20
    "-Werror",                              // Treat warnings as errors
    "-Wall",                                // Enable most warnings
    "-Wextra",                              // Enable extra warnings
    "-Wshadow",                             // Warn about shadow variables
    "-Wcast-align",                         // Warn about pointer casts that increase alignment requirement
    "-Wcast-qual",                          // Warn about casts that remove const qualifier
    "-Wunused",                             // Warn about unused variables
    "-Wold-style-cast",                     // Warn about C-style casts
    "-Wdouble-promotion",                   // Warn about float being implicitly promoted to double
    "-Wformat=2",                           // Warn about printf/scanf/strftime/strfmon format string issue
    "-Wundef",                              // Warn if an undefined identifier is evaluated in an #if
    "-Wpointer-arith",                      // Warn about sizeof(void) and add/sub with void*
    "-Wunreachable-code",                   // Warn about unreachable code
    "-fno-omit-frame-pointer",              // Prevent omitting frame pointer for debugging and stack unwinding
    "-funwind-tables",                      // Generate unwind tables for stack unwinding
    "-ffunction-sections",                  // Place each function in its own section
    "-fdata-sections",                      // Place each data object in its own section
    "-fstandalone-debug",                   // Emit standalone debug information
    "-fno-sanitize=undefined",              // Disable sanitizer to prevent "missing ubsan" compile errors
    "-Wno-unused-command-line-argument",    // Supresses "argument unused during compilation" warning
};
// zig fmt: on

pub fn build_flint_parser_lib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) !*std.Build.Step.Compile {
    const update_fip = try updateFip(b);

    const lib = b.addLibrary(.{
        .name = "flint-parser",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libcpp = true,
            .pic = true,
        }),
        .linkage = .static,
    });
    lib.link_function_sections = true;
    lib.link_data_sections = true;
    lib.link_gc_sections = true;
    lib.compress_debug_sections = .zlib;
    lib.build_id = .fast;

    lib.step.dependOn(&update_fip.step);

    lib.root_module.addCMacro("VERSION", b.fmt("\"{s}\"", .{FLINTC_VERSION}));
    if (optimize == .Debug) {
        lib.root_module.addCMacro("DEBUG_BUILD", "");
    }

    // Add Include paths
    lib.root_module.addIncludePath(b.path("vendor/sources/fip"));
    lib.root_module.addIncludePath(b.path("include"));

    // Collect C++ files
    const path = try b.path(".").getPath4(b, &lib.step);
    var src_dir: std.Io.Dir = try path.openDir(b.graph.io, "src", .{ .iterate = true });
    defer src_dir.close(b.graph.io);
    var cpp_files: std.ArrayList([]const u8) = .empty;
    defer cpp_files.deinit(b.allocator);
    var walker = try src_dir.walk(b.allocator);
    defer walker.deinit();
    while (try walker.next(b.graph.io)) |entry| {
        if (entry.kind == .file and
            std.mem.endsWith(u8, entry.basename, ".cpp") and
            !std.mem.containsAtLeast(u8, entry.path, 1, "generator") and
            !std.mem.eql(u8, entry.basename, "main.cpp") and
            !std.mem.eql(u8, entry.basename, "linker.cpp") and
            !std.mem.eql(u8, entry.basename, "fip.cpp"))
        {
            try cpp_files.append(b.allocator, try b.allocator.dupe(u8, entry.path));
        }
    }

    // Add C++ src files
    lib.root_module.addCSourceFiles(.{
        .root = b.path("src"),
        .files = cpp_files.items,
        .flags = compile_flags,
    });

    // Add toml C src file for FIP
    lib.root_module.addCSourceFile(.{
        .file = b.path("vendor/sources/fip/toml/tomlc17.c"),
        .flags = &[_][]const u8{
            "-fno-sanitize=undefined", // Disable sanitizer to prevent "missing ubsan" compile errors
        },
    });

    return lib;
}

fn updateFip(b: *std.Build) !*std.Build.Step.Run {
    // 1. Check if fip exists in vendor directory
    std.debug.print("-- Updating the 'fip' repository\n", .{});
    const cwd = b.path(".");
    const cwd_path = try b.path(".").getPath4(b, null);
    if (cwd_path.openDir(b.graph.io, "vendor/sources/fip", .{})) |_| {
        // 2. Check for internet connection
        if (!hasInternetConnection(b)) {
            std.debug.print("-- No internet connection found, skipping updating 'fip'...\n", .{});
            return makeEmptyStep(b);
        }

        // 3. Reset hard
        const reset_fip_cmd = b.addSystemCommand(&[_][]const u8{ "git", "reset", "--hard", "-q" });
        reset_fip_cmd.setName("reset_fip");
        reset_fip_cmd.setCwd(b.path("vendor/sources/fip"));

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
        fetch_fip_complete_step.setCwd(cwd);

        // 4. Checkout fip hash
        const checkout_fip_hash_cmd = b.addSystemCommand(&[_][]const u8{ "git", "checkout", "-fq", FIP_VERSION });
        checkout_fip_hash_cmd.setName("checkout_fip_hash");
        checkout_fip_hash_cmd.setCwd(b.path("vendor/sources/fip"));
        checkout_fip_hash_cmd.step.dependOn(&fetch_fip_complete_step.step);

        return checkout_fip_hash_cmd;
    }
}
