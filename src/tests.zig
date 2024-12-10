// DO NOT DELETE THIS FILE
const std = @import("std");
const tests = @cImport({
    @cInclude("wchar.h");
    @cInclude("tests.hpp");
});

pub const wchar_t = tests.wchar_t;
pub extern "C" fn run_parser_tests() tests.struct_TestResult;
pub extern "C" fn run_signature_tests() tests.struct_TestResult;

fn convertWcharToUtf8(wstr: [*c]const wchar_t) ![]u8 {
    if (wstr == null) return &[_]u8{};
    const allocator = std.heap.page_allocator; // Use page_allocator directly
    var utf8_str = std.ArrayList(u8).init(allocator);

    var i: usize = 0;
    while (wstr.?[i] != 0) : (i += 1) {
        const code_point = @as(u21, @intCast(wstr[i]));
        if (code_point <= 0x7F) {
            try utf8_str.appendSlice(&[_]u8{@as(u8, @intCast(code_point))});
        } else if (code_point <= 0x7FF) {
            try utf8_str.appendSlice(&[_]u8{ 0xC0 | @as(u8, @intCast(code_point >> 6)), 0x80 | @as(u8, @intCast(code_point & 0x3F)) });
        } else if (code_point <= 0xFFFF) {
            try utf8_str.appendSlice(&[_]u8{ 0xE0 | @as(u8, @intCast(code_point >> 12)), 0x80 | @as(u8, @intCast((code_point >> 6) & 0x3F)), 0x80 | @as(u8, @intCast(code_point & 0x3F)) });
        } else {
            try utf8_str.appendSlice(&[_]u8{ 0xF0 | @as(u8, @intCast(code_point >> 18)), 0x80 | @as(u8, @intCast((code_point >> 12) & 0x3F)), 0x80 | @as(u8, @intCast((code_point >> 6) & 0x3F)), 0x80 | @as(u8, @intCast(code_point & 0x3F)) });
        }
    }

    return try utf8_str.toOwnedSlice();
}

test "parser_tests" {
    const result = tests.run_parser_tests();
    std.testing.expect(result.count == 0) catch {
        std.log.warn("\n{} test(s) failed.\n", .{result.count});
        if (result.message != null) {
            std.log.warn("\n{s}", .{try convertWcharToUtf8(result.message)});
        } else {
            std.log.warn("\nNo error message available\n", .{});
        }
    };
}

test "signature_tests" {
    const result = tests.run_signature_tests();
    std.testing.expect(result.count == 0) catch {
        std.log.warn("\n{} test(s) failed.\n", .{result.count});
        if (result.message != null) {
            std.log.warn("\n{s}", .{try convertWcharToUtf8(result.message)});
        } else {
            std.log.warn("\nNo error message available\n", .{});
        }
    };
}
