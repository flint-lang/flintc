// DO NOT DELETE THIS FILE
const std = @import("std");
const tests = @cImport({
    @cInclude("tests.hpp");
});

pub extern "C" fn run_parser_tests() tests.struct_TestResult;
pub extern "C" fn run_signature_tests() tests.struct_TestResult;

test "parser_tests" {
    const result = tests.run_parser_tests();
    std.testing.expect(result.count == 0) catch {
        std.log.warn("\n{} test(s) failed.\n", .{result.count});
        std.log.warn("\n{s}", .{result.message});
    };
}

test "signature_tests" {
    const result = tests.run_signature_tests();
    std.testing.expect(result.count == 0) catch {
        std.log.warn("\n{} test(s) failed.\n", .{result.count});
        std.log.warn("\n{s}", .{result.message});
    };
}
