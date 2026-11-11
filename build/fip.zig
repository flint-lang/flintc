const std = @import("std");

pub fn build(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    fip_source: ?[]const u8,
    tomlc17_source: ?[]const u8,
) !*std.Build.Step.Compile {
    // Normal Zig build: use the FIP package and its own build.zig
    if (fip_source == null and tomlc17_source == null) {
        const fip_dep = try b.dependencyLazy("fip", .{
            .target = target,
            .optimize = optimize,
            .@"lib-mode" = .master,
        });

        return fip_dep.artifact("fip");
    }

    // Explicit-source mode for dynamic builds for NixOS
    const fip_dir = fip_source orelse
        return error.MissingOptionFipSource;
    const toml_dir = tomlc17_source orelse
        return error.MissingOptionTomlc17Source;

    const fip_header = b.graph.cwdRelativePath(b.fmt("{s}/fip.h", .{fip_dir}));
    const toml_header = b.graph.cwdRelativePath(b.fmt("{s}/src/tomlc17.h", .{toml_dir}));
    const toml_source = b.graph.cwdRelativePath(b.fmt("{s}/src/tomlc17.c", .{toml_dir}));

    const lib = b.addLibrary(.{
        .name = "fip",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
            .pic = true,
        }),
        .linkage = .static,
    });
    lib.root_module.addIncludePath(b.graph.cwdRelativePath(fip_dir));
    lib.root_module.addIncludePath(b.graph.cwdRelativePath(b.fmt("{s}/src", .{toml_dir})));
    lib.root_module.addCSourceFile(.{
        .file = fip_header,
        .flags = &.{
            "-DFIP_IMPLEMENTATION",
            "-DFIP_MASTER",
        },
        .language = .c,
    });
    lib.root_module.addCSourceFile(.{
        .file = toml_source,
    });
    lib.installHeader(fip_header, "fip.h");
    lib.installHeader(toml_header, "tomlc17.h");

    b.installArtifact(lib);
    return lib;
}
