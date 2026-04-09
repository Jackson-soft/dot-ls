#!/bin/bash

# LSP 服务器测试脚本
# 使用方法: ./run_test.sh [test_type]
#   default    - 运行所有测试套件
#   completion - 仅运行补全测试
#   document   - 仅运行文档协议测试 (didOpen/didChange/didSave)
#   performance - 运行性能测试
#   help       - 显示帮助

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER_PATH="$PROJECT_DIR/build/Debug/dot-ls"
CLIENT_DIR="$PROJECT_DIR/client"

echo "=== DOT-LS LSP Server Test Runner ==="
echo "Project: $PROJECT_DIR"
echo "Server : $SERVER_PATH"
echo ""

# 检查服务器是否存在
if [ ! -f "$SERVER_PATH" ]; then
    echo "❌ LSP server not found at: $SERVER_PATH"
    echo "Please build the project first: cd $PROJECT_DIR && make"
    exit 1
fi

# 检查 lua 是否可用
if ! command -v lua &>/dev/null; then
    echo "❌ Lua not found. Install with: brew install lua"
    exit 1
fi

# 运行单个 Lua 测试脚本
run_lua_test() {
    local name="$1"
    local script="$2"
    echo "🧪 Running: $name"
    echo "=================================="
    if lua "$script" "$SERVER_PATH"; then
        echo "✅ $name passed"
    else
        echo "❌ $name FAILED"
        return 1
    fi
    echo ""
}

# 旧式管道测试（client.lua）
run_pipe_test() {
    echo "🧪 Running: Pipe client test"
    echo "=================================="
    cd "$PROJECT_DIR"
    lua "$CLIENT_DIR/client.lua" "$SERVER_PATH" 2>&1 &
    local pid=$!
    sleep 3
    if kill -0 $pid 2>/dev/null; then
        kill $pid 2>/dev/null || true
        wait $pid 2>/dev/null || true
    fi
    echo "✅ Pipe client test completed"
    echo ""
}

# 性能测试
run_performance_test() {
    echo "⚡ Running performance test..."
    echo "=================================="
    local start_time end_time duration
    start_time=$(date +%s.%N)
    for i in {1..5}; do
        echo "Iteration $i/5..."
        lua "$CLIENT_DIR/test_completion.lua" "$SERVER_PATH" 2>/dev/null
    done
    end_time=$(date +%s.%N)
    duration=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "N/A")
    echo "⏱️  Total time: ${duration}s"
    echo "✅ Performance test completed"
}

case "${1:-default}" in
    "completion" | "c")
        run_lua_test "Completion Test Suite" "$CLIENT_DIR/test_completion.lua"
        ;;
    "document" | "doc" | "d")
        run_lua_test "Document Protocol Test Suite" "$CLIENT_DIR/test_document.lua"
        ;;
    "performance" | "perf" | "p")
        run_performance_test
        ;;
    "help" | "h" | "--help")
        echo "Usage: $0 [test_type]"
        echo ""
        echo "  default     Run all test suites (completion + document)"
        echo "  completion  Run completion tests only"
        echo "  document    Run didOpen/didChange/didSave/didClose lifecycle tests"
        echo "  performance Run completion test 5x and report time"
        echo "  help        Show this help"
        ;;
    *)
        run_lua_test "Completion Test Suite"        "$CLIENT_DIR/test_completion.lua"
        run_lua_test "Document Protocol Test Suite" "$CLIENT_DIR/test_document.lua"
        ;;
esac

echo "🎉 All tests completed!"
