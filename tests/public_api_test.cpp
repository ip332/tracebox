#include <gtest/gtest.h>

#include <type_traits>

#include "tracebox/reader.h"
#include "tracebox/recorder.h"
#include "tracebox/storage.h"
#include "tracebox/stream.h"
#include "tracebox/timestamp.h"
#include "tracebox/writer.h"

TEST(PublicApiTest, ExposesMoveOnlyFacadeTypesWithoutProtocolHeaders) {
    static_assert(!std::is_copy_constructible_v<tracebox::Recorder>);
    static_assert(std::is_move_constructible_v<tracebox::Recorder>);
    static_assert(!std::is_copy_constructible_v<tracebox::Reader>);
    static_assert(std::is_move_constructible_v<tracebox::Reader>);
    static_assert(!std::is_copy_constructible_v<tracebox::Storage>);
    static_assert(std::is_move_constructible_v<tracebox::Storage>);
    static_assert(!std::is_copy_constructible_v<tracebox::Writer>);
    static_assert(std::is_move_constructible_v<tracebox::Writer>);

    tracebox::Recorder recorder("test");
    tracebox::Reader reader;
    EXPECT_FALSE(recorder.connected());
    EXPECT_FALSE(reader.connected());
    EXPECT_NE(tracebox::day(1704067200000000000ULL), 0U);
}
