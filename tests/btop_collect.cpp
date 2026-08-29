// SPDX-License-Identifier: Apache-2.0
//
// macOS 专属：Net::read_ifstats_pair() 的对齐安全回归测试。
// 与 src/osx/btop_collect.cpp 配套——sysctl(NET_RT_IFLIST2) 返回的缓冲区仅
// 保证 4 字节对齐，直接解引用其中的 u_int64_t 字段（ifm_data.ifi_ibytes/
// ifi_obytes，要求 8 字节对齐）属未定义行为（UBSan: misaligned address）。
// 本测试在"仅 4 字节对齐"的存储上构造非对齐视图，验证修复实现读取值与
// 对齐拷贝一致；在 UBSan 构建下运行本测试可在实现回归为直接解引用时
// （misaligned 报错）使测试失败，起到回归保护作用。

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <tuple>

#include <gtest/gtest.h>

#include <net/if.h>
#include <net/if_var.h>   // struct if_msghdr2 / if_data（macOS）
#include <net/route.h>    // RTM_IFINFO2 等消息常量

namespace Net {
	// 由 src/osx/btop_collect.cpp 提供（该平台实现无独立头文件，此处显式声明，
	// 符号与实现严格同名同签名）
	std::tuple<uint64_t, uint64_t> read_ifstats_pair(const struct if_msghdr2 *if2m);
}

namespace {

	// 构造"仅 4 字节对齐"的存储（sysctl 缓冲区的等价物），并把消息字节拷入
	// 偏移 4 处——若消息内 8 字节字段落在 4 字节对齐位，即构成非对齐视图。
	struct UnalignedStorage {
		alignas(4) std::byte bytes[sizeof(struct if_msghdr2) + 4] = {};
		auto view() const -> const struct if_msghdr2 * {
			// 仅创建指针视图，不解引用任何成员（构造非对齐视图必然需要类型转换）
			return reinterpret_cast<const struct if_msghdr2 *>(bytes + 4);
		}
	};

}

TEST(net_ifstats, read_unaligned_pair_matches_source_values) {
	const uint64_t want_rx = 0x1122334455667788ull;
	const uint64_t want_tx = 0x99aabbccddeeff00ull;

	// 1) 在正确对齐的源结构中写入测试值
	alignas(alignof(struct if_msghdr2)) std::byte src[sizeof(struct if_msghdr2)] = {};
	auto &src_msg = *reinterpret_cast<struct if_msghdr2 *>(src);
	src_msg.ifm_data.ifi_ibytes = want_rx;
	src_msg.ifm_data.ifi_obytes = want_tx;

	// 2) 字节拷贝到 4 字节对齐存储的偏移 4 处（memcpy 无对齐要求）
	UnalignedStorage storage;
	std::memcpy(storage.bytes + 4, src, sizeof(struct if_msghdr2));

	// 3) 被测函数：不允许出现 misaligned 访问
	const auto [rx, tx] = Net::read_ifstats_pair(storage.view());

	EXPECT_EQ(rx, want_rx);
	EXPECT_EQ(tx, want_tx);
}

TEST(net_ifstats, read_unaligned_pair_all_zero) {
	UnalignedStorage storage;   // 全零初始化
	const auto [rx, tx] = Net::read_ifstats_pair(storage.view());
	EXPECT_EQ(rx, 0u);
	EXPECT_EQ(tx, 0u);
}