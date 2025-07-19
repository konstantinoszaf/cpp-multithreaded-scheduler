#pragma once

#include <gmock/gmock.h>
#include "detail/thread_pool_impl.h"

namespace scheduler {
namespace detail {

class MockThreadPool : public IThreadPool {
public:
  MOCK_METHOD(void, start, (), (override));
  MOCK_METHOD(void, stop, (), (override));
  MOCK_METHOD(bool, submit, (std::function<void()> job), (override));
};

}  // namespace detail
}  // namespace scheduler
