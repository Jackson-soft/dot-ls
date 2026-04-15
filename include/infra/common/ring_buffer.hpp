#pragma once

// 环形缓存（定容循环队列）

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

namespace infra::common {

template<typename T>
class RingBuffer {
public:
    static constexpr std::size_t npos = ~std::size_t{0};

    explicit RingBuffer(std::size_t capacity) : capacity_(capacity), buf_(capacity) {
        assert(capacity > 0);
    }

    RingBuffer(const RingBuffer &)                     = delete;
    auto operator=(const RingBuffer &) -> RingBuffer & = delete;
    RingBuffer(RingBuffer &&)                          = default;
    auto operator=(RingBuffer &&) -> RingBuffer &      = default;
    ~RingBuffer()                                      = default;

    // 追加单个元素，缓冲区已满时返回 false
    auto push(T item) noexcept(std::is_nothrow_move_assignable_v<T>) -> bool {
        if (full()) {
            return false;
        }
        buf_[tail_] = std::move(item);
        tail_       = advance(tail_);
        ++size_;
        return true;
    }

    // 批量追加，返回实际写入的元素数
    auto push(std::span<const T> items) -> std::size_t {
        std::size_t count = 0;
        for (const T &item : items) {
            if (!push(item))
                break;
            ++count;
        }
        return count;
    }

    // 取出头部单个元素，缓冲区为空时返回 false
    bool pop(T &out) noexcept(std::is_nothrow_move_assignable_v<T>) {
        if (empty()) {
            return false;
        }
        out   = std::move(buf_[head_]);
        head_ = advance(head_);
        --size_;
        return true;
    }

    // 读取距头部偏移 i 处的元素（不消费）
    [[nodiscard]] auto peek(std::size_t i) const noexcept -> const T & {
        assert(i < size_);
        return buf_[(head_ + i) % capacity_];
    }

    // 丢弃头部 n 个元素
    void consume(std::size_t n) noexcept {
        n     = std::min(n, size_);
        head_ = (head_ + n) % capacity_;
        size_ -= n;
    }

    // 从头部偏移 from 处起查找 needle；找到返回起始索引，否则返回 npos
    [[nodiscard]] auto find(std::span<const T> needle, std::size_t from = 0) const noexcept -> std::size_t {
        if (needle.empty() || needle.size() + from > size_) {
            return npos;
        }
        const std::size_t limit = size_ - needle.size() + 1;
        for (std::size_t i = from; i < limit; ++i) {
            bool match = true;
            for (std::size_t j = 0; j < needle.size() && match; ++j) {
                match = (peek(i + j) == needle[j]);
            }
            if (match) {
                return i;
            }
        }
        return npos;
    }

    // 将头部 n 个元素复制到 dest（不消费）
    void copy_out(std::size_t n, T *dest) const {
        assert(n <= size_);
        for (std::size_t i = 0; i < n; ++i) {
            dest[i] = peek(i);
        }
    }

    [[nodiscard]] auto empty() const noexcept -> bool {
        return size_ == 0;
    }

    [[nodiscard]] auto full() const noexcept -> bool {
        return size_ == capacity_;
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return size_;
    }

    [[nodiscard]] auto capacity() const noexcept -> std::size_t {
        return capacity_;
    }

    void clear() noexcept {
        head_ = tail_ = size_ = 0;
    }

private:
    [[nodiscard]] auto advance(std::size_t idx) const noexcept -> std::size_t {
        return (++idx == capacity_) ? 0 : idx;
    }

    std::size_t    capacity_;
    std::vector<T> buf_;
    std::size_t    head_{0};
    std::size_t    tail_{0};
    std::size_t    size_{0};
};

}  // namespace infra::common