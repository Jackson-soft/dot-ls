#!/bin/bash

# LSP 服务器测试脚本
# 使用方法: ./run_test.sh [test_type]

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER_PATH="$PROJECT_DIR/build/Debug/dot-ls"
CLIENT_PATH="$PROJECT_DIR/client/client.lua"

echo "=== DOT-LS LSP Server Test Runner ==="
echo "Project: $PROJECT_DIR"
echo "Server: $SERVER_PATH"
echo "Client: $CLIENT_PATH"
echo ""

# 检查服务器是否存在
if [ ! -f "$SERVER_PATH" ]; then
    echo "❌ LSP server not found at: $SERVER_PATH"
    echo "Please build the project first:"
    echo "  cd $PROJECT_DIR"
    echo "  make"
    exit 1
fi

# 检查客户端是否存在
if [ ! -f "$CLIENT_PATH" ]; then
    echo "❌ Client script not found at: $CLIENT_PATH"
    exit 1
fi

# 检查 lua 和 cjson 是否可用
if ! command -v lua &>/dev/null; then
    echo "❌ Lua not found. Please install Lua:"
    echo "  brew install lua"
    exit 1
fi

if ! lua -e "require('cjson')" &>/dev/null; then
    echo "❌ lua-cjson not found. Please install it:"
    echo "  brew install lua"
    echo "  luarocks install lua-cjson"
    exit 1
fi

# 运行测试的函数
run_test() {
    local test_name="$1"
    echo "🧪 Running test: $test_name"
    echo "=================================="

    # 启动服务器并通过管道发送测试数据
    cd "$PROJECT_DIR"
    lua "$CLIENT_PATH" | "$SERVER_PATH" 2>&1 &

    local server_pid=$!

    # 等待测试完成
    sleep 3

    # 检查服务器是否还在运行
    if kill -0 $server_pid 2>/dev/null; then
        echo "⚠️  Server still running, killing..."
        kill $server_pid 2>/dev/null || true
        wait $server_pid 2>/dev/null || true
    fi

    echo "✅ Test completed: $test_name"
    echo ""
}

# 运行交互式测试的函数
run_interactive() {
    echo "🎮 Running interactive test..."
    echo "Choose your test and the client will pipe to the server"
    echo "=================================="

    cd "$PROJECT_DIR"
    lua client/interactive_test.lua | "$SERVER_PATH" 2>&1

    echo "✅ Interactive test completed"
}

# 运行性能测试
run_performance_test() {
    echo "⚡ Running performance test..."
    echo "=================================="

    local start_time
    start_time=$(date +%s.%N)

    # 运行多次测试
    for i in {1..5}; do
        echo "Running iteration $i/5..."
        run_test "Performance Test $i"
    done

    local end_time
    end_time=$(date +%s.%N)
    local duration
    duration=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "N/A")

    echo "⏱️  Total time: ${duration}s"
    echo "✅ Performance test completed"
}

# 主逻辑
case "${1:-default}" in
    "interactive" | "i")
        run_interactive
        ;;
    "performance" | "perf" | "p")
        run_performance_test
        ;;
    "help" | "h" | "--help")
        echo "Usage: $0 [test_type]"
        echo ""
        echo "Test types:"
        echo "  default      - Run full test sequence"
        echo "  interactive  - Run interactive test"
        echo "  performance  - Run performance test"
        echo "  help         - Show this help"
        ;;
    *)
        run_test "Full Test Sequence"
        ;;
esac

echo "🎉 All tests completed!"
