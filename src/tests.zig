// DO NOT DELETE THIS FILE
const std = @import("std");
const tests = @cImport({
    @cInclude("tests.hpp");
});

pub extern "C" fn run_tests() tests.struct_TestResult;

test "cpp_tests" {
    const result = tests.run_tests();
    std.testing.expect(result.count == 0) catch {
        std.log.warn("\n{} tests failed.\n", .{result.count});
        std.log.warn("\n{s}", .{result.message});
    };
}
