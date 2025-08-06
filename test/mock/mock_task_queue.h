// #pragma once

// #include <gmock/gmock.h>
// #include "detail/task_queue_impl.h"

// namespace scheduler {
// namespace detail {

// class MockTaskQueue : public ITaskQueue {
// public:
//     MOCK_METHOD(void, push, (Task&& task), (override));
//     MOCK_METHOD((std::optional<Task>), pop, (), (override));
//     MOCK_METHOD((std::optional<std::reference_wrapper<const std::chrono::steady_clock::time_point>>),
//                 peek_time, (), (const, override));
//     MOCK_METHOD(bool, empty, (), (const, override));
//     MOCK_METHOD(size_t, size, (), (const, override));
// };


// }  // namespace detail
// }  // namespace scheduler
