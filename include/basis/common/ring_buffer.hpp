#pragma once

// 环形缓存

#include <cstddef>

namespace basis::common {
template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity) {}

    ~RingBuffer() = default;
};
}  // namespace basis::common